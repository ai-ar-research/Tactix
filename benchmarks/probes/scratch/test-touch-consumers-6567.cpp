// Fixed shared-touch contract before any separate material tracker is added.
// No solver run, candidate implementation, or simulated native forward pass.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include <array>
#include <cassert>
#include <cstdio>

using namespace tactix;

static unsigned cases, snapshots;

static void exhaustive_event_contract() {
  // Four-event sequences over two variables and three distinct producer types.
  // A repeated event must still advance the shared epoch, unlike a native
  // false-to-true material flag. Querying any watermark never consumes events.
  for (unsigned sequence = 0; sequence < 1296; ++sequence) {
    Kernel k;
    kernel_reserve_vars(k, 2);
    const auto initial_factor = k.factor_marks;
    const uint64_t initial_factor_count = k.factor_literals_marked;
    const uint64_t initial_bve = k.stats.bve_marks;
    auto touched = k.touched, removed = k.removed, kept = k.touched_kept;
    uint64_t epoch = k.touch_epoch;
    uint64_t ordinary_count = k.stats.touch_marks;
    uint64_t kept_count = k.stats.touch_kept_marks;
    unsigned code = sequence;
    for (unsigned step = 0; step <= 4; ++step) {
      assert(k.touch_epoch == epoch);
      assert(k.touched == touched && k.removed == removed && k.touched_kept == kept);
      assert(k.stats.touch_marks == ordinary_count && k.stats.touch_kept_marks == kept_count);
      assert(k.factor_marks == initial_factor && k.factor_literals_marked == initial_factor_count);
      assert(k.stats.bve_marks == initial_bve);
      for (uint64_t watermark = 0; watermark <= epoch + 1; ++watermark)
        for (Var v = 0; v < 2; ++v) {
          assert(kernel_touched_since(k, v, watermark) == (touched[v] > watermark));
          assert(kernel_kept_since(k, v, watermark) == (kept[v] > watermark));
          assert(kernel_changed_since(k, v, watermark) ==
                 (touched[v] > watermark || removed[v] > watermark));
        }
      ++snapshots;
      if (step == 4)
        break;
      const unsigned event = code % 6;
      code /= 6;
      const Var v = event % 2;
      ++epoch;
      switch (event / 2) {
      case 0:
        kernel_touch_var(k, v);
        touched[v] = epoch;
        ++ordinary_count;
        break;
      case 1:
        kernel_removed_var(k, v);
        removed[v] = epoch;
        ++ordinary_count;
        break;
      case 2:
        kernel_touch_kept_var(k, v);
        kept[v] = epoch;
        ++kept_count;
        break;
      }
    }
    ++cases;
  }
}

static void deferred_activation_and_extension() {
  Kernel k;
  kernel_defer_cold_state(k);
  kernel_reserve_vars(k, 3);
  const Lit literals[] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, literals, 3, false, 0, 1);
  kernel_add_binary_clause(k, literals[0], literals[1], 2);
  assert(k.touch_epoch == 8 && k.stats.touch_marks == 8);
  assert(k.touched.empty() && k.removed.empty() && k.touched_kept.empty());
  kernel_materialize_cold_state(k);
  for (Var v = 0; v < 3; ++v) {
    assert(k.touched[v] == 8 && kernel_touched_since(k, v, 0));
    assert(k.removed[v] == 0 && k.touched_kept[v] == 0);
  }
  k.subsume_touch_mark = k.strengthen_touch_mark = k.bce_touch_mark = 8;
  k.ternary_touch_mark = k.instantiate_touch_mark = k.cce_touch_mark = 8;
  const Var fresh = kernel_new_extension_vars(k, 1);
  assert(fresh == 3 && k.touch_epoch == 9 && k.touched[fresh] == 9);
  assert(kernel_touched_since(k, fresh, 8) && !kernel_touched_since(k, 0, 8));
  assert(k.subsume_touch_mark == 8 && k.strengthen_touch_mark == 8 && k.bce_touch_mark == 8);
  assert(k.ternary_touch_mark == 8 && k.instantiate_touch_mark == 8 && k.cce_touch_mark == 8);
  ++cases;
}

static void root_notification_is_not_factor_or_removal() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  const uint64_t mark = k.touch_epoch, factor = k.factor_literals_marked;
  const uint64_t removal = k.stats.bve_marks;
  kernel_assign_unit(k, internalize(1));
  kernel_touch_sync_fixed(k);
  assert(k.touch_epoch == mark + 1 && k.touch_fixed_head == 1);
  assert(kernel_touched_since(k, 0, mark) && !kernel_touched_since(k, 1, mark));
  assert(k.factor_literals_marked == factor && k.stats.bve_marks == removal);
  kernel_touch_sync_fixed(k);
  assert(k.touch_epoch == mark + 1);
  ++cases;
}

static void publication_suppression_and_duplicate_events() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit a = internalize(1), b = internalize(2);
  const uint64_t epoch = k.touch_epoch, removal = k.stats.bve_marks;
  kernel_add_binary_clause(k, a, b, 1, false);
  assert(k.touch_epoch == epoch);
  kernel_add_binary_clause(k, a, b, 2, true);
  assert(k.touch_epoch == epoch + 2);
  const uint64_t factor = k.factor_literals_marked;
  kernel_add_binary_clause(k, a, b, 3, true);
  assert(k.touch_epoch == epoch + 4 && k.factor_literals_marked == factor);
  assert(k.stats.bve_marks == removal && kernel_binary_copies(k, a, b) == 3);
  const Lit large[] = {a, b, internalize(3)};
  kernel_add_large_clause(k, large, 3, true, 2, 4);
  assert(k.touch_epoch == epoch + 4); // default redundant band remains isolated
  kernel_add_large_clause(k, large, 3, false, 0, 5, false);
  assert(k.touch_epoch == epoch + 4);
  ++cases;
}

static void compaction_preserves_surviving_stamps() {
  Kernel k;
  ScopedHeuristics h(k);
  kernel_reserve_vars(k, 4);
  kernel_touch_var(k, 3);
  kernel_removed_var(k, 0);
  kernel_touch_kept_var(k, 2);
  const auto t = k.touched, r = k.removed, b = k.touched_kept;
  const uint64_t epoch = k.touch_epoch, factor = k.factor_literals_marked;
  k.var_status.assign(k.nvars, VarStatus::Active);
  k.var_status[1] = VarStatus::Eliminated;
  heuristics_deactivate_var(k, 1);
  --k.unassigned_active;
  assert(kernel_compact_variables(k));
  assert((k.internal_to_external == std::vector<Var>{0, 2, 3}));
  for (Var v = 0; v < 3; ++v) {
    const Var old = k.internal_to_external[v];
    assert(k.touched[v] == t[old] && k.removed[v] == r[old] && k.touched_kept[v] == b[old]);
  }
  assert(k.touch_epoch == epoch && k.factor_literals_marked == factor);
  kernel_check_invariants(k);
  ++cases;
}

int main() {
  exhaustive_event_contract();
  deferred_activation_and_extension();
  root_notification_is_not_factor_or_removal();
  publication_suppression_and_duplicate_events();
  compaction_preserves_surviving_stamps();
  assert(cases == 1300 && snapshots == 6480);
  std::printf("PASS shared-touch contract: %u cases, %u exhaustive snapshots; no material-tracker implementation\n", cases, snapshots);
}
