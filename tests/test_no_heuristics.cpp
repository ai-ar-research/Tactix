// Track H · H5: `H` (search guidance) is OPTIONAL. A solver with k.h == nullptr
// is fully sound, just unguided (plan rev 60). This pins the null-`H` contracts:
// the `H`-only policy operations become inapplicable, the `H`-consuming reads
// fall back safely, and Decide degrades to index order (N1 — every unassigned
// literal is still a legal branch). Soundness + proofs at scale are the
// `--random-policy --no-heuristics` fuzz leg; this is the fast regression guard.
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
  k.h = nullptr; // unguided (also the default now — the kernel owns no H)
  assert(!k.h);
  kernel_reserve_vars(k, 4); // stays H-free (reserve skips H growth when null)
  kernel_check_invariants(k); // heuristics_check is a no-op with no H

  // policy operations that MUTATE H are inapplicable without H (a pure
  // precondition of an H-only policy op, plan §5)
  assert(!switchmode_guard(k));
  assert(!rephase_guard(k));
  // reuse-level cannot rank without H, so a full restart (keep nothing)
  assert(restart_reuse_level(k) == 0);

  // Decide falls back to index order, positive phase
  Lit cand[8];
  const uint32_t n = decide_candidates(k, cand, 8);
  assert(n == 4);
  assert(cand[0] == make_lit(0, false)); // lowest-index unassigned var, positive
  assert(cand[1] == make_lit(1, false));
  assert(cand[3] == make_lit(3, false));
  // the canonical pick is that same lowest-index unassigned var
  assert(decide_guard(k));
  assert(decide_apply_canonical(k) == ApplyResult::Ok);
  assert(k.value(make_lit(0, false)) > 0); // var 0 decided true
}

// a tiny UNSAT with no H: x forces both y and ¬y -> a root conflict.
static void test_unguided_root_refutation() {
  Kernel k;
  k.h = nullptr; // unguided (the default)
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
