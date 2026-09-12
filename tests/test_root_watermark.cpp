// Actual scheduler admission and trail-prefix consumers across root folding.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "rules/propagate/propagate.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace tactix;

static void scheduling_watermark() {
  for (bool fold : {false, true}) {
    Kernel k;
    ScopedHeuristics owner(k);
    kernel_reserve_vars(k, 12);
    for (Var v = 0; v < 8; v++)
      kernel_assign_unit(k, make_lit(v, v & 1));
    assert(propagate_apply(k) == ApplyResult::Ok);
    heuristics_deactivate_inactive_vars(k);
    heuristics_compact_random_order_fixed(k);
    DefaultPolicy policy;
    auto &scheduler = policy.inprocess();
    scheduler.pose_initialized();
    auto &clock = scheduler.elim_clock;
    clock.rounds = 1;
    clock.next_conflicts = 100;
    clock.fixed_mark = kernel_root_fixed(k);
    clock.marks_mark = k.stats.bve_marks;
    clock.material_mark = k.subsume_material_count;
    k.stats.conflicts = 100;
    assert(!scheduler.want_phase(k, SlotGroup::Eliminate));
    for (unsigned repeat = 0; repeat < 3; repeat++) {
      assert(kernel_compact_variables(k, fold));
      heuristics_compact_random_order_fixed(k);
      assert(kernel_root_fixed(k) + k.fixed_external_count == 8);
      assert(!scheduler.want_phase(k, SlotGroup::Eliminate));
    }
    kernel_assign_unit(k, kernel_internalize(k, -9));
    assert(propagate_apply(k) == ApplyResult::Ok);
    // No other material source changes: this invitation is solely the new
    // proved root. A smaller physical trail must not hide it from Eliminate.
    assert(clock.marks_mark == k.stats.bve_marks);
    assert(clock.material_mark == k.subsume_material_count);
    assert(kernel_root_fixed(k) + k.fixed_external_count == 9);
    assert(scheduler.want_phase(k, SlotGroup::Eliminate));
    clock.fixed_mark = 9;
    assert(!scheduler.want_phase(k, SlotGroup::Eliminate));
    kernel_check_invariants(k);
  }
}

static void late_true_level_root() {
  Kernel k;
  kernel_reserve_vars(k, 6);
  kernel_assign_unit(k, internalize(1));
  kernel_assign_unit(k, internalize(-2));
  assert(propagate_apply(k) == ApplyResult::Ok);
  kernel_assign_decision(k, internalize(4));
  // The public stepping surface permits a true-level-zero assignment after
  // higher-level literals. The physical prefix count does not include it yet.
  kernel_assign_unit(k, internalize(3));
  assert(propagate_apply(k) == ApplyResult::Ok);
  const uint32_t before = kernel_root_fixed(k);
  assert(before == 2);
  assert(kernel_compact_variables(k, true));
  assert(kernel_root_fixed(k) + k.fixed_external_count == before);
  assert(kernel_internalize(k, 3) != INVALID_LIT);
  assert(kernel_external_value(k, internalize(3)) == 1);
  assert(k.frames[1] == 1 && k.level == 1);
  kernel_check_invariants(k);
  kernel_backtrack(k, 0);
  assert(kernel_root_fixed(k) + k.fixed_external_count == 3);
  assert(kernel_compact_variables(k, true));
  assert(k.nvars == 4 && k.fixed_external_count == 2);
  assert(kernel_internalize(k, 3) == INVALID_LIT);
  assert(kernel_root_fixed(k) + k.fixed_external_count == 3);
  kernel_check_invariants(k);
}

int main(int argc, char **argv) {
  assert(argc == 1 || argc == 2);
  if (argc == 1 || !std::strcmp(argv[1], "watermark")) scheduling_watermark();
  if (argc == 1 || !std::strcmp(argv[1], "late")) late_true_level_root();
  std::puts("root-watermark: actual Eliminate admission and late-root compaction PASS");
}
