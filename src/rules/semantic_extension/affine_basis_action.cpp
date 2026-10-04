#include "rules/semantic_extension/affine_basis_action.hpp"

#include "proof/proof.hpp"
#include "strategy/semantic_variable_advice.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <utility>

namespace tactix {

struct AffineBasisActionSessionAccess {
  static void commit(AffineBasisActionSession &session, const Kernel &kernel,
                     std::vector<AffineBasisConsumedKey> keys) {
    session.owner_ = &kernel;
    session.keys_.swap(keys);
  }
};

namespace {

bool add_work(uint64_t amount, uint64_t maximum, uint64_t &work) {
  if (amount > maximum - work) return false;
  work += amount;
  return true;
}

enum class MeasureStatus : uint8_t { Complete, Invalid, Exhausted };

MeasureStatus measure_keys(const std::vector<AffineBasisConsumedKey> &keys,
                           uint64_t maximum, uint64_t &work) {
  for (size_t key_index = 0; key_index < keys.size(); ++key_index) {
    if (key_index) {
      if (!add_work(1, maximum, work)) return MeasureStatus::Exhausted;
      if (!(keys[key_index - 1] < keys[key_index]))
        return MeasureStatus::Invalid;
    }
    const auto &key = keys[key_index];
    if (!add_work(1, maximum, work) ||
        !add_work(key.variables.size(), maximum, work))
      return MeasureStatus::Exhausted;
    for (const auto &equation : key.source_equations) {
      if (!add_work(1, maximum, work) ||
          !add_work(equation.variables.size(), maximum, work))
        return MeasureStatus::Exhausted;
      for (const auto &premise : equation.premises)
        if (!add_work(1, maximum, work) ||
            !add_work(premise.clause.size(), maximum, work))
          return MeasureStatus::Exhausted;
    }
  }
  return MeasureStatus::Complete;
}

enum class MergeStatus : uint8_t { Complete, Invalid, Exhausted, Overlap };

MergeStatus merge_new_keys(const AffineBasisActionSession &session,
                           const std::vector<AffineBasisConsumedKey> &incoming,
                           const AffineBasisActionLimits &limits,
                           uint64_t &work,
                           std::vector<AffineBasisConsumedKey> &merged) {
  const auto &old = session.keys();
  if (incoming.empty()) return MergeStatus::Invalid;
  const MeasureStatus old_status =
      measure_keys(old, limits.maximum_session_work, work);
  if (old_status != MeasureStatus::Complete)
    return old_status == MeasureStatus::Invalid ? MergeStatus::Invalid
                                                : MergeStatus::Exhausted;
  const MeasureStatus incoming_status =
      measure_keys(incoming, limits.maximum_session_work, work);
  if (incoming_status != MeasureStatus::Complete)
    return incoming_status == MeasureStatus::Invalid ? MergeStatus::Invalid
                                                     : MergeStatus::Exhausted;

  size_t i = 0, j = 0;
  while (i < old.size() && j < incoming.size()) {
    if (!add_work(1, limits.maximum_session_work, work))
      return MergeStatus::Exhausted;
    if (old[i] < incoming[j]) {
      ++i;
    } else if (incoming[j] < old[i]) {
      ++j;
    } else {
      return MergeStatus::Overlap;
    }
  }

  if (old.size() > limits.maximum_session_keys ||
      incoming.size() > limits.maximum_session_keys - old.size() ||
      incoming.size() > std::numeric_limits<size_t>::max() - old.size())
    return MergeStatus::Exhausted;

  // Allocate and build the complete post-commit session before the live
  // transaction mutates the kernel.  The final session commit is a swap.
  merged.reserve(old.size() + incoming.size());
  i = j = 0;
  while (i < old.size() || j < incoming.size()) {
    if (!add_work(1, limits.maximum_session_work, work))
      return MergeStatus::Exhausted;
    if (j == incoming.size() ||
        (i < old.size() && old[i] < incoming[j]))
      merged.push_back(old[i++]);
    else
      merged.push_back(incoming[j++]);
  }
  return MergeStatus::Complete;
}

AffineBasisActionStatus action_status(AffineBasisLiveStatus status) {
  switch (status) {
  case AffineBasisLiveStatus::Invalid:
    return AffineBasisActionStatus::Invalid;
  case AffineBasisLiveStatus::Inapplicable:
    return AffineBasisActionStatus::Inapplicable;
  case AffineBasisLiveStatus::NoStructure:
    return AffineBasisActionStatus::NoStructure;
  case AffineBasisLiveStatus::ResourceExhausted:
    return AffineBasisActionStatus::ResourceExhausted;
  case AffineBasisLiveStatus::Interrupted:
    return AffineBasisActionStatus::Interrupted;
  case AffineBasisLiveStatus::UnsupportedProof:
    return AffineBasisActionStatus::UnsupportedProof;
  case AffineBasisLiveStatus::Stale:
    return AffineBasisActionStatus::Stale;
  case AffineBasisLiveStatus::Prepared:
    return AffineBasisActionStatus::Invalid;
  case AffineBasisLiveStatus::Applied:
    return AffineBasisActionStatus::Applied;
  }
  std::abort();
}

AffineBasisLiveRequest make_request(const Kernel &kernel,
                                    const AffineBasisActionLimits &limits,
                                    uint64_t action_id) {
  AffineBasisLiveRequest request;
  request.image = limits.image;
  request.basis = limits.basis;
  request.maximum_snapshot_literals = limits.maximum_snapshot_literals;
  request.maximum_snapshot_work = limits.maximum_snapshot_work;
  request.maximum_proof_additions = limits.maximum_proof_additions;
  request.maximum_proof_deletions = limits.maximum_proof_deletions;
  request.maximum_journal_bytes = limits.maximum_journal_bytes;
  request.maximum_external_extent = limits.maximum_external_extent;
  request.action_id = action_id;
  request.semantic_id = action_id;
  request.records = kernel.structural_provenance;
  return request;
}

} // namespace

bool AffineBasisActionLimits::valid() const {
  return image.maximum_clauses && image.maximum_groups &&
         image.maximum_patterns && image.maximum_elimination_work &&
         image.maximum_arity && image.maximum_arity <= 8 &&
         image.maximum_intermediate_width && image.maximum_sources &&
         image.maximum_output_width <= 4 &&
         basis.maximum_work && basis.maximum_intermediate_width &&
         basis.maximum_sources && basis.maximum_premises &&
         basis.maximum_nodes && basis.maximum_direct_width >= 2 &&
         basis.maximum_direct_width <= 8 && maximum_snapshot_literals &&
         maximum_snapshot_work && maximum_proof_additions &&
         maximum_journal_bytes && maximum_external_extent &&
         maximum_session_keys && maximum_session_work;
}

RuleEligibility affine_basis_action_guard(const Kernel &kernel) {
  return RuleEligibility{}
      .fact("consistent", !kernel.inconsistent)
      .fact("root", !kernel.level)
      .fact("assignment-free", kernel.trail.empty())
      .fact("no-conflict", !kernel.conflict_valid && !kernel.pending.valid)
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
            !kernel.proof || (!kernel.proof->lrat() && !kernel.proof->pb()));
}

AffineBasisActionResult affine_basis_action_apply(
    Kernel &kernel, AffineBasisActionSession &session,
    const AffineBasisActionLimits &limits) {
  AffineBasisActionResult output;
  output.committed_keys = session.committed_keys();
  if (!affine_basis_action_guard(kernel)) {
    output.status = AffineBasisActionStatus::Inapplicable;
    output.live.status = AffineBasisLiveStatus::Inapplicable;
    return output;
  }
  if (!limits.valid()) return output;
  if (session.bound() && !session.belongs_to(kernel)) {
    output.status = AffineBasisActionStatus::Stale;
    output.live.status = AffineBasisLiveStatus::Stale;
    return output;
  }

  output.proposed_action_id = kernel.structural_action_sequence + 1;
  output.semantic_id = output.proposed_action_id;
  const AffineBasisLiveRequest request =
      make_request(kernel, limits, output.proposed_action_id);
  AffineBasisLiveTransaction transaction;
  output.live = prepare_affine_basis_live(kernel, request, transaction);
  output.status = action_status(output.live.status);
  if (output.live.status != AffineBasisLiveStatus::Prepared) return output;

  output.transaction_keys = transaction.consumed_keys().size();
  std::vector<AffineBasisConsumedKey> merged;
  const MergeStatus merge =
      merge_new_keys(session, transaction.consumed_keys(), limits,
                     output.session_work, merged);
  if (merge == MergeStatus::Invalid) {
    output.status = AffineBasisActionStatus::Invalid;
    output.live.status = AffineBasisLiveStatus::Invalid;
    return output;
  }
  if (merge == MergeStatus::Exhausted) {
    output.status = AffineBasisActionStatus::ResourceExhausted;
    output.live.status = AffineBasisLiveStatus::ResourceExhausted;
    return output;
  }
  if (merge == MergeStatus::Overlap) {
    output.status = AffineBasisActionStatus::AlreadyPublished;
    return output;
  }

  output.live = publish_affine_basis_live(kernel, request, transaction);
  output.status = action_status(output.live.status);
  output.result = output.live.result;
  if (output.live.status != AffineBasisLiveStatus::Applied ||
      output.live.result == ApplyResult::NoOp)
    return output;
  if (output.live.result != ApplyResult::Ok &&
      output.live.result != ApplyResult::Unsat)
    std::abort();

  AffineBasisActionSessionAccess::commit(session, kernel, std::move(merged));
  output.committed_keys = session.committed_keys();
  if (output.live.result == ApplyResult::Ok) {
    std::vector<strategy::SemanticVariable> variables;
    variables.reserve(output.live.existing_theory_variables.size() +
                      output.live.introduced_theory_variables.size());
    for (int32_t external : output.live.existing_theory_variables) {
      if (external <= 0) std::abort();
      variables.push_back({static_cast<uint32_t>(external),
                           strategy::SemanticVariableRole::ExistingTheory, 1});
    }
    for (int32_t external : output.live.introduced_theory_variables) {
      if (external <= 0) std::abort();
      variables.push_back(
          {static_cast<uint32_t>(external),
           strategy::SemanticVariableRole::IntroducedTheory, 1});
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
