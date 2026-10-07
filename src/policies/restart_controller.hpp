#pragma once

#include "policies/heuristics.hpp"
#include "strategy/action_execution.hpp"
#include "policies/default_restart.hpp"
#include "rules/restart/restart.hpp"
#include "rules/semantic_extension/affine_matrix_search.hpp"
#include "strategy/controller_observation.hpp"
#include "strategy/policy_feedback.hpp"

#include <optional>
#include <chrono>
#include <utility>
#include <map>

namespace tactix::strategy::controller {

// Single-threaded owner attached for one controller lifetime. An embedding's
// previously attached H is transferred in and returned on detach, including
// variable growth and rule advice effects; restoring an old copy would lose
// those changes. The solver's formula/trail/proof always stay in Kernel.
class RestartController {
public:
  explicit RestartController(Kernel &kernel,
                             DefaultStrategyConfig settings = {},
                             FeedbackReceiver *feedback = nullptr)
      : kernel_(kernel), previous_(kernel.h),
        previous_saved_(kernel.saved_phases), settings_(std::move(settings)),
        feedback_(feedback) {
    execution::execute_action(kernel_, execution::HeuristicAttachmentRequest{
        heuristics_, previous_, execution::ActivationStorage::Retain});
    if (feedback_.enabled()) feedback_.begin(observe());
  }

  ~RestartController() {
    if (feedback_.enabled()) {
      feedback_.pause_measurement();
      feedback_.finish(observe(), 0, StopReason::Detached);
    }
    execution::execute_action(kernel_, execution::HeuristicDetachmentRequest{
        heuristics_, previous_, previous_saved_});
  }

  RestartController(const RestartController &) = delete;
  RestartController &operator=(const RestartController &) = delete;

  Observation observe() const {
    const Kernel &k = kernel_;
    Observation result = observe_kernel(k);
    result.sequence = sequence_;
    result.searching = searching_;
    result.have_search_configuration = have_search_;
    result.search = search_;
    result.rotation_intervals = rotation_intervals_;
    result.next_restart_conflicts = restarts_.restart.next_conflicts;
    result.reluctant_u = restarts_.reluctant.u;
    result.reluctant_v = restarts_.reluctant.v;
    result.reluctant_wait = restarts_.reluctant.wait;
    result.fast_glue = restarts_.averages[0].fast_glue.value;
    result.slow_glue = restarts_.averages[0].slow_glue.value;
    result.last_action = last_action_;
    return result;
  }

  ActionResult resume_search(const ActionRequest &request) {
    const auto result = feedback_.execute(
        FeedbackKind::ResumeSearch, "ResumeSearch", INSTANCE_CANONICAL,
        [&] { return observe(); }, [&] { return resume_search_impl(request); }, &request);
    if (result.resumed_search && feedback_.enabled())
      feedback_.begin_interval(observe(), IntervalStart::ResumeSearch);
    return result;
  }

private:
  ActionResult resume_search_impl(const ActionRequest &request) {
    ActionResult result;
    result.type = request.type;
    result.sequence = request.sequence;
    result.before = result.after = state_version(kernel_);
    result.invocation.before = result.invocation.after = result.before;
    if (request.version != ACTION_SPACE_VERSION ||
        request.type != ActionType::ResumeSearch ||
        request.sequence != sequence_ + 1 || !valid(request.search) ||
        !settings_.branching.focused_phase_switch_span ||
        settings_.branching.focused_phase_pattern.empty() ||
        !settings_.restart.reluctant_period ||
        settings_.restart.fast_ema_window <= 0 ||
        settings_.restart.slow_ema_window <= 0)
      return result;
    if (!same_state(request.expected, result.before)) {
      result.invocation.status = InvocationStatus::StaleRequest;
      return result;
    }
    if (searching_ || kernel_.level || kernel_.inconsistent ||
        kernel_.conflict_valid || !kernel_propagation_fixpoint(kernel_)) {
      result.invocation.status = InvocationStatus::Inapplicable;
      return result;
    }
    if (request.search.restart == Restart::Reluctant) {
      if (!have_search_ || search_.restart != Restart::Reluctant) {
        restarts_.reluctant.u = restarts_.reluctant.v = 1;
        restarts_.reluctant.wait = settings_.restart.reluctant_period;
      }
      restarts_.reluctant.trigger = false;
    }
    search_ = request.search;
    if (search_.phase == Phase::FocusedRotation) {
      const auto &branch = settings_.branching;
      rotation_phase_ = branch.focused_phase_pattern[
          (rotation_intervals_ / branch.focused_phase_switch_span) %
          branch.focused_phase_pattern.size()];
      ++rotation_intervals_;
    }
    heuristics_.stable = search_.branching == Branching::Evsids;
    if (heuristics_.stable)
      heuristics_update_scores(kernel_);
    else
      heuristics_reset_queue_search(kernel_);
    restarts_.update_restart_limit(kernel_, settings_.restart);
    searching_ = have_search_ = true;
    sequence_ = request.sequence;
    ++kernel_.strategy_epoch;
    result.after = state_version(kernel_);
    result.invocation.after = result.after;
    result.invocation.status = InvocationStatus::Completed;
    result.resumed_search = true;
    last_action_ = result;
    return result;
  }

public:
  // Search services run only after ResumeSearch. Root work never advances a
  // restart clock, and switching branch or phase never resets its history.
  void search_conflict(uint32_t glue) {
    assert(searching_);
    if (search_.restart == Restart::Reluctant) {
      restarts_.tick_reluctant(settings_.restart);
    } else {
      auto &avg = restarts_.averages[0];
      if (!avg.initialized) {
        avg.fast_glue.init(settings_.restart.fast_ema_window);
        avg.slow_glue.init(settings_.restart.slow_ema_window);
        avg.initialized = true;
      }
      avg.fast_glue.update(glue);
      avg.slow_glue.update(glue);
    }
  }

  bool restart_due() const {
    if (!searching_ ||
        kernel_.stats.conflicts < restarts_.restart.next_conflicts)
      return false;
    if (search_.restart == Restart::Reluctant)
      return restarts_.reluctant.trigger;
    const auto &avg = restarts_.averages[0];
    return avg.initialized && settings_.restart.glue_margin *
        avg.slow_glue.value <= avg.fast_glue.value;
  }

  void capture_before_backtrack() {
    assert(searching_);
    heuristics_capture_target_phases(kernel_);
  }

  ApplyResult restart(uint32_t keep_level) {
    const bool interval = searching_;
    bool applied = false;
    const auto result = feedback_.execute(
        FeedbackKind::SearchService, "Restart", keep_level,
        [&] { return observe(); }, [&] { return restart_impl(keep_level, applied); },
        nullptr, nullptr, [&](ApplyResult native) {
          auto outcome = feedback_outcome(native);
          if (!applied) {
            outcome.invocation.applications = outcome.invocation.work = 0;
            outcome.invocation.status = InvocationStatus::Inapplicable;
          }
          return outcome;
        });
    if (interval && feedback_.enabled() && result != ApplyResult::NoOp) {
      feedback_.pause_measurement();
      const auto after = observe();
      feedback_.end_interval(after, result == ApplyResult::Unsat
                                          ? StopReason::Unsat : StopReason::Restart);
      if (searching_)
        feedback_.begin_interval(after, IntervalStart::PartialContinuation);
    }
    return result;
  }

private:
  ApplyResult restart_impl(uint32_t keep_level, bool &applied) {
    // A stopped solve may leave a non-root trail before this new controller
    // session has installed any configuration. Its entry still uses Restart.
    if ((!searching_ && !kernel_.level) || !restart_guard(kernel_) ||
        (!searching_ && keep_level))
      return ApplyResult::NoOp;
    keep_level = std::min(keep_level, kernel_.level);
    if (!keep_level || keep_level < kernel_.level)
      heuristics_capture_target_phases(kernel_);
    applied = true;
    const auto result =
        execution::execute_action(kernel_, execution::RestartRequest{keep_level});
    ++kernel_.strategy_epoch;
    if (!keep_level || result == ApplyResult::Unsat) {
      searching_ = false;
    } else {
      // Continuing the installed configuration consumes the due restart,
      // without reinstalling branch/phase state or entering the root loop.
      restarts_.reluctant.trigger = false;
      restarts_.update_restart_limit(kernel_, settings_.restart);
    }
    return result;
  }

public:
  ApplyResult full_restart() { return restart(0); }

  template <class Apply>
  ApplyResult search_service(const char *identity, uint64_t instance, Apply &&apply) {
    return feedback_.execute(FeedbackKind::SearchService, identity, instance,
        [&] { return observe(); }, std::forward<Apply>(apply));
  }

  bool affine_matrix_search_enabled() const {
    return kernel_.affine_matrix_search_session &&
           kernel_.affine_matrix_search_session->enabled();
  }

  // An initialized matrix is part of the installed search configuration.
  // At a Boolean fixpoint it may publish one authenticated reason; ordinary
  // Propagate remains the only consumer of that reason.
  AffineMatrixServiceResult affine_matrix_fixpoint_service() {
    NativeActionDescription description;
    description.version = 2;
    description.description = "deterministic-affine-matrix-fixpoint";
    description.effort_currency = "matrix-synchronization-work";
    description.have_affine_matrix_event = true;
    if (kernel_.affine_matrix_search_session)
      description.effort_allowance = WorkLimit::bounded(
          kernel_.affine_matrix_search_session->synchronization_allowance());
    auto describe = [&](const AffineMatrixServiceResult &result) {
      FeedbackOutcome out;
      auto &inv = out.invocation;
      inv.rule_result = result.result;
      inv.work = result.synchronization_work;
      inv.applications = result.status == AffineMatrixServiceStatus::Applied;
      inv.made_rule_progress = result.status == AffineMatrixServiceStatus::Applied;
      inv.selection_complete = true;
      switch (result.status) {
      case AffineMatrixServiceStatus::Applied:
        inv.status = InvocationStatus::Completed;
        break;
      case AffineMatrixServiceStatus::Disabled:
      case AffineMatrixServiceStatus::NoEvent:
      case AffineMatrixServiceStatus::Duplicate:
        inv.status = InvocationStatus::NoChange;
        break;
      case AffineMatrixServiceStatus::Inapplicable:
      case AffineMatrixServiceStatus::UnsupportedProof:
        inv.status = InvocationStatus::Inapplicable;
        break;
      case AffineMatrixServiceStatus::ResourceExhausted:
        inv.status = InvocationStatus::ResourceExhausted;
        break;
      case AffineMatrixServiceStatus::Interrupted:
        inv.status = InvocationStatus::Interrupted;
        break;
      case AffineMatrixServiceStatus::Stale:
        inv.status = InvocationStatus::StaleRequest;
        break;
      case AffineMatrixServiceStatus::Invalid:
        inv.status = InvocationStatus::InvalidRequest;
        break;
      }
      return out;
    };
    const auto result = feedback_.execute_filtered(
        FeedbackKind::SearchService, "AffineMatrixSearch", INSTANCE_CANONICAL,
        [&] { return observe(); },
        [&] {
          AffineMatrixServiceResult result;
          if (!searching_ || !kernel_.affine_matrix_search_session) {
            result.status = AffineMatrixServiceStatus::Inapplicable;
          } else {
            result = affine_matrix_search_step(
                kernel_, *kernel_.affine_matrix_search_session);
          }
          description.work = result.synchronization_work;
          description.affine_matrix_status =
              static_cast<uint8_t>(result.status);
          description.affine_matrix_event_identity = result.event_identity;
          description.affine_matrix_synchronization_work =
              result.synchronization_work;
          description.affine_matrix_session_work = result.session_work;
          description.affine_matrix_reason_width = result.reason_width;
          description.affine_matrix_planner_work = result.live.planner_work;
          description.affine_matrix_revalidation_work =
              result.live.revalidation_work;
          description.affine_matrix_proof_additions = result.live.additions;
          description.affine_matrix_proof_deletions = result.live.deletions;
          description.affine_matrix_helper_extent = result.live.helper_extent;
          description.affine_matrix_journal_bytes = result.live.journal_bytes;
          description.affine_matrix_bcp_pending = result.bcp_pending;
          return result;
        },
        [](const AffineMatrixServiceResult &result) {
          return result.status != AffineMatrixServiceStatus::NoEvent &&
                 result.status != AffineMatrixServiceStatus::Disabled;
        }, describe, &description);
    return result;
  }

  void finish_feedback(int verdict, std::optional<StopReason> reason = std::nullopt) {
    if (feedback_.enabled()) {
      feedback_.pause_measurement();
      feedback_.finish(observe(), verdict, reason.value_or(verdict == 10 ? StopReason::Sat
          : verdict == 20 ? StopReason::Unsat
          : verdict == 0 ? StopReason::Interrupted : StopReason::Error));
    }
  }

  Lit next_decision() {
    assert(searching_ && kernel_.unassigned_active);
    const Var variable = heuristics_next_active_decision_var(kernel_);
    int8_t phase = 0;
    switch (search_.phase) {
    case Phase::Target: phase = heuristics_.target[variable]; break;
    case Phase::Positive: phase = 1; break;
    case Phase::Negative: phase = -1; break;
    case Phase::FocusedRotation: phase = rotation_phase_; break;
    case Phase::Saved: break;
    }
    if (!phase)
      phase = heuristics_.saved[variable];
    return make_lit(variable, phase < 0);
  }

  // Execute one requested atomic rule boundary. The policy decides whether to
  // request further work; the controller never substitutes an ineligible rule.
  // Continuation owned by the native rule remains in Kernel between requests.
  ActionResult apply_rule(const ActionRequest &request,
                          const ActionAllowance &allowance,
                          volatile sig_atomic_t *stop = nullptr) {
    return feedback_.execute(FeedbackKind::Rule, request.invocation.rule.c_str(),
        request.invocation.instances.size() == 1 ? request.invocation.instances[0]
                                               : INSTANCE_CANONICAL,
        [&] { return observe(); },
        [&] { return apply_rule_impl(request, allowance, stop); }, &request, &allowance);
  }

private:
  ActionResult apply_rule_impl(const ActionRequest &request,
                              const ActionAllowance &allowance,
                              volatile sig_atomic_t *stop) {
    ActionResult result;
    result.sequence = request.sequence;
    result.type = request.type;
    result.before = result.after = state_version(kernel_);
    result.invocation.before = result.invocation.after = result.before;
    if (request.version != ACTION_SPACE_VERSION ||
        request.type != ActionType::Rule || request.sequence != sequence_ + 1)
      return result;
    if (!same_state(request.expected, result.before)) {
      result.invocation.status = InvocationStatus::StaleRequest;
      return result;
    }
    if (searching_ || kernel_.level != 0) {
      result.invocation.status = InvocationStatus::Inapplicable;
      return result;
    }
    const int id = tactix_rule_id(request.invocation.rule.c_str());
    if (id < 0 || !root_exclusion(tactix_rules()[id]).empty())
      return result;
    const auto &rule = tactix_rules()[id];
    const bool selected_guard =
        std::holds_alternative<DirectCspGuardedCoverParameters>(request.invocation.parameters)
            ? kernel_.direct_csp_guarded_cover_action_session &&
                  bool(direct_csp_guarded_cover_action_guard(kernel_))
            : bool(rule.guard(kernel_));
    if (!selected_guard &&
        !explicit_fixpoint_propagation_service(request.invocation, kernel_)) {
      result.invocation.status = InvocationStatus::Inapplicable;
      return result;
    }
    OnlineStrategyProgram program;
    auto invocation = request.invocation;
    invocation.id = 1; // stable executor step; public sequence is request-owned
    invocation.repeat = {RepeatMode::Once, 1};
    invocation.work_unit = allowance.unit;
    invocation.budget = WorkLimit::bounded(allowance.amount);
    invocation.rule_work_budget = allowance.native;
    program.sequence.push_back(std::move(invocation));
    if (!validate_online_strategy_program(program))
      return result;
    // Only a matching semantic request retains a pending scope. A new choice
    // replaces that family's selection without undoing committed changes.
    const auto identity = serialize_online_strategy_program(program);
    auto &pending = pending_[request.invocation.rule];
    if (pending.identity != identity ||
        !pending.executor.renew_atomic_dose(program.sequence[0].budget, request.sequence)) {
      if (!pending.executor.configure(program, request.sequence))
        return result;
      pending.identity = identity;
    }
    const auto started = std::chrono::steady_clock::now();
    const auto ticks = kernel_.stats.ticks;
    const auto assignments = kernel_.stats.root_assignments;
    result.invocation = pending.executor.advance(kernel_, stop);
    result.invocation.step_id = request.sequence;
    if (result.invocation.status == InvocationStatus::StaleRequest ||
        result.invocation.status == InvocationStatus::ResourceExhausted ||
        !pending.executor.pending_atomic_scope())
      pending_.erase(request.invocation.rule);
    result.composition_overshoot = result.invocation.work > allowance.amount
                                      ? result.invocation.work - allowance.amount : 0;
    sequence_ = request.sequence;
    result.after = state_version(kernel_);
    result.propagation_ticks = kernel_.stats.ticks - ticks;
    result.root_assignments = kernel_.stats.root_assignments - assignments;
    result.elapsed_nanos = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - started).count());
    last_action_ = result;
    return result;
  }

private:
  static bool same_state(StateVersion a, StateVersion b) {
    return a.arena_generation == b.arena_generation &&
           a.touch_epoch == b.touch_epoch &&
           a.strategy_epoch == b.strategy_epoch;
  }
  Kernel &kernel_;
  Heuristics *previous_;
  std::vector<int8_t> *previous_saved_;
  Heuristics heuristics_;
  DefaultStrategyConfig settings_;
  DefaultRestartControl restarts_;
  uint64_t sequence_ = 0;
  bool searching_ = false;
  bool have_search_ = false;
  uint64_t rotation_intervals_ = 0;
  int8_t rotation_phase_ = 0;
  SearchConfiguration search_;
  std::optional<ActionResult> last_action_;
  struct Pending {
    std::string identity;
    OnlineStrategyExecutor executor;
  };
  std::map<std::string, Pending> pending_;
  FeedbackSession feedback_;
};

} // namespace tactix::strategy::controller
