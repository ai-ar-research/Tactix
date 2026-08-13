// N1 regression: Factor admission lives wholly in the calling policy. A rule
// that guards true, advertises an instance and then NoOps on a strategy
// predicate removes a capability from every external policy.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/factor/factor.hpp"
#include "rules/registry.hpp"

#include <cassert>
#include <cstdio>
#include <algorithm>
#include <cstring>

using namespace tactix;

// A 2x3 grid with delta = m*n - (m+n) = 1 > 0, so the factoring is worth
// applying, plus padding that holds the binary density far below the shipped
// admission threshold of 6 binaries per variable.
static void build_sparse_factorable(Kernel &k, uint32_t pad_vars) {
  kernel_reserve_vars(k, 5 + pad_vars);
  const Lit a = make_lit(0, true), b = make_lit(1, true);
  const Lit x = make_lit(2, true), y = make_lit(3, true), z = make_lit(4, true);
  for (const Lit f : {a, b})
    for (const Lit r : {x, y, z})
      kernel_add_binary_clause(k, f, r);
}

int main() {
  Kernel k; // orders of magnitude under the gate: not a lucky pass
  build_sparse_factorable(k, 200);
  assert(k.binary_ids.size() < 6ull * kernel_num_output_vars(k) &&
         "test is vacuous unless the formula is genuinely below the threshold");

  // Drive the registry surface, the path an external policy takes; calling
  // `factor_apply` directly would not exercise the advertised instance.
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

  // An advertised instance must apply: NoOp here is the N1 breach.
  assert(r == ApplyResult::Ok &&
         "N1 BREACH: a legal factoring is not applicable through the rule API "
         "on a binary-sparse formula");
  assert(k.stats.factorings > before && "apply reported Ok but factored nothing");

  // `N1-FACTOR-INSTANCES`: the sweep is not the only addressable application.
  // Every unassigned, unretired literal is a legal seed, and seeding at one
  // applies at most that one factoring.
  {
    Kernel s;
    build_sparse_factorable(s, 4);
    uint64_t seeds[512];
    const uint32_t m = factor->instances(s, seeds, 512);
    assert(m == 1 + 2 * s.nvars &&
           "N1: canonical, then every unassigned literal as a seed");
    assert(seeds[0] == INSTANCE_CANONICAL);

    // DefaultPolicy's per-literal touch filter is a schedule narrowing, so it
    // must not reach the API: clear every factor mark (the state in which the
    // canonical sweep declines to schedule ANY seed) and seed explicitly.
    std::fill(s.factor_marks.begin(), s.factor_marks.end(), 0);
    s.touch_filter = true;
    const uint64_t canonical_before = s.stats.factorings;
    assert(factor->apply_instance(s, INSTANCE_CANONICAL) == ApplyResult::NoOp &&
           "the filtered canonical schedule must be empty for this to be "
           "an adversarial test of the instance surface");
    assert(s.stats.factorings == canonical_before);

    const Lit seed = make_lit(0, true); // divider `a` of the 2x3 grid
    assert(factor->apply_instance(s, lit_instance(s, seed)) ==
               ApplyResult::Ok &&
           "N1 BREACH: a legal seed is unreachable once DefaultPolicy's touch "
           "filter declines to schedule it");
    assert(s.stats.factorings == canonical_before + 1 &&
           "a literal instance applies exactly one factoring");
  }

  std::printf("test_factor_n1: all passed (%llu factorings on a formula at "
              "%.3f binaries/var)\n",
              (unsigned long long)(k.stats.factorings - before),
              (double)k.binary_ids.size() / kernel_num_output_vars(k));
  return 0;
}
