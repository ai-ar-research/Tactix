#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "rules/strengthen/strengthen.hpp"
#include "rules/subsume_forward/subsume_forward.hpp"
#include <cassert>
#include <cstdio>

using namespace tactix;

static void test_flip_clear_rearm_and_isolation() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  assert(k.subsume_material_count == 3);
  kernel_clear_subsume_material(k);
  const auto factor = k.factor_marks, retry = k.bve_retry;
  const uint64_t epoch = k.touch_epoch;
  kernel_mark_subsume_material(k, 0);
  kernel_mark_subsume_material(k, 0);
  assert(k.subsume_material_count == 4);
  assert(kernel_has_subsume_material(k, 0));
  assert(!kernel_has_subsume_material(k, 1));
  kernel_clear_subsume_material(k);
  assert(k.subsume_material_count == 4);
  kernel_rearm_subsume_material(k, 0);
  kernel_mark_subsume_material(k, 0);
  assert(k.subsume_material_count == 4);
  kernel_mark_subsume_material(k, 1);
  assert(k.subsume_material_count == 5);
  assert(k.factor_marks == factor && k.bve_retry == retry && k.touch_epoch == epoch);
  kernel_clear_subsume_material(k);
  kernel_touch_var(k, 0);
  kernel_touch_var(k, 0); // repeated generic retry is still a touch event
  kernel_removed_var(k, 1);
  kernel_touch_kept_var(k, 2);
  assert(k.touch_epoch == epoch + 4 && k.subsume_material_count == 5);
  for (Var v = 0; v < 3; ++v)
    assert(!kernel_has_subsume_material(k, v));
}

static void test_cold_activation_growth() {
  for (bool deferred : {false, true}) {
    Kernel k;
    if (deferred)
      kernel_defer_cold_state(k);
    kernel_reserve_vars(k, 3);
    const Lit lits[] = {internalize(1), internalize(2), internalize(3)};
    kernel_add_large_clause(k, lits, 3, false, 0, 1);
    kernel_add_binary_clause(k, lits[0], lits[1], 2);
    assert(k.subsume_material_count == 3);
    if (deferred)
      assert(k.subsume_material.empty());
    kernel_materialize_cold_state(k);
    assert((k.subsume_material == std::vector<uint8_t>{1, 1, 1}));
    assert(k.subsume_material_count == 3);
    kernel_clear_subsume_material(k);
    const Var fresh = kernel_new_extension_vars(k, 1);
    assert(fresh == 3 && k.subsume_material_count == 4);
    assert(kernel_has_subsume_material(k, fresh));
    for (Var v = 0; v < fresh; ++v)
      assert(!kernel_has_subsume_material(k, v));
    kernel_reserve_vars(k, 4); // repeated reserve creates no activation
    assert(k.subsume_material_count == 4);
  }
}

static void test_binary_and_fixed_producers() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_clear_subsume_material(k);
  kernel_assign_unit(k, internalize(1));
  kernel_touch_sync_fixed(k);
  assert(k.subsume_material_count == 3 && !kernel_has_subsume_material(k, 0));
  const Lit a = internalize(-1), b = internalize(2);
  kernel_add_binary_clause(k, a, b, 1, false);
  assert(k.subsume_material_count == 3);
  kernel_add_binary_clause(k, a, b, 2);
  assert(k.subsume_material_count == 5 && kernel_has_subsume_material(k, 0));
  kernel_add_binary_clause(k, negate(a), negate(b), 3);
  assert(k.subsume_material_count == 5); // polarity does not split the variable
  kernel_clear_subsume_material(k);
  kernel_add_binary_clause(k, a, b, 4);
  assert(k.subsume_material_count == 7); // duplicate publication is a real producer
}

static void test_large_publication_and_rewrite() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit original[] = {internalize(1), internalize(2), internalize(3), internalize(4)};
  kernel_clear_subsume_material(k);
  kernel_add_large_clause(k, original, 4, true, 2, 1);
  assert(k.subsume_material_count == 4);
  const Ref ref = kernel_add_large_clause(k, original, 4, false, 0, 2, false);
  assert(k.subsume_material_count == 4);
  k.next_clause_id = 2; // the fixture's two existing input identities
  kernel_replace_large_clause_in_place(k, ref, ++k.next_clause_id, original,
                                        3, 0, "material-test");
  assert(k.subsume_material_count == 7);
  for (Var v = 0; v < 4; ++v)
    assert(kernel_has_subsume_material(k, v) == (v < 3));
  kernel_clear_subsume_material(k);
  kernel_add_large_clause(k, original, 3, false, 0, 4);
  assert(k.subsume_material_count == 10);
}

static void test_compaction_preserves_flags_and_counter() {
  Kernel k;
  ScopedHeuristics h(k);
  kernel_reserve_vars(k, 4);
  kernel_clear_subsume_material(k);
  kernel_mark_subsume_material(k, 1); // retiring a marked variable does not recount
  kernel_mark_subsume_material(k, 3);
  k.var_status.assign(k.nvars, VarStatus::Active);
  k.var_status[1] = VarStatus::Eliminated;
  heuristics_deactivate_var(k, 1);
  --k.unassigned_active;
  assert(kernel_compact_variables(k));
  assert((k.internal_to_external == std::vector<Var>{0, 2, 3}));
  assert((k.subsume_material == std::vector<uint8_t>{0, 0, 1}));
  assert(k.subsume_material_count == 6);
  kernel_check_invariants(k);
}

static void test_public_instances_ignore_material_flags() {
  for (bool strengthen : {false, true}) {
    Kernel k;
    kernel_reserve_vars(k, 3);
    const Lit lits[] = {internalize(1), internalize(2), internalize(3)};
    const Ref victim = kernel_add_large_clause(k, lits, 3, false, 0, 1);
    kernel_add_binary_clause(k, lits[0], lits[1], 2);
    kernel_clear_subsume_material(k);
    const auto result = strengthen ? strengthen_try(k, victim) : subsume_forward_try(k, victim);
    assert(result == ApplyResult::Ok && k.subsume_material_count == 3);
    for (Var v = 0; v < 3; ++v)
      assert(!kernel_has_subsume_material(k, v));
  }
}

int main() {
  test_flip_clear_rearm_and_isolation();
  test_cold_activation_growth();
  test_binary_and_fixed_producers();
  test_large_publication_and_rewrite();
  test_compaction_preserves_flags_and_counter();
  test_public_instances_ignore_material_flags();
  std::puts("PASS separate subsumption material state and producer isolation");
}
