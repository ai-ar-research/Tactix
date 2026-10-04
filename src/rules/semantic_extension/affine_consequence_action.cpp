#include "rules/semantic_extension/affine_consequence_action.hpp"

#include "proof/proof.hpp"
#include "strategy/semantic_variable_advice.hpp"

#include <cstdlib>
#include <limits>
#include <vector>

namespace tactix {
namespace {

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

} // namespace

RuleEligibility affine_consequence_action_guard(const Kernel &kernel) {
  return RuleEligibility{}
      .fact("consistent", !kernel.inconsistent)
      .fact("root", !kernel.level)
      .fact("no-conflict",
            !kernel.conflict_valid && !kernel.pending.valid)
      .fact("propagated", kernel_propagation_fixpoint(kernel))
      .fact("sparse", !kernel.dense && kernel.large_watches_connected)
      .fact("no-probe-transaction", !kernel.probing.recording)
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
  AffineConsequenceActionResult output;
  if (!affine_consequence_action_guard(kernel)) {
    output.live.status = AffineLiveStatus::Inapplicable;
    return output;
  }

  const auto &parameters = kernel.affine_consequence_parameters;
  if (!parameters.valid() ||
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

} // namespace tactix
