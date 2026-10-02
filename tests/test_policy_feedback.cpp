#undef NDEBUG
#include "policies/random_controller_policy.hpp"
#include "policies/ops/rephase/rephase.hpp"
#include "rules/propagate/propagate.hpp"

#include <cassert>
#include <functional>
#include <sstream>

using namespace tactix;
using namespace tactix::strategy;
using namespace tactix::strategy::controller;

struct Capture final : FeedbackReceiver {
  unsigned begun = 0, ended = 0;
  int verdict = -1;
  StopReason stop = StopReason::Error;
  std::vector<ActionFeedback> actions;
  std::vector<SearchIntervalFeedback> intervals;
  std::function<void()> read;
  void session_begin(const Observation &) override { ++begun; }
  void after_action(const ActionFeedback &record) override {
    assert(record.version == FEEDBACK_VERSION);
    assert(record.sequence == actions.size() + 1);
    actions.push_back(record);
    if (read) read();
  }
  void search_interval(const SearchIntervalFeedback &record) override {
    assert(record.version == FEEDBACK_VERSION);
    assert(record.sequence == intervals.size() + 1);
    intervals.push_back(record);
  }
  void session_end(const Observation &, int result, StopReason reason) override {
    ++ended; verdict = result; stop = reason;
  }
};

static ActionRequest request(RestartController &controller, Kernel &k,
                             const char *identity = nullptr) {
  ActionRequest out;
  out.sequence = controller.observe().sequence + 1;
  out.expected = state_version(k);
  if (identity) {
    out.invocation.rule = identity;
    out.invocation.granularity = InvocationGranularity::CanonicalApplication;
  } else {
    out.type = ActionType::ResumeSearch;
    out.search = {Branching::Vmtf, Restart::Reluctant, Phase::Saved};
  }
  return out;
}

static std::string state(const Kernel &k) {
  const auto observation = observe_kernel(k);
  std::ostringstream out;
  out << observation.state.arena_generation << ':' << observation.state.touch_epoch
      << ':' << observation.state.strategy_epoch << ':' << k.level << ':'
      << k.propagate_head << ':' << k.inconsistent << ':' << k.conflict_valid << ':'
      << k.pending.valid << ':' << k.work_budget << ':' << k.work_budget_bounded;
  for (Lit lit : k.trail) out << ':' << lit;
  if (k.h) {
    for (auto phase : k.h->saved) out << ":p" << int(phase);
    for (auto v : k.h->scores.stack) out << ":h" << v;
    for (auto score : k.h->scores.score) out << ":s" << score;
  }
  const auto &s = k.stats;
  out << ':' << s.decisions << ':' << s.conflicts << ':' << s.propagations << ':'
      << s.ticks << ':' << s.search_ticks << ':' << s.root_assignments << ':'
      << s.learned << ':' << s.learned_glue_sum << ':' << s.restarts << ':'
      << s.restarts_reused << ':' << s.rephased;
  return out.str();
}

static void test_disabled_and_read_only() {
  Kernel k;
  unsigned reads = 0, applications = 0;
  FeedbackSession disabled;
  assert(disabled.execute(FeedbackKind::Support, "control", 0,
      [&] { ++reads; return observe_kernel(k); },
      [&] { ++applications; return ApplyResult::NoOp; }) == ApplyResult::NoOp);
  assert(reads == 0 && applications == 1);
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  const auto before = state(k);
  for (unsigned i = 0; i < 3; ++i) {
    const auto observation = observe_kernel(k);
    assert(observation.variables == 2 && observation.irredundant_clauses == 1);
  }
  assert(state(k) == before);
}

static void test_outcomes_and_intervals() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  Capture capture;
  {
    RestartController controller(k, {}, &capture);
    capture.read = [&] {
      const auto before = state(k);
      controller.observe(); controller.observe();
      assert(state(k) == before);
    };
    assert(capture.begun == 1);
    assert(controller.restart(0) == ApplyResult::NoOp);
    assert(capture.actions.back().stop == StopReason::Inapplicable);
    assert(!capture.actions.back().outcome.invocation.applications);
    auto rephase = request(controller, k, "Rephase");
    auto stale = rephase; ++stale.expected.strategy_epoch;
    assert(controller.apply_rule(stale, {}).invocation.status == InvocationStatus::StaleRequest);
    assert(capture.actions.back().stop == StopReason::Stale);
    assert(!capture.actions.back().outcome.invocation.work);
    assert(controller.apply_rule(rephase, {}).invocation.applications == 1);
    assert(capture.actions.back().before.cumulative.rephased == 0);
    assert(capture.actions.back().after.cumulative.rephased == 1);
    assert(capture.actions.back().request->sequence == 1);
    auto zero = request(controller, k, "Rephase");
    assert(controller.apply_rule(zero, {WorkUnit::Applications, 0, {}})
               .invocation.status == InvocationStatus::BudgetExhausted);
    assert(capture.actions.back().stop == StopReason::Budget);
    assert(!capture.actions.back().outcome.invocation.applications);
    assert(controller.resume_search(request(controller, k)).resumed_search);
    for (unsigned i = 0; i < 2; ++i) {
      const auto literal = controller.next_decision();
      assert(controller.search_service("Decide", lit_instance(k, literal), [&] {
        return execution::execute_action(k, execution::DecisionRequest{literal});
      }) == ApplyResult::Ok);
    }
    assert(controller.restart(1) == ApplyResult::Ok);
    assert(capture.intervals.size() == 1);
    assert(capture.intervals[0].before.cumulative.rephased == 1);
    assert(capture.intervals[0].after.cumulative.rephased == 1);
    assert(capture.intervals[0].after.level == 1 &&
           capture.intervals[0].stop == StopReason::Restart);
    assert(capture.intervals[0].after.cumulative.decisions -
           capture.intervals[0].before.cumulative.decisions == 2);
    auto invalid_resume = request(controller, k);
    assert(!controller.resume_search(invalid_resume).resumed_search);
    assert(capture.actions.back().stop == StopReason::Inapplicable);
    assert(controller.full_restart() == ApplyResult::Ok);
    assert(capture.intervals.size() == 2);
    assert(capture.intervals[1].start == IntervalStart::PartialContinuation &&
           capture.intervals[1].preceding_interval == 1);
    assert(controller.apply_rule(request(controller, k, "Rephase"), {}).invocation.applications == 1);
    assert(controller.resume_search(request(controller, k)).resumed_search);
    controller.finish_feedback(0);
    assert(capture.intervals.size() == 3 && capture.intervals[2].stop == StopReason::Interrupted);
    assert(capture.intervals[2].before.cumulative.rephased == 2);
    assert(capture.ended == 1 && capture.verdict == 0);
    capture.read = {};
  }
  assert(capture.ended == 1 && capture.stop == StopReason::Interrupted);
  kernel_check_invariants(k);
}

struct ResumeOnly final : ControllerSelection {
  unsigned calls = 0;
  bool choose(Kernel &k, const Observation &observation,
              const RandomControllerConfig &, ActionRequest &out,
              std::string &, volatile sig_atomic_t *) override {
    ++calls;
    out = {};
    out.sequence = observation.sequence + 1;
    out.expected = state_version(k);
    out.type = ActionType::ResumeSearch;
    out.search = {Branching::Evsids, Restart::AdaptiveGlue, Phase::Positive};
    return true;
  }
};

static void test_selection_and_terminal() {
  Kernel native, observed;
  for (Kernel *k : {&native, &observed}) {
    kernel_reserve_vars(*k, 3);
    kernel_add_binary_clause(*k, internalize(1), internalize(2));
  }
  Capture capture;
  ResumeOnly a, b;
  RandomControllerPolicy first(17, {}, nullptr, &a);
  RandomControllerPolicy second(17, {}, nullptr, &b, &capture);
  assert(first.solve(native) == 10 && second.solve(observed) == 10);
  assert(a.calls == b.calls && a.calls == 1);
  assert(state(native) == state(observed));
  assert(capture.ended == 1 && capture.verdict == 10);
  assert(capture.intervals.size() == 1 && capture.intervals[0].stop == StopReason::Sat);
  assert(capture.actions.front().identity == "ResumeSearch");
  assert(capture.actions.size() > 1);
  for (const auto &record : capture.actions)
    assert(record.identity == "ResumeSearch" || record.kind == FeedbackKind::SearchService);
}

struct InvalidSelection final : ControllerSelection {
  bool choose(Kernel &k, const Observation &observation,
              const RandomControllerConfig &, ActionRequest &out,
              std::string &, volatile sig_atomic_t *) override {
    out.sequence = observation.sequence + 1;
    out.expected = state_version(k);
    out.invocation.rule = "nonexistent-action";
    return true;
  }
};

static void test_stop_and_execution_error() {
  for (bool interrupted : {false, true}) {
    Kernel k;
    kernel_reserve_vars(k, 3);
    Capture capture;
    InvalidSelection selector;
    RandomControllerPolicy policy(1, {}, nullptr, &selector, &capture);
    volatile sig_atomic_t stop = interrupted;
    assert(policy.solve(k, &stop) == 0);
    assert(capture.ended == 1 && capture.verdict == 0);
    assert(capture.stop == (interrupted ? StopReason::Interrupted : StopReason::Error));
    assert(capture.actions.size() == (interrupted ? 0 : 1));
    if (!interrupted) assert(capture.actions[0].stop == StopReason::Invalid);
    assert(capture.intervals.empty());
    kernel_check_invariants(k);
  }
}

static void test_native_effort_and_interruption() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(1), internalize(3));
  kernel_add_binary_clause(k, internalize(-1), internalize(4));
  kernel_add_binary_clause(k, internalize(-1), internalize(5));
  Capture capture;
  RestartController controller(k, {}, &capture);
  auto action = request(controller, k, "BVE");
  action.invocation.granularity = InvocationGranularity::OneInstance;
  action.invocation.instances = {lit_instance(k, internalize(1))};
  const ActionAllowance allowance{WorkUnit::BveTicks, 1, {}};
  const auto result = controller.apply_rule(action, allowance);
  if (result.invocation.applications != 1 || result.invocation.work <= 1)
    std::fprintf(stderr, "BVE status=%u applications=%llu work=%llu\n",
        unsigned(result.invocation.status), (unsigned long long)result.invocation.applications,
        (unsigned long long)result.invocation.work);
  assert(result.invocation.applications == 1 && result.invocation.work > 1);
  const auto &record = capture.actions.back();
  assert(record.instance == action.invocation.instances[0]);
  assert(record.allowance->unit == WorkUnit::BveTicks && record.allowance->amount == 1);
  assert(record.outcome.invocation.work == result.invocation.work);
  assert(record.outcome.composition_overshoot == result.invocation.work - 1);
  assert(record.after.cumulative.bve_eliminated > record.before.cumulative.bve_eliminated);
  const auto before = state(k);
  volatile sig_atomic_t stop = 1;
  const auto interrupted = controller.apply_rule(request(controller, k, "Rephase"), {}, &stop);
  assert(interrupted.invocation.status == InvocationStatus::Interrupted);
  assert(capture.actions.back().stop == StopReason::Interrupted);
  assert(!capture.actions.back().outcome.invocation.applications);
  assert(state(k) == before);
  controller.finish_feedback(0);
}

int main() {
  test_disabled_and_read_only();
  test_outcomes_and_intervals();
  test_selection_and_terminal();
  test_stop_and_execution_error();
  test_native_effort_and_interruption();
}
