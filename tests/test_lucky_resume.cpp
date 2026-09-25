// A whole-CNF-validated model is a terminal SAT state, not a BCP-derived
// decision trail. A partial Restart must rescan its retained prefix.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "rules/lucky/model.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/restart/restart.hpp"

#include <cassert>
#include <cstdio>

using namespace tactix;

int main() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(1), internalize(-2));
  // The total assignment satisfies the binary, but -2 was chosen by the
  // private model search rather than derived from the earlier decision -1.
  lucky_publish_model(k, {0, 0});
  assert(k.trail_tainted && k.level == 2);
  assert(kernel_propagation_fixpoint(k));
  kernel_check_invariants(k);

  assert(restart_apply_to(k, 1) == ApplyResult::Ok);
  assert(k.propagate_head == 0);
  assert(!kernel_propagation_fixpoint(k));
  assert(propagate_apply(k) == ApplyResult::Ok);
  assert(k.value(internalize(-2)) > 0);
  assert(k.assigned[1].level == 1);
  kernel_check_invariants(k);
  puts("test_lucky_resume OK");
  return 0;
}
