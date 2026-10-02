#pragma once

#include "strategy/controller_observation.hpp"

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace tactix::strategy::controller {

inline constexpr uint32_t FEEDBACK_VERSION = 1;

enum class FeedbackKind : uint8_t { Rule, ResumeSearch, SearchService, Support };
enum class StopReason : uint8_t {
  Completed, Fixpoint, NoChange, Inapplicable, Budget, Interrupted, Conflict,
  Sat, Unsat, Invalid, Stale, Resource, Accounting, Restart, Error, Detached,
  RootPreparation, StrategyChange
};
enum class IntervalStart : uint8_t { ResumeSearch, PartialContinuation, NativeContinuation };

inline StopReason stop_reason(InvocationStatus status) {
  switch (status) {
  case InvocationStatus::Completed: return StopReason::Completed;
  case InvocationStatus::ScopedFixpoint: return StopReason::Fixpoint;
  case InvocationStatus::NoChange: return StopReason::NoChange;
  case InvocationStatus::Inapplicable: return StopReason::Inapplicable;
  case InvocationStatus::BudgetExhausted: return StopReason::Budget;
  case InvocationStatus::Interrupted: return StopReason::Interrupted;
  case InvocationStatus::Conflict: return StopReason::Conflict;
  case InvocationStatus::Sat: return StopReason::Sat;
  case InvocationStatus::Unsat: return StopReason::Unsat;
  case InvocationStatus::InvalidRequest: return StopReason::Invalid;
  case InvocationStatus::StaleRequest: return StopReason::Stale;
  case InvocationStatus::ResourceExhausted: return StopReason::Resource;
  case InvocationStatus::AccountingError: return StopReason::Accounting;
  }
  return StopReason::Error;
}

struct FeedbackOutcome {
  InvocationResult invocation;
  uint64_t composition_overshoot = 0;
  bool resumed_search = false;
};

inline FeedbackOutcome feedback_outcome(const ActionResult &result) {
  return {result.invocation, result.composition_overshoot, result.resumed_search};
}
inline FeedbackOutcome feedback_outcome(const InvocationResult &result) {
  return {result, 0, false};
}
inline FeedbackOutcome feedback_outcome(ApplyResult result) {
  FeedbackOutcome out;
  out.invocation.rule_result = result;
  out.invocation.applications = 1;
  out.invocation.work = 1;
  out.invocation.made_rule_progress = result == ApplyResult::Ok;
  switch (result) {
  case ApplyResult::Ok: out.invocation.status = InvocationStatus::Completed; break;
  case ApplyResult::NoOp: out.invocation.status = InvocationStatus::NoChange; break;
  case ApplyResult::Conflict: out.invocation.status = InvocationStatus::Conflict; break;
  case ApplyResult::Sat: out.invocation.status = InvocationStatus::Sat; break;
  case ApplyResult::Unsat: out.invocation.status = InvocationStatus::Unsat; break;
  }
  return out;
}

// Observations retain every raw native Stats field. Their differences have no
// chosen scalar reward or cross-rule currency. Native composition work and its
// allowance/overshoot are explicit alongside those independent raw counters.
struct NativeActionDescription {
  uint32_t version = 1;
  std::string description;
  std::optional<OnlineInvocation> invocation;
  std::string effort_currency;
  WorkLimit effort_allowance = WorkLimit::unlimited();
  uint64_t work = 0, overshoot = 0;
  bool accounting_overflow = false;
};

struct ActionFeedback {
  uint32_t version = FEEDBACK_VERSION;
  uint64_t sequence = 0;
  FeedbackKind kind = FeedbackKind::Rule;
  std::string identity;
  uint64_t instance = INSTANCE_CANONICAL;
  std::optional<ActionRequest> request;
  std::optional<ActionAllowance> allowance;
  std::optional<NativeActionDescription> native_request;
  Observation before, after;
  FeedbackOutcome outcome;
  StopReason stop = StopReason::Completed;
  uint64_t duration_nanos = 0;
};

struct SearchIntervalFeedback {
  uint32_t version = FEEDBACK_VERSION;
  uint64_t sequence = 0;
  uint64_t preceding_interval = 0;
  IntervalStart start = IntervalStart::ResumeSearch;
  Observation before, after;
  StopReason stop = StopReason::Restart;
  uint64_t duration_nanos = 0;
};

// Receivers get immutable value records, never the mutable kernel. They must
// not steer execution, consume selector state or signal a solver stop. A file
// receiver exposes its own error out of band; delivery is not a fallback arm.
class FeedbackReceiver {
public:
  virtual ~FeedbackReceiver() = default;
  virtual void session_begin(const Observation &) {}
  virtual void after_action(const ActionFeedback &) = 0;
  virtual void search_interval(const SearchIntervalFeedback &) = 0;
  virtual void session_end(const Observation &, int, StopReason) {}
};

struct DescribeFeedbackOutcome {
  template <class Result> FeedbackOutcome operator()(const Result &result) const {
    return feedback_outcome(result);
  }
};

// Policy-neutral completion/accounting lifetime. The disabled path calls the
// native action directly: no snapshots, timer reads, records or receiver calls.
// Search duration excludes feedback snapshot/receiver work and includes native policy
// work between actions. No native kernel counter or scheduler is written here.
class FeedbackSession {
public:
  explicit FeedbackSession(FeedbackReceiver *receiver = nullptr)
      : receiver_(receiver) {}
  FeedbackSession(const FeedbackSession &) = delete;
  FeedbackSession &operator=(const FeedbackSession &) = delete;
  bool enabled() const { return receiver_ != nullptr && !finished_; }
  uint64_t sequence() const { return sequence_; }
  void pause_measurement() { pause_interval(); }
  void resume_measurement() { resume_interval(); }
  void begin(const Observation &observation) {
    if (receiver_) receiver_->session_begin(observation);
  }

  template <class Observe, class Apply, class Describe = DescribeFeedbackOutcome>
  auto execute(FeedbackKind kind, const char *identity, uint64_t instance,
               Observe &&observe, Apply &&apply,
               const ActionRequest *request = nullptr,
               const ActionAllowance *allowance = nullptr,
               Describe &&describe = Describe{},
               const NativeActionDescription *native_request = nullptr) {
    if (!enabled()) return apply();
    pause_interval();
    auto record = std::make_unique<ActionFeedback>();
    record->sequence = ++sequence_;
    record->kind = kind;
    record->identity = identity;
    record->instance = instance;
    if (request) record->request = *request;
    if (allowance) record->allowance = *allowance;
    record->before = observe();
    const auto start = Clock::now();
    auto result = apply();
    const auto end = Clock::now();
    record->duration_nanos = nanos(end - start);
    if (interval_) interval_->duration_nanos += record->duration_nanos;
    record->outcome = describe(result);
    record->after = observe();
    if (native_request) record->native_request = *native_request;
    if (native_request && record->outcome.invocation.rule_id != UINT32_MAX)
      record->instance = record->outcome.invocation.instance;
    record->outcome.invocation.before = record->before.state;
    record->outcome.invocation.after = record->after.state;
    record->stop = stop_reason(record->outcome.invocation.status);
    receiver_->after_action(*record);
    resume_interval();
    return result;
  }

  void begin_interval(const Observation &observation, IntervalStart start) {
    if (!receiver_) return;
    assert(!interval_);
    interval_ = std::make_unique<SearchIntervalFeedback>();
    interval_->sequence = ++interval_sequence_;
    interval_->preceding_interval =
        start != IntervalStart::ResumeSearch ? interval_sequence_ - 1 : 0;
    interval_->start = start;
    interval_->before = observation;
    resume_interval();
  }
  void end_interval(const Observation &observation, StopReason reason) {
    if (!receiver_ || !interval_) return;
    pause_interval();
    interval_->after = observation;
    interval_->stop = reason;
    receiver_->search_interval(*interval_);
    interval_.reset();
  }
  void finish(const Observation &observation, int verdict, StopReason reason) {
    if (!receiver_ || finished_) return;
    end_interval(observation, reason);
    receiver_->session_end(observation, verdict, reason);
    finished_ = true;
  }

private:
  using Clock = std::chrono::steady_clock;
  static uint64_t nanos(Clock::duration elapsed) {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
  }
  void pause_interval() {
    if (interval_ && interval_running_) {
      interval_->duration_nanos += nanos(Clock::now() - interval_started_);
      interval_running_ = false;
    }
  }
  void resume_interval() {
    if (interval_) {
      interval_started_ = Clock::now();
      interval_running_ = true;
    }
  }
  FeedbackReceiver *receiver_;
  uint64_t sequence_ = 0, interval_sequence_ = 0;
  bool finished_ = false, interval_running_ = false;
  Clock::time_point interval_started_;
  std::unique_ptr<SearchIntervalFeedback> interval_;
};

} // namespace tactix::strategy::controller
