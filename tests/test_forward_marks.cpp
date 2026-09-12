// C2's canonical admission producer versus its unrestricted public surface.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/strengthen/strengthen.hpp"
#include <cassert>
#include <cstdio>

using namespace tactix;

static void check(bool dense, bool filtered, bool actual_added_mark,
                  bool explicit_action) {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit lits[] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, lits, 3, false, 0);
  kernel_add_binary_clause(k, internalize(2), internalize(3));
  k.strengthen_touch_mark = k.touch_epoch;
  const uint64_t watermark = k.strengthen_touch_mark;
  kernel_clear_subsume_material(k);
  kernel_touch_var(k, var_of(internalize(3)));
  kernel_mark_subsume_material(k, var_of(internalize(3)));
  kernel_assign_unit(k, internalize(-1));
  assert(propagate_apply(k) == ApplyResult::Ok);
  const uint32_t entry = k.propagate_head;
  assert(!kernel_touched_since(k, 0, watermark));
  if (actual_added_mark) {
    kernel_touch_var(k, 0);
    kernel_mark_subsume_material(k, 0);
  }
  if (dense)
    kernel_flush_large_watches(k);
  k.touch_filter = filtered;
  const auto &rule = tactix_rules()[tactix_rule_id("Strengthen")];
  assert(rule.guard(k));
  uint64_t payload;
  assert(rule.instances(k, &payload, 1) == 1);
  const bool admitted = !filtered || actual_added_mark || explicit_action;
  const ApplyResult result = explicit_action ? rule.apply_instance(k, payload)
                                            : rule.apply(k);
  assert(result == (admitted ? ApplyResult::Ok : ApplyResult::NoOp));
  assert(k.stats.strengthen_checked == unsigned(admitted));
  assert(k.stats.strengthen_subsumed == unsigned(admitted));
  assert(kernel_touched_since(k, 0, watermark) == actual_added_mark);
  if (!admitted) {
    // A skipped canonical candidate is still a legal public action.
    assert(rule.instances(k, &payload, 1) == 1);
    assert(rule.apply_instance(k, payload) == ApplyResult::Ok);
    assert(k.stats.strengthen_subsumed == 1);
  }
  // The shared root-notification primitive remains available to C1 and CCE;
  // removing C2's consumer must not silently weaken those independent paths.
  kernel_touch_sync_fixed(k);
  assert(kernel_touched_since(k, 0, watermark));
  if (dense) {
    kernel_rebuild_large_watches(k);
    assert(default_resume_sparse_propagation(k, entry, true) == ApplyResult::Ok);
  }
  kernel_check_invariants(k);
  assert(default_solve(k) == 10);
  kernel_extend_model(k);
  assert(k.value(internalize(-1)) > 0);
  assert(k.value(internalize(2)) > 0 || k.value(internalize(3)) > 0);
  k.inconsistent = true;
  assert(!rule.guard(k));
}

int main() {
  for (bool dense : {false, true}) {
    check(dense, true, false, false);
    check(dense, true, true, false);
    check(dense, false, false, false);
    check(dense, true, false, true);
  }
  std::puts("Forward admission: eight fixed state/public-action/model cases PASS");
}
