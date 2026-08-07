// `H` (search guidance) is optional: with k.h == nullptr the solver is sound,
// just unguided. `H`-only policy ops become inapplicable, `H`-consuming reads
// fall back safely, and Decide degrades to index order so every unassigned
// literal stays a legal branch (N1).
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "rules/decide/decide.hpp"
#include "rules/propagate/propagate.hpp"
#include "policies/ops/rephase/rephase.hpp"
#include "rules/restart/restart.hpp"
#include "policies/ops/switchmode/switchmode.hpp"

#include <cassert>
#include <cstdio>

using namespace tactix;

static void test_h_optional_contracts() {
  Kernel k;
  k.h = nullptr;              // the default: the kernel owns no H
  assert(!k.h);
  kernel_reserve_vars(k, 4);  // reserve skips H growth when null
  kernel_check_invariants(k); // heuristics_check is a no-op with no H

  // a policy op that mutates H is inapplicable without H (plan §5)
  assert(!switchmode_guard(k));
  assert(!rephase_guard(k));
  assert(restart_reuse_level(k) == 0); // cannot rank: keep nothing

  Lit cand[8]; // Decide falls back to index order, positive phase
  const uint32_t n = decide_candidates(k, cand, 8);
  assert(n == 4);
  assert(cand[0] == make_lit(0, false)); // lowest-index unassigned, positive
  assert(cand[1] == make_lit(1, false));
  assert(cand[3] == make_lit(3, false));
  assert(decide_guard(k)); // the canonical pick is that same variable
  assert(decide_apply_canonical(k) == ApplyResult::Ok);
  assert(k.value(make_lit(0, false)) > 0); // var 0 decided true
}

static void test_unguided_root_refutation() {
  Kernel k;
  k.h = nullptr;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));  // x -> y
  kernel_add_binary_clause(k, internalize(-1), internalize(-2)); // x -> ¬y
  kernel_assign_unit(k, internalize(1));                         // x
  const ApplyResult r = propagate_apply(k);
  assert(r == ApplyResult::Unsat);
  assert(k.inconsistent);
}

int main() {
  test_h_optional_contracts();
  test_unguided_root_refutation();
  puts("test_no_heuristics OK");
  return 0;
}
