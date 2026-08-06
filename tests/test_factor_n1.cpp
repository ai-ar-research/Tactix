// N1 REGRESSION: Factor's admission heuristic must not live inside the rule.
//
// As a NoOp inside `factor_apply` it is an N1 breach, not a tuning choice:
// `factor_guard` reports APPLICABLE, `factor_instances` offers its canonical
// instance, and applying that instance does NOTHING on any binary-sparse
// formula. So no caller of the public rule API — an LLM policy, an RL policy,
// RandomPolicy — can apply a legal factoring there at all. The N1 carve-out
// permits the built-in POLICY to decline a choice; it does not permit a RULE
// to refuse everyone, which is a capability removed.
//
// The predicate lives in DefaultPolicy and reaches the rule as
// `Kernel::factor_admit`, which DEFAULTS TRUE. This test pins that default:
// a formula BELOW the density threshold, carrying a genuine factorable grid,
// must factor when driven through the rule's own surface.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/factor/factor.hpp"
#include "rules/registry.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>

using namespace tactix;

// A 2x3 grid over factors {a, b} and remainders {x, y, z}: the six clauses
// (a|x) (a|y) (a|z) (b|x) (b|y) (b|z). delta = m*n - (m+n) = 6 - 5 = 1 > 0, so
// it is worth applying. Padding variables keep the BINARY DENSITY far below the
// shipped threshold of 6 binaries per variable, which is the condition that used
// to silence the rule.
static void build_sparse_factorable(Kernel &k, uint32_t pad_vars) {
  kernel_reserve_vars(k, 5 + pad_vars);
  const Lit a = make_lit(0, true), b = make_lit(1, true);
  const Lit x = make_lit(2, true), y = make_lit(3, true), z = make_lit(4, true);
  for (const Lit f : {a, b})
    for (const Lit r : {x, y, z})
      kernel_add_binary_clause(k, f, r);
}

int main() {
  // The gate was `binary_ids.size() >= 6 * num_output_vars`. With 6 binaries and
  // 205 variables the ratio is ~0.03 — two orders of magnitude below it, so the
  // pre-fix build NoOps here with certainty rather than by luck.
  Kernel k;
  build_sparse_factorable(k, 200);
  assert(k.binary_ids.size() < 6ull * kernel_num_output_vars(k) &&
         "test is vacuous unless the formula is genuinely below the threshold");

  // Drive the REGISTRY surface — guard, then instances(), then
  // apply_instance() — because that is exactly what `tactix_rules_applicable`
  // / `tactix_apply_instance` route an external policy through. Testing
  // `factor_apply` directly would miss the point: the breach was that the
  // advertised instance could not be applied.
  const std::vector<RuleVTable> &rules = tactix_rules();
  const RuleVTable *factor = nullptr;
  for (const RuleVTable &rv : rules)
    if (!std::strcmp(rv.name, "Factor"))
      factor = &rv;
  assert(factor && "Factor must be registered");
  assert(factor->guard(k) && "guard must hold at level 0 on a consistent formula");

  uint64_t inst[4];
  const uint32_t n = factor->instances ? factor->instances(k, inst, 4) : 0;
  assert(n >= 1 && "N1: instances() must offer at least the canonical instance");

  const uint64_t before = k.stats.factorings;
  const ApplyResult r = factor->apply_instance(k, inst[0]);

  // The breach: this returned NoOp with zero factorings, while the guard said
  // the rule was applicable and instances() had just offered this very payload.
  assert(r == ApplyResult::Ok &&
         "N1 BREACH: a legal factoring is not applicable through the rule API "
         "on a binary-sparse formula");
  assert(k.stats.factorings > before && "apply reported Ok but factored nothing");

  std::printf("test_factor_n1: all passed (%llu factorings on a formula at "
              "%.3f binaries/var)\n",
              (unsigned long long)(k.stats.factorings - before),
              (double)k.binary_ids.size() / kernel_num_output_vars(k));
  return 0;
}
