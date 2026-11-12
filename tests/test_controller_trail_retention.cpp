#include "unit_test.hpp"
#include "policies/restart_controller.hpp"
#include "strategy/online_inprocess.hpp"
#include "rules/semantic_extension/direct_csp_support_action.hpp"
#include "rules/semantic_extension/nary_csp_equality_action.hpp"
#include "rules/semantic_extension/gf3_affine_action.hpp"
#include "rules/semantic_extension/gf3_gaussian_action.hpp"
#include "rules/semantic_extension/gf3_general_row_action.hpp"
#include "rules/lucky/tseitin_cycle_action.hpp"
#include "rules/propagate/propagate.hpp"

#include <unistd.h>

using namespace tactix::strategy;
using namespace tactix::strategy::controller;

static void test_controller_boundary(Branching branching) {
  Kernel k;
  kernel_reserve_vars(k, 4);
  DefaultStrategyConfig settings;
  settings.restart.initial_interval = 1;
  settings.restart.reluctant_period = 1;
  RestartController controller(k, settings);
  ActionRequest resume;
  resume.type = ActionType::ResumeSearch;
  resume.sequence = 1;
  resume.expected = state_version(k);
  resume.search = {branching, Restart::Reluctant, Phase::FocusedRotation};
  assert(controller.resume_search(resume).resumed_search);
  for (unsigned i = 0; i < 2; ++i) {
    assert(execution::execute_action(k, execution::DecisionRequest{
               controller.next_decision()}) == ApplyResult::Ok);
    assert(propagate_apply(k) == ApplyResult::Ok);
  }
  const Lit first = k.trail[k.frames[1]], second = k.trail[k.frames[2]];
  ++k.stats.conflicts;
  controller.search_conflict(2);
  assert(controller.restart_due());
  const auto before = controller.observe();
  assert(controller.restart(1) == ApplyResult::Ok);
  const auto after = controller.observe();
  assert(k.level == 1 && k.value(first) > 0 && k.value(second) == 0);
  assert(after.searching && after.have_search_configuration);
  assert(after.search == before.search && after.sequence == before.sequence);
  assert(after.rotation_intervals == before.rotation_intervals);
  assert(after.reluctant_u == before.reluctant_u);
  assert(after.reluctant_v == before.reluctant_v);
  assert(after.reluctant_wait == before.reluctant_wait);
  assert(after.next_restart_conflicts > k.stats.conflicts);
  assert(!controller.restart_due());
  assert(k.stats.restarts == 1 && k.stats.restarts_reused == 1);
  resume.sequence = 2;
  resume.expected = state_version(k);
  resume.search.branching = branching == Branching::Vmtf
                                ? Branching::Evsids : Branching::Vmtf;
  assert(!controller.resume_search(resume).resumed_search);
  ActionRequest rule;
  rule.sequence = 2;
  rule.expected = state_version(k);
  rule.invocation.rule = "Rephase";
  assert(controller.apply_rule(rule, {}).invocation.status == InvocationStatus::Inapplicable);
  assert(controller.full_restart() == ApplyResult::Ok);
  assert(k.level == 0 && !controller.observe().searching);
  resume.expected = state_version(k);
  assert(controller.resume_search(resume).resumed_search);
  assert(controller.observe().rotation_intervals == before.rotation_intervals + 1);
  kernel_check_invariants(k);
}

int main() {
  test_controller_boundary(Branching::Vmtf);
  test_controller_boundary(Branching::Evsids);
}
