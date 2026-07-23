// E2 Factor (BVA). S0: extension-variable allocation + model projection.
#include <cstdlib>
// Extension variables (added mid-solve by Factor) are assigned by search but
// projected out of the reported model — the v-line ranges over original vars only.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "rules/factor/factor.hpp"

#include <cassert>
#include <cstdio>

using namespace tactix;

static bool has_binary(const Kernel &k, Lit a, Lit b) {
  const uint32_t *ws = k.watches.arena.data() + k.watches.heads[a].offset;
  for (uint32_t i = 0; i < k.watches.heads[a].size;) {
    const Watch w{ws[i]};
    if (w.binary()) {
      if (w.other() == b)
        return true;
      i += 1;
    } else
      i += 2;
  }
  return false;
}

// kernel_new_extension_vars grows all per-var state additively, records the
// extension boundary, and keeps the model-output count at the original variables.
static void test_extension_var_boundary() {
  Kernel k;
  ScopedHeuristics _hg_k(k); // Track H · H7: attach H
  kernel_reserve_vars(k, 5); // 5 original vars: 0..4
  assert(k.nvars == 5);
  assert(k.first_extension_var == UINT32_MAX);
  assert(kernel_num_output_vars(k) == 5); // all original

  const Var first = kernel_new_extension_vars(k, 2); // add 2 extension vars
  assert(first == 5);
  assert(k.nvars == 7);
  assert(k.first_extension_var == 5);          // boundary set to first extension
  assert(kernel_num_output_vars(k) == 5);      // extension vars projected out
  assert(k.values.size() == 2 * 7);            // per-var state grew (by lit)
  assert(k.h->saved.size() == 7 && k.h->scores.score.size() == 7); // and others

  const Var f2 = kernel_new_extension_vars(k, 1); // add one more
  assert(f2 == 7);
  assert(k.nvars == 8);
  assert(k.first_extension_var == 5);          // boundary unchanged
  assert(kernel_num_output_vars(k) == 5);
}

// A 2×3 binary grid F={a,b} × R={p,q,r} (6 clauses) factors to 2 dividers + 3
// quotients (5 clauses) over a fresh x; originals are deleted.
static void test_factoring_2x3() {
  Kernel k;
  ScopedHeuristics _hg_k(k); // Track H · H7: attach H
  kernel_reserve_vars(k, 5); // a=0 b=1 p=2 q=3 r=4
  const Lit a = make_lit(0, false), b = make_lit(1, false);
  const Lit p = make_lit(2, false), q = make_lit(3, false), r = make_lit(4, false);
  const Lit R[3] = {p, q, r};
  for (const Lit f : {a, b})
    for (const Lit rr : R)
      kernel_add_binary_clause(k, f, rr, ++k.next_clause_id);

  assert(factor_guard(k));
  assert(factor_apply(k) == ApplyResult::Ok);
  assert(k.stats.factorings == 1);
  assert(k.nvars == 6 && k.first_extension_var == 5); // one fresh var x = 5
  const Lit x = make_lit(5, false), nx = make_lit(5, true);

  // dividers (x∨a),(x∨b) present
  assert(has_binary(k, x, a) && has_binary(k, x, b));
  // quotients (¬x∨p),(¬x∨q),(¬x∨r) present
  for (const Lit rr : R)
    assert(has_binary(k, nx, rr));
  // every original (f∨r) deleted
  for (const Lit f : {a, b})
    for (const Lit rr : R)
      assert(!has_binary(k, f, rr));
}

int main() {
  // bypass the density selectivity gate (raised to 8× with the BVE-last fix):
  // these unit tests exercise the factoring MECHANISM on tiny grids that sit
  // below the threshold, which is exactly what TACTIX_FACTOR_FORCE is for.
  setenv("TACTIX_FACTOR_FORCE", "1", 1);
  test_extension_var_boundary();
  test_factoring_2x3();
  std::printf("test_factor: all passed\n");
  return 0;
}
