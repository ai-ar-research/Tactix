#include "unit_test.hpp"
#include "kernel_fixture.hpp"
#include "rules/factor/fx.hpp"

static Kernel matrix(FxGate gate, unsigned contexts, unsigned signs = 0,
                     bool mixed = false) {
  Kernel k = kernel_with_vars(3 + contexts);
  const int selector = signs & 1 ? -1 : 1;
  const int then_literal = signs & 2 ? -2 : 2;
  const int else_literal = gate == FxGate::Xor ? -then_literal : signs & 4 ? -3 : 3;
  for (unsigned i = 0; i < contexts; ++i) {
    const int phase = i & 1 ? -1 : 1;
    const Lit positive[] = {internalize(selector), internalize(phase * else_literal),
                            internalize(4 + int(i))};
    const Lit negative[] = {internalize(-selector), internalize(phase * then_literal),
                            internalize(4 + int(i))};
    kernel_add_large_clause(k, positive, 3, mixed && (i & 1), 0, ++k.next_clause_id);
    kernel_add_large_clause(k, negative, 3, mixed && (i & 1), 0, ++k.next_clause_id);
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
