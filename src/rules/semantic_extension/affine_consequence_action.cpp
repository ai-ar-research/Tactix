#include "rules/semantic_extension/affine_consequence_action.hpp"

#include "proof/proof.hpp"
#include "strategy/semantic_variable_advice.hpp"

#include <cstdlib>
#include <limits>
#include <vector>

namespace tactix {
namespace {

namespace action_parameters = strategy;

AffineLiveRequest request_from_kernel(Kernel &kernel, uint64_t action_id,
                                      uint64_t semantic_id) {
  const auto &parameters = kernel.affine_consequence_parameters;
  AffineLiveRequest request;
  request.image.maximum_clauses = parameters.maximum_clauses;
  request.image.maximum_groups = parameters.maximum_groups;
  request.image.maximum_patterns = parameters.maximum_patterns;
  request.image.maximum_elimination_work =
      parameters.maximum_elimination_work;
  request.image.maximum_arity = parameters.maximum_arity;
  request.image.maximum_intermediate_width =
      parameters.maximum_intermediate_width;
  request.image.maximum_sources = parameters.maximum_sources;
  request.image.maximum_output_width = parameters.maximum_output_width;
  request.maximum_snapshot_literals = parameters.maximum_snapshot_literals;
  request.maximum_snapshot_work = parameters.maximum_snapshot_work;
  request.maximum_proof_additions = parameters.maximum_proof_additions;
  request.maximum_proof_deletions = parameters.maximum_proof_deletions;
  request.maximum_journal_bytes = parameters.maximum_journal_bytes;
  request.consequence = static_cast<size_t>(parameters.consequence);
  request.action_id = action_id;
  request.semantic_id = semantic_id;
  request.records = kernel.structural_provenance;
  return request;
}

AffineBasisActionLimits basis_limits_from_kernel(const Kernel &kernel) {
  const auto &parameters = kernel.affine_consequence_parameters;
  AffineBasisActionLimits limits;
  limits.image.maximum_clauses = parameters.maximum_clauses;
  limits.image.maximum_groups = parameters.maximum_groups;
  limits.image.maximum_patterns = parameters.maximum_patterns;
  limits.image.maximum_elimination_work =
      parameters.maximum_elimination_work;
  limits.image.maximum_arity = parameters.maximum_arity;
  limits.image.maximum_intermediate_width =
      parameters.maximum_intermediate_width;
  limits.image.maximum_sources = parameters.maximum_sources;
  limits.image.maximum_output_width = parameters.maximum_output_width;
  limits.basis.maximum_work = parameters.basis_maximum_work;
  limits.basis.maximum_intermediate_width =
      parameters.basis_maximum_intermediate_width;
  limits.basis.maximum_sources = parameters.basis_maximum_sources;
  limits.basis.maximum_premises = parameters.basis_maximum_premises;
  limits.basis.maximum_nodes = parameters.basis_maximum_nodes;
  limits.basis.maximum_direct_width = parameters.basis_maximum_direct_width;
  limits.maximum_snapshot_literals = parameters.maximum_snapshot_literals;
  limits.maximum_snapshot_work = parameters.maximum_snapshot_work;
  limits.maximum_proof_additions = parameters.maximum_proof_additions;
  limits.maximum_proof_deletions = parameters.maximum_proof_deletions;
  limits.maximum_journal_bytes = parameters.maximum_journal_bytes;
  limits.maximum_external_extent = parameters.maximum_external_extent;
  limits.maximum_session_keys = parameters.maximum_session_keys;
  limits.maximum_session_work = parameters.maximum_session_work;
  return limits;
}

AffineConsequenceActionStatus status(AffineLiveStatus value) {
  switch (value) {
  case AffineLiveStatus::Invalid:
    return AffineConsequenceActionStatus::Invalid;
  case AffineLiveStatus::Inapplicable:
    return AffineConsequenceActionStatus::Inapplicable;
  case AffineLiveStatus::NoStructure:
    return AffineConsequenceActionStatus::NoStructure;
  case AffineLiveStatus::ResourceExhausted:
    return AffineConsequenceActionStatus::ResourceExhausted;
  case AffineLiveStatus::Interrupted:
    return AffineConsequenceActionStatus::Interrupted;
  case AffineLiveStatus::UnsupportedProof:
    return AffineConsequenceActionStatus::UnsupportedProof;
  case AffineLiveStatus::Stale:
    return AffineConsequenceActionStatus::Stale;
  case AffineLiveStatus::Applied:
    return AffineConsequenceActionStatus::Applied;
  }
  std::abort();
}

AffineConsequenceActionStatus status(AffineBasisActionStatus value) {
  switch (value) {
  case AffineBasisActionStatus::Invalid:
    return AffineConsequenceActionStatus::Invalid;
  case AffineBasisActionStatus::Inapplicable:
    return AffineConsequenceActionStatus::Inapplicable;
  case AffineBasisActionStatus::NoStructure:
    return AffineConsequenceActionStatus::NoStructure;
  case AffineBasisActionStatus::ResourceExhausted:
    return AffineConsequenceActionStatus::ResourceExhausted;
  case AffineBasisActionStatus::Interrupted:
    return AffineConsequenceActionStatus::Interrupted;
  case AffineBasisActionStatus::UnsupportedProof:
    return AffineConsequenceActionStatus::UnsupportedProof;
  case AffineBasisActionStatus::Stale:
    return AffineConsequenceActionStatus::Stale;
  case AffineBasisActionStatus::AlreadyPublished:
    return AffineConsequenceActionStatus::AlreadyPublished;
  case AffineBasisActionStatus::Applied:
    return AffineConsequenceActionStatus::Applied;
  }
  std::abort();
}

struct RecordAffineConsequence {
  Kernel &kernel;
  AffineConsequenceActionResult &result;
  ~RecordAffineConsequence() {
    auto &stats = kernel.stats;
    ++stats.affine_consequence_attempts;
    if (result.semantics ==
        action_parameters::AffineConsequenceSemantics::SelectedConsequenceV1) {
      const auto &live = result.live;
      stats.affine_consequence_snapshot_clauses += live.snapshot_clauses;
      stats.affine_consequence_snapshot_literals += live.snapshot_literals;
      stats.affine_consequence_snapshot_work += live.snapshot_work;
      stats.affine_consequence_equations += live.equations;
      stats.affine_consequence_consequences += live.consequences;
      stats.affine_consequence_recognition_work += live.recognition_work;
      stats.affine_consequence_selected_sources += live.selected_sources;
      stats.affine_consequence_selected_premises += live.selected_premises;
      stats.affine_consequence_proof_work += live.proof_addition_work;
      stats.affine_consequence_preflight_additions +=
          live.preflight_additions;
      stats.affine_consequence_preflight_deletions +=
          live.preflight_deletions;
      stats.affine_consequence_additions += live.additions;
      stats.affine_consequence_deletions += live.deletions;
      stats.affine_consequence_published += live.published;
      stats.affine_consequence_journal_work += live.journal_work;
      stats.affine_consequence_journal_bytes += live.journal_bytes;
      if (result.candidate_staged)
        stats.affine_consequence_candidate_variables +=
            live.semantic_variables.size();
    } else {
      const auto &basis = result.basis;
      const auto &live = basis.live;
      stats.affine_consequence_snapshot_clauses += live.snapshot_clauses;
      stats.affine_consequence_snapshot_literals += live.snapshot_literals;
      stats.affine_consequence_equations += live.equations;
      stats.affine_consequence_recognition_work += live.recognition_work;
      stats.affine_consequence_preflight_additions +=
          live.preflight_additions;
      stats.affine_consequence_preflight_deletions +=
          live.preflight_deletions;
      stats.affine_consequence_additions += live.additions;
      stats.affine_consequence_deletions += live.deletions;
      stats.affine_consequence_published += live.published_clauses;
      stats.affine_consequence_journal_bytes += live.journal_bytes;
      stats.affine_consequence_basis_work += live.basis_work;
      stats.affine_consequence_rows += live.rows;
      stats.affine_consequence_definitions += live.definitions;
      stats.affine_consequence_extent_growth += live.extent_growth;
      stats.affine_consequence_session_work += basis.session_work;
      stats.affine_consequence_excluded_rows += live.excluded_rows;
      stats.affine_consequence_transaction_keys += basis.transaction_keys;
      if (result.status == AffineConsequenceActionStatus::Applied)
        stats.affine_consequence_committed_keys += basis.transaction_keys;
      if (result.candidate_staged)
        stats.affine_consequence_candidate_variables +=
            live.existing_theory_variables.size() +
            live.introduced_theory_variables.size();
    }
    switch (result.status) {
    case AffineConsequenceActionStatus::Applied:
      ++stats.affine_consequence_applied;
      break;
    case AffineConsequenceActionStatus::NoStructure:
      ++stats.affine_consequence_no_structure;
      break;
    case AffineConsequenceActionStatus::AlreadyPublished:
      ++stats.affine_consequence_already_published;
      break;
    case AffineConsequenceActionStatus::Invalid:
      ++stats.affine_consequence_invalid;
      break;
    case AffineConsequenceActionStatus::Inapplicable:
      ++stats.affine_consequence_inapplicable;
      break;
    case AffineConsequenceActionStatus::ResourceExhausted:
      ++stats.affine_consequence_resource_exhausted;
      break;
    case AffineConsequenceActionStatus::Interrupted:
      ++stats.affine_consequence_interrupted;
      break;
    case AffineConsequenceActionStatus::UnsupportedProof:
      ++stats.affine_consequence_unsupported_proof;
      break;
    case AffineConsequenceActionStatus::Stale:
      ++stats.affine_consequence_stale;
      break;
    }
  }
};

void finalize_selected_consequence(AffineConsequenceActionResult &result) {
  result.semantics =
      action_parameters::AffineConsequenceSemantics::SelectedConsequenceV1;
  result.status = status(result.live.status);
  result.result = result.live.result;
}

AffineConsequenceActionResult apply_selected_consequence(Kernel &kernel) {
  AffineConsequenceActionResult output;
  if (!affine_consequence_action_guard(kernel)) {
    output.live.status = AffineLiveStatus::Inapplicable;
    return output;
  }

  const auto &parameters = kernel.affine_consequence_parameters;
  if (!parameters.valid() ||
      parameters.semantics !=
          action_parameters::AffineConsequenceSemantics::SelectedConsequenceV1 ||
      parameters.consequence > std::numeric_limits<size_t>::max() ||
      parameters.consequence == UINT64_MAX)
    return output;

  output.proposed_action_id = kernel.structural_action_sequence + 1;
  output.semantic_id = parameters.consequence + 1;
  output.live = derive_affine_live(
      kernel, request_from_kernel(kernel, output.proposed_action_id,
                                  output.semantic_id));
  if (output.live.status != AffineLiveStatus::Applied ||
      output.live.result == ApplyResult::NoOp)
    return output;

  if (output.live.result != ApplyResult::Ok &&
      output.live.result != ApplyResult::Unsat)
    std::abort();

  if (output.live.result == ApplyResult::Ok) {
    if (!output.live.published || output.live.semantic_variables.empty())
      std::abort();
    std::vector<strategy::SemanticVariable> variables;
    variables.reserve(output.live.semantic_variables.size());
    for (const int32_t external : output.live.semantic_variables) {
      if (external <= 0)
        std::abort();
      variables.push_back(
          {static_cast<uint32_t>(external),
           strategy::SemanticVariableRole::ExistingTheory, 1});
    }
    if (!strategy::stage_semantic_variable_candidate(
            kernel, strategy::SemanticVariableProducer::Affine,
            output.proposed_action_id, variables))
      std::abort();
    output.candidate_staged = true;
  }

  kernel.structural_action_sequence = output.proposed_action_id;
  output.committed_action_id = output.proposed_action_id;
  return output;
}

} // namespace

RuleEligibility affine_consequence_action_guard(const Kernel &kernel) {
  return RuleEligibility{}
      .fact("consistent", !kernel.inconsistent)
      .fact("root", !kernel.level)
      .fact("basis-assignment-free",
            kernel.affine_consequence_parameters.semantics !=
                    strategy::AffineConsequenceSemantics::CertifiedBasisV2 ||
                kernel.trail.empty())
      .fact("no-conflict",
            !kernel.conflict_valid && !kernel.pending.valid)
      .fact("propagated", kernel_propagation_fixpoint(kernel))
      .fact("sparse", !kernel.dense && kernel.large_watches_connected)
      .fact("no-probe-transaction", !kernel.probing.recording)
      .fact("basis-session-attached",
            kernel.affine_consequence_parameters.semantics !=
                    strategy::AffineConsequenceSemantics::CertifiedBasisV2 ||
                kernel.affine_basis_action_session != nullptr)
      .fact("provenance-bound", kernel.structural_provenance != nullptr)
      .fact("action-id-available",
            kernel.structural_action_sequence != UINT64_MAX)
      .fact("candidate-slot-empty",
            !kernel.semantic_variable_candidate_pending)
      .fact("candidate-id-available",
            kernel.semantic_variable_publication != (UINT64_MAX >> 8))
      .fact("clausal-certificate",
            !kernel.proof ||
                (!kernel.proof->lrat() && !kernel.proof->pb()));
}

AffineConsequenceActionResult affine_consequence_action_apply(Kernel &kernel) {
  if (kernel.affine_consequence_parameters.semantics ==
          strategy::AffineConsequenceSemantics::CertifiedBasisV2 &&
      kernel.affine_basis_action_session)
    return affine_consequence_action_apply(
        kernel, *kernel.affine_basis_action_session);
  AffineConsequenceActionResult output;
  {
    RecordAffineConsequence record{kernel, output};
    if (kernel.affine_consequence_parameters.semantics ==
        strategy::AffineConsequenceSemantics::SelectedConsequenceV1) {
      output = apply_selected_consequence(kernel);
      finalize_selected_consequence(output);
    } else {
      output.semantics = kernel.affine_consequence_parameters.semantics;
      if (kernel.affine_consequence_parameters.valid()) {
        output.status = AffineConsequenceActionStatus::Inapplicable;
        output.basis.status = AffineBasisActionStatus::Inapplicable;
      }
    }
  }
  return output;
}

AffineConsequenceActionResult affine_consequence_action_apply(
    Kernel &kernel, AffineBasisActionSession &session) {
  AffineConsequenceActionResult output;
  {
    RecordAffineConsequence record{kernel, output};
    if (kernel.affine_consequence_parameters.semantics ==
        strategy::AffineConsequenceSemantics::SelectedConsequenceV1) {
      output = apply_selected_consequence(kernel);
      finalize_selected_consequence(output);
    } else {
      output.semantics = kernel.affine_consequence_parameters.semantics;
      if (kernel.affine_consequence_parameters.valid()) {
        if (kernel.affine_basis_action_session != &session) {
          output.status = AffineConsequenceActionStatus::Inapplicable;
          output.basis.status = AffineBasisActionStatus::Inapplicable;
        } else {
          output.basis = affine_basis_action_apply(
              kernel, session, basis_limits_from_kernel(kernel));
          output.status = status(output.basis.status);
          output.result = output.basis.result;
          output.proposed_action_id = output.basis.proposed_action_id;
          output.committed_action_id = output.basis.committed_action_id;
          output.semantic_id = output.basis.semantic_id;
          output.candidate_staged = output.basis.candidate_staged;
        }
      }
    }
  }
  return output;
}

ApplyResult affine_consequence_rule_apply(Kernel &kernel) {
  return affine_consequence_action_apply(kernel).result;
}

TACTIX_REGISTER_RULE(AffineConsequence, RuleTier::Inprocess,
                     affine_consequence_action_guard,
                     affine_consequence_rule_apply);

} // namespace tactix
