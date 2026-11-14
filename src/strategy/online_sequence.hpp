#pragma once

#include "strategy/online_inprocess.hpp"

#include <algorithm>
#include <cassert>
#include <optional>
#include <utility>

namespace tactix::strategy {

struct DirectSequenceDispatch {
  template <class Apply>
  InvocationResult operator()(const OnlineInvocation &, Apply &&apply) const {
    return apply();
  }
};

enum class BudgetExhaustionPolicy : uint8_t {
  Stop,                 // Keep the pending scope until caller reconfiguration.
  Continue,             // Advance; changed later rounds may retry discovery.
  FinishRoundThenStop,   // Complete later steps once, then prohibit repetition.
};

struct OnlineStrategyStage {
  OnlineStrategyProgram program;
  BudgetExhaustionPolicy on_budget_exhaustion = BudgetExhaustionPolicy::Stop;
  // Optional followups by stable step id: one starts only after a non-followup
  // step progresses in this scope; the first completed followup consumes the
  // credit, incomplete ones keep it.
  std::vector<StepId> progress_followups = {};
  // A listed step may start only before any effective application.
  std::vector<StepId> before_progress = {};
  // When set, a GraphSymmetry step's internal structured handoff is admitted
  // only after an earlier stage made an effective transition; generic discovery
  // stays enabled.
  bool structured_handoff_after_prior_progress = false;
};

// Typed sequence controller with an explicit disposition for bounded producers,
// all applications through OnlineStrategyExecutor. Continuing discards only
// that producer's pending scope and keeps its spent budget; interruption and
// stale/resource failures never skip a producer.
template <class Executor = OnlineStrategyExecutor>
class BasicOnlineStrategySequence {
  OnlineStrategyProgram program_;
  BudgetExhaustionPolicy exhaustion_ = BudgetExhaustionPolicy::Continue;
  bool configured_ = false;
  const Kernel *owner_ = nullptr;
  StateVersion expected_;
  std::vector<Executor> executors_;
  size_t current_ = 0;
  uint64_t rounds_ = 0;
  uint64_t revision_ = 0;
  uint64_t effective_generation_ = 0;
  std::vector<std::optional<uint64_t>> stopped_generation_;
  std::vector<uint64_t> dispatches_;
  std::vector<bool> progress_followups_;
  std::vector<bool> before_progress_;
  bool before_progress_admitted_ = false;
  bool followup_credit_ = false;
  bool followup_admitted_ = false;
  bool step_progress_ = false;
  bool progress_ = false;
  bool incomplete_ = false;
  bool any_incomplete_ = false;
  bool complete_ = false;
  bool finish_round_ = false;
  InvocationResult terminal_;

  void configure_round(ReconfigureMode mode) {
    for (size_t i = 0; i < executors_.size(); ++i) {
      OnlineStrategyProgram one;
      one.sequence.push_back(program_.sequence[i]);
      const bool configured = executors_[i].configure(one, revision_, mode);
      assert(configured);
      (void)configured;
    }
    current_ = 0;
    step_progress_ = false;
    followup_admitted_ = false;
    before_progress_admitted_ = false;
    progress_ = false;
    incomplete_ = false;
  }

  // A result with no dispatch, state transition or application.
  InvocationResult undispatched(const Kernel &k, InvocationStatus status) const {
    InvocationResult out;
    out.configuration_revision = revision_;
    out.before = out.after = state_version(k);
    out.status = status;
    return out;
  }

  // Admission flags a step transition clears, in addition to step progress.
  enum class Admissions : uint8_t { Kept, Followup, All };

  // Leave the current step; the next starts without step progress.
  void next_step(Admissions cleared = Admissions::All) {
    ++current_;
    step_progress_ = false;
    if (cleared != Admissions::Kept) followup_admitted_ = false;
    if (cleared == Admissions::All) before_progress_admitted_ = false;
  }

  // Skip the step: trivially complete, or retained producer exhaustion.
  InvocationResult skip(const Kernel &k, InvocationStatus status,
                        Admissions cleared) {
    InvocationResult skipped = undispatched(k, status);
    skipped.step_id = program_.sequence[current_].id;
    skipped.selection_complete = status == InvocationStatus::NoChange;
    if (status == InvocationStatus::BudgetExhausted)
      skipped.budget_status = BudgetStatus::Exhausted;
    next_step(cleared);
    return skipped;
  }

public:
  bool configure(const OnlineStrategyProgram &program, uint64_t revision = 1,
                 ReconfigureMode mode = ReconfigureMode::ResetRuntime,
                 BudgetExhaustionPolicy exhaustion = BudgetExhaustionPolicy::Continue,
                 std::string *error = nullptr,
                 std::span<const StepId> progress_followups = {},
                 std::span<const StepId> before_progress = {}) {
    if (exhaustion != BudgetExhaustionPolicy::Stop &&
        exhaustion != BudgetExhaustionPolicy::Continue &&
        exhaustion != BudgetExhaustionPolicy::FinishRoundThenStop) {
      if (error) *error = "budget exhaustion policy is invalid";
      return false;
    }
    if (mode != ReconfigureMode::ResetRuntime &&
        mode != ReconfigureMode::RetainMatchingStepBudgets) {
      if (error) *error = "reconfiguration mode is invalid";
      return false;
    }
    if (!validate_online_strategy_program(program, error))
      return false;
    std::vector<bool> followups(program.sequence.size(), false);
    for (const StepId id : progress_followups) {
      const auto at = std::find_if(program.sequence.begin(), program.sequence.end(),
          [id](const OnlineInvocation &step) { return step.id == id; });
      if (at == program.sequence.end() || followups[at - program.sequence.begin()]) {
        if (error) *error = "progress followup must name a unique existing step";
        return false;
      }
      followups[at - program.sequence.begin()] = true;
    }
    std::vector<bool> initial(program.sequence.size(), false);
    for (const StepId id : before_progress) {
      const auto at = std::find_if(program.sequence.begin(), program.sequence.end(),
          [id](const OnlineInvocation &step) { return step.id == id; });
      if (at == program.sequence.end() || initial[at - program.sequence.begin()] ||
          followups[at - program.sequence.begin()]) {
        if (error) *error = "before-progress step must name a unique existing non-followup";
        return false;
      }
      initial[at - program.sequence.begin()] = true;
    }
    // Build every replacement before publishing; budgets keep identity.
    std::vector<Executor> replacement(program.sequence.size());
    for (size_t i = 0; i < program.sequence.size(); ++i) {
      if (configured_ && mode == ReconfigureMode::RetainMatchingStepBudgets)
        for (size_t j = 0; j < program_.sequence.size(); ++j)
          if (program_.sequence[j].id == program.sequence[i].id) {
            replacement[i] = executors_[j];
            break;
          }
      OnlineStrategyProgram one;
      one.sequence.push_back(program.sequence[i]);
      if (!replacement[i].configure(one, revision, mode, error))
        return false;
    }
    program_ = program;
    executors_ = std::move(replacement);
    exhaustion_ = exhaustion;
    revision_ = revision;
    rounds_ = effective_generation_ = current_ = 0;
    stopped_generation_.assign(program.sequence.size(), std::nullopt);
    dispatches_.assign(program.sequence.size(), 0);
    progress_followups_ = std::move(followups);
    before_progress_ = std::move(initial);
    before_progress_admitted_ = false;
    // Reconfiguration declares a new scope; advances keep the credit.
    followup_credit_ = followup_admitted_ = false;
    finish_round_ = false;
    complete_ = any_incomplete_ = step_progress_ = progress_ = incomplete_ = false;
    configured_ = true;
    owner_ = nullptr;
    terminal_ = {};
    if (error) error->clear();
    return true;
  }

  bool configure(const OnlineStrategyStage &stage, uint64_t revision = 1,
                 ReconfigureMode mode = ReconfigureMode::ResetRuntime,
                 std::string *error = nullptr) {
    return configure(stage.program, revision, mode, stage.on_budget_exhaustion,
                     error, stage.progress_followups, stage.before_progress);
  }

  bool complete() const { return complete_; }
  uint64_t rounds() const { return rounds_; }
  uint64_t dispatches(size_t step) const { return dispatches_[step]; }
  uint64_t consumed(size_t step) const {
    return executors_[step].step_consumed(program_.sequence[step].id);
  }

  InvocationResult advance(Kernel &k, volatile sig_atomic_t *stop = nullptr,
                           Allowance *sr_discovery_allowance = nullptr,
                           DiscoveryFeedbackPolicy discovery_feedback = {}) {
    return advance_with(k, DirectSequenceDispatch{}, stop,
                        sr_discovery_allowance, discovery_feedback);
  }

  // Dispatch only actual executor advances.
  template <class Dispatch>
  InvocationResult advance_with(Kernel &k, Dispatch &&dispatch,
                           volatile sig_atomic_t *stop = nullptr,
                           Allowance *sr_discovery_allowance = nullptr,
                           DiscoveryFeedbackPolicy discovery_feedback = {}) {
    if (!configured_) {
      InvocationResult out;
      out.before = out.after = state_version(k);
      out.status = InvocationStatus::InvalidRequest;
      return out;
    }
    const StateVersion current = state_version(k);
    if (owner_ && (owner_ != &k || expected_ != current)) {
      // External transitions may revive discovery; stale picks stay rejected.
      std::fill(stopped_generation_.begin(), stopped_generation_.end(), std::nullopt);
    }
    owner_ = &k;
    expected_ = current;
    for (;;) {
      // Poll before resetting any selection or crossing any producer boundary.
      if ((stop && *stop) || (k.stop && *k.stop))
        return undispatched(k, InvocationStatus::Interrupted);
      if (complete_)
        return terminal_;
      if (current_ < executors_.size()) {
        auto &executor = executors_[current_];
        if (before_progress_[current_] && !before_progress_admitted_) {
          if (effective_generation_)
            return skip(k, InvocationStatus::NoChange, Admissions::Followup);
          before_progress_admitted_ = true;
        }
        const bool followup = progress_followups_[current_];
        if (followup && !followup_admitted_) {
          if (!followup_credit_)
            return skip(k, InvocationStatus::NoChange, Admissions::Kept);
          followup_admitted_ = true;
        }
        if (exhaustion_ == BudgetExhaustionPolicy::Continue &&
            stopped_generation_[current_] == effective_generation_) {
          incomplete_ = any_incomplete_ = true;
          return skip(k, InvocationStatus::BudgetExhausted, Admissions::All);
        }
        ++dispatches_[current_];
        InvocationResult out = dispatch(program_.sequence[current_], [&] {
          return executor.advance(k, stop, sr_discovery_allowance, discovery_feedback);
        });
        expected_ = out.after;
        // A producer can show its deterministic prefix is still growing without
        // publishing a subgroup; keep the step pending while the scheduler
        // decides on more work.
        if (out.discovery_progress_units || out.discovery_entry_required)
          return out;
        if (!followup && out.made_rule_progress)
          followup_credit_ = true;
        // A followup's effect counts but grants no further followup credit.
        progress_ |= out.made_rule_progress;
        step_progress_ |= out.made_rule_progress;
        effective_generation_ += out.made_rule_progress;
        if (out.status == InvocationStatus::BudgetExhausted) {
          incomplete_ = any_incomplete_ = true;
          if (exhaustion_ == BudgetExhaustionPolicy::Stop)
            return out;
          if (exhaustion_ == BudgetExhaustionPolicy::FinishRoundThenStop)
            finish_round_ = true;
          // Retry next round only after an effective mutation.
          stopped_generation_[current_] = step_progress_
              ? std::nullopt : std::optional<uint64_t>(effective_generation_);
          next_step();
        } else if (executor.complete()) {
          if (followup)
            followup_credit_ = false;
          next_step();
        }
        // Resource failures and terminal statuses return unchanged.
        return out;
      }
      ++rounds_;
      bool repeat = false;
      switch (program_.repeat.mode) {
      case RepeatMode::Once:
        break;
      case RepeatMode::Count:
        repeat = rounds_ < program_.repeat.limit;
        break;
      case RepeatMode::UntilScopedFixpoint:
      case RepeatMode::UntilSizeScaledFixpoint: {
        const uint64_t limit = repeat_round_limit(k, program_.repeat);
        repeat = progress_ && (!limit || rounds_ < limit);
        break;
      }
      }
      if (repeat && !finish_round_) {
        // Reconfigure without replenishing budgets; no retry without progress.
        configure_round(ReconfigureMode::RetainMatchingStepBudgets);
        continue;
      }
      complete_ = true;
      const bool fixpoint = fixpoint_repeat(program_.repeat.mode);
      const bool incomplete = fixpoint ? incomplete_ : any_incomplete_;
      const bool exhausted = incomplete || (fixpoint && progress_);
      terminal_ = undispatched(k, exhausted ? InvocationStatus::BudgetExhausted
                                  : fixpoint ? InvocationStatus::ScopedFixpoint
                                             : InvocationStatus::Completed);
      terminal_.selection_complete = !incomplete;
      if (exhausted)
        terminal_.budget_status = BudgetStatus::Exhausted;
      return terminal_;
    }
  }
};

using OnlineStrategySequence = BasicOnlineStrategySequence<>;

} // namespace tactix::strategy
