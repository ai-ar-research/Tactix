// Seeded fuzz strategy implemented exclusively through the installed public
// C interface.  This file deliberately includes no src/ header: the release
// fuzz gate must exercise the same surface available to an external policy.
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

enum class Form : unsigned { Canonical, Instance, Semantic, Online, Count };
enum class Profile : unsigned {
  RootHeavy,
  SearchHeavy,
  Alternating,
  RestartHeavy,
  InstanceHeavy,
  AlgebraicHeavy,
  Count
};

struct Rng {
  uint64_t state;
  explicit Rng(uint64_t seed) : state(seed ? seed : UINT64_C(0x9e3779b97f4a7c15)) {}
  uint64_t next() {
    uint64_t z = (state += UINT64_C(0x9e3779b97f4a7c15));
    z = (z ^ (z >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94d049bb133111eb);
    return z ^ (z >> 31);
  }
  uint64_t below(uint64_t bound) { return bound ? next() % bound : 0; }
  bool one_in(uint64_t n) { return below(n) == 0; }
};

struct Coverage {
  std::vector<uint64_t> rules;
  std::vector<uint64_t> effective_rules;
  std::array<uint64_t, static_cast<unsigned>(Form::Count)> forms{};
  std::array<uint64_t, static_cast<unsigned>(Form::Count)> effective_forms{};
  uint64_t root_actions = 0, search_actions = 0;
  uint64_t root_to_search = 0, partial_restarts = 0, full_restarts = 0;
  uint64_t search_to_root = 0, algebraic_actions = 0;
  uint64_t online_declines = 0, public_declines = 0;
};

bool algebraic(std::string_view name) {
  for (const std::string_view token :
       {"Affine", "Gf3", "Parity", "Cardinality", "Csp", "CSP",
        "Equality", "Tseitin"})
    if (name.find(token) != std::string_view::npos)
      return true;
  return false;
}

bool service(std::string_view name) {
  return name == "Propagate" || name == "ConflictAnalysis" ||
         name == "Decide" || name == "Restart";
}

uint32_t declared_variables(const char *path) {
  std::ifstream input(path);
  std::string line;
  uint32_t result = 0;
  while (std::getline(input, line)) {
    const auto begin = line.find_first_not_of(" \t\r");
    if (begin == std::string::npos || line[begin] == 'c')
      continue;
    if (line[begin] == 'p') {
      std::istringstream words(line.substr(begin));
      std::string p, cnf;
      uint64_t variables = 0, clauses = 0;
      if (words >> p >> cnf >> variables >> clauses)
        result = static_cast<uint32_t>(std::min<uint64_t>(variables, UINT32_MAX));
      continue;
    }
    std::istringstream literals(line.substr(begin));
    int64_t literal = 0;
    while (literals >> literal)
      if (literal && literal != std::numeric_limits<int64_t>::min())
        result = std::max<uint32_t>(result, static_cast<uint32_t>(std::min<uint64_t>(
            literal < 0 ? static_cast<uint64_t>(-literal)
                        : static_cast<uint64_t>(literal),
            UINT32_MAX)));
  }
  return result;
}

std::vector<int> applicable(tactix_c_handle *solver) {
  const int count = tactix_rule_count();
  std::vector<int> result(static_cast<size_t>(std::max(count, 0)));
  const int found = tactix_rules_applicable(solver, result.data(), count);
  if (found < 0) return {};
  result.resize(static_cast<size_t>(found));
  return result;
}

bool contains(const std::vector<int> &rules, int id) {
  return id >= 0 && std::find(rules.begin(), rules.end(), id) != rules.end();
}

int rule_id(std::string_view name) {
  for (int id = 0; id < tactix_rule_count(); ++id) {
    const char *candidate = tactix_rule_name(id);
    if (candidate && name == candidate) return id;
  }
  return -1;
}

uint64_t select_instance(tactix_c_handle *solver, int id, Rng &rng,
                         bool semantic_form, bool &available) {
  std::array<uint64_t, 256> instances{};
  const int count = semantic_form
                        ? tactix_rule_semantic_instances(
                              solver, id, instances.data(), instances.size())
                        : tactix_rule_instances(solver, id, instances.data(),
                                                instances.size());
  if (count <= 0) {
    available = false;
    return TACTIX_INSTANCE_CANONICAL;
  }
  available = true;
  size_t first = 0;
  // Prefer a real selected instance when the public surface offers one; the
  // canonical whole-rule action remains independently covered by apply().
  if (count > 1 && instances[0] == TACTIX_INSTANCE_CANONICAL) first = 1;
  return instances[first + rng.below(static_cast<size_t>(count) - first)];
}

bool typed_complete_profile(std::string_view name, uint32_t variables,
                            tactix_online_parameters &parameters) {
  const uint64_t clauses = std::max<uint64_t>(1024, 32 * variables);
  const uint64_t literals = std::max<uint64_t>(8192, 256 * variables);
  const uint64_t work = std::max<uint64_t>(UINT64_C(1) << 18,
                                           4096 * variables);
  const uint32_t extent = std::max<uint32_t>(4096, 16 * variables);
  if (name == "ParityConsequence") {
    parameters.kind = TACTIX_ONLINE_PARAMETERS_PARITY_CONSEQUENCE;
    auto &p = parameters.value.parity_consequence;
    p.maximum_clauses = clauses;
    p.maximum_groups = clauses;
    p.selection_work = work;
    p.intermediate_width = 16;
    p.source_count = 8;
    p.output_width = 4;
    p.maximum_proof_additions = work;
    p.maximum_tree_variables = 16;
    return true;
  }
  if (name == "AffineConsequence") {
    parameters.kind = TACTIX_ONLINE_PARAMETERS_AFFINE_CONSEQUENCE;
    auto &p = parameters.value.affine_consequence;
    p.maximum_clauses = clauses;
    p.maximum_snapshot_literals = literals;
    p.maximum_snapshot_work = work;
    p.maximum_groups = clauses;
    p.maximum_patterns = work;
    p.maximum_elimination_work = work;
    p.maximum_arity = 8;
    p.maximum_intermediate_width = 16;
    p.maximum_sources = 64;
    p.maximum_output_width = 4;
    p.maximum_proof_additions = work;
    p.maximum_proof_deletions = work;
    p.maximum_journal_bytes = work * 8;
    p.semantics = TACTIX_ONLINE_AFFINE_CONSEQUENCE_CERTIFIED_BASIS_V2;
    p.basis_maximum_work = work;
    p.basis_maximum_intermediate_width = 32;
    p.basis_maximum_sources = 64;
    p.basis_maximum_premises = 64;
    p.basis_maximum_nodes = 4096;
    p.basis_maximum_direct_width = 8;
    p.maximum_external_extent = extent;
    p.maximum_session_keys = clauses;
    p.maximum_session_work = work;
    return true;
  }
  // Other structured theory families require an exact variable-length public
  // payload.  Their old native-producer templates were private controller
  // machinery, not public actions, so this external policy never uses them as
  // a hidden discovery back door.
  return false;
}

int public_work_unit(std::string_view name) {
  if (name == "BVE") return TACTIX_ONLINE_WORK_BVE_TICKS;
  if (name == "Propagate") return TACTIX_ONLINE_WORK_PROPAGATIONS;
  return TACTIX_ONLINE_WORK_APPLICATIONS;
}

int online_once(tactix_c_handle *solver, int id, uint64_t instance,
                uint32_t variables, uint64_t &revision,
                tactix_online_result &result) {
  tactix_online_step step{};
  step.step_id = ++revision;
  step.rule_id = id;
  const char *name = tactix_rule_name(id);
  const bool typed = name && typed_complete_profile(name, variables,
                                                     step.parameters);
  step.granularity = typed ? TACTIX_ONLINE_GRANULARITY_COMPLETE_PASS
                     : instance == TACTIX_INSTANCE_CANONICAL
                         ? TACTIX_ONLINE_GRANULARITY_CANONICAL
                         : TACTIX_ONLINE_GRANULARITY_ONE;
  if (!typed && instance != TACTIX_INSTANCE_CANONICAL) {
    step.instances = &instance;
    step.instance_count = 1;
  }
  step.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.repeat_limit = 1;
  step.rule_budget_mode = TACTIX_ONLINE_RULE_BUDGET_BOUNDED;
  step.rule_budget = UINT64_C(1) << 16;
  step.touch_filter_mode = TACTIX_ONLINE_SETTING_INHERIT;
  step.likely_kept_redundant_mode = TACTIX_ONLINE_SETTING_INHERIT;
  step.work_unit = public_work_unit(name ? name : "");
  step.budget_unlimited = 0;
  step.budget = name && !std::strcmp(name, "BVE")
                    ? UINT64_C(1) << 14
                    : name && !std::strcmp(name, "Propagate")
                          ? UINT64_C(1) << 16
                          : 1;
  step.post_application = TACTIX_ONLINE_POST_NONE;
  const tactix_online_program program{
      &step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  if (tactix_online_configure(solver, &program, revision,
                              TACTIX_ONLINE_RECONFIGURE_RESET) != 0) {
    if (std::getenv("TACTIX_PUBLIC_RANDOM_TRACE"))
      std::fprintf(stderr, "public-random online configure declined: %s\n",
                   tactix_rule_name(id));
    return -1;
  }
  if (tactix_online_advance(solver, &result) != 0) {
    if (std::getenv("TACTIX_PUBLIC_RANDOM_TRACE"))
      std::fprintf(stderr, "public-random online advance declined: %s\n",
                   tactix_rule_name(id));
    return -1;
  }
  if (result.status == TACTIX_ONLINE_SAT) return 10;
  if (result.status == TACTIX_ONLINE_UNSAT) return 20;
  if (result.status == TACTIX_ONLINE_CONFLICT) return 2;
  if (result.status == TACTIX_ONLINE_INVALID_REQUEST ||
      result.status == TACTIX_ONLINE_ACCOUNTING_ERROR)
    return -1;
  return result.made_rule_progress ? 0 : 1;
}

Form choose_form(const Coverage &coverage, Profile profile, Rng &rng) {
  uint64_t best = UINT64_MAX;
  std::array<Form, static_cast<unsigned>(Form::Count)> choices{};
  size_t count = 0;
  for (unsigned raw = 0; raw < static_cast<unsigned>(Form::Count); ++raw) {
    const Form form = static_cast<Form>(raw);
    uint64_t score = coverage.forms[raw] * 8 + rng.below(8);
    if (profile == Profile::InstanceHeavy && form != Form::Canonical)
      score = score > 4 ? score - 4 : 0;
    if (score < best) best = score, count = 0;
    if (score == best) choices[count++] = form;
  }
  return choices[rng.below(count)];
}

int execute_rule(tactix_c_handle *solver, int id, Profile profile, Rng &rng,
                 Coverage &coverage, uint32_t variables,
                 uint64_t &revision) {
  Form form = choose_form(coverage, profile, rng);
  bool have = true;
  uint64_t instance = TACTIX_INSTANCE_CANONICAL;
  int result = -1;
  if (form == Form::Canonical) {
    result = tactix_apply(solver, id);
  } else if (form == Form::Instance) {
    instance = select_instance(solver, id, rng, false, have);
    if (have) result = tactix_apply_instance(solver, id, instance);
  } else if (form == Form::Semantic) {
    instance = select_instance(solver, id, rng, true, have);
    if (have) result = tactix_apply_semantic_instance(solver, id, instance);
  } else {
    instance = select_instance(solver, id, rng, true, have);
    tactix_online_result online{};
    if (have)
      result = online_once(solver, id, instance, variables, revision, online);
    if (result < 0) ++coverage.online_declines;
  }
  if (result < 0) {
    ++coverage.public_declines;
    // Enumeration/configuration is transactional. A rejected form is useful
    // API coverage, but the strategy still executes the selected rule once.
    form = Form::Canonical;
    result = tactix_apply(solver, id);
  }
  ++coverage.forms[static_cast<unsigned>(form)];
  if (id >= 0 && static_cast<size_t>(id) < coverage.rules.size())
    ++coverage.rules[static_cast<size_t>(id)];
  if (result == 0) {
    ++coverage.effective_forms[static_cast<unsigned>(form)];
    if (id >= 0 && static_cast<size_t>(id) < coverage.effective_rules.size())
      ++coverage.effective_rules[static_cast<size_t>(id)];
  }
  const char *name = tactix_rule_name(id);
  if (name && algebraic(name)) ++coverage.algebraic_actions;
  return result;
}

int choose_rule(const std::vector<int> &pool, const Coverage &coverage,
                Profile profile, Rng &rng) {
  if (pool.empty()) return -1;
  std::vector<int> candidates = pool;
  if (profile == Profile::AlgebraicHeavy) {
    std::vector<int> algebraic_pool;
    for (const int id : pool) {
      const char *name = tactix_rule_name(id);
      if (name && algebraic(name)) algebraic_pool.push_back(id);
    }
    if (!algebraic_pool.empty() && !rng.one_in(4))
      candidates.swap(algebraic_pool);
  }
  uint64_t least = UINT64_MAX;
  for (const int id : candidates)
    least = std::min(least, coverage.rules[static_cast<size_t>(id)]);
  std::vector<int> undercovered;
  for (const int id : candidates)
    if (coverage.rules[static_cast<size_t>(id)] <= least + rng.below(2))
      undercovered.push_back(id);
  return undercovered[rng.below(undercovered.size())];
}

uint64_t root_burst(Profile profile, Rng &rng) {
  switch (profile) {
  case Profile::RootHeavy: return 4 + rng.below(9);
  case Profile::SearchHeavy: return rng.below(2);
  case Profile::Alternating: return rng.below(4);
  case Profile::RestartHeavy: return rng.below(3);
  case Profile::InstanceHeavy: return 2 + rng.below(6);
  case Profile::AlgebraicHeavy: return 3 + rng.below(7);
  case Profile::Count: break;
  }
  return 0;
}

bool extra_search_action(Profile profile, Rng &rng) {
  switch (profile) {
  case Profile::RootHeavy: return rng.one_in(5);
  case Profile::SearchHeavy: return !rng.one_in(2);
  case Profile::Alternating: return rng.one_in(3);
  case Profile::RestartHeavy: return rng.one_in(4);
  case Profile::InstanceHeavy: return !rng.one_in(3);
  case Profile::AlgebraicHeavy: return !rng.one_in(4);
  case Profile::Count: break;
  }
  return false;
}

bool restart_now(Profile profile, Rng &rng) {
  const uint64_t denominator = profile == Profile::RestartHeavy ? 3
                               : profile == Profile::Alternating ? 7
                                                                 : 19;
  return rng.one_in(denominator);
}

int public_random_solve(tactix_c_handle *solver, uint64_t seed,
                        uint32_t variables, Coverage &coverage) {
  Rng rng(seed);
  const Profile profile = static_cast<Profile>(seed % static_cast<unsigned>(Profile::Count));
  const int propagation = rule_id("Propagate");
  const int analysis = rule_id("ConflictAnalysis");
  const int restart = rule_id("Restart");
  uint64_t revision = 0;
  uint64_t remaining_root = root_burst(profile, rng);
  // Diversity is aggregated over the campaign.  Keeping each prefix short is
  // what lets thousands of independent cases explore more state transitions
  // instead of one case monopolizing a worker in an expensive root action.
  const uint64_t random_budget = 24 + std::min<uint64_t>(variables, 64);
  uint64_t next_restart_conflict = 64 + rng.below(65);
  uint64_t next_reduce_conflict = 256 + rng.below(257);
  bool in_search = false;

  for (uint64_t steps = 0;; ++steps) {
    const int status = tactix_status(solver);
    if (status) return status;
    auto rules = applicable(solver);

    // Conflicts and propagation are solver obligations. Still vary the public
    // call form so the same semantic boundary exercises every API adapter.
    if (contains(rules, analysis)) {
      const int result = execute_rule(solver, analysis, profile, rng, coverage,
                                      variables, revision);
      if (result == 10 || result == 20) return result;
      continue;
    }
    if (contains(rules, propagation) && (steps >= random_budget || !rng.one_in(5))) {
      const int result = execute_rule(solver, propagation, profile, rng, coverage,
                                      variables, revision);
      if (result == 10 || result == 20) return result;
      continue;
    }

    const uint64_t level = tactix_stat(solver, TACTIX_STAT_LEVEL);
    if (in_search && level > 0 && contains(rules, restart) &&
        steps < random_budget && restart_now(profile, rng)) {
      const bool full = level == 1 || rng.one_in(2);
      const uint64_t keep = full ? 0 : 1 + rng.below(level - 1);
      const int result = tactix_apply_instance(solver, restart, keep);
      ++coverage.forms[static_cast<unsigned>(Form::Instance)];
      ++coverage.rules[static_cast<size_t>(restart)];
      if (result == 10 || result == 20) return result;
      if (full) {
        ++coverage.full_restarts;
        ++coverage.search_to_root;
        in_search = false;
        remaining_root = root_burst(profile, rng);
      } else {
        ++coverage.partial_restarts;
      }
      continue;
    }

    // After the diverse prefix, retain a complete public CDCL loop instead of
    // degenerating into restart-free random DPLL. The first public decision
    // candidate carries the installed heuristic ranking; periodic full
    // restarts and reductions keep hard corpus cases moving while preserving
    // one-action-at-a-time attribution.
    const uint64_t conflicts = tactix_stat(solver, TACTIX_STAT_CONFLICTS);
    if (steps >= random_budget && in_search && level > 0 &&
        contains(rules, restart) && conflicts >= next_restart_conflict) {
      const int result = tactix_apply_instance(solver, restart, 0);
      ++coverage.forms[static_cast<unsigned>(Form::Instance)];
      ++coverage.rules[static_cast<size_t>(restart)];
      if (result == 10 || result == 20) return result;
      ++coverage.full_restarts;
      ++coverage.search_to_root;
      in_search = false;
      next_restart_conflict = conflicts + 64 + rng.below(193);
      continue;
    }
    const int reduce = rule_id("Reduce");
    if (steps >= random_budget && contains(rules, reduce) &&
        conflicts >= next_reduce_conflict) {
      const int result = execute_rule(solver, reduce, profile, rng, coverage,
                                      variables, revision);
      if (result == 10 || result == 20) return result;
      next_reduce_conflict = conflicts + 256 + rng.below(769);
      continue;
    }

    std::vector<int> nonservice;
    for (const int id : rules) {
      const char *name = tactix_rule_name(id);
      if (name && !service(name)) nonservice.push_back(id);
    }
    const bool random_phase = steps < random_budget;
    const bool take_root = !in_search && remaining_root && !nonservice.empty();
    const bool take_search_extra = in_search && random_phase &&
                                   !nonservice.empty() &&
                                   extra_search_action(profile, rng);
    if (random_phase && (take_root || take_search_extra)) {
      const int id = choose_rule(nonservice, coverage, profile, rng);
      const int result = execute_rule(solver, id, profile, rng, coverage,
                                      variables, revision);
      if (take_root) --remaining_root, ++coverage.root_actions;
      else ++coverage.search_actions;
      if (result == 10 || result == 20) return result;
      continue;
    }

    std::vector<int> candidates(std::max<uint64_t>(
        1, std::min<uint64_t>(tactix_stat(solver, TACTIX_STAT_UNASSIGNED), 4096)));
    const int count = tactix_decide_candidates(solver, candidates.data(),
                                               static_cast<int>(candidates.size()));
    if (count > 0) {
      const int literal = steps < random_budget
                              ? candidates[rng.below(static_cast<size_t>(count))]
                              : candidates[0];
      if (tactix_decide(solver, literal) == 0) {
        if (!in_search) ++coverage.root_to_search;
        in_search = true;
        continue;
      }
    }

    // No multi-step escape hatch: a random policy is valuable precisely when
    // every mutation is attributable to one public action.  Reaching this
    // state means the public stepping contract is incomplete for the current
    // solver state, and the fuzz harness should retain it as UNKNOWN.
    return 0;
  }
}

void print_model(tactix_c_handle *solver, uint32_t variables) {
  std::fputs("v", stdout);
  for (uint32_t variable = 1; variable <= variables; ++variable) {
    const int value = tactix_value(solver, static_cast<int>(variable));
    std::printf(" %d", value >= 0 ? static_cast<int>(variable)
                                  : -static_cast<int>(variable));
    if (variable % 100 == 0 && variable != variables) std::fputs("\nv", stdout);
  }
  std::fputs(" 0\n", stdout);
}

void print_coverage(uint64_t seed, Profile profile, const Coverage &coverage) {
  static constexpr const char *profiles[] = {
      "root-heavy", "search-heavy", "alternating", "restart-heavy",
      "instance-heavy", "algebraic-heavy"};
  std::printf("c public-random seed %llu profile %s\n",
              static_cast<unsigned long long>(seed),
              profiles[static_cast<unsigned>(profile)]);
  std::printf("c public-random transitions root-actions=%llu search-actions=%llu "
              "root-to-search=%llu partial-restarts=%llu full-restarts=%llu "
              "search-to-root=%llu algebraic-actions=%llu\n",
              static_cast<unsigned long long>(coverage.root_actions),
              static_cast<unsigned long long>(coverage.search_actions),
              static_cast<unsigned long long>(coverage.root_to_search),
              static_cast<unsigned long long>(coverage.partial_restarts),
              static_cast<unsigned long long>(coverage.full_restarts),
              static_cast<unsigned long long>(coverage.search_to_root),
              static_cast<unsigned long long>(coverage.algebraic_actions));
  std::printf("c public-random forms canonical=%llu instance=%llu semantic=%llu "
              "online=%llu effective-canonical=%llu effective-instance=%llu "
              "effective-semantic=%llu effective-online=%llu "
              "online-declines=%llu public-declines=%llu\n",
              static_cast<unsigned long long>(coverage.forms[0]),
              static_cast<unsigned long long>(coverage.forms[1]),
              static_cast<unsigned long long>(coverage.forms[2]),
              static_cast<unsigned long long>(coverage.forms[3]),
              static_cast<unsigned long long>(coverage.effective_forms[0]),
              static_cast<unsigned long long>(coverage.effective_forms[1]),
              static_cast<unsigned long long>(coverage.effective_forms[2]),
              static_cast<unsigned long long>(coverage.effective_forms[3]),
              static_cast<unsigned long long>(coverage.online_declines),
              static_cast<unsigned long long>(coverage.public_declines));
  for (size_t id = 0; id < coverage.rules.size(); ++id)
    if (coverage.rules[id])
      std::printf("c public-random rule %s applications=%llu effective=%llu\n",
                  tactix_rule_name(static_cast<int>(id)),
                  static_cast<unsigned long long>(coverage.rules[id]),
                  static_cast<unsigned long long>(coverage.effective_rules[id]));
}

} // namespace

int main(int argc, char **argv) {
  uint64_t seed = 20260710;
  int proof_format = TACTIX_PROOF_PBP;
  const char *cnf_path = nullptr, *proof_path = nullptr;
  for (int i = 1; i < argc; ++i) {
    const char *arg = argv[i];
    if (!std::strcmp(arg, "--pbp")) proof_format = TACTIX_PROOF_PBP;
    else if (!std::strcmp(arg, "--auto-proof")) proof_format = TACTIX_PROOF_AUTOMATIC;
    else if (!std::strcmp(arg, "--dsr")) proof_format = TACTIX_PROOF_DSR_BINARY;
    else if (!std::strcmp(arg, "--drat")) proof_format = TACTIX_PROOF_DRAT_BINARY;
    else if (!std::strcmp(arg, "--lrat")) proof_format = TACTIX_PROOF_LRAT;
    else if (!std::strcmp(arg, "--no-binary")) {
      if (proof_format == TACTIX_PROOF_DSR_BINARY) proof_format = TACTIX_PROOF_DSR_ASCII;
      if (proof_format == TACTIX_PROOF_DRAT_BINARY) proof_format = TACTIX_PROOF_DRAT_ASCII;
    } else if (!std::strncmp(arg, "--random-policy", 15)) {
      if (arg[15] == '=') {
        const char *begin = arg + 16, *end = begin + std::strlen(begin);
        const auto parsed = std::from_chars(begin, end, seed);
        if (parsed.ec != std::errc() || parsed.ptr != end) {
          std::fprintf(stderr, "tactix-public-random: invalid seed\n");
          return 1;
        }
      } else if (arg[15]) {
        std::fprintf(stderr, "tactix-public-random: unknown option '%s'\n", arg);
        return 1;
      }
    } else if (arg[0] == '-' && arg[1]) {
      std::fprintf(stderr, "tactix-public-random: unsupported option '%s'\n", arg);
      std::puts("s UNKNOWN");
      return 1;
    } else if (!cnf_path) cnf_path = arg;
    else if (!proof_path) proof_path = arg;
    else {
      std::fprintf(stderr, "usage: tactix-public-random [--random-policy=SEED] "
                           "[--pbp|--auto-proof|--dsr|--drat|--lrat] CNF [PROOF]\n");
      return 1;
    }
  }
  if (!cnf_path) {
    std::fputs("usage: tactix-public-random [--random-policy=SEED] CNF [PROOF]\n",
               stderr);
    std::puts("s UNKNOWN");
    return 1;
  }

  const uint32_t variables = declared_variables(cnf_path);
  tactix_c_handle *solver = tactix_init();
  if (!solver) return 1;
  FILE *provenance = std::tmpfile();
  if (!provenance || tactix_bind_structural_provenance(solver, provenance) != 0 ||
      tactix_set_guidance_publication(solver, TACTIX_GUIDANCE_NATIVE) != 0 ||
      (proof_path && tactix_proof(solver, proof_path, proof_format) != 0)) {
    std::fputs("tactix-public-random: setup failed\n", stderr);
    std::puts("s UNKNOWN");
    if (provenance) std::fclose(provenance);
    tactix_release(solver);
    return 1;
  }
  FILE *input = std::fopen(cnf_path, "r");
  if (!input || tactix_read_dimacs(solver, input) != 0) {
    std::fprintf(stderr, "tactix-public-random: cannot read '%s'\n", cnf_path);
    std::puts("s UNKNOWN");
    if (input) std::fclose(input);
    std::fclose(provenance);
    tactix_release(solver);
    return 1;
  }
  std::fclose(input);

  Coverage coverage;
  coverage.rules.resize(static_cast<size_t>(tactix_rule_count()));
  coverage.effective_rules.resize(static_cast<size_t>(tactix_rule_count()));
  const Profile profile = static_cast<Profile>(seed % static_cast<unsigned>(Profile::Count));
  const int result = public_random_solve(solver, seed, variables, coverage);
  std::puts("c tactix-public-random");
  print_coverage(seed, profile, coverage);
  if (result == 10) {
    std::puts("s SATISFIABLE");
    print_model(solver, variables);
  } else if (result == 20) {
    std::puts("c proof-route pbp");
    std::puts("s UNSATISFIABLE");
  } else {
    std::puts("s UNKNOWN");
  }
  tactix_release(solver);
  std::fclose(provenance);
  return result == 10 ? 10 : result == 20 ? 20 : 0;
}
