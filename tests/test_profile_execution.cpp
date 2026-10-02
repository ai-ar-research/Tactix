#undef NDEBUG
#include "policies/default_policy.hpp"
#include "strategy/parameter_execution.hpp"

#include <cassert>

using namespace tactix;
using namespace tactix::strategy;
using namespace tactix::execution;

static DefaultStrategyConfig profile() {
  DefaultStrategyConfig cfg;
  cfg.inprocess.structured_root_stages.clear();
  cfg.inprocess.structured_root_large_domain_stages.clear();
  cfg.inprocess.bce_parameters.occurrence_limit = 17;
  cfg.inprocess.factor_parameters = {4, 3, 5, true, false};
  cfg.inprocess.vivify_parameters = {3, false, true, 19};
  cfg.inprocess.bve_occurrence_limit = 23;
  cfg.inprocess.bve_clause_size_limit = 29;
  cfg.inprocess.bve_backward = true;
  cfg.inprocess.substitute_parameters.demote_substituted = true;
  cfg.analysis.minimize_depth_limit = 31;
  cfg.analysis.conflict_reuse = false;
  cfg.analysis.bump_extension_reasons = false;
  cfg.analysis.chronological_levels = 37;
  cfg.reduction.victim_numerator = 1;
  cfg.reduction.victim_denominator = 2;
  cfg.reduction.protect_tiers = false;
  cfg.reduction.canonical_compact_collection = true;
  cfg.reduction.bookkeeping = false;
  cfg.rephase.walk_even_break_base_milli = 2700;
  return cfg;
}

static void assert_inprocess_profile(const Kernel &k) {
  assert(k.bce_occ_limit == 17);
  assert(k.factor_size == 4 && k.factor_bound == 3 && k.factor_cand_rounds == 5);
  assert(k.factor_composite && !k.factor_demote_fresh && k.subst_demote);
  assert(k.vivify_scope == 3 && !k.vivify_reuse && k.vivify_flush);
  assert(k.vivify_irredundant_sort_ratio == 19);
  assert(k.bve_occurrence_limit == 23 && k.bve_clause_size_limit == 29);
  assert(k.bve_resolvent_count_limit == UINT32_MAX && k.bve_backward);
  assert(k.bve_gate_mask == BVE_GATE_AND_OR && k.bve_bound == 41);
  assert(k.bve_fastel_traversal);
}

static void test_build_and_phase_installation() {
  const auto cfg = profile();
  for (auto group : {SlotGroup::Probe, SlotGroup::Eliminate}) {
    Kernel k;
    k.bve_gate_mask = BVE_GATE_AND_OR;
    k.bve_bound = 41;
    k.bve_fastel_traversal = true;
    k.work_budget = 43; k.walk_random_seed = 47;
    DefaultPolicy policy(cfg);
    policy.inprocess().build(k);
    assert_inprocess_profile(k);
    // An explicit caller override restores to the installed policy profile.
    {
      ActionParameters p = FactorParameters{5, 7, 2, false, true};
      ParameterExecutionScope<FactorParameters> temporary(k, p);
      assert(k.factor_bound == 7 && k.factor_demote_fresh);
    }
    assert_inprocess_profile(k);
    // A phase reposes its configured family values, while unowned BVE state
    // and Walk's independent seed/work wires remain inherited.
    k.bce_occ_limit = 1;
    k.factor_bound = 2;
    k.vivify_irredundant_sort_ratio = 3;
    for (auto &e : policy.inprocess().entries) e.enabled = false;
    assert(policy.inprocess().run_phase(k, group, nullptr) == 0);
    assert_inprocess_profile(k);
    assert(k.work_budget == 43 && k.walk_random_seed == 47);
    assert(!k.dense && !k.inprocessing && !k.phase_saving_suppressed);
    kernel_check_invariants(k);
  }
}

static void test_partial_profile_inheritance() {
  Kernel k;
  k.bump_reason_limit = 53; k.bump_reason_depth = 59;
  k.jump_reasons_bigbig = true;
  auto analysis = AnalyzeParameters::from_kernel(k);
  analysis.minimize_depth_limit = 61;
  analysis.bump_extension_reasons = false;
  execute_action(k, ParameterInstallationRequest{analysis});
  assert(k.analyze_parameters.minimize_depth_limit == 61 && k.no_ext_reason_bump);
  assert(k.bump_reason_limit == 53 && k.bump_reason_depth == 59 && k.jump_reasons_bigbig);
  {
    auto temporary = analysis;
    temporary.bump_reason_limit = 67;
    ScopedAnalyzeParameters scope(k, temporary);
    assert(k.bump_reason_limit == 67);
  }
  assert(k.bump_reason_limit == 53 && k.no_ext_reason_bump);

  k.tier1 = 7; k.tier2 = 11;
  auto reduce = ReduceParameters::from_kernel(k);
  reduce.victim_numerator = 1; reduce.victim_denominator = 2;
  execute_action(k, ParameterInstallationRequest{reduce});
  assert(k.tier1 == 7 && k.tier2 == 11);
  assert(k.reduce_victim_numerator == 1 && k.reduce_victim_denominator == 2);

  k.work_budget = 71; k.work_budget_offset = 73;
  k.work_budget_bounded = false; k.midround_budget = true;
  k.walk_random_seed = 79;
  const auto curve = WalkParameters::reference_score_curve();
  execute_action(k, WalkGuidanceInstallationRequest{2800, curve});
  assert(k.walk_even_break_base_milli == 2800 && k.walk_score_curve == curve);
  {
    WalkParameters temporary;
    temporary.step_budget = 0;
    ScopedWalkParameters scope(k, temporary);
    assert(k.work_budget == 0 && k.work_budget_bounded);
  }
  assert(k.work_budget == 71 && k.work_budget_offset == 73 && k.walk_random_seed == 79);
  assert(!k.work_budget_bounded && k.midround_budget);
  assert(k.walk_even_break_base_milli == 2800 && k.walk_score_curve == curve);
}

static void test_solve_entry_profile() {
  const auto cfg = profile();
  Kernel k;
  // No formula work is necessary to validate the solve-entry lifetime. The
  // complete-run signature fixtures independently check nonempty certificates.
  k.bump_reason_limit = 83; k.bump_reason_depth = 89;
  k.jump_reasons_bigbig = true;
  k.tier1 = 7; k.tier2 = 11;
  DefaultPolicy policy(cfg);
  assert(policy.solve(k, nullptr) == 10);
  assert(k.analyze_parameters.minimize_depth_limit == 31);
  assert(k.conflict_reuse_chrono_levels == 37 && !k.conflict_reuse && k.no_ext_reason_bump);
  assert(k.reduce_victim_numerator == 1 && k.reduce_victim_denominator == 2);
  assert(!k.reduce_protect_tiers && k.reduce_compact_collection && !k.reduce_bookkeeping);
  assert(k.walk_even_break_base_milli == 2700);
  kernel_check_invariants(k);
}

int main() {
  test_build_and_phase_installation();
  test_partial_profile_inheritance();
  test_solve_entry_profile();
}
