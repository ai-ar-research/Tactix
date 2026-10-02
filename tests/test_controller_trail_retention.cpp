#undef NDEBUG
#include "policies/random_controller_policy.hpp"
#include "rules/propagate/propagate.hpp"

#include <cassert>
#include <cstdio>
#include <string>
#include <unistd.h>

using namespace tactix;
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

static Kernel pigeonhole() {
  Kernel k;
  constexpr unsigned pigeons = 5, holes = 4;
  kernel_reserve_vars(k, pigeons * holes);
  auto cell = [](unsigned pigeon, unsigned hole) {
    return make_lit(pigeon * holes + hole, false);
  };
  for (unsigned p = 0; p < pigeons; ++p) {
    Lit clause[holes];
    for (unsigned h = 0; h < holes; ++h)
      clause[h] = cell(p, h);
    kernel_add_large_clause(k, clause, holes, false, 0);
    for (unsigned q = p + 1; q < pigeons; ++q)
      for (unsigned h = 0; h < holes; ++h)
        kernel_add_binary_clause(k, negate(cell(p, h)), negate(cell(q, h)));
  }
  return k;
}

int main() {
  test_controller_boundary(Branching::Vmtf);
  test_controller_boundary(Branching::Evsids);
  uint64_t retained = 0;
  uint64_t replay_seed = 0;
  for (const bool reuse : {false, true}) {
    for (uint64_t seed = 1; seed <= 16; ++seed) {
      Kernel k = pigeonhole();
      RandomControllerConfig config;
      config.parameters.rules = {"Propagate"};
      config.search.restart.initial_interval = 1;
      config.search.restart.reluctant_period = 1;
      config.reuse_search_trail = reuse;
      RandomControllerPolicy policy(seed, config);
      assert(policy.solve(k) == 20);
      assert(k.inconsistent && !k.pending.valid);
      if (reuse) {
        retained += policy.coverage().partial_restarts;
        if (!replay_seed && policy.coverage().partial_restarts)
          replay_seed = seed;
      } else
        assert(policy.coverage().partial_restarts == 0);
      assert(k.stats.restarts_reused == policy.coverage().partial_restarts);
      kernel_check_invariants(k);
    }
  }
  assert(retained && replay_seed);
  RandomControllerConfig config;
  config.parameters.rules = {"Propagate"};
  config.search.restart.initial_interval = 1;
  config.search.restart.reluctant_period = 1;
  const std::string path = "controller-retention-" + std::to_string(getpid()) + ".trace";
  uint64_t recorded_partial = 0, recorded_full = 0;
  {
    ControllerTrace trace;
    assert(trace.write(path.c_str(), replay_seed, controller_configuration(config)));
    Kernel k = pigeonhole();
    RandomControllerPolicy policy(replay_seed, config, &trace);
    assert(policy.solve(k) == 20);
    recorded_partial = policy.coverage().partial_restarts;
    recorded_full = policy.coverage().full_restarts;
    assert(recorded_partial && trace.error().empty());
  }
  {
    ControllerTrace replay;
    assert(replay.replay(path.c_str(), controller_configuration(config)));
    Kernel k = pigeonhole();
    RandomControllerPolicy policy(999, config, &replay);
    assert(policy.solve(k) == 20);
    assert(policy.coverage().partial_restarts == recorded_partial);
    assert(policy.coverage().full_restarts == recorded_full);
    assert(replay.error().empty());
  }
  {
    ControllerTrace wrong_profile;
    config.reuse_search_trail = false;
    assert(!wrong_profile.replay(path.c_str(), controller_configuration(config)));
    assert(wrong_profile.error().find("configuration differs") != std::string::npos);
  }
  assert(std::remove(path.c_str()) == 0);
}
