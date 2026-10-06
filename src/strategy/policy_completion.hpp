#pragma once

#include "strategy/policy_feedback.hpp"

#include <type_traits>
#include <vector>

namespace tactix::execution {

using strategy::controller::FeedbackKind;
using strategy::controller::FeedbackOutcome;
using strategy::controller::IntervalStart;
using strategy::controller::NativeActionDescription;
using strategy::controller::Observation;
using strategy::controller::StopReason;

struct NativeFeedbackSpec {
  NativeActionDescription description;
  uint64_t (*meter)(const Kernel &) = nullptr;
};

// One policy's explicit completion lifetime. There is no Kernel hook, global
// observer or change to native accounting. Owners provide a pure snapshot and
// lazy request descriptions, keeping their own scheduling and execution order.
class PolicyCompletion {
  class PublicationReceiver final : public strategy::controller::FeedbackReceiver {
  public:
    explicit PublicationReceiver(strategy::controller::FeedbackReceiver *receiver)
        : receiver_(receiver) {}
    void session_begin(const Observation &value) override { receiver_->session_begin(value); }
    void after_action(const strategy::controller::ActionFeedback &value) override {
      if (depth_) pending_.push_back(value);
      else receiver_->after_action(value);
    }
    void search_interval(const strategy::controller::SearchIntervalFeedback &value) override {
      assert(!depth_);
      receiver_->search_interval(value);
    }
    void session_end(const Observation &value, int verdict, StopReason reason) override {
      assert(!depth_);
      receiver_->session_end(value, verdict, reason);
    }
    void begin_publication() { ++depth_; }
    void end_publication() {
      assert(depth_);
      if (--depth_) return;
      for (const auto &record : pending_) receiver_->after_action(record);
      pending_.clear();
    }
  private:
    strategy::controller::FeedbackReceiver *receiver_;
    unsigned depth_ = 0;
    std::vector<strategy::controller::ActionFeedback> pending_;
  };
public:
  using Observe = Observation (*)(const Kernel &, const void *);
  PolicyCompletion(Kernel &k, strategy::controller::FeedbackReceiver *receiver,
                   Observe observe, const void *owner)
      : kernel_(k), receiver_(receiver), session_(receiver ? &receiver_ : nullptr),
        observe_(observe), owner_(owner) {}
  bool enabled() const { return session_.enabled(); }
  bool searching() const { return searching_; }
  bool have_search_configuration() const { return have_search_configuration_; }
  uint64_t sequence() const { return session_.sequence(); }
  const std::optional<strategy::controller::ActionResult> &last_action() const { return last_action_; }
  const std::string &last_native_measurement() const { return last_native_measurement_; }
  void begin() { if (enabled()) session_.begin(observe()); }

  template <class Build, class Apply,
            class Describe = strategy::controller::DescribeFeedbackOutcome>
  auto execute(FeedbackKind kind, const char *identity, uint64_t instance,
               Build &&build, Apply &&apply, Describe &&describe = Describe{}) {
    if (!enabled()) return apply();
    session_.pause_measurement();
    NativeFeedbackSpec spec = build();
    const uint64_t before = spec.meter ? spec.meter(kernel_) : 0;
    const auto before_state = strategy::state_version(kernel_);
    const uint64_t ticks = kernel_.stats.ticks, roots = kernel_.stats.root_assignments;
    const uint64_t interrupted = kernel_.stats.rule_interrupted;
    const uint64_t stop_fires = kernel_.stats.stop_fuzz_fires;
    return session_.execute(kind, identity, instance, [&] { return observe(); },
        std::forward<Apply>(apply), nullptr, nullptr, [&](const auto &result) {
          auto outcome = [&] {
            if constexpr (std::is_invocable_v<Describe &, decltype(result),
                                              NativeActionDescription &>)
              return describe(result, spec.description);
            else
              return describe(result);
          }();
          // Support transactions prepare/publish native execution; they are
          // not additional registered-rule applications. Keep their measured
          // work and state deltas, without inheriting the generic Ok result's
          // synthetic application or proof-rule progress. Explicit native
          // composition currencies remain independent of application counts.
          if (kind == FeedbackKind::Support) {
            outcome.invocation.applications = 0;
            outcome.invocation.made_rule_progress = false;
            if (outcome.invocation.work_unit == strategy::WorkUnit::Applications) {
              outcome.invocation.work = 0;
              outcome.composition_overshoot = 0;
            }
          }
          // Observe native interruption events without introducing another
          // poll (and therefore another fuzz RNG draw). Preserve terminal,
          // conflict and rejected-request outcomes unchanged.
          using strategy::InvocationStatus;
          if ((kernel_.stats.rule_interrupted != interrupted ||
               kernel_.stats.stop_fuzz_fires != stop_fires) &&
              (outcome.invocation.status == InvocationStatus::Completed ||
               outcome.invocation.status == InvocationStatus::ScopedFixpoint ||
               outcome.invocation.status == InvocationStatus::NoChange ||
               outcome.invocation.status == InvocationStatus::BudgetExhausted)) {
            outcome.invocation.status = InvocationStatus::Interrupted;
            outcome.invocation.selection_complete = false;
          }
          auto &native = spec.description;
          if (spec.meter) {
            const uint64_t after = spec.meter(kernel_);
            native.accounting_overflow = after < before;
            native.work = native.accounting_overflow ? 0 : after - before;
          } else if (kind == FeedbackKind::Support &&
                     outcome.invocation.work_unit == strategy::WorkUnit::Applications) {
            // A support call without its own meter still has the already
            // captured native tick counter. Zero counted applications does
            // not imply zero effort; preserve measured ticks separately.
            native.effort_currency = "ticks";
            native.accounting_overflow = kernel_.stats.ticks < ticks;
            native.work = native.accounting_overflow ? 0 : kernel_.stats.ticks - ticks;
          } else {
            static constexpr const char *currencies[] = {
                "applications", "bve-ticks", "propagations", "sr-discovery-ticks"};
            const unsigned unit = static_cast<unsigned>(outcome.invocation.work_unit);
            native.effort_currency = unit < 4 ? currencies[unit] : "invalid-work-unit";
            native.work = outcome.invocation.work;
          }
          if (!native.effort_allowance.is_unlimited() &&
              native.work > native.effort_allowance.amount())
            native.overshoot = native.work - native.effort_allowance.amount();
          auto &last = last_action_.emplace();
          last.sequence = sequence();
          last.type = kind == FeedbackKind::ResumeSearch ? strategy::controller::ActionType::ResumeSearch
                                                        : strategy::controller::ActionType::Rule;
          last.before = before_state;
          last.after = strategy::state_version(kernel_);
          last.invocation = outcome.invocation;
          last.invocation.before = last.before;
          last.invocation.after = last.after;
          last.composition_overshoot = outcome.composition_overshoot;
          last.resumed_search = outcome.resumed_search;
          last.propagation_ticks = kernel_.stats.ticks - ticks;
          last.root_assignments = kernel_.stats.root_assignments - roots;
          last_native_measurement_ = std::string(identity) + ':' + std::to_string(instance) + ':' +
              native.effort_currency + ':' + std::to_string(native.work) + ':' +
              std::to_string(native.overshoot) + ':' + std::to_string(native.accounting_overflow);
          return outcome;
        }, &spec.description);
  }

  // Native composed calls may publish advice or restore parameters only after
  // several leaf applications. Retain each leaf's immutable measurements,
  // delivering them at that existing safe boundary without moving publication.
  void begin_publication() { if (enabled()) receiver_.begin_publication(); }
  void end_publication() {
    if (!enabled()) return;
    session_.pause_measurement();
    receiver_.end_publication();
    session_.resume_measurement();
  }

  void end_interval(StopReason reason) {
    if (!enabled() || !searching_) return;
    session_.pause_measurement();
    searching_ = false;
    session_.end_interval(observe(), reason);
  }
  void resume(IntervalStart start, const char *description) {
    if (!enabled()) return;
    execute(FeedbackKind::ResumeSearch, "ResumeSearch", INSTANCE_CANONICAL,
        [&] { NativeFeedbackSpec spec; spec.description.description = description; return spec; },
        [&] {
          searching_ = true;
          have_search_configuration_ = true;
          return ApplyResult::Ok;
        }, [](ApplyResult result) {
          auto out = strategy::controller::feedback_outcome(result);
          out.invocation.work = out.invocation.applications = 0;
          out.invocation.made_rule_progress = false;
          out.resumed_search = true;
          return out;
        });
    continue_search(start);
  }
  void continue_search(IntervalStart start) {
    if (!enabled()) return;
    searching_ = true;
    have_search_configuration_ = true;
    session_.begin_interval(observe(), start);
  }
  void finish(int verdict, StopReason reason) {
    if (!enabled()) return;
    session_.pause_measurement();
    searching_ = false;
    session_.finish(observe(), verdict, reason);
  }

private:
  Observation observe() const { return observe_(kernel_, owner_); }
  Kernel &kernel_;
  PublicationReceiver receiver_;
  strategy::controller::FeedbackSession session_;
  Observe observe_;
  const void *owner_;
  bool searching_ = false;
  bool have_search_configuration_ = false;
  std::optional<strategy::controller::ActionResult> last_action_;
  std::string last_native_measurement_;
};

class CompletionPublication {
public:
  explicit CompletionPublication(PolicyCompletion *completion) : completion_(completion) {
    if (completion_) completion_->begin_publication();
  }
  ~CompletionPublication() { if (completion_) completion_->end_publication(); }
  CompletionPublication(const CompletionPublication &) = delete;
  CompletionPublication &operator=(const CompletionPublication &) = delete;
private:
  PolicyCompletion *completion_;
};

// Native hot calls remain statically dispatched. A null completion pointer
// does not construct descriptions, query meters, observe state or read a clock.
template <class Build, class Apply,
          class Describe = strategy::controller::DescribeFeedbackOutcome>
auto complete_action(PolicyCompletion *completion, FeedbackKind kind,
                     const char *identity, uint64_t instance,
                     Build &&build, Apply &&apply, Describe &&describe = Describe{}) {
  if (!completion) return apply();
  return completion->execute(kind, identity, instance, std::forward<Build>(build),
      std::forward<Apply>(apply), std::forward<Describe>(describe));
}

} // namespace tactix::execution
