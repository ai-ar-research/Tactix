#undef NDEBUG
#include "policies/default_policy.hpp"
#include "strategy/feedback_trace.hpp"
#include "strategy/native_feedback.hpp"
#include "strategy/action_execution.hpp"

#include <cassert>
#include <functional>
#include <unistd.h>

using namespace tactix;
using namespace tactix::strategy;
using namespace tactix::strategy::controller;

struct Capture final : FeedbackReceiver {
  unsigned begun = 0, ended = 0;
  std::vector<ActionFeedback> actions;
  std::vector<SearchIntervalFeedback> intervals;
  std::function<void()> check;
  void session_begin(const Observation &) override { ++begun; }
  void after_action(const ActionFeedback &value) override {
    assert(value.sequence == actions.size() + 1);
    assert(value.native_request && !value.native_request->effort_currency.empty());
    actions.push_back(value);
    if (check) check();
  }
  void search_interval(const SearchIntervalFeedback &value) override {
    assert(value.sequence == intervals.size() + 1);
    intervals.push_back(value);
    if (check) check();
  }
  void session_end(const Observation &, int, StopReason) override { ++ended; }
};

static Kernel pigeonhole() {
  Kernel k;
  constexpr unsigned pigeons = 5, holes = 4;
  kernel_reserve_vars(k, pigeons * holes);
  auto cell = [](unsigned p, unsigned h) { return make_lit(p * holes + h, false); };
  for (unsigned p = 0; p < pigeons; ++p) {
    std::vector<Lit> clause;
    for (unsigned h = 0; h < holes; ++h) clause.push_back(cell(p, h));
    kernel_add_large_clause(k, clause.data(), holes, false, 0, ++k.next_clause_id);
    for (unsigned q = p + 1; q < pigeons; ++q)
      for (unsigned h = 0; h < holes; ++h)
        kernel_add_binary_clause(k, negate(cell(p, h)), negate(cell(q, h)), ++k.next_clause_id);
  }
  return k;
}

static DefaultStrategyConfig search_profile() {
  DefaultStrategyConfig config;
  config.inprocess.program.clear();
  config.inprocess.append_unlisted_rules = false;
  config.inprocess.structured_root_stages.clear();
  config.inprocess.structured_root_bounded_backtracking_stages.clear();
  config.inprocess.structured_root_large_domain_stages.clear();
  config.inprocess.initial_fast_elimination = false;
  config.rephase.late_lucky = false;
  config.rephase.structural_walk_seed = false;
  config.rephase.stable_only = false;
  config.rephase.initial_interval = config.rephase.interval = 1;
  config.rephase.rotation = {DefaultRephaseSlot::Walk};
  config.rephase.walk_permille = 1;
  config.mode.initial_conflicts = config.mode.interval = 1;
  config.restart.reluctant_period = 2;
  config.reduction.initial_interval = 5;
  return config;
}

static void test_completion_accounting() {
  Kernel k;
  Capture capture;
  unsigned reads = 0, builds = 0;
  auto observe = [](const Kernel &kernel, const void *owner) {
    ++*static_cast<unsigned *>(const_cast<void *>(owner));
    return observe_kernel(kernel);
  };
  execution::PolicyCompletion disabled(k, nullptr, observe, &reads);
  assert(disabled.execute(FeedbackKind::Support, "native", 0,
      [&] { ++builds; return execution::NativeFeedbackSpec{}; },
      [] { return ApplyResult::NoOp; }) == ApplyResult::NoOp);
  assert(!reads && !builds);
  execution::PolicyCompletion enabled(k, &capture, observe, &reads);
  enabled.begin();
  auto build = [&] {
    execution::NativeFeedbackSpec spec;
    spec.meter = [](const Kernel &kernel) { return kernel.stats.ticks; };
    spec.description.description = "native atomic overshoot";
    spec.description.effort_currency = "ticks";
    spec.description.effort_allowance = WorkLimit::bounded(2);
    return spec;
  };
  enabled.execute(FeedbackKind::Support, "native", 0, build,
      [&] { k.stats.ticks += 7; return ApplyResult::Ok; });
  const auto &request = *capture.actions.back().native_request;
  assert(request.work == 7 && request.overshoot == 5 && !request.accounting_overflow);
  enabled.execute(FeedbackKind::Support, "overflow", 0, build,
      [&] { k.stats.ticks = 0; return ApplyResult::NoOp; });
  assert(capture.actions.back().native_request->accounting_overflow);
  assert(capture.actions.back().native_request->work == 0);
  enabled.resume(IntervalStart::ResumeSearch, "initial");
  enabled.end_interval(StopReason::Restart);
  const auto count = capture.actions.size();
  enabled.continue_search(IntervalStart::NativeContinuation);
  assert(capture.actions.size() == count); // Continuation does not select/install.
  enabled.finish(0, StopReason::Interrupted);
  assert(capture.begun == 1 && capture.ended == 1 && capture.intervals.size() == 2);
  assert(capture.intervals.back().preceding_interval == 1);
}

static void test_default_observation_and_replay(const char *path) {
  const auto config = search_profile();
  Kernel native = pigeonhole(), observed = pigeonhole();
  ScopedHeuristics first_h(native), second_h(observed);
  DefaultPolicy first(config), second(config);
  Capture capture;
  second.set_feedback(&capture);
  capture.check = [&] {
    assert(!observed.pending.valid && !observed.warming);
    const auto before = serialize_observation(second.observe(observed));
    assert(before == serialize_observation(second.observe(observed)));
  };
  assert(first.solve(native) == 20 && second.solve(observed) == 20);
  assert(serialize_observation(first.observe(native)) == serialize_observation(second.observe(observed)));
  assert(native.trail.size() == observed.trail.size());
  assert(std::equal(native.trail.begin(), native.trail.end(), observed.trail.begin()));
  assert(native.h->saved == observed.h->saved && native.h->target == observed.h->target);
  assert(native.h->scores.score == observed.h->scores.score);
  assert(capture.begun == 1 && capture.ended == 1);
  bool analysis = false, warmup = false, mode = false, work = false, root = false;
  for (const auto &record : capture.actions) {
    analysis |= record.identity == "ConflictAnalysis";
    warmup |= record.identity == "Warmup";
    mode |= record.identity == "SwitchMode";
    if (record.identity == "Propagate")
      work |= record.native_request->work > 0 && record.native_request->effort_currency == "propagations";
  }
  for (const auto &interval : capture.intervals) {
    if (interval.start != IntervalStart::ResumeSearch)
      assert(interval.preceding_interval + 1 == interval.sequence);
    root |= interval.stop == StopReason::RootPreparation;
  }
  assert(analysis && warmup && mode && work && root);
  for (const bool replay : {false, true}) {
    Kernel k = pigeonhole();
    DefaultPolicy policy(config);
    FeedbackTrace trace;
    assert(replay ? trace.replay(path, "native-default-test") : trace.write(path, "native-default-test"));
    policy.set_feedback(&trace);
    assert(policy.solve(k) == 20);
    assert(trace.terminal() && trace.error().empty());
    assert(!k.h); // Owned guidance is detached only after completion delivery.
  }
}

static void test_early_terminal() {
  Kernel k;
  k.inconsistent = true;
  DefaultPolicy policy;
  Capture capture;
  policy.set_feedback(&capture);
  assert(policy.solve(k) == 20);
  assert(capture.begun == 1 && capture.ended == 1 && capture.actions.empty());
}

static void test_interrupted_entry_and_fresh_session() {
  Kernel k = pigeonhole();
  ScopedHeuristics h(k);
  assert(execution::execute_action(k, execution::DecisionRequest{make_lit(0, false)}) == ApplyResult::Ok);
  DefaultPolicy policy(search_profile());
  Capture interrupted;
  policy.set_feedback(&interrupted);
  volatile sig_atomic_t stop = 1;
  assert(policy.solve(k, &stop) == 0);
  assert(interrupted.begun == 1 && interrupted.ended == 1);
  assert(interrupted.actions.front().identity == "Restart");
  assert(interrupted.actions.front().native_request->description.find("purpose=preparation") != std::string::npos);
  assert(!k.level && !k.pending.valid);
  Capture resumed;
  policy.set_feedback(&resumed);
  stop = 0;
  assert(policy.solve(k, &stop) == 20);
  assert(resumed.begun == 1 && resumed.ended == 1);
  assert(resumed.actions.front().sequence == 1);
}

int main() {
  char temporary[] = "/tmp/tactix-native-feedback-XXXXXX";
  const int fd = mkstemp(temporary);
  assert(fd >= 0 && close(fd) == 0);
  test_completion_accounting();
  test_default_observation_and_replay(temporary);
  test_early_terminal();
  test_interrupted_entry_and_fresh_session();
  assert(unlink(temporary) == 0);
}
