#include "rules/semantic_extension/direct_csp_support_action.hpp"

#include "proof/proof.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <utility>

namespace tactix {

struct DirectCspSupportActionSessionAccess {
  static void commit(DirectCspSupportActionSession &session,
                     const Kernel &kernel,
                     DirectCspSupportOwnerSnapshot snapshot,
                     std::vector<DirectCspSupportActionKey> keys) {
    session.owner_ = &kernel;
    if (session.snapshot_.source.empty())
      session.snapshot_ = std::move(snapshot);
    session.keys_.swap(keys);
  }
};

namespace {

bool add_work(uint64_t amount, uint64_t maximum, uint64_t &work) {
  if (work > maximum || amount > maximum - work) return false;
  work += amount;
  return true;
}

enum class KeyStatus : uint8_t { Novel, Present, Invalid, Exhausted };

KeyStatus prepare_keys(const DirectCspSupportActionSession &session,
                       const DirectCspSupportActionKey &key,
                       const DirectCspSupportActionLimits &limits,
                       uint64_t &work,
                       std::vector<DirectCspSupportActionKey> &merged) {
  const auto &old = session.keys();
  if (old.size() > limits.maximum_session_keys)
    return KeyStatus::Invalid;
  for (size_t index = 0; index < old.size(); ++index) {
    if (!add_work(1, limits.maximum_session_work, work))
      return KeyStatus::Exhausted;
    if (index && !(old[index - 1] < old[index]))
      return KeyStatus::Invalid;
  }
  const auto position = std::lower_bound(old.begin(), old.end(), key);
  if (position != old.end() && *position == key) return KeyStatus::Present;
  if (old.size() == limits.maximum_session_keys)
    return KeyStatus::Exhausted;

  // Allocate the complete post-commit state before the live publisher crosses
  // its irreversible mutation boundary.
  merged.reserve(old.size() + 1);
  merged.insert(merged.end(), old.begin(), position);
  merged.push_back(key);
  merged.insert(merged.end(), position, old.end());
  if (!add_work(uint64_t{1} + merged.size(), limits.maximum_session_work,
                work))
    return KeyStatus::Exhausted;
  return KeyStatus::Novel;
}

DirectCspSupportActionStatus action_status(
    DirectCspSupportLiveStatus status) {
  switch (status) {
  case DirectCspSupportLiveStatus::Applied:
    return DirectCspSupportActionStatus::Applied;
  case DirectCspSupportLiveStatus::Invalid:
    return DirectCspSupportActionStatus::Invalid;
  case DirectCspSupportLiveStatus::Inapplicable:
    return DirectCspSupportActionStatus::Inapplicable;
  case DirectCspSupportLiveStatus::NoStructure:
    return DirectCspSupportActionStatus::NoStructure;
  case DirectCspSupportLiveStatus::NoConsequence:
    return DirectCspSupportActionStatus::NoConsequence;
  case DirectCspSupportLiveStatus::Duplicate:
    return DirectCspSupportActionStatus::AlreadyPublished;
  case DirectCspSupportLiveStatus::ResourceExhausted:
    return DirectCspSupportActionStatus::ResourceExhausted;
  case DirectCspSupportLiveStatus::Interrupted:
    return DirectCspSupportActionStatus::Interrupted;
  case DirectCspSupportLiveStatus::UnsupportedProof:
    return DirectCspSupportActionStatus::UnsupportedProof;
  case DirectCspSupportLiveStatus::Stale:
    return DirectCspSupportActionStatus::Stale;
  }
  std::abort();
}

DirectCspSupportLiveRequest make_request(
    const Kernel &kernel, const DirectCspSupportActionLimits &limits,
    const DirectCspSupportActionSelection &selection, uint64_t action_id) {
  DirectCspSupportLiveRequest request;
  request.left_values = selection.left_values;
  request.right_values = selection.right_values;
  request.selected_left_value = selection.selected_left_value;
  request.maximum_source_clauses = limits.maximum_source_clauses;
  request.maximum_source_literals = limits.maximum_source_literals;
  request.maximum_recognition_work = limits.maximum_recognition_work;
  request.maximum_domain_values = limits.maximum_domain_values;
  request.maximum_direct_clause_literals =
      limits.maximum_direct_clause_literals;
  request.maximum_definition_clause_literals =
      limits.maximum_definition_clause_literals;
  request.maximum_proof_work = limits.maximum_proof_work;
  request.maximum_proof_additions = limits.maximum_proof_additions;
  request.maximum_proof_literal_occurrences =
      limits.maximum_proof_literal_occurrences;
  request.maximum_journal_bytes = limits.maximum_journal_bytes;
  request.maximum_extent_growth = limits.maximum_extent_growth;
  request.maximum_external_extent = limits.maximum_external_extent;
  request.allow_extension = limits.allow_extension;
  request.action_id = action_id;
  request.semantic_id = action_id;
  request.records = kernel.structural_provenance;
  return request;
}

void copy_capture_effort(DirectCspSupportActionResult &output,
                         const DirectCspSupportOwnerCaptureResult &capture) {
  output.effort.source_clauses = capture.source_clauses;
  output.effort.source_literals = capture.source_literals;
  output.effort.owner_capture_work = capture.recognition_work;
}

void copy_live_effort(DirectCspSupportActionResult &output,
                      const DirectCspSupportLiveResult &live) {
  output.effort.source_clauses = live.source_clauses;
  output.effort.source_literals = live.source_literals;
  output.effort.recognition_work = live.recognition_work;
  output.effort.source_scan_work = live.source_scan_work;
  output.effort.planning_work = live.planning_work;
  output.effort.revalidation_work = live.revalidation_work;
  output.effort.proof_work = live.proof_work;
  output.effort.planned_additions = live.planned_additions;
  output.effort.emitted_additions = live.emitted_additions;
  output.effort.proof_literal_occurrences =
      live.proof_literal_occurrences;
  output.effort.journal_work = live.journal_work;
  output.effort.journal_bytes = live.journal_bytes;
  output.effort.extent_growth = live.extent_growth;
}

uint32_t external_variable(int32_t literal) {
  if (!literal || literal == std::numeric_limits<int32_t>::min())
    std::abort();
  return static_cast<uint32_t>(literal < 0 ? -literal : literal);
}

void materialize_semantics(DirectCspSupportActionResult &output) {
  output.semantic_variables.reserve(output.live.semantic_variables.size());
  for (const DirectCspSemanticVariable &variable :
       output.live.semantic_variables) {
    if (variable.fresh_extension) {
      if (variable.role != DirectCspSemanticRole::SupportDisjunction ||
          variable.definition_inputs.empty() ||
          output.projection.extension_variable)
        std::abort();
      output.semantic_variables.push_back(
          {external_variable(variable.literal),
           strategy::SemanticVariableRole::IntroducedTheory, 1});
      output.projection.extension_variable =
          external_variable(variable.literal);
      output.projection.definition_inputs = variable.definition_inputs;
      output.projection.exact_or_definition = true;
      output.projection.project_from_external_model = true;
      output.projection.reconstruction_witness_required = false;
    } else {
      if (variable.role == DirectCspSemanticRole::SupportDisjunction ||
          !variable.definition_inputs.empty())
        std::abort();
      output.semantic_variables.push_back(
          {external_variable(variable.literal),
           strategy::SemanticVariableRole::ExistingTheory, 1});
    }
  }
  output.semantic_candidate_ready = !output.semantic_variables.empty();
}

bool selection_matches_owner(const DirectCspSupportActionSelection &selection,
                             const DirectCspSupportOwnerSnapshot &owner) {
  return selection.left_values == owner.relation.left.values &&
         selection.right_values == owner.relation.right.values;
}

} // namespace

bool DirectCspSupportActionLimits::valid() const {
  return maximum_source_clauses && maximum_source_literals &&
         maximum_recognition_work && maximum_domain_values >= 2 &&
         maximum_direct_clause_literals &&
         maximum_definition_clause_literals && maximum_proof_work &&
         maximum_proof_additions && maximum_proof_literal_occurrences &&
         maximum_journal_bytes && maximum_external_extent &&
         maximum_session_keys && maximum_session_work &&
         (!allow_extension || maximum_extent_growth);
}

RuleEligibility direct_csp_support_action_guard(const Kernel &kernel) {
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

DirectCspSupportActionResult direct_csp_support_action_apply(
    Kernel &kernel, DirectCspSupportActionSession &session,
    const DirectCspSupportActionLimits &limits,
    const DirectCspSupportActionSelection &selection) {
  DirectCspSupportActionResult output;
  output.owner_retained = session.retained();
  output.committed_keys = session.committed_keys();
  output.key = {selection.selected_left_value};
  if (!direct_csp_support_action_guard(kernel)) {
    output.status = DirectCspSupportActionStatus::Inapplicable;
    output.live.status = DirectCspSupportLiveStatus::Inapplicable;
    return output;
  }
  if (!limits.valid() || selection.left_values.empty() ||
      selection.right_values.empty() ||
      selection.selected_left_value >= selection.left_values.size())
    return output;
  if (session.retained() && !session.belongs_to(kernel)) {
    output.status = DirectCspSupportActionStatus::Stale;
    output.live.status = DirectCspSupportLiveStatus::Stale;
    return output;
  }
  if (session.retained() &&
      !selection_matches_owner(selection, *session.snapshot()))
    return output;

  std::vector<DirectCspSupportActionKey> merged;
  const KeyStatus key_status = prepare_keys(
      session, output.key, limits, output.effort.session_work, merged);
  if (key_status == KeyStatus::Invalid) return output;
  if (key_status == KeyStatus::Exhausted) {
    output.status = DirectCspSupportActionStatus::ResourceExhausted;
    output.live.status = DirectCspSupportLiveStatus::ResourceExhausted;
    return output;
  }
  if (key_status == KeyStatus::Present) {
    output.status = DirectCspSupportActionStatus::AlreadyPublished;
    output.live.status = DirectCspSupportLiveStatus::Duplicate;
    return output;
  }

  output.proposed_action_id = kernel.structural_action_sequence + 1;
  output.semantic_id = output.proposed_action_id;
  DirectCspSupportLiveRequest request =
      make_request(kernel, limits, selection, output.proposed_action_id);
  DirectCspSupportOwnerSnapshot captured;
  const bool first = !session.retained();
  if (first) {
    const DirectCspSupportOwnerCaptureResult capture =
        direct_csp_support_live_capture_owner(kernel, request, captured);
    copy_capture_effort(output, capture);
    if (capture.status != DirectCspSupportLiveStatus::Applied) {
      output.status = action_status(capture.status);
      output.live.status = capture.status;
      return output;
    }
    request.retained_owner = &captured;
  } else {
    request.retained_owner = session.snapshot();
  }

  output.live = direct_csp_support_live_apply(kernel, request);
  output.status = action_status(output.live.status);
  output.result = output.live.result;
  copy_live_effort(output, output.live);
  if (output.live.status != DirectCspSupportLiveStatus::Applied ||
      output.live.result == ApplyResult::NoOp)
    return output;
  if (output.live.result != ApplyResult::Ok) std::abort();

  DirectCspSupportActionSessionAccess::commit(
      session, kernel, std::move(captured), std::move(merged));
  output.owner_retained = true;
  output.committed_keys = session.committed_keys();
  output.committed_action_id = output.proposed_action_id;
  materialize_semantics(output);
  kernel.structural_action_sequence = output.proposed_action_id;
  return output;
}

} // namespace tactix
