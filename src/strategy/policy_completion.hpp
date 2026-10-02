#pragma once

#include "strategy/policy_feedback.hpp"

#include <type_traits>

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
public:
  using Observe = Observation (*)(const Kernel &, const void *);
  PolicyCompletion(Kernel &k, strategy::controller::FeedbackReceiver *receiver,
                   Observe observe, const void *owner)
      : kernel_(k), session_(receiver), observe_(observe), owner_(owner) {}
  bool enabled() const { return session_.enabled(); }
  bool searching() const { return searching_; }
  bool have_search_configuration() const { return have_search_configuration_; }
  uint64_t sequence() const { return session_.sequence(); }
  void begin() { if (enabled()) session_.begin(observe()); }

  template <class Build, class Apply,
            class Describe = strategy::controller::DescribeFeedbackOutcome>
  auto execute(FeedbackKind kind, const char *identity, uint64_t instance,
               Build &&build, Apply &&apply, Describe &&describe = Describe{}) {
    if (!enabled()) return apply();
    session_.pause_measurement();
    NativeFeedbackSpec spec = build();
    const uint64_t before = spec.meter ? spec.meter(kernel_) : 0;
    return session_.execute(kind, identity, instance, [&] { return observe(); },
        std::forward<Apply>(apply), nullptr, nullptr, [&](const auto &result) {
          auto outcome = describe(result);
          auto &native = spec.description;
          if (spec.meter) {
            const uint64_t after = spec.meter(kernel_);
            native.accounting_overflow = after < before;
            native.work = native.accounting_overflow ? 0 : after - before;
          } else {
            native.effort_currency = "applications";
            native.work = outcome.invocation.applications;
          }
          if (!native.effort_allowance.is_unlimited() &&
              native.work > native.effort_allowance.amount())
            native.overshoot = native.work - native.effort_allowance.amount();
          return outcome;
        }, &spec.description);
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
  strategy::controller::FeedbackSession session_;
  Observe observe_;
  const void *owner_;
  bool searching_ = false;
  bool have_search_configuration_ = false;
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
