#undef NDEBUG
#include "rules/propagate/propagate.hpp"
#include "strategy/vivify_execution.hpp"

#include <algorithm>
#include <cassert>
#include <vector>

using namespace tactix;
using namespace tactix::execution;
using namespace tactix::strategy;

static void test_nested_work_settings() {
  for (bool bounded : {false, true})
    for (bool midround : {false, true}) {
      Kernel k;
      k.work_budget = 77; k.work_budget_offset = 13;
      k.work_budget_bounded = bounded; k.midround_budget = midround;
      {
        ScopedRuleWorkBudget outer(k, {RuleWorkBudgetMode::Bounded, 0});
        assert(k.work_budget == 0 && k.work_budget_offset == 0);
        assert(k.work_budget_bounded && k.midround_budget);
        {
          ScopedRuleWorkBudget inner(k, {RuleWorkBudgetMode::Unlimited, 0});
          assert(!k.work_budget_bounded && !k.midround_budget);
          k.work_budget = 91; k.work_budget_offset = 5;
        }
        assert(k.work_budget == 0 && k.work_budget_offset == 0);
        assert(k.work_budget_bounded && k.midround_budget);
      }
      assert(k.work_budget == 77 && k.work_budget_offset == 13);
      assert(k.work_budget_bounded == bounded && k.midround_budget == midround);
      {
        ScopedRuleWorkBudget inherited(k, {});
        k.work_budget = 92; // Inherit does not acquire these wires.
      }
      assert(k.work_budget == 92);
      {
        WorkSettingsScope native(k, {2, std::nullopt, std::nullopt, false},
                                 WorkSettingsExit::ClearBudget);
        assert(k.work_budget == 2 && k.work_budget_offset == 13);
      }
      assert(k.work_budget == 0 && k.work_budget_offset == 13);
      assert(k.work_budget_bounded == bounded && k.midround_budget == midround);
    }
}

static void test_material_and_tick_lifetimes() {
  Kernel k;
  k.touch_filter = true; k.touch_redundant = false;
  {
    ScopedRuleMaterialSettings outer(k, {SettingOverride::Disable,
                                         SettingOverride::Enable});
    assert(!k.touch_filter && k.touch_redundant);
    {
      ScopedRuleMaterialSettings inherited(k, {});
      k.touch_filter = true; k.touch_redundant = false;
    }
    assert(!k.touch_filter && k.touch_redundant);
  }
  assert(k.touch_filter && !k.touch_redundant);
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(1), internalize(-2));
  k.tick_sink = &k.stats.probe_ticks;
  {
    TickSinkScope sink(k, &Stats::vivify_ticks, true);
    kernel_assign_unit(k, internalize(-1));
    assert(propagate_apply(k) == ApplyResult::Ok);
    assert(k.stats.vivify_ticks > 0 && k.stats.probe_ticks == 0);
  }
  assert(k.tick_sink == &k.stats.probe_ticks);
  { TickSinkScope native(k, nullptr); }
  assert(k.tick_sink == nullptr);
  kernel_check_invariants(k);
}

static void fixture(Kernel &k) {
  kernel_reserve_vars(k, 4);
  const Lit a[] = {internalize(1), internalize(2), internalize(3)};
  const Lit b[] = {internalize(1), internalize(2), internalize(4)};
  kernel_add_large_clause(k, a, 3, false, 0);
  kernel_add_large_clause(k, b, 3, true, 2);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  k.work_budget = 77; k.work_budget_offset = 13;
  k.midround_budget = true; k.vivify_scope = 5;
}

static void test_band(uint32_t scope, uint64_t budget, uint64_t offset) {
  Kernel old, shared;
  fixture(old); fixture(shared);
  const RuleVTable &rule = tactix_rules()[tactix_rule_id("Vivify")];
  old.work_budget_bounded = true;
  old.vivify_scope = scope;
  old.work_budget = budget; old.work_budget_offset = offset;
  old.tick_sink = &old.stats.vivify_ticks;
  const auto expected = apply_rule(rule, old);
  old.tick_sink = nullptr;
  {
    VivifyDoseScope dose(shared);
    const auto actual = execute_action(
        shared, rule, VivifyBandRequest{scope, budget, offset,
                                        &Stats::vivify_ticks, nullptr});
    assert(actual == expected);
    assert(shared.vivify_scope == scope && shared.work_budget == budget);
    assert(shared.work_budget_offset == offset && shared.work_budget_bounded);
    assert(shared.midround_budget && shared.tick_sink == nullptr);
  }
  assert(shared.vivify_scope == 0 && shared.work_budget == 0);
  assert(shared.work_budget_offset == 0 && !shared.work_budget_bounded);
  assert(shared.midround_budget);
  assert(shared.values == old.values);
  assert(shared.trail.size() == old.trail.size());
  assert(std::equal(shared.trail.begin(), shared.trail.end(), old.trail.begin()));
  assert(shared.arena.size() == old.arena.size());
  for (Ref r = 0; r < old.arena.size(); ++r)
    assert(shared.arena[r] == old.arena[r]);
  assert(shared.stats.vivify_ticks == old.stats.vivify_ticks);
  assert(shared.stats.vivify_checked == old.stats.vivify_checked);
  assert(shared.stats.vivify_subsumed == old.stats.vivify_subsumed);
  assert(shared.stats.ticks == old.stats.ticks);
  kernel_check_invariants(shared);
}

int main() {
  test_nested_work_settings();
  test_material_and_tick_lifetimes();
  for (uint32_t scope = 0; scope <= 5; ++scope)
    for (uint64_t budget : {0ull, 1000ull})
      for (uint64_t offset : {0ull, 100ull})
        test_band(scope, budget, offset);
}
