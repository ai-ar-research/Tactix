#undef NDEBUG
#include "policies/random_controller_policy.hpp"
#include "rules/decide/decide.hpp"

#include <cassert>
#include <fstream>
#include <iterator>
#include <unistd.h>

using namespace tactix;
using namespace tactix::strategy;
using namespace tactix::strategy::controller;

static Kernel fixture(bool unsat) {
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  if (unsat) {
    kernel_add_binary_clause(k, internalize(1), internalize(-2));
    kernel_add_binary_clause(k, internalize(-1), internalize(-2));
  }
  return k;
}

int main() {
  Kernel a = fixture(false), b = fixture(false);
  RestartController ca(a), cb(b);
  RandomControllerSelector sa(17), sb(17);
  RandomControllerConfig config;
  bool saw_rule = false, saw_resume = false;
  for (unsigned i = 0; i < 100; ++i) {
    // Extra observations never alter subsequent action requests.
    ca.observe();
    ca.observe();
    ActionRequest ra, rb;
    std::string error;
    assert(sa.choose(a, ca.observe(), config, ra, error));
    assert(sb.choose(b, cb.observe(), config, rb, error));
    assert(ra.type == rb.type && ra.sequence == rb.sequence);
    assert(ra.search == rb.search);
    if (ra.type == ActionType::ResumeSearch) {
      saw_resume = true;
      assert(valid(ra.search));
    } else {
      saw_rule = true;
      assert(root_exclusion(tactix_rules()[tactix_rule_id(
          ra.invocation.rule.c_str())]).empty());
      OnlineStrategyProgram pa, pb;
      pa.sequence.push_back(ra.invocation);
      pb.sequence.push_back(rb.invocation);
      assert(serialize_online_strategy_program(pa) ==
             serialize_online_strategy_program(pb));
      const auto allowance = config.allowance(ra.invocation.rule);
      assert(ra.invocation.work_unit == allowance.unit);
      assert(ra.invocation.budget.amount() == allowance.amount);
      assert(ra.invocation.rule_work_budget.amount ==
             config.default_allowance.native.amount);
      // The controller supplies the executor's internal step identity and
      // once-only repetition; generated semantic payloads must then preflight.
      pa.sequence[0].id = 1;
      pa.sequence[0].repeat = {RepeatMode::Once, 1};
      std::string validation_error;
      assert(validate_online_strategy_program(pa, &validation_error));
    }
  }
  assert(saw_rule && saw_resume);
  {
    Kernel k = fixture(false);
    RestartController controller(k);
    RandomControllerConfig walk_only;
    walk_only.parameters.rules = {"Walk"};
    RandomControllerSelector chooser(17);
    unsigned walks = 0;
    for (unsigned i = 0; i < 100; ++i) {
      ActionRequest request;
      std::string error;
      assert(chooser.choose(k, controller.observe(), walk_only, request, error));
      if (request.type == ActionType::ResumeSearch)
        continue;
      ++walks;
      assert(request.invocation.rule == "Walk");
      assert(request.invocation.granularity == InvocationGranularity::CanonicalApplication);
      assert(request.invocation.instances.empty());
    }
    assert(walks);
  }
  {
    Kernel k = fixture(false);
    RestartController controller(k);
    RandomControllerConfig offered;
    offered.parameters.rules = {"Condition"};
    const auto unavailable = controller_action_availability(k, offered);
    bool accounted = false;
    for (const auto &entry : unavailable)
      if (entry.rule == "Condition") {
        assert(!entry.eligible && entry.reason == "structured-witness-not-offered");
        accounted = true;
      }
    assert(accounted);
    OnlineInvocation condition;
    condition.rule = "Condition";
    condition.granularity = InvocationGranularity::OneInstance;
    condition.condition_instances.push_back(
        {{internalize(1), internalize(2)}, {internalize(2)}});
    offered.offered_actions["Condition"] = {condition};
    OnlineStrategyProgram collection;
    condition.id = 1;
    condition.budget = WorkLimit::bounded(1);
    condition.rule_work_budget = offered.default_allowance.native;
    collection.sequence = {condition};
    assert(offered.configure_actions(collection));
    const auto accepted = controller_configuration(offered);
    auto inconsistent = collection;
    inconsistent.sequence.push_back(condition);
    inconsistent.sequence.back().id = 2;
    inconsistent.sequence.back().budget = WorkLimit::bounded(2);
    assert(!offered.configure_actions(inconsistent));
    assert(controller_configuration(offered) == accepted);
    RandomControllerSelector chooser(1);
    bool executed = false;
    for (unsigned i = 0; i < 20 && !executed; ++i) {
      ActionRequest request;
      std::string error;
      assert(chooser.choose(k, controller.observe(), offered, request, error));
      if (request.type == ActionType::Rule) {
        const auto result = controller.apply_rule(request, offered.allowance("Condition"));
        assert(result.invocation.applications == 1);
        assert(result.invocation.made_rule_progress);
        assert(!kernel_binary_live(k, internalize(1), internalize(2)));
        executed = true;
      }
    }
    assert(executed);
  }
  for (uint64_t seed : {1, 7, 17}) {
    Kernel sat = fixture(false), unsat = fixture(true);
    RandomControllerPolicy ps(seed), pu(seed);
    assert(ps.solve(sat) == 10);
    kernel_extend_model(sat);
    // Both original clauses require x2; check the reconstructed assignment.
    assert(sat.value(internalize(2)) > 0);
    assert(pu.solve(unsat) == 20);
    assert(unsat.inconsistent);
    assert(!sat.pending.valid && !unsat.pending.valid);
  }
  Kernel stopped = fixture(true);
  volatile sig_atomic_t stop = 1;
  RandomControllerPolicy policy(1);
  assert(policy.solve(stopped, &stop) == 0);
  assert(!stopped.h && !stopped.saved_phases && !stopped.stop);
  for (const bool conflict_boundary : {false, true}) {
    Kernel k = fixture(false);
    if (conflict_boundary)
      kernel_add_binary_clause(k, internalize(-1), internalize(-2));
    ScopedHeuristics embedding(k);
    kernel_reserve_vars(k, 3);
    kernel_assign_unit(k, internalize(3));
    apply_rule(tactix_rules()[tactix_rule_id("Propagate")], k);
    assert(decide_apply(k, internalize(1), false) == ApplyResult::Ok);
    if (conflict_boundary) {
      apply_rule(tactix_rules()[tactix_rule_id("Propagate")], k);
      assert(k.conflict_valid);
    }
    const auto level = k.level;
    const auto binaries = k.watches.binary_clauses();
    const auto restarts = k.stats.restarts;
    RandomControllerConfig config;
    config.parameters.rules = {"Rephase"};
    RandomControllerPolicy interrupted(17, config);
    volatile sig_atomic_t stop = 1;
    assert(interrupted.solve(k, &stop) == 0);
    assert(k.level == level && k.stats.restarts == restarts);
    stop = 0;
    assert(interrupted.solve(k, &stop) == 10);
    assert(k.stats.restarts > restarts);
    assert(interrupted.coverage().full_restarts == 1);
    assert(interrupted.coverage().configuration_changes == 0);
    assert(k.watches.binary_clauses() >= binaries);
    assert(k.value(internalize(3)) > 0 &&
           k.assigned[var_of(internalize(3))].level == 0);
    kernel_extend_model(k);
    assert(k.value(internalize(2)) > 0);
    if (conflict_boundary)
      assert(k.value(internalize(1)) < 0);
  }
  {
    char path[] = "/tmp/tactix-controller-transcript-XXXXXX";
    const int fd = mkstemp(path);
    assert(fd >= 0);
    close(fd);
    RandomControllerConfig replay_config;
    replay_config.parameters.rules = {"Rephase"};
    const auto signature = controller_configuration(replay_config);
    {
      ControllerTrace record;
      assert(record.write(path, 17, signature));
      Kernel original = fixture(true);
      RandomControllerPolicy writer(17, replay_config, &record);
      assert(writer.solve(original) == 20);
    }
    {
      ControllerTrace replay;
      assert(replay.replay(path, signature));
      Kernel repeated = fixture(true);
      // The transcript supplies its seed; the caller's different seed cannot
      // silently select a different run.
      RandomControllerPolicy reader(99, replay_config, &replay);
      assert(reader.solve(repeated) == 20);
      assert(repeated.inconsistent);
    }
    {
      ControllerTrace mismatch;
      auto changed = replay_config;
      ++changed.default_allowance.native.amount;
      assert(!mismatch.replay(path, controller_configuration(changed)));
    }
    std::ifstream input(path);
    std::string transcript((std::istreambuf_iterator<char>(input)), {});
    input.close();
    assert(transcript.find("service Decide") != std::string::npos);
    assert(transcript.find("service ConflictAnalysis") != std::string::npos);
    const auto terminal = transcript.rfind("verdict 20");
    assert(terminal != std::string::npos);
    transcript.replace(terminal, 10, "verdict 10");
    {
      std::ofstream corrupt(path);
      corrupt << transcript;
    }
    {
      ControllerTrace replay;
      assert(replay.replay(path, signature));
      Kernel repeated = fixture(true);
      RandomControllerPolicy reader(17, replay_config, &replay);
      assert(reader.solve(repeated) == -1);
      assert(!replay.error().empty());
    }
    unlink(path);
  }
}
