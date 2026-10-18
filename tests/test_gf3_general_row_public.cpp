#undef NDEBUG
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix
#include "frontend/parse.hpp"
#include "proof/proof.hpp"
#include "policies/random_controller_policy.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/semantic_extension/gf3_general_row_action.hpp"
#include "strategy/online_inprocess.hpp"

#include <cassert>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

using namespace tactix;
using namespace tactix::strategy;

namespace {

struct Formula {
  std::vector<std::vector<int32_t>> clauses;
  std::vector<std::vector<int32_t>> domains;
  uint32_t domain(std::vector<int32_t> values) {
    const uint32_t id = domains.size();
    domains.push_back(values);
    clauses.push_back(values);
    for (uint32_t a = 0; a != 3; ++a)
      for (uint32_t b = a + 1; b != 3; ++b)
        clauses.push_back({-values[a], -values[b]});
    return id;
  }
  void equation(std::vector<uint32_t> selected,
                std::vector<uint8_t> coefficients, uint8_t rhs) {
    uint64_t tuples = 1;
    for (size_t i = 0; i != selected.size(); ++i) tuples *= 3;
    for (uint64_t tuple = 0; tuple != tuples; ++tuple) {
      uint64_t rest = tuple;
      uint8_t sum = 0;
      std::vector<int32_t> clause;
      for (size_t i = 0; i != selected.size(); ++i) {
        const uint8_t value = rest % 3;
        rest /= 3;
        sum = (sum + coefficients[i] * value) % 3;
        clause.push_back(-domains[selected[i]][value]);
      }
      if (sum != rhs) clauses.push_back(std::move(clause));
    }
  }
};

Formula formula() {
  Formula f;
  for (int32_t first = 1; first <= 10; first += 3)
    f.domain({first, first + 1, first + 2});
  f.equation({0, 1, 3}, {1, 1, 1}, 0);
  f.equation({1, 2, 3}, {1, 1, 1}, 0);
  return f;
}

Kernel install() {
  Kernel k;
  for (const auto &external : formula().clauses) {
    std::vector<Lit> clause;
    for (const int32_t literal : external)
      clause.push_back(internalize(literal));
    ingest_clause(k, clause);
  }
  finish_input(k);
  return k;
}

Gf3GeneralRowActionLimits limits() {
  const Gf3GeneralRowConsequenceParameters p;
  const auto &g = p.gaussian;
  return {{{g.maximum_source_clauses, g.maximum_source_literals,
            g.maximum_domains, g.maximum_equations,
            g.maximum_equation_arity, g.maximum_tuples_per_equation},
           {{g.maximum_bridge_work, g.maximum_bridge_source_clauses,
             g.maximum_bridge_source_literals, g.maximum_bridge_domains,
             g.maximum_bridge_equations, g.maximum_bridge_terms,
             g.maximum_authentication_references,
             g.maximum_references_per_equation,
             {g.maximum_gaussian_work, g.maximum_fill_entries,
              g.maximum_intermediate_width, g.maximum_provenance_terms,
              g.maximum_derivations, g.maximum_retained_plan_bytes}},
            g.maximum_certificate_work, g.maximum_events,
            g.maximum_event_literals, g.maximum_active_clauses,
            g.maximum_retained_source_references, p.maximum_row_tuples,
            g.maximum_event_width, g.maximum_branch_domains,
            p.maximum_row_width},
           g.maximum_capture_work, p.maximum_duplicate_scan_work,
           g.maximum_revalidation_work, g.maximum_proof_additions,
           g.maximum_proof_literal_occurrences, p.maximum_arena_words,
           g.maximum_journal_bytes, g.maximum_identity_bytes,
           g.maximum_external_extent},
          g.maximum_session_keys, g.maximum_session_work,
          g.maximum_session_identity_bytes};
}

struct Bound {
  Kernel kernel = install();
  Gf3GeneralRowActionSession session;
  FILE *records = std::tmpfile();
  Bound() {
    assert(records);
    kernel.structural_provenance = records;
    kernel.gf3_general_row_action_session = &session;
  }
  ~Bound() { std::fclose(records); }
};

void pagination_replay_and_atomic_apply() {
  Bound b;
  Gf3GeneralRowActionOfferWindow window{1000, 1, 10000000, false, {}};
  const auto first =
      gf3_general_row_action_offer(b.kernel, b.session, limits(), window);
  assert(first.status == Gf3GeneralRowActionStatus::Applied);
  assert(first.offers.size() == 1 && first.have_resume_after);
  assert(!first.complete);
  window.have_after = true;
  window.after = first.resume_after;
  const auto second =
      gf3_general_row_action_offer(b.kernel, b.session, limits(), window);
  assert(second.offers.empty() || first.offers.front() < second.offers.front());

  OnlineInvocation step;
  step.id = 32;
  step.rule = "Gf3GeneralRowConsequence";
  step.parameters = Gf3GeneralRowConsequenceParameters{};
  step.granularity = InvocationGranularity::OneInstance;
  step.post_application = PostApplication::Propagate;
  step.gf3_general_row_instances = first.offers;
  step.budget = WorkLimit::bounded(1);
  const OnlineStrategyProgram program{{step}, {}};
  const std::string replay_text = serialize_online_strategy_program(program);
  OnlineStrategyProgram replay;
  std::string error;
  assert(deserialize_online_strategy_program(replay_text, replay, &error));
  assert(replay.sequence.front().gf3_general_row_instances == first.offers);

  const uint64_t before = b.kernel.next_clause_id;
  auto corrupt = first.offers.front();
  corrupt.identity.back() = corrupt.identity.back() == '0' ? '1' : '0';
  const auto rejected = gf3_general_row_action_apply(
      b.kernel, b.session, limits(), corrupt);
  assert(rejected.status == Gf3GeneralRowActionStatus::Stale);
  assert(b.kernel.next_clause_id == before && b.session.keys().empty());

  const auto applied = gf3_general_row_action_apply(
      b.kernel, b.session, limits(), first.offers.front());
  assert(applied.status == Gf3GeneralRowActionStatus::Applied);
  assert(applied.live.installed_clause_id &&
         applied.live.plan.projection.external_extent_unchanged &&
         applied.live.plan.projection.extension_variables.empty());
  assert(b.session.keys().size() == 1);
  record_gf3_general_row_action_feedback(b.kernel, applied);
  assert(b.kernel.stats.gf3_general_row_applied == 1);
}

void executor_requires_propagate_and_emits_dsr() {
  Bound source;
  const auto offered = gf3_general_row_action_offer(
      source.kernel, source.session, limits(), {1000, 1, 10000000, false, {}});
  assert(offered.offers.size() == 1);
  OnlineInvocation step;
  step.id = 32;
  step.rule = "Gf3GeneralRowConsequence";
  step.parameters = Gf3GeneralRowConsequenceParameters{};
  step.granularity = InvocationGranularity::OneInstance;
  step.gf3_general_row_instances = offered.offers;
  step.budget = WorkLimit::bounded(1);
  OnlineStrategyProgram program{{step}, {}};
  std::string error;
  assert(!validate_online_strategy_program(program, &error));
  step.post_application = PostApplication::Propagate;
  program.sequence = {step};

  Bound target;
  char path[] = "/tmp/tactix-gf3-row-public-XXXXXX";
  const int fd = mkstemp(path);
  assert(fd >= 0 && !close(fd));
  std::remove(path);
  Proof proof;
  assert(proof.open(path, ProofFormat::DsrAscii));
  proof.bind_variable_map(&target.kernel.internal_to_external);
  proof.bind_external_variable_map(&target.kernel.external_to_internal);
  target.kernel.proof = &proof;
  OnlineStrategyExecutor executor;
  assert(executor.configure(program, 1, ReconfigureMode::ResetRuntime,
                            &error));
  const auto result = executor.advance(target.kernel);
  assert(result.status == InvocationStatus::Completed &&
         result.applications == 1);
  assert(target.kernel.stats.gf3_general_row_applied == 1);
  proof.flush();
  target.kernel.proof = nullptr;
  FILE *stream = std::fopen(path, "rb");
  assert(stream && !std::fseek(stream, 0, SEEK_END) && std::ftell(stream) > 0);
  std::fclose(stream);
  std::remove(path);
}

void native_producer_reaches_exact_action() {
  bool reached = false;
  for (uint64_t seed = 1; seed <= 32 && !reached; ++seed) {
    Bound b;
    RandomControllerConfig config;
    config.parameters.rules = {"Gf3GeneralRowConsequence"};
    config.gf3_general_row_action_producer.enabled = true;
    config.gf3_general_row_action_producer.maximum_offers = 1;
    RandomControllerSelector selector(seed);
    controller::ActionRequest request;
    std::string error;
    const auto observation = controller::observe_kernel(b.kernel);
    assert(selector.choose(b.kernel, observation, config, request, error));
    if (request.type != controller::ActionType::Rule) continue;
    reached = true;
    assert(request.invocation.rule == "Gf3GeneralRowConsequence");
    assert(request.invocation.gf3_general_row_instances.size() == 1);
    assert(request.invocation.post_application == PostApplication::Propagate);
    assert(request.producer_effort.size() == 1 &&
           request.producer_effort.front().family ==
               "Gf3GeneralRowConsequence" &&
           request.producer_effort.front().offers == 1);
  }
  assert(reached);
}

bool apply_search_implication(Kernel &kernel,
                              Gf3GeneralRowActionSession &session,
                              FILE *records) {
  const auto offered = gf3_general_row_action_offer(
      kernel, session, limits(), {10000, 128, 10000000, false, {}});
  assert(offered.status == Gf3GeneralRowActionStatus::Applied &&
         !offered.offers.empty());
  bool implied = false;
  for (const auto &selection : offered.offers) {
    Gf3GeneralRowLiveRequest request;
    request.limits = limits().live;
    request.selection = {selection.outcome, selection.tuple};
    request.selected_identity = selection.identity;
    request.action_id = kernel.structural_action_sequence + 1;
    request.semantic_id = request.action_id;
    request.records = records;
    request.retained_owner = session.owner();
    const auto planned = gf3_general_row_live_inspect(kernel, request);
    if (planned.status != Gf3GeneralRowLiveStatus::Ready ||
        planned.plan.consequence.size() != 2)
      continue;
    for (unsigned pivot = 0; pivot < 2 && !implied; ++pivot) {
      const Lit false_literal = kernel_internalize(
          kernel, -planned.plan.consequence[pivot]);
      const Lit implied_literal = kernel_internalize(
          kernel, planned.plan.consequence[1 - pivot]);
      if (false_literal == INVALID_LIT || implied_literal == INVALID_LIT ||
          kernel.value(false_literal) || kernel.value(implied_literal))
        continue;
      kernel_assign_decision(kernel, false_literal);
      const auto propagated = propagate_apply(kernel);
      if (propagated == ApplyResult::Ok && !kernel.value(implied_literal)) {
        const auto applied = gf3_general_row_action_apply(
            kernel, session, limits(), selection);
        assert(applied.status == Gf3GeneralRowActionStatus::Applied &&
               applied.live.propagation_pending);
        assert(propagate_apply(kernel) == ApplyResult::Ok);
        assert(kernel.value(implied_literal) > 0);
        implied = true;
      }
      kernel_backtrack(kernel, 0);
      assert(propagate_apply(kernel) == ApplyResult::Ok);
    }
    if (implied) break;
  }
  return implied;
}

void retained_owner_search_implication() {
  Bound b;
  const auto first = gf3_general_row_action_offer(
      b.kernel, b.session, limits(), {1000, 1, 10000000, false, {}});
  assert(first.offers.size() == 1);
  const auto initialized = gf3_general_row_action_apply(
      b.kernel, b.session, limits(), first.offers.front());
  assert(initialized.status == Gf3GeneralRowActionStatus::Applied);
  assert(propagate_apply(b.kernel) == ApplyResult::Ok);
  assert(apply_search_implication(b.kernel, b.session, b.records));
}

void c_v11_deep_copies_typed_selection() {
  Bound source;
  const auto offered = gf3_general_row_action_offer(
      source.kernel, source.session, limits(), {1000, 1, 10000000, false, {}});
  assert(offered.offers.size() == 1);
  std::string identity = offered.offers.front().identity;

  tactix_c_handle *solver = tactix_init();
  FILE *records = std::tmpfile();
  assert(solver && records &&
         tactix_bind_structural_provenance(solver, records) == 0);
  for (const auto &clause : formula().clauses) {
    for (const int32_t literal : clause) tactix_add(solver, literal);
    tactix_add(solver, 0);
  }
  const Gf3GeneralRowConsequenceParameters p;
  const auto &g = p.gaussian;
  tactix_online_step_v11 cstep{};
  auto &base = cstep.base.base.base.base.base.base.base.base.base.base;
  base.step_id = 32;
  base.rule_id = 43;
  base.parameters.kind =
      TACTIX_ONLINE_PARAMETERS_GF3_GENERAL_ROW_CONSEQUENCE;
  auto &c = base.parameters.value.gf3_general_row_consequence;
  c.gaussian = {
      g.maximum_source_clauses, g.maximum_source_literals,
      g.maximum_domains, g.maximum_equations, g.maximum_equation_arity,
      g.maximum_tuples_per_equation, g.maximum_bridge_work,
      g.maximum_bridge_source_clauses, g.maximum_bridge_source_literals,
      g.maximum_bridge_domains, g.maximum_bridge_equations,
      g.maximum_bridge_terms, g.maximum_authentication_references,
      g.maximum_references_per_equation, g.maximum_gaussian_work,
      g.maximum_fill_entries, g.maximum_intermediate_width,
      g.maximum_provenance_terms, g.maximum_derivations,
      g.maximum_retained_plan_bytes, g.maximum_certificate_work,
      g.maximum_events, g.maximum_event_literals, g.maximum_active_clauses,
      g.maximum_retained_source_references, g.maximum_event_width,
      g.maximum_branch_domains, g.maximum_consequences,
      g.maximum_capture_work, g.maximum_revalidation_work,
      g.maximum_proof_additions, g.maximum_proof_literal_occurrences,
      g.maximum_journal_bytes, g.maximum_identity_bytes,
      g.maximum_external_extent, g.maximum_session_keys,
      g.maximum_session_work, g.maximum_session_identity_bytes};
  c.maximum_row_tuples = p.maximum_row_tuples;
  c.maximum_row_width = p.maximum_row_width;
  c.maximum_duplicate_scan_work = p.maximum_duplicate_scan_work;
  c.maximum_arena_words = p.maximum_arena_words;
  base.granularity = TACTIX_ONLINE_GRANULARITY_ONE;
  base.post_application = TACTIX_ONLINE_POST_PROPAGATE;
  base.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  base.repeat_limit = 1;
  base.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  base.budget = 1;
  const tactix_online_gf3_general_row_instance selected{
      offered.offers.front().outcome, offered.offers.front().tuple,
      identity.c_str()};
  cstep.gf3_general_row_instances = &selected;
  cstep.gf3_general_row_instance_count = 1;
  const tactix_online_program_v11 program{
      &cstep, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_configure_v11(
             solver, &program, 1, TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  identity.assign(identity.size(), '0');
  tactix_online_result result{};
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.rule_id == 43 && result.status == TACTIX_ONLINE_COMPLETED);
  tactix_release(solver);
  std::fclose(records);
}

void emit_public_fixture(const std::string &cnf_path,
                         const std::string &proof_path, bool search,
                         ProofFormat format = ProofFormat::DsrAscii) {
  Formula source = formula();
  source.clauses.push_back({13, 14});
  source.clauses.push_back({13, -14});
  source.clauses.push_back({-13, 14});
  source.clauses.push_back({-13, -14});
  std::ofstream cnf(cnf_path);
  assert(cnf);
  cnf << "p cnf 14 " << source.clauses.size() << '\n';
  for (const auto &clause : source.clauses) {
    for (const int32_t literal : clause) cnf << literal << ' ';
    cnf << "0\n";
  }
  cnf.close();

  Kernel kernel;
  const Formula live_source = formula();
  for (const auto &external : live_source.clauses) {
    std::vector<Lit> clause;
    for (const int32_t literal : external)
      clause.push_back(internalize(literal));
    ingest_clause(kernel, clause);
  }
  finish_input(kernel);
  kernel_reserve_vars(kernel, 14);
  // The four disjoint checker-only clauses close the otherwise satisfiable
  // fixture after the public action. Reserve their native IDs so every public
  // addition has the same ID in the emitted checker CNF.
  kernel.next_clause_id += 4;
  Gf3GeneralRowActionSession session;
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  kernel.gf3_general_row_action_session = &session;
  const auto offered = gf3_general_row_action_offer(
      kernel, session, limits(), {1000, 1, 10000000, false, {}});
  assert(offered.status == Gf3GeneralRowActionStatus::Applied &&
         offered.offers.size() == 1);
  Proof proof;
  assert(proof.open(proof_path.c_str(), format));
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  uint64_t input_id = 0;
  for (const auto &external : source.clauses) {
    std::vector<Lit> clause;
    for (int32_t literal : external) clause.push_back(internalize(literal));
    proof.seed_population(++input_id, clause.data(), clause.size());
  }
  assert(input_id == kernel.next_clause_id);
  proof.finish_input_transaction();
  kernel.proof = &proof;
  const auto applied = gf3_general_row_action_apply(
      kernel, session, limits(), offered.offers.front());
  assert(applied.status == Gf3GeneralRowActionStatus::Applied);
  if (search) {
    assert(propagate_apply(kernel) == ApplyResult::Ok);
    assert(apply_search_implication(kernel, session, records));
  }
  const Lit positive = internalize(13);
  const Lit negative = internalize(-13);
  proof.add_rup(++kernel.next_clause_id, &positive, 1, nullptr, 0);
  proof.add_rup(++kernel.next_clause_id, &negative, 1, nullptr, 0);
  proof.add_rup(++kernel.next_clause_id, nullptr, 0, nullptr, 0);
  proof.flush();
  kernel.proof = nullptr;
  std::fclose(records);
}

} // namespace

int main(int argc, char **argv) {
  assert(tactix_rules().size() == 47);
  assert(std::string(tactix_rules()[43].name) == "Gf3GeneralRowConsequence");
  const auto reference = reference_inprocess_program();
  const auto step = std::find_if(reference.begin(), reference.end(),
                                 [](const auto &candidate) {
    return candidate.id == 32;
  });
  assert(step != reference.end() &&
         step->rule == "Gf3GeneralRowConsequence" && !step->enabled);
  pagination_replay_and_atomic_apply();
  executor_requires_propagate_and_emits_dsr();
  native_producer_reaches_exact_action();
  retained_owner_search_implication();
  c_v11_deep_copies_typed_selection();
  if (argc == 3) emit_public_fixture(argv[1], argv[2], false);
  else if (argc == 4 && std::string(argv[1]) == "--proof-search")
    emit_public_fixture(argv[2], argv[3], true);
  else if (argc == 4 && std::string(argv[1]) == "--proof-search-pbp")
    emit_public_fixture(argv[2], argv[3], true, ProofFormat::PbpStreaming);
  else assert(argc == 1);
  std::puts("test_gf3_general_row_public: passed");
}
