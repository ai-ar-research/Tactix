#include "unit_test.hpp"
#include "kernel_fixture.hpp"
#include "rules/factor/fx.hpp"
#include "rules/factor/factor.hpp"
#include "policies/default_policy.hpp"
#include "frontend/parse.hpp"
#include "policies/heuristics.hpp"
#include <random>

static std::vector<std::vector<int>> matrix_clauses(FxGate gate, unsigned contexts,
                                                  unsigned signs) {
  std::vector<std::vector<int>> clauses;
  const int selector = signs & 1 ? -1 : 1;
  const int then_literal = signs & 2 ? -2 : 2;
  const int else_literal = gate == FxGate::Xor ? -then_literal : signs & 4 ? -3 : 3;
  for (unsigned i = 0; i < contexts; ++i) {
    const int phase = i & 1 ? -1 : 1;
    clauses.push_back({selector, phase * else_literal, 4 + int(i)});
    clauses.push_back({-selector, phase * then_literal, 4 + int(i)});
  }
  return clauses;
}

static Kernel matrix(FxGate gate, unsigned contexts, unsigned signs = 0,
                     bool mixed = false) {
  Kernel k = kernel_with_vars(3 + contexts);
  const auto clauses = matrix_clauses(gate, contexts, signs);
  for (size_t i = 0; i < clauses.size(); ++i) {
    std::vector<Lit> literals;
    for (int l : clauses[i]) literals.push_back(internalize(l));
    kernel_add_large_clause(k, literals.data(), literals.size(), mixed && ((i / 2) & 1),
                            0, ++k.next_clause_id);
  }
  return k;
}

static bool truth(Lit l, unsigned mask) {
  return bool(mask & (1u << var_of(l))) != negated(l);
}

static std::optional<FxPlan> find(Kernel &k, Lit selector,
                                   const FxOptions &options, uint64_t &remaining) {
  std::vector<Ref> sources;
  for (auto [ref, clause] : arena_records(k.arena)) sources.push_back(ref);
  return factor_find_fx(k, selector, sources, options, remaining);
}

static int publish(Kernel &k, const FxPlan &plan, uint64_t &remaining) {
  int output = 0;
  const auto &rule = tactix_rules()[tactix_rule_id("ExtendedResolution")];
  apply_rule_structured_preserve_startup(rule, k, "FX fixture", [&] {
    output = factor_publish_fx(k, plan, remaining);
    return output ? ApplyResult::Ok : ApplyResult::NoOp;
  });
  return output;
}

static bool satisfied(const Kernel &k, unsigned mask) {
  for (auto [ref, clause] : arena_records(k.arena)) {
    if (clause->garbage()) continue;
    bool yes = false;
    for (Lit l : *clause) yes |= truth(l, mask);
    if (!yes) return false;
  }
  for (Lit l = 0; l < 2 * k.nvars; ++l)
    for (const WatchEntry w : k.watches.entries(l))
      if (w.binary() && !truth(l, mask) && !truth(w.other(), mask)) return false;
  return true;
}

TEST(publication_projection_and_models) {
  for (FxGate gate : {FxGate::Xor, FxGate::Ite})
  for (unsigned contexts : {4, 6}) for (unsigned signs = 0; signs < 8; ++signs) {
    Kernel k = matrix(gate, contexts, signs);
    std::vector<std::vector<Lit>> original;
    for (auto [ref, clause] : arena_records(k.arena))
      original.emplace_back(clause->begin(), clause->end());
    const Var variables = k.nvars;
    FxOptions options;
    options.gates = unsigned(gate);
    options.minimum_reduction = contexts == 4 ? 0 : 1;
    uint64_t remaining = UINT64_MAX;
    auto plan = find(k, internalize(1), options, remaining);
    assert(plan && publish(k, *plan, remaining) > 0 && k.nvars == variables + 1);
    for (const FxContext &context : plan->contexts) {
      assert(clause_at(k.arena, context.positive)->garbage());
      assert(clause_at(k.arena, context.negative)->garbage());
    }
    // Check the actual published formula, existentially projecting the fresh
    // variable, rather than only checking the planner's proposed quotients.
    for (unsigned mask = 0; mask < (1u << variables); ++mask) {
      bool before = true;
      for (const auto &clause : original) {
        bool yes = false;
        for (Lit l : clause) yes |= truth(l, mask);
        before &= yes;
      }
      assert(before == (satisfied(k, mask) || satisfied(k, mask | (1u << variables))));
    }
    DefaultPolicy policy;
    assert(policy.solve(k, nullptr) == 10);
    kernel_extend_model(k);
    for (const auto &clause : original) {
      bool yes = false;
      for (Lit l : clause) yes |= k.model_values[l] > 0;
      assert(yes);
    }
  }
}

TEST(stale_and_malformed_publication_declines) {
  Kernel k = matrix(FxGate::Xor, 6);
  FxOptions options;
  uint64_t remaining = UINT64_MAX;
  const auto plan = find(k, internalize(1), options, remaining);
  assert(plan);
  const auto variables = k.nvars;
  const auto clauses = k.next_clause_id;
  const auto words = k.arena.size();
  auto decline = [&](const FxPlan &bad) {
    assert(!factor_publish_fx(k, bad, remaining));
    assert(k.nvars == variables && k.next_clause_id == clauses && k.arena.size() == words);
    for (const FxContext &context : plan->contexts)
      assert(!clause_at(k.arena, context.positive)->garbage() &&
             !clause_at(k.arena, context.negative)->garbage());
  };
  FxPlan bad = *plan;
  bad.owner = nullptr;
  decline(bad);
  bad = *plan; ++bad.arena_generation;
  decline(bad);
  bad = *plan; ++bad.contexts[0].positive;
  decline(bad); // An in-bounds word offset is not necessarily a record start.
  bad = *plan; ++bad.contexts[0].positive_id;
  decline(bad);
  bad = *plan; bad.contexts[0].negative = bad.contexts[0].positive;
  decline(bad);
  bad = *plan; bad.contexts[0].remainder[0] = internalize(9);
  decline(bad);
  remaining = 0;
  decline(*plan);
}

TEST(native_fx_competes_with_and_and_respects_budget) {
  for (FxGate gate : {FxGate::Xor, FxGate::Ite}) {
    Kernel k = matrix(gate, 6);
    k.factor_fx = true;
    k.factor_fx_options.gates = unsigned(gate);
    const auto &rule = tactix_rules()[tactix_rule_id("Factor")];
    const Var before = k.nvars;
    const auto result = apply_rule_structured_preserve_startup(rule, k,
        "native FX", [&] { return factor_apply(k); });
    assert(result == ApplyResult::Ok && k.nvars == before + 1);
    assert(k.structural_action_sequence == 1); // FX won over the AND grid.
    assert(!k.factor_completed && k.large_watches_connected);
    assert(k.stats.factorings == 1);
  }
  Kernel k = matrix(FxGate::Xor, 6);
  k.factor_fx = true;
  k.work_budget_bounded = true;
  k.work_budget = 0;
  const auto ids = k.next_clause_id;
  assert(factor_apply(k) == ApplyResult::NoOp);
  assert(k.nvars == 9 && k.next_clause_id == ids);
  assert(k.large_watches_connected);
}

TEST(native_fx_honors_the_coupled_factor_bound) {
  for (bool selected : {false, true}) {
    Kernel k = matrix(FxGate::Xor, 6); // FX reduction 2; AND reduction 1.
    k.factor_fx = true;
    DefaultFactorMatch selection;
    selection.set_bound(2);
    if (selected) k.factor_selection = &selection;
    else k.factor_bound = 2;
    const auto ids = k.next_clause_id;
    assert(factor_apply(k) == ApplyResult::NoOp);
    assert(k.nvars == 9 && k.next_clause_id == ids);
  }
}

TEST(learned_fx_material_rearms_seeds_only_when_enabled) {
  for (bool enabled : {false, true}) {
    Kernel k = kernel_with_vars(3);
    std::fill(k.factor_marks.begin(), k.factor_marks.end(), uint8_t{0});
    k.factor_fx = enabled;
    const Lit clause[] = {internalize(1), internalize(-2), internalize(3)};
    kernel_add_large_clause(k, clause, 3, true, 3, ++k.next_clause_id);
    for (Lit l : clause) assert(bool(k.factor_marks[l]) == enabled);
  }
}

TEST(collection_invalidates_plan_and_rediscovery_succeeds) {
  Kernel k = matrix(FxGate::Xor, 6);
  kernel_delete_clause(k, clause_at(k.arena, 0));
  FxOptions options;
  uint64_t remaining = UINT64_MAX;
  const auto old = find(k, internalize(1), options, remaining);
  assert(old && old->contexts.size() == 5);
  kernel_collect_garbage(k);
  const auto clauses = k.next_clause_id;
  assert(!factor_publish_fx(k, *old, remaining));
  assert(k.next_clause_id == clauses && k.nvars == 9);
  const auto current = find(k, internalize(1), options, remaining);
  assert(current && publish(k, *current, remaining) > 0);
}

TEST(signed_gate_plans) {
  for (FxGate gate : {FxGate::Xor, FxGate::Ite}) for (unsigned signs = 0; signs < 8; ++signs) {
    Kernel k = matrix(gate, 6, signs, true);
    FxOptions options;
    options.gates = unsigned(gate);
    uint64_t remaining = UINT64_MAX;
    const auto plan = find(k, internalize(1), options, remaining);
    assert(plan && plan->gate == gate && plan->contexts.size() == 6 && plan->reduction() == 2);
    std::vector<Ref> parents;
    for (const FxContext &context : plan->contexts) {
      assert(clause_at(k.arena, context.positive)->id() == context.positive_id);
      assert(clause_at(k.arena, context.negative)->id() == context.negative_id);
      parents.push_back(context.positive);
      parents.push_back(context.negative);
    }
    std::sort(parents.begin(), parents.end());
    assert(std::adjacent_find(parents.begin(), parents.end()) == parents.end());
    // Independent exhaustive equivalence after substituting the exact gate
    // function; this checks both quotient polarities and all input models.
    for (unsigned mask = 0; mask < (1u << k.nvars); ++mask) {
      bool original = true, quotient = true;
      for (auto [ref, clause] : arena_records(k.arena)) {
        bool satisfied = false;
        for (Lit l : *clause) satisfied |= truth(l, mask);
        original &= satisfied;
      }
      const bool output = gate == FxGate::Xor
          ? truth(plan->left, mask) != truth(plan->right, mask)
          : truth(plan->left, mask) ? truth(plan->right, mask) : truth(plan->otherwise, mask);
      for (const FxContext &context : plan->contexts) {
        bool satisfied = output != context.negate_output;
        for (Lit l : context.remainder) satisfied |= truth(l, mask);
        quotient &= satisfied;
      }
      assert(original == quotient);
    }
    options.scope = FxClauseScope::Irredundant;
    assert(!find(k, internalize(1), options, remaining));
    options.scope = FxClauseScope::Redundant;
    assert(!find(k, internalize(1), options, remaining));
  }
}

TEST(zero_growth_and_work_limits) {
  Kernel k = matrix(FxGate::Xor, 4);
  FxOptions options;
  options.gates = unsigned(FxGate::Xor);
  uint64_t remaining = UINT64_MAX;
  assert(!find(k, internalize(-1), options, remaining));
  options.minimum_reduction = 0;
  auto plan = find(k, internalize(-1), options, remaining);
  assert(plan && plan->reduction() == 0);
  const auto clauses = k.next_clause_id;
  remaining = 1;
  assert(!find(k, internalize(1), options, remaining));
  assert(!remaining && k.next_clause_id == clauses);
  options.gates = 4;
  assert(!options.valid());
}

TEST(deleted_and_repeated_occurrences) {
  Kernel k = matrix(FxGate::Xor, 6);
  std::vector<Ref> sources;
  for (auto [ref, clause] : arena_records(k.arena)) sources.push_back(ref);
  const auto original_sources = sources;
  sources.insert(sources.end(), original_sources.begin(), original_sources.end());
  FxOptions options;
  options.gates = unsigned(FxGate::Xor);
  uint64_t remaining = UINT64_MAX;
  auto plan = factor_find_fx(k, internalize(1), sources, options, remaining);
  assert(plan && plan->contexts.size() == 6);
  kernel_delete_clause(k, clause_at(k.arena, original_sources[0]));
  plan = factor_find_fx(k, internalize(1), sources, options, remaining);
  assert(plan && plan->contexts.size() == 5);
  kernel_delete_clause(k, clause_at(k.arena, original_sources[2]));
  assert(!factor_find_fx(k, internalize(1), sources, options, remaining));
}

int main(int argc, char **argv) {
  if (argc == 1) {
    std::printf("%zu tests passed\n", run_tests());
    return 0;
  }
  assert(argc >= 5 && argc <= 8);
  const bool native = argc >= 7 && !std::strcmp(argv[6], "native");
  const std::string stem = argv[1];
  const FxGate gate = !std::strcmp(argv[2], "xor") ? FxGate::Xor : FxGate::Ite;
  const bool sat = !std::strcmp(argv[3], "sat");
  const bool mixed = argc >= 6 && !std::strcmp(argv[5], "mixed");
  const bool derived = mixed || (argc >= 6 && !std::strcmp(argv[5], "learned"));
  const unsigned contexts = argc >= 6 && !std::strcmp(argv[5], "zero") ? 4 : 6;
  const bool randomized = argc == 8;
  std::mt19937 rng(randomized ? unsigned(std::strtoul(argv[7], nullptr, 10)) : 0);
  const unsigned variables = randomized ? 16 : derived ? 12 : 11;
  auto parents = matrix_clauses(gate, contexts, unsigned(std::atoi(argv[4])));
  if (randomized) {
    for (unsigned context = 0; context < contexts; ++context) {
      std::vector<int> extra{13, 14, 15, 16};
      std::shuffle(extra.begin(), extra.end(), rng);
      extra.resize(rng() % 5);
      for (int &literal : extra) if (rng() & 1) literal = -literal;
      for (unsigned side = 0; side < 2; ++side) {
        auto &parent = parents[2 * context + side];
        parent.insert(parent.end(), extra.begin(), extra.end());
        std::shuffle(parent.begin(), parent.end(), rng);
      }
    }
    std::shuffle(parents.begin(), parents.end(), rng);
  }
  std::vector<std::vector<int>> clauses;
  for (const auto &parent : parents) {
    if (!derived) clauses.push_back(parent);
    else for (int guard : {12, -12}) {
      clauses.push_back(parent);
      clauses.back().push_back(guard);
    }
  }
  clauses.push_back({10, 11});
  if (!sat) {
    clauses.push_back({10, -11});
    clauses.push_back({-10, 11});
    clauses.push_back({-10, -11});
  }
  if (randomized) {
    for (auto &clause : clauses) std::shuffle(clause.begin(), clause.end(), rng);
    std::shuffle(clauses.begin(), clauses.end(), rng);
  }
  FILE *input = std::fopen((stem + ".cnf").c_str(), "w");
  assert(input);
  std::fprintf(input, "p cnf %u %zu\n", variables, clauses.size());
  for (const auto &clause : clauses) {
    for (int l : clause) std::fprintf(input, "%d ", l);
    std::fputs("0\n", input);
  }
  std::fclose(input);
  Kernel k;
  ScopedHeuristics h(k);
  Proof proof;
  assert(proof.open((stem + ".proof").c_str(), ProofFormat::DsrAscii));
  k.proof = &proof;
  proof.bind_variable_map(&k.internal_to_external);
  proof.bind_external_variable_map(&k.external_to_internal);
  input = std::fopen((stem + ".cnf").c_str(), "r");
  assert(input && !parse_dimacs(input, k));
  std::fclose(input);
  // Genuine learned parents: each C is RUP from the retained IRR pair
  // (C or guard), (C or not guard), rather than just setting its RED bit.
  if (derived) for (const auto &parent : parents) {
    std::vector<Lit> literals;
    for (int l : parent) literals.push_back(kernel_internalize(k, l));
    const uint64_t id = ++k.next_clause_id;
    proof.add_rup(id, literals.data(), literals.size());
    kernel_add_large_clause(k, literals.data(), literals.size(), true, 3, id);
  }
  FxOptions options;
  options.gates = unsigned(gate);
  options.minimum_reduction = contexts == 4 ? 0 : 1;
  if (derived && !mixed) options.scope = FxClauseScope::Redundant;
  uint64_t remaining = UINT64_MAX;
  const auto plan = find(k, internalize(1), options, remaining);
  assert(plan);
  if (derived) {
    bool has_original = false, has_learned = false;
    for (const FxContext &context : plan->contexts)
      for (Ref ref : {context.positive, context.negative}) {
        has_original |= !clause_at(k.arena, ref)->redundant();
        has_learned |= clause_at(k.arena, ref)->redundant();
      }
    assert(has_learned && has_original == mixed);
  }
  if (native) {
    k.factor_fx = true;
    k.factor_fx_options = options;
    const auto &rule = tactix_rules()[tactix_rule_id("Factor")];
    const Var before = k.nvars;
    const auto result = apply_rule_structured_preserve_startup(rule, k,
        "native FX fixture", [&] { return factor_apply(k); });
    assert(result == ApplyResult::Ok && k.nvars > before);
    assert(k.structural_action_sequence > 0);
  } else assert(publish(k, *plan, remaining) > 0);
  DefaultPolicy policy;
  assert(policy.solve(k, nullptr) == (sat ? 10 : 20));
  proof.flush();
  if (sat) {
    kernel_extend_model(k);
    FILE *model = std::fopen((stem + ".model").c_str(), "w");
    assert(model);
    for (int v = 1; v <= int(variables); ++v)
      std::fprintf(model, "%d ", k.model_values[internalize(v)] > 0 ? v : -v);
    std::fputs("0\n", model);
    std::fclose(model);
  }
  return 0;
}
