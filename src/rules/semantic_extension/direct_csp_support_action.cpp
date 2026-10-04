#include "rules/semantic_extension/direct_csp_support_action.hpp"

#include "proof/proof.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <utility>

namespace tactix {

struct DirectCspSupportActionSessionAccess {
  static void commit_existing(DirectCspSupportActionSession &session,
                              std::vector<DirectCspSupportActionKey> keys) {
    session.keys_.swap(keys);
  }

  static void commit_new(
      DirectCspSupportActionSession &session, const Kernel &kernel,
      std::vector<DirectCspSupportOwnerSnapshot> snapshots,
      std::vector<DirectCspSupportActionKey> keys) {
    session.owner_ = &kernel;
    session.snapshots_.swap(snapshots);
    session.keys_.swap(keys);
  }
};

const DirectCspSupportOwnerSnapshot *DirectCspSupportActionSession::snapshot(
    const std::vector<int32_t> &left_values,
    const std::vector<int32_t> &right_values) const {
  for (const DirectCspSupportOwnerSnapshot &snapshot : snapshots_)
    if (snapshot.relation.left.values == left_values &&
        snapshot.relation.right.values == right_values)
      return &snapshot;
  return nullptr;
}

namespace {

bool add_work(uint64_t amount, uint64_t maximum, uint64_t &work) {
  if (work > maximum || amount > maximum - work) return false;
  work += amount;
  return true;
}

enum class KeyStatus : uint8_t { Novel, Present, Invalid, Exhausted };

uint32_t variable(int32_t literal) {
  return static_cast<uint32_t>(literal < 0 ? -int64_t(literal) : literal);
}

bool literal_less(int32_t first, int32_t second) {
  const uint32_t first_variable = variable(first);
  const uint32_t second_variable = variable(second);
  return first_variable < second_variable ||
         (first_variable == second_variable && first < second);
}

bool pair_identity_valid(const std::vector<int32_t> &left,
                         const std::vector<int32_t> &right,
                         const DirectCspSupportActionLimits &limits) {
  if (left.size() < 2 || right.size() < 2 ||
      left.size() > limits.maximum_domain_values ||
      right.size() > limits.maximum_domain_values)
    return false;
  std::vector<uint32_t> variables;
  variables.reserve(left.size() + right.size());
  const auto append = [&](const std::vector<int32_t> &domain) {
    for (size_t index = 0; index < domain.size(); ++index) {
      const int32_t literal = domain[index];
      if (!literal || literal == std::numeric_limits<int32_t>::min() ||
          (index && !literal_less(domain[index - 1], literal)))
        return false;
      variables.push_back(variable(literal));
    }
    return true;
  };
  if (!append(left) || !append(right)) return false;
  std::sort(variables.begin(), variables.end());
  return std::adjacent_find(variables.begin(), variables.end()) ==
         variables.end();
}

DirectCspSupportActionKey make_key(
    const DirectCspSupportActionSelection &selection) {
  DirectCspSupportActionKey key;
  key.left_values = selection.left_values;
  key.right_values = selection.right_values;
  key.selected_left_value = selection.selected_left_value;
  if (selection.selected_left_value < selection.left_values.size())
    key.selected_left_literal =
        selection.left_values[selection.selected_left_value];
  return key;
}

bool key_valid(const DirectCspSupportActionKey &key,
               const DirectCspSupportActionLimits &limits) {
  return pair_identity_valid(key.left_values, key.right_values, limits) &&
         key.selected_left_value < key.left_values.size() &&
         key.selected_left_literal ==
             key.left_values[key.selected_left_value];
}

bool owner_less(const DirectCspSupportOwnerSnapshot &first,
                const DirectCspSupportOwnerSnapshot &second) {
  if (first.relation.left.values != second.relation.left.values)
    return first.relation.left.values < second.relation.left.values;
  return first.relation.right.values < second.relation.right.values;
}

bool owner_matches_key(const DirectCspSupportOwnerSnapshot &owner,
                       const DirectCspSupportActionKey &key) {
  return owner.relation.left.values == key.left_values &&
         owner.relation.right.values == key.right_values;
}

bool occurrence_valid(uint64_t occurrence, size_t source_size) {
  return occurrence && occurrence <= source_size;
}

bool domain_payload_valid(const direct_csp_support::Domain &domain,
                          size_t source_size) {
  const size_t size = domain.values.size();
  if (!occurrence_valid(domain.at_least_one, source_size) ||
      !size || size > std::numeric_limits<size_t>::max() / size ||
      domain.at_most_one.size() != size * size)
    return false;
  for (size_t first = 0; first < size; ++first)
    for (size_t second = 0; second < size; ++second) {
      const uint64_t occurrence = domain.at_most_one[first * size + second];
      if (first == second) {
        if (occurrence) return false;
      } else if (!occurrence_valid(occurrence, source_size) ||
                 occurrence !=
                     domain.at_most_one[second * size + first]) {
        return false;
      }
    }
  return true;
}

bool owner_payload_valid(const DirectCspSupportOwnerSnapshot &owner,
                         const DirectCspSupportActionLimits &limits) {
  if (!owner.external_extent || owner.source.empty() ||
      owner.source.size() > limits.maximum_source_clauses ||
      owner.source_literals > limits.maximum_source_literals ||
      !pair_identity_valid(owner.relation.left.values,
                           owner.relation.right.values, limits) ||
      !domain_payload_valid(owner.relation.left, owner.source.size()) ||
      !domain_payload_valid(owner.relation.right, owner.source.size()) ||
      owner.relation.left.values.size() >
          std::numeric_limits<size_t>::max() /
              owner.relation.right.values.size() ||
      owner.relation.forbidden.size() !=
          owner.relation.left.values.size() *
              owner.relation.right.values.size())
    return false;
  uint64_t source_literals = 0;
  for (size_t index = 0; index < owner.source.size(); ++index) {
    const auto &premise = owner.source[index];
    if (premise.occurrence != index + 1 || premise.literals.empty() ||
        premise.literals.size() >
            limits.maximum_source_literals - source_literals)
      return false;
    source_literals += premise.literals.size();
  }
  if (source_literals != owner.source_literals) return false;
  for (uint64_t occurrence : owner.relation.forbidden)
    if (occurrence && !occurrence_valid(occurrence, owner.source.size()))
      return false;
  return true;
}

bool charge_owner(const DirectCspSupportOwnerSnapshot &owner,
                  uint64_t maximum, uint64_t &work) {
  return add_work(1, maximum, work) &&
         add_work(owner.relation.left.values.size(), maximum, work) &&
         add_work(owner.relation.right.values.size(), maximum, work) &&
         add_work(owner.source.size(), maximum, work) &&
         add_work(owner.source_literals, maximum, work) &&
         add_work(owner.relation.left.at_most_one.size(), maximum, work) &&
         add_work(owner.relation.right.at_most_one.size(), maximum, work) &&
         add_work(owner.relation.forbidden.size(), maximum, work);
}

enum class SessionStatus : uint8_t { Valid, Invalid, Exhausted };

SessionStatus validate_session(const DirectCspSupportActionSession &session,
                               const DirectCspSupportActionLimits &limits,
                               uint64_t &work) {
  if (session.retained() != (session.retained_owners() != 0) ||
      session.committed_keys() < session.retained_owners() ||
      session.committed_keys() > limits.maximum_session_keys)
    return SessionStatus::Invalid;
  const auto &owners = session.snapshots();
  for (size_t index = 0; index < owners.size(); ++index) {
    const auto &owner = owners[index];
    if (!charge_owner(owner, limits.maximum_session_work, work))
      return SessionStatus::Exhausted;
    if (!owner_payload_valid(owner, limits) ||
        (index && !owner_less(owners[index - 1], owner)))
      return SessionStatus::Invalid;
  }
  const auto &keys = session.keys();
  for (size_t index = 0; index < keys.size(); ++index) {
    const auto &key = keys[index];
    if (!add_work(uint64_t{1} + key.left_values.size() +
                      key.right_values.size(),
                  limits.maximum_session_work, work) ||
        !add_work(owners.size(), limits.maximum_session_work, work))
      return SessionStatus::Exhausted;
    if (!key_valid(key, limits) ||
        (index && !(keys[index - 1] < key)) ||
        std::none_of(owners.begin(), owners.end(),
                     [&](const DirectCspSupportOwnerSnapshot &owner) {
                       return owner_matches_key(owner, key);
                     }))
      return SessionStatus::Invalid;
  }
  for (const DirectCspSupportOwnerSnapshot &owner : owners) {
    if (!add_work(keys.size(), limits.maximum_session_work, work))
      return SessionStatus::Exhausted;
    if (std::none_of(keys.begin(), keys.end(),
                     [&](const DirectCspSupportActionKey &key) {
                       return owner_matches_key(owner, key);
                     }))
      return SessionStatus::Invalid;
  }
  return SessionStatus::Valid;
}

KeyStatus prepare_keys(const DirectCspSupportActionSession &session,
                       const DirectCspSupportActionKey &key,
                       const DirectCspSupportActionLimits &limits,
                       uint64_t &work,
                       std::vector<DirectCspSupportActionKey> &merged) {
  if (!add_work(uint64_t{1} + key.left_values.size() +
                    key.right_values.size(),
                limits.maximum_session_work, work))
    return KeyStatus::Exhausted;
  if (!key_valid(key, limits)) return KeyStatus::Invalid;
  const SessionStatus session_status =
      validate_session(session, limits, work);
  if (session_status == SessionStatus::Invalid) return KeyStatus::Invalid;
  if (session_status == SessionStatus::Exhausted)
    return KeyStatus::Exhausted;
  const auto &old = session.keys();
  const auto position = std::lower_bound(old.begin(), old.end(), key);
  if (position != old.end() && *position == key) return KeyStatus::Present;
  if (old.size() == limits.maximum_session_keys)
    return KeyStatus::Exhausted;

  // Allocate the complete post-commit state before the live publisher crosses
  // its irreversible mutation boundary.
  if (!add_work(uint64_t{1} + key.left_values.size() +
                    key.right_values.size(),
                limits.maximum_session_work, work))
    return KeyStatus::Exhausted;
  for (const DirectCspSupportActionKey &retained : old)
    if (!add_work(uint64_t{1} + retained.left_values.size() +
                      retained.right_values.size(),
                  limits.maximum_session_work, work))
      return KeyStatus::Exhausted;
  merged.reserve(old.size() + 1);
  merged.insert(merged.end(), old.begin(), position);
  merged.push_back(key);
  merged.insert(merged.end(), position, old.end());
  return KeyStatus::Novel;
}

enum class OwnerStatus : uint8_t { Complete, Invalid, Exhausted };

OwnerStatus prepare_new_owner(
    const DirectCspSupportActionSession &session,
    const DirectCspSupportOwnerSnapshot &incoming,
    const DirectCspSupportActionLimits &limits, uint64_t &work,
    std::vector<DirectCspSupportOwnerSnapshot> &merged) {
  const auto &old = session.snapshots();
  if (old.size() >= limits.maximum_session_keys)
    return OwnerStatus::Exhausted;
  if (!owner_payload_valid(incoming, limits))
    return OwnerStatus::Invalid;
  const auto position =
      std::lower_bound(old.begin(), old.end(), incoming, owner_less);
  if (position != old.end() && !owner_less(incoming, *position) &&
      !owner_less(*position, incoming))
    return OwnerStatus::Invalid;
  if (!charge_owner(incoming, limits.maximum_session_work, work))
    return OwnerStatus::Exhausted;
  for (const DirectCspSupportOwnerSnapshot &retained : old)
    if (!charge_owner(retained, limits.maximum_session_work, work))
      return OwnerStatus::Exhausted;

  // Allocate the complete post-commit owner set before live publication. The
  // final session update is an infallible vector swap.
  merged.reserve(old.size() + 1);
  merged.insert(merged.end(), old.begin(), position);
  merged.push_back(incoming);
  merged.insert(merged.end(), position, old.end());
  return OwnerStatus::Complete;
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
  output.committed_keys = session.committed_keys();
  output.retained_owners = session.retained_owners();
  output.key = make_key(selection);
  if (!direct_csp_support_action_guard(kernel)) {
    output.status = DirectCspSupportActionStatus::Inapplicable;
    output.live.status = DirectCspSupportLiveStatus::Inapplicable;
    return output;
  }
  if (!limits.valid()) return output;
  if (session.retained() && !session.belongs_to(kernel)) {
    output.status = DirectCspSupportActionStatus::Stale;
    output.live.status = DirectCspSupportLiveStatus::Stale;
    return output;
  }
  std::vector<DirectCspSupportActionKey> merged;
  const KeyStatus key_status = prepare_keys(
      session, output.key, limits, output.effort.session_work, merged);
  if (key_status == KeyStatus::Invalid) return output;
  if (key_status == KeyStatus::Exhausted) {
    output.status = DirectCspSupportActionStatus::ResourceExhausted;
    output.live.status = DirectCspSupportLiveStatus::ResourceExhausted;
    return output;
  }
  if (!add_work(session.retained_owners(), limits.maximum_session_work,
                output.effort.session_work)) {
    output.status = DirectCspSupportActionStatus::ResourceExhausted;
    output.live.status = DirectCspSupportLiveStatus::ResourceExhausted;
    return output;
  }
  const DirectCspSupportOwnerSnapshot *owner =
      session.snapshot(selection.left_values, selection.right_values);
  output.owner_retained = owner != nullptr;
  if (key_status == KeyStatus::Present) {
    if (!owner) return output;
    output.status = DirectCspSupportActionStatus::AlreadyPublished;
    output.live.status = DirectCspSupportLiveStatus::Duplicate;
    return output;
  }

  output.proposed_action_id = kernel.structural_action_sequence + 1;
  output.semantic_id = output.proposed_action_id;
  DirectCspSupportLiveRequest request =
      make_request(kernel, limits, selection, output.proposed_action_id);
  DirectCspSupportOwnerSnapshot captured;
  std::vector<DirectCspSupportOwnerSnapshot> merged_owners;
  if (!owner) {
    const DirectCspSupportOwnerCaptureResult capture =
        direct_csp_support_live_capture_owner(kernel, request, captured);
    copy_capture_effort(output, capture);
    if (capture.status != DirectCspSupportLiveStatus::Applied) {
      output.status = action_status(capture.status);
      output.live.status = capture.status;
      return output;
    }
    const OwnerStatus owner_status = prepare_new_owner(
        session, captured, limits, output.effort.session_work, merged_owners);
    if (owner_status == OwnerStatus::Invalid) return output;
    if (owner_status == OwnerStatus::Exhausted) {
      output.status = DirectCspSupportActionStatus::ResourceExhausted;
      output.live.status = DirectCspSupportLiveStatus::ResourceExhausted;
      return output;
    }
    request.retained_owner = &captured;
  } else {
    request.retained_owner = owner;
  }

  output.live = direct_csp_support_live_apply(kernel, request);
  output.status = action_status(output.live.status);
  output.result = output.live.result;
  copy_live_effort(output, output.live);
  if (output.live.status != DirectCspSupportLiveStatus::Applied ||
      output.live.result == ApplyResult::NoOp)
    return output;
  if (output.live.result != ApplyResult::Ok) std::abort();

  if (owner) {
    DirectCspSupportActionSessionAccess::commit_existing(session,
                                                         std::move(merged));
  } else {
    DirectCspSupportActionSessionAccess::commit_new(
        session, kernel, std::move(merged_owners), std::move(merged));
  }
  output.owner_retained = true;
  output.committed_keys = session.committed_keys();
  output.retained_owners = session.retained_owners();
  output.committed_action_id = output.proposed_action_id;
  materialize_semantics(output);
  kernel.structural_action_sequence = output.proposed_action_id;
  return output;
}

} // namespace tactix
