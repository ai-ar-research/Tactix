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

// What earns another round of a fixpoint program. SymmetryBreakingOutput is
// Satsuma's iteration condition `sbp.n_clauses() >= 1` (satsuma.h:809): a
// committed SR consequence other than a unique-literal AMO completion binary,
// which the owner adds to its clause database, not to the predicate.
enum class RepeatTrigger : uint8_t {
  AnyProgress,
  SymmetryBreakingOutput,
};

inline uint64_t symmetry_breaking_outputs(const Kernel &k) {
  return k.stats.sr_applied - k.stats.sr_unique_literal_amo_binaries;
}

// A generated Orbitopal/GraphSymmetry pass that ended without effect, complete
// or at a limit, sees the same formula while no effective transition
// intervenes; its shared allowance can only have shrunk. A scheduler can share
// one record across the sequences of a root lifecycle so that such a
// detection is not paid again.
class UnchangedDetections {
  std::vector<OnlineInvocation> completed_;

public:
  static bool eligible(const OnlineInvocation &step) {
    const auto *parameters =
        std::get_if<SubstitutionRedundancyParameters>(&step.parameters);
    return step.rule == "SubstitutionRedundancy" && parameters &&
           step.granularity == InvocationGranularity::CompleteCurrentPass &&
           (parameters->producer == SubstitutionRedundancyProducer::Orbitopal ||
            parameters->producer ==
                SubstitutionRedundancyProducer::GraphSymmetry);
  }
  bool current(const OnlineInvocation &step) const {
    return eligible(step) &&
           std::any_of(completed_.begin(), completed_.end(),
                       [&](const OnlineInvocation &done) {
                         return done.granularity == step.granularity &&
                                done.post_application ==
                                    step.post_application &&
                                std::get<SubstitutionRedundancyParameters>(
                                    done.parameters) ==
                                    std::get<SubstitutionRedundancyParameters>(
                                        step.parameters);
                       });
  }
  void record(const OnlineInvocation &step) {
    if (eligible(step) && !current(step))
      completed_.push_back(step);
  }
  void transition() { completed_.clear(); }
};

struct OnlineStrategyStage {
  OnlineStrategyProgram program;
  BudgetExhaustionPolicy on_budget_exhaustion = BudgetExhaustionPolicy::Stop;
  // Optional followups, by stable step identity. A followup starts only after
  // another (non-followup) step makes progress in this sequence scope. The
  // first completed followup consumes that credit; incomplete or interrupted
  // applications retain it. Explicit atomic rule invocation is unaffected.
  std::vector<StepId> progress_followups = {};
  // A listed step may start only before any effective application in this
  // sequence scope. Admission lasts through the selected batch; its own
  // progress cannot truncate it. Repetition retains the scope's history.
  std::vector<StepId> before_progress = {};
  // A GraphSymmetry step can internally repeat the same structured discovery
  // already attempted by an earlier root stage.  When this feedback switch is
  // set, that internal handoff is admitted only after an earlier stage made an
  // effective formula transition.  Generic graph discovery remains enabled.
  bool structured_handoff_after_prior_progress = false;
  RepeatTrigger repeat_trigger = RepeatTrigger::AnyProgress;
  // Satsuma's sticky `reached_limits` (satsuma.h:49, 194, 267, 810): no
  // further round once any generated discovery reached a detection limit.
  bool stop_repeat_at_detection_limit = false;
};

// Typed sequence controller with an explicit disposition for a bounded
// producer. Every application uses OnlineStrategyExecutor. Continuing discards
// only that producer's pending scope and preserves its spent action budget;
// interruption and stale/resource failures never skip a producer.
class OnlineStrategySequence {
  OnlineStrategyProgram program_;
  BudgetExhaustionPolicy exhaustion_ = BudgetExhaustionPolicy::Continue;
  bool configured_ = false;
  const Kernel *owner_ = nullptr;
  StateVersion expected_;
  std::vector<OnlineStrategyExecutor> executors_;
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
  RepeatTrigger repeat_trigger_ = RepeatTrigger::AnyProgress;
  bool stop_repeat_at_detection_limit_ = false;
  bool detection_limit_reached_ = false;
  bool round_open_ = false;
  uint64_t round_outputs_ = 0;
  bool entered_ = false;
  UnchangedDetections *unchanged_ = nullptr;
  uint64_t unchanged_skips_ = 0;
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
    round_open_ = false;
    entered_ = false;
  }

  void leave_step() {
    ++current_;
    step_progress_ = false;
    followup_admitted_ = false;
    before_progress_admitted_ = false;
    entered_ = false;
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
    // Build every replacement before publishing any configuration change.
    // Stable step id, rule and work unit determine retained budget identity,
    // exactly as in the atomic executor. Reordering does not replenish it.
    std::vector<OnlineStrategyExecutor> replacement(program.sequence.size());
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
    // Reconfiguration declares a new scope, even when action budgets are
    // retained. Plain advance/resume and internal rounds preserve the credit.
    followup_credit_ = followup_admitted_ = false;
    finish_round_ = false;
    complete_ = any_incomplete_ = step_progress_ = progress_ = incomplete_ = false;
    repeat_trigger_ = RepeatTrigger::AnyProgress;
    stop_repeat_at_detection_limit_ = detection_limit_reached_ = false;
    round_open_ = entered_ = false;
    unchanged_skips_ = 0;
    configured_ = true;
    owner_ = nullptr;
    terminal_ = {};
    if (error) error->clear();
    return true;
  }

  bool configure(const OnlineStrategyStage &stage, uint64_t revision = 1,
                 ReconfigureMode mode = ReconfigureMode::ResetRuntime,
                 std::string *error = nullptr) {
    if (!configure(stage.program, revision, mode, stage.on_budget_exhaustion,
                   error, stage.progress_followups, stage.before_progress))
      return false;
    repeat_trigger_ = stage.repeat_trigger;
    stop_repeat_at_detection_limit_ = stage.stop_repeat_at_detection_limit;
    return true;
  }

  // Optional caller-owned record shared with other sequences; null disables
  // unchanged-detection skipping. The record must outlive every advance.
  void share_unchanged_detections(UnchangedDetections *record) {
    unchanged_ = record;
  }

  bool complete() const { return complete_; }
  uint64_t rounds() const { return rounds_; }
  bool detection_limit_reached() const { return detection_limit_reached_; }
  uint64_t unchanged_detection_skips() const { return unchanged_skips_; }
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

  // Dispatch only actual executor advances. Admission/skip/round policy stays
  // here, and the statically typed seam sees the exact immutable step request.
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
    if (owner_ && (owner_ != &k ||
        expected_.arena_generation != current.arena_generation ||
        expected_.touch_epoch != current.touch_epoch ||
        expected_.strategy_epoch != current.strategy_epoch)) {
      // An external transition may make stopped discovery useful again. The
      // underlying executor still rejects stale pending selections; clearing
      // retry suppression does not authorize consuming an old payload.
      std::fill(stopped_generation_.begin(), stopped_generation_.end(), std::nullopt);
    }
    owner_ = &k;
    expected_ = current;
    for (;;) {
      // Poll before resetting any selection or crossing any producer boundary.
      if ((stop && *stop) || (k.stop && *k.stop)) {
        InvocationResult out;
        out.configuration_revision = revision_;
        out.before = out.after = state_version(k);
        out.status = InvocationStatus::Interrupted;
        return out;
      }
      if (complete_)
        return terminal_;
      if (!round_open_) {
        round_open_ = true;
        round_outputs_ = symmetry_breaking_outputs(k);
      }
      if (current_ < executors_.size()) {
        auto &executor = executors_[current_];
        if (before_progress_[current_] && !before_progress_admitted_) {
          if (effective_generation_) {
            InvocationResult skipped;
            skipped.configuration_revision = revision_;
            skipped.before = skipped.after = state_version(k);
            skipped.step_id = program_.sequence[current_].id;
            skipped.status = InvocationStatus::NoChange;
            skipped.selection_complete = true;
            ++current_;
            step_progress_ = false;
            followup_admitted_ = false;
            return skipped;
          }
          before_progress_admitted_ = true;
        }
        const bool followup = progress_followups_[current_];
        if (followup && !followup_admitted_) {
          if (!followup_credit_) {
            InvocationResult skipped;
            skipped.configuration_revision = revision_;
            skipped.before = skipped.after = state_version(k);
            skipped.step_id = program_.sequence[current_].id;
            skipped.status = InvocationStatus::NoChange;
            skipped.selection_complete = true;
            ++current_;
            step_progress_ = false;
            return skipped;
          }
          followup_admitted_ = true;
        }
        if (unchanged_ && !entered_ &&
            unchanged_->current(program_.sequence[current_])) {
          InvocationResult skipped;
          skipped.configuration_revision = revision_;
          skipped.before = skipped.after = state_version(k);
          skipped.step_id = program_.sequence[current_].id;
          skipped.status = InvocationStatus::NoChange;
          skipped.selection_complete = true;
          ++unchanged_skips_;
          leave_step();
          return skipped;
        }
        if (exhaustion_ == BudgetExhaustionPolicy::Continue &&
            stopped_generation_[current_] == effective_generation_) {
          InvocationResult skipped;
          skipped.configuration_revision = revision_;
          skipped.before = skipped.after = state_version(k);
          skipped.step_id = program_.sequence[current_].id;
          skipped.status = InvocationStatus::BudgetExhausted;
          skipped.budget_status = BudgetStatus::Exhausted;
          incomplete_ = any_incomplete_ = true;
          ++current_;
          step_progress_ = false;
          followup_admitted_ = false;
          before_progress_admitted_ = false;
          return skipped;
        }
        ++dispatches_[current_];
        entered_ = true;
        InvocationResult out = dispatch(program_.sequence[current_], [&] {
          return executor.advance(k, stop, sr_discovery_allowance, discovery_feedback);
        });
        expected_ = out.after;
        if (out.made_rule_progress && unchanged_)
          unchanged_->transition();
        const bool step_limit = executor.detection_limit_reached();
        detection_limit_reached_ |= step_limit;
        // A producer can prove that its affordable deterministic prefix is
        // still growing without publishing an arbitrary subgroup. Keep the
        // same step pending while the enclosing scheduler decides whether to
        // grant more discovery work.
        if (out.discovery_progress_units || out.discovery_entry_required)
          return out;
        if (!followup && out.made_rule_progress)
          followup_credit_ = true;
        // A useful followup may enable more discovery. Keep its actual effect
        // visible to scoped fixpoints and exhausted-producer invalidation,
        // while preventing it from granting another followup credit itself.
        progress_ |= out.made_rule_progress;
        step_progress_ |= out.made_rule_progress;
        effective_generation_ += out.made_rule_progress;
        if (out.status == InvocationStatus::BudgetExhausted) {
          incomplete_ = any_incomplete_ = true;
          if (exhaustion_ == BudgetExhaustionPolicy::Stop)
            return out;
          if (exhaustion_ == BudgetExhaustionPolicy::FinishRoundThenStop)
            finish_round_ = true;
          // A published prefix may itself enable more discovery next round.
          // Without effective mutation, do not retry an identical exhausted job.
          stopped_generation_[current_] = step_progress_
              ? std::nullopt : std::optional<uint64_t>(effective_generation_);
          if (unchanged_ && !step_progress_)
            unchanged_->record(program_.sequence[current_]);
          leave_step();
        } else if (executor.complete()) {
          if (followup)
            followup_credit_ = false;
          if (unchanged_ && !step_progress_)
            unchanged_->record(program_.sequence[current_]);
          leave_step();
        }
        // Resource failures and terminal/conflict statuses are returned to the
        // caller unchanged. In particular an interrupt never skips a producer.
        return out;
      }
      ++rounds_;
      // A round earns repetition by its trigger; a reached detection limit
      // ends repetition for the rest of this sequence.
      const bool earned =
          (repeat_trigger_ == RepeatTrigger::AnyProgress
               ? progress_
               : symmetry_breaking_outputs(k) > round_outputs_) &&
          !(stop_repeat_at_detection_limit_ && detection_limit_reached_);
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
        repeat = earned && (!limit || rounds_ < limit);
        break;
      }
      }
      if (repeat && !finish_round_) {
        // A changed formula may make previously bounded discovery productive.
        // Reconfiguration drops stale payloads but never replenishes an action
        // budget. A no-progress fixpoint round is never retried.
        configure_round(ReconfigureMode::RetainMatchingStepBudgets);
        continue;
      }
      complete_ = true;
      terminal_ = {};
      terminal_.configuration_revision = revision_;
      terminal_.before = terminal_.after = state_version(k);
      const bool fixpoint = fixpoint_repeat(program_.repeat.mode);
      const bool incomplete = fixpoint ? incomplete_ : any_incomplete_;
      terminal_.selection_complete = !incomplete;
      if (incomplete || (fixpoint && progress_ && earned) ||
          (fixpoint && stop_repeat_at_detection_limit_ &&
           detection_limit_reached_)) {
        terminal_.status = InvocationStatus::BudgetExhausted;
        terminal_.budget_status = BudgetStatus::Exhausted;
      } else {
        terminal_.status = fixpoint
                               ? InvocationStatus::ScopedFixpoint
                               : InvocationStatus::Completed;
      }
      return terminal_;
    }
  }
};


} // namespace tactix::strategy
