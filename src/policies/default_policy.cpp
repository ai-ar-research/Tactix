#include "policies/default_policy.hpp"
#include "policies/strategy_settings.hpp"
#include "policies/progress.hpp"
#include "strategy/action_execution.hpp"
#include "strategy/conflict_analysis_execution.hpp"
#include "strategy/execution_scope.hpp"
#include "strategy/lucky_execution.hpp"
#include "strategy/representation_execution.hpp"
#include "strategy/substitute_execution.hpp"
#include "strategy/vivify_execution.hpp"
#include "strategy/parameter_execution.hpp"
#include "strategy/root_execution.hpp"
#include "strategy/slot_execution.hpp"
#include "strategy/occurrence_execution.hpp"
#include "strategy/phase_execution.hpp"
#include "strategy/publication_execution.hpp"
#include "strategy/guidance_execution.hpp"
#include "strategy/reduction_execution.hpp"
#include "strategy/sweep_execution.hpp"
#include "strategy/native_feedback.hpp"
#include "strategy/work_execution.hpp"
#include "strategy/guidance_outcome.hpp"
#include "policies/rule_publication.hpp"
#include "diagnostics/event_trace.hpp"
#include "diagnostics/rule_journal.hpp"
#include "policies/heuristics.hpp"

#include "rules/analyze/analyze.hpp"
#include "rules/backjump/backjump.hpp"
#include "rules/bce/bce.hpp"
#include "rules/bve/bve.hpp"
#include "rules/decide/decide.hpp"
#include "rules/lucky/lucky.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/reduce/reduce.hpp"
#include "policies/ops/rephase/rephase.hpp"
#include "policies/ops/reorder/reorder.hpp"
#include "policies/ops/local_search/local_search.hpp"
#include "rules/lucky/parity_model.hpp"
#include "rules/lucky/parity_refutation.hpp"
#include "rules/lucky/count_preservation.hpp"
#include "rules/lucky/onehot_model.hpp"
#include "rules/lucky/affine_model.hpp"
#include "rules/lucky/exact_one_refutation.hpp"
#include "rules/lucky/balanced_signature_model.hpp"
#include "rules/lucky/fermat_model.hpp"
#include "rules/lucky/cerny_model.hpp"
#include "rules/lucky/lookup_circuit_model.hpp"
#include "rules/lucky/simon_model.hpp"
#include "rules/lucky/stconn_proof.hpp"
#include "rules/lucky/signed_cardinality_proof.hpp"
#include "rules/lucky/exact_cover_model.hpp"
#include "rules/lucky/independent_set_model.hpp"
#include "rules/semantic_extension/affine_matrix_search.hpp"
#include "policies/ops/walk/walk.hpp"
#include "rules/restart/restart.hpp"
#include "rules/registry.hpp"
#include "rules/strengthen/strengthen.hpp"
#include "rules/subsume_forward/subsume_forward.hpp"
#include "rules/substitute/substitute.hpp"
#include "rules/sweep/sweep.hpp"
#include "rules/vivify/vivify.hpp"
#include "policies/ops/switchmode/switchmode.hpp"
#include "strategy/bve_invocation.hpp"
#include "strategy/action_port.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <iomanip>
#include <locale>
#include <sstream>
#include <utility>

namespace tactix {

namespace {

uint64_t saturated_sum(uint64_t first, uint64_t second) {
  return first > UINT64_MAX - second ? UINT64_MAX : first + second;
}

struct AffineSurvivalImage {
  bool enabled;
  uint64_t rank, support;
};

AffineSurvivalImage affine_survival_image(const AffineMatrixSearchSession &session) {
  return {session.enabled(), session.basis_rank(), session.support_variables()};
}

void record_affine_survival(DefaultAffineSearchDiagnostics &stats,
                           const AffineSurvivalImage &before,
                           const AffineMatrixSearchSession &session,
                           uint64_t action, bool isolated_projection = false,
                           bool optional_disable = false) {
  const auto after = affine_survival_image(session);
  stats.active_sources = session.active_sources();
  stats.retained_basis_rank = after.rank;
  stats.retained_support_variables = after.support;
  stats.rank_before_last_action = before.rank;
  stats.rank_after_last_action = after.rank;
  stats.support_before_last_action = before.support;
  stats.support_after_last_action = after.support;
  stats.last_survival_action = action;
  const auto loss = before.rank > after.rank ? before.rank - after.rank : 0;
  const auto gain = after.rank > before.rank ? after.rank - before.rank : 0;
  if (action == 1) {
    stats.recognition_rank_gains = saturated_sum(stats.recognition_rank_gains, gain);
    stats.recognition_rank_losses = saturated_sum(stats.recognition_rank_losses, loss);
  } else if (action == 2) {
    stats.maintenance_rank_gains = saturated_sum(stats.maintenance_rank_gains, gain);
    stats.maintenance_rank_losses = saturated_sum(stats.maintenance_rank_losses, loss);
  } else {
    stats.service_rank_losses = saturated_sum(stats.service_rank_losses, loss);
    if (isolated_projection && after.enabled)
      stats.isolated_projection_rank_losses = saturated_sum(stats.isolated_projection_rank_losses, loss);
  }
  if (before.enabled && !after.enabled) {
    if (optional_disable) ++stats.optional_failure_disables;
    else ++stats.native_session_disables;
    stats.last_disable_cause = optional_disable ? 2 : 1;
  }
}

uint64_t affine_init_work(const Kernel &kernel) {
  uint64_t result = 0;
  for (uint64_t value : {kernel.stats.affine_matrix_init_snapshot_work,
       kernel.stats.affine_matrix_init_recognition_work,kernel.stats.affine_matrix_init_matrix_work,
       kernel.stats.affine_matrix_init_premise_retention_work,kernel.stats.affine_matrix_init_root_unit_work,
       kernel.stats.affine_matrix_init_maintenance_service_work})
    result = saturated_sum(result,value);
  return result;
}

uint64_t affine_service_work(const Kernel &kernel) {
  uint64_t result = kernel.stats.affine_matrix_service_trail_capture_work;
  for (const uint64_t value : {
           kernel.stats.affine_matrix_service_sync_work,
           kernel.stats.affine_matrix_service_session_work,
           kernel.stats.affine_matrix_service_planner_work,
           kernel.stats.affine_matrix_service_revalidation_work,
           kernel.stats.affine_matrix_service_lifecycle_work})
    result = saturated_sum(result, value);
  return result;
}

std::optional<strategy::AffineConsequenceParameters>
default_affine_root_parameters(
    const Kernel &kernel,
    const DefaultStrategyConfig::AffineRootClosure &config) {
  const uint64_t clauses = kernel_irredundant_clauses(kernel);
  const uint64_t extent = kernel.external_to_internal.size();
  if (!config.enabled || !clauses || clauses > config.maximum_clauses ||
      clauses > UINT32_MAX || clauses > UINT64_MAX / 3 ||
      extent > uint64_t{INT32_MAX} ||
      config.maximum_proof_additions > uint64_t{INT32_MAX} - extent)
    return std::nullopt;

  strategy::AffineConsequenceParameters result;
  result.maximum_clauses = clauses;
  result.maximum_snapshot_literals = 3 * clauses;
  // The same allowance covers capture and later exact source/matrix
  // revalidation. Capture itself is bounded by 7*clauses for the admitted
  // ternary shape; the checked target profile supplies the larger ceiling.
  result.maximum_snapshot_work = config.maximum_snapshot_work;
  result.maximum_groups = clauses;
  result.maximum_patterns = clauses;
  result.maximum_elimination_work = config.maximum_recognition_work;
  result.maximum_arity = 3;
  result.maximum_intermediate_width = config.maximum_row_width;
  result.maximum_sources = static_cast<uint32_t>(clauses);
  result.maximum_output_width = 4;
  result.maximum_proof_additions = config.maximum_proof_additions;
  result.maximum_proof_deletions = config.maximum_proof_deletions;
  result.maximum_journal_bytes = config.maximum_journal_bytes;
  result.semantics = strategy::AffineConsequenceSemantics::CertifiedBasisV2;
  result.basis_maximum_work = config.maximum_basis_work;
  result.basis_maximum_intermediate_width = config.maximum_row_width;
  // No GF(2) derivation can cite more recovered source equations than the
  // exact live clause population from which those equations were recognized.
  result.basis_maximum_sources = static_cast<uint32_t>(clauses);
  result.basis_maximum_premises = static_cast<uint32_t>(clauses);
  result.basis_maximum_nodes = static_cast<uint32_t>(clauses);
  result.basis_maximum_direct_width = 3;
  result.maximum_external_extent = static_cast<uint32_t>(
      extent + config.maximum_proof_additions);
  result.maximum_session_keys = clauses;
  result.maximum_session_work = config.maximum_session_work;
  if (!result.valid()) return std::nullopt;
  return result;
}

strategy::controller::FeedbackOutcome affine_init_outcome(
    const AffineMatrixInitResult &result) {
  using strategy::InvocationStatus;
  strategy::controller::FeedbackOutcome out;
  auto &inv = out.invocation;
  inv.rule_result = result.result;
  inv.applications = result.status == AffineMatrixInitStatus::Applied ||
                     result.status == AffineMatrixInitStatus::AppliedPartial;
  inv.made_rule_progress = inv.applications;
  inv.selection_complete = true;
  switch (result.status) {
  case AffineMatrixInitStatus::Applied:
  case AffineMatrixInitStatus::AppliedPartial:
    inv.status = InvocationStatus::Completed;
    break;
  case AffineMatrixInitStatus::NoStructure:
  case AffineMatrixInitStatus::IncompleteImage:
  case AffineMatrixInitStatus::IncompleteMatrix:
    inv.status = InvocationStatus::NoChange;
    break;
  case AffineMatrixInitStatus::Inapplicable:
    inv.status = InvocationStatus::Inapplicable;
    break;
  case AffineMatrixInitStatus::Pending:
    inv.status=InvocationStatus::BudgetExhausted;inv.selection_complete=false;
    break;
  case AffineMatrixInitStatus::ResourceExhausted:
    inv.status = InvocationStatus::ResourceExhausted;
    break;
  case AffineMatrixInitStatus::Interrupted:
    inv.status = InvocationStatus::Interrupted;
    break;
  case AffineMatrixInitStatus::Stale:
    inv.status = InvocationStatus::StaleRequest;
    break;
  case AffineMatrixInitStatus::Invalid:
    inv.status = InvocationStatus::InvalidRequest;
    break;
  }
  return out;
}

strategy::controller::FeedbackOutcome affine_service_outcome(
    const AffineMatrixServiceResult &result,
    strategy::controller::NativeActionDescription &description) {
  using strategy::InvocationStatus;
  description.affine_matrix_status = static_cast<uint8_t>(result.status);
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

  strategy::controller::FeedbackOutcome out;
  auto &inv = out.invocation;
  inv.rule_result = result.result;
  inv.work = result.synchronization_work;
  inv.applications = result.status == AffineMatrixServiceStatus::Applied;
  inv.made_rule_progress = inv.applications;
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
  case AffineMatrixServiceStatus::Pending:
    inv.status = strategy::InvocationStatus::BudgetExhausted;inv.selection_complete=false;
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
}

bool is_affine_structured_step(const strategy::OnlineInvocation &step) {
  return step.id == 109 && step.rule == "Lucky" && step.instances.size() == 1 &&
         step.instances.front() == 8;
}

void pose_affine_structured_stage(DefaultStrategyConfig &config) {
  const auto pose = [&](std::vector<strategy::OnlineStrategyStage> &stages) {
    for (auto stage = stages.begin(); stage != stages.end();) {
      auto &sequence = stage->program.sequence;
      if (!config.inprocess.structured_root_symmetry_enabled &&
          std::any_of(sequence.begin(), sequence.end(), [](const auto &step) {
            return step.rule == "SubstitutionRedundancy";
          })) {
        stage = stages.erase(stage);
        continue;
      }
      bool removed = false;
      if (!config.affine_model.enabled) {
        const size_t before = sequence.size();
        sequence.erase(
            std::remove_if(sequence.begin(), sequence.end(),
                           is_affine_structured_step),
            sequence.end());
        removed = sequence.size() != before;
      } else {
        for (auto &step : sequence)
          if (is_affine_structured_step(step))
            step.parameters = config.affine_model.parameters;
      }
      if (removed && sequence.empty())
        stage = stages.erase(stage);
      else
        ++stage;
    }
  };
  pose(config.inprocess.structured_root_stages);
  pose(config.inprocess.structured_root_bounded_backtracking_stages);
  pose(config.inprocess.structured_root_large_domain_stages);
}

} // namespace

// Admitted support of a persistent affine session (sorted externals).
static bool affine_support_protected(const Kernel &kernel, Var variable) {
  const auto *session = kernel.affine_matrix_search_session;
  if (!session || !session->enabled() || !session->persistent())
    return false;
  const auto &support = session->support();
  return std::binary_search(support.begin(), support.end(),
                            kernel_externalize(kernel, make_lit(variable, false)));
}

std::optional<strategy::AffineConsequenceParameters>
default_affine_search_parameters(const Kernel &kernel,
                                 const DefaultStrategyConfig &config) {
  const auto &search = config.affine_search;
  if (!search.enabled || kernel.nvars < 2 ||
      kernel.external_to_internal.size() >= uint64_t{INT32_MAX})
    return std::nullopt;
  const uint64_t bytes = search.maximum_retained_bytes
      ? search.maximum_retained_bytes : config.affine_root_closure.maximum_journal_bytes;
  const uint64_t initialization = search.maximum_initialization_work
      ? search.maximum_initialization_work : config.inprocess.preprocess_initial_ticks;
  const uint64_t service = search.maximum_service_work
      ? search.maximum_service_work : config.inprocess.minimum_effort_ticks;
  // Count caps are derived from requested source metadata/literal storage.
  // The persistence API separately checks the complete retained population.
  const uint64_t source_entry_bytes = sizeof(AffineLiveSource) +
      sizeof(affine_semantic_extension::Premise) + 2 * sizeof(int32_t);
  const uint64_t clauses = std::min<uint64_t>(UINT32_MAX, bytes / source_entry_bytes);
  const uint32_t rows = static_cast<uint32_t>(std::min<uint64_t>(search.maximum_rows, clauses));
  const uint32_t width = std::min(search.maximum_row_width, kernel.nvars);
  if (!clauses || !rows || width < 2 || !initialization || !service ||
      !config.affine_root_closure.maximum_proof_deletions)
    return std::nullopt;
  strategy::AffineConsequenceParameters result;
  result.maximum_clauses = clauses;
  result.maximum_snapshot_literals = bytes / sizeof(int32_t);
  result.maximum_snapshot_work = initialization;
  result.maximum_groups = clauses;
  result.maximum_patterns = clauses;
  result.maximum_elimination_work = initialization;
  result.maximum_arity = std::min(search.maximum_arity, width);
  result.maximum_intermediate_width = width;
  result.maximum_sources = rows;
  result.maximum_output_width = 4; // Existing V2 typed compatibility field.
  result.maximum_proof_additions = config.affine_root_closure.maximum_proof_additions;
  result.maximum_proof_deletions = config.affine_root_closure.maximum_proof_deletions;
  result.maximum_journal_bytes = config.affine_root_closure.maximum_journal_bytes;
  result.semantics = strategy::AffineConsequenceSemantics::CertifiedBasisV2;
  result.basis_maximum_work = service;
  result.basis_maximum_intermediate_width = width;
  result.basis_maximum_sources = rows;
  result.basis_maximum_premises = static_cast<uint32_t>(clauses);
  result.basis_maximum_nodes = rows;
  result.basis_maximum_direct_width = result.maximum_arity;
  result.maximum_external_extent = static_cast<uint32_t>(std::min<uint64_t>(
      INT32_MAX, saturated_sum(kernel.external_to_internal.size(),result.maximum_proof_additions)));
  result.maximum_session_keys = clauses;
  // The persistent service retains duplicate identities under `bytes`; its
  // session-work meter includes that lifetime key population.  Capping it at
  // one per-call Gaussian quantum eventually discarded a still-useful matrix
  // solely because enough certified reasons had been published.  Derive the
  // lifetime allowance from the already-bounded retained-state budget, while
  // preserving at least one complete service quantum for small profiles.
  result.maximum_session_work = std::max(service, bytes);
  return result.valid() ? std::optional{result} : std::nullopt;
}

using execution::LuckyOpportunity;
using execution::PolicyCompletion;
using strategy::controller::FeedbackKind;
using strategy::controller::IntervalStart;
using strategy::controller::StopReason;

namespace {
template <class Apply>
ApplyResult default_direct_rule(Kernel &k, const char *name,
                                const std::string &candidate, Apply &&apply,
                                ApplyResult declined_result =
                                    ApplyResult::NoOp) {
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  return journal_direct_rule(k, name, candidate,
                             std::function<ApplyResult()>(apply),
                             declined_result);
#else
  (void)k;
  (void)name;
  (void)candidate;
  (void)declined_result;
  return apply();
#endif
}

// Candidate descriptions are diagnostic rule-journal data, not part of the
// production rule transition.  Keep dynamic descriptions lazy so a build
// without the journal does not allocate and shift a string for every hot-path
// application (in particular, every decision).
template <class Candidate, class Apply>
ApplyResult default_direct_rule_lazy(
    Kernel &k, const char *name, Candidate &&candidate, Apply &&apply,
    ApplyResult declined_result = ApplyResult::NoOp) {
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  return journal_direct_rule(k, name, candidate(),
                             std::function<ApplyResult()>(apply),
                             declined_result);
#else
  (void)k;
  (void)name;
  (void)candidate;
  (void)declined_result;
  return apply();
#endif
}

// Macro actions leave the native search interval as public registry requests
// (gap plan Step 1): the same dispatch, timing and strategy_epoch as any client.
const RuleVTable &registered_rule(const char *name) {
  const int id = tactix_rule_id(name);
  assert(id >= 0);
  return tactix_rules()[static_cast<size_t>(id)];
}

ApplyResult default_propagate(Kernel &k, const char *candidate,
                              PolicyCompletion *completion = nullptr) {
  return port::complete_action(completion, FeedbackKind::Rule, "Propagate",
      INSTANCE_CANONICAL, [&] {
        return execution::native_rule_feedback(k, "Propagate", INSTANCE_CANONICAL,
            candidate, [](const Kernel &kernel) { return kernel.stats.propagations; },
            "propagations");
      }, [&] {
    const size_t trail_begin = k.trail.size();
    const uint64_t root_assignments = k.stats.root_assignments;
    const ApplyResult result = default_direct_rule(
        k, "Propagate", candidate, [&] { return port::propagate_apply(k); },
        ApplyResult::Ok);
    // Kissat marks every true-level-zero assignment fixed in its assignment
    // primitive. Keep the proof rule independent of policy state, but consume
    // the same lifecycle transition before DefaultPolicy next chooses a rule or
    // decision. This includes root implications discovered above level zero.
    if (k.stats.root_assignments != root_assignments)
      heuristics_consume_root_assignments(k, trail_begin);
    return result;
  });
}

ApplyResult default_affine_root_closure(
    Kernel &kernel, const DefaultStrategyConfig::AffineRootClosure &config,
    PolicyCompletion *completion) {
  const auto selected = default_affine_root_parameters(kernel, config);
  if (!selected || !kernel.affine_matrix_search_session ||
      !kernel.structural_provenance)
    return ApplyResult::Ok;

  port::execute_action(
      kernel, execution::ParameterInstallationRequest{*selected}, completion,
      "AffineMatrixInitialize");
  const uint64_t init_allowance = saturated_sum(
      saturated_sum(selected->maximum_snapshot_work,
                    selected->maximum_elimination_work),
      selected->basis_maximum_work);
  const AffineMatrixInitResult initialized = port::complete_action(
      completion, FeedbackKind::Rule, "AffineMatrixInitialize",
      INSTANCE_CANONICAL,
      [&] {
        return execution::native_rule_feedback(
            kernel, "AffineMatrixInitialize", INSTANCE_CANONICAL,
            "default-root-complete-snapshot", affine_init_work,
            "affine-initialization-work",
            strategy::WorkLimit::bounded(init_allowance));
      },
      [&] {
        return port::affine_matrix_search_initialize(
            kernel, *kernel.affine_matrix_search_session, *selected,
            AffineMatrixAdmission::CompleteSnapshot);
      },
      affine_init_outcome);
  if (initialized.status != AffineMatrixInitStatus::Applied)
    return ApplyResult::Ok;

  for (;;) {
    const AffineMatrixServiceResult service = port::complete_action(
        completion, FeedbackKind::SearchService, "AffineMatrixSearch",
        INSTANCE_CANONICAL,
        [&] {
          execution::NativeFeedbackSpec spec;
          spec.meter = affine_service_work;
          spec.description.version = 2;
          spec.description.description =
              "default-root-complete-snapshot-fixpoint";
          spec.description.effort_currency = "affine-matrix-work";
          spec.description.effort_allowance = strategy::WorkLimit::bounded(
              selected->basis_maximum_work);
          spec.description.have_affine_matrix_event = true;
          return spec;
        },
        [&] {
          return port::affine_matrix_search_step(
              kernel, *kernel.affine_matrix_search_session);
        },
        affine_service_outcome);
    if (service.status == AffineMatrixServiceStatus::Applied) {
      if (service.result == ApplyResult::Unsat) return ApplyResult::Unsat;
      if (!service.bcp_pending) std::abort();
      const ApplyResult propagated =
          default_propagate(kernel, "affine-matrix-root", completion);
      if (propagated != ApplyResult::Ok) return propagated;
      continue;
    }
    // This startup schedule deliberately retains no dormant matrix. Future
    // learned policies may initialize the same public action again after an
    // explicit root transition with their own interval and effort choice.
    port::affine_matrix_search_release(*kernel.affine_matrix_search_session);
    return ApplyResult::Ok;
  }
}

template <LuckyOpportunity Opportunity>
ApplyResult default_lucky_opportunity(Kernel &k, const char *candidate,
                                      PolicyCompletion *completion = nullptr) {
  constexpr uint64_t instance = execution::lucky_feedback_instance<Opportunity>();
  const strategy::GuidanceOutcomeSnapshot before(k);
  return port::complete_action(completion, FeedbackKind::Rule, "Lucky", instance,
      [&] {
        auto spec = execution::native_rule_feedback(k, "Lucky", instance, candidate);
        if constexpr (Opportunity == LuckyOpportunity::GraphLabeling) {
          spec.meter = [](const Kernel &kernel) { return kernel.stats.graph_labeling_work; };
          spec.description.effort_currency = "graph-work";
          uint64_t allowance = 0;
          for (uint64_t dose : k.graph_labeling_parameters.work) allowance += dose;
          if (kernel_has_work_budget(k)) allowance = std::min(allowance, k.work_budget);
          spec.description.effort_allowance = strategy::WorkLimit::bounded(allowance);
        }
        return spec;
      }, [&] {
    static const RuleVTable &lucky = registered_rule("Lucky");
    // Proof instances first close the root; DefaultPolicy offers them only at
    // a propagated root, where that closure is a no-op.
    if constexpr (Opportunity == LuckyOpportunity::Stconn ||
                  Opportunity == LuckyOpportunity::SignedCardinality ||
                  Opportunity == LuckyOpportunity::ParityProof ||
                  Opportunity == LuckyOpportunity::ExactOneProof)
      assert(kernel_propagation_fixpoint(k));
    return port::apply_rule_instance(lucky, k, instance);
  }, [&](ApplyResult result) {
    auto out = strategy::controller::feedback_outcome(result);
    out.invocation.status = before.refine(k, result, out.invocation.status);
    return out;
  });
}

ApplyResult default_lucky(Kernel &k, const char *candidate,
                          PolicyCompletion *completion = nullptr) {
  return port::complete_action(completion, FeedbackKind::Rule, "Lucky",
      INSTANCE_CANONICAL, [&] {
        return execution::native_rule_feedback(k, "Lucky", INSTANCE_CANONICAL, candidate);
      }, [&] {
    const size_t trail_begin = k.trail.size();
    const uint64_t roots_before = k.stats.root_assignments;
    static const RuleVTable &lucky = registered_rule("Lucky");
    const ApplyResult result = port::apply_rule_instance(lucky, k, INSTANCE_CANONICAL);
    // Lucky can prove failed-literal units even when every model probe fails.
    // Its root entry prefix survives speculative backtracking, so consume the
    // appended roots in assignment order before any later policy consumer.
    // Kissat deactivates these identities in assignment; raw Lucky stays
    // independent of guidance here. The deferred early call still has no H:
    // its subsequent attachment reconstructs the same fixed-variable image.
    if (k.stats.root_assignments != roots_before)
      heuristics_consume_root_assignments(k, trail_begin);
    return result;
  });
}
} // namespace

ApplyResult default_resume_sparse_propagation(Kernel &k,
                                              uint32_t trail_begin,
                                              bool charge_search_ticks) {
  // Kissat clears its old root trail on phase entry, then resets propagation
  // to the beginning of the suffix produced by the simplifier after sparse
  // watches are rebuilt. Tactix retains immutable root identities, so the
  // equivalent cursor is the saved suffix boundary, not literal trail zero.
  return port::execute_action(
      k, execution::SparseResumeRequest{trail_begin, charge_search_ticks}, [&] {
        return default_propagate(k, charge_search_ticks ? "eliminate-sparse-resume"
                                                       : "probe-sparse-resume");
      });
}

void default_prepare_reduction(Kernel &k) {
  port::execute_action(k, execution::ReductionPreparationRequest{},
      [&](size_t begin) { heuristics_consume_root_assignments(k, begin); });
}

bool default_should_cleanup_unique_literal_amo(
    uint64_t binaries_added, bool later_progress,
    bool generic_graph_root_progress) {
  return binaries_added && later_progress && !generic_graph_root_progress;
}

bool default_should_seed_structured_residual(
    bool grew_root_trail, uint64_t unique_literal_amo_binaries,
    bool generic_graph_root_progress) {
  return grew_root_trail &&
         (!unique_literal_amo_binaries || generic_graph_root_progress);
}

bool default_should_run_initial_fast_elimination(uint64_t candidates,
                                                 uint64_t active) {
  return candidates && candidates > active - std::min(candidates, active);
}

bool default_fast_elimination_discovery_admissible(uint64_t active,
                                                   uint64_t remaining) {
  // Discovery has to inspect every live variable before the owner order is
  // known. Reserve an equal amount of the shared dose for the transformation;
  // otherwise startup would pay mostly to construct a domain that can only be
  // mutated by a short, incomplete prefix.
  return active && active <= remaining / 2;
}

bool default_should_suppress_fast_elimination_after_structured(
    bool structured_root_grew_trail, uint64_t extension_candidates) {
  // A root change alone is not enough to prove that FastEl competes with the
  // structured representation: large clausal formulas can gain incidental
  // roots yet have no eliminable structured helper.  Preserve the Satsuma to
  // Kissat ownership boundary only when both effects meet in the same startup
  // candidate domain.  Clause-only SR and root growth without an eligible
  // helper still compose with bounded FastEl.
  return structured_root_grew_trail && extension_candidates;
}

namespace {
// Unit conversions and protocol values, not strategy parameters.
constexpr uint64_t PERMILLE_SCALE = 1000;
static uint64_t scale_permille(uint64_t reference, uint32_t permille);

// Count solver-created variables that the startup FastEl occurrence screen
// would admit, without entering dense mode or applying its local forward
// subsumption.  This preflight is used only after structured reasoning grew
// the root trail.  It deliberately scans the current sparse formula once:
// probing FastEl by running its ordinary gather would already choose victims
// and mutate the trajectory that this ownership boundary is meant to protect.
// A caller-authored structured step may leave newly assigned roots pending;
// that path waits for ordinary Eliminate root preparation and relies on the
// composed-cohort application fallback instead of classifying a stale image.
uint64_t initial_fast_elimination_extension_candidates(
    const Kernel &k, const strategy::BveParameters &parameters,
    const DefaultInprocessScheduler::StructuredFastelOwnership &ownership) {
  struct ExtensionCounts {
    uint64_t positive = 0;
    uint64_t negative = 0;
    bool oversized = false;
  };

  uint64_t cohort_extent = ownership.external_end > ownership.external_begin
                               ? ownership.external_end - ownership.external_begin
                               : 0;
  for (const auto &range : ownership.coupled_factor_ranges)
    cohort_extent = saturated_sum(
        cohort_extent, range.end > range.begin ? range.end - range.begin : 0);
  if (!cohort_extent)
    return 0;
  std::vector<uint32_t> extension_index(k.nvars, UINT32_MAX);
  std::vector<ExtensionCounts> extensions;
  extensions.reserve(static_cast<size_t>(
      std::min<uint64_t>(cohort_extent, k.nvars)));
  for (Var variable = 0; variable < k.nvars; ++variable) {
    const Lit positive = make_lit(variable, false);
    const uint32_t external = k.internal_to_external[variable];
    if (!kernel_is_solver_extension(k, variable) ||
        !ownership.composed_owns(external) || k.value(positive) ||
        kernel_var_retired(k, variable) || variable >= k.bve_retry.size() ||
        !k.bve_retry[variable])
      continue;
    extension_index[variable] = static_cast<uint32_t>(extensions.size());
    extensions.emplace_back();
  }
  if (extensions.empty())
    return 0;

  // Dense FastEl contains only irredundant large clauses.  Its occurrence
  // flush rejects an oversized clause before testing whether that clause is
  // root-satisfied, so preserve that slightly unusual ordering here.
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    const uint32_t words = clause_actual_words(clause);
    if (!clause->garbage() && !clause->redundant()) {
      const bool oversized = clause->size > parameters.clause_size_limit;
      bool satisfied = false;
      if (!oversized)
        for (const Lit literal : *clause)
          satisfied |= k.value(literal) > 0;
      if (oversized || !satisfied) {
        for (const Lit literal : *clause) {
          const Var variable = var_of(literal);
          if (variable >= extension_index.size())
            continue;
          const uint32_t index = extension_index[variable];
          if (index == UINT32_MAX)
            continue;
          ExtensionCounts &counts = extensions[index];
          if (oversized)
            counts.oversized = true;
          else if (negated(literal))
            ++counts.negative;
          else
            ++counts.positive;
        }
      }
    }
    ref += words;
  }

  // Binary occurrences are already represented authoritatively by the sparse
  // watch lists.  The dense entry that FastEl would take keeps the first copy
  // of a repeated binary and deletes later copies, so count distinct partners
  // without performing that deletion.  Match the subsequent flush filters as
  // well: a satisfied binary and a partner whose variable has left the active
  // formula do not count.
  std::vector<Lit> binary_partners;
  for (Var variable = 0; variable < k.nvars; ++variable) {
    const uint32_t index = extension_index[variable];
    if (index == UINT32_MAX)
      continue;
    ExtensionCounts &counts = extensions[index];
    for (uint32_t sign = 0; sign < 2; ++sign) {
      uint64_t &count = sign ? counts.negative : counts.positive;
      const Lit literal = make_lit(variable, sign != 0);
      binary_partners.clear();
      for (const WatchEntry watch : k.watches.entries(literal)) {
        if (!watch.binary() || k.value(watch.other()) > 0 ||
            kernel_var_retired(k, var_of(watch.other())))
          continue;
        binary_partners.push_back(watch.other());
      }
      std::sort(binary_partners.begin(), binary_partners.end());
      count += static_cast<uint64_t>(
          std::unique(binary_partners.begin(), binary_partners.end()) -
          binary_partners.begin());
    }
  }

  uint64_t eligible = 0;
  for (const ExtensionCounts &counts : extensions) {
    // Round-one gathering preserves FastEl's shipped twice-positive bug before
    // the later application rechecks both polarities.  A protected helper has
    // to pass both boundaries; otherwise FastEl would not have offered it.
    if (counts.oversized ||
        counts.positive > parameters.occurrence_limit / 2 ||
        counts.positive > parameters.occurrence_limit ||
        counts.negative > parameters.occurrence_limit)
      continue;
    const uint64_t total = saturated_sum(counts.positive, counts.negative);
    eligible += total <= parameters.occurrence_limit;
  }
  return eligible;
}

// Kissat's SET_EFFORT_LIMIT converts the per-mille option to a scale factor
// and only truncates after multiplying by the complete reference.  Splitting
// the quotient and remainder keeps that floor exact without overflowing the
// uint64_t product for the supported 0..1000 per-mille range.
static uint64_t scale_permille(uint64_t reference, uint32_t permille) {
  assert(permille <= PERMILLE_SCALE);
  return reference / PERMILLE_SCALE * permille +
         reference % PERMILLE_SCALE * permille / PERMILLE_SCALE;
}
constexpr uint64_t PERCENT_SCALE = 100;
constexpr uint32_t MODES_PER_CYCLE = 2;
constexpr uint32_t REORDER_STABLE_ONLY = 2;

// The posed tier boundaries are ReduceParameters' tier1/tier2, which Reduce
// ranking, Vivify's tier bands and the likely-kept material filter read. A
// pose is a permanent install of that profile with the selected modes' tiers.
void pose_tiers(Kernel &k, const RetierSchedule &tiers, uint8_t tier1_mode,
                uint8_t tier2_mode, execution::PolicyCompletion *completion) {
  assert(tier1_mode < MODES_PER_CYCLE && tier2_mode < MODES_PER_CYCLE);
  strategy::ReduceParameters posed = strategy::ReduceParameters::from_kernel(k);
  posed.tier1 = tiers.tier1_by_mode[tier1_mode];
  posed.tier2 = std::max(posed.tier1, tiers.tier2_by_mode[tier2_mode]);
  port::execute_action(k, execution::ParameterInstallationRequest{posed},
                            completion, "Reduce");
}

void pose_reduce_tiers(Kernel &k, const RetierSchedule &tiers,
                       const DefaultStrategyConfig::Reduction &config,
                       execution::PolicyCompletion *completion) {
  // Kissat 4.0.4's shipped internal.h deliberately uses focused tier-1 and
  // stable tier-2 for Reduce (the general focusedtiers selector is compiled
  // out). Keep the unusual hybrid explicit: it is the first-reduction witness
  // on P4A tr-n300-s055, not an inferred simplification.
  pose_tiers(k, tiers, config.reduce_tier1_mode, config.reduce_tier2_mode,
             completion);
}

void pose_vivify_tiers(Kernel &k, const RetierSchedule &tiers,
                       const DefaultStrategyConfig::Inprocess &config,
                       execution::PolicyCompletion *completion) {
  pose_tiers(k, tiers, config.vivify_tier_mode, config.vivify_tier_mode,
             completion);
}

void retier_current_mode(Kernel &k, RetierSchedule &tiers) {
  reduce_retier(k, tiers, k.h && k.h->stable ? 1u : 0u);
}
} // namespace

strategy::OnlineStrategyProgram
DefaultStrategyConfig::reference_structured_root_program() {
  strategy::OnlineInvocation pure;
  pure.id = 1000;
  pure.rule = "BVE";
  strategy::BveParameters pure_parameters;
  // The owner closes pure literals inside its simplification (satsuma.h).
  pure_parameters.candidate_scope = strategy::BveCandidateScope::PureClosure;
  pure.parameters = pure_parameters;
  pure.granularity = strategy::InvocationGranularity::CompleteCurrentPass;
  pure.work_unit = strategy::WorkUnit::BveTicks;
  pure.post_application = strategy::PostApplication::Propagate;

  strategy::OnlineInvocation orbitopal;
  orbitopal.id = 1001;
  orbitopal.rule = "SubstitutionRedundancy";
  SubstitutionRedundancyParameters orbitopal_parameters;
  orbitopal_parameters.maximum_validation_ticks = 10'000'000;
  orbitopal_parameters.producer =
      SubstitutionRedundancyProducer::Orbitopal;
  // The two-dimensional refinement producer needs both axes so it can select
  // the ULC-bearing dimension after exact row and column checks. One-axis
  // matrices continue to emit only their independently certified action.
  orbitopal_parameters.orbitopal_orientation = OrbitopalOrientation::Both;
  orbitopal_parameters.minimum_orbitopal_rows = 2;
  orbitopal_parameters.minimum_orbitopal_columns = 2;
  // A complete orbitopal pass is the semantic scheduling unit.  A finite
  // matrix prefix can miss a later productive matrix and, because truncation
  // correctly stops the startup program, can suppress every following
  // producer.  Keep caller-selected finite prefixes available through the
  // typed interface, but let the reference program enumerate the full pass.
  orbitopal_parameters.maximum_orbitopal_matrices = UINT64_MAX;
  orbitopal_parameters.maximum_orbitopal_instances = UINT64_MAX;
  orbitopal_parameters.minimum_orbitopal_clique_columns = 5;
  orbitopal_parameters.maximum_orbitopal_clique_vertices = 100'000;
  orbitopal_parameters.maximum_orbitopal_clique_ticks = 10'000'000;
  orbitopal_parameters.maximum_orbitopal_product_states = 100'000;
  orbitopal_parameters.complete_unique_literal_amos = true;
  // Orbitopal refinement owns structured row actions. Generic generator
  // fixing is the following GraphSymmetry sub-strategy; repeating it inside
  // every inexact refinement orbit duplicates formula-wide group work and can
  // delay an already available exact structured action.
  orbitopal_parameters.orbitopal_generator_fallback = false;
  // Bound repeated pointwise-stabilizer refinement in the reference startup
  // schedule. Callers retain zero, finite, and unbounded choices through the
  // typed parameter surface.
  orbitopal_parameters.maximum_orbitopal_refinement_orbits = 64;
  orbitopal_parameters.orbitopal_progress_feedback = true;
  orbitopal_parameters.maximum_detection_ticks = UINT64_MAX;
  orbitopal_parameters.orbitopal_column_order =
      OrbitopalColumnOrder::CooccurrenceGreedyClique;
  orbitopal_parameters.orbitopal_completion =
      OrbitopalCompletion::IndividualizedGraphCompletion;
  // Consume formula-derived structured columns before asking the generic
  // equitable partition for candidate row orbits. The scoped-fixpoint repeat
  // resumes the generic phase after the cheaper producer stops making
  // progress.
  orbitopal_parameters.orbitopal_discovery_order =
      OrbitopalDiscoveryOrder::UniqueLiteralThenRefinement;
  orbitopal_parameters.graph_symmetry.maximum_graph_vertices =
      SYMMETRY_GRAPH_COMPONENT_LIMIT_DEFAULT;
  orbitopal_parameters.graph_symmetry.maximum_graph_edges =
      SYMMETRY_GRAPH_EDGE_LIMIT_DEFAULT;
  orbitopal_parameters.graph_symmetry.maximum_generators =
      SYMMETRY_GENERATOR_LIMIT_DEFAULT;
  orbitopal_parameters.graph_symmetry.backend_search_budget =
      SYMMETRY_BACKTRACK_LIMIT_DEFAULT;
  orbitopal_parameters.graph_symmetry.backend_support_budget =
      SYMMETRY_SUPPORT_LIMIT_DEFAULT;
  orbitopal_parameters.graph_symmetry.backend_component_size_limit =
      SYMMETRY_GRAPH_COMPONENT_LIMIT_DEFAULT;
  orbitopal.parameters = orbitopal_parameters;
  orbitopal.granularity =
      strategy::InvocationGranularity::CompleteCurrentPass;
  // ULC completion deliberately emits only its binary phase in the current
  // pass. Re-enumerate this producer against that strengthened formula before
  // a later sub-strategy can stop the outer sequence.
  orbitopal.repeat = {strategy::RepeatMode::UntilScopedFixpoint, 0};
  orbitopal.post_application = strategy::PostApplication::Propagate;

  strategy::OnlineInvocation graph;
  graph.id = 1002;
  graph.rule = "SubstitutionRedundancy";
  SubstitutionRedundancyParameters graph_parameters;
  graph_parameters.maximum_validation_ticks =
      SYMMETRY_TOTAL_TICK_LIMIT_DEFAULT;
  graph_parameters.maximum_detection_ticks =
      SYMMETRY_TOTAL_TICK_LIMIT_DEFAULT;
  graph_parameters.producer =
      SubstitutionRedundancyProducer::GraphSymmetry;
  graph_parameters.graph_symmetry.maximum_graph_vertices =
      SYMMETRY_GRAPH_COMPONENT_LIMIT_DEFAULT;
  graph_parameters.graph_symmetry.maximum_graph_edges =
      SYMMETRY_GRAPH_EDGE_LIMIT_DEFAULT;
  graph_parameters.graph_symmetry.maximum_generators =
      SYMMETRY_GENERATOR_LIMIT_DEFAULT;
  graph_parameters.graph_symmetry.backend_search_budget =
      SYMMETRY_BACKTRACK_LIMIT_DEFAULT;
  graph_parameters.graph_symmetry.backend_support_budget =
      SYMMETRY_SUPPORT_LIMIT_DEFAULT;
  graph_parameters.graph_symmetry.backend_component_size_limit =
      SYMMETRY_GRAPH_COMPONENT_LIMIT_DEFAULT;
  graph_parameters.graph_symmetry.maximum_fixing_instances =
      SYMMETRY_FIXING_LIMIT_DEFAULT;
  // One direct generator changes the stabilizer used by the next pass.  Fresh
  // detection is therefore the complete semantic unit, matching the
  // Satsuma-derived scheduling experiment rather than retaining a stale
  // generator prefix.
  graph_parameters.graph_symmetry.maximum_generator_instances_per_pass = 1;
  graph_parameters.graph_symmetry.maximum_group_ticks =
      SYMMETRY_TOTAL_TICK_LIMIT_DEFAULT;
  graph_parameters.graph_symmetry.clause_scope = SymmetryClauseScope::All;
  graph_parameters.graph_symmetry.binary_graph_mode =
      SymmetryBinaryGraphMode::Automatic;
  graph_parameters.graph_symmetry.binary_graph_density_permille =
      SYMMETRY_BINARY_DENSITY_PERMILLE_DEFAULT;
  graph_parameters.graph_symmetry.clause_selection =
      SymmetryClauseSelection::AmoThenShortest;
  graph_parameters.graph_symmetry.chain_mode = SymmetryChainMode::FixAndCut;
  graph_parameters.graph_symmetry.base_selection =
      SymmetryBaseSelection::CommonClauseAmo;
  // Satsuma fixes negation orbits before fix-and-cut, fixes a clause only
  // through its first stored literal, and cuts at the largest stabilizer
  // orbit's highest variable (symmetries.h negation_fixing_schreier,
  // orbit_overlaps_clause, find_non_trivial_orbit).
  graph_parameters.graph_symmetry.cut_point = SymmetryCutPoint::LargestOrbitLast;
  graph_parameters.graph_symmetry.negation_fixing_first = true;
  graph_parameters.graph_symmetry.first_literal_fix_anchor = true;
  graph.parameters = graph_parameters;
  graph.granularity = strategy::InvocationGranularity::CompleteCurrentPass;
  graph.post_application = strategy::PostApplication::Propagate;

  strategy::OnlineStrategyProgram result;
  result.sequence =
      {std::move(pure), std::move(orbitopal), std::move(graph)};
  // Repeat to the program's scoped no-progress boundary. Root assignments
  // alone are not a termination measure: ULC can add useful binaries without
  // fixing a variable. An incomplete budget stop is not a scoped fixpoint.
  // satsuma.h iteration_max with its iteration_limit of 128.
  result.repeat = {strategy::RepeatMode::UntilSizeScaledFixpoint, 128};
  return result;
}

bool DefaultStrategyConfig::valid(std::string *error) const {
  const auto reject = [error](const char *message) {
    if (error)
      *error = message;
    return false;
  };
  const auto positive_finite = [](double value) {
    return std::isfinite(value) && value > 0.0;
  };
  if (!branching.focused_phase_switch_span ||
      branching.focused_phase_pattern.empty())
    return reject("branching phase schedule must be nonempty with a positive span");
  for (const int8_t phase : branching.focused_phase_pattern)
    if (phase < -1 || phase > 1)
      return reject("branching phase pattern contains an invalid phase");
  if (!positive_finite(branching.logarithm_offset))
    return reject("branching logarithm offset must be positive and finite");
  if (!std::isfinite(analysis.bump_reason_rate) ||
      analysis.bump_reason_rate < 0.0)
    return reject("analysis reason-bump rate must be finite and nonnegative");
  if (analysis.eager_subsume >
      strategy::AnalyzeParameters::EAGER_SUBSUME_CAPACITY)
    return reject("analysis eager-subsumption size exceeds its representation");
  if (analysis.big_binary_fraction_permille > PERMILLE_SCALE)
    return reject("analysis binary fraction exceeds 1000 permille");
  if (!positive_finite(restart.glue_margin) ||
      !positive_finite(restart.fast_ema_window) ||
      !positive_finite(restart.slow_ema_window) ||
      !positive_finite(restart.logarithm_offset) ||
      !positive_finite(restart.logarithm_bias) || !restart.initial_interval ||
      !restart.reluctant_period || !restart.reluctant_limit)
    return reject("restart settings contain a zero or non-finite scale");
  if (mode.force > 2 || !mode.initial_conflicts || !mode.interval ||
      !positive_finite(mode.logarithm_offset) || !mode.logarithm_power)
    return reject("mode schedule is invalid");
  if (!reduction.initial_interval || !reduction.arithmetic_interval ||
      !reduction.arithmetic_step ||
      !positive_finite(reduction.logarithm_offset) ||
      !std::isfinite(reduction.low_percent) ||
      reduction.low_percent < 0.0 ||
      reduction.low_percent > static_cast<double>(PERCENT_SCALE) ||
      !std::isfinite(reduction.high_percent) ||
      reduction.high_percent < 0.0 ||
      reduction.high_percent > static_cast<double>(PERCENT_SCALE) ||
      reduction.compact_inactive_percent > PERCENT_SCALE ||
      reduction.tier1_default > reduction.tier2_default ||
      reduction.tier1_relative_permille >
          reduction.tier2_relative_permille ||
      reduction.tier2_relative_permille > PERMILLE_SCALE ||
      reduction.reduce_tier1_mode >= MODES_PER_CYCLE ||
      reduction.reduce_tier2_mode >= MODES_PER_CYCLE ||
      !reduction.victim_denominator ||
      reduction.victim_numerator > reduction.victim_denominator ||
      !reduction.retier_initial_interval || !reduction.retier_max_interval ||
      reduction.retier_growth < 1)
    return reject("reduction schedule is invalid");
  if (!rephase.initial_interval || !rephase.interval ||
      !positive_finite(rephase.logarithm_offset) || !rephase.logarithm_power ||
      rephase.walk_permille > PERMILLE_SCALE)
    return reject("rephase schedule is invalid");
  strategy::WalkParameters walk_parameters;
  walk_parameters.even_break_base_milli =
      rephase.walk_even_break_base_milli;
  walk_parameters.score_curve = rephase.walk_score_curve;
  if (!walk_parameters.valid() || rephase.rotation.empty())
    return reject("rephase Walk settings are invalid");
  for (const DefaultRephaseSlot slot : rephase.rotation)
    if (slot != DefaultRephaseSlot::Best &&
        slot != DefaultRephaseSlot::Walk &&
        slot != DefaultRephaseSlot::Inverted &&
        slot != DefaultRephaseSlot::Original)
      return reject("rephase rotation contains an invalid slot");
  if (rephase.walk_inapplicable_fallback == DefaultRephaseSlot::Walk ||
      (rephase.walk_inapplicable_fallback != DefaultRephaseSlot::Best &&
       rephase.walk_inapplicable_fallback != DefaultRephaseSlot::Inverted &&
       rephase.walk_inapplicable_fallback != DefaultRephaseSlot::Original))
    return reject("rephase Walk fallback is invalid");
  if (!reorder.initial_interval || !reorder.interval || reorder.mode > 2 ||
      (reorder.target != DefaultReorderTarget::Active &&
       reorder.target != DefaultReorderTarget::Queue &&
       reorder.target != DefaultReorderTarget::Heap) ||
      !reorder.parameters.valid())
    return reject("reorder schedule is invalid");
  if (!parity_model.parameters.valid())
    return reject("parity-model settings are invalid");
  if (!onehot_model.parameters.valid())
    return reject("onehot-model settings are invalid");
  if (!balanced_signature_model.parameters.valid())
    return reject("balanced-signature-model settings are invalid");
  if (!fermat_model.parameters.valid())
    return reject("fermat-model settings are invalid");
  if (!lookup_circuit_model.parameters.valid())
    return reject("lookup-circuit-model settings are invalid");
  if (!simon_model.parameters.valid())
    return reject("simon-model settings are invalid");
  if (!stconn_proof.parameters.valid())
    return reject("stconn-proof settings are invalid");
  if (!signed_cardinality_proof.parameters.valid())
    return reject("signed-cardinality-proof settings are invalid");
  if (!exact_cover_model.parameters.valid())
    return reject("exact-cover-model settings are invalid");
  if (!ramsey_startup.parameters.valid())
    return reject("ramsey-startup settings are invalid");
  if (!affine_root_closure.maximum_clauses ||
      !affine_root_closure.maximum_snapshot_work ||
      !affine_root_closure.maximum_recognition_work ||
      !affine_root_closure.maximum_basis_work ||
      affine_root_closure.maximum_row_width < 3 ||
      !affine_root_closure.maximum_proof_additions ||
      !affine_root_closure.maximum_proof_deletions ||
      !affine_root_closure.maximum_journal_bytes ||
      !affine_root_closure.maximum_session_work)
    return reject("affine-root-closure settings are invalid");
  if (!affine_search.maximum_rows || !affine_search.maximum_row_width ||
      affine_search.maximum_arity < 2 || affine_search.maximum_arity > 8 ||
      affine_search.maximum_row_width < affine_search.maximum_arity ||
      !std::isfinite(affine_search.minimum_usefulness) ||
      affine_search.minimum_usefulness < 0 || affine_search.minimum_usefulness > 1 ||
      !affine_search.autodisable_minimum_calls || !affine_search.autodisable_check_every)
    return reject("affine-search settings are invalid");
  if (!graph_labeling_model.parameters.valid())
    return reject("graph-labeling settings are invalid");
  if (!independent_set_model.parameters.valid())
    return reject("independent-set-model settings are invalid");

  const auto &ip = inprocess;
  if (!ip.eliminate_interval || !ip.probe_interval || !ip.phase_rounds ||
      !ip.initial_fast_elimination_rounds ||
      !ip.substitute_rounds || !ip.substitute_search_rounds ||
      !positive_finite(ip.logarithm_offset) ||
      !std::isfinite(ip.scale_quadratic) || ip.scale_quadratic < 0.0 ||
      !std::isfinite(ip.scale_constant) || ip.scale_constant < 0.0 ||
      !std::isfinite(ip.scale_log_shift) ||
      ip.vivify_tier_mode >= MODES_PER_CYCLE)
    return reject("inprocessing cadence or scaling is invalid");
  if (!ip.congruence_parameters.valid() ||
      !ip.instantiate_parameters.valid() || !ip.backbone_parameters.valid() ||
      !ip.vivify_parameters.valid() || !ip.bce_parameters.valid() ||
      !ip.ternary_parameters.valid() || !ip.factor_parameters.valid() ||
      !ip.transitive_parameters.valid() || !ip.substitute_parameters.valid() ||
      !ip.sweep_parameters.valid())
    return reject("inprocessing rule parameters are invalid");
  strategy::BveParameters bve;
  bve.occurrence_limit = ip.bve_occurrence_limit;
  bve.clause_size_limit = ip.bve_clause_size_limit;
  bve.xor_arity_limit = ip.bve_xor_arity_limit;
  bve.resolvent_size_limit = ip.bve_resolvent_size_limit;
  bve.backward_occurrence_limit = ip.bve_backward_occurrence_limit;
  bve.definition_budget = ip.bve_definition_budget;
  bve.definition_cores = ip.bve_definition_cores;
  bve.definition_core_budget_multiplier =
      ip.bve_definition_core_budget_multiplier;
  bve.on_the_fly_strengthening = ip.bve_on_the_fly_strengthening;
  if (!bve.valid())
    return reject("inprocessing BVE parameters are invalid");
  if (!ip.initial_fast_elimination_parameters.valid())
    return reject("initial fast-elimination parameters are invalid");
  if (ip.bve_bounds.empty() ||
      ip.bve_bounds.front() > ip.bve_default_ceiling)
    return reject("BVE bound ladder is empty or starts above its ceiling");
  for (size_t i = 1; i < ip.bve_bounds.size(); ++i)
    if (ip.bve_bounds[i - 1] > ip.bve_bounds[i])
      return reject("BVE bound ladder is not monotone");
  if (ip.factor_decade_base < 2 ||
      uint64_t{ip.factor_decade_round_cap} + ip.factor_decade_offset >
          ip.factor_decade_cap)
    return reject("Factor delay schedule is invalid");
  uint64_t threshold = 1;
  for (uint32_t i = 0; i < ip.factor_decade_cap; ++i) {
    if (threshold > UINT64_MAX / ip.factor_decade_base)
      return reject("Factor delay threshold overflows uint64_t");
    threshold *= ip.factor_decade_base;
  }
  const uint32_t permilles[] = {
      ip.bve_permille, ip.initial_fast_elimination_bve_permille,
      ip.probe_permille,
      ip.hyper_binary_permille, ip.backbone_permille,
      ip.transitive_permille,   ip.vivify_permille,
      ip.strengthen_permille,   ip.subsume_forward_permille,
      ip.factor_permille,       ip.hyper_ternary_permille,
      ip.bce_permille,          ip.cce_permille,
      ip.instantiate_permille,  ip.sweep_permille,
      ip.substitute_permille,   ip.congruence_productivity_permille,
      ip.sweep_productivity_permille};
  for (const uint32_t value : permilles)
    if (value > PERMILLE_SCALE)
      return reject("inprocessing permille setting exceeds 1000");
  const uint64_t tier_share = uint64_t{ip.vivify_tier_shares[0]} +
                              ip.vivify_tier_shares[1] +
                              ip.vivify_tier_shares[2];
  if (!ip.vivify_total_share || !ip.vivify_redundant_share ||
      ip.vivify_redundant_share > ip.vivify_total_share ||
      tier_share > ip.vivify_redundant_share)
    return reject("Vivify share partition is invalid");
  for (const uint8_t scope : ip.vivify_tier_scopes)
    if (scope > 5)
      return reject("Vivify tier scope is invalid");
  if (ip.vivify_irredundant_productivity_percent > PERCENT_SCALE)
    return reject("Vivify productivity exceeds 100 percent");
  if (!strategy::validate_inprocess_program(ip.program, error))
    return false;
  for (const strategy::InprocessStep &step : ip.program)
    if (step.rule == "SubstitutionRedundancy")
      return reject("slot program duplicates structured root SR ownership");
  for (const auto *stages : {&ip.structured_root_stages,
                             &ip.structured_root_bounded_backtracking_stages,
                             &ip.structured_root_large_domain_stages})
  for (const auto &stage : *stages) {
    if (stage.on_budget_exhaustion != strategy::BudgetExhaustionPolicy::Stop &&
        stage.on_budget_exhaustion != strategy::BudgetExhaustionPolicy::Continue &&
        stage.on_budget_exhaustion != strategy::BudgetExhaustionPolicy::FinishRoundThenStop)
      return reject("structured root budget disposition is invalid");
    if (!strategy::validate_online_strategy_program(stage.program, error))
      return false;
    if (stage.structured_handoff_after_prior_progress &&
        std::none_of(stage.program.sequence.begin(), stage.program.sequence.end(),
                     [](const auto &step) {
                       const auto *parameters =
                           std::get_if<SubstitutionRedundancyParameters>(
                               &step.parameters);
                       return parameters &&
                              parameters->producer ==
                                  SubstitutionRedundancyProducer::GraphSymmetry &&
                              (parameters->graph_symmetry.use_structured_generators ||
                               parameters->graph_symmetry
                                   .stabilize_structured_variables);
                     }))
      return reject("structured handoff feedback requires a graph handoff");
    for (size_t i = 0; i < stage.progress_followups.size(); ++i) {
      const auto id = stage.progress_followups[i];
      if (std::none_of(stage.program.sequence.begin(), stage.program.sequence.end(),
              [id](const auto &step) { return step.id == id; }) ||
          std::find(stage.progress_followups.begin(), stage.progress_followups.begin() + i,
                    id) != stage.progress_followups.begin() + i)
        return reject("progress followup must name a unique existing step");
    }
    for (size_t i = 0; i < stage.before_progress.size(); ++i) {
      const auto id = stage.before_progress[i];
      if (std::none_of(stage.program.sequence.begin(), stage.program.sequence.end(),
              [id](const auto &step) { return step.id == id; }) ||
          std::find(stage.before_progress.begin(), stage.before_progress.begin() + i,
                    id) != stage.before_progress.begin() + i ||
          std::find(stage.progress_followups.begin(), stage.progress_followups.end(),
                    id) != stage.progress_followups.end())
        return reject("before-progress step must name a unique existing non-followup");
    }
    // This scheduler owns a quiescent-root preprocessing scope. All declared
    // inprocessing actions, Reduce, Propagate and Lucky fit that boundary
    // (Lucky may terminate with a model); other search
    // transitions remain freely composable through the general online API.
    for (const auto &step : stage.program.sequence) {
      const int id = tactix_rule_id(step.rule.c_str());
      if (id < 0 || (tactix_rules()[id].tier != RuleTier::Inprocess &&
                    step.rule != "Reduce" && step.rule != "Propagate" &&
                    step.rule != "Lucky"))
        return reject("structured root action does not preserve its root scope");
    }
  }
  for (const strategy::InprocessStep &step : ip.program)
    if (step.effort_permille && *step.effort_permille > PERMILLE_SCALE)
      return reject("inprocess step effort exceeds 1000 permille");
  if (error)
    error->clear();
  return true;
}

std::string DefaultStrategyConfig::serialize() const {
  std::ostringstream out;
  out << "strategy-v1"
      << " branching.random_initial_interval="
      << branching.random_initial_interval
      << " branching.random_sequence_length="
      << branching.random_sequence_length
      << " branching.random_seed=" << branching.random_seed
      << " branching.logarithm_offset=" << branching.logarithm_offset
      << " branching.focused_phase_switch_span="
      << branching.focused_phase_switch_span
      << " branching.random_sequences=" << branching.random_sequences;
  for (size_t i = 0; i < branching.focused_phase_pattern.size(); ++i)
    out << " branching.focused_phase" << i << '='
        << static_cast<int>(branching.focused_phase_pattern[i]);
  out << " analysis.minimize_depth_limit=" << analysis.minimize_depth_limit
      << " analysis.bump_reason_limit=" << analysis.bump_reason_limit
      << " analysis.bump_reason_rate=" << analysis.bump_reason_rate
      << " analysis.reason_depth=" << analysis.reason_depth
      << " analysis.chronological_levels=" << analysis.chronological_levels
      << " analysis.conflict_reuse=" << analysis.conflict_reuse
      << " analysis.jump_reasons=" << analysis.jump_reasons
      << " analysis.shrink_glue_promote=" << analysis.shrink_glue_promote
      << " analysis.bump_extension_reasons="
      << analysis.bump_extension_reasons
      << " analysis.eager_subsume=" << analysis.eager_subsume
      << " analysis.small_clause_limit=" << analysis.small_clause_limit
      << " analysis.big_binary_fraction_permille="
      << analysis.big_binary_fraction_permille
      << " restart.glue_margin=" << restart.glue_margin
      << " restart.initial_interval=" << restart.initial_interval
      << " restart.fast_ema_window=" << restart.fast_ema_window
      << " restart.slow_ema_window=" << restart.slow_ema_window
      << " restart.logarithm_offset=" << restart.logarithm_offset
      << " restart.logarithm_bias=" << restart.logarithm_bias
      << " restart.reluctant_period=" << restart.reluctant_period
      << " restart.reluctant_limit=" << restart.reluctant_limit
      << " mode.initial_conflicts=" << mode.initial_conflicts
      << " mode.interval=" << mode.interval
      << " mode.logarithm_offset=" << mode.logarithm_offset
      << " mode.logarithm_power=" << mode.logarithm_power
      << " mode.force=" << mode.force
      << " reduction.initial_interval=" << reduction.initial_interval
      << " reduction.arithmetic_interval=" << reduction.arithmetic_interval
      << " reduction.arithmetic_step=" << reduction.arithmetic_step
      << " reduction.low_percent=" << reduction.low_percent
      << " reduction.high_percent=" << reduction.high_percent
      << " reduction.logarithm_offset=" << reduction.logarithm_offset
      << " reduction.compact_inactive_percent="
      << reduction.compact_inactive_percent
      << " reduction.retier_initial_interval="
      << reduction.retier_initial_interval
      << " reduction.retier_max_interval=" << reduction.retier_max_interval
      << " reduction.retier_growth=" << reduction.retier_growth
      << " reduction.tier1_default=" << reduction.tier1_default
      << " reduction.tier2_default=" << reduction.tier2_default
      << " reduction.tier1_relative_permille="
      << reduction.tier1_relative_permille
      << " reduction.tier2_relative_permille="
      << reduction.tier2_relative_permille
      << " reduction.reduce_tier1_mode="
      << unsigned(reduction.reduce_tier1_mode)
      << " reduction.reduce_tier2_mode="
      << unsigned(reduction.reduce_tier2_mode)
      << " reduction.victim_numerator=" << reduction.victim_numerator
      << " reduction.victim_denominator=" << reduction.victim_denominator
      << " reduction.protect_tiers=" << reduction.protect_tiers
      << " reduction.canonical_compact_collection="
      << reduction.canonical_compact_collection
      << " reduction.bookkeeping=" << reduction.bookkeeping
      << " reduction.square_root_cadence="
      << reduction.square_root_cadence
      << " reduction.ramp_fraction=" << reduction.ramp_fraction
      << " reduction.dynamic_tiers=" << reduction.dynamic_tiers
      << " reduction.compact_variables=" << reduction.compact_variables
      << " rephase.initial_interval=" << rephase.initial_interval
      << " rephase.interval=" << rephase.interval
      << " rephase.logarithm_offset=" << rephase.logarithm_offset
      << " rephase.logarithm_power=" << rephase.logarithm_power
      << " rephase.walk_permille=" << rephase.walk_permille
      << " rephase.walk_even_break_base_milli="
      << rephase.walk_even_break_base_milli
      << " rephase.walk_inapplicable_fallback="
      << static_cast<unsigned>(rephase.walk_inapplicable_fallback)
      << " rephase.walk_warmup=" << rephase.walk_warmup
      << " rephase.walk_use_branch_seed="
      << rephase.walk_use_branch_seed
      << " rephase.walk_random_seed=" << rephase.walk_random_seed
      << " rephase.stable_only=" << rephase.stable_only
      << " rephase.late_lucky=" << rephase.late_lucky;
  for (size_t i = 0; i < rephase.walk_score_curve.size(); ++i)
    out << " rephase.walk_score_point" << i << '='
        << rephase.walk_score_curve[i].average_clause_size_milli << ':'
        << rephase.walk_score_curve[i].break_base_milli;
  for (size_t i = 0; i < rephase.rotation.size(); ++i)
    out << " rephase.rotation" << i << '='
        << static_cast<unsigned>(rephase.rotation[i]);
  out
      << " reorder.initial_interval=" << reorder.initial_interval
      << " reorder.interval=" << reorder.interval
      << " reorder.mode=" << reorder.mode
      << " reorder.target=" << static_cast<unsigned>(reorder.target)
      << " reorder.maximum_clause_size="
      << reorder.parameters.maximum_clause_size
      << " reorder.clause_decay_milli="
      << reorder.parameters.clause_decay_milli
      << " reorder.binary_weight_milli="
      << reorder.parameters.binary_weight_milli
      << " reorder.balanced_polarity_milli="
      << reorder.parameters.balanced_polarity_milli
      << " parity_model.enabled=" << parity_model.enabled
      << " parity_model.maximum_variables="
      << parity_model.parameters.maximum_variables
      << " parity_model.maximum_groups="
      << parity_model.parameters.maximum_groups
      << " parity_model.maximum_constraints="
      << parity_model.parameters.maximum_constraints
      << " parity_model.minimum_group_clauses="
      << parity_model.parameters.minimum_group_clauses
      << " parity_model.minimum_equations="
      << parity_model.parameters.minimum_equations
      << " parity_model.maximum_free_variables="
      << parity_model.parameters.maximum_free_variables
      << " parity_model.candidate_budget="
      << parity_model.parameters.candidate_budget
      << " parity_model.refutation_maximum_variables="
      << parity_model.parameters.refutation_maximum_variables
      << " parity_model.refutation_maximum_groups="
      << parity_model.parameters.refutation_maximum_groups
      << " parity_model.refutation_maximum_constraints="
      << parity_model.parameters.refutation_maximum_constraints
      << " parity_model.refutation_maximum_arity="
      << parity_model.parameters.refutation_maximum_arity
      << " parity_model.refutation_probe_constraints="
      << parity_model.parameters.refutation_probe_constraints
      << " parity_model.refutation_minimum_probe_repeats="
      << parity_model.parameters.refutation_minimum_probe_repeats
      << " parity_model.refutation_maximum_proof_additions="
      << parity_model.parameters.refutation_maximum_proof_additions
      << " onehot_model.enabled=" << onehot_model.enabled
      << " onehot_model.maximum_variables="
      << onehot_model.parameters.maximum_variables
      << " onehot_model.maximum_domains="
      << onehot_model.parameters.maximum_domains
      << " onehot_model.maximum_constraints="
      << onehot_model.parameters.maximum_constraints
      << " onehot_model.maximum_equation_arity="
      << onehot_model.parameters.maximum_equation_arity
      << " onehot_model.minimum_domains="
      << onehot_model.parameters.minimum_domains
      << " onehot_model.minimum_equations="
      << onehot_model.parameters.minimum_equations
      << " onehot_model.maximum_free_domains="
      << onehot_model.parameters.maximum_free_domains
      << " onehot_model.candidate_budget="
      << onehot_model.parameters.candidate_budget
      << " affine_search.enabled=" << affine_search.enabled
      << " affine_search.maximum_rows=" << affine_search.maximum_rows
      << " affine_search.maximum_row_width=" << affine_search.maximum_row_width
      << " affine_search.maximum_arity=" << affine_search.maximum_arity
      << " affine_search.autodisable=" << affine_search.autodisable
      << " affine_search.minimum_usefulness=" << affine_search.minimum_usefulness
      << " affine_search.autodisable_minimum_calls=" << affine_search.autodisable_minimum_calls
      << " affine_search.autodisable_check_every=" << affine_search.autodisable_check_every
      << " affine_search.maximum_initialization_work=" << affine_search.maximum_initialization_work
      << " affine_search.maximum_service_work=" << affine_search.maximum_service_work
      << " affine_search.maximum_retained_bytes=" << affine_search.maximum_retained_bytes
      << " affine_model.enabled=" << affine_model.enabled;
  {
    strategy::OnlineStrategyProgram affine;
    strategy::OnlineInvocation invocation;
    invocation.id = 1;
    invocation.rule = "Lucky";
    invocation.parameters = affine_model.parameters;
    invocation.granularity = strategy::InvocationGranularity::OneInstance;
    invocation.instances = {8};
    affine.sequence.push_back(std::move(invocation));
    out << " affine_model.parameters="
        << strategy::serialize_online_strategy_program(affine);
  }
  out
      << " balanced_signature_model.enabled="
      << balanced_signature_model.enabled
      << " balanced_signature_model.maximum_balls="
      << balanced_signature_model.parameters.maximum_balls
      << " balanced_signature_model.maximum_width="
      << balanced_signature_model.parameters.maximum_width
      << " balanced_signature_model.restarts="
      << balanced_signature_model.parameters.restarts
      << " balanced_signature_model.steps_per_restart="
      << balanced_signature_model.parameters.steps_per_restart
      << " balanced_signature_model.samples_per_step="
      << balanced_signature_model.parameters.samples_per_step
      << " balanced_signature_model.seed="
      << balanced_signature_model.parameters.seed
      << " balanced_signature_model.completion_rounds="
      << balanced_signature_model.parameters.completion_rounds
      << " fermat_model.enabled=" << fermat_model.enabled
      << " fermat_model.maximum_bits="
      << fermat_model.parameters.maximum_bits
      << " fermat_model.comparator_tail_clauses="
      << fermat_model.parameters.comparator_tail_clauses
      << " fermat_model.rho_constants="
      << fermat_model.parameters.rho_constants
      << " fermat_model.rho_iterations="
      << fermat_model.parameters.rho_iterations
      << " cerny_model.enabled=" << cerny_model.enabled
      << " lookup_circuit_model.enabled=" << lookup_circuit_model.enabled
      << " lookup_circuit_model.maximum_fanin="
      << lookup_circuit_model.parameters.maximum_fanin
      << " lookup_circuit_model.maximum_fanout="
      << lookup_circuit_model.parameters.maximum_fanout
      << " lookup_circuit_model.maximum_primary_inputs="
      << lookup_circuit_model.parameters.maximum_primary_inputs
      << " lookup_circuit_model.maximum_support="
      << lookup_circuit_model.parameters.maximum_support
      << " lookup_circuit_model.maximum_vector_gate_evaluations="
      << lookup_circuit_model.parameters.maximum_vector_gate_evaluations
      << " simon_model.enabled=" << simon_model.enabled
      << " simon_model.maximum_rounds="
      << simon_model.parameters.maximum_rounds
      << " simon_model.maximum_candidates="
      << simon_model.parameters.maximum_candidates
      << " stconn_proof.enabled=" << stconn_proof.enabled
      << " stconn_proof.maximum_width="
      << stconn_proof.parameters.maximum_width
      << " stconn_proof.maximum_columns="
      << stconn_proof.parameters.maximum_columns
      << " stconn_proof.maximum_rup_clauses="
      << stconn_proof.parameters.maximum_rup_clauses
      << " signed_cardinality_proof.enabled="
      << signed_cardinality_proof.enabled
      << " signed_cardinality_proof.maximum_variables="
      << signed_cardinality_proof.parameters.maximum_variables
      << " signed_cardinality_proof.maximum_groups="
      << signed_cardinality_proof.parameters.maximum_groups
      << " signed_cardinality_proof.maximum_constraints="
      << signed_cardinality_proof.parameters.maximum_constraints
      << " signed_cardinality_proof.maximum_group_arity="
      << signed_cardinality_proof.parameters.maximum_group_arity
      << " signed_cardinality_proof.maximum_bdd_nodes="
      << signed_cardinality_proof.parameters.maximum_bdd_nodes
      << " signed_cardinality_proof.maximum_proof_additions="
      << signed_cardinality_proof.parameters.maximum_proof_additions
      << " exact_cover_model.enabled=" << exact_cover_model.enabled
      << " exact_cover_model.block_arity="
      << exact_cover_model.parameters.block_arity
      << " exact_cover_model.maximum_nodes="
      << exact_cover_model.parameters.maximum_nodes
      << " exact_cover_model.restarts=" << exact_cover_model.parameters.restarts
      << " exact_cover_model.restart_nodes="
      << exact_cover_model.parameters.restart_nodes
      << " exact_cover_model.seed=" << exact_cover_model.parameters.seed
      << " independent_set_model.enabled=" << independent_set_model.enabled
      << " independent_set_model.maximum_vertices="
      << independent_set_model.parameters.maximum_vertices
      << " independent_set_model.maximum_steps="
      << independent_set_model.parameters.maximum_steps
      << " independent_set_model.completion_rounds="
      << independent_set_model.parameters.completion_rounds
      << " independent_set_model.seed=" << independent_set_model.parameters.seed;
  out << " ramsey_startup.enabled=" << ramsey_startup.enabled;
  {
    strategy::OnlineStrategyProgram program;
    strategy::OnlineInvocation invocation;
    invocation.id = 1; invocation.rule = "Lucky";
    invocation.parameters = ramsey_startup.parameters;
    invocation.granularity = strategy::InvocationGranularity::OneInstance;
    invocation.instances = {LUCKY_STARTUP_RAMSEY};
    program.sequence.push_back(std::move(invocation));
    out << " ramsey_startup.parameters=" << strategy::serialize_online_strategy_program(program);
  }
  out << " affine_root_closure.enabled=" << affine_root_closure.enabled
      << " affine_root_closure.maximum_clauses="
      << affine_root_closure.maximum_clauses
      << " affine_root_closure.maximum_snapshot_work="
      << affine_root_closure.maximum_snapshot_work
      << " affine_root_closure.maximum_recognition_work="
      << affine_root_closure.maximum_recognition_work
      << " affine_root_closure.maximum_basis_work="
      << affine_root_closure.maximum_basis_work
      << " affine_root_closure.maximum_row_width="
      << affine_root_closure.maximum_row_width
      << " affine_root_closure.maximum_proof_additions="
      << affine_root_closure.maximum_proof_additions
      << " affine_root_closure.maximum_proof_deletions="
      << affine_root_closure.maximum_proof_deletions
      << " affine_root_closure.maximum_journal_bytes="
      << affine_root_closure.maximum_journal_bytes
      << " affine_root_closure.maximum_session_work="
      << affine_root_closure.maximum_session_work;
  out << " graph_labeling_model.enabled=" << graph_labeling_model.enabled;
  {
    strategy::OnlineStrategyProgram graph;
    strategy::OnlineInvocation invocation;
    invocation.id = 1;
    invocation.rule = "Lucky";
    invocation.parameters = graph_labeling_model.parameters;
    invocation.granularity = strategy::InvocationGranularity::OneInstance;
    invocation.instances = {LUCKY_GRAPH_LABELING};
    graph.sequence.push_back(std::move(invocation));
    out << " graph_labeling_model.parameters="
        << strategy::serialize_online_strategy_program(graph);
  }
  out
      << " inprocess.bve_enabled=" << inprocess.bve_enabled
      << " inprocess.vivify_enabled=" << inprocess.vivify_enabled
      << " inprocess.factor_enabled=" << inprocess.factor_enabled
      << " inprocess.sweep_enabled=" << inprocess.sweep_enabled
      << " inprocess.factor_repeated_enabled="
      << inprocess.factor_repeated_enabled
      << " inprocess.preprocess_only=" << inprocess.preprocess_only
      << " inprocess.initial_fast_elimination="
      << inprocess.initial_fast_elimination
      << " inprocess.initial_fast_elimination_rounds="
      << inprocess.initial_fast_elimination_rounds
      << " inprocess.initial_fast_elimination_resolvent_count_limit="
      << inprocess.initial_fast_elimination_parameters.resolvent_count_limit
      << " inprocess.initial_fast_elimination_traversal="
      << static_cast<unsigned>(
             inprocess.initial_fast_elimination_parameters.traversal)
      << " inprocess.cadical_arena_ordering_enabled="
      << inprocess.cadical_arena_ordering_enabled
      << " inprocess.append_unlisted_rules="
      << inprocess.append_unlisted_rules
      << " inprocess.eliminate_interval=" << inprocess.eliminate_interval
      << " inprocess.probe_interval=" << inprocess.probe_interval
      << " inprocess.minimum_effort_ticks="
      << inprocess.minimum_effort_ticks
      << " inprocess.preprocess_initial_ticks="
      << inprocess.preprocess_initial_ticks
      << " inprocess.phase_rounds=" << inprocess.phase_rounds
      << " inprocess.substitute_rounds=" << inprocess.substitute_rounds
      << " inprocess.substitute_search_rounds="
      << inprocess.substitute_search_rounds
      << " inprocess.factor_initial_ticks=" << inprocess.factor_initial_ticks
      << " inprocess.logarithm_offset=" << inprocess.logarithm_offset
      << " inprocess.scale_quadratic=" << inprocess.scale_quadratic
      << " inprocess.scale_constant=" << inprocess.scale_constant
      << " inprocess.scale_log_shift=" << inprocess.scale_log_shift
      << " inprocess.candidate_order_seed="
      << inprocess.candidate_order_seed
      << " inprocess.congruence_and_arity_limit="
      << inprocess.congruence_parameters.and_arity_limit
      << " inprocess.congruence_gate_count_limit="
      << inprocess.congruence_parameters.gate_count_limit
      << " inprocess.congruence_xor_arity_limit="
      << inprocess.congruence_parameters.xor_arity_limit
      << " inprocess.congruence_binary_watch_order="
      << static_cast<unsigned>(inprocess.congruence_parameters.binary_watch_order)
      << " inprocess.instantiate_minimum_clause_size="
      << inprocess.instantiate_parameters.minimum_clause_size
      << " inprocess.instantiate_occurrence_limit="
      << inprocess.instantiate_parameters.occurrence_limit
      << " inprocess.instantiate_minimum_unassigned="
      << inprocess.instantiate_parameters.minimum_unassigned
      << " inprocess.instantiate_include_redundant="
      << inprocess.instantiate_parameters.include_redundant
      << " inprocess.instantiate_new_material_threshold="
      << inprocess.instantiate_parameters.new_material_threshold
      << " inprocess.backbone_initial_rounds="
      << inprocess.backbone_parameters.initial_rounds
      << " inprocess.backbone_maximum_rounds="
      << inprocess.backbone_parameters.maximum_rounds
      << " inprocess.vivify_scope="
      << unsigned(inprocess.vivify_parameters.scope)
      << " inprocess.vivify_reuse=" << inprocess.vivify_parameters.reuse
      << " inprocess.vivify_flush=" << inprocess.vivify_parameters.flush
      << " inprocess.vivify_irredundant_sort_ratio="
      << inprocess.vivify_parameters.irredundant_sort_ratio
      << " inprocess.bce_occurrence_limit="
      << inprocess.bce_parameters.occurrence_limit
      << " inprocess.ternary_occurrence_limit="
      << inprocess.ternary_parameters.occurrence_limit
      << " inprocess.ternary_maximum_add_percent="
      << inprocess.ternary_parameters.maximum_add_percent
      << " inprocess.factor_maximum_clause_size="
      << inprocess.factor_parameters.maximum_clause_size
      << " inprocess.factor_growth_bound="
      << inprocess.factor_parameters.growth_bound
      << " inprocess.factor_candidate_rounds="
      << inprocess.factor_parameters.candidate_rounds
      << " inprocess.factor_composite="
      << inprocess.factor_parameters.composite
      << " inprocess.factor_demote_fresh="
      << inprocess.factor_parameters.demote_fresh
      << " inprocess.transitive_early_exit="
      << inprocess.transitive_parameters.early_exit
      << " inprocess.transitive_live_meter="
      << inprocess.transitive_parameters.live_meter
      << " inprocess.transitive_checked_budget_meter="
      << inprocess.transitive_parameters.checked_budget_meter
      << " inprocess.substitute_demote="
      << inprocess.substitute_parameters.demote_substituted
      << " inprocess.touch_filter=" << inprocess.touch_filter
      << " inprocess.likely_kept_redundant="
      << inprocess.likely_kept_redundant
      << " inprocess.structured_root_before_lucky="
      << inprocess.structured_root_before_lucky
      << " inprocess.structured_root_symmetry_enabled="
      << inprocess.structured_root_symmetry_enabled
      << " inprocess.structured_root_residual_search_handoff="
      << inprocess.structured_root_residual_search_handoff
      << " inprocess.structured_root_sr_discovery_unlimited="
      << inprocess.structured_root_sr_discovery_budget.is_unlimited()
      << " inprocess.structured_root_sr_discovery_ticks="
      << (inprocess.structured_root_sr_discovery_budget.is_unlimited()
              ? 0
              : inprocess.structured_root_sr_discovery_budget.amount())
      << " inprocess.structured_root_progress_feedback="
      << inprocess.structured_root_progress_feedback
      << " inprocess.structured_root_large_domain_progress_feedback="
      << inprocess.structured_root_large_domain_progress_feedback
      << " inprocess.structured_root_progress_minimum_ticks="
      << inprocess.structured_root_progress_minimum_ticks
      << " inprocess.structured_root_progress_ticks_per_variable="
      << inprocess.structured_root_progress_ticks_per_variable
      << " inprocess.structured_root_bce_after_unique_literal_amo="
      << inprocess.structured_root_bce_after_unique_literal_amo
      << " inprocess.structured_root_backtracking_domain_limit="
      << inprocess.structured_root_backtracking_domain_limit
      << " inprocess.structured_root_large_domain_limit="
      << inprocess.structured_root_large_domain_limit
;
  for (size_t i = 0; i < inprocess.structured_root_stages.size(); ++i) {
    const auto &stage = inprocess.structured_root_stages[i];
    out << " inprocess.structured_root_stage" << i << "={"
        << static_cast<unsigned>(stage.on_budget_exhaustion) << ':'
        << strategy::serialize_online_strategy_program(stage.program) << '}';
    if (!stage.progress_followups.empty()) {
      out << " inprocess.structured_root_stage" << i << ".progress_followups=";
      for (size_t j = 0; j < stage.progress_followups.size(); ++j)
        out << (j ? "," : "") << stage.progress_followups[j];
    }
    if (!stage.before_progress.empty()) {
      out << " inprocess.structured_root_stage" << i << ".before_progress=";
      for (size_t j = 0; j < stage.before_progress.size(); ++j)
        out << (j ? "," : "") << stage.before_progress[j];
    }
    out << " inprocess.structured_root_stage" << i
        << ".structured_handoff_after_prior_progress="
        << stage.structured_handoff_after_prior_progress;
  }
  for (size_t i = 0;
       i < inprocess.structured_root_bounded_backtracking_stages.size(); ++i) {
    const auto &stage =
        inprocess.structured_root_bounded_backtracking_stages[i];
    out << " inprocess.structured_root_bounded_backtracking_stage" << i << "={"
        << static_cast<unsigned>(stage.on_budget_exhaustion) << ':'
        << strategy::serialize_online_strategy_program(stage.program) << '}';
    if (!stage.progress_followups.empty()) {
      out << " inprocess.structured_root_bounded_backtracking_stage" << i
          << ".progress_followups=";
      for (size_t j = 0; j < stage.progress_followups.size(); ++j)
        out << (j ? "," : "") << stage.progress_followups[j];
    }
    if (!stage.before_progress.empty()) {
      out << " inprocess.structured_root_bounded_backtracking_stage" << i
          << ".before_progress=";
      for (size_t j = 0; j < stage.before_progress.size(); ++j)
        out << (j ? "," : "") << stage.before_progress[j];
    }
    out << " inprocess.structured_root_bounded_backtracking_stage" << i
        << ".structured_handoff_after_prior_progress="
        << stage.structured_handoff_after_prior_progress;
  }
  for (size_t i = 0; i < inprocess.structured_root_large_domain_stages.size();
       ++i) {
    const auto &stage = inprocess.structured_root_large_domain_stages[i];
    out << " inprocess.structured_root_large_stage" << i << "={"
        << static_cast<unsigned>(stage.on_budget_exhaustion) << ':'
        << strategy::serialize_online_strategy_program(stage.program) << '}';
    if (!stage.progress_followups.empty()) {
      out << " inprocess.structured_root_large_stage" << i
          << ".progress_followups=";
      for (size_t j = 0; j < stage.progress_followups.size(); ++j)
        out << (j ? "," : "") << stage.progress_followups[j];
    }
    if (!stage.before_progress.empty()) {
      out << " inprocess.structured_root_large_stage" << i
          << ".before_progress=";
      for (size_t j = 0; j < stage.before_progress.size(); ++j)
        out << (j ? "," : "") << stage.before_progress[j];
    }
    out << " inprocess.structured_root_large_stage" << i
        << ".structured_handoff_after_prior_progress="
        << stage.structured_handoff_after_prior_progress;
  }
  for (size_t i = 0; i < inprocess.bve_bounds.size(); ++i)
    out << " inprocess.bve_bound" << i << '=' << inprocess.bve_bounds[i];
  out << " inprocess.bve_default_ceiling=" << inprocess.bve_default_ceiling
      << " inprocess.bve_occurrence_limit="
      << inprocess.bve_occurrence_limit
      << " inprocess.bve_clause_size_limit="
      << inprocess.bve_clause_size_limit
      << " inprocess.bve_xor_arity_limit="
      << inprocess.bve_xor_arity_limit
      << " inprocess.bve_resolvent_size_limit="
      << inprocess.bve_resolvent_size_limit
      << " inprocess.bve_backward_occurrence_limit="
      << inprocess.bve_backward_occurrence_limit
      << " inprocess.bve_definition_budget="
      << inprocess.bve_definition_budget
      << " inprocess.bve_definition_cores="
      << inprocess.bve_definition_cores
      << " inprocess.bve_definition_core_budget_multiplier="
      << inprocess.bve_definition_core_budget_multiplier
      << " inprocess.bve_backward=" << inprocess.bve_backward
      << " inprocess.bve_on_the_fly_strengthening="
      << inprocess.bve_on_the_fly_strengthening
      << " inprocess.factor_decade_round_cap="
      << inprocess.factor_decade_round_cap
      << " inprocess.factor_decade_cap=" << inprocess.factor_decade_cap
      << " inprocess.factor_decade_offset=" << inprocess.factor_decade_offset
      << " inprocess.factor_decade_base=" << inprocess.factor_decade_base
      << " inprocess.factor_active_ceiling=" << inprocess.factor_active_ceiling
      << " inprocess.bve_permille=" << inprocess.bve_permille
      << " inprocess.initial_fast_elimination_bve_permille="
      << inprocess.initial_fast_elimination_bve_permille
      << " inprocess.probe_permille=" << inprocess.probe_permille
      << " inprocess.hyper_binary_permille="
      << inprocess.hyper_binary_permille
      << " inprocess.backbone_permille=" << inprocess.backbone_permille
      << " inprocess.transitive_permille=" << inprocess.transitive_permille
      << " inprocess.vivify_permille=" << inprocess.vivify_permille
      << " inprocess.strengthen_permille=" << inprocess.strengthen_permille
      << " inprocess.subsume_forward_permille="
      << inprocess.subsume_forward_permille
      << " inprocess.factor_permille=" << inprocess.factor_permille
      << " inprocess.hyper_ternary_permille="
      << inprocess.hyper_ternary_permille
      << " inprocess.bce_permille=" << inprocess.bce_permille
      << " inprocess.cce_permille=" << inprocess.cce_permille
      << " inprocess.instantiate_permille=" << inprocess.instantiate_permille
      << " inprocess.sweep_permille=" << inprocess.sweep_permille
      << " inprocess.substitute_permille=" << inprocess.substitute_permille
      << " inprocess.sweep_depth=" << inprocess.sweep_parameters.max_depth
      << " inprocess.sweep_max_depth="
      << inprocess.sweep_parameters.max_depth_ceiling
      << " inprocess.sweep_vars=" << inprocess.sweep_parameters.max_env_vars
      << " inprocess.sweep_max_vars="
      << inprocess.sweep_parameters.max_env_vars_ceiling
      << " inprocess.sweep_clauses="
      << inprocess.sweep_parameters.max_env_clauses
      << " inprocess.sweep_max_clauses="
      << inprocess.sweep_parameters.max_env_clauses_ceiling
      << " inprocess.sweep_candidate_pairs="
      << inprocess.sweep_parameters.max_candidates
      << " inprocess.sweep_flip_rounds="
      << inprocess.sweep_parameters.flip_rounds
      << " inprocess.sweep_seed_cap=" << inprocess.sweep_parameters.max_seeds
      << " inprocess.sweep_environment_ticks="
      << inprocess.sweep_parameters.env_ticks
      << " inprocess.sweep_round_ticks="
      << inprocess.sweep_parameters.round_ticks
      << " inprocess.sweep_proof_steps="
      << inprocess.sweep_parameters.max_proof_steps
      << " inprocess.sweep_strip_root_false="
      << inprocess.sweep_parameters.strip_false
      << " inprocess.sweep_measure_closure="
      << inprocess.sweep_parameters.measure_closure
      << " inprocess.sweep_full_partition="
      << inprocess.sweep_parameters.full_partition
      << " inprocess.sweep_skip_embedded_fixed="
      << inprocess.sweep_parameters.skip_embedded_fixed
      << " inprocess.sweep_resume=" << inprocess.sweep_parameters.resume
      << " inprocess.sweep_force_general_proof="
      << inprocess.sweep_parameters.force_general_proof
      << " inprocess.congruence_productivity_permille="
      << inprocess.congruence_productivity_permille
      << " inprocess.sweep_productivity_permille="
      << inprocess.sweep_productivity_permille
      << " inprocess.vivify_irredundant_productivity_percent="
      << inprocess.vivify_irredundant_productivity_percent
      << " inprocess.vivify_tier_mode="
      << unsigned(inprocess.vivify_tier_mode)
      << " inprocess.vivify_redundant_share="
      << inprocess.vivify_redundant_share
      << " inprocess.vivify_total_share=" << inprocess.vivify_total_share;
  for (size_t i = 0; i < inprocess.vivify_tier_shares.size(); ++i)
    out << " inprocess.vivify_tier_share" << i << '='
        << inprocess.vivify_tier_shares[i]
        << " inprocess.vivify_tier_scope" << i << '='
        << unsigned(inprocess.vivify_tier_scopes[i]);
  out << " inprocess.closure_delay=" << inprocess.closure_delay
      << " inprocess.split_clocks=" << inprocess.split_clocks
      << " inprocess.watermarks=" << inprocess.watermarks
      << " inprocess.preprocess_followups="
      << inprocess.preprocess_followups
      << " inprocess.hold_subsume_until_eliminate="
      << inprocess.hold_subsume_until_eliminate
      << " inprocess.forward_midround_budget="
      << inprocess.forward_midround_budget
      << " inprocess.couple_factor_bound="
      << inprocess.couple_factor_bound;
  for (size_t i = 0; i < inprocess.program.size(); ++i) {
    const strategy::InprocessStep &step = inprocess.program[i];
    out << " inprocess.step" << i << '=' << step.id << ':' << step.rule << ':'
        << (step.phase == strategy::InprocessPhase::Probe ? 'P' : 'E') << ':'
        << step.enabled << ':' << step.preprocessing << ':'
        << static_cast<unsigned>(step.followup) << ":effort=";
    if (step.effort_permille)
      out << *step.effort_permille;
    else
      out << "inherit";
    out << ":max=" << step.max_applications << ":cursor=";
    if (step.cursor_enabled)
      out << *step.cursor_enabled;
    else
      out << "inherit";
    out << ":order=";
    if (step.candidate_order)
      out << candidate_order_name(*step.candidate_order);
    else
      out << "inherit";
    if (const auto *p =
            std::get_if<strategy::BveParameters>(&step.parameters)) {
      out << ":bve(" << p->gate_mask << ',' << p->growth_bound << ','
          << p->occurrence_limit << ',' << p->clause_size_limit << ','
          << p->xor_arity_limit << ',' << p->resolvent_size_limit << ','
          << p->backward_occurrence_limit << ',' << p->definition_budget << ','
          << p->definition_cores << ','
          << p->definition_core_budget_multiplier << ',' << p->backward << ','
          << static_cast<unsigned>(p->candidate_scope) << ','
          << p->resolvent_count_limit << ','
          << static_cast<unsigned>(p->traversal) << ','
          << p->on_the_fly_strengthening << ')';
    } else if (const auto *p =
                   std::get_if<strategy::SubsumeForwardParameters>(
                       &step.parameters)) {
      out << ":c1(" << p->clause_size_limit << ','
          << p->large_occurrence_limit << ','
          << p->binary_occurrence_limit << ')';
    } else if (const auto *p =
                   std::get_if<strategy::StrengthenParameters>(
                       &step.parameters)) {
      out << ":c2(" << p->clause_size_limit << ','
          << p->occurrence_limit << ')';
    } else if (const auto *p =
                   std::get_if<strategy::ParityConsequenceParameters>(&step.parameters)) {
      out << ":parity-consequence(" << p->maximum_clauses << ','
          << p->maximum_groups << ',' << p->selection_work << ','
          << p->intermediate_width << ',' << p->source_count << ','
          << p->output_width << ',' << p->descending << ','
          << p->maximum_proof_additions << ',' << p->maximum_tree_variables
          << ',' << p->consequence << ')';
    } else if (const auto *p =
                   std::get_if<strategy::CongruenceParameters>(
                       &step.parameters)) {
      out << ":congruence(" << p->and_arity_limit << ','
          << p->gate_count_limit << ',' << p->xor_arity_limit << ','
          << static_cast<unsigned>(p->binary_watch_order) << ')';
    } else if (const auto *p =
                   std::get_if<strategy::InstantiateParameters>(
                       &step.parameters)) {
      out << ":instantiate(" << p->minimum_clause_size << ','
          << p->occurrence_limit << ',' << p->minimum_unassigned << ','
          << p->include_redundant << ',' << p->new_material_threshold << ')';
    } else if (const auto *p =
                   std::get_if<strategy::BackboneParameters>(
                       &step.parameters)) {
      out << ":backbone(" << p->initial_rounds << ','
          << p->maximum_rounds << ')';
    } else if (const auto *p = std::get_if<SweepLimits>(&step.parameters)) {
      out << ":sweep(" << p->max_depth << ',' << p->max_depth_ceiling << ','
          << p->max_env_vars << ',' << p->max_env_vars_ceiling << ','
          << p->max_env_clauses << ',' << p->max_env_clauses_ceiling << ','
          << p->max_candidates << ',' << p->env_ticks << ','
          << p->max_proof_steps << ',' << p->max_seeds << ','
          << p->round_ticks << ',' << p->strip_false << ','
          << p->measure_closure << ',' << p->full_partition << ','
          << p->skip_embedded_fixed << ',' << p->flip_rounds << ','
          << p->resume << ',' << p->force_general_proof << ')';
    } else if (const auto *p =
                   std::get_if<strategy::VivifyParameters>(&step.parameters)) {
      out << ":vivify(" << unsigned(p->scope) << ',' << p->reuse << ','
          << p->flush << ',' << p->irredundant_sort_ratio << ')';
    } else {
      out << ":inherit";
    }
  }
  return out.str();
}

DefaultPolicy::DefaultPolicy()
    : chrono_levels(DefaultStrategyConfig::CHRONOLOGICAL_LEVELS_DEFAULT),
      base_config_(), config_(base_config_),
      default_branching_(&config_.branching),
      default_reduction_(&config_.reduction), inprocess_(&config_.inprocess) {
  pose_affine_structured_stage(base_config_);
  pose_affine_structured_stage(config_);
  // Avoid constructing and then copying a third default program. Besides
  // needless startup allocation, that transient copy can cross a 2-MiB heap
  // arena boundary and become visible in peak-RSS comparisons.
  default_reduction_.ramp = config_.reduction.ramp_fraction;
  retier_.next_conflicts = retier_.interval =
      config_.reduction.retier_initial_interval;
  sync_retier_profile();
  inprocess_.set_retier_schedule(&retier_);
  assert(config_.valid());
}

DefaultPolicy::DefaultPolicy(const DefaultStrategyConfig &config)
    : chrono_levels(config.analysis.chronological_levels),
      base_config_(config), config_(config),
      default_branching_(&config_.branching),
      default_reduction_(&config_.reduction), inprocess_(&config_.inprocess) {
  pose_affine_structured_stage(base_config_);
  pose_affine_structured_stage(config_);
  default_reduction_.ramp = config_.reduction.ramp_fraction;
  retier_.next_conflicts = retier_.interval =
      config_.reduction.retier_initial_interval;
  sync_retier_profile();
  inprocess_.set_retier_schedule(&retier_);
  std::string config_error;
  if (!config_.valid(&config_error)) {
    std::fprintf(stderr, "c ERROR: invalid DefaultStrategyConfig: %s\n",
                 config_error.c_str());
    std::abort();
  }
}

const DefaultStrategyConfig::Inprocess &
DefaultInprocessScheduler::config() const {
  if (config_)
    return *config_;
  static const DefaultStrategyConfig::Inprocess defaults;
  return defaults;
}

uint32_t DefaultBveBoundRamp::num_passes() const {
  const DefaultStrategyConfig::Inprocess *cfg = config_;
  if (!cfg) {
    static const DefaultStrategyConfig::Inprocess defaults;
    cfg = &defaults;
  }
  const uint32_t cap = cfg->bve_default_ceiling;
  uint32_t n = 0;
  for (uint32_t i = 0; i < cfg->bve_bounds.size(); i++)
    if (cfg->bve_bounds[i] <= cap)
      n = i + 1;
  return n ? n : 1;
}

uint32_t DefaultBveBoundRamp::bound(uint32_t pass) const {
  const DefaultStrategyConfig::Inprocess *cfg = config_;
  if (!cfg) {
    static const DefaultStrategyConfig::Inprocess defaults;
    cfg = &defaults;
  }
  const size_t last = cfg->bve_bounds.size() - 1;
  return cfg->bve_bounds[pass < cfg->bve_bounds.size() ? pass : last];
}

void CadicalArenaOrder::order(const Kernel &k, std::vector<Ref> &refs) {
  if (!k.h || k.h->queue_last == INVALID_VAR || refs.empty())
    return;
  std::vector<uint8_t> live(k.arena.size());
  for (const Ref ref : refs)
    live[ref] = 1;
  std::vector<Ref> ordered;
  ordered.reserve(refs.size());
  for (const bool opposite : {true, false})
    for (Var v = k.h->queue_last; v != INVALID_VAR; v = k.h->links[v].prev) {
      Lit lit = make_lit(v, k.h->saved[v] < 0);
      if (opposite)
        lit = negate(lit);
      for (const WatchEntry w : k.watches.entries(lit)) {
        if (w.binary())
          continue;
        const Ref ref = w.ref();
        if (ref < live.size() && live[ref]) {
          ordered.push_back(ref);
          live[ref] = 0;
        }
      }
    }
  for (const Ref ref : refs)
    if (live[ref])
      ordered.push_back(ref);
  refs.swap(ordered);
}

void DefaultPolicy::observe_glue(const Kernel &k, uint32_t glue) {
  DefaultRestartControl::observe_glue(k, config_.restart, glue);
}

void DefaultPolicy::observe_decision_rate(const Kernel &k) {
  DefaultRestartControl::observe_decision_rate(k, config_.restart);
}

uint32_t DefaultPolicy::want_bump_reasons(const Kernel &k) const {
  if (!bump_reasons_limit_)
    return 0;
  if (bump_reasons_delay_.count)
    return 0;
  if (averages[k.h->stable].decision_rate.value >= bump_reasons_rate_)
    return 0;
  return bump_reasons_limit_;
}

void DefaultPolicy::tick_reluctant() {
  DefaultRestartControl::tick_reluctant(config_.restart);
}

bool DefaultPolicy::want_restart(const Kernel &k) const {
  return DefaultRestartControl::want_restart(k, config_.restart);
}

bool DefaultPolicy::want_switch_mode(const Kernel &k) const {
  return DefaultModeControl::wants_switch(k, search_ticks(k));
}

void DefaultPolicy::update_mode_limits(const Kernel &k) {
  if (DefaultModeControl::update_limits(k, config_.mode, search_ticks(k)))
    update_restart_limit(k);
}

void DefaultPolicy::update_restart_limit(const Kernel &k) {
  DefaultRestartControl::update_restart_limit(k, config_.restart);
}

bool DefaultPolicy::want_reduce(const Kernel &k) const {
  return DefaultDatabaseControl::wants_reduce(k);
}

bool DefaultPolicy::want_rephase(const Kernel &k) const {
  return DefaultPhaseControl::wants_rephase(k, config_.rephase);
}

ApplyResult DefaultPolicy::prepare_rephase_root(Kernel &k) {
  // Restart is publicly applicable as a level-zero no-op, but this helper
  // preserves the shipped policy's old root-preparation schedule.
  // Kissat rephase.c backtracks, propagates and flushes the root trail before
  // changing phases. Chronological backtracking can retain out-of-order root
  // assignments that must be rescanned after their transient blockers vanish.
  // Warm-up must not defer those scans until after its first decision.
  constexpr execution::RootPreparationRequest preparation{false, true};
  return port::complete_action(completion_, FeedbackKind::Support, "RootPreparation",
      execution::root_preparation_argument(preparation, true), [] {
        execution::NativeFeedbackSpec spec;
        spec.description.description = "probing=0;clear-unflushed-root=1;restart-purpose=preparation";
        return spec;
      }, [&] { return port::execute_action(k, preparation,
      [&] { heuristics_update_target_phases(k); },
      [&](const execution::RestartRequest &request) {
        return default_direct_rule(k, "Restart", "keep-level=0;rephase=1",
            [&] { return port::execute_action(k, request); });
      }, [&] { return default_propagate(k, "rephase-root"); });
  });
}

bool DefaultPolicy::want_reorder(const Kernel &k) const {
  return DefaultPhaseControl::wants_reorder(k, config_.reorder);
}

void DefaultPolicy::update_reorder_limit(const Kernel &k) {
  DefaultPhaseControl::update_reorder_limit(k, config_.reorder);
}

void DefaultPolicy::update_rephase_limit(const Kernel &k) {
  DefaultPhaseControl::update_rephase_limit(k, config_.rephase);
}

uint64_t DefaultPolicy::walk_effort_reference(const Kernel &k) const {
  // Kissat runs Walk from the search loop with `solver->probing == false`.
  // SET_EFFORT_LIMIT therefore reads `last.ticks.eliminate`, shared with the
  // Eliminate pipeline.  Entering the simplifier timing profile does not flip
  // that semantic flag, and Walk does not own or advance an independent
  // anchor.
  const uint64_t now = search_ticks(k);
  const uint64_t anchor = inprocess_.elim_clock.anchor;
  uint64_t reference = now >= anchor ? now - anchor : 0;
  if (reference < config_.inprocess.minimum_effort_ticks)
    reference = config_.inprocess.minimum_effort_ticks;
  return reference;
}

void DefaultPolicy::reset_schedule(const Kernel &k) {
  const uint64_t conflicts = k.stats.conflicts;
  const uint64_t ticks = search_ticks(k);

  averages[0] = averages[1] = {};
  update_restart_limit(k);
  // Kissat initializes Reduce from `reduceinit` independently of the cadence
  // used after a completed reduction (kimits.c:kissat_init_limits).  The
  // arithmetic interval is a recurrence delta, not an alternative first
  // deadline: using it here postponed LegacySearch's first Reduce from
  // conflict 1000 to 2000.
  reduce.next_conflicts = conflicts + config_.reduction.initial_interval;
  mode = {};
  mode.conflicts_limit = conflicts + config_.mode.initial_conflicts;
  mode.ticks_at_focus_entry = ticks;
  reluctant = {};
  reluctant.wait = config_.restart.reluctant_period;
  rephase = {};
  rephase.next_conflicts = conflicts + config_.rephase.initial_interval;
  reorder = {};
  reorder.next_conflicts = conflicts + config_.reorder.initial_interval;
  bump_reasons_delay_ = {};
  sync_retier_profile();
  retier_.next_conflicts = config_.reduction.retier_initial_interval;
  retier_.interval = config_.reduction.retier_initial_interval;
  if (dynamic_tiers_)
    retier_.next_conflicts = conflicts + retier_.interval;
  default_branching_.reset(conflicts);
  inprocess_.reset(k);
}

static uint64_t active_variables(const Kernel &k) {
  // Kissat's `solver->active` is live state.  Cumulative retirement counters
  // cannot reconstruct it after compaction and also miss Factor's fresh
  // activation, which is precisely what makes `proberounds` enter round two.
  return kernel_active_variables(k);
}

// Formula rules publish events without depending on decision heuristics.
// Consume each Substitute round before a later round or watch reconnection
// derives more roots: native substitute_round retires its representatives at
// that boundary. Call-wide root-first batching changes arbitrary heap removals.
static void default_publish_substitute_round(Kernel &k, size_t trail_begin) {
  heuristics_consume_root_assignments(k, trail_begin);
  heuristics_consume_retirements(k);
}

static uint64_t scale_inprocessing_delta(
    const Kernel &k, uint64_t delta,
    const DefaultStrategyConfig::Inprocess &config) {
  // kimits.c calls `kissat_logn(C + 1)`, while kissat_logn(count) computes
  // log10(count + 9).  Preserve both additions: the scale is therefore based
  // on log10(C + 10), not log10(C + 1).  The distinction crosses an integer
  // deadline at common states (for example 22,575 clauses and probe round 2).
  const double f =
      std::log10(static_cast<double>(kernel_irredundant_clauses(k)) + 1.0 +
                 config.logarithm_offset) -
      config.scale_log_shift;
  return static_cast<uint64_t>((config.scale_quadratic * f * f +
                                config.scale_constant) *
                               static_cast<double>(delta));
}

namespace {

using port::TickSinkScope;
using execution::metered_rule_apply;

using InprocessScope =
    execution::ExecutionScope<execution::ExecutionContext::Inprocessing>;

// Kissat's `search_ticks` is charged only by the ordinary CDCL search
// propagator.  Startup propagation, both Lucky trials, sparse root collection,
// and `probe_initially` all run before search and therefore must not seed mode
// or inprocessing effort references.  This scope suppresses only that tick
// currency; unlike `InprocessScope` it deliberately leaves assignment-time
// phase saving unchanged outside the actual Probe pipeline.
using PreSearchTickScope =
    execution::ExecutionScope<execution::ExecutionContext::Startup>;

// An execution context held across a span of actions. Entry and an explicit
// leave are the public ENTER_CONTEXT / LEAVE_CONTEXT supports; an early exit
// after the feedback session has finished restores the wires unrecorded.
template <execution::ExecutionContext Context> class RecordedExecutionContext {
public:
  RecordedExecutionContext(Kernel &k, PolicyCompletion *completion)
      : kernel_(k), completion_(completion) {
    (void)port::complete_action(completion_, FeedbackKind::Support,
        "EnterExecutionContext", execution::execution_context_argument(Context), [] {
          execution::NativeFeedbackSpec spec;
          spec.description.description = description();
          return spec;
        }, [&] {
          scope_.emplace(kernel_);
          return ApplyResult::Ok;
        });
  }
  void leave() {
    if (!scope_) return;
    (void)port::complete_action(completion_, FeedbackKind::Support,
        "LeaveExecutionContext", execution::execution_context_argument(Context), [] {
          execution::NativeFeedbackSpec spec;
          spec.description.description = description();
          return spec;
        }, [&] {
          scope_.reset();
          return ApplyResult::Ok;
        });
  }
  RecordedExecutionContext(const RecordedExecutionContext &) = delete;
  RecordedExecutionContext &operator=(const RecordedExecutionContext &) = delete;

private:
  static const char *description() {
    return Context == execution::ExecutionContext::Startup ? "context=startup"
           : Context == execution::ExecutionContext::Inprocessing
               ? "context=inprocessing"
               : "context=speculation";
  }
  Kernel &kernel_;
  PolicyCompletion *completion_;
  std::optional<execution::ExecutionScope<Context>> scope_;
};

uint64_t meter_probe(const Kernel &k) { return k.stats.probe_ticks; }
uint64_t meter_backbone(const Kernel &k) { return k.stats.backbone_ticks; }
uint64_t meter_vivify(const Kernel &k) { return k.stats.vivify_ticks; }
uint64_t meter_instantiate(const Kernel &k) { return k.stats.instantiate_ticks; }
uint64_t meter_bve(const Kernel &k) { return k.stats.bve_resolutions; }
uint64_t meter_forward_pair(const Kernel &k) {
  return k.stats.strengthen_visits + k.stats.forward_visits;
}
uint64_t meter_transitive(const Kernel &k) {
  return k.transitive_budget_checked ? k.stats.transitive_checked
                                     : k.stats.transitive_ticks;
}
uint64_t meter_factor(const Kernel &k) { return k.stats.factor_ticks; }
uint64_t meter_ternary(const Kernel &k) { return k.stats.ternary_ticks; }
uint64_t meter_hbr(const Kernel &k) { return k.stats.hbr_ticks; }
uint64_t meter_bce(const Kernel &k) { return k.stats.bce_ticks; }
uint64_t meter_cce(const Kernel &k) { return k.stats.cce_ticks; }
uint64_t meter_sweep(const Kernel &k) { return sweep_effort(k); }
#ifdef TACTIX_ENABLE_RULE_JOURNAL
// Congruence has no shipped effort ceiling in Kissat.  Keep its inclusive
// logical counter available to compile-time rule journals without placing it
// back in EFFORT_SPECS (which would make the meter behavior-affecting).
uint64_t meter_congruence(const Kernel &k) { return k.stats.congruence_ticks; }
#endif
uint64_t meter_substitute(const Kernel &k) { return k.stats.substitute_ticks; }

SweepLimits configured_sweep_limits(
    const DefaultStrategyConfig::Inprocess &config) {
  return config.sweep_parameters;
}

struct EffortSpec {
  const char *name;
  uint32_t DefaultStrategyConfig::Inprocess::*permille;
  uint64_t (*meter)(const Kernel &);
  bool dosed;
  bool budget_aware;
  uint64_t Stats::*tick_counter = nullptr;
  bool closure = false;
};

struct DelaySpec {
  const char *name;
  uint64_t (*yield)(const Kernel &);
  uint64_t (*denom)(const Kernel &);
  uint32_t DefaultStrategyConfig::Inprocess::*permille;
  bool denom_delta;
};
constexpr DelaySpec DELAY_SPECS[] = {
    {"Congruence", [](const Kernel &k) { return k.stats.congruence_merges; },
     active_variables,
     &DefaultStrategyConfig::Inprocess::congruence_productivity_permille,
     false},
    // kissat `sweep.c`: `if (kissat_average (eliminated, swept) < 0.001)
    // BUMP_DELAY (sweep)`, where `swept` counts the scheduled VARIABLES the
    // call swept, not the candidate pairs its environments solved.
    {"Sweep", [](const Kernel &k) {
       return k.stats.sweep_equivalences + k.stats.sweep_units;
     },
     [](const Kernel &k) { return k.stats.sweep_swept; },
     &DefaultStrategyConfig::Inprocess::sweep_productivity_permille, true},
};
static const DelaySpec *delay_spec(const char *name) {
  for (const DelaySpec &d : DELAY_SPECS)
    if (!std::strcmp(d.name, name))
      return &d;
  return nullptr;
}

constexpr EffortSpec EFFORT_SPECS[] = {
    {"BVE", &DefaultStrategyConfig::Inprocess::bve_permille, meter_bve, true,
     false, nullptr},
    {"Probe", &DefaultStrategyConfig::Inprocess::probe_permille, meter_probe,
     true, false, &Stats::probe_ticks},
    {"HyperBinaryRes",
     &DefaultStrategyConfig::Inprocess::hyper_binary_permille, meter_hbr, true,
     true, &Stats::hbr_ticks},
    {"Backbone", &DefaultStrategyConfig::Inprocess::backbone_permille,
     meter_backbone, false, true, &Stats::backbone_ticks},
    {"TransitiveReduce", &DefaultStrategyConfig::Inprocess::transitive_permille,
     meter_transitive, true, true},
    {"Vivify", &DefaultStrategyConfig::Inprocess::vivify_permille,
     meter_vivify, false, true, &Stats::vivify_ticks},
    {"Strengthen", &DefaultStrategyConfig::Inprocess::strengthen_permille,
     meter_forward_pair, false, true, nullptr},
    {"SubsumeForward",
     &DefaultStrategyConfig::Inprocess::subsume_forward_permille,
     meter_forward_pair, false, true, nullptr},
    {"Factor", &DefaultStrategyConfig::Inprocess::factor_permille,
     meter_factor, false, true, nullptr},
    {"HyperTernaryRes",
     &DefaultStrategyConfig::Inprocess::hyper_ternary_permille, meter_ternary,
     false, true, nullptr},
    {"BCE", &DefaultStrategyConfig::Inprocess::bce_permille, meter_bce, false,
     true},
    {"CCE", &DefaultStrategyConfig::Inprocess::cce_permille, meter_cce, false,
     true},
    {"Instantiate", &DefaultStrategyConfig::Inprocess::instantiate_permille,
     meter_instantiate, false, true, &Stats::instantiate_ticks},
    {"Sweep", &DefaultStrategyConfig::Inprocess::sweep_permille, meter_sweep,
     false, true},
    {"Substitute", &DefaultStrategyConfig::Inprocess::substitute_permille,
     meter_substitute, false, true},
};

const EffortSpec *effort_spec(const char *name) {
  for (const EffortSpec &s : EFFORT_SPECS)
    if (!std::strcmp(s.name, name))
      return &s;
  return nullptr; // all-or-nothing rule: no titration, no meter
}

const char *effort_currency(const char *name) {
  if (!std::strcmp(name, "BVE"))
    return "resolutions";
  if (!std::strcmp(name, "Strengthen") ||
      !std::strcmp(name, "SubsumeForward"))
    return "forward-plus-strengthen-visits";
  if (!std::strcmp(name, "Sweep"))
    return "sweep-scheduled-effort";
  if (!std::strcmp(name, "TransitiveReduce"))
    return "transitive-cache-lines";
  if (!std::strcmp(name, "Congruence"))
    return "congruence-ticks";
  if (!std::strcmp(name, "Substitute"))
    return "substitute-cache-lines";
  return "probing-rule-ticks";
}

#ifdef TACTIX_ENABLE_RULE_JOURNAL
const char *journal_result(ApplyResult result) {
  switch (result) {
  case ApplyResult::Ok:
    return "applied";
  case ApplyResult::NoOp:
    return "no-op";
  case ApplyResult::Conflict:
    return "conflict";
  case ApplyResult::Sat:
    return "sat";
  case ApplyResult::Unsat:
    return "refuted";
  }
  return "unknown";
}
#endif

execution::NativeFeedbackSpec slot_feedback(
    const Kernel &k, const DefaultInprocessScheduler::SlotEntry &entry,
    const RuleVTable &rule, uint64_t instance, strategy::WorkLimit allowance,
    const char *operation) {
  auto meter = entry.meter;
  if (!meter && !std::strcmp(rule.name, "Congruence"))
    meter = [](const Kernel &kernel) { return kernel.stats.congruence_ticks; };
  auto spec = execution::native_rule_feedback(k, rule.name, instance,
      std::string(operation) + ";step=" + std::to_string(entry.step_id) +
      ";limit=" + std::to_string(entry.limit) + ";work-budget=" +
      std::to_string(k.work_budget) + ";work-offset=" + std::to_string(k.work_budget_offset) +
      ";work-bounded=" + std::to_string(k.work_budget_bounded),
      meter, meter ? effort_currency(rule.name) : "applications", allowance);
  spec.description.invocation->id = entry.step_id;
  spec.description.invocation->rule_work_budget = {
      kernel_has_work_budget(k) ? strategy::RuleWorkBudgetMode::Bounded
                               : strategy::RuleWorkBudgetMode::Unlimited,
      k.work_budget};
  spec.description.invocation->material.touch_filter = k.touch_filter
      ? strategy::SettingOverride::Enable : strategy::SettingOverride::Disable;
  spec.description.invocation->material.likely_kept_redundant = k.touch_redundant
      ? strategy::SettingOverride::Enable : strategy::SettingOverride::Disable;
  // An Eliminate-pipeline Strengthen pass closes with its forward closure.
  if (entry.group == SlotGroup::Eliminate && !std::strcmp(rule.name, "Strengthen"))
    spec.description.invocation->post_application =
        strategy::PostApplication::ForwardClosure;
  return spec;
}

// The meter the compile-time journal prices an entry in.  For every metered
// rule this is the ledger meter; Congruence has none, so the journal alone
// reads its inclusive counter.  Scheduling never consults this function, so
// production binaries see exactly `e.meter`.
uint64_t (*journal_effort_meter(
    const DefaultInprocessScheduler::SlotEntry &e,
    const RuleVTable &r))(const Kernel &) {
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  if (!e.meter && !std::strcmp(r.name, "Congruence"))
    return meter_congruence;
#else
  (void)r;
#endif
  return e.meter;
}

SlotGroup slot_group(strategy::InprocessPhase phase) {
  return phase == strategy::InprocessPhase::Probe ? SlotGroup::Probe
                                                   : SlotGroup::Eliminate;
}

FollowupKind followup_kind(strategy::Followup followup) {
  switch (followup) {
  case strategy::Followup::AfterCongruence:
    return FollowupKind::AfterCongruence;
  case strategy::Followup::AfterSweepOrFinal:
    return FollowupKind::AfterSweepOrFinal;
  default:
    return FollowupKind::None;
  }
}


} // namespace

const SlotOrderRow *default_slot_order(size_t *count) {
  struct ReferenceRows {
    std::vector<strategy::InprocessStep> program =
        strategy::reference_inprocess_program();
    std::vector<SlotOrderRow> rows;
    ReferenceRows() {
      rows.reserve(program.size());
      for (const strategy::InprocessStep &step : program)
        rows.push_back({step.rule.c_str(), slot_group(step.phase)});
    }
  };
  static const ReferenceRows reference;
  if (count)
    *count = reference.rows.size();
  return reference.rows.data();
}


const char *candidate_order_name(CandidateOrder o) {
  switch (o) {
  case CandidateOrder::Native:
    return "native";
  case CandidateOrder::Reverse:
    return "reverse";
  case CandidateOrder::Random:
    return "random";
  case CandidateOrder::OccAsc:
    return "occ-asc";
  case CandidateOrder::OccDesc:
    return "occ-desc";
  case CandidateOrder::NegOccDesc:
    return "negocc-desc";
  case CandidateOrder::Heuristic:
    return "heuristic";
  }
  return "native";
}

bool parse_candidate_order(const char *s, CandidateOrder *out) {
  static const struct {
    const char *n;
    CandidateOrder o;
  } table[] = {
      {"native", CandidateOrder::Native},
      {"reverse", CandidateOrder::Reverse},
      {"random", CandidateOrder::Random},
      {"occ-asc", CandidateOrder::OccAsc},
      {"occ-desc", CandidateOrder::OccDesc},
      {"negocc-desc", CandidateOrder::NegOccDesc},
      {"heuristic", CandidateOrder::Heuristic},
  };
  for (const auto &row : table)
    if (!std::strcmp(s, row.n)) {
      *out = row.o;
      return true;
    }
  return false;
}

namespace {

uint64_t instance_key(uint64_t inst) {
  Lit l;
  return lit_of_instance(inst, &l) ? static_cast<uint64_t>(l) : inst;
}

uint32_t lit_occurrences(const Kernel &k, Lit l, bool binary_only) {
  uint32_t n = 0;
  for (const WatchEntry w : k.watches.entries(l))
    n += w.binary() ? 1u : (binary_only ? 0u : 1u);
  return n;
}

double transitive_activity(const Kernel &k, Var v) {
  return k.h->scores.score[v];
}

uint64_t order_key(const Kernel &k, CandidateOrder arm, uint64_t inst,
                   bool transitive_reference_order,
                   const std::vector<Var> *reference_compact_order,
                   const std::vector<Var> *reference_compact_position) {
  Lit l;
  if (!lit_of_instance(k, inst, &l) || var_of(l) >= k.nvars)
    return 0; // not a literal payload: leave it where the native order had it
  const Var v = var_of(l);
  switch (arm) {
  case CandidateOrder::OccAsc:
  case CandidateOrder::OccDesc: {
    const uint64_t occ = lit_occurrences(k, make_lit(v, false), false) +
                         lit_occurrences(k, make_lit(v, true), false);
    return arm == CandidateOrder::OccAsc ? occ : ~occ;
  }
  case CandidateOrder::NegOccDesc:
    return ~static_cast<uint64_t>(lit_occurrences(k, negate(l), true));
  case CandidateOrder::Heuristic: {
    // transitive.c schedules variable indices but its comparator applies IDX
    // once more before consulting activity.  Keep that literal reference
    // behavior local to TransitiveReduce; other users of the generic arm rank
    // the candidate's own variable as expected.
    Var proxy = transitive_reference_order ? v >> 1 : v;
    if (transitive_reference_order && reference_compact_order &&
        reference_compact_position &&
        v < reference_compact_position->size()) {
      const Var compact = (*reference_compact_position)[v];
      if (compact != INVALID_VAR &&
          (compact >> 1) < reference_compact_order->size())
        proxy = (*reference_compact_order)[compact >> 1];
    }
    if (k.h->stable) {
      const double s = transitive_reference_order
                           ? transitive_activity(k, proxy)
                           : k.h->scores.score[proxy];
      uint64_t bits = 0;
      std::memcpy(&bits, &s, sizeof bits);
      return bits;
    }
    return k.h->links[proxy].stamp;
  }
  default:
    return 0;
  }
}

template <class Less>
void kissat_reference_sort(std::vector<Var> &values, Less less) {
  const size_t n = values.size();
  if (n <= 1)
    return;

  auto greater_swap = [&](size_t p, size_t q) {
    if (less(values[q], values[p]))
      std::swap(values[p], values[q]);
  };

  constexpr size_t quick_sort_limit = 10;
  size_t left = 0, right = n - 1;
  std::vector<std::pair<size_t, size_t>> pending;
  if (right - left > quick_sort_limit) {
    for (;;) {
      const size_t middle = left + (right - left) / 2;
      std::swap(values[middle], values[right - 1]);
      greater_swap(left, right - 1);
      greater_swap(left, right);
      greater_swap(right - 1, right);

      size_t i = left;
      size_t j = right - 1;
      const Var pivot = values[j];
      for (;;) {
        while (less(values[++i], pivot))
          ;
        while (less(pivot, values[--j]))
          if (j == left)
            break;
        if (i >= j)
          break;
        std::swap(values[i], values[j]);
      }
      std::swap(values[i], values[right - 1]);

      size_t other_left, other_right;
      if (i - left < right - i) {
        other_left = i + 1;
        other_right = right;
        right = i - 1;
      } else {
        other_left = left;
        other_right = i - 1;
        left = i + 1;
      }
      if (right - left > quick_sort_limit) {
        assert(other_right - other_left > quick_sort_limit);
        pending.emplace_back(other_left, other_right);
      } else if (other_right - other_left > quick_sort_limit) {
        left = other_left;
        right = other_right;
      } else if (!pending.empty()) {
        left = pending.back().first;
        right = pending.back().second;
        pending.pop_back();
      } else {
        break;
      }
    }
  }

  // Kissat finishes the whole array with the same sentinel-seeded insertion
  // sort after quicksort partitions it (sort.h:83-104).
  for (size_t i = n - 1; i > 0; i--)
    greater_swap(i - 1, i);
  for (size_t i = 2; i < n; i++) {
    const Var pivot = values[i];
    size_t j = i;
    while (less(pivot, values[j - 1])) {
      values[j] = values[j - 1];
      j--;
    }
    values[j] = pivot;
  }
}

bool rank_transitive_reference(const Kernel &k, uint64_t *insts,
                               uint32_t n) {
  if (!k.h || k.compact_import_order.empty())
    return false;
  std::vector<Var> compact_position(k.nvars, INVALID_VAR);
  for (Var compact = 0; compact < k.compact_import_order.size(); compact++)
    compact_position[k.compact_import_order[compact]] = compact;

  std::vector<uint64_t> positive(k.nvars, UINT64_MAX);
  std::vector<uint64_t> negative(k.nvars, UINT64_MAX);
  std::vector<Var> compact_candidates;
  std::vector<uint8_t> seen(k.compact_import_order.size(), 0);
  for (uint32_t i = 0; i < n; i++) {
    Lit lit;
    if (!lit_of_instance(k, insts[i], &lit) || var_of(lit) >= k.nvars)
      return false;
    const Var v = var_of(lit);
    const Var compact = compact_position[v];
    if (compact == INVALID_VAR)
      return false;
    (negated(lit) ? negative[v] : positive[v]) = insts[i];
    if (!seen[compact]) {
      seen[compact] = 1;
      compact_candidates.push_back(compact);
    }
  }
  // Kissat's `all_variables` seeds the stack in current compact-index order.
  std::sort(compact_candidates.begin(), compact_candidates.end());
  auto less = [&](Var a, Var b) {
    const Var i = a >> 1, j = b >> 1; // transitive.c's second IDX
    const Var iv = k.compact_import_order[i], jv = k.compact_import_order[j];
    if (k.h->stable) {
      const double s = transitive_activity(k, iv);
      const double t = transitive_activity(k, jv);
      if (s < t)
        return true;
      if (s > t)
        return false;
      return i < j;
    }
    return k.h->links[iv].stamp < k.h->links[jv].stamp;
  };
  kissat_reference_sort(compact_candidates, less);

  uint32_t out = 0;
  for (auto it = compact_candidates.rbegin(); it != compact_candidates.rend();
       ++it) {
    const Var v = k.compact_import_order[*it];
    if (positive[v] != UINT64_MAX)
      insts[out++] = positive[v];
    if (negative[v] != UINT64_MAX)
      insts[out++] = negative[v];
  }
  assert(out == n);
  return true;
}


bool rank_bve_reference(const Kernel &k, uint64_t *insts, uint32_t n) {
  if (!k.h)
    return false;
  BveOccurrences occ;
  bve_build_occurrences(k, occ);
  std::vector<std::pair<double, uint64_t>> ranked;
  ranked.reserve(n);
  const uint64_t occurrence_limit = k.bve_occurrence_limit;
  for (uint32_t i = 0; i < n; i++) {
    Lit lit;
    if (!lit_of_instance(k, insts[i], &lit) || var_of(lit) >= k.nvars)
      return false;
    const Var v = var_of(lit);
    const double relevance =
        k.h->stable ? k.h->scores.score[v] : k.h->links[v].stamp;
    const double score = kissat_bve_variable_score(
        relevance, occ.count(make_lit(v, false)),
        occ.count(make_lit(v, true)), occurrence_limit);
    ranked.emplace_back(score, insts[i]);
  }
  std::stable_sort(ranked.begin(), ranked.end(),
                   [](const auto &a, const auto &b) {
                     return a.first < b.first;
                   });
  for (uint32_t i = 0; i < n; i++)
    insts[i] = ranked[i].second;
  return true;
}

} // namespace

void DefaultCandidateOrder::rank(const Kernel &k, uint32_t rule,
                                 uint64_t *insts, uint32_t n) {
  const CandidateOrder arm = arm_of(rule);
  if (arm == CandidateOrder::Native || n < 2)
    return;
  if (arm == CandidateOrder::Reverse) {
    std::reverse(insts, insts + n);
    return;
  }
  if (arm == CandidateOrder::Random) {
    for (uint32_t i = n; i > 1; i--) {
      rng_ += increment_;
      uint64_t z = rng_;
      z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
      z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
      z ^= z >> 31;
      std::swap(insts[i - 1], insts[z % i]);
    }
    return;
  }
  if (arm == CandidateOrder::Heuristic && !k.h)
    return;
  const std::vector<RuleVTable> &rules = tactix_rules();
  const bool transitive_reference_order =
      arm == CandidateOrder::Heuristic && rule < rules.size() &&
      !std::strcmp(rules[rule].name, "TransitiveReduce");
  const bool bve_reference_order =
      arm == CandidateOrder::Heuristic && rule < rules.size() &&
      !std::strcmp(rules[rule].name, "BVE");
  if (transitive_reference_order && rank_transitive_reference(k, insts, n))
    return;
  if (bve_reference_order && rank_bve_reference(k, insts, n))
    return;
  std::vector<Var> compact_position;
  const bool has_reference_compact_order =
      transitive_reference_order && k.h && !k.compact_import_order.empty();
  if (has_reference_compact_order) {
    compact_position.assign(k.nvars, INVALID_VAR);
    for (Var i = 0; i < k.compact_import_order.size(); i++)
      compact_position[k.compact_import_order[i]] = i;
  }
  scratch_.resize(n);
  for (uint32_t i = 0; i < n; i++)
    scratch_[i] = {order_key(k, arm, insts[i], transitive_reference_order,
                                has_reference_compact_order
                                    ? &k.compact_import_order
                                    : nullptr,
                                has_reference_compact_order ? &compact_position
                                                            : nullptr),
                   insts[i]};
  std::stable_sort(
      scratch_.begin(), scratch_.end(),
      [transitive_reference_order, has_reference_compact_order,
       &compact_position](
          const std::pair<uint64_t, uint64_t> &a,
          const std::pair<uint64_t, uint64_t> &b) {
        if (a.first != b.first)
          return transitive_reference_order ? a.first > b.first
                                            : a.first < b.first;
        if (transitive_reference_order) {
          Lit al, bl;
          if (lit_of_instance(a.second, &al) &&
              lit_of_instance(b.second, &bl)) {
            const Var av = var_of(al), bv = var_of(bl);
            const Var ac = has_reference_compact_order &&
                                   av < compact_position.size() &&
                                   compact_position[av] != INVALID_VAR
                               ? compact_position[av]
                               : av;
            const Var bc = has_reference_compact_order &&
                                   bv < compact_position.size() &&
                                   compact_position[bv] != INVALID_VAR
                               ? compact_position[bv]
                               : bv;
            // The second IDX maps each adjacent pair of compact candidate
            // variables to one activity owner.  Kissat's sort/pop pose consumes
            // the lower compact candidate first inside that equal-key pair;
            // See benchmarks/reports/2026-08-21-tierd-rank27-transitive-reference-order.md.
            if ((ac >> 1) != (bc >> 1))
              return (ac >> 1) < (bc >> 1);
            if (ac != bc)
              return ac < bc;
          }
        }
        return false;
      });
  for (uint32_t i = 0; i < n; i++)
    insts[i] = scratch_[i].second;
}

void DefaultInprocessScheduler::order_candidates(const Kernel &k, SlotEntry &e,
                                     uint64_t *insts, uint32_t n) {
  if (n == 0)
    return;
  e.ord.lists++;
  e.ord.candidates += n;
  if (e.cursor_enabled) {
    uint32_t start = 0;
    while (start < n && instance_key(insts[start]) < e.cursor)
      start++;
    if (start >= n)
      start = 0; // the cursor ran past the set: begin again at the head
    if (start) {
      std::rotate(insts, insts + start, insts + n);
      e.ord.rotated++;
    }
  }
  if (!candidate_order && e.order == CandidateOrder::Native)
    return;
  CandidateOrderPolicy &cp =
      candidate_order ? *candidate_order : default_candidate_order_;
  order_before_.assign(insts, insts + n);
  cp.rank(k, static_cast<uint32_t>(e.rule), insts, n);
  uint64_t moved = 0;
  for (uint32_t i = 0; i < n; i++)
    moved += (order_before_[i] != insts[i]);
  if (moved) {
    e.ord.ranked++;
    e.ord.moved += moved;
  }
}

bool DefaultInprocessScheduler::want_phase(const Kernel &k, SlotGroup g) {
  if (!initialized_)
    return false;
  if (preprocess_only_ && k.stats.conflicts)
    return false;
  if (k.stats.conflicts == 0 && g == SlotGroup::Eliminate)
    return false;
  // Kissat declines both phases on the same conflict as its latest Reduce.
  // The reduction-count Probe watermark below answers a different question:
  // whether any Reduce happened since the preceding Probe round.
  if (last_reduce_conflicts_ == k.stats.conflicts)
    return false;
  SlotClock &c = g == SlotGroup::Probe ? probe_clock : elim_clock;
  // Kissat's two guards deliberately differ.  Probe tests
  // `limit < conflicts` (probe.c:kissat_probing), while Eliminate declines
  // only for `limit > conflicts` (eliminate.c:kissat_eliminating) and is due
  // at equality.  The round-zero probe clock is the explicit startup-
  // preprocessing invitation, not such a deadline, and remains due at zero.
  const bool startup_probe =
      g == SlotGroup::Probe && c.rounds == 0 &&
      k.stats.conflicts == c.next_conflicts;
  const bool before_deadline =
      g == SlotGroup::Probe ? k.stats.conflicts <= c.next_conflicts
                            : k.stats.conflicts < c.next_conflicts;
  if (!startup_probe && before_deadline)
    return false;
  const bool gated =
      slot_watermark_ && c.rounds != 0 &&
      (g == SlotGroup::Probe
           ? k.stats.reductions == c.reductions_mark
           : (k.stats.bve_marks <= c.marks_mark &&
              kernel_logical_root_fixed(k) <= c.fixed_mark &&
              k.subsume_material_count <= c.material_mark));
  if (gated) {
    if (!c.blocked) { // count episodes, not search steps
      c.blocked = true;
      c.blocked_episodes++;
    }
    return false;
  }
  return true;
}

void DefaultInprocessScheduler::update_limit(const Kernel &k, SlotGroup g) {
  const auto &cfg = config();
  SlotClock &c = g == SlotGroup::Probe ? probe_clock : elim_clock;
  const bool split = slot_clocks_split_;
  uint64_t n = split ? c.rounds
                     : std::max(probe_clock.rounds, elim_clock.rounds);
  if (!n)
    n = 1;
  const double lg =
      std::log10(static_cast<double>(n) + cfg.logarithm_offset);
  const double base = split && g == SlotGroup::Probe
                          ? cfg.probe_interval * static_cast<double>(n) * lg
                          : cfg.eliminate_interval * static_cast<double>(n) *
                                lg * lg;
  c.next_conflicts = k.stats.conflicts +
                     scale_inprocessing_delta(k, static_cast<uint64_t>(base),
                                              cfg);
}

void DefaultPolicy::update_reduce_limit(const Kernel &k) {
  DefaultDatabaseControl::update_limit(k, config_.reduction, reduce_sqrt_);
}

// The schedule's profile follows the configuration; its deadline, interval
// and snapshots are schedule state (reset_schedule and solve entry).
void DefaultPolicy::sync_retier_profile() {
  const DefaultStrategyConfig::Reduction &config = config_.reduction;
  retier_.enabled = dynamic_tiers_;
  retier_.max_interval = config.retier_max_interval;
  retier_.growth = config.retier_growth;
  retier_.tier1_default = config.tier1_default;
  retier_.tier2_default = config.tier2_default;
  retier_.tier1_relative_permille = config.tier1_relative_permille;
  retier_.tier2_relative_permille = config.tier2_relative_permille;
}

FormulaClassification DefaultPolicy::classify_formula_counts(
    uint64_t binaries, uint64_t irredundant_large) const {
  const uint64_t clauses = binaries + irredundant_large;
  FormulaClassification result;
  result.small = clauses <= config_.analysis.small_clause_limit;
  if (result.small)
    return result;

  // This intentionally preserves Kissat's unit mismatch: `kissat_percent`
  // returns 0..100 while `bigbigfraction` is divided by 1000.0. Thus the
  // shipped 990 threshold is 0.99%, not 99%.
  const unsigned __int128 lhs =
      static_cast<unsigned __int128>(100000) * binaries;
  const unsigned __int128 rhs =
      static_cast<unsigned __int128>(
          config_.analysis.big_binary_fraction_permille) *
      clauses;
  result.bigbig = lhs >= rhs;
  return result;
}

FormulaClassification DefaultPolicy::classify_formula(const Kernel &k) const {
  // kissat_classify reads `clauses_binary + clauses_irredundant`; recomputing
  // them here walked every watch list and the whole arena on every call.
#ifndef NDEBUG
  kernel_check_binirr_counts(k);
#endif
  return classify_formula_counts(k.watches.binary_clauses(),
                                 k.live_irredundant_large);
}

void DefaultPolicy::update_formula_classification(Kernel &k) const {
  const FormulaClassification classification = classify_formula(k);
  // `classification.small` is write-only in pinned Kissat. Do not invent a
  // second consumer: only `bigbig` gates ordinary fast binary assignment,
  // through the installed ConflictAnalysis profile.
  strategy::AnalyzeParameters analysis = strategy::AnalyzeParameters::from_kernel(k);
  analysis.jump_reasons_bigbig = classification.bigbig;
  port::execute_action(k, execution::ParameterInstallationRequest{analysis},
                            completion_, "ConflictAnalysis");
  k.stats.jump_reason_classifications++;
  k.stats.jump_reason_bigbig += classification.bigbig;
}

size_t GlueTieredReduction::victims(const Kernel &k, size_t candidates) {
  static const DefaultStrategyConfig::Reduction defaults;
  const auto &cfg = config_ ? *config_ : defaults;
  if (!ramp)
    return reduce_fractional_victims(candidates, cfg.victim_numerator,
                                     cfg.victim_denominator);
  if (cfg.low_percent >= cfg.high_percent)
    return static_cast<size_t>(static_cast<double>(candidates) *
                               cfg.low_percent / PERCENT_SCALE);
  const double r = static_cast<double>(k.stats.reductions) + 1.0;
  const double percent =
      cfg.high_percent - (cfg.high_percent - cfg.low_percent) /
                             std::log10(r + cfg.logarithm_offset);
  return static_cast<size_t>(static_cast<double>(candidates) * percent /
                             PERCENT_SCALE);
}

Lit DefaultBranching::pick(Kernel &k) { return pick_impl(k, false); }

Lit DefaultBranching::pick_for_restart_reuse(Kernel &k) {
  return pick_impl(k, true);
}

Lit DefaultBranching::pick_impl(Kernel &k, bool restart_reuse) {
  assert(k.unassigned > 0);
  if (!k.warming && randec_enabled && !k.h->stable) {
    const uint64_t c = k.stats.conflicts;
    event_trace_randec_state(k, "pre", randec_remaining, randec_next,
                             randec_sequences, randec_rng);
    // An ordinary kissat_decide increments `solver->level` before this test,
    // so Tactix's pre-frame equivalent admits only level zero.  Restart trail
    // reuse calls kissat_next_decision_variable directly, without incrementing
    // the level, and therefore admits retained levels zero or one.  Keep that
    // call-context distinction explicit; conflating them suppressed due random
    // sequences at level-one restarts and changed the subsequent CDCL path.
    const bool level_allows_start = restart_reuse ? k.level <= 1 : k.level == 0;
    if (!randec_remaining && level_allows_start && c >= randec_next)
      start_random_sequence(k);
    event_trace_randec_state(k, randec_remaining ? "active" : "inactive",
                             randec_remaining, randec_next, randec_sequences,
                             randec_rng);
    if (randec_remaining) {
      for (;;) {
        randec_rng = randec_rng * RANDEC_MULTIPLIER + RANDEC_INCREMENT;
        const uint32_t random = static_cast<uint32_t>(randec_rng >> 32);
        assert(!k.compact_import_order.empty());
        const uint32_t slot = random % k.compact_import_order.size();
        const Var v = k.compact_import_order[slot];
        const bool accepted = k.values[make_lit(v, true)] == 0 &&
                              !kernel_var_retired(k, v);
        event_trace_randec_draw(k, random, slot, v, accepted);
        if (accepted) {
          k.stats.random_decisions++;
          return decide_lit(k, v);
        }
      }
    }
  }
  const Var v = heuristics_next_active_decision_var(k);
  return decide_lit(k, v);
}

int8_t DefaultBranching::forced_phase(const Kernel &k) const {
  static const DefaultStrategyConfig::Branching defaults;
  const auto &cfg = config_ ? *config_ : defaults;
  if (k.h->stable || !cfg.focused_phase_switch_span ||
      cfg.focused_phase_pattern.empty())
    return 0;
  const uint64_t epoch = k.stats.switched / cfg.focused_phase_switch_span;
  return cfg.focused_phase_pattern[epoch % cfg.focused_phase_pattern.size()];
}

Lit DefaultBranching::decide_lit(const Kernel &k, Var v) {
  static const DefaultStrategyConfig::Branching defaults;
  const auto &cfg = config_ ? *config_ : defaults;
  int8_t phase = 0;
  if (!k.h->stable && cfg.focused_phase_switch_span &&
      !cfg.focused_phase_pattern.empty()) {
    const uint64_t epoch =
        k.stats.switched / cfg.focused_phase_switch_span;
    if (focused_phase_epoch_ != epoch) {
      focused_phase_epoch_ = epoch;
      focused_phase_ =
          cfg.focused_phase_pattern[epoch % cfg.focused_phase_pattern.size()];
    }
    phase = focused_phase_;
  }
  const int8_t target = k.h->stable ? k.h->target[v] : 0;
  const int8_t saved = k.h->saved[v];
  const Lit result =
      phase ? make_lit(v, phase < 0) : heuristics_decide_lit(k, v);
  event_trace_phase_pick(k, v, target, saved,
                         negated(result) ? -1 : 1);
  return result;
}

void DefaultBranching::start_random_sequence(Kernel &k) {
  if (!randec_enabled || k.h->stable || randec_remaining)
    return;
  static const DefaultStrategyConfig::Branching defaults;
  const auto &cfg = config_ ? *config_ : defaults;
  randec_sequences++;
  const double n = static_cast<double>(randec_sequences);
  const double logn = std::log10(n + cfg.logarithm_offset);
  randec_remaining =
      static_cast<uint64_t>(cfg.random_sequence_length * logn);
  randec_next =
      k.stats.conflicts +
      static_cast<uint64_t>(cfg.random_initial_interval * logn);
  k.stats.random_sequences++;
}

void DefaultPolicy::configure_from_env(const Kernel &) {
  config_ = base_config_;
  // Preserve the pre-existing white-box policy pose surface: callers and
  // focused tests may set these public members before solve().
  config_.analysis.chronological_levels = chrono_levels;
  config_.branching.random_sequences = default_branching_.randec_enabled;
  if (const char *fm = strategy_setting("TACTIX_FORCE_MODE")) {
    if (!std::strcmp(fm, "focused"))
      config_.mode.force = 1;
    else if (!std::strcmp(fm, "stable"))
      config_.mode.force = 2;
  }
  if (strategy_setting("TACTIX_NO_BUMP_REASONS"))
    config_.analysis.bump_reason_limit = 0;
  if (const char *bl = strategy_setting("TACTIX_BUMP_REASONS_LIMIT"))
    config_.analysis.bump_reason_limit =
        static_cast<uint32_t>(std::strtoul(bl, nullptr, 10));
  if (const char *br = strategy_setting("TACTIX_BUMP_REASONS_RATE"))
    config_.analysis.bump_reason_rate = std::strtod(br, nullptr);
  const char *cad = strategy_setting("TACTIX_REDUCE_CADENCE"); // "sqrt" | "arith"
  const char *rmp = strategy_setting("TACTIX_REDUCE_RAMP");    // "1" | "0"
  if (cad)
    config_.reduction.square_root_cadence =
        std::strcmp(cad, "arith") != 0;
  if (rmp)
    config_.reduction.ramp_fraction = rmp[0] == '1';
  else if (cad)
    config_.reduction.ramp_fraction =
        config_.reduction.square_root_cadence;
  if (strategy_setting("TACTIX_NO_DYNAMIC_TIERS"))
    config_.reduction.dynamic_tiers = false;
  if (strategy_setting("TACTIX_NO_COMPACT"))
    config_.reduction.compact_variables = false;
  if (const char *ll = strategy_setting("TACTIX_LUCKY_LATE"))
    config_.rephase.late_lucky = std::strtoul(ll, nullptr, 10) != 0;
  if (strategy_setting("TACTIX_REPHASE_BOTH_MODES"))
    config_.rephase.stable_only = false;
  if (const char *ro = strategy_setting("TACTIX_REORDER"))
    config_.reorder.mode =
        static_cast<uint32_t>(std::strtoul(ro, nullptr, 10));
  if (const char *cl = strategy_setting("TACTIX_CHRONO_LEVELS")) {
    const long n = std::strtol(cl, nullptr, 10);
    if (n >= 0)
      config_.analysis.chronological_levels = static_cast<uint32_t>(n);
  }
  if (const char *value = strategy_setting("TACTIX_CONFLICT_REUSE"))
    config_.analysis.conflict_reuse = *value != '0';
  if (const char *value = strategy_setting("TACTIX_JUMP_REASONS"))
    config_.analysis.jump_reasons = *value != '0';
  if (const char *value = strategy_setting("TACTIX_SHRINK_GLUE"))
    config_.analysis.shrink_glue_promote = *value != '0';
  if (strategy_setting("TACTIX_NO_EXT_REASON_BUMP"))
    config_.analysis.bump_extension_reasons = false;
  if (const char *value = strategy_setting("TACTIX_EAGER_SUBSUME"))
    config_.analysis.eager_subsume = std::min<uint32_t>(
        static_cast<uint32_t>(std::strtoul(value, nullptr, 10)),
        strategy::AnalyzeParameters::EAGER_SUBSUME_CAPACITY);
  if (const char *rd = strategy_setting("TACTIX_RANDEC"))
    config_.branching.random_sequences = *rd != '0';
  if (const char *wp = strategy_setting("TACTIX_WALK_PERMILLE"))
    config_.rephase.walk_permille = std::strtoull(wp, nullptr, 10);
  if (const char *bc = strategy_setting("TACTIX_BVE_RAMP_MAX"))
    config_.inprocess.bve_default_ceiling =
        static_cast<uint32_t>(std::strtoul(bc, nullptr, 10));
  if (const char *dt = strategy_setting("TACTIX_BVE_DEF_TICKS"))
    config_.inprocess.bve_definition_budget = std::strtoull(dt, nullptr, 10);
  if (const char *dc = strategy_setting("TACTIX_BVE_DEF_CORES"))
    config_.inprocess.bve_definition_cores = std::clamp<uint32_t>(
        static_cast<uint32_t>(std::strtoul(dc, nullptr, 10)), 1, 100);
  if (const char *dm = strategy_setting("TACTIX_BVE_DEF_CORE_MULTIPLIER"))
    config_.inprocess.bve_definition_core_budget_multiplier =
        static_cast<uint32_t>(std::strtoul(dm, nullptr, 10));
  if (const char *value = strategy_setting("TACTIX_BVE_BACKWARD"))
    config_.inprocess.bve_backward = *value != '0';
  if (const char *value = strategy_setting("TACTIX_BVE_ON_THE_FLY"))
    config_.inprocess.bve_on_the_fly_strengthening = *value != '0';
  if (const char *value = strategy_setting("TACTIX_BCE_OCC_LIMIT"))
    config_.inprocess.bce_parameters.occurrence_limit =
        static_cast<uint32_t>(std::strtoul(value, nullptr, 10));
  if (const char *value = strategy_setting("TACTIX_TERNARY_OCC_LIMIT"))
    config_.inprocess.ternary_parameters.occurrence_limit =
        static_cast<uint32_t>(std::strtoul(value, nullptr, 10));
  if (const char *value = strategy_setting("TACTIX_TERNARY_MAXADD"))
    config_.inprocess.ternary_parameters.maximum_add_percent =
        static_cast<uint32_t>(std::strtoul(value, nullptr, 10));
  if (const char *value = strategy_setting("TACTIX_FACTOR_SIZE"))
    config_.inprocess.factor_parameters.maximum_clause_size =
        static_cast<uint32_t>(std::strtoul(value, nullptr, 10));
  if (const char *value = strategy_setting("TACTIX_FACTOR_BOUND"))
    config_.inprocess.factor_parameters.growth_bound =
        static_cast<uint32_t>(std::strtoul(value, nullptr, 10));
  if (const char *value = strategy_setting("TACTIX_FACTOR_DEMOTE"))
    config_.inprocess.factor_parameters.demote_fresh = *value != '0';
  if (const char *value = strategy_setting("TACTIX_FACTOR_CAND_ROUNDS"))
    config_.inprocess.factor_parameters.candidate_rounds =
        static_cast<uint32_t>(std::strtoul(value, nullptr, 10));
  if (const char *value = strategy_setting("TACTIX_V4_EARLY_EXIT"))
    config_.inprocess.transitive_parameters.early_exit = *value != '0';
  if (const char *value = strategy_setting("TACTIX_TRANSITIVE_LIVE_METER"))
    config_.inprocess.transitive_parameters.live_meter = *value != '0';
  if (const char *value = strategy_setting("TACTIX_V4_METER"))
    config_.inprocess.transitive_parameters.checked_budget_meter =
        !std::strcmp(value, "checked");
  if (const char *value = strategy_setting("TACTIX_VIVIFY_REUSE"))
    config_.inprocess.vivify_parameters.reuse = *value != '0';
  if (const char *value = strategy_setting("TACTIX_VIVIFY_FLUSH"))
    config_.inprocess.vivify_parameters.flush = *value != '0';
  if (strategy_setting("TACTIX_SUBST_DEMOTE"))
    config_.inprocess.substitute_parameters.demote_substituted = true;
  if (strategy_setting("TACTIX_NO_BVE"))
    config_.inprocess.bve_enabled = false;
  if (strategy_setting("TACTIX_NO_FACTOR"))
    config_.inprocess.factor_enabled = false;
  if (strategy_setting("TACTIX_NO_SWEEP"))
    config_.inprocess.sweep_enabled = false;
  if (const char *hold = strategy_setting("TACTIX_SUBSUME_HOLD"))
    config_.inprocess.hold_subsume_until_eliminate = *hold != '0';
  if (const char *touch = strategy_setting("TACTIX_TOUCH_MARKS"))
    config_.inprocess.touch_filter = *touch != '0';
  if (const char *redundant = strategy_setting("TACTIX_TOUCH_REDUNDANT"))
    config_.inprocess.likely_kept_redundant = *redundant != '0';
  const auto sweep_u32 = [](const char *name, uint32_t &slot) {
    if (const char *s = strategy_setting(name))
      slot = static_cast<uint32_t>(std::strtoul(s, nullptr, 10));
  };
  const auto sweep_bool = [](const char *name, bool &slot) {
    if (const char *s = strategy_setting(name))
      slot = std::strtoul(s, nullptr, 10) != 0;
  };
  sweep_u32("TACTIX_SWEEP_DEPTH",
            config_.inprocess.sweep_parameters.max_depth);
  sweep_u32("TACTIX_SWEEP_VARS",
            config_.inprocess.sweep_parameters.max_env_vars);
  sweep_u32("TACTIX_SWEEP_CLAUSES",
            config_.inprocess.sweep_parameters.max_env_clauses);
  sweep_u32("TACTIX_SWEEP_CANDIDATES",
            config_.inprocess.sweep_parameters.max_candidates);
  sweep_u32("TACTIX_SWEEP_FLIP_ROUNDS",
            config_.inprocess.sweep_parameters.flip_rounds);
  sweep_u32("TACTIX_SWEEP_SEEDS",
            config_.inprocess.sweep_parameters.max_seeds);
  if (const char *s = strategy_setting("TACTIX_SWEEP_TICKS"))
    config_.inprocess.sweep_parameters.env_ticks =
        std::strtoull(s, nullptr, 10);
  if (const char *s = strategy_setting("TACTIX_SWEEP_ROUND_TICKS"))
    config_.inprocess.sweep_parameters.round_ticks =
        std::strtoull(s, nullptr, 10);
  sweep_bool("TACTIX_SWEEP_STRIP",
             config_.inprocess.sweep_parameters.strip_false);
  sweep_bool("TACTIX_SWEEP_CLOSURE",
             config_.inprocess.sweep_parameters.measure_closure);
  sweep_bool("TACTIX_SWEEP_PARTITION",
             config_.inprocess.sweep_parameters.full_partition);
  sweep_bool("TACTIX_SWEEP_RESUME",
             config_.inprocess.sweep_parameters.resume);
  sweep_bool("TACTIX_SWEEP_FORCE_GENERAL",
             config_.inprocess.sweep_parameters.force_general_proof);
  sweep_bool("TACTIX_ARENA_ORDER",
             config_.inprocess.cadical_arena_ordering_enabled);

  if (strategy_setting("TACTIX_NO_CLOSURE_DELAY"))
    config_.inprocess.closure_delay = false;
  if (const char *value = strategy_setting("TACTIX_FACTOR_INIT_TICKS"))
    config_.inprocess.factor_initial_ticks = std::strtoull(value, nullptr, 10);
  if (strategy_setting("TACTIX_PREPROCESS_ONLY"))
    config_.inprocess.preprocess_only = true;
  if (const char *value = strategy_setting("TACTIX_PREPROCESS_FOLLOWUPS"))
    config_.inprocess.preprocess_followups =
        std::strtoul(value, nullptr, 10) != 0;
  if (const char *value = strategy_setting("TACTIX_SLOT_CLOCKS"))
    config_.inprocess.split_clocks = std::strtoul(value, nullptr, 10) != 1;
  if (const char *value = strategy_setting("TACTIX_SLOT_WATERMARK"))
    config_.inprocess.watermarks = std::strtoul(value, nullptr, 10) != 0;
  if (const char *value = strategy_setting("TACTIX_FACTOR_COUPLE"))
    config_.inprocess.couple_factor_bound =
        std::strtoul(value, nullptr, 10) != 0;

  const auto list_has = [](const char *list, const std::string &name) {
    if (!list)
      return false;
    const size_t n = name.size();
    for (const char *p = list; (p = std::strstr(p, name.c_str())); p += n)
      if ((p == list || p[-1] == ',') && (p[n] == '\0' || p[n] == ','))
        return true;
    return false;
  };
  const auto mapped_u32 = [](const char *map, const std::string &name,
                             uint32_t &value) {
    if (!map)
      return false;
    const size_t n = name.size();
    for (const char *p = map; (p = std::strstr(p, name.c_str())); p += n)
      if ((p == map || p[-1] == ',') && p[n] == ':') {
        value = static_cast<uint32_t>(std::strtoul(p + n + 1, nullptr, 10));
        return true;
      }
    return false;
  };
  const char *enabled = strategy_setting("TACTIX_SLOT_ENABLE");
  const char *disabled = strategy_setting("TACTIX_SLOT_DISABLE");
  const char *permille = strategy_setting("TACTIX_SLOT_PERMILLE");
  const char *maximum = strategy_setting("TACTIX_SLOT_MAX_APPLIES");
  for (strategy::InprocessStep &step : config_.inprocess.program) {
    if (list_has(enabled, step.rule)) {
      step.enabled = true;
      if (step.rule == "BVE")
        config_.inprocess.bve_enabled = true;
      else if (step.rule == "Vivify")
        config_.inprocess.vivify_enabled = true;
      else if (step.rule == "Factor")
        config_.inprocess.factor_enabled = true;
      else if (step.rule == "Sweep")
        config_.inprocess.sweep_enabled = true;
    }
    if (list_has(disabled, step.rule))
      step.enabled = false;
    uint32_t value = 0;
    if (mapped_u32(permille, step.rule, value) &&
        effort_spec(step.rule.c_str()))
      step.effort_permille = value;
    if (mapped_u32(maximum, step.rule, value))
      step.max_applications = value;

    char knob[96];
    const auto knob_name = [&knob](const char *prefix,
                                   const std::string &name) {
      size_t p = 0;
      for (const char *s = prefix; *s && p + 1 < sizeof knob; ++s)
        knob[p++] = *s;
      for (const char c : name)
        if (p + 1 < sizeof knob)
          knob[p++] = static_cast<char>(
              std::toupper(static_cast<unsigned char>(c)));
      knob[p] = '\0';
    };
    knob_name("TACTIX_CURSOR_", step.rule);
    if (const char *cursor = strategy_setting(knob))
      step.cursor_enabled = std::strtoul(cursor, nullptr, 10) != 0;
    knob_name("TACTIX_ORDER_", step.rule);
    if (const char *order = strategy_setting(knob)) {
      CandidateOrder parsed;
      if (!parse_candidate_order(order, &parsed)) {
        std::fprintf(stderr, "c ERROR: %s: unknown candidate-order arm\n",
                     knob);
        std::abort();
      }
      step.candidate_order = parsed;
    }
  }

  std::string config_error;
  if (!config_.valid(&config_error)) {
    std::fprintf(stderr, "c ERROR: invalid strategy configuration: %s\n",
                 config_error.c_str());
    std::abort();
  }

  force_mode_ = config_.mode.force;
  bump_reasons_limit_ = config_.analysis.bump_reason_limit;
  bump_reasons_rate_ = config_.analysis.bump_reason_rate;
  reduce_sqrt_ = config_.reduction.square_root_cadence;
  dynamic_tiers_ = config_.reduction.dynamic_tiers;
  sync_retier_profile();
  compact_ = config_.reduction.compact_variables;
  lucky_late_ = config_.rephase.late_lucky;
  chrono_levels = config_.analysis.chronological_levels;
  default_branching_.set_config(&config_.branching);
  default_branching_.randec_enabled = config_.branching.random_sequences;
  default_reduction_.set_config(&config_.reduction);
  default_reduction_.ramp = config_.reduction.ramp_fraction;
  inprocess_.set_config(&config_.inprocess);
  inprocess_.configure();
}

void DefaultInprocessScheduler::configure() {
  const auto &cfg = config();
  closure_delay_ = cfg.closure_delay;
  slot_clocks_split_ = cfg.split_clocks;
  slot_watermark_ = cfg.watermarks;
  couple_factor_bound_ = cfg.couple_factor_bound;
  factor_init_ticks_ = cfg.factor_initial_ticks;
  preprocess_followups_ = cfg.preprocess_followups;
  preprocess_only_ = cfg.preprocess_only;
}

void DefaultInprocessScheduler::reset(const Kernel &) {
  initialized_ = false;
  search_limits_initialized_ = false;
  last_reduce_conflicts_ = UINT64_MAX;
  probe_clock = elim_clock = {};
  // Kissat initializes both `last.ticks.probe` and
  // `last.ticks.eliminate` to zero.  Input-unit closure already uses the
  // ordinary search propagator and contributes to `search_ticks` before
  // solve entry, so rebasing these clocks here silently removes real reference
  // work from the first search-time Probe and Eliminate doses.  Initial
  // preprocessing deliberately does not advance the Probe anchor; completed
  // search-time phases update their own clock in `run_phase` below.
  bve_rounds_run_ = 0;
  factor_large_formula_ = false;
  factor_marked_watermark_ = 0;
  bve_level_ = 0;
  bve_level_idx_ = 0;
  initial_fast_elimination_active_ = false;
  initial_fast_elimination_ownership_ = {};
  initial_fast_elimination_discovery_ticks_ = 0;
  initial_fast_elimination_budget_exhausted_ = false;
  structured_root_consumed_ = false;
  structured_fastel_ownership_ = {};
  structured_root = {};
  default_candidate_order_.reset(config().candidate_order_seed);
  for (SlotEntry &e : entries) {
    e.limit = 0;
    e.applies = 0;
    e.doses = 0;
    e.cursor = 0;
    e.tm = {};
    e.ord = {};
    e.binaries_mark = 0;
    e.root_fixed_mark = 0;
    e.arena_mark = 0;
    e.delay_count = 0;
    e.delay_current = 0;
  }
  forward_pair_limit_ = 0;
}

void DefaultInprocessScheduler::install_parameters(Kernel &k, PolicyCompletion *completion) const {
  port::execute_action(k, execution::ParameterInstallationRequest{
                                   config().congruence_parameters}, completion, "Congruence");
  port::execute_action(k, execution::ParameterInstallationRequest{
                                   config().instantiate_parameters}, completion, "Instantiate");
  port::execute_action(k, execution::ParameterInstallationRequest{
                                   config().backbone_parameters}, completion, "Backbone");
  port::execute_action(k, execution::ParameterInstallationRequest{
                                   config().bce_parameters}, completion, "BCE");
  port::execute_action(k, execution::ParameterInstallationRequest{
                                   config().ternary_parameters}, completion, "HyperTernaryRes");
  port::execute_action(k, execution::ParameterInstallationRequest{
                                   config().factor_parameters}, completion, "Factor");
  port::execute_action(k, execution::ParameterInstallationRequest{
                                   config().transitive_parameters}, completion, "TransitiveReduce");
  port::execute_action(k, execution::ParameterInstallationRequest{
                                   config().substitute_parameters}, completion, "Substitute");
  port::execute_action(k, execution::ParameterInstallationRequest{
                                   config().vivify_parameters}, completion, "Vivify");
  // The scheduler owns these limits, while gate mask, growth ramp and physical
  // traversal retain the installed caller profile until their native occasion.
  strategy::BveParameters bve = strategy::BveParameters::from_kernel(k);
  bve.occurrence_limit = config().bve_occurrence_limit;
  bve.clause_size_limit = config().bve_clause_size_limit;
  bve.resolvent_count_limit = UINT32_MAX;
  bve.xor_arity_limit = config().bve_xor_arity_limit;
  bve.resolvent_size_limit = config().bve_resolvent_size_limit;
  bve.backward_occurrence_limit = config().bve_backward_occurrence_limit;
  bve.definition_budget = config().bve_definition_budget;
  bve.definition_cores = config().bve_definition_cores;
  bve.definition_core_budget_multiplier = config().bve_definition_core_budget_multiplier;
  bve.backward = config().bve_backward;
  bve.on_the_fly_strengthening = config().bve_on_the_fly_strengthening;
  port::execute_action(k, execution::ParameterInstallationRequest{bve}, completion, "BVE");
}

void DefaultInprocessScheduler::build(Kernel &k, PolicyCompletion *completion) {
  configure();
  if (entries.empty()) {
    const std::vector<RuleVTable> &rules = tactix_rules();
    const auto add_entry = [&](int i,
                               const strategy::InprocessStep *planned = nullptr) {
      {
        const char *nm = rules[i].name;
        // DefaultPolicy's Phase-4 reference is original Kissat.  Keep the
        // CaDiCaL-derived proof rules registered and explicitly poseable, but
        // do not splice them into Kissat's shipped probe/eliminate pipelines:
        // doing so changes the formula before the next common control choice.
        // Sweep remains in the reference pipeline and therefore ships on.
        const bool non_kissat_pipeline =
            !std::strcmp(nm, "HyperBinaryRes") ||
            !std::strcmp(nm, "Probe") ||
            !std::strcmp(nm, "HyperTernaryRes") ||
            !std::strcmp(nm, "Instantiate") || !std::strcmp(nm, "BCE") ||
            !std::strcmp(nm, "CCE");
        // Strengthen is the one fused Kissat forward pass: it subsumes and
        // self-subsumes under the shared allowance.  Keep standalone C1 fully
        // registered/poseable, but do not run the duplicated decomposition in
        // the reference-shaped default strategy.
        const bool duplicated_decomposition =
            !std::strcmp(nm, "SubsumeForward");
        bool on = planned ? planned->enabled
                          : !non_kissat_pipeline && !duplicated_decomposition;
        on = on &&
             !(!config().bve_enabled && !std::strcmp(nm, "BVE")) &&
             !(!config().vivify_enabled && !std::strcmp(nm, "Vivify")) &&
             !(!config().factor_enabled && !std::strcmp(nm, "Factor")) &&
             !(!config().sweep_enabled && !std::strcmp(nm, "Sweep"));
        SlotEntry e;
        e.step_id = planned ? planned->id : entries.size() + 1;
        e.defer_preprocess = !std::strcmp(nm, "BVE");
        e.rule = i;
        // Kissat transitive.c activity-sorts its variable stack and consumes
        // it from the back.  The dedicated comparator path above also retains
        // its exact IDX-on-variable reference behavior.
        if (!std::strcmp(nm, "TransitiveReduce") || !std::strcmp(nm, "BVE"))
          e.order = CandidateOrder::Heuristic;
        e.enabled = on;
        if (planned) {
          e.group = slot_group(planned->phase);
          e.preprocess = planned->preprocessing;
          e.followup = followup_kind(planned->followup);
          e.parameters = planned->parameters;
          e.max_applies = planned->max_applications;
          if (planned->candidate_order)
            e.order = *planned->candidate_order;
        } else {
          size_t norder = 0;
          const SlotOrderRow *order = default_slot_order(&norder);
          for (size_t o = 0; o < norder; o++)
            if (!std::strcmp(order[o].name, nm)) {
              e.group = order[o].group;
              break;
            }
        }
        // No production entry receives the generic closure watermark merely
        // because it is unmetered.  In particular, Kissat calls Congruence on
        // every admitted Probe phase subject only to its named productivity
        // delay; it has no binary/root "new material" admission gate.
        e.closure = false;
        if (const EffortSpec *s = effort_spec(nm)) {
          e.closure = s->closure;
          e.permille = config().*(s->permille);
          e.meter = s->meter;
          e.tick_counter = s->tick_counter;
          e.dosed = s->dosed;
          e.budget_aware = s->budget_aware;
          if (planned && planned->effort_permille)
            e.permille = *planned->effort_permille;
        }
        if (e.dosed)
          e.cursor_enabled = std::strcmp(nm, "BVE") != 0;
        if (planned && planned->cursor_enabled)
          e.cursor_enabled = *planned->cursor_enabled;
        entries.push_back(e);
      }
    };
    std::vector<bool> placed(rules.size(), false);
    std::string program_error;
    if (!strategy::validate_inprocess_program(config().program,
                                              &program_error)) {
      std::fprintf(stderr, "c ERROR: invalid inprocess program: %s\n",
                   program_error.c_str());
      std::abort();
    }
    // Resolve and validate every occurrence before constructing any runtime
    // entry, so a bad typed program cannot leave a usable prefix behind.
    for (const strategy::InprocessStep &step : config().program) {
      const int id = tactix_rule_id(step.rule.c_str());
      if (id < 0 || rules[id].tier != RuleTier::Inprocess) {
        std::fprintf(stderr,
                     "c ERROR: inprocess program rule '%s' is not a "
                     "registered inprocessing action\n",
                     step.rule.c_str());
        std::abort();
      }
      const EffortSpec *spec = effort_spec(step.rule.c_str());
      if (step.effort_permille && !spec) {
        std::fprintf(stderr,
                     "c ERROR: inprocess step '%s' has an effort share but "
                     "the action has no scheduler effort meter\n",
                     step.rule.c_str());
        std::abort();
      }
      if ((step.cursor_enabled || step.candidate_order) &&
          (!spec || !spec->dosed)) {
        std::fprintf(stderr,
                     "c ERROR: inprocess step '%s' configures candidate "
                     "traversal but the action runs as a batch\n",
                     step.rule.c_str());
        std::abort();
      }
    }
    for (const strategy::InprocessStep &step : config().program) {
      const int id = tactix_rule_id(step.rule.c_str());
      placed[id] = true;
      add_entry(id, &step);
    }
    if (config().append_unlisted_rules)
      for (int i = 0; i < static_cast<int>(rules.size()); i++)
        if (rules[i].tier == RuleTier::Inprocess &&
            rules[i].default_inprocess && !placed[i] &&
            std::strcmp(rules[i].name, "SubstitutionRedundancy") &&
            std::strcmp(rules[i].name, "Condition") &&
            std::strcmp(rules[i].name, "ParityConsequence") &&
            std::strcmp(rules[i].name, "AffineConsequence") &&
            std::strcmp(rules[i].name, "ExactCoverExtension") &&
            std::strcmp(rules[i].name, "IndependentSetCover") &&
            std::strcmp(rules[i].name, "Gf3AffineConsequence") &&
            std::strcmp(rules[i].name, "DirectCspSupport") &&
            std::strcmp(rules[i].name, "DirectPhpRefutation") &&
            std::strcmp(rules[i].name, "Gf3GaussianConsequence") &&
            std::strcmp(rules[i].name, "Gf3GeneralRowConsequence") &&
            std::strcmp(rules[i].name, "CardinalitySearch") &&
            std::strcmp(rules[i].name, "TseitinCycleBreaking") &&
            std::strcmp(rules[i].name, "NaryCspEquality") &&
            std::strcmp(rules[i].name, "NaryCspEqualityAmo") &&
            std::strcmp(rules[i].name, "NaryCspEqualityTransitivity") &&
            std::strcmp(rules[i].name, "GraphIntervalEquivalence") &&
            std::strcmp(rules[i].name, "AffineMatrixInitialize"))
          add_entry(i);
    default_candidate_order_.arms.assign(rules.size(),
                                         CandidateOrder::Native);
    for (const SlotEntry &se : entries)
      default_candidate_order_.arms[se.rule] = se.order;
  }
  // `eliminateocclim` is a Kissat option, so `variable_score` reads the same
  // bound wherever it runs, not only inside an elimination round.  Install it
  // when the scheduler attaches to the kernel: the candidate order is a public
  // action surface and may be consulted before the first phase, and the
  // kernel's unlimited default squares to a magnitude that cancels every
  // score difference in `kissat_bve_variable_score`.
  install_parameters(k, completion);
  port::execute_action(k, execution::MaterialSettingsRequest{
                                   config().touch_filter,
                                   config().likely_kept_redundant}, completion);
  if (!initialized_) {
    initialized_ = true;
    // Probe is due immediately because it owns initial preprocessing.
    // Eliminate is not part of that pipeline.  Kissat initializes its first
    // Eliminate deadline in start_search, after preprocessing has established
    // the formula population used by kissat_scale_delta.  Keep it unreachable
    // until initialize_search_limits observes that same lifecycle boundary.
    probe_clock.next_conflicts = k.stats.conflicts;
    elim_clock.next_conflicts = UINT64_MAX;
  }
}

void DefaultInprocessScheduler::build(Kernel &k) {
  build(k, nullptr);
}

void DefaultInprocessScheduler::initialize_search_limits(const Kernel &k) {
  if (search_limits_initialized_)
    return;
  // kimits.c:kissat_init_limits runs from search.c:start_search, after
  // preprocessing, late Lucky, and classification.  rounds==0 makes the
  // existing NLOG2N updater use n=1, i.e. eliminateinit=500 with the exact
  // post-preprocessing BINIRR_CLAUSES scaling population.
  update_limit(k, SlotGroup::Eliminate);
  search_limits_initialized_ = true;
}

strategy::controller::Observation DefaultPolicy::observe(const Kernel &k) const {
  using namespace strategy::controller;
  auto result = observe_kernel(k);
  const bool stable = k.h && k.h->stable;
  result.sequence = completion_ ? completion_->sequence() : 0;
  result.searching = completion_ && completion_->searching();
  if (completion_) result.last_action = completion_->last_action();
  result.have_search_configuration = completion_ &&
      completion_->have_search_configuration();
  result.search.branching = stable ? Branching::Evsids : Branching::Vmtf;
  result.search.restart = stable ? Restart::Reluctant : Restart::AdaptiveGlue;
  result.search.phase = stable ? Phase::Target
      : config_.branching.focused_phase_switch_span &&
        !config_.branching.focused_phase_pattern.empty() ? Phase::FocusedRotation : Phase::Saved;
  result.next_restart_conflicts = restart.next_conflicts;
  result.reluctant_u = reluctant.u;
  result.reluctant_v = reluctant.v;
  result.reluctant_wait = reluctant.wait;
  result.fast_glue = averages[stable].fast_glue.value;
  result.slow_glue = averages[stable].slow_glue.value;
  std::ostringstream native;
  native.imbue(std::locale::classic());
  native << std::setprecision(17) << "version=1 stable=" << stable
         << " mode=" << mode.count << ':' << mode.conflicts_limit << ':'
         << mode.ticks_limit << ':' << mode.ticks_at_focus_entry
         << " reluctant_trigger=" << reluctant.trigger
         << " reduce=" << reduce.next_conflicts
         << " rephase=" << rephase.count << ':' << rephase.next_conflicts
         << " reorder=" << reorder.count << ':' << reorder.next_conflicts
         << " randec=" << default_branching_.randec_remaining << ':'
         << default_branching_.randec_next << ':' << default_branching_.randec_sequences
         << ':' << default_branching_.randec_rng
         << " bump_delay=" << bump_reasons_delay_.count << ':' << bump_reasons_delay_.current
         << " retier=" << retier_.next_conflicts << ':' << retier_.interval
         << " rule-state=" << k.factor_completed << ':' << k.subsume_complete << ':'
         << k.strengthen_complete << ':' << k.sweep_incomplete
         << " material=" << k.touch_filter << ':' << k.touch_redundant
         << " work=" << k.work_budget << ':' << k.work_budget_offset << ':' << k.work_budget_bounded
         << " watches=" << k.large_watches_connected;
  for (const auto &average : averages) {
    native << " average=" << average.initialized << ':' << average.saved_decisions;
    for (const auto *smooth : {&average.fast_glue, &average.slow_glue,
                               &average.decision_rate})
      native << ':' << smooth->value << ':' << smooth->biased << ':' << smooth->alpha
             << ':' << smooth->beta << ':' << smooth->exp;
  }
  for (const auto *clock : {&inprocess_.probe_clock, &inprocess_.elim_clock})
    native << " inprocess_clock=" << clock->next_conflicts << ':' << clock->rounds
           << ':' << clock->reductions_mark << ':' << clock->marks_mark << ':'
           << clock->fixed_mark << ':' << clock->material_mark << ':' << clock->blocked
           << ':' << clock->blocked_episodes << ':' << clock->anchor;
  native << " inprocess_initialized=" << inprocess_.initialized();
  if (completion_) native << " last-native=" << completion_->last_native_measurement();
  for (const auto &entry : inprocess_.entries)
    native << " slot=" << entry.step_id << ':' << entry.enabled << ':' << entry.limit
           << ':' << entry.applies << ':' << entry.cursor << ':' << entry.arena_mark
           << ':' << entry.binaries_mark << ':' << entry.root_fixed_mark << ':'
           << entry.doses << ':' << entry.delay_count << ':' << entry.delay_current;
  result.native_policy_state = native.str();
  return result;
}

AffineMatrixInitResult DefaultPolicy::initialize_affine_search(Kernel &kernel,bool refresh,bool recognize) {
  AffineMatrixInitResult result;
  // Schedule only a public-eligible persistent action.  In particular, the
  // retained owner requires a live proof and provenance stream; asking the C
  // port to decline and then falling back statically would violate trajectory
  // identity even though both paths ultimately publish nothing.
  if (!affine_matrix_initialize_guard(kernel) || !kernel.proof ||
      kernel.model_extended) {
    result.status = AffineMatrixInitStatus::Inapplicable;
    return result;
  }
  // Root maintenance has no source owner to maintain after an empty admission.
  // Additional recognition still requires its ordinary-search-earned dose.
  if (refresh && !recognize &&
      (!kernel.affine_matrix_search_session->enabled() ||
       !kernel.affine_matrix_search_session->persistent())) {
    ++affine_search_diagnostics_.empty_maintenance_skips;
    return result;
  }
  const auto census_stamp = [&] {
    return std::array<uint64_t, 13>{kernel.arena_generation, kernel.touch_epoch,
        kernel.touch_fixed_head, kernel.next_clause_id,
        kernel.affine_semantic_namespace_epoch, kernel.arena.size(),
        kernel.nvars, kernel.external_to_internal.size(),
        kernel.proof ? kernel.proof->premise_epoch() : 0,
        static_cast<uint64_t>(reinterpret_cast<uintptr_t>(kernel.proof)),
        kernel.stats.root_assignments, kernel.affine_trail_retraction_epoch,
        kernel.trail.size()};
  };
  // A completed empty census is useful negative evidence only for this exact
  // source state. Learned additions and in-place simplifications invalidate it.
  if (refresh && recognize && kernel.affine_capture_epochs_trusted &&
      affine_completed_census_ && *affine_completed_census_ == census_stamp()) {
    ++affine_search_diagnostics_.unchanged_census_skips;
    result.status = AffineMatrixInitStatus::NoStructure;
    result.unchanged = true;
    return result;
  }
  const bool retained_maintenance = refresh && !recognize &&
      kernel.affine_matrix_search_session->enabled() &&
      kernel.affine_matrix_search_session->persistent();
  auto parameters = retained_maintenance
      ? std::optional{kernel.affine_matrix_search_session->parameters()}
      : default_affine_search_parameters(kernel, config_);
  if (!parameters) return result;
  const uint64_t bytes = config_.affine_search.maximum_retained_bytes
      ? config_.affine_search.maximum_retained_bytes
      : config_.affine_root_closure.maximum_journal_bytes;
  AffineMatrixPersistenceLimits persistence{
      bytes, parameters->maximum_proof_deletions, parameters->maximum_session_work};
  if (retained_maintenance) persistence=kernel.affine_matrix_search_session->persistence_limits();
  const auto survival_before = affine_survival_image(*kernel.affine_matrix_search_session);
  const uint64_t owner_before = kernel.affine_matrix_search_session->owner_version();
  const uint64_t started = now_nanos();
  if(refresh)++affine_search_diagnostics_.refreshes;
  else ++affine_search_diagnostics_.initializations;
  // Each phase owns an independent cap: lifecycle, trail, synchronization,
  // planner, source revalidation, and durable-session bookkeeping.
  affine_service_allowance_=parameters->maximum_snapshot_work;
  for (uint64_t cap : {parameters->basis_maximum_work,parameters->basis_maximum_work,
       parameters->basis_maximum_work,parameters->basis_maximum_work,
       parameters->maximum_session_work})
    affine_service_allowance_=saturated_sum(affine_service_allowance_,cap);
  port::execute_action(kernel, execution::ParameterInstallationRequest{*parameters},
                            completion_, "AffineMatrixInitialize");
  result = port::complete_action(
      completion_, FeedbackKind::Rule, "AffineMatrixInitialize", INSTANCE_CANONICAL,
      [&] {
        return execution::native_rule_feedback(
            kernel,"AffineMatrixInitialize",INSTANCE_CANONICAL,
            "default-persistent-authenticated-subimage;retained-bytes=" + std::to_string(persistence.maximum_retained_bytes),
            affine_init_work,"affine-initialization-work",
            strategy::WorkLimit::bounded(saturated_sum(
                saturated_sum(parameters->maximum_snapshot_work,parameters->maximum_elimination_work),
                saturated_sum(parameters->basis_maximum_work,
                    saturated_sum(parameters->maximum_session_work,parameters->basis_maximum_work)))));
      },
      [&] {
        auto initialized = refresh
            ? port::affine_matrix_search_refresh(kernel,*kernel.affine_matrix_search_session,
                *parameters,persistence,recognize)
            : port::affine_matrix_search_initialize(kernel,*kernel.affine_matrix_search_session,
                *parameters,AffineMatrixAdmission::AuthenticatedSubimage,persistence);
        uint64_t work = 0;
        for (uint64_t amount : {initialized.snapshot_work,initialized.recognition_work,
             initialized.matrix_work,initialized.premise_retention_work,initialized.root_unit_work,initialized.maintenance_service_work})
          work = saturated_sum(work,amount);
        affine_search_diagnostics_.initialization_work = saturated_sum(
            affine_search_diagnostics_.initialization_work,work);
        return initialized;
      },affine_init_outcome);
  if (const char *diagnostic=std::getenv("TACTIX_AFFINE_INIT_DIAG");
      diagnostic && diagnostic[0]=='1' && diagnostic[1]=='\0') {
    std::fprintf(stderr,
        "tactix-affine-init status=%u resource-stage=%u required-bytes=%llu limit-bytes=%llu snapshot-work=%llu recognition-work=%llu matrix-work=%llu retention-work=%llu equations=%llu admitted=%llu\n",
        unsigned(result.status),unsigned(result.resource_stage),
        (unsigned long long)result.resource_required_bytes,
        (unsigned long long)result.resource_limit_bytes,
        (unsigned long long)result.snapshot_work,
        (unsigned long long)result.recognition_work,
        (unsigned long long)result.matrix_work,
        (unsigned long long)result.premise_retention_work,
        (unsigned long long)result.equations,
        (unsigned long long)result.admitted_sources);
  }
  affine_search_diagnostics_.overhead_nanos += now_nanos()-started;
  if(result.root_service) {
    affine_search_diagnostics_.service_work=saturated_sum(
        affine_search_diagnostics_.service_work,result.maintenance_service_work);
    (void)record_affine_service_result(kernel,*result.root_service,true);
  }
  if (!refresh || recognize) {
    // Only remember a genuinely empty successful census. A nonempty owner
    // retains its own refresh/lifecycle protocol, including usefulness resets.
    if (kernel.affine_capture_epochs_trusted &&
        result.status == AffineMatrixInitStatus::NoStructure &&
        !kernel.affine_matrix_search_session->enabled())
      affine_completed_census_ = census_stamp();
    else if (result.status == AffineMatrixInitStatus::Applied ||
             result.status == AffineMatrixInitStatus::AppliedPartial)
      affine_completed_census_.reset();
  }
  if(result.unchanged)++affine_search_diagnostics_.unchanged_refreshes;
  affine_search_diagnostics_.root_unit_work+=result.root_unit_work;
  affine_search_diagnostics_.root_unit_additions+=result.root_unit_additions;
  if (!refresh || recognize) {
    affine_refresh_ticks_=search_ticks(kernel);
    affine_refresh_work_=0;
    for (uint64_t amount : {result.snapshot_work,result.recognition_work,result.matrix_work,
                           result.premise_retention_work,result.root_unit_work})
      affine_refresh_work_=saturated_sum(affine_refresh_work_,amount);
  }
  // Root maintenance reports the current owner image, not newly recognized
  // equations. Only an initialization/recognition dose earns census counters.
  if (!refresh || recognize) {
    affine_search_diagnostics_.recognized_equations += result.equations;
    affine_search_diagnostics_.admitted_sources += result.admitted_sources;
  }
  record_affine_survival(affine_search_diagnostics_, survival_before,
      *kernel.affine_matrix_search_session, (!refresh || recognize) ? 1 : 2);
  const bool applied = result.status == AffineMatrixInitStatus::Applied ||
                       result.status == AffineMatrixInitStatus::AppliedPartial;
  const bool admitted_change = applied && !result.unchanged &&
      (!refresh || recognize) && kernel.affine_matrix_search_session->enabled() &&
      kernel.affine_matrix_search_session->owner_version() != owner_before;
  // Maintenance completion is not new evidence of search usefulness. Only a
  // newly authenticated system or an actual new certified root fact starts
  // another sample; duplicate recognition retains its prior zero-yield pause.
  if (admitted_change || result.root_unit_additions) {
    if (affine_usefulness_paused_)
      ++affine_search_diagnostics_.usefulness_resumptions;
    affine_usefulness_calls_ = affine_usefulness_yield_ = affine_usefulness_checks_ = 0;
    affine_usefulness_paused_ = false;
  }
  return result;
}

AffineMatrixServiceResult DefaultPolicy::service_affine_search(Kernel &kernel,bool root) {
  AffineMatrixServiceResult result;
  if (!config_.affine_search.enabled || !kernel.affine_matrix_search_session ||
      !kernel.affine_matrix_search_session->enabled()) return result;
  const auto survival_before = affine_survival_image(*kernel.affine_matrix_search_session);
  const uint64_t started=now_nanos();
  result=port::complete_action(
      completion_,FeedbackKind::SearchService,"AffineMatrixSearch",INSTANCE_CANONICAL,
      [&] {
        execution::NativeFeedbackSpec spec;
        spec.meter=affine_service_work;
        spec.description.version=2;
        spec.description.description=root ? "default-persistent-restart-root"
                                          : "default-persistent-search";
        spec.description.effort_currency="affine-matrix-work";
        spec.description.effort_allowance=strategy::WorkLimit::bounded(
            affine_service_allowance_);
        spec.description.have_affine_matrix_event=true;
        return spec;
      },[&] {
        auto serviced=port::affine_matrix_search_step(kernel,*kernel.affine_matrix_search_session);
        uint64_t work=0;
        for (uint64_t amount : {serviced.trail_capture_work,serviced.synchronization_work,
             serviced.session_work,serviced.live.planner_work,serviced.live.revalidation_work,
             serviced.lifecycle_work}) work=saturated_sum(work,amount);
        affine_search_diagnostics_.service_work=saturated_sum(
            affine_search_diagnostics_.service_work,work);
        return serviced;
      },
      affine_service_outcome);
  affine_search_diagnostics_.overhead_nanos += now_nanos()-started;
  const bool optional_disable=record_affine_service_result(kernel,result,root);
  record_affine_survival(affine_search_diagnostics_, survival_before,
      *kernel.affine_matrix_search_session, root ? 3 : 4,
      result.projected_variables && !result.retired_source_groups, optional_disable);
  return result;
}

bool DefaultPolicy::affine_additional_recognition_due(
    const Kernel &kernel) const {
  // Once Default owns an authenticated persistent image, elapsed search work
  // only earns root maintenance/GE. It is not evidence that the source
  // population changed, and rescanning on that clock repeatedly revisits every
  // declined scope. Explicit recognition refreshes remain available to admit
  // newly installed authenticated sources until the kernel exposes a cheap
  // eligible-source addition epoch.
  if (kernel.affine_matrix_search_session &&
      kernel.affine_matrix_search_session->enabled() &&
      kernel.affine_matrix_search_session->persistent())
    return false;
  const uint64_t quantum = config_.affine_search.maximum_initialization_work
      ? config_.affine_search.maximum_initialization_work
      : config_.inprocess.preprocess_initial_ticks;
  const uint64_t ticks = search_ticks(kernel);
  return ticks >= affine_refresh_ticks_ &&
      ticks - affine_refresh_ticks_ >= std::max(quantum, affine_refresh_work_);
}

bool DefaultPolicy::record_affine_service_result(Kernel &kernel,
    const AffineMatrixServiceResult &result,bool root) {
  if(result.status==AffineMatrixServiceStatus::Pending)++affine_search_diagnostics_.pending_slices;
  affine_search_diagnostics_.resumed_slices+=result.slice_resumed;
  affine_search_diagnostics_.completed_slice_steps=saturated_sum(
      affine_search_diagnostics_.completed_slice_steps,result.slice_completed_steps);
  affine_search_diagnostics_.slice_work=saturated_sum(affine_search_diagnostics_.slice_work,result.slice_work);
  affine_search_diagnostics_.slice_cumulative_work=result.slice_cumulative_work;
  affine_search_diagnostics_.minimum_slice_work=result.slice_minimum_work;
  if(root)++affine_search_diagnostics_.root_services;
  else ++affine_search_diagnostics_.search_services;
  affine_search_diagnostics_.incremental_capture_calls+=result.incremental_capture;
  affine_search_diagnostics_.unchanged_support_calls+=result.unchanged_support;
  affine_search_diagnostics_.cached_event_calls+=result.synchronized && result.cached_event;
  affine_search_diagnostics_.capture_delta_literals=saturated_sum(
      affine_search_diagnostics_.capture_delta_literals,result.capture_delta_literals);
  affine_search_diagnostics_.lifecycle_work+=result.lifecycle_work;
  affine_search_diagnostics_.trail_capture_work+=result.trail_capture_work;
  affine_search_diagnostics_.synchronization_work+=result.synchronization_work;
  affine_search_diagnostics_.conditioned_rows+=result.synchronization_conditioned;
  affine_search_diagnostics_.retired_sources+=result.retired_source_groups;
  affine_search_diagnostics_.projected_variables+=result.projected_variables;
  if(result.synchronized && result.cached_event)++affine_search_diagnostics_.unchanged_reductions;
  if(result.status==AffineMatrixServiceStatus::NoEvent)++affine_search_diagnostics_.no_event;
  if(result.status==AffineMatrixServiceStatus::Duplicate)++affine_search_diagnostics_.duplicates;
  if(result.status==AffineMatrixServiceStatus::ResourceExhausted)++affine_search_diagnostics_.resource_declines;
  if(result.status==AffineMatrixServiceStatus::Applied) {
    ++affine_search_diagnostics_.reasons_published;
    affine_search_diagnostics_.implications_consumed +=
        result.live.implication_consumed;
    affine_search_diagnostics_.widest_reason=std::max<uint64_t>(
        affine_search_diagnostics_.widest_reason,result.installed_reason.size());
    affine_search_diagnostics_.proof_additions+=result.live.additions;
    affine_search_diagnostics_.proof_deletions+=result.live.deletions;
  }
  // A paused owner can still deliver a genuinely useful root consequence.
  // Its guarded lemma is consumed by ordinary BCP before further search.
  if (affine_usefulness_paused_ && !kernel.level &&
      result.status == AffineMatrixServiceStatus::Applied) {
    ++affine_search_diagnostics_.usefulness_resumptions;
    affine_usefulness_calls_ = affine_usefulness_yield_ = affine_usefulness_checks_ = 0;
    affine_usefulness_paused_ = false;
  }
  // Reference-style usefulness sampling counts charged noncached reductions.
  // Unchanged capture can still reduce after an interrupted synchronization.
  // A later work-earned root refresh may
  // admit a changed theory and begins a new sample round.
  if (!affine_usefulness_paused_ && result.synchronized &&
      result.synchronization_work && !result.cached_event) {
    ++affine_usefulness_calls_;
    if(result.status==AffineMatrixServiceStatus::Applied)++affine_usefulness_yield_;
    if(++affine_usefulness_checks_>=config_.affine_search.autodisable_check_every) {
      affine_usefulness_checks_=0;
      const long double required=std::floor(static_cast<long double>(affine_usefulness_calls_)*
          config_.affine_search.minimum_usefulness);
      if(config_.affine_search.autodisable &&
          affine_usefulness_calls_>config_.affine_search.autodisable_minimum_calls &&
          affine_usefulness_yield_<required) {
        ++affine_search_diagnostics_.usefulness_disabled;
        affine_usefulness_paused_ = true;
        affine_search_diagnostics_.paused_basis_rank = kernel.affine_matrix_search_session->basis_rank();
        affine_search_diagnostics_.paused_support_variables = kernel.affine_matrix_search_session->support_variables();
        // Keep the bounded authenticated owner and its leases. Erasing it
        // would make an identical later census look like a fresh admission,
        // and would lose independent root maintenance while search is paused.
      }
    }
  }
  // Ordinary CDCL continues after a finite optional-service decline. Repeated
  // resource failures do not spend the same allowance at every decision.
  const bool optional_disable = result.terminal_failure() &&
      result.status != AffineMatrixServiceStatus::Interrupted &&
      kernel.affine_matrix_search_session->enabled();
  if (optional_disable) port::affine_matrix_search_release(*kernel.affine_matrix_search_session, kernel);
  return optional_disable;
}

void DefaultPolicy::schedule_root_service_occurrence(
    const Kernel &k, DefaultRootServiceSchedule &s) const {
  // kimits.h UPDATE_CONFLICT_LIMIT(eliminate, eliminations, NLOG2N, true):
  // eliminateint * n * log10(n + 9)^2, then kissat_scale_delta.
  ++s.occurrences;
  const auto &config = config_.inprocess;
  const double n = static_cast<double>(s.occurrences);
  const double lg = std::log10(n + config.logarithm_offset);
  const uint64_t delta = scale_inprocessing_delta(
      k, static_cast<uint64_t>(static_cast<double>(config.eliminate_interval) * n * lg * lg),
      config);
  s.next_conflicts = saturated_sum(k.stats.conflicts, delta);
}

bool DefaultPolicy::root_service_delaying(const Kernel &k,
                                          DefaultRootServiceSchedule &s) const {
  // kimits.c kissat_delaying: a delayed occurrence still advances the
  // interval count, as Kissat's probe round does while congruence delays.
  if (!s.delay_count) return false;
  --s.delay_count;
  schedule_root_service_occurrence(k, s);
  return true;
}

static void judge_root_service_run(DefaultRootServiceSchedule &s, bool fruitful) {
  if (fruitful) {
    s.delay_current /= 2; // kimits.c kissat_reduce_delay
  } else {
    // Plan §9.5: doubling, more aggressive than kissat_bump_delay's +1.
    s.delay_current = !s.delay_current ? 1
        : s.delay_current > UINT32_MAX / 2 ? UINT32_MAX : 2 * s.delay_current;
  }
  s.delay_count = s.delay_current;
}

uint64_t DefaultPolicy::root_service_effort(const Kernel &k,
    const DefaultRootServiceSchedule &s, uint64_t permille) const {
  // kimits.h SET_EFFORT_LIMIT: per-mille share of the search ticks since the
  // last run, floored at mineffort; the initial preprocessing reference
  // matches DefaultInprocessScheduler::set_entry_limit at zero conflicts.
  uint64_t reference;
  if (!k.stats.conflicts) {
    reference = config_.inprocess.preprocess_initial_ticks;
  } else {
    const uint64_t now = search_ticks(k);
    reference = now >= s.last_ticks ? now - s.last_ticks : 0;
    if (reference < config_.inprocess.minimum_effort_ticks)
      reference = config_.inprocess.minimum_effort_ticks;
  }
  return scale_permille(reference, static_cast<uint32_t>(permille));
}

ApplyResult DefaultPolicy::run_affine_search(Kernel &k, bool startup) {
  // One run at the startup root or a natural full-restart root: retained
  // maintenance (restart only), then atomic Gaussian steps, each published
  // consequence followed by ordinary root BCP, until a fixpoint, an
  // unfinished slice or the run's effort limit. Step allowances are the
  // session's own; the limit is checked between steps, so a run overshoots
  // by at most one step allowance.
  if (k.level) std::abort();
  auto &d = affine_search_diagnostics_;
  const uint64_t effort = root_service_effort(k, affine_schedule_,
                                              config_.inprocess.sweep_permille);
  uint64_t spent = 0, published = 0;
  bool more = true;
  if (!startup) {
    const bool recognize = affine_additional_recognition_due(k);
    const uint64_t before = d.initialization_work;
    const auto refreshed = initialize_affine_search(k, true, recognize);
    spent = saturated_sum(spent, d.initialization_work - before);
    published += refreshed.root_unit_additions || refreshed.root_reason_additions;
    if ((refreshed.root_unit_additions || refreshed.root_reason_additions) &&
        default_propagate(k, "affine-restart-publication", completion_) != ApplyResult::Ok)
      return ApplyResult::Unsat;
    if (refreshed.root_service)
      more = refreshed.root_service->status == AffineMatrixServiceStatus::Applied ||
             refreshed.root_service->status == AffineMatrixServiceStatus::Pending;
  }
  while (more && k.affine_matrix_search_session &&
         k.affine_matrix_search_session->enabled() && spent < effort) {
    const uint64_t before = d.service_work;
    const auto service = service_affine_search(k, true);
    spent = saturated_sum(spent, d.service_work - before);
    ++d.run_steps;
    if (service.result == ApplyResult::Unsat) return ApplyResult::Unsat;
    if (service.status == AffineMatrixServiceStatus::Applied) {
      if (!service.bcp_pending) std::abort();
      ++published;
      if (default_propagate(k, startup ? "affine-persistent-root"
                                       : "affine-restart-publication",
                            completion_) != ApplyResult::Ok)
        return ApplyResult::Unsat;
      continue;
    }
    more = service.status == AffineMatrixServiceStatus::Pending;
  }
  d.run_work = saturated_sum(d.run_work, spent);
  d.run_effort_last = effort;
  d.run_effort_total = saturated_sum(d.run_effort_total, effort);
  // Fruitful = the run published output. A run cut by its effort limit or an
  // unfinished slice without output defers the verdict.
  if (published || !more) {
    judge_root_service_run(affine_schedule_, published != 0);
    ++(published ? d.fruitful_runs : d.fruitless_runs);
  }
  schedule_root_service_occurrence(k, affine_schedule_);
  affine_schedule_.last_ticks = search_ticks(k);
  d.run_conflict_limit = affine_schedule_.next_conflicts;
  d.delay_current = affine_schedule_.delay_current;
  // A run that reached its fixpoint has consumed its own support facts.
  if (!more && k.affine_matrix_search_session &&
      k.affine_matrix_search_session->enabled())
    affine_matrix_search_consume_inputs(k, *k.affine_matrix_search_session, affine_inputs_);
  return ApplyResult::Ok;
}

ApplyResult DefaultPolicy::affine_restart_service(Kernel &k) {
  // Natural full-restart root window, due at Kissat's Eliminate conflict
  // limit, which at expiry waits (eliminate.c) until the retained inputs
  // changed: a root fact on the support, a retired support variable, an
  // owner or premise-lease change, or an unfinished slice.
  auto *session = k.affine_matrix_search_session;
  if (k.level || !session || !session->enabled() ||
      k.stats.conflicts < affine_schedule_.next_conflicts)
    return ApplyResult::NoOp;
  auto &d = affine_search_diagnostics_;
  uint64_t scan = 0;
  const bool changed =
      affine_matrix_search_inputs_changed(k, *session, affine_inputs_, scan);
  d.trigger_work = saturated_sum(d.trigger_work, scan);
  if (!changed) {
    ++d.unchanged_skips;
    return ApplyResult::NoOp;
  }
  if (root_service_delaying(k, affine_schedule_)) {
    ++d.delayed_runs;
    d.run_conflict_limit = affine_schedule_.next_conflicts;
    return ApplyResult::NoOp;
  }
  // The cursor keeps the run's own support facts visible unless the run
  // reaches a fixpoint.
  affine_matrix_search_consume_inputs(k, *session, affine_inputs_);
  ++d.restart_runs;
  if (run_affine_search(k, false) == ApplyResult::Unsat) return ApplyResult::Unsat;
  if (k.affine_matrix_search_session && k.affine_matrix_search_session->enabled()) {
    const uint64_t consumed = affine_inputs_.root_scanned;
    affine_matrix_search_consume_inputs(k, *k.affine_matrix_search_session, affine_inputs_);
    affine_inputs_.root_scanned = std::min(affine_inputs_.root_scanned, consumed);
  }
  return ApplyResult::Ok;
}

void DefaultPolicy::reset_graph_labeling_schedule(const Kernel &kernel) {
  (void)kernel;
  graph_labeling_diagnostics_ = {};
  graph_labeling_schedule_ = {};
}

bool DefaultPolicy::graph_labeling_due(const Kernel &kernel) {
  // Due at Kissat's Eliminate conflict limit, after the fruitlessness delay.
  if (!config_.graph_labeling_model.enabled ||
      !kernel.graph_labeling_session.authenticated() ||
      kernel.graph_labeling_session.complete() ||
      kernel.stats.conflicts < graph_labeling_schedule_.next_conflicts)
    return false;
  if (root_service_delaying(kernel, graph_labeling_schedule_)) {
    ++graph_labeling_diagnostics_.delayed_runs;
    graph_labeling_diagnostics_.run_conflict_limit =
        graph_labeling_schedule_.next_conflicts;
    return false;
  }
  return true;
}

void DefaultPolicy::schedule_graph_labeling(
    const Kernel &kernel, const GraphLabelingSliceResult &result) {
  // A run that ends without a model exhausted its owner dose: fruitless.
  // An interrupted run defers the verdict.
  auto &d = graph_labeling_diagnostics_;
  ++d.runs;
  const bool fruitful = result.status == GraphLabelingSliceStatus::Sat;
  if (result.status != GraphLabelingSliceStatus::Interrupted) {
    judge_root_service_run(graph_labeling_schedule_, fruitful);
    ++(fruitful ? d.fruitful_runs : d.fruitless_runs);
  }
  schedule_root_service_occurrence(kernel, graph_labeling_schedule_);
  graph_labeling_schedule_.last_ticks = search_ticks(kernel);
  d.run_conflict_limit = graph_labeling_schedule_.next_conflicts;
  d.delay_current = graph_labeling_schedule_.delay_current;
}

GraphLabelingSliceResult DefaultPolicy::run_graph_labeling(Kernel &kernel) {
  // The run dose follows the owner: LymphoSAT's dispatcher runs its matched
  // antibandwidth solver to completion before the CDCL fallback
  // (winners/lymphosat-sc2026/green/lymphosat/src/solver/core/main.cpp,
  // solvers/antibandwidth/solver/meta.json). A run continues the trajectory
  // slice by slice, each within its serialized stage dose, until it
  // completes, finds a model or is interrupted.
  GraphLabelingSliceResult result;
  bool sliced = false;
  for (;;) {
    const GraphLabelingSliceResult slice = service_graph_labeling(kernel);
    if (sliced && slice.status == GraphLabelingSliceStatus::NoStructure) break;
    result = slice;
    sliced = true;
    if (result.status == GraphLabelingSliceStatus::Sat) break;
    if (result.status != GraphLabelingSliceStatus::Pending) break;
    if (kernel_terminating(kernel)) {
      result.status = GraphLabelingSliceStatus::Interrupted;
      break;
    }
  }
  if (sliced) schedule_graph_labeling(kernel, result);
  return result;
}

GraphLabelingAuthenticationResult
DefaultPolicy::authenticate_graph_labeling(Kernel &kernel) {
  GraphLabelingAuthenticationResult result;
  if (!config_.graph_labeling_model.enabled || kernel.level ||
      !graph_labeling_model_guard(kernel))
    return result;
  ++graph_labeling_diagnostics_.authentication_attempts;
  result = port::complete_action(
      completion_, FeedbackKind::Rule, "Lucky", LUCKY_GRAPH_AUTHENTICATE,
      [&] { return execution::native_rule_feedback(kernel, "Lucky",
          LUCKY_GRAPH_AUTHENTICATE, "graph-authenticate"); },
      [&] {
        // Registry dispatch of Lucky 26; the policy keeps the typed result.
        execution::GraphLabelingActionResult out;
        static const RuleVTable &lucky = registered_rule("Lucky");
        (void)port::apply_rule_structured(lucky, kernel, "26", [&] {
          out = port::execute_action(kernel, execution::GraphLabelingActionRequest{
              execution::GraphLabelingOperation::Authenticate,
              strategy::GraphLabelingParameters::from_kernel(kernel)});
          return out.result;
        });
        return out;
      },
      [](const execution::GraphLabelingActionResult &out) {
        return strategy::controller::feedback_outcome(out.result);
      }).authentication;
  graph_labeling_diagnostics_.authentication_work = saturated_sum(
      graph_labeling_diagnostics_.authentication_work, result.work);
  switch (result.status) {
  case GraphLabelingAuthenticationStatus::Authenticated:
    ++graph_labeling_diagnostics_.authentication_successes;
    break;
  case GraphLabelingAuthenticationStatus::Exhausted:
    ++graph_labeling_diagnostics_.authentication_exhausted;
    break;
  case GraphLabelingAuthenticationStatus::NoStructure:
    ++graph_labeling_diagnostics_.authentication_no_structure;
    break;
  case GraphLabelingAuthenticationStatus::Interrupted:
    ++graph_labeling_diagnostics_.authentication_interrupted;
    break;
  }
  return result;
}

GraphLabelingSliceResult DefaultPolicy::service_graph_labeling(Kernel &kernel) {
  GraphLabelingSliceResult result;
  if (!config_.graph_labeling_model.enabled ||
      !kernel.graph_labeling_session.authenticated() || kernel.level ||
      !graph_labeling_model_guard(kernel))
    return result;
  // One slice: the profile's serialized Window continuation dose (other
  // stages are bounded by their own stage work), within any caller budget.
  const uint64_t suggested =
      kernel.graph_labeling_session.recommended_continuation_work();
  const uint64_t allowance = kernel_has_work_budget(kernel)
      ? std::min(kernel.work_budget, suggested ? suggested : kernel.work_budget)
      : suggested;
  port::ScopedRuleWorkBudget work_budget(
      kernel, suggested
                  ? strategy::RuleWorkBudget{
                        strategy::RuleWorkBudgetMode::Bounded, allowance}
                  : strategy::RuleWorkBudget{});
  const strategy::GuidanceOutcomeSnapshot before(kernel);
  result = port::complete_action(
      completion_, FeedbackKind::Rule, "Lucky", LUCKY_GRAPH_CONTINUE,
      [&] { return execution::native_rule_feedback(kernel, "Lucky",
          LUCKY_GRAPH_CONTINUE, "graph-continue"); },
      [&] {
        // Registry dispatch of Lucky 27; the policy keeps the typed result.
        execution::GraphLabelingActionResult out;
        static const RuleVTable &lucky = registered_rule("Lucky");
        (void)port::apply_rule_structured(lucky, kernel, "27", [&] {
          out = port::execute_action(kernel, execution::GraphLabelingActionRequest{
              execution::GraphLabelingOperation::Continue,
              strategy::GraphLabelingParameters::from_kernel(kernel)});
          return out.result;
        });
        return out;
      },
      [&](const execution::GraphLabelingActionResult &out) {
        auto feedback = strategy::controller::feedback_outcome(out.result);
        feedback.invocation.status = before.refine(kernel, out.result, feedback.invocation.status);
        return feedback;
      }).slice;
  auto &d = graph_labeling_diagnostics_;
  ++d.slices;
  if (result.useful_hint || result.status == GraphLabelingSliceStatus::Sat)
    ++d.useful_hints;
  if (result.status == GraphLabelingSliceStatus::Pending ||
      result.status == GraphLabelingSliceStatus::Interrupted)
    ++d.pending_slices;
  if (result.status == GraphLabelingSliceStatus::TrajectoryComplete)
    ++d.completed_trajectories;
  return result;
}

int DefaultPolicy::solve(Kernel &k, volatile sig_atomic_t *stop) {
  KernelStopPublisher stop_published(k, stop);
  configure_from_env(k);
  progress_.start(k);
  // Completion ends while owned heuristic state is still attached, including
  // solve-entry and startup terminal paths. No mutable Kernel hook is installed.
  std::optional<ScopedHeuristics> owned_h;
  PolicyCompletion feedback(k, feedback_receiver_,
      [](const Kernel &kernel, const void *owner) {
        return static_cast<const DefaultPolicy *>(owner)->observe(kernel);
      }, this);
  struct CompletionLifetime {
    PolicyCompletion &feedback;
    PolicyCompletion *&slot;
    PolicyCompletion *previous;
    ~CompletionLifetime() {
      feedback.finish(0, StopReason::Detached);
      slot = previous;
    }
  } feedback_lifetime{feedback, completion_, completion_};
  completion_ = feedback.enabled() ? &feedback : nullptr;
  feedback.begin();
  // Kissat creation order is production state.  CaDiCaL's VMTF/phase arena
  // permutation remains available only through the explicit typed/environment
  // diagnostic arm; no formula classifier or benchmark selector controls it.
  ArenaOrdering *arena_ordering =
      config_.inprocess.cadical_arena_ordering_enabled
          ? inprocess_.cadical_arena_ordering()
          : nullptr;
  execution::ArenaOrderingScope arena_ordered(
      k, execution::ArenaOrderingInstallationRequest{arena_ordering});
  if (k.inconsistent) {
    feedback.finish(20, StopReason::Unsat);
    return 20;
  }
  // Solve-entry root normalization must retire an externally recorded conflict,
  // not only discard its decision trail. S1 Restart owns that transition
  // (docs/dev/calculus.md §4); startup inprocessing requires !conflict_valid.
  if (k.level || k.conflict_valid) {
    if (port::complete_action(completion_, FeedbackKind::Support, "Restart", 0,
            [&] { return execution::native_rule_feedback(k, "Restart", 0,
                "keep-level=0;purpose=preparation;solve-entry=1"); }, [&] {
              return port::execute_action(k, execution::RestartRequest{
                  0, execution::RestartPurpose::Preparation});
            }) == ApplyResult::Unsat)
      { feedback.finish(20, StopReason::Unsat); return 20; }
  }
  solves++;
  affine_search_diagnostics_={};
  affine_completed_census_.reset();
  const uint64_t alias_work_begin=k.stats.affine_matrix_alias_work;
  const uint64_t alias_additions_begin=k.stats.affine_matrix_alias_proof_additions;
  const uint64_t premise_maintenance_begin=k.proof ? k.proof->premise_maintenance_work() : 0;
  affine_refresh_ticks_=search_ticks(k);
  affine_refresh_work_=0;
  affine_inputs_={};
  affine_schedule_={};
  affine_usefulness_calls_=affine_usefulness_yield_=affine_usefulness_checks_=0;
  affine_usefulness_paused_=false;
  (void)port::complete_action(completion_, FeedbackKind::Support,
      "ResetGraphLabelingSession", INSTANCE_CANONICAL, [] {
        execution::NativeFeedbackSpec spec;
        spec.description.description = "per-solve";
        return spec;
      }, [&] {
        port::execute_action(k, execution::ResetGraphLabelingSessionRequest{});
        return ApplyResult::Ok;
      });
  reset_graph_labeling_schedule(k);
  strategy::AnalyzeParameters analysis = strategy::AnalyzeParameters::from_kernel(k);
  analysis.chronological_levels = chrono_levels;
  analysis.minimize_depth_limit = config_.analysis.minimize_depth_limit;
  analysis.conflict_reuse = config_.analysis.conflict_reuse;
  analysis.jump_reasons = config_.analysis.jump_reasons;
  analysis.shrink_glue_promote = config_.analysis.shrink_glue_promote;
  analysis.bump_extension_reasons = config_.analysis.bump_extension_reasons;
  analysis.eager_subsume = config_.analysis.eager_subsume;
  port::execute_action(k, execution::ParameterInstallationRequest{analysis}, completion_, "ConflictAnalysis");
  port::execute_action(k, execution::ParameterInstallationRequest{
                                   config_.reorder.parameters}, completion_, "Reorder");
  port::execute_action(k, execution::ParameterInstallationRequest{
                                   config_.parity_model.parameters}, completion_, "Lucky", LUCKY_PARITY_PROOF);
  port::execute_action(k, execution::ParameterInstallationRequest{
                                   config_.onehot_model.parameters}, completion_, "Lucky", LUCKY_EXACT_ONE_PROOF);
  port::execute_action(k, execution::ParameterInstallationRequest{
                                   config_.affine_model.parameters}, completion_, "Lucky", 8);
  port::execute_action(
      k,
      execution::ParameterInstallationRequest{
          config_.balanced_signature_model.parameters},
      completion_, "Lucky", 10);
  port::execute_action(k, execution::ParameterInstallationRequest{
                                   config_.fermat_model.parameters}, completion_, "Lucky", 11);
  port::execute_action(k, execution::ParameterInstallationRequest{
                                   config_.lookup_circuit_model.parameters}, completion_, "Lucky", 13);
  port::execute_action(k, execution::ParameterInstallationRequest{
                                   config_.simon_model.parameters}, completion_, "Lucky", 14);
  port::execute_action(k, execution::ParameterInstallationRequest{
                                   config_.stconn_proof.parameters}, completion_, "Lucky", 15);
  port::execute_action(k, execution::ParameterInstallationRequest{
                                   config_.signed_cardinality_proof.parameters}, completion_, "Lucky", 16);
  port::execute_action(k, execution::ParameterInstallationRequest{
                                   config_.exact_cover_model.parameters}, completion_, "Lucky", 17);
  port::execute_action(k, execution::ParameterInstallationRequest{
                                   config_.independent_set_model.parameters}, completion_, "Lucky", 18);
  port::execute_action(k, execution::ParameterInstallationRequest{
                                   config_.graph_labeling_model.parameters}, completion_, "Lucky",
                            LUCKY_GRAPH_LABELING);
  port::execute_action(k, execution::ParameterInstallationRequest{
                                   config_.ramsey_startup.parameters}, completion_, "Lucky", LUCKY_STARTUP_RAMSEY);
  strategy::ReduceParameters reduce = strategy::ReduceParameters::from_kernel(k);
  reduce.victim_numerator = config_.reduction.victim_numerator;
  reduce.victim_denominator = config_.reduction.victim_denominator;
  reduce.protect_tiers = config_.reduction.protect_tiers;
  reduce.compact_collection = config_.reduction.canonical_compact_collection;
  reduce.reduction_bookkeeping = config_.reduction.bookkeeping;
  port::execute_action(k, execution::ParameterInstallationRequest{reduce}, completion_, "Reduce");
  port::execute_action(k, execution::WalkGuidanceInstallationRequest{
                                   config_.rephase.walk_even_break_base_milli,
                                   config_.rephase.walk_score_curve}, completion_);
  if (reset_schedule_per_solve) {
    for (uint32_t mode = 0; mode != 2; ++mode) {
      retier_.tier1_by_mode[mode] = config_.reduction.tier1_default;
      retier_.tier2_by_mode[mode] = std::max(config_.reduction.tier1_default,
                                             config_.reduction.tier2_default);
    }
    pose_vivify_tiers(k, retier_, config_.inprocess, completion_);
    reset_schedule(k);
  }
  inprocess_.build(k, completion_);
  EventTrace event_trace;
  event_trace.open_from_env(k);
  // Paired search-time deletion counters on every analysis_end row: the
  // clause a solver deletes between two boundaries is invisible in the
  // journals, and kissat's rows carry the same three statistics.
#ifdef TACTIX_ENABLE_EVENT_TRACE
  const auto event_trace_analysis_deletions = [&](const Kernel &kk) {
    event_trace.detail(kk, "analysis_eager_subsumed",
                       static_cast<int64_t>(kk.stats.eager_subsumed));
    event_trace.detail(kk, "analysis_otfs_strengthened",
                       static_cast<int64_t>(kk.stats.otfs_strengthened));
    event_trace.detail(kk, "analysis_otfs_subsumed",
                       static_cast<int64_t>(kk.stats.otfs_subsumed));
  };
#else
  const auto event_trace_analysis_deletions = [](const Kernel &) {};
#endif
  event_trace.macro(k, "startup_begin");
  auto finish = [&](int verdict) {
    progress_.finish(k, verdict);
    // These source/proof hook costs overlap ordinary structural actions and
    // are diagnostics, not another charge of the whole Substitute invocation.
    affine_search_diagnostics_.alias_work=k.stats.affine_matrix_alias_work-alias_work_begin;
    affine_search_diagnostics_.alias_proof_additions=
        k.stats.affine_matrix_alias_proof_additions-alias_additions_begin;
    if(k.proof) {
      const uint64_t work=k.proof->premise_maintenance_work();
      affine_search_diagnostics_.premise_maintenance_work=work>=premise_maintenance_begin ? work-premise_maintenance_begin : work;
    }
    char payload[16];
    std::snprintf(payload, sizeof payload, "%d", verdict);
    event_trace.macro(k, "terminate", payload);
    event_trace.close();
    feedback.finish(verdict, verdict == 10 ? StopReason::Sat
                            : verdict == 20 ? StopReason::Unsat
                                            : StopReason::Interrupted);
    return verdict;
  };

  std::optional<RecordedExecutionContext<execution::ExecutionContext::Startup>>
      presearch_ticks;
  presearch_ticks.emplace(k, completion_);

  // The references expose two distinct Lucky opportunities around initial
  // preprocessing. Keep them out of the CDCL loop so neither can repeat after
  // a later inprocessing phase.
  if (config_.ramsey_startup.enabled && ramsey_startup_guard(k)) {
    const ApplyResult ramsey = default_lucky_opportunity<LuckyOpportunity::StartupRamsey>(
        k, "startup-ramsey-dsr", completion_);
    if (ramsey == ApplyResult::Unsat) return finish(20);
  }
  if (startup_parity_guard(k)) {
    const ApplyResult pb = default_lucky_opportunity<LuckyOpportunity::StartupParity>(
        k, "startup-parity-pb", completion_);
    if (pb == ApplyResult::Unsat)
      return finish(20);
  }
  if (count_preservation_guard(k)) {
    const ApplyResult counting = default_lucky_opportunity<LuckyOpportunity::StartupCount>(
        k, "startup-count-pb", completion_);
    if (counting == ApplyResult::Unsat)
      return finish(20);
  }
  (void)port::complete_action(completion_, FeedbackKind::Support,
      "CloseStartupCertificate", INSTANCE_CANONICAL, [] {
        execution::NativeFeedbackSpec spec;
        spec.description.description = "close-original-input-opportunity;commit-native-dsr";
        return spec;
      }, [&] {
        port::execute_action(k, execution::CloseStartupCertificateRequest{});
        return ApplyResult::NoOp;
      });
  ApplyResult startup = default_propagate(k, "startup", completion_);
  if (startup != ApplyResult::Ok)
    return finish(20); // startup is at root: a conflict is a refutation
  const auto graph_authentication = authenticate_graph_labeling(k);
  if (graph_authentication.status ==
      GraphLabelingAuthenticationStatus::Interrupted)
    return finish(0);
  const ApplyResult affine_root = default_affine_root_closure(
      k, config_.affine_root_closure, completion_);
  if (affine_root == ApplyResult::Unsat ||
      affine_root == ApplyResult::Conflict)
    return finish(20);
  if (config_.signed_cardinality_proof.enabled &&
      signed_cardinality_proof_guard(k)) {
    event_trace.macro(k, "signed_cardinality_proof_begin");
    const ApplyResult counting = default_lucky_opportunity<LuckyOpportunity::SignedCardinality>(
        k, "signed-cardinality-proof", completion_);
    if (counting == ApplyResult::Unsat) {
      event_trace.macro(k, "signed_cardinality_proof_end", "unsat");
      return finish(20);
    }
    event_trace.macro(k, "signed_cardinality_proof_end", "unknown");
  }
  // Signed at-most-one block exact cover (the sgen owner): exact recognition
  // on the live clause set, then bounded search and whole-formula model check.
  if (config_.exact_cover_model.enabled && exact_cover_model_guard(k)) {
    const ApplyResult cover =
        default_lucky_opportunity<LuckyOpportunity::ExactCover>(
            k, "exact-cover-model", completion_);
    if (cover == ApplyResult::Sat)
      return finish(10);
  }
  // Independent set under a layered exact-count encoding: construct and
  // complete a candidate, then accept only a whole-formula validated model.
  if (config_.independent_set_model.enabled &&
      independent_set_model_guard(k)) {
    const ApplyResult independent =
        default_lucky_opportunity<LuckyOpportunity::IndependentSet>(
            k, "independent-set-model", completion_);
    if (independent == ApplyResult::Sat)
      return finish(10);
  }
  if (config_.stconn_proof.enabled && stconn_proof_guard(k)) {
    event_trace.macro(k, "stconn_proof_begin");
    const ApplyResult stconn = default_lucky_opportunity<LuckyOpportunity::Stconn>(
        k, "stconn-proof", completion_);
    if (stconn == ApplyResult::Unsat) {
      event_trace.macro(k, "stconn_proof_end", "unsat");
      return finish(20);
    }
    event_trace.macro(k, "stconn_proof_end", "unknown");
  }
  // The LymphoSAT Oddball owner constructs balanced ternary signatures at the
  // untouched imported root. A complete candidate is checked against the
  // entire live CNF before publication; an incomplete search
  // publishes only its partial assignment as later Walk/CDCL guidance.
  if (config_.balanced_signature_model.enabled &&
      balanced_signature_model_guard(k)) {
    event_trace.macro(k, "balanced_signature_model_begin");
    const ApplyResult balanced =
        default_lucky_opportunity<LuckyOpportunity::BalancedSignature>(
            k, "balanced-signature-model", completion_);
    if (balanced == ApplyResult::Sat) {
      event_trace.macro(k, "balanced_signature_model_end", "sat");
      return finish(10);
    }
    event_trace.macro(k, "balanced_signature_model_end", "unknown");
  }
  // SAT-only LymphoSAT owners retain their bounded theory-specific model
  // constructors. Every completed assignment is checked against the whole live
  // CNF before publication; exhaustion remains unknown.
  if (config_.fermat_model.enabled && fermat_model_guard(k)) {
    event_trace.macro(k, "fermat_model_begin");
    const ApplyResult result = default_lucky_opportunity<LuckyOpportunity::Fermat>(
        k, "fermat-model", completion_);
    if (result == ApplyResult::Sat) {
      event_trace.macro(k, "fermat_model_end", "sat");
      return finish(10);
    }
    event_trace.macro(k, "fermat_model_end", "unknown");
  }
  if (config_.cerny_model.enabled && cerny_model_guard(k)) {
    event_trace.macro(k, "cerny_model_begin");
    const ApplyResult result = default_lucky_opportunity<LuckyOpportunity::Cerny>(
        k, "cerny-model", completion_);
    if (result == ApplyResult::Sat) {
      event_trace.macro(k, "cerny_model_end", "sat");
      return finish(10);
    }
    event_trace.macro(k, "cerny_model_end", "unknown");
  }
  if (config_.lookup_circuit_model.enabled && lookup_circuit_model_guard(k)) {
    event_trace.macro(k, "lookup_circuit_model_begin");
    const ApplyResult result =
        default_lucky_opportunity<LuckyOpportunity::LookupCircuit>(
            k, "lookup-circuit-model", completion_);
    if (result == ApplyResult::Sat) {
      event_trace.macro(k, "lookup_circuit_model_end", "sat");
      return finish(10);
    }
    event_trace.macro(k, "lookup_circuit_model_end", "unknown");
  }
  if (config_.simon_model.enabled && simon_model_guard(k)) {
    event_trace.macro(k, "simon_model_begin");
    const ApplyResult result = default_lucky_opportunity<LuckyOpportunity::Simon>(
        k, "simon-model", completion_);
    if (result == ApplyResult::Sat) {
      event_trace.macro(k, "simon_model_end", "sat");
      return finish(10);
    }
    event_trace.macro(k, "simon_model_end", "unknown");
  }
  // The proof-only parity action keeps the constructor's exact recognizer and
  // certificate path without entering its private assignment search.
  if (parity_model_guard(k)) {
    event_trace.macro(k, "parity_proof_begin");
    const ApplyResult parity =
        default_lucky_opportunity<LuckyOpportunity::ParityProof>(
            k, "parity-proof", completion_);
    if (parity == ApplyResult::Unsat) {
      event_trace.macro(k, "parity_proof_end", "unsat");
      return finish(20);
    }
    event_trace.macro(k, "parity_proof_end", "unknown");
  }
  if (config_.parity_model.enabled && parity_model_guard(k)) {
    event_trace.macro(k, "parity_model_begin");
    const ApplyResult parity =
        default_lucky_opportunity<LuckyOpportunity::Parity>(
            k, "parity-model", completion_);
    if (parity == ApplyResult::Sat) {
      event_trace.macro(k, "parity_model_end", "sat");
      return finish(10);
    }
    event_trace.macro(k, "parity_model_end", "unknown");
  }
  // The exact-one proof compares a complete live incidence system with the
  // parser's stable binary order. Run it while both still describe the same
  // imported formula, before structured-root rules may add helpers or retire
  // clauses. Its count-only preflight keeps every decline proof-atomic.
  if (onehot_model_guard(k)) {
    event_trace.macro(k, "exact_one_refutation_begin");
    const ApplyResult exact_one = port::complete_action(completion_, FeedbackKind::Rule,
        "Lucky", LUCKY_EXACT_ONE_PROOF, [&] {
          return execution::native_rule_feedback(
              k, "Lucky", LUCKY_EXACT_ONE_PROOF,
            "native-exact-one-refutation-opportunity"); }, [&] {
          static const RuleVTable &lucky = registered_rule("Lucky");
          assert(kernel_propagation_fixpoint(k)); // its root closure is a no-op
          return port::apply_rule_instance(lucky, k, LUCKY_EXACT_ONE_PROOF);
        });
    if (exact_one == ApplyResult::Unsat) {
      event_trace.macro(k, "exact_one_refutation_end", "unsat");
      return finish(20);
    }
    event_trace.macro(k, "exact_one_refutation_end", "unknown");
  }
  bool structured_root_grew_trail = false;
  uint64_t structured_root_amo_binaries_added = 0;
  bool structured_root_graph_root_progress = false;
  std::vector<Var> residual_import_order;
  // Symmetry detection may depend on adjacency insertion order. Schedule the
  // shared structured program on the imported root before speculative Lucky
  // propagation can rearrange watches. The reference Probe-entry order remains
  // an explicit strategy choice; empty/compatibility programs retain the cold
  // Lucky fast path.
  if (config_.inprocess.structured_root_before_lucky &&
      !config_.inprocess.structured_root_stages.empty() &&
      (!k.proof || k.proof->supports_sr())) {
    port::execute_action(k, execution::MaterializeColdStateRequest{}, completion_);
    const uint64_t amo_binaries_before =
        k.stats.sr_unique_literal_amo_binaries;
    const uint64_t post_amo_progress_before =
        k.stats.sr_post_amo_nonhelper_applied;
    const uint64_t post_amo_graph_root_units_before =
        k.stats.sr_post_amo_graph_root_units;
    const size_t structured_trail_before = k.trail.size();
    const uint64_t structured_started = now_nanos();
    const int structured_result = inprocess_.run_structured_root(k, stop, completion_);
    const uint64_t structured_elapsed = now_nanos() - structured_started;
    k.stats.structured_root_nanos += structured_elapsed;
    k.stats.inprocess_nanos += structured_elapsed;
    structured_root_grew_trail = k.trail.size() > structured_trail_before;
    if (structured_result == 10 || structured_result == 20)
      return finish(structured_result);
    if ((stop && *stop) || (k.stop && *k.stop))
      return finish(0);
    const uint64_t amo_binaries_added =
        k.stats.sr_unique_literal_amo_binaries - amo_binaries_before;
    structured_root_amo_binaries_added = amo_binaries_added;
    // ULC completion is useful in three distinguishable ways.  If generic
    // graph symmetry consumes the completed BIG and fixes a root literal,
    // retain the pairs as input structure for search: the fixing is durable
    // evidence that completion exposed a useful subgroup.  If later progress
    // does not do that, the helpers have served their discovery purpose and
    // one canonical BCE pass may remove them.  If completion was the only
    // progress, retain the pairs instead of paying for a formula round trip
    // which leaves only scheduler/heuristic perturbation.  These are producer
    // outcomes, independent of formula size or benchmark identity.
    const bool amo_enabled_later_progress =
        k.stats.sr_post_amo_nonhelper_applied != post_amo_progress_before;
    const bool amo_enabled_graph_root_progress =
        k.stats.sr_post_amo_graph_root_units !=
        post_amo_graph_root_units_before;
    structured_root_graph_root_progress =
        amo_enabled_graph_root_progress;
    if (config_.inprocess.structured_root_bce_after_unique_literal_amo &&
        default_should_cleanup_unique_literal_amo(
            amo_binaries_added, amo_enabled_later_progress,
            amo_enabled_graph_root_progress)) {
      if (inprocess_reach_root(k))
        return finish(20);
      if (bce_guard(k)) {
        const ApplyResult bce = port::complete_action(completion_,
            FeedbackKind::Rule, "BCE", INSTANCE_CANONICAL, [&] {
              return execution::native_rule_feedback(k, "BCE", INSTANCE_CANONICAL,
                  "post-structured-amo", [](const Kernel &kernel) {
                    return kernel.stats.bce_ticks;
                  }, "bce-ticks");
            }, [&] {
              InprocessScope scope(k);
              static const RuleVTable &bce = registered_rule("BCE");
              return port::apply_rule(bce, k);
            }, [&](ApplyResult result) {
              return execution::native_rule_outcome(k, "BCE", result);
            });
        if (bce == ApplyResult::Unsat)
          return finish(20);
      }
      if ((stop && *stop) || (k.stop && *k.stop))
        return finish(0);
    }
  }
  if (config_.onehot_model.enabled && onehot_model_guard(k)) {
    event_trace.macro(k, "direct_csp_model_begin");
    const ApplyResult direct_csp =
        default_lucky_opportunity<LuckyOpportunity::DirectCsp>(
            k, "direct-csp-model", completion_);
    if (direct_csp == ApplyResult::Sat) {
      event_trace.macro(k, "direct_csp_model_end", "sat");
      return finish(10);
    }
    event_trace.macro(k, "direct_csp_model_end", "unknown");
  }
  if (k.input_binary_order_captured)
    (void)port::complete_action(completion_, FeedbackKind::Support,
        "ReleaseInputBinaryOrder", INSTANCE_CANONICAL, [] {
          execution::NativeFeedbackSpec spec;
          spec.description.description = "after-original-input-opportunities";
          return spec;
        }, [&] {
          port::execute_action(k, execution::ReleaseInputBinaryOrderRequest{});
          return ApplyResult::Ok;
        });
  if (lucky_guard(k)) {
    event_trace.macro(k, "lucky_early_begin");
    k.stats.lucky_early_calls++;
    const ApplyResult lucky = default_lucky(k, "early", completion_);
    if (lucky == ApplyResult::Unsat) {
      event_trace.macro(k, "lucky_early_end", "unsat");
      return finish(20);
    }
    if (lucky == ApplyResult::Sat) {
      event_trace.macro(k, "lucky_early_end", "sat");
      return finish(10);
    }
    event_trace.macro(k, "lucky_early_end", "unknown");
  }
  if (config_.onehot_model.enabled && onehot_model_guard(k)) {
    event_trace.macro(k, "onehot_model_begin");
    const ApplyResult onehot =
        default_lucky_opportunity<LuckyOpportunity::Onehot>(
            k, "onehot-model", completion_);
    if (onehot == ApplyResult::Sat) {
      event_trace.macro(k, "onehot_model_end", "sat");
      return finish(10);
    }
    event_trace.macro(k, "onehot_model_end", "unknown");
  }
  // The shipped frontend postpones state that neither root propagation nor
  // early Lucky can read. A failed opportunity crosses the first consumer
  // boundary: materialize before sparse collection and every authored rule.
  port::execute_action(k, execution::MaterializeColdStateRequest{}, completion_);
  // Search guidance is likewise unobservable to startup propagation and Lucky.
  // Attach it only after the early opportunity fails. ScopedHeuristics consumes
  // the parser's retained activation order, preserving the exact first-search
  // pose; callers that supplied H keep their existing session-wide object.
  // Optionally reseed from the live post-structured formula. Its internal
  // clause presentation need not match an external residual serialization.
  // The default retains the parser's original activation and import seed.
  if (!k.h &&
      default_should_seed_structured_residual(
          structured_root_grew_trail,
          structured_root_amo_binaries_added,
          structured_root_graph_root_progress) &&
      config_.inprocess.structured_root_residual_search_handoff) {
    (void)port::complete_action(completion_, FeedbackKind::Support,
        "PrepareResidualGuidance", INSTANCE_CANONICAL, [] {
          execution::NativeFeedbackSpec spec;
          spec.description.description = "prepare-first-search-activation-seed";
          spec.description.effort_currency = "activation-variables";
          spec.meter = [](const Kernel &kernel) { return kernel.stats.residual_handoff_variables; };
          return spec;
        }, [&] {
          const uint64_t started = now_nanos();
          k.stats.residual_handoff_variables +=
              port::execute_action(k, execution::ResidualActivationRequest{});
          residual_import_order = k.input_activation_order;
          k.stats.residual_handoff_nanos += now_nanos() - started;
          ++k.stats.residual_handoff_calls;
          return ApplyResult::Ok;
        });
  }
  if (!k.h)
    (void)port::complete_action(completion_, FeedbackKind::Support,
        "AttachHeuristics", INSTANCE_CANONICAL, [] {
          execution::NativeFeedbackSpec spec;
          spec.description.description = "fresh;activation-storage=shrink";
          spec.description.effort_currency = "ticks";
          spec.meter = [](const Kernel &kernel) { return kernel.stats.ticks; };
          return spec;
        }, [&] {
          owned_h.emplace(k);
          return ApplyResult::Ok;
        });
  if (!residual_import_order.empty())
    (void)port::complete_action(completion_, FeedbackKind::Support,
        "SeedResidualImportOrder", INSTANCE_CANONICAL, [&] {
          execution::NativeFeedbackSpec spec;
          spec.description.description = "variables=" + std::to_string(residual_import_order.size());
          spec.description.effort_currency = "ticks";
          spec.meter = [](const Kernel &kernel) { return kernel.stats.ticks; };
          return spec;
        }, [&] {
          port::execute_action(k, execution::ResidualImportRequest{residual_import_order});
          return ApplyResult::Ok;
        });
  event_trace.macro(k, "preprocess_begin");
  // Kissat preprocess.c initially propagates, then consumes those root units
  // by sparse-collecting the whole large-clause arena before the first Probe
  // technique. This is lifecycle scaffolding, not an authored Strengthen slot.
  RootCollectOutcome collected;
  do {
#ifdef TACTIX_ENABLE_EVENT_TRACE
    for (Lit l = 0; l < static_cast<Lit>(2 * k.nvars); l++)
      event_trace_substitute_map(k, l, l);
#endif
    event_trace_slot_boundary(k, "SparseCollect", "begin");
    collected = port::execute_action(k, execution::SparseCollectRequest{}, completion_);
    event_trace_slot_boundary(k, "SparseCollect", "end");
    if (collected == RootCollectOutcome::Refuted)
      return finish(20);
    if (collected == RootCollectOutcome::Unit) {
      const ApplyResult root_result = default_propagate(k, "root-collect", completion_);
      if (root_result == ApplyResult::Unsat)
        return finish(20);
    }
  } while (collected == RootCollectOutcome::Unit);
  // Kissat's initial sparse collection compacts every fixed internal variable
  // onto one representative.  Tactix preserves kernel identities for its
  // universal rule surface, but the random brancher's index permutation is
  // policy state and must use the same compacted population and ordering.
  event_trace_target_state(k, "startup-compact-pre",
                           static_cast<uint32_t>(k.trail.size()));
  (void)port::complete_action(completion_, FeedbackKind::Support,
      "CompactRandomOrder", INSTANCE_CANONICAL, [] {
        execution::NativeFeedbackSpec spec;
        spec.description.description = "fold-root-fixed-branching-population";
        spec.description.effort_currency = "ticks";
        spec.meter = [](const Kernel &kernel) { return kernel.stats.ticks; };
        return spec;
      }, [&] {
        port::execute_action(k, execution::CompactRandomOrderRequest{});
        return ApplyResult::Ok;
      });
  event_trace_target_state(
      k, "startup-compact-post",
      static_cast<uint32_t>(k.compact_import_order.size() -
                            k.unassigned_active));
#ifdef TACTIX_ENABLE_EVENT_TRACE
  for (Var v = 0; v < k.nvars; v++) {
    uint32_t ordinal = 0;
    for (uint32_t sign = 0; sign < 2; sign++)
      for (const WatchEntry w : k.watches.entries(make_lit(v, sign != 0)))
        if (w.binary())
          event_trace_sweep_watch(k, v, ordinal++, w.other());
  }
#endif
  if (inprocess_.want_phase(k, SlotGroup::Probe)) {
    const int phase_result = inprocess_.run_phase(k, SlotGroup::Probe, stop, completion_);
    if (phase_result == 10 || phase_result == 20) return finish(phase_result);
  }
  if (config_.inprocess.initial_fast_elimination) {
    event_trace.macro(k, "initial_fast_eliminate_begin");
    const int phase_result =
        inprocess_.run_initial_fast_elimination(
            k, stop, completion_, inprocess_.structured_fastel_ownership());
    event_trace.macro(k, "initial_fast_eliminate_end");
    if (phase_result == 10 || phase_result == 20)
      return finish(phase_result);
  }
  event_trace.macro(k, "preprocess_end");
  startup = default_propagate(k, "post-preprocess", completion_);
  if (startup != ApplyResult::Ok)
    return finish(20);
  // CryptoMiniSat's startup simplification finds XORs after variable
  // elimination ("occ-bve,occ-xor", solverconf.cpp simplify_schedule_startup).
  if(config_.affine_search.enabled) {
    (void)initialize_affine_search(k);
    if(k.affine_matrix_search_session && k.affine_matrix_search_session->enabled() &&
       run_affine_search(k,true)==ApplyResult::Unsat)
      return finish(20);
  }
  if (lucky_late_ && lucky_guard(k)) {
    event_trace.macro(k, "lucky_late_begin");
    k.stats.lucky_late_calls++;
    const ApplyResult lucky = default_lucky(k, "late", completion_);
    if (lucky == ApplyResult::Unsat) {
      event_trace.macro(k, "lucky_late_end", "unsat");
      return finish(20);
    }
    if (lucky == ApplyResult::Sat) {
      k.stats.lucky_late_sat++;
      event_trace.macro(k, "lucky_late_end", "sat");
      return finish(10);
    }
    event_trace.macro(k, "lucky_late_end", "unknown");
  }
  if (config_.graph_labeling_model.enabled &&
      k.graph_labeling_session.authenticated()) {
    const auto graph = run_graph_labeling(k);
    if (graph.status == GraphLabelingSliceStatus::Sat)
      return finish(10);
  }
  presearch_ticks->leave();
  presearch_ticks.reset();
  // There is no Kissat collection point between late Lucky and search
  // (preprocess.c:57, reduce.c:160-185).  In particular, collecting the whole
  // arena here would apply the optional CaDiCaL diagnostic order after
  // preprocessing and invent a representation transition the reference never
  // performs.  Tactix models
  // initial fixed-variable compaction's decision-population effect above;
  // subsequent variable/arena compaction remains coupled to Reduce below.
  // Preprocessing has consumed the root-unit trail prefix.  Tactix retains
  // those literals for stable IDs/proof witnesses, so only the lifecycle
  // marker is cleared (Kissat physically clears that prefix).
  (void)port::complete_action(completion_, FeedbackKind::Support,
      "ConsumeRootPrefix", INSTANCE_CANONICAL, [] {
        execution::NativeFeedbackSpec spec;
        spec.description.description = "before-search";
        return spec;
      }, [&] {
        return port::execute_action(k, execution::ConsumeRootPrefixRequest{})
                   ? ApplyResult::Ok : ApplyResult::NoOp;
      });
#ifdef TACTIX_ENABLE_EVENT_TRACE
  if (const char *selected = strategy_setting("TACTIX_EVENT_TRACE_FACTOR_LITERAL"))
    for (Lit lit = 0; lit < static_cast<Lit>(2 * k.nvars); lit++)
      if (std::atoi(selected) == kernel_externalize(k, lit)) {
        event_trace_factor_literal_state(
            k, lit, k.factor_marks[lit], kernel_var_retired(k, var_of(lit)),
            k.value(lit), k.watches.size(lit));
        uint32_t ordinal = 0;
        for (const WatchEntry watch : k.watches.entries(lit)) {
          const Clause *clause =
              watch.binary() ? nullptr : clause_at(k.arena, watch.ref());
          event_trace_factor_literal_entry(
              k, lit, ordinal++, !clause || !clause->garbage(),
              watch.binary() ? watch.other() : INVALID_LIT,
              watch.binary() ? kernel_binary_id(k, lit, watch.other())
                             : clause->id(),
              clause);
        }
      }
#endif

  // Kissat classifies once after startup simplification and again while
  // entering search (`search.c:kissat_search`, then `start_search`).  The
  // calls are adjacent there too; preserve both so the reach counter and the
  // classification lifetime match the reference exactly.
  update_formula_classification(k);
  update_formula_classification(k);

  // Kissat initializes the first search-time Eliminate limit only from
  // start_search, after startup simplification has changed BINIRR_CLAUSES.
  inprocess_.initialize_search_limits(k);

  // Kissat initializes the first focused-mode tick anchor in `start_search`,
  // after Lucky and preprocessing.  Tactix's schedule object is reset before
  // those startup phases, whose kernel work also uses the shared tick meter;
  // rebase this one clock so startup work cannot lengthen the first stable
  // interval.  Later anchors are maintained by `update_mode_limits`.
  mode.ticks_at_focus_entry = search_ticks(k);
  feedback.resume(IntervalStart::ResumeSearch, "native-start-search");

  for (;;) {
    event_trace.checkpoint(k);
#ifdef TACTIX_ENABLE_EVENT_TRACE
    if (k.propagate_head < k.trail.size()) {
      const std::string cursor_payload =
          "next=" + std::to_string(kernel_externalize(k, k.trail[k.propagate_head])) +
          "|pos=" + std::to_string(k.propagate_head) +
          "|trail=" + std::to_string(k.trail.size());
      event_trace.detail_text(k, "propagate_cursor", cursor_payload.c_str());
    }
#endif
    const size_t trace_trail_begin = k.trail.size();
    const ApplyResult r = default_propagate(k, "search", completion_);
    event_trace.implications(k, trace_trail_begin);
    if (r == ApplyResult::Unsat)
      return finish(20);
    if (r == ApplyResult::Conflict) {
      default_branching_.on_search_propagation_conflict();
      event_trace.conflict(k);
      event_trace.detail(k, "analysis_begin", k.level);
      event_trace.mode_clock(k, "pre_analyze", mode.ticks_limit,
                             mode.ticks_at_focus_entry, mode.count);
      observe_decision_rate(k); // kissat: before the analysis, once per conflict
#ifdef TACTIX_ENABLE_EVENT_TRACE
      event_trace.detail(k, "reason_bump_delay_before",
                         bump_reasons_delay_.count);
      event_trace.detail(k, "reason_bump_rate_milli",
                         static_cast<int64_t>(
                             averages[k.h->stable].decision_rate.value * 1000));
#endif
      // The per-conflict payload, by value (gap plan Step 6): the public
      // ANALYZE_TARGET_CHRONO instance.
      execution::NativeConflictAnalysisRequest request;
      request.bump_reason_limit = want_bump_reasons(k);
      request.bump_reason_depth = config_.analysis.reason_depth;
      request.target_mode = AnalysisTarget::Chrono;
      request.retier_schedule = retier_;
      assert(k.conflict_reuse_chrono_levels == chrono_levels);
      event_trace.detail(k, "reason_bump_limit", request.bump_reason_limit);
      const uint64_t analysis_instance = ANALYZE_TARGET_CHRONO;
      const uint64_t learned_before = k.stats.learned;
      const uint64_t glue_sum_before = k.stats.learned_glue_sum;
      const uint64_t backjumps_before = k.stats.backjumps;
      const uint64_t br_rounds = k.stats.reason_bump_rounds;
      const uint64_t br_aborted = k.stats.reason_bump_aborted;
      const auto analysis = port::complete_action(
          completion_, FeedbackKind::SearchService, "ConflictAnalysis", analysis_instance,
          [&] {
            auto spec = execution::native_rule_feedback(k, "ConflictAnalysis",
                analysis_instance, "native-target-chrono");
            // The record names the request's whole payload: the profile with
            // its bump fields, and the schedule it carries.
            auto &profile = std::get<strategy::AnalyzeParameters>(
                spec.description.invocation->parameters);
            profile.bump_reason_limit = request.bump_reason_limit;
            profile.bump_reason_depth = request.bump_reason_depth;
            spec.description.description +=
                ";retier=" + execution::retier_schedule_text(request.retier_schedule);
            return spec;
          }, [&] {
        return port::execute_action(
          k, request,
          [&](auto &&apply) {
            return default_direct_rule_lazy(
                k, "ConflictAnalysis",
                [&] { return "conflict=" + std::to_string(k.stats.conflicts); },
                apply);
          },
          [&](ApplyResult ar) -> uint32_t {
            // Advice only traces; the policy's own updates read the completed
            // transaction below.
#ifdef TACTIX_ENABLE_EVENT_TRACE
            if (ar == ApplyResult::Unsat) {
              event_trace.detail(k, "analysis_end", -1);
            } else if (!k.pending.valid) {
              event_trace.detail(k, "analysis_end", k.level);
              event_trace_analysis_deletions(k);
              event_trace.mode_clock(k, "post_analyze", mode.ticks_limit,
                                     mode.ticks_at_focus_entry, mode.count);
            } else {
              event_trace.learned(k, k.pending.lits);
              event_trace.detail(k, "analysis_tail_learned_emitted",
                                 k.pending.lits.size());
              event_trace.detail(k, "analysis_tail_policy_updated",
                                 k.pending.glue);
              const uint32_t chosen_back = k.level - 1;
              event_trace.detail(
                  k, "analysis_tail_target",
                  chosen_back - k.pending.jump_level > chrono_levels
                      ? chosen_back
                      : k.pending.jump_level);
            }
#else
            (void)ar;
#endif
            return 0;
          });
      }, [&](const execution::ConflictAnalysisExecutionResult &result,
             strategy::controller::NativeActionDescription &description) {
        // The realized target makes the native choice replayable as the
        // public ConflictAnalysis instance (target clamped to [jump, level-1]).
        // Conflict reuse and level-one unit batches end without Backjump at
        // their own realized level.
        if (result.backjumped)
          description.description += ";target=" + std::to_string(result.target) +
                                     ";jump=" + std::to_string(result.jump);
        else if (result.result != ApplyResult::Unsat)
          description.description += ";target=" + std::to_string(k.level) +
                                     ";no-backjump=1";
        description.description +=
            ";retier-after=" + execution::retier_schedule_text(result.retier_schedule);
        return strategy::controller::feedback_outcome(result.result);
      });
      retier_ = analysis.retier_schedule;
      if (analysis.analysis == ApplyResult::Unsat)
        return finish(20);
      // Post-transaction observation through public statistics only: a
      // learned result (a backjumped clause, or a level-one unit batch, which
      // Kissat observes as size one / glue zero, learn.c:120-130) feeds the
      // glue averages and the reluctant clock; only a final learned clause
      // (a counted backjump) reaches Kissat's DELAYING(bumpreasons) gate,
      // never reuse or early exits.
      const bool backjumped = k.stats.backjumps != backjumps_before;
      assert(backjumped == analysis.backjumped);
      if (k.stats.learned != learned_before) {
        if (backjumped)
          bump_reasons_delay_.reached_gate(
              k.stats.reason_bump_rounds != br_rounds,
              k.stats.reason_bump_aborted != br_aborted);
        observe_glue(k, static_cast<uint32_t>(k.stats.learned_glue_sum -
                                              glue_sum_before));
        if (k.h->stable)
          tick_reluctant(); // per learned clause (kissat learn.c)
      }
      assert(!backjumped || k.stats.learned == learned_before + 1);
      if (!backjumped)
        continue;
      event_trace.detail(k, "analysis_tail_backjump_return", k.level);
      event_trace.heap_scores(k);
      event_trace.detail(k, "analysis_propagate_head", k.propagate_head);
      event_trace.detail(k, "analysis_trail_size", k.trail.size());
      event_trace.detail(k, "analysis_end", k.level);
      event_trace_analysis_deletions(k);
      event_trace.mode_clock(k, "post_analyze", mode.ticks_limit,
                             mode.ticks_at_focus_entry, mode.count);
      continue;
    }
    // The retained matrix observes a completed Boolean propagation prefix.
    // A published conditional reason re-arms ordinary watched propagation,
    // which consumes the implication or conflict on the next loop iteration.
    if (k.level && config_.affine_search.enabled &&
        k.affine_matrix_search_session &&
        k.affine_matrix_search_session->enabled() &&
        !affine_usefulness_paused_) {
      const auto algebraic = service_affine_search(k, false);
      if (algebraic.status == AffineMatrixServiceStatus::Applied)
        continue;
    }
    if (k.unassigned_active == 0)
      return finish(10);
    if (want_reduce(k) && reduce_guard(k)) {
      event_trace.macro(k, "reduce_begin");
      feedback.end_interval(StopReason::RootPreparation);
      (void)port::complete_action(completion_, FeedbackKind::Support,
          "ReductionPreparation", INSTANCE_CANONICAL, [] {
            execution::NativeFeedbackSpec spec;
            spec.description.description = "native-reduce-root-preparation";
            return spec;
          }, [&] {
            default_prepare_reduction(k);
            return k.inconsistent ? ApplyResult::Unsat : ApplyResult::Ok;
          });
      if (k.inconsistent)
        return finish(20);
      ReductionPolicy &rp = reduction ? *reduction : default_reduction_;
      if (dynamic_tiers_)
        retier_current_mode(k, retier_);
      pose_reduce_tiers(k, retier_, config_.reduction, completion_);
      // Kissat samples compactlim before selecting/deleting victims and uses
      // that decision to choose sparse-collection start 0 versus the first
      // reducible clause.  Reusing the decision after Reduce also prevents a
      // formula-changing pass from silently changing the admission boundary.
      const bool compact_due =
          compact_ && kernel_should_compact_variables(
                          k, config_.reduction.compact_inactive_percent);
      // The policy's victim count is Reduce's canonical fraction for the
      // default unramped reduction; any other count is the explicit instance.
      // The rule ranks the same candidates under the posed tiers.
      const bool canonical_victims = !reduction && !default_reduction_.ramp;
      size_t reduce_victims = 0;
      bool ranked = false;
      const auto rank_victims = [&] {
        if (ranked) return;
        ranked = true;
        reduce_candidates(k, reduce_scratch_, /*measure=*/false,
                          config_.reduction.protect_tiers);
        reduce_victims = std::min(rp.victims(k, reduce_scratch_.size()),
                                  reduce_scratch_.size());
      };
      if (!canonical_victims) rank_victims();
#ifdef TACTIX_ENABLE_EVENT_TRACE
      rank_victims();
      k.diagnostic_policy_reduce_bytes =
          reduce_scratch_.capacity() * sizeof(Ref);
      event_trace.reduction(k, reduce_scratch_, reduce_victims);
#endif
      strategy::ReduceParameters selected = strategy::ReduceParameters::from_kernel(k);
      selected.victim_numerator = config_.reduction.victim_numerator;
      selected.victim_denominator = config_.reduction.victim_denominator;
      selected.protect_tiers = config_.reduction.protect_tiers;
      selected.compact_collection = compact_due;
      const uint64_t reduce_instance =
          canonical_victims ? INSTANCE_CANONICAL : reduce_victims;
      (void)port::complete_action(completion_, FeedbackKind::Rule, "Reduce",
          reduce_instance, [&] {
            rank_victims();
            std::string description = "compact=" + std::to_string(compact_due) + ";victim-ids=";
            for (size_t i = 0; i < reduce_victims; ++i)
              description += std::to_string(clause_at(k.arena, reduce_scratch_[i])->id()) + ',';
            auto spec = execution::native_rule_feedback(k, "Reduce", reduce_instance,
                                                        std::move(description));
            spec.description.invocation->parameters = selected;
            return spec;
          }, [&] {
            static const RuleVTable &reduce_rule = registered_rule("Reduce");
            const port::ScopedReduceParameters scoped(k, selected);
            return canonical_victims ? port::apply_rule(reduce_rule, k)
                                     : port::apply_rule_instance(reduce_rule, k, reduce_victims);
          });
      if (compact_due)
        event_trace_target_state(
            k, "reduce-compact-pre",
            static_cast<uint32_t>(k.compact_import_order.size() -
                                  k.unassigned_active));
      if (compact_due)
        (void)port::complete_action(completion_, FeedbackKind::Support,
            "CompactVariables", execution::COMPACT_FOLD_FIXED | execution::COMPACT_PUBLISH_RANDOM_ORDER, [] {
              execution::NativeFeedbackSpec spec;
              spec.description.description = "fold-fixed=1;publish-random-order=1";
              return spec;
            }, [&] {
              const bool compacted = port::execute_action(k, execution::CompactVariablesRequest{true});
              if (compacted) heuristics_compact_random_order_fixed(k);
              return compacted ? ApplyResult::Ok : ApplyResult::NoOp;
            });
      if (compact_due)
        event_trace_target_state(
            k, "reduce-compact-post",
            static_cast<uint32_t>(k.compact_import_order.size() -
                                  k.unassigned_active));
      update_reduce_limit(k);
      pose_vivify_tiers(k, retier_, config_.inprocess, completion_);
      update_formula_classification(k);
      inprocess_.note_reduce(k);
      progress_.event(k, 'R');
      feedback.resume(IntervalStart::NativeContinuation, "native-reduce-search-continuation");
      event_trace.macro(k, "reduce_end");
      continue;
    }
    const bool pin_mode =
        force_mode_ == 1 || (force_mode_ == 2 && k.h->stable);
    if (!pin_mode && want_switch_mode(k) && switchmode_guard(k)) {
      event_trace.macro(k, "switch_begin");
      feedback.end_interval(StopReason::StrategyChange);
      (void)port::complete_action(completion_, FeedbackKind::Rule, "SwitchMode",
          INSTANCE_CANONICAL, [&] { return execution::native_rule_feedback(
              k, "SwitchMode", INSTANCE_CANONICAL, "native-mode-transition"); }, [&] {
        static const RuleVTable &switch_mode = registered_rule("SwitchMode");
        (void)port::apply_rule(switch_mode, k);
        mode.count++;
        averages[k.h->stable].saved_decisions = k.stats.decisions;
        if (k.h->stable) // entering stable: fresh reluctant sequence
          reluctant = {};
        update_mode_limits(k);
        // Kissat mode.c calls `kissat_start_random_sequence` after every mode
        // transition.  It is a no-op on stable entry and starts (or continues)
        // a focused sequence regardless of decision level or its ordinary
        // conflict deadline.
        default_branching_.start_random_sequence(k);
        return ApplyResult::Ok;
      });
      feedback.resume(IntervalStart::NativeContinuation, "native-mode-search-continuation");
      event_trace.macro(k, "switch_end");
      event_trace.heap_scores(k);
      event_trace.mode_clock(k, "post_switch", mode.ticks_limit,
                             mode.ticks_at_focus_entry, mode.count);
      continue;
    }
    {
      const ModeAverages &avg = averages[k.h->stable];
      const bool ordinary_restart=want_restart(k);
      event_trace.restart_clock(k, restart.next_conflicts, avg.initialized,
                                avg.fast_glue.value, avg.slow_glue.value,
                                config_.restart.glue_margin, ordinary_restart);
      if (ordinary_restart && k.level > 0 && restart_guard(k)) {
        event_trace.macro(k, "restart_begin");
      // Kissat restart.c asks `kissat_next_decision_variable` for the reuse
      // threshold.  That call is semantically part of restart: during a
      // random-decision sequence it consumes and counts a draw before the
      // post-restart decision.  Route through the branching subpolicy, then
      // hand only its variable to the policy-neutral Restart rule.
      const Lit next = default_branching_.pick_for_restart_reuse(k);
      const uint32_t keep = heuristics_restart_reuse_level_for(k, var_of(next));
      const ApplyResult restart_result = port::complete_action(
          completion_, FeedbackKind::SearchService,
          "Restart", keep, [&] { return execution::native_rule_feedback(
              k, "Restart", keep, "retained-level=" + std::to_string(keep) +
                  ";purpose=scheduled"); }, [&] {
        if (keep < k.level)
          heuristics_update_target_phases(k);
        static const RuleVTable &restart_rule = registered_rule("Restart");
        const ApplyResult restarted = port::apply_rule_instance(restart_rule, k, keep);
        reluctant.trigger = false; // consumed (kissat reluctant.h)
        if (!k.h->stable) // kissat restart.c: focused-only limit update
          update_restart_limit(k);
        return restarted;
      });
      feedback.end_interval(StopReason::Restart);
      progress_.event(k, 'r');
      if (restart_result == ApplyResult::Unsat)
        return finish(20);
      // The retained root services (persistent affine search, graph
      // labeling) run only in the root window of a natural full restart and
      // never request a restart themselves (docs/dev/implementation-notes.md).
      if (!k.level) {
        auto *session = k.affine_matrix_search_session;
        const bool affine_candidate = config_.affine_search.enabled &&
            session && session->enabled();
        if (affine_candidate || graph_labeling_due(k)) {
          const ApplyResult rooted = default_propagate(
              k, "retained-service-restart-root", completion_);
          if (rooted != ApplyResult::Ok)
            return finish(20);
        }
        if (affine_candidate && affine_restart_service(k) == ApplyResult::Unsat)
          return finish(20);
        if (graph_labeling_due(k)) {
          const auto graph = run_graph_labeling(k);
          if (graph.status == GraphLabelingSliceStatus::Sat)
            return finish(10);
        }
      }
      feedback.continue_search(keep ? IntervalStart::PartialContinuation
                                    : IntervalStart::NativeContinuation);
      event_trace.macro(k, "restart_end");
      continue;
      }
    }
    if (want_reorder(k)) {
      k.stats.reorder_eligible++;
      if (reorder_guard(k)) {
        event_trace.reorder_begin(k);
        const uint64_t target = config_.reorder.target == DefaultReorderTarget::Queue
                                    ? REORDER_QUEUE
                                : config_.reorder.target == DefaultReorderTarget::Heap
                                    ? REORDER_HEAP : INSTANCE_CANONICAL;
        (void)port::complete_action(completion_, FeedbackKind::Rule, "Reorder", target,
            [&] { return execution::native_rule_feedback(k, "Reorder", target, "native-reorder"); }, [&] {
          static const RuleVTable &reorder_rule = registered_rule("Reorder");
          const ApplyResult reordered = port::apply_rule_instance(reorder_rule, k, target);
          reorder.count++;
          update_reorder_limit(k);
          return reordered;
        });
        event_trace.reorder_end(k);
        continue;
      }
    }
    if (want_rephase(k) && rephase_guard(k)) {
      event_trace.macro(k, "rephase_begin");
      feedback.end_interval(StopReason::RootPreparation);
      if (prepare_rephase_root(k) == ApplyResult::Unsat)
        return finish(20);
      const DefaultRephaseSlot slot = current_rephase_slot();
      rephase.count++; // ONE rephase event, whatever it is composed of
      if (slot == DefaultRephaseSlot::Walk && walk_guard(k)) {
        // Kissat warmup.c: before importing phases into the walker, complete a
        // temporary assignment with the active branch ranking and propagate
        // beyond conflicts. Assignment-time phase saving is the output; the
        // trail is then discarded without undoing it.
        if (config_.rephase.walk_warmup) {
          // The record's instance is the public WARMUP phase argument.
          const int8_t forced = default_branching_.forced_phase(k);
          const execution::WarmupPhase phase =
              forced > 0 ? execution::WarmupPhase::Positive
              : forced < 0 ? execution::WarmupPhase::Negative
                           : execution::WarmupPhase::Heuristic;
          (void)port::complete_action(completion_, FeedbackKind::Support,
              "Warmup", static_cast<uint64_t>(phase), [] {
                execution::NativeFeedbackSpec spec;
                spec.meter = [](const Kernel &kernel) { return kernel.stats.ticks; };
                spec.description.effort_currency = "ticks";
                spec.description.description = "native-assignment-phase-warmup";
                return spec;
              }, [&] { return port::execute_action(k, execution::WarmupRequest{},
              [&] { return default_branching_.pick(k); }, [&](Lit decision) {
            event_trace.detail(k, "decision", kernel_externalize(k, decision));
            event_trace.heap_after_decision(k);
            event_trace.heap_scores(k);
          }, [&](size_t begin) { event_trace.implications(k, begin); });
          }, [](const execution::WarmupResult &result) {
            auto out = strategy::controller::feedback_outcome(ApplyResult::Ok);
            if (result.interrupted) out.invocation.status = strategy::InvocationStatus::Interrupted;
            return out;
          });
        }
        const uint64_t reference = walk_effort_reference(k);
        // Kissat seeds the walker from the search policy's shared generator.
        // DefaultBranching owns that generator in Tactix; snapshot it here so
        // the H-only Walk operation need not reach back into its caller.
        const uint64_t seed = config_.rephase.walk_use_branch_seed
                                  ? default_branching_.randec_rng
                                  : config_.rephase.walk_random_seed;
        const uint64_t allowance = scale_permille(reference, config_.rephase.walk_permille);
        const strategy::GuidanceOutcomeSnapshot before_walk(k);
        (void)port::complete_action(completion_, FeedbackKind::Rule, "Walk", INSTANCE_CANONICAL,
            [&] {
              auto spec = execution::native_rule_feedback(k, "Walk", INSTANCE_CANONICAL,
                  "native-walk", [](const Kernel &kernel) { return kernel.stats.walk_steps; },
                  "walk-steps", strategy::WorkLimit::bounded(allowance));
              auto parameters = strategy::WalkParameters::from_kernel(k);
              parameters.step_budget = allowance;
              parameters.random_seed = seed;
              // DefaultPolicy runs unseeded Walk; seeding is a controller choice.
              parameters.seed_candidate = 0;
              spec.description.invocation->parameters = std::move(parameters);
              return spec;
            }, [&] {
              static const RuleVTable &walk_rule = registered_rule("Walk");
              return port::execute_action(k, execution::WalkRequest{allowance, seed, 0},
                                               [&] { return port::apply_rule(walk_rule, k); });
            },
            [&](ApplyResult result) {
              auto out = strategy::controller::feedback_outcome(result);
              out.invocation.status = before_walk.refine(k, result, out.invocation.status);
              return out;
            });
      } else {
        const DefaultRephaseSlot effective =
            slot == DefaultRephaseSlot::Walk
                ? config_.rephase.walk_inapplicable_fallback
                : slot;
        uint64_t type = REPHASE_BEST;
        if (effective == DefaultRephaseSlot::Inverted)
          type = REPHASE_INVERTED;
        else if (effective == DefaultRephaseSlot::Original)
          type = REPHASE_ORIGINAL;
        (void)port::complete_action(completion_, FeedbackKind::Rule, "Rephase", type,
            [&] { return execution::native_rule_feedback(k, "Rephase", type, "native-rephase"); },
            [&] {
              static const RuleVTable &rephase_rule = registered_rule("Rephase");
              return port::apply_rule_instance(rephase_rule, k, type);
            });
      }
      update_rephase_limit(k);
      feedback.resume(IntervalStart::NativeContinuation, "native-rephase-search-continuation");
      event_trace.macro(k, "rephase_end");
      continue;
    }
    if (inprocess_.want_phase(k, SlotGroup::Probe)) {
      event_trace.macro(k, "probe_begin");
      feedback.end_interval(StopReason::RootPreparation);
      const int phase_result = inprocess_.run_phase(k, SlotGroup::Probe, stop, completion_);
      if (phase_result == 10 || phase_result == 20) return finish(phase_result);
      update_formula_classification(k);
      feedback.resume(IntervalStart::NativeContinuation, "native-probe-search-continuation");
      event_trace.macro(k, "probe_end");
      continue;
    }
    if (inprocess_.want_phase(k, SlotGroup::Eliminate)) {
      event_trace.macro(k, "eliminate_begin");
      feedback.end_interval(StopReason::RootPreparation);
      const int phase_result = inprocess_.run_phase(k, SlotGroup::Eliminate, stop, completion_);
      if (phase_result == 10 || phase_result == 20) return finish(phase_result);
      update_formula_classification(k);
      feedback.resume(IntervalStart::NativeContinuation, "native-eliminate-search-continuation");
      event_trace.macro(k, "eliminate_end");
      continue;
    }
    if (stop && *stop)
      return finish(0);
    const Lit decision = default_branching_.pick(k);
    (void)port::complete_action(completion_, FeedbackKind::SearchService,
        "Decide", lit_instance(k, decision), [&] {
          return execution::native_rule_feedback(k, "Decide", lit_instance(k, decision),
              "literal=" + std::to_string(kernel_externalize(k, decision)));
        }, [&] { return default_direct_rule_lazy(
        k, "Decide",
        [&] {
          return "literal=" +
                 std::to_string(kernel_externalize(k, decision));
        },
        [&] {
          const ApplyResult result =
              port::execute_action(k, execution::DecisionRequest{decision});
          if (result == ApplyResult::Ok)
            heuristics_note_target_decision(
                k, decision, k.h->stable);
          return result;
        });
    });
    event_trace.detail(k, "decision", kernel_externalize(k, decision));
    event_trace.heap_after_decision(k);
    event_trace.heap_scores(k);
  }
}


namespace {
void slot_trace_entry(const Kernel &k, const DefaultInprocessScheduler::SlotEntry &e,
                      const RuleVTable &r, int outer) {
  if (!strategy_setting("TACTIX_SLOT_TRACE"))
    return;
  std::fprintf(stderr,
               "[slot] phase=%llu outer=%d %s enabled=%d defer=%d "
               "guard=%d dosed=%d followup=%u mode=%u meter=%llu limit=%llu\n",
               (unsigned long long)k.stats.inprocess_phases, outer, r.name,
               (int)e.enabled, (int)e.defer_preprocess, (int)r.guard(k),
               (int)e.dosed, static_cast<unsigned>(e.followup),
               static_cast<unsigned>(e.followup_mode),
               (unsigned long long)(e.meter ? e.meter(k) : 0),
               (unsigned long long)e.limit);
}
} // namespace

bool DefaultInprocessScheduler::slot_entry_admissible(SlotEntry &e, const Kernel &k,
                                          const SlotPhaseCtx &ctx) {
  if (!e.enabled || ctx.phase_unsat) {
    e.tm.dec_disabled++;
    return false;
  }
  const bool initial_fast_bve =
      initial_fast_elimination_active_ &&
      !std::strcmp(tactix_rules()[e.rule].name, "BVE");
  if (k.stats.conflicts == 0 && !e.preprocess && !initial_fast_bve) {
    e.tm.dec_preprocess++;
    return false;
  }
  if (e.max_applies && e.applies >= e.max_applies) {
    e.tm.dec_cap++;
    return false; // configured per-occurrence cap (see SlotEntry::max_applies)
  }
  if (e.defer_preprocess && k.stats.conflicts == 0 && !initial_fast_bve) {
    e.tm.dec_preprocess++;
    return false; // no elimination at preprocessing (kissat fastel=0)
  }
  e.applies++;
  return true;
}

bool DefaultInprocessScheduler::followup_admissible(SlotEntry &e,
                                                     const Kernel &k,
                                                     SlotPhaseCtx &ctx) {
  e.followup_mode = FollowupMode::None;
  if (k.stats.conflicts != 0 || e.followup == FollowupKind::None ||
      !preprocess_followups_)
    return true;
  e.tm.followup_offered++;
  e.followup_mode = preprocessing_followup_mode(
      e.followup, ctx.congruence_changed, ctx.sweep_changed);
  if (e.followup_mode == FollowupMode::None)
    return false;
  e.tm.followup_taken++;
  if (e.followup_mode == FollowupMode::Eager)
    e.tm.followup_eager++;
  else
    e.tm.followup_final++;
  return true;
}

FollowupMode DefaultInprocessScheduler::preprocessing_followup_mode(
    FollowupKind kind, bool congruence_changed, bool sweep_changed) {
  if (kind == FollowupKind::AfterCongruence)
    return congruence_changed ? FollowupMode::Eager : FollowupMode::None;
  if (kind == FollowupKind::AfterSweepOrFinal) {
    if (sweep_changed)
      return FollowupMode::Eager;
    if (!congruence_changed)
      return FollowupMode::Final;
  }
  return FollowupMode::None;
}

// SC2024 fastel folds a small forward-subsumption check into each occurrence
// flush. Keep the deletion in the SubsumeForward rule; this helper owns only
// the outer occurrence traversal, clause-size cap and population accounting.
static size_t fastel_flush_occurrences(Kernel &k, Lit lit,
                                       const RuleVTable &subsume,
                                       std::vector<uint8_t> &marks,
  uint32_t occurrence_limit,
  uint32_t clause_size_limit, PolicyCompletion *completion = nullptr) {
  execution::CompletionPublication published(completion);
  assert(k.dense && k.dense->track_order);
  size_t count = 0;
  // Kissat compacts the inspected prefix in place once this immutable span
  // expires; an early cap leaves the uninspected suffix untouched.
  size_t at = 0;
  // SubsumeForward marks a clause garbage but does not collect or mutate the
  // dense payload, so this owner-ordered view remains stable during the scan.
  for (const OccIndex::Entry entry : k.dense->ordered[lit]) {
    if (entry.binary) {
      const Lit other = entry.payload;
      if (k.value(other) > 0) {
        ++at;
        continue;
      }
      if (kernel_var_retired(k, var_of(other))) {
        ++at;
        continue;
      }
    } else {
      const Ref ref = entry.payload;
      Clause *clause = clause_at(k.arena, ref);
      bool contains = false;
      for (const Lit other : *clause)
        contains |= other == lit;
      if (clause->garbage() || !contains) {
        ++at;
        continue;
      }
      if (clause->size > clause_size_limit) {
        count = static_cast<size_t>(occurrence_limit) + 1;
        ++at;
        break;
      }
      bool satisfied = false;
      for (const Lit other : *clause)
        satisfied |= k.value(other) > 0;
      if (satisfied) {
        ++at;
        continue;
      }
      const ApplyResult cleaned = port::complete_action(completion, FeedbackKind::Rule,
          "SubsumeForward", ref, [&] {
            auto spec = execution::native_rule_feedback(k, "SubsumeForward", ref,
                "fastel-occurrence;clause-id=" + std::to_string(clause->id()) +
                ";literal=" + std::to_string(kernel_externalize(k, lit)) +
                ";occurrence-limit=" + std::to_string(occurrence_limit) +
                ";clause-size-limit=" + std::to_string(clause_size_limit),
                meter_forward_pair, "forward-plus-strengthen-visits");
            return spec;
          }, [&] { return port::apply_rule_structured(
          subsume, k, "fastel-occurrence",
          [&] {
            return port::subsume_forward_try_fastel(k, ref, occurrence_limit,
                                              marks);
          });
      });
      if (cleaned == ApplyResult::Ok) {
        ++at;
        continue;
      }
    }
    ++at;
    if (++count > occurrence_limit)
      break;
  }
  port::execute_action(k, execution::CompactFastelPrefixRequest{lit, at});
  return count;
}

uint64_t DefaultInprocessScheduler::ledger_remaining(SlotEntry &e, const Kernel &k,
                                        uint64_t limit) {
  const uint64_t spent = e.meter(k);
  if (limit >= spent)
    return limit - spent;
  e.tm.wrapped++;
  return 0;
}

DefaultInprocessScheduler::SlotStep
DefaultInprocessScheduler::run_dosed_entry(Kernel &k, SlotEntry &e, const RuleVTable &r,
                               SlotPhaseCtx &ctx) {
  uint64_t limit = e.limit;
  ctx.last_result = ApplyResult::NoOp;
  if (!r.guard(k)) {
    e.tm.dec_guard++;
    return SlotStep::Next;
  }
  const bool bve_entry = !std::strcmp(r.name, "BVE");
  const bool initial_fast_bve =
      bve_entry && initial_fast_elimination_active_;
  BveBoundPolicy *ramp = nullptr;
  if (bve_entry && !initial_fast_bve)
    ramp = bve_bound ? bve_bound : &default_bve_ramp_;
  const bool level_mode = bve_entry && ramp;
  const uint32_t passes = level_mode ? 1 : ramp ? ramp->num_passes() : 1;
  uint64_t elim_at_last_pass = k.stats.bve_eliminated;
  const bool use_retry = bve_entry && !ramp;
  bool engaged = false; // a pass got past the budget break and ran instances
  for (uint32_t pass = 0; pass < passes && !ctx.phase_unsat; pass++) {
    if (ramp && e.meter(k) > limit) {
      break;
    }
    engaged = true;
    if (ramp) {
      // The pass bound is the installed BVE growth bound, which the
      // policy's candidate scores and the pass's inherited profile read.
      strategy::BveParameters bound = strategy::BveParameters::from_kernel(k);
      bound.growth_bound = level_mode ? bve_level_ : ramp->bound(pass);
      port::execute_action(k, execution::ParameterInstallationRequest{bound},
                                ctx.completion, "BVE");
    }
    elim_at_last_pass = k.stats.bve_eliminated; // fixpoint test above
    // Pose and validate BVE's typed invocation once per pass.  Keeping this
    // session outside the hot candidate loop preserves the legacy default
    // schedule without paying parameter save/restore and rule validation for
    // every pivot.
    std::optional<strategy::InvocationBudget> bve_invocation_budget;
    std::optional<port::BveInvocationSession> bve_invocation_session;
    if (bve_entry) {
      bve_invocation_budget.emplace(strategy::WorkUnit::BveTicks,
                                    strategy::WorkLimit::unlimited());
      const auto *posed = std::get_if<strategy::BveParameters>(&e.parameters);
      const strategy::BveParameters parameters =
          initial_fast_bve
              ? config().initial_fast_elimination_parameters
              : posed ? *posed : strategy::BveParameters::from_kernel(k);
      bve_invocation_session.emplace(
          r, k, parameters, *bve_invocation_budget);
    }
    if (initial_fast_bve) {
      // Candidate discovery must inspect every live variable before the
      // owner's radix order is known. Charge that mandatory lower bound to
      // the same typed startup dose as resolution. If it cannot fit, decline
      // before allocating or mutating a partial owner domain.
      const uint64_t active = active_variables(k);
      ++k.stats.fastel_rounds;
      k.stats.fastel_active += active;
      const uint64_t resolution_remaining = ledger_remaining(e, k, e.limit);
      const bool overflow =
          initial_fast_elimination_discovery_ticks_ > resolution_remaining;
      const uint64_t discovery_remaining =
          overflow ? 0
                   : resolution_remaining -
                         initial_fast_elimination_discovery_ticks_;
      if (!default_fast_elimination_discovery_admissible(
              active, discovery_remaining)) {
        ++k.stats.fastel_budget_stops;
        initial_fast_elimination_budget_exhausted_ = true;
        ctx.eliminate_limit_reached = true;
        ++e.tm.dec_budget;
        return SlotStep::Next;
      }
      initial_fast_elimination_discovery_ticks_ += active;
      k.stats.fastel_discovery_ticks += active;
      assert(initial_fast_elimination_discovery_ticks_ <= e.limit);
      limit = e.limit - initial_fast_elimination_discovery_ticks_;
    }
    payloads_.resize(2 * static_cast<size_t>(k.nvars) + 1);
    uint32_t n =
        r.instances(k, payloads_.data(),
                    static_cast<uint32_t>(payloads_.size()));
    if (initial_fast_bve) {
      const std::vector<RuleVTable> &rules = tactix_rules();
      const int subsume_id = tactix_rule_id("SubsumeForward");
      assert(subsume_id >= 0);
      const RuleVTable &subsume = rules[static_cast<size_t>(subsume_id)];
      const strategy::BveParameters &parameters =
          config().initial_fast_elimination_parameters;
      struct Candidate {
        uint64_t payload;
        uint64_t score;
        uint64_t tie;
      };
      std::vector<Candidate> candidates;
      candidates.reserve(n);
      const auto import_position = [&](Var variable) -> uint64_t {
        return variable < k.compact_import_position.size() &&
                       k.compact_import_position[variable] != INVALID_VAR
                   ? k.compact_import_position[variable]
                   : variable;
      };
      // fastel.c gathers over all_variables, i.e. in the owner's import
      // order.  The gather flush mutates (satisfied/subsumed occurrences), so
      // its traversal order decides which clause of a subsuming pair dies.
      {
        std::vector<std::pair<uint64_t, uint64_t>> gather;
        gather.reserve(n);
        for (uint32_t j = 0; j < n; ++j) {
          Lit candidate;
          const uint64_t key = lit_of_instance(k, payloads_[j], &candidate)
                                   ? import_position(var_of(candidate))
                                   : UINT64_MAX;
          gather.emplace_back(key, payloads_[j]);
        }
        std::stable_sort(gather.begin(), gather.end(),
                         [](const auto &a, const auto &b) {
                           return a.first < b.first;
                         });
        for (uint32_t j = 0; j < n; ++j)
          payloads_[j] = gather[j].second;
      }
      for (uint32_t j = 0; j < n; ++j) {
        Lit candidate;
        if (!lit_of_instance(k, payloads_[j], &candidate))
          continue;
        const Var variable = var_of(candidate);
        // `flags.eliminate`: cleared after each attempt, re-armed by every
        // removal of a clause containing the variable (kernel bve_retry).
        if (variable >= k.bve_retry.size() || !k.bve_retry[variable] ||
            k.value(candidate) || kernel_var_retired(k, variable))
          continue;
        const Lit positive = make_lit(variable, false);
        const size_t pos = fastel_flush_occurrences(
            k, positive, subsume, initial_fast_elimination_marks_,
            parameters.occurrence_limit,
            parameters.clause_size_limit, ctx.completion);
        if (pos > parameters.occurrence_limit)
          continue;
        // Preserve the shipped owner's observable gather bug: both terms use
        // LIT(pivot), so the radix score is twice the positive population.
        const size_t neg = fastel_flush_occurrences(
            k, positive, subsume, initial_fast_elimination_marks_,
            parameters.occurrence_limit,
            parameters.clause_size_limit, ctx.completion);
        if (neg > parameters.occurrence_limit ||
            pos + neg > parameters.occurrence_limit)
          continue;
        candidates.push_back(
            {payloads_[j], pos + neg, import_position(variable)});
      }
      k.stats.fastel_candidates += candidates.size();
      // Admission decides whether the owner pass runs at all; its later
      // rounds (fastel.c) gather the re-armed variables unconditionally.
      if (!initial_fast_elimination_admitted_ &&
          !default_should_run_initial_fast_elimination(candidates.size(),
                                                       active_variables(k))) {
        ++k.stats.fastel_declined;
        candidates.clear();
      } else {
        initial_fast_elimination_admitted_ = true;
      }
      std::stable_sort(candidates.begin(), candidates.end(),
                       [](const Candidate &a, const Candidate &b) {
                         return std::tie(a.score, a.tie) <
                                std::tie(b.score, b.tie);
                       });
      n = static_cast<uint32_t>(candidates.size());
      for (uint32_t j = 0; j < n; ++j)
        payloads_[j] = candidates[j].payload;
    }
    const bool dynamic_bve = bve_entry && !candidate_order &&
                             e.order == CandidateOrder::Heuristic &&
                             !initial_fast_bve;
    if (dynamic_bve)
      bve_invocation_session->order_by_score_heap(payloads_.data(), n);
    else if (!initial_fast_bve)
      order_candidates(k, e, payloads_.data(), n);
    if (strategy_setting("TACTIX_SLOT_TRACE"))
      std::fprintf(stderr,
                   "[slot]   %s pass=%u n=%u bve_bound=%u meter=%llu "
                   "limit=%llu\n",
                   r.name, pass, n, k.bve_bound,
                   (unsigned long long)e.meter(k), (unsigned long long)limit);
    uint32_t i = 0;
    while (dynamic_bve ? !bve_invocation_session->ordering_empty() : i < n) {
      const uint64_t payload =
          dynamic_bve ? bve_invocation_session->next_ordered() : payloads_[i++];
      if (bve_entry) {
        Lit candidate;
        if (lit_of_instance(k, payload, &candidate) &&
            var_of(candidate) < k.nvars &&
            (k.value(candidate) || kernel_var_retired(k, var_of(candidate))))
          continue;
        // CryptoMiniSat OccSimplifier::can_eliminate_var keeps every variable
        // of a retained XOR (`xorclauses_vars`): DefaultPolicy does not offer
        // the persistent affine session's admitted support to BVE.
        if (lit_of_instance(k, payload, &candidate) &&
            var_of(candidate) < k.nvars &&
            affine_support_protected(k, var_of(candidate)))
          continue;
        // Resolvent generation can delete a newly satisfied large antecedent
        // and reinsert its still-active pivot before retiring it. Preserve
        // that physical ghost until pop, but do not apply/count BVE again.
        // Kissat tests active status before its effort limit at this boundary;
        // this is policy eligibility, not a restriction on public BVE actions.
      }
      if (initial_fast_bve) {
        Lit candidate;
        if (!lit_of_instance(k, payload, &candidate))
          continue;
        const Var variable = var_of(candidate);
        if (variable >= k.bve_retry.size() || !k.bve_retry[variable])
          continue;
        const int subsume_id = tactix_rule_id("SubsumeForward");
        assert(subsume_id >= 0);
        const RuleVTable &subsume =
            tactix_rules()[static_cast<size_t>(subsume_id)];
        const strategy::BveParameters &parameters =
            config().initial_fast_elimination_parameters;
        const size_t pos = fastel_flush_occurrences(
            k, make_lit(variable, false), subsume,
            initial_fast_elimination_marks_,
            parameters.occurrence_limit, parameters.clause_size_limit, ctx.completion);
        const size_t neg = fastel_flush_occurrences(
            k, make_lit(variable, true), subsume,
            initial_fast_elimination_marks_,
            parameters.occurrence_limit, parameters.clause_size_limit, ctx.completion);
        if (pos > parameters.occurrence_limit ||
            neg > parameters.occurrence_limit ||
            pos + neg > parameters.occurrence_limit) {
          port::execute_action(k, execution::RetireBveCandidateRequest{variable});
          continue;
        }
        // The structured invocation can itself introduce compact extension
        // definitions. Defer those direct helpers whenever SR applied. Under
        // root-growing SR, also defer a startup Factor helper that only became
        // eligible after earlier FastEl mutations and therefore escaped the
        // read-only preflight. Explicit/public BVE instances remain completely
        // legal and unfiltered.
        const uint32_t external = k.internal_to_external[variable];
        if (initial_fast_elimination_ownership_.protects_at_application(
                external)) {
          ++k.stats.fastel_extensions_protected;
          // The owner has completed this startup consideration. Clear the
          // retry flag exactly as after an attempted pivot so an unchanged
          // helper is not rediscovered in every bounded round; neighbouring
          // clause removal can still re-arm it through the normal lifecycle.
          port::execute_action(
              k, execution::RetireBveCandidateRequest{variable});
          continue;
        }
      }
      if (e.meter(k) > limit) {
        if (initial_fast_bve &&
            !initial_fast_elimination_budget_exhausted_) {
          ++k.stats.fastel_budget_stops;
          initial_fast_elimination_budget_exhausted_ = true;
          ctx.eliminate_limit_reached = true;
        }
        break; // dose spent
      }
      if (!r.guard(k)) {
        break;
      }
      if (use_retry || ramp) {
        Lit rl;
        if (lit_of_instance(k, payload, &rl) &&
            var_of(rl) < k.nvars && !k.bve_retry[var_of(rl)])
          continue;
      }
      if (ctx.stop && *ctx.stop)
        break; // §6: a dose can enumerate thousands of candidates
      if (e.cursor_enabled) {
        e.cursor = instance_key(payload) + 1;
        e.ord.advanced++;
      }
      const bool hand_residual = e.budget_aware && e.meter;
      std::optional<port::WorkSettingsScope> candidate_work;
      if (hand_residual) {
        const uint64_t remaining = ledger_remaining(e, k, limit);
        candidate_work.emplace(
            k, execution::WorkSettingsRequest{remaining ? remaining : 1,
                                              std::nullopt, std::nullopt,
                                              std::nullopt},
            execution::WorkSettingsExit::ClearBudget);
      }
      // Kissat mutates H at the same points as each BVE candidate: units
      // produced while generating/connecting resolvents become fixed first,
      // then a successfully eliminated pivot is removed, and only afterward
      // can propagation fix more variables.  Formula rules publish those
      // effects to the policy as trail and retirement events, so remember the
      // candidate boundary and consume the two streams in that same order.
      // Deferring all BVE retirements to the slot boundary batches hundreds
      // of arbitrary score-heap removals and changes the physical heap even
      // when the final formula and scores are identical.
#ifdef TACTIX_ENABLE_EVENT_TRACE
      Var trace_bve_var = INVALID_VAR;
      const uint64_t trace_bve_eliminated_before = k.stats.bve_eliminated;
#endif
      const auto applied = port::complete_action(ctx.completion, FeedbackKind::Rule,
          r.name, payload, [&] {
            const uint64_t spent = e.meter ? e.meter(k) : 0;
            return slot_feedback(k, e, r, payload,
                e.meter ? strategy::WorkLimit::bounded(limit >= spent ? limit - spent : 0)
                        : strategy::WorkLimit::unlimited(), "native-dosed-candidate");
          }, [&] {
        auto published = port::execute_action(
          k, execution::RuleApplicationRequest{
                 execution::RootPublication::NewAssignments, bve_entry}, [&] {
        TickSinkScope sink(k, e.tick_counter);
#ifdef TACTIX_ENABLE_EVENT_TRACE
        if (bve_entry) {
          Lit candidate;
          if (lit_of_instance(k, payload, &candidate)) {
            trace_bve_var = var_of(candidate);
            event_trace_bve_candidate(k, trace_bve_var, "begin", e.meter(k),
                                      limit, false);
          }
        }
#endif
        const ApplyResult result = metered_rule_apply(k, e.meter, [&] {
          if (!bve_entry)
            return port::apply_rule_instance(r, k, payload);
          const strategy::InvocationResult invoked =
              port::bve_apply_one(*bve_invocation_session, payload);
          // Existing configurations predate typed validation.  The surrounding
          // scheduler has already checked this candidate and its stop/guard;
          // preserve that legacy transition if an as-yet-unmigrated setting
          // cannot be represented instead of silently turning it into NoOp.
          if (!invoked.applications)
            return port::apply_rule_instance(r, k, payload);
          return invoked.rule_result;
        });
#ifdef TACTIX_ENABLE_EVENT_TRACE
        if (trace_bve_var != INVALID_VAR)
          event_trace_bve_candidate(k, trace_bve_var, "end", e.meter(k),
                                    limit,
                                    k.stats.bve_eliminated !=
                                        trace_bve_eliminated_before);
#endif
        ctx.last_result = result;
        return result;
      }, HeuristicRulePublisher{});
        candidate_work.reset();
        return published;
      }, [](const execution::RuleApplicationResult &published) {
        return strategy::controller::feedback_outcome(published.result);
      });
      const ApplyResult ar = applied.result;
      if (initial_fast_bve && !k.inconsistent) {
        // The attempt clears `eliminate` after it ran, overriding re-arms by
        // its own strengthened or weakened clauses.
        Lit attempted;
        if (lit_of_instance(k, payload, &attempted))
          port::execute_action(
              k, execution::RetireBveCandidateRequest{var_of(attempted)});
      }
      if (ar == ApplyResult::Unsat || k.inconsistent) {
        ctx.phase_unsat = true;
        break;
      }
      if (ar != ApplyResult::Ok)
        continue; // this candidate changed nothing; try the next
      // Kissat's round repeats on eliminations (`last_round_eliminated`), not
      // on a unit a declined candidate happened to learn.
      if (!bve_entry || k.stats.bve_eliminated != applied.eliminated_before)
        ctx.round_changed = true;
      // resolve.c can learn a unit immediately and then reject the same BVE
      // candidate on its later growth bound.  eliminate.c nevertheless flushes
      // that unit after every candidate, and the rule reports such a decline
      // as a state change, so root propagation follows it here as well.
      const size_t propagation_trail_before = k.trail.size();
      const ApplyResult propagation =
          default_propagate(k, "inprocess-candidate", ctx.completion);
      if (propagation == ApplyResult::Unsat) {
        ctx.phase_unsat = true;
        break;
      }
      if (bve_entry && k.trail.size() != propagation_trail_before)
        heuristics_consume_root_assignments(k, propagation_trail_before);
      if (bve_entry)
        bve_invocation_session->flush_units_while_connected(applied.propagation_begin);
#ifdef TACTIX_ENABLE_EVENT_TRACE
      if (trace_bve_var != INVALID_VAR)
        event_trace_bve_candidate(
            k, trace_bve_var, "postflush", e.meter(k), limit,
            k.stats.bve_eliminated != trace_bve_eliminated_before);
#endif
    }
    if (dynamic_bve)
      bve_invocation_session->finish_ordering();
  }
  if (level_mode && engaged && !ctx.phase_unsat)
    bve_ran_this_phase_ = true;
  (void)elim_at_last_pass;
  if (bve_entry && ctx.outer == 0 && !initial_fast_bve)
    bve_rounds_run_++;
  if (bve_entry && !initial_fast_bve && !ctx.phase_unsat) {
    // Kissat flushes and densely collects after every completed elimination
    // round, including a round which schedules candidates but eliminates
    // none (eliminate.c:416-509).  The dosed scheduler is Tactix's authored
    // elimination round; deferring its lazy garbage until phase exit carries
    // a much larger arena into the following Strengthen round and changes the
    // physical state from which that common consumer reconstructs.
    (void)port::complete_action(ctx.completion, FeedbackKind::Support,
        "CollectGarbage", INSTANCE_CANONICAL, [] {
          execution::NativeFeedbackSpec spec;
          spec.description.description = "completed-bve-round;watch-start=0;sparse=0;preserve-eager=0";
          return spec;
        }, [&] {
          port::execute_action(k, execution::CollectGarbageRequest{});
          return ApplyResult::NoOp;
        });
    // The resolution ceiling belongs to the whole Eliminate phase.  Finish
    // this round's cleanup, but do not start another forward round after
    // exhausting it (Kissat eliminate.c::eliminate_variables).  Equality still
    // admits work; neither heap completeness nor progress identifies this
    // boundary, since even a rejected last candidate can cross the ceiling.
    if (ctx.group == SlotGroup::Eliminate && e.meter(k) > limit)
      ctx.eliminate_limit_reached = true;
  }
  e.tm.admitted++; // the dosed branch ran; it exits here, not below
  return SlotStep::Next;
}

bool DefaultInprocessScheduler::factor_entry_declines(const Kernel &k, SlotEntry &e) {
  const auto &cfg = config();
  // This remains an explicit typed diagnostic arm. Formula classification
  // never changes it; production admits both startup and repeated Factor work.
  if (k.stats.conflicts > 0 && !cfg.factor_repeated_enabled) {
    e.tm.dec_profile++;
    return true;
  }
  const uint64_t active = active_variables(k);
  // A nonzero ceiling is an explicit diagnostic arm. Production leaves it at
  // zero and follows Kissat's factordelay predicate below for every formula.
  if (cfg.factor_active_ceiling && active >= cfg.factor_active_ceiling)
    factor_large_formula_ = true;
  if (factor_large_formula_) {
    e.tm.dec_warmup++;
    return true;
  }
  uint64_t threshold = 1;
  const uint32_t decades =
      bve_rounds_run_ > cfg.factor_decade_round_cap
          ? cfg.factor_decade_cap
          : bve_rounds_run_ + cfg.factor_decade_offset;
  for (uint32_t d = 0; d < decades; d++)
    threshold *= cfg.factor_decade_base;
  if (active >= threshold) {
    e.tm.dec_warmup++;
    return true;
  }
  // `limits.factor.marked >= statistics.literals_factor` is a call-level
  // cumulative watermark, not a scan of the current `flags->factor` bits.
  // A completed Factor call records the counter *after* creating its own
  // divider/quotient clauses, so those self-produced marks do not schedule an
  // otherwise material-free second call.  An interrupted call leaves the
  // watermark unchanged and therefore remains retryable.
  if (factor_marked_watermark_ >= k.factor_literals_marked) {
    e.tm.dec_material++;
    return true;
  }
  return false;
}

void DefaultInprocessScheduler::factor_dose_config(Kernel &k, SlotEntry &,
                                                   PolicyCompletion *completion) {
  set_factor_composite(k, true, completion);
}

void DefaultInprocessScheduler::set_factor_composite(Kernel &k, bool composite,
                                                     PolicyCompletion *completion) {
  strategy::FactorParameters factor = strategy::FactorParameters::from_kernel(k);
  factor.composite = composite;
  port::execute_action(k, execution::ParameterInstallationRequest{factor},
                            completion, "Factor");
}

ApplyResult DefaultInprocessScheduler::run_vivify_dose(Kernel &k, SlotEntry &e,
                                           const RuleVTable &r,
                                           uint64_t limit, PolicyCompletion *completion) {
  // `vivifyfocusedtiers=1` is Kissat's shipped policy: every Vivify band reads
  // the focused-mode tier snapshot, independent of the current search mode.
  // A stable OTFS retier legitimately poses the stable snapshot for the
  // analysis it resumes, so pose the Vivify consumer here instead of relying
  // on the last Reduce to have left these shared fields in focused mode.
  pose_vivify_tiers(k, retier_schedule(), config(), completion);
  const auto &cfg = config();
  const uint64_t remaining0 = ledger_remaining(e, k, limit);
  const uint64_t remaining = remaining0 ? remaining0 : 1;
  const uint64_t dose_start = e.meter(k);
  const bool irr_delayed = k.stats.conflicts > 0 && e.delay_count > 0;
  if (irr_delayed)
    e.delay_count--;
  ApplyResult ar = ApplyResult::NoOp;
  // Kissat advances one cumulative probing-tick ceiling by each band's
  // proportional share (vivify.c:1455-1490).  Keep the same ledger here:
  // unused work from an early tier belongs to every later tier, while an
  // overrun is precharged rather than granting the next call a fresh dose.
  const uint64_t denominator = irr_delayed ? cfg.vivify_redundant_share
                                           : cfg.vivify_total_share;
  auto share = [&](uint32_t weight) {
    return remaining / denominator * weight +
           remaining % denominator * weight / denominator;
  };
  uint64_t cumulative_budget = 0;
  port::VivifyDoseScope dose(k);
  const auto apply_band = [&](uint32_t band_scope, uint64_t cumulative) {
    const uint64_t spent = e.meter(k) - dose_start;
    // The band is a typed Vivify invocation: scope plus its cumulative ledger.
    strategy::VivifyParameters band = strategy::VivifyParameters::from_kernel(k);
    band.scope = static_cast<uint8_t>(band_scope);
    band.band = true;
    band.band_budget = cumulative;
    band.band_offset = spent;
    return port::complete_action(completion, FeedbackKind::Rule, r.name,
        INSTANCE_CANONICAL, [&] {
          auto spec = slot_feedback(k, e, r, INSTANCE_CANONICAL,
              strategy::WorkLimit::bounded(cumulative >= spent ? cumulative - spent : 0),
              "native-vivify-band");
          spec.description.invocation->parameters = band;
          spec.description.description += ";scope=" + std::to_string(band_scope) +
              ";cumulative=" + std::to_string(cumulative) + ";offset=" + std::to_string(spent);
          return spec;
        }, [&] { return port::execute_action(k, r,
            execution::VivifyBandRequest{band, e.tick_counter, e.meter}); },
        [&](ApplyResult result) { return execution::native_rule_outcome(k, r.name, result); });
  };
  const uint32_t passes =
      static_cast<uint32_t>(cfg.vivify_tier_scopes.size());
  // Equal focused tier-1 and tier-2 glue limits leave tier 2 empty.  Kissat
  // then hands tier 2's share to tier 1 up front and never runs the tier-2
  // round, so no flush/rewatch happens for it (vivify.c:kissat_vivify,
  // "tier1 matches tier2 thus using tier2 budget for tier1"). Without the
  // merge tier 1 stops at half its budget and the leftover flows to tier 3.
  std::array<uint32_t, 3> tier_shares = cfg.vivify_tier_shares;
  if (k.tier1 == k.tier2 && tier_shares[0] && tier_shares[1]) {
    tier_shares[0] += tier_shares[1];
    tier_shares[1] = 0;
  }
  for (uint32_t p = 0; p < passes && ar != ApplyResult::Unsat && !k.inconsistent;
       p++) {
    if (tier_shares[p] == 0)
      continue; // a band without a share is not a round
    cumulative_budget += share(tier_shares[p]);
    const ApplyResult a = apply_band(cfg.vivify_tier_scopes[p], cumulative_budget);
    if (a == ApplyResult::Unsat ||
        (a == ApplyResult::Ok && ar != ApplyResult::Unsat))
      ar = a;
  }
  if (!irr_delayed && ar != ApplyResult::Unsat && !k.inconsistent) {
    cumulative_budget += share(cfg.vivify_total_share -
                               cfg.vivify_redundant_share);
    const uint64_t tried_before = k.stats.vivify_checked;
    const auto produced = [&] {
      // Kissat increments vivifier.vivified for every successful candidate
      // terminal, not only learned replacements (vivify.c:1129-1217,
      // 1313-1315).  Root-schedule cleanup is outside the tried population.
      return k.stats.vivify_subsumed + k.stats.vivify_shrunken +
             k.stats.vivify_implied + k.stats.vivify_asymmetric +
             k.stats.vivify_instantiated;
    };
    const uint64_t produced_before = produced();
    const ApplyResult ar2 = apply_band(2, cumulative_budget);
    if (ar2 == ApplyResult::Unsat)
      ar = ar2;
    else if (ar2 == ApplyResult::Ok && ar != ApplyResult::Ok)
      ar = ar2;
    if (k.stats.conflicts > 0) {
      const uint64_t tried = k.stats.vivify_checked - tried_before;
      const uint64_t produced_now = produced() - produced_before;
      if (tried == 0 ||
          static_cast<unsigned __int128>(produced_now) * PERCENT_SCALE <
              static_cast<unsigned __int128>(tried) *
                  cfg.vivify_irredundant_productivity_percent) {
        update_inprocess_delay(e.delay_current,e.delay_count,true);
      } else {
        update_inprocess_delay(e.delay_current,e.delay_count,false);
      }
    }
  }
  return ar;
}

ApplyResult DefaultInprocessScheduler::run_budgeted_batch(Kernel &k, SlotEntry &e,
                                              const RuleVTable &r,
                                              uint64_t limit,
                                              bool factor_entry, PolicyCompletion *completion) {
  const uint64_t remaining0 = ledger_remaining(e, k, limit);
  const bool substitute = !std::strcmp(r.name, "Substitute");
  // Substitute's effort is tested after an indivisible productive round.
  // A zero allowance still admits that mandatory first round; other
  // budget-aware rules retain a one-tick hand-off so they can make progress.
  const uint64_t handed = substitute ? remaining0
                                     : (remaining0 ? remaining0 : 1);
  const uint64_t spent_before = e.meter(k);
  bool substitute_round_budget_break = false;
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  e.journal_last_round_spent = 0;
  e.journal_rounds = 0;
  e.journal_round_budget_break = false;
#endif
  // kissat substitute.c runs round one of every authored call unconditionally;
  // after a round retires a variable its own effort is compared with the
  // handed allowance (10 per mille of the Probe reference), and an overrun
  // suppresses the optional next round, never a later call. `complete` during
  // initial probing ignores the comparison.
  strategy::SubstituteParameters substitute_rounds =
      strategy::SubstituteParameters::from_kernel(k);
  if (substitute) {
    substitute_rounds.max_rounds = config().substitute_rounds;
    substitute_rounds.round_effort_ceiling = k.stats.conflicts != 0;
  }
  if (factor_entry)
    factor_dose_config(k, e, completion);
  // The fused Strengthen pass reproduces Kissat's forward-step ceiling at the
  // only reference cut point: strictly between complete candidates.  The
  // standalone C1 decomposition retains its diagnostic per-partner budget.
  // Both cuts are resumable through the touched-variable substrate.
  std::optional<port::WorkSettingsScope> work;
  work.emplace(
      k, execution::WorkSettingsRequest{
             handed, std::nullopt, std::nullopt,
             config().forward_midround_budget &&
                 (!std::strcmp(r.name, "SubsumeForward") ||
                  !std::strcmp(r.name, "Strengthen"))},
      execution::WorkSettingsExit::ClearBudget);
  const ApplyResult ar = port::complete_action(completion, FeedbackKind::Rule,
      r.name, INSTANCE_CANONICAL, [&] {
        auto spec = slot_feedback(k, e, r, INSTANCE_CANONICAL,
            strategy::WorkLimit::bounded(handed), substitute
                ? "native-substitute-rounds" : "native-budgeted-batch");
        if (!std::strcmp(r.name, "Sweep")) {
          const auto *posed = std::get_if<SweepLimits>(&e.parameters);
          spec.description.invocation->parameters = posed ? *posed : configured_sweep_limits(config());
        }
        if (substitute) spec.description.invocation->parameters = substitute_rounds;
        if (substitute) spec.description.description += ";max-rounds=" +
            std::to_string(config().substitute_rounds) + ";complete=" + std::to_string(k.stats.conflicts == 0);
        return spec;
      }, [&] {
  ApplyResult ar;
  {
    TickSinkScope sink(k, e.tick_counter);
    if (!std::strcmp(r.name, "Sweep")) {
      const auto *posed = std::get_if<SweepLimits>(&e.parameters);
      const SweepLimits lim = posed ? *posed : configured_sweep_limits(config());
      ar = port::apply_rule_structured(r, k, "budgeted-batch", [&] {
        return port::execute_action(k, execution::SweepRequest{lim});
      });
    } else if (substitute) {
      // The typed round batch retains one watch session; publication of each
      // round's roots and retirements is DefaultPolicy's guidance profile.
      const execution::SubstituteRoundsResult rounds = [&] {
        execution::SubstituteRoundsResult out;
        (void)port::apply_rule_structured(r, k, "rounds", [&] {
          out = port::execute_substitute_rounds(
              k, substitute_rounds, [&](size_t trail_begin) {
                default_publish_substitute_round(k, trail_begin);
              });
          return out.result;
        });
        return out;
      }();
      ar = rounds.result;
      substitute_round_budget_break = rounds.round_budget_break;
#ifdef TACTIX_ENABLE_RULE_JOURNAL
      e.journal_rounds = rounds.rounds;
      e.journal_last_round_spent = rounds.last_round_effort;
      e.journal_round_budget_break = rounds.round_budget_break;
#endif
    } else {
      ar = metered_rule_apply(k, e.meter, [&] {
        return port::execute_action(k, execution::RegisteredRuleRequest{r});
      });
    }
  }
  if (factor_entry && k.factor_completed)
    factor_marked_watermark_ = k.factor_literals_marked;
  work.reset();
  return ar;
  }, [&](ApplyResult result) { return execution::native_rule_outcome(k, r.name, result); });
  // Every budgeted dose leaves composite Factor matching off; only a Factor
  // dose turns it on, so the install is needed only after one.
  if (k.factor_composite)
    set_factor_composite(k, false, completion);
  const uint64_t spent = e.meter(k) - spent_before;
  e.tm.handed += handed;
  e.tm.spent += spent;
  if (spent > 0)
    e.doses++;
  if (spent > 0 &&
      (substitute ? !substitute_round_budget_break : spent <= handed)) {
    e.tm.completed++;
    e.binaries_mark = k.binary_ids.size();
    e.arena_mark = k.arena.size();
    e.root_fixed_mark = kernel_logical_root_fixed(k);
  }
  return ar;
}

DefaultInprocessScheduler::SlotStep
DefaultInprocessScheduler::run_batch_entry(Kernel &k, SlotEntry &e, const RuleVTable &r,
                               SlotPhaseCtx &ctx) {
  const uint64_t limit = e.limit;
  ctx.last_result = ApplyResult::NoOp;
  const bool substitute = !std::strcmp(r.name, "Substitute");
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  e.journal_last_round_spent = 0;
  e.journal_rounds = 0;
  e.journal_round_budget_break = false;
#endif
  // Substitute has no call-entry effort gate in Kissat. Its first round is
  // mandatory and the round-local comparison lives in run_budgeted_batch.
  if (!substitute && e.meter && e.meter(k) > limit) {
    e.tm.dec_budget++;
    return SlotStep::Next;
  }
  const bool factor_entry = e.budget_aware && !std::strcmp(r.name, "Factor");
  if (factor_entry && factor_entry_declines(k, e))
    return SlotStep::Next;
  // The ordinary begin boundary is before Kissat's call-entry predicates.
  // This second observer-only boundary is the exact point after factordelay
  // and the cumulative marked-material watermark admit the call, but before
  // Factor changes representation or formula state.
  if (factor_entry)
    event_trace_slot_boundary(k, r.name, "admitted");
  if (e.closure && k.stats.conflicts > 0 &&
      k.binary_ids.size() <= e.binaries_mark &&
      kernel_logical_root_fixed(k) <= e.root_fixed_mark) {
    e.tm.dec_material++;
    return SlotStep::Next;
  }
  if (closure_delay_ && (e.closure || delay_spec(r.name)) &&
      k.stats.conflicts > 0 && e.delay_count > 0) {
    e.delay_count--;
    e.tm.dec_delay++;
    return SlotStep::Next;
  }
  if (!r.guard(k)) {
    e.tm.dec_guard++;
    return SlotStep::Next;
  }
  // Per-occurrence strategy parameters are posed for exactly this safe rule
  // application and restored before the scheduler advances. The reference
  // program carries monostate, so its legacy Kernel values and hot path remain
  // unchanged.
  execution::CompletionPublication published(ctx.completion);
  execution::ParameterExecutionScope<
      strategy::SubsumeForwardParameters, strategy::StrengthenParameters,
      strategy::CongruenceParameters, strategy::ParityConsequenceParameters,
      strategy::AffineConsequenceParameters,
      strategy::ExactCoverExtensionParameters,
      strategy::IndependentSetCoverParameters,
      strategy::Gf3AffineConsequenceParameters,
      strategy::DirectCspSupportParameters,
      strategy::DirectPhpRefutationParameters,
      strategy::Gf3GaussianConsequenceParameters,
      strategy::Gf3GeneralRowConsequenceParameters,
      strategy::TseitinCycleBreakingParameters,
      strategy::InstantiateParameters,
      strategy::BackboneParameters, strategy::VivifyParameters>
      parameters(k, e.parameters);
  e.tm.admitted++;
  const DelaySpec *const dspec = delay_spec(r.name);
  const uint64_t yield_before = dspec ? dspec->yield(k) : 0;
  const uint64_t denom_before = dspec ? dspec->denom(k) : 0;
  const bool budgeted = e.meter && !e.dosed && e.budget_aware;
  const bool vivify_dose = budgeted && !std::strcmp(r.name, "Vivify");
  const bool forward_entry = ctx.group == SlotGroup::Eliminate &&
                             !std::strcmp(r.name, "Strengthen");
  const uint32_t forward_trail_from = k.propagate_head;
  assert(k.external_to_internal.size() <= UINT32_MAX);
  const uint32_t factor_external_begin =
      factor_entry ? static_cast<uint32_t>(k.external_to_internal.size()) : 0;
  ApplyResult ar;
  if (vivify_dose) {
    ar = run_vivify_dose(k, e, r, limit, ctx.completion);
  } else if (budgeted) {
    ar = run_budgeted_batch(k, e, r, limit, factor_entry, ctx.completion);
  } else {
    ar = port::complete_action(ctx.completion, FeedbackKind::Rule, r.name,
        INSTANCE_CANONICAL, [&] {
          auto spec = slot_feedback(k, e, r, INSTANCE_CANONICAL,
              strategy::WorkLimit::unlimited(), "native-unbudgeted-batch");
          if (substitute) {
            strategy::SubstituteParameters rounds = strategy::SubstituteParameters::from_kernel(k);
            rounds.max_rounds = k.stats.conflicts == 0 ? config().substitute_rounds
                                                       : config().substitute_search_rounds;
            spec.description.invocation->parameters = rounds;
            spec.description.description += ";max-rounds=" + std::to_string(rounds.max_rounds);
          }
          return spec;
        }, [&] {
    ApplyResult result;
    TickSinkScope sink(k, e.tick_counter);
    // Kissat's two preprocessing follow-ups call substitute with
    // `complete=true`: a productive rewrite can expose another binary SCC,
    // and the shipped `substituterounds=2` consumes that closure immediately.
    // Unbudgeted search-time calls retain the ordinary one-round contract.
    // Publish every round's roots/retirements before reconnecting watches.
    if (substitute) {
      strategy::SubstituteParameters rounds = strategy::SubstituteParameters::from_kernel(k);
      rounds.max_rounds = k.stats.conflicts == 0 ? config().substitute_rounds
                                                 : config().substitute_search_rounds;
      const std::string candidate =
          "policy-complete-rounds=" + std::to_string(rounds.max_rounds);
      result = metered_rule_apply(k, journal_effort_meter(e, r), [&] {
        return port::apply_rule_structured(r, k, candidate.c_str(), [&] {
          return port::execute_substitute_rounds(
                     k, rounds, [&](size_t trail_begin) {
                       default_publish_substitute_round(k, trail_begin);
                     })
              .result;
        });
      });
    } else {
      result = metered_rule_apply(k, journal_effort_meter(e, r),
                              [&] {
        return port::execute_action(k, execution::RegisteredRuleRequest{r});
      });
    }
    return result;
    }, [&](ApplyResult result) { return execution::native_rule_outcome(k, r.name, result); });
  }
  ctx.last_result = ar;
  if (factor_entry && !k.stats.conflicts &&
      structured_fastel_ownership_.symmetry_applied) {
    assert(k.external_to_internal.size() <= UINT32_MAX);
    const uint32_t factor_external_end =
        static_cast<uint32_t>(k.external_to_internal.size());
    if (factor_external_end > factor_external_begin) {
      auto &ranges = structured_fastel_ownership_.coupled_factor_ranges;
      if (!ranges.empty() && ranges.back().end == factor_external_begin)
        ranges.back().end = factor_external_end;
      else
        ranges.push_back({factor_external_begin, factor_external_end});
    }
  }
  if (factor_entry)
    event_trace_target_state(k, "factor-stamps-pre",
        k.fresh_var_events.empty() ? 0 : k.fresh_var_events.back().end - k.fresh_var_events.back().first);
  heuristics_consume_rule_advice(k);
  if (factor_entry)
    event_trace_target_state(k, "factor-stamps-post", 0);
  // Match Kissat's post-Factor return point after sparse reconstruction and
  // fresh-variable heuristic adjustment, but before caller propagation or a
  // later inprocessing slot can change the semantic formula.
  if (factor_entry)
    event_trace_slot_boundary(k, r.name, "applied");
  if (e.closure) {
    e.binaries_mark = k.binary_ids.size();
    e.arena_mark = k.arena.size();
    e.root_fixed_mark = kernel_logical_root_fixed(k);
  }
  // Kissat bumps/reduces the named Congruence and Sweep delay ladders after
  // their preprocessing calls too.  Suppressing this update at conflict zero
  // made Tactix run the first search-time Congruence while Kissat consumed its
  // one-call delay.  Substitute has no Kissat delay object; retain its local
  // closure backoff only for search-time applications.
  const bool update_named_delay = dspec;
  const bool update_local_closure_delay = e.closure && k.stats.conflicts > 0;
  if (closure_delay_ &&
      (update_named_delay || update_local_closure_delay)) {
    bool unproductive;
    if (dspec) {
      const uint64_t yield = dspec->yield(k) - yield_before;
      const uint64_t denom =
          dspec->denom_delta ? dspec->denom(k) - denom_before : dspec->denom(k);
      const uint32_t permille = config().*(dspec->permille);
      unproductive = denom == 0 ||
                     yield * PERMILLE_SCALE < uint64_t(permille) * denom;
    } else {
      unproductive = ar != ApplyResult::Ok;
    }
    update_inprocess_delay(e.delay_current,e.delay_count,unproductive);
  }
  if (ar == ApplyResult::Ok)
    e.tm.ok++;
  if (ar == ApplyResult::Unsat || k.inconsistent) {
    ctx.phase_unsat = true;
    return SlotStep::Stop;
  }
  if (forward_entry) {
    // C2's internal partial scan can consume the cursor while leaving full
    // clause consequences pending. Publish its roots in assignment order,
    // then close the reconnected suffix before any following slot observes it.
    heuristics_consume_root_assignments(k, forward_trail_from);
    const size_t closure_trail_from = k.trail.size();
    const ApplyResult closure = port::complete_action(ctx.completion,
        FeedbackKind::Support, "ForwardClosure", INSTANCE_CANONICAL, [&] {
          execution::NativeFeedbackSpec spec;
          spec.description.description = "trail-from=" + std::to_string(forward_trail_from);
          spec.description.effort_currency = "propagations";
          spec.meter = [](const Kernel &kernel) { return kernel.stats.propagations; };
          return spec;
        }, [&] {
          const ApplyResult result = port::execute_action(
              k, execution::ForwardClosureRequest{forward_trail_from});
          heuristics_consume_root_assignments(k, closure_trail_from);
          return result;
        });
    if (closure == ApplyResult::Unsat) {
      ctx.phase_unsat = true;
      return SlotStep::Stop;
    }
  }
  if (ar != ApplyResult::Ok)
    return SlotStep::Next;
  ctx.round_changed = true;
  const ApplyResult propagation = default_propagate(k, "inprocess-batch", ctx.completion);
  if (propagation == ApplyResult::Unsat) {
    ctx.phase_unsat = true;
    return SlotStep::Stop;
  }
  return SlotStep::Next;
}

int DefaultInprocessScheduler::run_structured_root(
    Kernel &k, volatile sig_atomic_t *stop, PolicyCompletion *completion) {
  using strategy::InvocationStatus;
  if (structured_root_consumed_ || k.stats.conflicts)
    return 0;
  structured_root_consumed_ = true;
  structured_fastel_ownership_ = {};
  const size_t structured_trail_before = k.trail.size();
  const uint64_t structured_sr_applied_before = k.stats.sr_applied;
  assert(k.external_to_internal.size() <= UINT32_MAX);
  structured_fastel_ownership_.external_begin =
      static_cast<uint32_t>(k.external_to_internal.size());
  structured_fastel_ownership_.external_end =
      structured_fastel_ownership_.external_begin;
  struct CaptureStructuredFastelOwnership {
    Kernel &kernel;
    StructuredFastelOwnership &ownership;
    size_t trail_before;
    uint64_t sr_applied_before;
    ~CaptureStructuredFastelOwnership() {
      assert(kernel.external_to_internal.size() <= UINT32_MAX);
      ownership.grew_root_trail = kernel.trail.size() > trail_before;
      ownership.symmetry_applied =
          kernel.stats.sr_applied > sr_applied_before;
      ownership.external_end =
          static_cast<uint32_t>(kernel.external_to_internal.size());
    }
  } capture_ownership{k, structured_fastel_ownership_,
                      structured_trail_before, structured_sr_applied_before};
  ++structured_root.runs;
  const bool large_domain =
      k.nvars > config().structured_root_large_domain_limit;
  const bool bounded_backtracking =
      k.nvars >= config().structured_root_backtracking_domain_limit;
  const auto &stages =
      large_domain
          ? config().structured_root_large_domain_stages
          : bounded_backtracking
                ? config().structured_root_bounded_backtracking_stages
                : config().structured_root_stages;
  if (stages.empty())
    return 0;
  if (k.proof && !k.proof->supports_sr()) {
    ++structured_root.proof_format_declines;
    structured_root.terminal = InvocationStatus::Inapplicable;
    return 0;
  }
  RecordedExecutionContext<execution::ExecutionContext::Inprocessing> scope(k, completion);
  struct LeaveOnReturn {
    RecordedExecutionContext<execution::ExecutionContext::Inprocessing> &scope;
    ~LeaveOnReturn() { scope.leave(); }
  } leave_on_return{scope};
  const auto hard_discovery_limit =
      config().structured_root_sr_discovery_budget;
  uint64_t progress_window = 0;
  const bool progress_feedback =
      config().structured_root_progress_feedback &&
      (!large_domain ||
       config().structured_root_large_domain_progress_feedback);
  if (progress_feedback) {
    const uint64_t scale =
        config().structured_root_progress_ticks_per_variable;
    const uint64_t variable_window =
        scale && k.nvars > UINT64_MAX / scale
            ? UINT64_MAX
            : scale * static_cast<uint64_t>(k.nvars);
    progress_window =
        std::max(config().structured_root_progress_minimum_ticks,
                 variable_window);
  }
  strategy::WorkLimit initial_discovery_limit = hard_discovery_limit;
  if (progress_window &&
      (hard_discovery_limit.is_unlimited() ||
       hard_discovery_limit.amount() > progress_window))
    initial_discovery_limit = strategy::WorkLimit::bounded(progress_window);
  uint64_t granted_discovery_limit =
      initial_discovery_limit.is_unlimited()
          ? UINT64_MAX
          : initial_discovery_limit.amount();
  strategy::Allowance sr_discovery_allowance(
      strategy::WorkUnit::SrDiscoveryTicks, initial_discovery_limit);
  structured_root.sr_discovery_progress_window = progress_window;
  structured_root.stages.resize(stages.size());
  bool prior_stage_progress = false;
  const uint64_t unique_literal_amo_before =
      k.stats.sr_unique_literal_amo_binaries;
  const uint64_t nonforcing_cuts_before =
      k.stats.sr_nonforcing_cuts_declined;
  for (size_t index = 0; index < stages.size(); ++index) {
    const auto stage = DefaultStrategyConfig::structured_root_stage_after_feedback(
        stages[index], prior_stage_progress,
        k.stats.sr_unique_literal_amo_binaries > unique_literal_amo_before,
        k.stats.sr_nonforcing_cuts_declined > nonforcing_cuts_before);
    auto &telem = structured_root.stages[index];
    ++telem.runs;
    strategy::OnlineStrategySequence executor;
    std::string error;
    if (!executor.configure(stage, 1,
                            strategy::ReconfigureMode::ResetRuntime, &error)) {
      telem.terminal = structured_root.terminal = InvocationStatus::InvalidRequest;
      return 0;
    }
    const uint64_t started = now_nanos();
    const auto finish = [&] {
      telem.sequence_passes = executor.rounds();
      telem.nanos += now_nanos() - started;
      structured_root.sequence_passes += telem.sequence_passes;
      structured_root.nanos += telem.nanos;
    };
    while (!executor.complete()) {
      // Rule-local probing and BVE snapshots must end with each application;
      // collection may relocate clauses at the following atomic boundary.
      assert(k.level == 0 && !k.probing.recording && !k.bve_occurrence_count_image);
      const auto advanced =
          executor.advance_with(
              k, [&](const strategy::OnlineInvocation &step, auto &&apply) {
                return port::complete_action(completion, FeedbackKind::Rule,
                    step.rule.c_str(), INSTANCE_CANONICAL, [&] {
                      execution::NativeFeedbackSpec spec;
                      spec.description.description = "structured-stage=" + std::to_string(index) +
                          ";revision=1;step=" + std::to_string(step.id) +
                          ";discovery-consumed=" + std::to_string(sr_discovery_allowance.consumed()) +
                          ";discovery-limit=" + std::to_string(granted_discovery_limit);
                      spec.description.invocation = step;
                      if (step.budget.is_unlimited())
                        spec.description.effort_allowance = step.budget;
                      else {
                        size_t position = 0;
                        while (position < stage.program.sequence.size() &&
                               stage.program.sequence[position].id != step.id) ++position;
                        assert(position < stage.program.sequence.size());
                        const uint64_t consumed = executor.consumed(position);
                        spec.description.effort_allowance = strategy::WorkLimit::bounded(
                            step.budget.amount() >= consumed ? step.budget.amount() - consumed : 0);
                      }
                      return spec;
                    }, std::forward<decltype(apply)>(apply));
              }, stop, &sr_discovery_allowance,
              strategy::DiscoveryFeedbackPolicy{
                  progress_window,
                  hard_discovery_limit.is_unlimited()
                      ? UINT64_MAX
                      : hard_discovery_limit.amount()});
      structured_root.sr_discovery_ticks =
          sr_discovery_allowance.consumed();
      structured_root.sr_discovery_overflow =
          sr_discovery_allowance.accounting_overflow();
      ++telem.advances;
      ++structured_root.advances;
      telem.applications += advanced.applications;
      structured_root.applications += advanced.applications;
      telem.effective += advanced.made_rule_progress;
      structured_root.effective += advanced.made_rule_progress;
      uint64_t discovery_credit_units =
          advanced.discovery_progress_units >
                  UINT64_MAX - uint64_t(advanced.made_rule_progress)
              ? UINT64_MAX
              : advanced.discovery_progress_units +
                    uint64_t(advanced.made_rule_progress);
      if (advanced.discovery_entry_required &&
          discovery_credit_units != UINT64_MAX)
        ++discovery_credit_units;
      if (progress_window && discovery_credit_units &&
          !sr_discovery_allowance.accounting_overflow()) {
        const uint64_t consumed = sr_discovery_allowance.consumed();
        const uint64_t base = std::max(granted_discovery_limit, consumed);
        const uint64_t credit =
            discovery_credit_units &&
                    progress_window > UINT64_MAX / discovery_credit_units
                ? UINT64_MAX
                : progress_window * discovery_credit_units;
        uint64_t renewed = credit > UINT64_MAX - base
                               ? UINT64_MAX
                               : base + credit;
        if (!hard_discovery_limit.is_unlimited())
          renewed = std::min(renewed, hard_discovery_limit.amount());
        if (renewed > granted_discovery_limit) {
          granted_discovery_limit = renewed;
          sr_discovery_allowance.set_limit(
              strategy::WorkLimit::bounded(renewed));
          ++structured_root.sr_discovery_renewals;
        }
      }
      telem.terminal = structured_root.terminal = advanced.status;
      switch (advanced.status) {
      case InvocationStatus::Completed:
      case InvocationStatus::NoChange:
      case InvocationStatus::ScopedFixpoint:
        continue;
      case InvocationStatus::Sat:
        finish();
        return 10;
      case InvocationStatus::Unsat:
        finish();
        return 20;
      case InvocationStatus::BudgetExhausted:
        if (stage.on_budget_exhaustion == strategy::BudgetExhaustionPolicy::Continue ||
            (stage.on_budget_exhaustion == strategy::BudgetExhaustionPolicy::FinishRoundThenStop &&
             !executor.complete()))
          continue;
        finish();
        return 0;
      case InvocationStatus::Interrupted:
      case InvocationStatus::ResourceExhausted:
      case InvocationStatus::Inapplicable:
      case InvocationStatus::InvalidRequest:
      case InvocationStatus::StaleRequest:
      case InvocationStatus::AccountingError:
        // Preserve the visible status and every committed atomic transition.
        // No stale payload is retried or silently advanced by the scheduler.
        finish();
        return 0;
      case InvocationStatus::Conflict:
        std::fprintf(stderr, "c ERROR: root preprocessing returned status %u\n",
                     static_cast<unsigned>(advanced.status));
        std::abort();
      }
    }
    finish();
    prior_stage_progress |= telem.effective != 0;
  }
  return k.inconsistent ? 20 : 0;
}

int DefaultInprocessScheduler::run_initial_fast_elimination(
    Kernel &k, volatile sig_atomic_t *stop, PolicyCompletion *completion) {
  return run_initial_fast_elimination(k, stop, completion, {});
}

int DefaultInprocessScheduler::run_initial_fast_elimination(
    Kernel &k, volatile sig_atomic_t *stop, PolicyCompletion *completion,
    const StructuredFastelOwnership &ownership) {
  assert(config().initial_fast_elimination);
  assert(!initial_fast_elimination_active_);
  // Consume the producer record exactly once. This method is commonly called
  // with the scheduler's own record by const reference, so copy before clearing
  // it; a policy reused without reset must not replay stale SR provenance.
  const StructuredFastelOwnership startup_ownership = ownership;
  structured_fastel_ownership_ = {};
  const uint64_t started = now_nanos();
  const uint64_t startup_allowance = scale_permille(
      config().preprocess_initial_ticks,
      config().initial_fast_elimination_bve_permille);
  const bool discovery_admissible =
      default_fast_elimination_discovery_admissible(
          kernel_active_variables(k), startup_allowance);
  if (kernel_propagation_fixpoint(k) && startup_ownership.grew_root_trail &&
      startup_ownership.symmetry_applied && discovery_admissible) {
    const uint64_t extension_candidates =
        initial_fast_elimination_extension_candidates(
            k, config().initial_fast_elimination_parameters,
            startup_ownership);
    if (default_should_suppress_fast_elimination_after_structured(
            true, extension_candidates)) {
      ++k.stats.fastel_root_suppressed;
      const uint64_t elapsed = now_nanos() - started;
      k.stats.fastel_nanos += elapsed;
      k.stats.inprocess_nanos += elapsed;
      return 0;
    }
  }
  k.stats.inprocess_nanos += now_nanos() - started;
  initial_fast_elimination_active_ = true;
  initial_fast_elimination_ownership_ = startup_ownership;
  initial_fast_elimination_discovery_ticks_ = 0;
  initial_fast_elimination_budget_exhausted_ = false;
  initial_fast_elimination_admitted_ = false;
  initial_fast_elimination_marks_.assign(2 * static_cast<size_t>(k.nvars), 0);
  const uint64_t eliminated_before = k.stats.bve_eliminated;
  const uint64_t resolutions_before = k.stats.bve_resolutions;
  const int result = run_phase(k, SlotGroup::Eliminate, stop, completion);
  // fastel.c ends by setting every variable's eliminate flag again.
  (void)port::execute_action(k, execution::RescheduleBveCandidatesRequest{});
  k.stats.fastel_nanos += now_nanos() - started;
  k.stats.fastel_eliminated += k.stats.bve_eliminated - eliminated_before;
  k.stats.fastel_resolutions += k.stats.bve_resolutions - resolutions_before;
  initial_fast_elimination_marks_.clear();
  initial_fast_elimination_ownership_ = {};
  initial_fast_elimination_active_ = false;
  return result;
}

int DefaultInprocessScheduler::run_phase(Kernel &k, SlotGroup group,
                                  volatile sig_atomic_t *stop, PolicyCompletion *completion) {
  SlotClock &clock = group == SlotGroup::Probe ? probe_clock : elim_clock;
  clock.reductions_mark = k.stats.reductions;
  clock.marks_mark = k.stats.bve_marks;
  clock.fixed_mark = kernel_logical_root_fixed(k);
  clock.blocked = false; // a new blocking episode may now be counted
  forward_pair_limit_ = 0;
  bve_ran_this_phase_ = false;
  // Kissat constructs one embedded Kitten at the beginning of the complete
  // `eliminate()` invocation and releases it after all of that invocation's
  // elimination rounds. `run_phase(Eliminate)` is that lifetime boundary;
  // its outer fixpoint rounds must reuse the same deterministic generator
  // sequence instead of restarting it at each BVE slot visit.
  if (group == SlotGroup::Eliminate && !initial_fast_elimination_active_)
    port::execute_action(k, execution::BeginBveDefinitionSessionRequest{}, completion);
  install_parameters(k, completion);
  factor_bound_snapshot_ =
      couple_factor_bound_ ? bve_level_ : 0;
  const uint64_t phase_props_at_entry = k.stats.propagations;
  const uint64_t phase_nanos_at_entry = now_nanos();
  k.stats.inprocess_phases++;
  // Probe sets Kissat's probing flag before root preparation; Eliminate
  // still uses ordinary search propagation here. Enter the common simplifier
  // scope only after that boundary so Eliminate credits search ticks and
  // saves phases for any newly exposed root implications.
  const execution::RootPreparationRequest preparation{group == SlotGroup::Probe, true};
  const ApplyResult root_result = port::complete_action(completion,
      FeedbackKind::Support, "RootPreparation",
      execution::root_preparation_argument(preparation, group != SlotGroup::Probe), [&] {
        execution::NativeFeedbackSpec spec;
        spec.description.description = std::string("probing=") +
            std::to_string(group == SlotGroup::Probe) + ";clear-unflushed-root=1;purpose=preparation";
        spec.description.effort_currency = "propagations";
        spec.meter = [](const Kernel &kernel) { return kernel.stats.propagations; };
        return spec;
      }, [&] { return port::execute_action(
      k, preparation,
      [&] {
        // Probe's bracket suppresses target capture; Eliminate retains it.
        if (group != SlotGroup::Probe) heuristics_update_target_phases(k);
      },
      [&](const execution::RestartRequest &request) {
        return default_direct_rule(k, "Restart", "keep-level=0;inprocess=1", [&] {
          return port::execute_action(k, request);
        });
      },
      [&] {
        return default_propagate(
            k, group == SlotGroup::Probe ? "probe-root" : "eliminate-root");
      });
  });
  if (root_result == ApplyResult::Unsat) return 20;
  // Slots can defer H publication (notably Sweep's import-ordered union) to
  // a later followup or phase exit. Deliver immutable leaf records only after
  // the enclosing native scope and all of that publication have finished.
  execution::CompletionPublication phase_published(completion);
  port::InprocessPhase phase;
  // The Factor selection's bound is the public entry argument.
  const uint32_t factor_bound = std::max(k.factor_bound, factor_bound_snapshot_);
  (void)port::complete_action(completion, FeedbackKind::Support,
      "InprocessPhaseEntry",
      execution::inprocess_phase_argument(group == SlotGroup::Eliminate, factor_bound), [&] {
        execution::NativeFeedbackSpec spec;
        spec.description.description = std::string("group=") +
            (group == SlotGroup::Probe ? "probe" : "eliminate") +
            ";factor-selection=default;factor-bound=" + std::to_string(factor_bound);
        spec.description.effort_currency = "ticks";
        spec.meter = [](const Kernel &kernel) { return kernel.stats.ticks; };
        return spec;
      }, [&] {
  if (group == SlotGroup::Eliminate)
    event_trace_eliminate_cleanup(k, "entry");
  // Kissat's dense-mode entry (eliminate.c) disconnects large watches before
  // occurrences are built; Probe consumers share the ordinary large occurrence
  // image and keep their own rule-local transitions (phase_execution.hpp).
  phase.enter(k, execution::InprocessPhaseEntryRequest{group == SlotGroup::Eliminate,
                                                       default_factor_match_, factor_bound});
  k.stats.inprocess_entry_nanos += now_nanos() - phase_nanos_at_entry;
    return ApplyResult::Ok;
      });
  // Structured startup owns rule-local occurrence scopes. Construct the
  // ordinary Probe index only after it finishes, so every SR insertion and
  // collection does not maintain an index that no startup consumer needs.
  const int structured_result = group == SlotGroup::Probe
                                    ? run_structured_root(k, stop, completion) : 0;
  if (structured_result == 10) {
    // No pipeline occurrence image has been attached yet. Preserve the
    // terminal model and release only the phase-owned guidance handoff.
    (void)port::complete_action(completion, FeedbackKind::Support,
        "InprocessPhaseExit", INSTANCE_CANONICAL, [] {
          execution::NativeFeedbackSpec spec;
          spec.description.description = "structured-terminal=sat;no-occurrence-session";
          spec.description.effort_currency = "ticks";
          spec.meter = [](const Kernel &kernel) { return kernel.stats.ticks; };
          return spec;
        }, [&] {
          phase.exit_terminal(k);
          k.stats.inprocess_propagations += k.stats.propagations - phase_props_at_entry;
          k.stats.inprocess_nanos += now_nanos() - phase_nanos_at_entry;
          return ApplyResult::Sat;
        });
    return 10;
  }
  const bool structured_unsat = structured_result == 20;
  const uint64_t dense_connect_started = now_nanos();
  (void)port::complete_action(completion, FeedbackKind::Support,
      "OpenOccurrenceSession", INSTANCE_CANONICAL, [&] {
        execution::NativeFeedbackSpec spec;
        spec.description.description = std::string("group=") +
            (group == SlotGroup::Probe ? "probe" : "eliminate") +
            ";track-order=" + (group == SlotGroup::Eliminate ? "1" : "0");
        spec.description.effort_currency = "ticks";
        spec.meter = [](const Kernel &kernel) { return kernel.stats.ticks; };
        return spec;
      }, [&] {
        phase.open_occurrences(k, occurrence_index_);
        return ApplyResult::Ok;
      });
  k.stats.inprocess_dense_connect_nanos +=
      now_nanos() - dense_connect_started;
  SlotPhaseCtx ctx{group, stop, completion};
  ctx.phase_unsat = structured_unsat;
  const std::vector<RuleVTable> &rules = tactix_rules();
  uint64_t active_before = active_variables(k);
  size_t sweep_retired_begin = SIZE_MAX;
  size_t sweep_substituted_begin = SIZE_MAX;
  const int rounds = initial_fast_elimination_active_
                         ? static_cast<int>(
                               config().initial_fast_elimination_rounds)
                         : k.stats.conflicts == 0
                               ? 1
                               : static_cast<int>(config().phase_rounds);
  const uint64_t slot_loop_started = now_nanos();
  for (ctx.outer = 0;
       ctx.outer < rounds && !ctx.phase_unsat &&
       !(stop && *stop);
       ctx.outer++) {
    // `SET_EFFORT_LIMIT(forward_steps)` lives inside Kissat's
    // `forward_subsume_all_clauses`, which is called once per elimination
    // round.  The search-tick reference is shared, but each round receives a
    // fresh delta starting at its current forward_steps counter.  Keep the C1
    // diagnostic decomposition and fused C2 on one allowance within a round,
    // then discard that absolute ceiling before the next round.
    forward_pair_limit_ = 0;
    ctx.round_changed = false;
    (void)port::complete_action(completion, FeedbackKind::Support,
        "BeginForwardRound", INSTANCE_CANONICAL, [&] {
          execution::NativeFeedbackSpec spec;
          spec.description.description = "round=" + std::to_string(ctx.outer);
          return spec;
        }, [&] {
          return port::execute_action(k, execution::BeginForwardRoundRequest{})
                     ? ApplyResult::Ok : ApplyResult::NoOp;
        });
    for (size_t slot = 0; slot < entries.size(); slot++) {
      SlotEntry &e = entries[slot];
      if (e.group != group)
        continue; // the other clock's pipeline: not offered here at all
      const RuleVTable &r = rules[e.rule];
      if (initial_fast_elimination_active_ && std::strcmp(r.name, "BVE"))
        continue;
      if (stop && *stop)
        break; // §6: leave the phase promptly; cleanup runs at its exit
      e.tm.occasions++;
      const bool followup = followup_admissible(e, k, ctx);
      slot_trace_entry(k, e, r, ctx.outer);
      if (!followup) {
        e.tm.dec_preprocess++;
        continue;
      }
      if (!slot_entry_admissible(e, k, ctx))
        continue;
      const bool substitute = !std::strcmp(r.name, "Substitute");
      if (e.enabled && e.meter) {
        const bool forward_round = e.meter == meter_forward_pair;
        if (substitute || group == SlotGroup::Probe || ctx.outer == 0 ||
            forward_round) {
          set_entry_limit(e, k);
        }
      }
#ifdef TACTIX_ENABLE_RULE_JOURNAL
      const std::string invocation =
          std::string("group=") +
          (group == SlotGroup::Probe ? "probe" : "eliminate") +
          ";phase=" + std::to_string(clock.rounds) +
          ";round=" + std::to_string(ctx.outer) +
          ";slot=" + std::to_string(slot) +
          ";occasion=" + std::to_string(e.tm.occasions);
      const bool journal_invocation =
          rule_journal_wants(k, r.name, invocation);
      const auto invocation_meter = journal_effort_meter(e, r);
      const uint64_t invocation_entry =
          journal_invocation && invocation_meter ? invocation_meter(k) : 0;
      const uint64_t invocation_pre_digest =
          journal_invocation ? rule_journal_digest(k) : 0;
      const uint64_t invocation_formula_epoch = k.arena_generation;
      const uint64_t invocation_root_epoch = k.touch_fixed_head;
      const SlotEntry::Telem invocation_tm = e.tm;
#endif
#ifdef TACTIX_ENABLE_EVENT_TRACE
      k.diagnostic_policy_inprocess_bytes =
          diagnostic_non_dense_storage_bytes();
#endif
      event_trace_slot_boundary(k, r.name, "begin");
      const bool sweep = !std::strcmp(r.name, "Sweep");
      const size_t trail_before = k.trail.size();
      const uint64_t root_assignments_before = k.stats.root_assignments;
      const size_t retired_before = k.retired_events.size();
      const size_t substituted_before = sweep ? k.substituted_events.size() : 0;
      const bool dosed_instance = e.dosed && r.instances && r.apply_instance;
      const SlotStep step = dosed_instance ? run_dosed_entry(k, e, r, ctx)
                                           : run_batch_entry(k, e, r, ctx);
      // Kissat's root assignment primitive marks a variable fixed and removes
      // it from both decision rankings immediately.  Formula rules remain
      // policy-agnostic in Tactix, so publish that same lifecycle at the rule
      // boundary, before consuming the rule's eliminated/substituted events.
      // Substitute has already consumed roots/retirements per SCC round;
      // this boundary also consumes roots from its final watch reconnection,
      // after those retirements (substitute.c substitute_rounds).
      // Dosed entries consume direct and propagated roots at each candidate
      // boundary inside run_dosed_entry.  Do not rescan the whole rule-local
      // trail here; batch entries still publish their roots at this boundary.
      if (!dosed_instance &&
          k.stats.root_assignments != root_assignments_before)
        heuristics_consume_root_assignments(k, trail_before);
#ifdef TACTIX_ENABLE_RULE_JOURNAL
      if (journal_invocation) {
        const bool admitted = e.tm.admitted != invocation_tm.admitted;
        const char *decline = "none";
        if (!admitted) {
          if (e.tm.dec_budget != invocation_tm.dec_budget)
            decline = "budget";
          else if (e.tm.dec_guard != invocation_tm.dec_guard)
            decline = "guard";
          else if (e.tm.dec_delay != invocation_tm.dec_delay)
            decline = "delay";
          else if (e.tm.dec_material != invocation_tm.dec_material)
            decline = "material";
          else if (e.tm.dec_warmup != invocation_tm.dec_warmup)
            decline = "warmup";
          else if (e.tm.dec_profile != invocation_tm.dec_profile)
            decline = "profile";
          else
            decline = "scheduler";
        }
        const bool bounded = e.meter != nullptr;
        const uint64_t exit =
            invocation_meter ? invocation_meter(k) : invocation_entry;
        const uint64_t delta = exit - invocation_entry;
        const uint64_t terminal_spent = substitute
                                            ? e.journal_last_round_spent
                                            : delta;
        const uint64_t overshoot = bounded && terminal_spent > e.journal_allowance
                                       ? terminal_spent - e.journal_allowance
                                       : 0;
        const uint64_t post_digest = rule_journal_digest(k);
        const char *terminal =
            !bounded ? "unbounded"
            : overshoot ? "overshot"
                      : terminal_spent == e.journal_allowance ? "exhausted"
                                                              : "within";
        RuleJournalRecord record;
        record.kind = "schedule";
        record.rule = r.name;
        record.event = "inprocess-invocation";
        record.candidate = invocation;
        record.trigger = "default-inprocess-scheduler";
        record.context =
            std::string("currency=") + effort_currency(r.name) +
            ";reference=" + std::to_string(e.journal_reference) +
            ";allowance=" + std::to_string(e.journal_allowance) +
            ";bounded=" + (bounded ? "1" : "0") +
            ";entry=" + std::to_string(invocation_entry) +
            ";exit=" + std::to_string(exit) +
            ";delta=" + std::to_string(delta) +
            ";overshoot=" + std::to_string(overshoot) +
            ";terminal=" + terminal +
            (substitute
                 ? ";rounds=" + std::to_string(e.journal_rounds) +
                       ";last-round=" +
                       std::to_string(e.journal_last_round_spent) +
                       ";round-budget-break=" +
                       (e.journal_round_budget_break ? "1" : "0")
                 : "");
        record.formula_epoch = invocation_formula_epoch;
        record.root_epoch = invocation_root_epoch;
        record.pre_digest = invocation_pre_digest;
        record.guards = std::string("admitted=") + (admitted ? '1' : '0');
        record.antecedents = "slot-ledger";
        record.outcome = admitted ? journal_result(ctx.last_result) : "declined";
        record.decline = decline;
        record.deltas = invocation_pre_digest == post_digest
                            ? "none"
                            : "state-digest-change";
        record.witnesses = "rule-owned";
        record.effort = delta;
        record.budget = bounded ? e.limit : 0;
        record.post_digest = post_digest;
        rule_journal_emit(k, record);
      }
#endif
      if (!std::strcmp(r.name, "Congruence"))
        ctx.congruence_changed = ctx.last_result == ApplyResult::Ok;
      if (sweep) {
        ctx.sweep_changed = ctx.last_result == ApplyResult::Ok;
        if (ctx.sweep_changed) {
          sweep_retired_begin = retired_before;
          sweep_substituted_begin = substituted_before;
        } else if (sweep_retired_begin == SIZE_MAX) {
          port::execute_action(k, execution::PublishRetirementsRequest{});
        }
      } else {
        if (substitute && sweep_retired_begin != SIZE_MAX) {
          // Kissat's post-Sweep Substitute scans one union containing both the
          // representatives proved by Sweep and any older SCC exposed by
          // their binaries.  Sweep has already ordered its own portion;
          // extend that same scan boundary across this mandated follow-up
          // before H consumes either portion at this call boundary.
          // Canonical Substitute already appends each SCC round in import
          // order. Sweep no longer retires inline; sorting this suffix would
          // move later-round removals ahead of earlier-round removals.
          sweep_retired_begin = sweep_substituted_begin = SIZE_MAX;
        }
        if (sweep_retired_begin == SIZE_MAX) {
          // All other Kissat rule-local eliminations are active-flag
          // transitions owned by that call.  Do not let their arbitrary heap
          // removals drift to phase exit.  A productive Sweep is the sole
          // exception: its inline rewrites deliberately wait for the
          // mandatory follow-up Substitute to retire the complete
          // import-ordered union.
          port::execute_action(k, execution::PublishRetirementsRequest{});
        }
      }
#ifdef TACTIX_ENABLE_EVENT_TRACE
      k.diagnostic_policy_inprocess_bytes =
          diagnostic_non_dense_storage_bytes();
#endif
      event_trace_slot_boundary(k, r.name, "end");
      if (step == SlotStep::Stop)
        break;
    }
    if (ctx.eliminate_limit_reached)
      break;
    const bool subsume_pending =
        group == SlotGroup::Eliminate &&
        config().hold_subsume_until_eliminate &&
        (!k.subsume_complete || !k.strengthen_complete);
    if (!ctx.round_changed && subsume_pending)
      k.stats.subsume_holds++;
    const uint64_t active_now = active_variables(k);
    if (active_now == active_before && !subsume_pending)
      break; // outer fixpoint: the round retired and created nothing
    active_before = active_now;
  }
  k.stats.inprocess_slot_loop_nanos += now_nanos() - slot_loop_started;
  const uint64_t phase_exit_started = now_nanos();
  (void)port::complete_action(completion, FeedbackKind::Support,
      "InprocessPhaseExit", INSTANCE_CANONICAL, [&] {
        execution::NativeFeedbackSpec spec;
        spec.description.description = std::string("group=") +
            (group == SlotGroup::Probe ? "probe" : "eliminate") +
            ";sparse-resume-trail=" + std::to_string(phase.sparse_resume_trail());
        spec.description.effort_currency = "ticks";
        spec.meter = [](const Kernel &kernel) { return kernel.stats.ticks; };
        return spec;
      }, [&] {
  if (group == SlotGroup::Eliminate && !ctx.phase_unsat)
    event_trace_eliminate_cleanup(k, "before");
  // A public client leaves the phase with the refutation it observes.
  assert(ctx.phase_unsat == k.inconsistent);
  // Kissat collects the arena only inside an Eliminate round (eliminate.c:519)
  // and at Reduce (reduce.c:202); Eliminate's sparse resume performs the single
  // arena-order collection, so only other groups flush retired redundant
  // clauses. Probe has no pipeline rewatch (probe.c; each dense-mode rule
  // reconnects itself, vivify.c:1265/1276, substitute.c:594, dense.c:105/217),
  // keeping Kissat's stale watch hints
  // (benchmarks/reports/2026-09-02-unit-cost-and-fidelity-stack.md 3.40).
  if (phase.exit(k, ctx.phase_unsat, [&] { heuristics_consume_retirements(k); },
                 [&] { return default_propagate(k, "eliminate-sparse-resume"); }) ==
      ApplyResult::Unsat)
    ctx.phase_unsat = true;
  if (group == SlotGroup::Eliminate && !ctx.phase_unsat)
    event_trace_eliminate_cleanup(k, "after");
  k.stats.inprocess_propagations +=
      k.stats.propagations - phase_props_at_entry;
  const uint64_t phase_finished = now_nanos();
  k.stats.inprocess_exit_nanos += phase_finished - phase_exit_started;
  k.stats.inprocess_nanos += phase_finished - phase_nanos_at_entry;
  return ctx.phase_unsat ? ApplyResult::Unsat : ApplyResult::Ok;
  });
  if (group == SlotGroup::Eliminate && bve_ran_this_phase_ &&
      !initial_fast_elimination_active_ && !ctx.phase_unsat) {
    BveBoundPolicy &bramp = bve_bound ? *bve_bound : default_bve_ramp_;
    const uint32_t last = bramp.num_passes() - 1;
    const uint32_t blim = bramp.bound(last);
    // Pinned Kissat releases the round schedule before final bookkeeping;
    // its outer last_round_eliminated is zero (the round counter shadows it).
    // Every consistent engaged phase therefore advances, even on a quota cut.
    // Preserve that policy contract directly, without copying the shadowing.
    if (bve_level_idx_ >= last) {
      clock.marks_mark = k.stats.bve_marks;
      clock.fixed_mark = kernel_logical_root_fixed(k);
      // Consume added material only at the native maximum-bound exit, never
      // at generic phase entry or when merely advancing the bound ladder.
      clock.material_mark = k.subsume_material_count;
    } else {
      bve_level_ = std::min(bramp.bound(++bve_level_idx_), blim);
      // eliminate.c::try_to_eliminate_all_variables_again: every variable is
      // a candidate again whenever the bound grows.
      (void)port::complete_action(completion, FeedbackKind::Support,
          "RescheduleBveCandidates", INSTANCE_CANONICAL, [&] {
            execution::NativeFeedbackSpec spec;
            spec.description.description =
                "growth-bound=" + std::to_string(bve_level_);
            return spec;
          }, [&] {
            return port::execute_action(k, execution::RescheduleBveCandidatesRequest{})
                       ? ApplyResult::Ok : ApplyResult::NoOp;
          });
      clock.marks_mark = 0;
      clock.fixed_mark = 0;
    }
  }
  // `kissat_probe_initially` does not update `last.ticks.probe`; the first
  // search-time Probe phase therefore measures effort from search tick zero.
  // Only a completed post-search Probe phase advances that anchor.
  if (!initial_fast_elimination_active_ &&
      (group != SlotGroup::Probe || k.stats.conflicts))
    clock.anchor = search_ticks(k);
  if (ctx.phase_unsat)
    return 20;
  if (!initial_fast_elimination_active_) {
    clock.rounds++;
    update_limit(k, group);
  }
  return 0;
}

void DefaultInprocessScheduler::set_entry_limit(SlotEntry &e, Kernel &k) {
  const std::vector<RuleVTable> &rules = tactix_rules();
  const SlotClock &clock =
      e.group == SlotGroup::Probe ? probe_clock : elim_clock;
  const uint64_t now = search_ticks(k);
  uint64_t reference = 0;
  if (now >= clock.anchor)
    reference = now - clock.anchor;
  else
    e.tm.wrapped++; // see Telem::wrapped
  const bool substitute =
      !std::strcmp(rules[e.rule].name, "Substitute");
  if (k.stats.conflicts == 0)
    reference = config().preprocess_initial_ticks;
  else if (!substitute && reference < config().minimum_effort_ticks)
    reference = config().minimum_effort_ticks;
  const bool startup_fast_bve =
      initial_fast_elimination_active_ &&
      !std::strcmp(rules[e.rule].name, "BVE");
  const uint32_t effective_permille =
      startup_fast_bve ? config().initial_fast_elimination_bve_permille
                       : e.permille;
  const uint64_t delta = scale_permille(reference, effective_permille);
  e.limit = e.meter(k) + delta;
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  e.journal_reference = reference;
#endif
  if (strategy_setting("TACTIX_SLOT_TRACE"))
    std::fprintf(stderr,
                 "[slot-limit] %s now=%llu anchor=%llu reference=%llu "
                 "permille=%u delta=%llu meter=%llu\n",
                 rules[e.rule].name, (unsigned long long)now,
                 (unsigned long long)clock.anchor,
                 (unsigned long long)reference, effective_permille,
                 (unsigned long long)delta,
                 (unsigned long long)e.meter(k));
  if (e.meter == meter_forward_pair) {
    if (forward_pair_limit_)
      e.limit = forward_pair_limit_;
    else
      forward_pair_limit_ = e.limit;
  }
  // Kissat's `factoriniticks` belongs to the first factorization invocation,
  // including the preprocessing call from `probe_initially`.  Restricting it
  // to post-search phases silently replaced the shipped 700M first dose with
  // 50 permille of the 10M preprocessing reference (only 0.5M ticks).
  if (e.doses == 0 && e.budget_aware &&
      !std::strcmp(rules[e.rule].name, "Factor"))
    e.limit = e.meter(k) + factor_init_ticks_;
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  const uint64_t entry = e.meter(k);
  e.journal_allowance = e.limit >= entry ? e.limit - entry : 0;
#endif
}

void DefaultInprocessScheduler::report(const Kernel &k) const {
  std::printf("c structured-root runs %llu advances %llu applications %llu "
              "effective %llu passes %llu seconds %.6f terminal %u "
              "proof-format-declines %llu sr-discovery-ticks %llu "
              "sr-discovery-window %llu renewals %llu "
              "sr-discovery-overflow %u\n",
              (unsigned long long)structured_root.runs,
              (unsigned long long)structured_root.advances,
              (unsigned long long)structured_root.applications,
              (unsigned long long)structured_root.effective,
              (unsigned long long)structured_root.sequence_passes,
              structured_root.nanos / 1e9,
              static_cast<unsigned>(structured_root.terminal),
              (unsigned long long)structured_root.proof_format_declines,
              (unsigned long long)structured_root.sr_discovery_ticks,
              (unsigned long long)structured_root.sr_discovery_progress_window,
              (unsigned long long)structured_root.sr_discovery_renewals,
              static_cast<unsigned>(structured_root.sr_discovery_overflow));
  for (size_t i = 0; i < structured_root.stages.size(); ++i) {
    const auto &stage = structured_root.stages[i];
    std::printf("c structured-stage %zu runs %llu advances %llu applications %llu "
                "effective %llu passes %llu seconds %.6f terminal %u\n", i,
                (unsigned long long)stage.runs,
                (unsigned long long)stage.advances,
                (unsigned long long)stage.applications,
                (unsigned long long)stage.effective,
                (unsigned long long)stage.sequence_passes, stage.nanos / 1e9,
                static_cast<unsigned>(stage.terminal));
  }

  std::printf("c meters      probe %llu backbone %llu vivify %llu "
              "instantiate %llu | global ticks %llu\n",
              (unsigned long long)k.stats.probe_ticks,
              (unsigned long long)k.stats.backbone_ticks,
              (unsigned long long)k.stats.vivify_ticks,
              (unsigned long long)k.stats.instantiate_ticks,
              (unsigned long long)k.stats.ticks);
  const std::vector<RuleVTable> &rules = tactix_rules();
  std::printf("c slot-order  authored");
  for (const SlotEntry &e : entries)
    std::printf(" %s", rules[e.rule].name);
  std::printf("\n");
  std::printf("c slot-clock  probe phases %llu next %llu blocked %llu | "
              "eliminate phases %llu next %llu blocked %llu | bve-marks %llu\n",
              (unsigned long long)probe_clock.rounds,
              (unsigned long long)probe_clock.next_conflicts,
              (unsigned long long)probe_clock.blocked_episodes,
              (unsigned long long)elim_clock.rounds,
              (unsigned long long)elim_clock.next_conflicts,
              (unsigned long long)elim_clock.blocked_episodes,
              (unsigned long long)k.stats.bve_marks);
  for (const SlotEntry &e : entries) {
    if (!e.dosed)
      continue;
    std::printf("c cand-order  %-18s arm %-12s cursor %s | lists %llu "
                "candidates %llu rotated %llu advanced %llu ranked %llu "
                "moved %llu\n",
                rules[e.rule].name, candidate_order_name(e.order),
                e.cursor_enabled ? "on " : "off",
                (unsigned long long)e.ord.lists,
                (unsigned long long)e.ord.candidates,
                (unsigned long long)e.ord.rotated,
                (unsigned long long)e.ord.advanced,
                (unsigned long long)e.ord.ranked,
                (unsigned long long)e.ord.moved);
  }
  for (const SlotEntry &e : entries) {
    const SlotEntry::Telem &t = e.tm;
    if (!t.occasions)
      continue;
    const uint64_t declined =
        t.dec_disabled + t.dec_cap + t.dec_preprocess + t.dec_guard +
        t.dec_budget + t.dec_delay + t.dec_material + t.dec_warmup +
        t.dec_profile;
    const char *balance = (t.admitted + declined == t.occasions) ? "" : "  ***UNACCOUNTED***";
    std::printf(
        "c slot %-18s occ %6llu adm %5llu ok %5llu | dec dis %llu cap %llu "
        "pre %llu grd %llu bud %llu dly %llu mat %llu warm %llu prof %llu "
        "| handed %llu spent %llu completed %llu | "
        "wrapped %llu%s\n",
        rules[e.rule].name, (unsigned long long)t.occasions,
        (unsigned long long)t.admitted, (unsigned long long)t.ok,
        (unsigned long long)t.dec_disabled, (unsigned long long)t.dec_cap,
        (unsigned long long)t.dec_preprocess, (unsigned long long)t.dec_guard,
        (unsigned long long)t.dec_budget, (unsigned long long)t.dec_delay,
        (unsigned long long)t.dec_material, (unsigned long long)t.dec_warmup,
        (unsigned long long)t.dec_profile,
        (unsigned long long)t.handed,
        (unsigned long long)t.spent, (unsigned long long)t.completed,
        (unsigned long long)t.wrapped, balance);
    if (t.followup_offered)
      std::printf("c followup %-14s offered %llu taken %llu eager %llu final %llu\n",
                  rules[e.rule].name,
                  (unsigned long long)t.followup_offered,
                  (unsigned long long)t.followup_taken,
                  (unsigned long long)t.followup_eager,
                  (unsigned long long)t.followup_final);
  }
}

int default_solve(Kernel &k, volatile sig_atomic_t *stop) {
  DefaultPolicy policy;
  const int res = policy.solve(k, stop);
  if (strategy_setting("TACTIX_STRATEGY_CONFIG"))
    std::printf("c strategy-config %s\n",
                policy.strategy_config_snapshot().c_str());
  if (strategy_setting("TACTIX_SLOT_STATS"))
    policy.inprocess().report(k);
  return res;
}

} // namespace tactix
