#include "rules/semantic_extension/nary_csp_prefix_action.hpp"

#include "proof/proof.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>

namespace tactix {

struct NaryCspPrefixActionSessionAccess {
  static void commit_existing(NaryCspPrefixActionSession &session,
                              std::vector<NaryCspPrefixActionKey> keys) {
    session.keys_.swap(keys);
  }
  static void commit_new(NaryCspPrefixActionSession &session,
                         const Kernel &kernel,
                         std::vector<NaryCspPrefixOwnerSnapshot> snapshots,
                         std::vector<NaryCspPrefixActionKey> keys) {
    session.owner_ = &kernel;
    session.snapshots_.swap(snapshots);
    session.keys_.swap(keys);
  }
};

namespace {

bool add_work(uint64_t amount, uint64_t maximum, uint64_t &work) {
  if (work > maximum || amount > maximum - work) return false;
  work += amount;
  return true;
}

uint32_t variable(int32_t literal) {
  return static_cast<uint32_t>(literal < 0 ? -int64_t(literal) : literal);
}

bool literal_less(int32_t first, int32_t second) {
  return std::pair(variable(first), first) <
         std::pair(variable(second), second);
}

bool identity_valid(const NaryCspPrefixActionKey &key) {
  if (key.domains.size() < 3 ||
      key.prefix.residual_domain >= key.domains.size() ||
      key.prefix.prefix.size() + 1 != key.domains.size())
    return false;
  std::vector<bool> selected(key.domains.size(), false);
  std::vector<uint32_t> variables;
  for (size_t domain = 0; domain < key.domains.size(); ++domain) {
    const auto &values = key.domains[domain];
    if (values.size() < 2) return false;
    for (size_t value = 0; value < values.size(); ++value) {
      const int32_t literal = values[value];
      if (!literal || literal == std::numeric_limits<int32_t>::min() ||
          (value && !literal_less(values[value - 1], literal)))
        return false;
      variables.push_back(variable(literal));
    }
  }
  std::sort(variables.begin(), variables.end());
  if (std::adjacent_find(variables.begin(), variables.end()) != variables.end())
    return false;
  uint32_t previous = 0;
  bool first = true;
  for (const auto choice : key.prefix.prefix) {
    if (choice.domain >= key.domains.size() ||
        choice.domain == key.prefix.residual_domain ||
        choice.value >= key.domains[choice.domain].size() ||
        (!first && choice.domain <= previous) || selected[choice.domain])
      return false;
    first = false;
    previous = choice.domain;
    selected[choice.domain] = true;
  }
  for (size_t domain = 0; domain < key.domains.size(); ++domain)
    if ((domain != key.prefix.residual_domain) != selected[domain])
      return false;
  return true;
}

bool key_within_limits(const NaryCspPrefixActionKey &key,
                       const NaryCspPrefixActionLimits &limits) {
  if (key.domains.size() > limits.maximum_domains ||
      key.prefix.prefix.size() > limits.maximum_prefix_arity)
    return false;
  for (const auto &domain : key.domains)
    if (domain.size() > limits.maximum_domain_values) return false;
  return true;
}

NaryCspPrefixActionKey make_key(
    const NaryCspPrefixActionSelection &selection) {
  return {selection.domains, selection.prefix};
}

std::vector<std::vector<int32_t>> owner_domains(
    const NaryCspPrefixOwnerSnapshot &owner) {
  std::vector<std::vector<int32_t>> result;
  result.reserve(owner.relation.domains.size());
  for (const auto &domain : owner.relation.domains)
    result.push_back(domain.values);
  return result;
}

bool owner_less(const NaryCspPrefixOwnerSnapshot &first,
                const NaryCspPrefixOwnerSnapshot &second) {
  const size_t common =
      std::min(first.relation.domains.size(), second.relation.domains.size());
  for (size_t domain = 0; domain < common; ++domain)
    if (first.relation.domains[domain].values !=
        second.relation.domains[domain].values)
      return first.relation.domains[domain].values <
             second.relation.domains[domain].values;
  return first.relation.domains.size() < second.relation.domains.size();
}

bool owner_matches_domains(
    const NaryCspPrefixOwnerSnapshot &owner,
    const std::vector<std::vector<int32_t>> &domains) {
  if (owner.relation.domains.size() != domains.size()) return false;
  for (size_t domain = 0; domain < domains.size(); ++domain)
    if (owner.relation.domains[domain].values != domains[domain]) return false;
  return true;
}

bool occurrence_valid(uint64_t occurrence, size_t source_size) {
  return occurrence && occurrence <= source_size;
}

bool owner_payload_valid(const NaryCspPrefixOwnerSnapshot &owner) {
  if (!owner.external_extent || owner.source.empty() ||
      owner.relation.domains.size() < 3) return false;
  uint64_t source_literals = 0;
  for (size_t index = 0; index < owner.source.size(); ++index) {
    const auto &premise = owner.source[index];
    if (premise.occurrence != index + 1 || premise.literals.empty() ||
        premise.literals.size() > UINT64_MAX - source_literals)
      return false;
    source_literals += premise.literals.size();
  }
  if (source_literals != owner.source_literals) return false;
  std::vector<uint32_t> previous;
  bool first_tuple = true;
  for (const auto &domain : owner.relation.domains) {
    const size_t size = domain.values.size();
    if (!occurrence_valid(domain.at_least_one, owner.source.size()) ||
        size < 2 || size > std::numeric_limits<size_t>::max() / size ||
        domain.at_most_one.size() != size * size)
      return false;
    for (size_t i = 0; i < size; ++i)
      for (size_t j = 0; j < size; ++j) {
        const uint64_t occurrence = domain.at_most_one[i * size + j];
        if (i == j) {
          if (occurrence) return false;
        } else if (!occurrence_valid(occurrence, owner.source.size()) ||
                   occurrence != domain.at_most_one[j * size + i]) {
          return false;
        }
      }
  }
  for (const auto &tuple : owner.relation.forbidden) {
    if (tuple.value_indices.size() != owner.relation.domains.size() ||
        !occurrence_valid(tuple.occurrence, owner.source.size()) ||
        (!first_tuple && !(previous < tuple.value_indices)))
      return false;
    first_tuple = false;
    previous = tuple.value_indices;
    for (size_t domain = 0; domain < tuple.value_indices.size(); ++domain)
      if (tuple.value_indices[domain] >=
          owner.relation.domains[domain].values.size())
        return false;
  }
  NaryCspPrefixActionKey shape{owner_domains(owner), {}};
  shape.prefix.residual_domain = 0;
  for (size_t domain = 1; domain < shape.domains.size(); ++domain)
    shape.prefix.prefix.push_back(
        {static_cast<uint32_t>(domain), uint32_t{0}});
  return identity_valid(shape);
}

bool owner_within_limits(const NaryCspPrefixOwnerSnapshot &owner,
                         const NaryCspPrefixActionLimits &limits) {
  if (owner.source.size() > limits.maximum_source_clauses ||
      owner.source_literals > limits.maximum_source_literals ||
      owner.relation.domains.size() > limits.maximum_domains ||
      owner.relation.forbidden.size() > limits.maximum_forbidden_tuples)
    return false;
  for (const auto &domain : owner.relation.domains)
    if (domain.values.size() > limits.maximum_domain_values) return false;
  return true;
}

bool charge_owner(const NaryCspPrefixOwnerSnapshot &owner,
                  const NaryCspPrefixActionLimits &limits, uint64_t &work) {
  if (!add_work(1, limits.maximum_session_work, work) ||
      !add_work(owner.source.size(), limits.maximum_session_work, work) ||
      !add_work(owner.source_literals, limits.maximum_session_work, work) ||
      !add_work(owner.relation.domains.size(), limits.maximum_session_work,
                work) ||
      !add_work(owner.relation.forbidden.size(), limits.maximum_session_work,
                work))
    return false;
  for (const auto &domain : owner.relation.domains)
    if (!add_work(1, limits.maximum_session_work, work) ||
        !add_work(domain.values.size(), limits.maximum_session_work, work) ||
        !add_work(domain.at_most_one.size(), limits.maximum_session_work,
                  work))
      return false;
  for (const auto &tuple : owner.relation.forbidden)
    if (!add_work(tuple.value_indices.size(), limits.maximum_session_work,
                  work))
      return false;
  return true;
}

bool charge_key(const NaryCspPrefixActionKey &key,
                const NaryCspPrefixActionLimits &limits, uint64_t &work) {
  if (!add_work(1, limits.maximum_session_work, work) ||
      !add_work(key.domains.size(), limits.maximum_session_work, work) ||
      !add_work(key.prefix.prefix.size(), limits.maximum_session_work, work))
    return false;
  for (const auto &domain : key.domains)
    if (!add_work(domain.size(), limits.maximum_session_work, work))
      return false;
  return true;
}

enum class SessionStatus : uint8_t { Valid, Invalid, Exhausted };

SessionStatus validate_session(const NaryCspPrefixActionSession &session,
                               const NaryCspPrefixActionLimits &limits,
                               uint64_t &work) {
  if (session.retained() != (session.retained_owners() != 0) ||
      session.committed_keys() < session.retained_owners())
    return SessionStatus::Invalid;
  for (size_t index = 0; index < session.snapshots().size(); ++index) {
    const auto &owner = session.snapshots()[index];
    if (!charge_owner(owner, limits, work)) return SessionStatus::Exhausted;
    if (!owner_payload_valid(owner) ||
        (index && !owner_less(session.snapshots()[index - 1], owner)))
      return SessionStatus::Invalid;
    if (!owner_within_limits(owner, limits)) return SessionStatus::Exhausted;
  }
  for (size_t index = 0; index < session.keys().size(); ++index) {
    const auto &key = session.keys()[index];
    if (!charge_key(key, limits, work)) return SessionStatus::Exhausted;
    if (!identity_valid(key) || !key_within_limits(key, limits) ||
        (index && !(session.keys()[index - 1] < key)) ||
        std::none_of(session.snapshots().begin(), session.snapshots().end(),
                     [&](const auto &owner) {
                       return owner_matches_domains(owner, key.domains);
                     }))
      return SessionStatus::Invalid;
  }
  for (const auto &owner : session.snapshots())
    if (std::none_of(session.keys().begin(), session.keys().end(),
                     [&](const auto &key) {
                       return owner_matches_domains(owner, key.domains);
                     }))
      return SessionStatus::Invalid;
  return session.committed_keys() > limits.maximum_session_keys
             ? SessionStatus::Exhausted
             : SessionStatus::Valid;
}

enum class KeyStatus : uint8_t { Novel, Present, Invalid, Exhausted };

KeyStatus prepare_keys(const NaryCspPrefixActionSession &session,
                       const NaryCspPrefixActionKey &key,
                       const NaryCspPrefixActionLimits &limits,
                       uint64_t &work,
                       std::vector<NaryCspPrefixActionKey> &merged) {
  if (!charge_key(key, limits, work)) return KeyStatus::Exhausted;
  if (!identity_valid(key)) return KeyStatus::Invalid;
  const SessionStatus valid = validate_session(session, limits, work);
  if (valid == SessionStatus::Invalid) return KeyStatus::Invalid;
  if (valid == SessionStatus::Exhausted || !key_within_limits(key, limits))
    return KeyStatus::Exhausted;
  const auto position =
      std::lower_bound(session.keys().begin(), session.keys().end(), key);
  if (position != session.keys().end() && *position == key)
    return KeyStatus::Present;
  if (session.keys().size() >= limits.maximum_session_keys)
    return KeyStatus::Exhausted;
  if (!charge_key(key, limits, work)) return KeyStatus::Exhausted;
  for (const auto &retained : session.keys())
    if (!charge_key(retained, limits, work)) return KeyStatus::Exhausted;
  merged.reserve(session.keys().size() + 1);
  merged.insert(merged.end(), session.keys().begin(), position);
  merged.push_back(key);
  merged.insert(merged.end(), position, session.keys().end());
  return KeyStatus::Novel;
}

enum class OwnerStatus : uint8_t { Complete, Invalid, Exhausted };

OwnerStatus prepare_new_owner(
    const NaryCspPrefixActionSession &session,
    const NaryCspPrefixOwnerSnapshot &incoming,
    const NaryCspPrefixActionLimits &limits, uint64_t &work,
    std::vector<NaryCspPrefixOwnerSnapshot> &merged) {
  if (!charge_owner(incoming, limits, work)) return OwnerStatus::Exhausted;
  if (!owner_payload_valid(incoming)) return OwnerStatus::Invalid;
  const auto position = std::lower_bound(session.snapshots().begin(),
                                         session.snapshots().end(), incoming,
                                         owner_less);
  if (position != session.snapshots().end() &&
      !owner_less(incoming, *position) && !owner_less(*position, incoming))
    return OwnerStatus::Invalid;
  if (session.snapshots().size() >= limits.maximum_session_keys ||
      !owner_within_limits(incoming, limits))
    return OwnerStatus::Exhausted;
  if (!charge_owner(incoming, limits, work)) return OwnerStatus::Exhausted;
  for (const auto &owner : session.snapshots())
    if (!charge_owner(owner, limits, work)) return OwnerStatus::Exhausted;
  merged.reserve(session.snapshots().size() + 1);
  merged.insert(merged.end(), session.snapshots().begin(), position);
  merged.push_back(incoming);
  merged.insert(merged.end(), position, session.snapshots().end());
  return OwnerStatus::Complete;
}

NaryCspPrefixActionStatus action_status(NaryCspPrefixLiveStatus status) {
  switch (status) {
  case NaryCspPrefixLiveStatus::Applied:
    return NaryCspPrefixActionStatus::Applied;
  case NaryCspPrefixLiveStatus::Invalid:
    return NaryCspPrefixActionStatus::Invalid;
  case NaryCspPrefixLiveStatus::Inapplicable:
    return NaryCspPrefixActionStatus::Inapplicable;
  case NaryCspPrefixLiveStatus::NoStructure:
    return NaryCspPrefixActionStatus::NoStructure;
  case NaryCspPrefixLiveStatus::NoConsequence:
    return NaryCspPrefixActionStatus::NoConsequence;
  case NaryCspPrefixLiveStatus::Duplicate:
    return NaryCspPrefixActionStatus::AlreadyPublished;
  case NaryCspPrefixLiveStatus::ResourceExhausted:
    return NaryCspPrefixActionStatus::ResourceExhausted;
  case NaryCspPrefixLiveStatus::Interrupted:
    return NaryCspPrefixActionStatus::Interrupted;
  case NaryCspPrefixLiveStatus::UnsupportedProof:
    return NaryCspPrefixActionStatus::UnsupportedProof;
  case NaryCspPrefixLiveStatus::Stale:
    return NaryCspPrefixActionStatus::Stale;
  }
  std::abort();
}

NaryCspPrefixLiveRequest make_request(
    const Kernel &kernel, const NaryCspPrefixActionLimits &limits,
    const NaryCspPrefixActionSelection &selection, uint64_t action_id) {
  NaryCspPrefixLiveRequest request;
  request.domains = selection.domains;
  request.selection = selection.prefix;
  request.maximum_source_clauses = limits.maximum_source_clauses;
  request.maximum_source_literals = limits.maximum_source_literals;
  request.maximum_recognition_work = limits.maximum_recognition_work;
  request.maximum_domains = limits.maximum_domains;
  request.maximum_domain_values = limits.maximum_domain_values;
  request.maximum_forbidden_tuples = limits.maximum_forbidden_tuples;
  request.maximum_prefix_arity = limits.maximum_prefix_arity;
  request.maximum_definition_clause_literals =
      limits.maximum_definition_clause_literals;
  request.maximum_proof_work = limits.maximum_proof_work;
  request.maximum_proof_additions = limits.maximum_proof_additions;
  request.maximum_proof_literal_occurrences =
      limits.maximum_proof_literal_occurrences;
  request.maximum_journal_bytes = limits.maximum_journal_bytes;
  request.maximum_extent_growth = limits.maximum_extent_growth;
  request.maximum_external_extent = limits.maximum_external_extent;
  request.action_id = action_id;
  request.semantic_id = action_id;
  request.records = kernel.structural_provenance;
  return request;
}

void copy_live_effort(NaryCspPrefixActionResult &output,
                      const NaryCspPrefixLiveResult &live) {
  output.effort.source_clauses = live.source_clauses;
  output.effort.source_literals = live.source_literals;
  output.effort.recognition_work = live.recognition_work;
  output.effort.source_scan_work = live.source_scan_work;
  output.effort.planning_work = live.planning_work;
  output.effort.revalidation_work = live.revalidation_work;
  output.effort.proof_work = live.proof_work;
  output.effort.planned_additions = live.planned_additions;
  output.effort.emitted_additions = live.emitted_additions;
  output.effort.proof_literal_occurrences = live.proof_literal_occurrences;
  output.effort.journal_work = live.journal_work;
  output.effort.journal_bytes = live.journal_bytes;
  output.effort.extent_growth = live.extent_growth;
}

uint32_t external_variable(int32_t literal) {
  if (!literal || literal == std::numeric_limits<int32_t>::min()) std::abort();
  return variable(literal);
}

void publish_semantics(NaryCspPrefixActionResult &output) {
  for (const auto &semantic : output.live.semantic_variables) {
    if (semantic.fresh_extension) {
      if (semantic.role != NaryCspSemanticRole::PrefixConjunction ||
          semantic.definition_inputs.empty() ||
          output.projection.extension_variable)
        std::abort();
      output.semantic_variables.push_back(
          {external_variable(semantic.literal),
           strategy::SemanticVariableRole::IntroducedTheory, 1});
      output.projection.extension_variable =
          external_variable(semantic.literal);
      output.projection.definition_inputs = semantic.definition_inputs;
      output.projection.exact_and_definition = true;
      output.projection.project_from_external_model = true;
    } else {
      if (semantic.role == NaryCspSemanticRole::PrefixConjunction ||
          !semantic.definition_inputs.empty())
        std::abort();
      output.semantic_variables.push_back(
          {external_variable(semantic.literal),
           strategy::SemanticVariableRole::ExistingTheory, 1});
    }
  }
  output.semantic_candidate_ready = !output.semantic_variables.empty();
}

} // namespace

const NaryCspPrefixOwnerSnapshot *NaryCspPrefixActionSession::snapshot(
    const std::vector<std::vector<int32_t>> &domains) const {
  for (const auto &snapshot : snapshots_)
    if (owner_matches_domains(snapshot, domains)) return &snapshot;
  return nullptr;
}

bool NaryCspPrefixActionLimits::valid() const {
  return maximum_source_clauses && maximum_source_literals &&
         maximum_recognition_work && maximum_domains >= 3 &&
         maximum_domain_values >= 2 && maximum_forbidden_tuples &&
         maximum_prefix_arity >= 2 &&
         maximum_definition_clause_literals && maximum_proof_work &&
         maximum_proof_additions && maximum_proof_literal_occurrences &&
         maximum_journal_bytes && maximum_extent_growth &&
         maximum_external_extent && maximum_session_keys &&
         maximum_session_work;
}

RuleEligibility nary_csp_prefix_action_guard(const Kernel &kernel) {
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

NaryCspPrefixActionResult nary_csp_prefix_action_apply(
    Kernel &kernel, NaryCspPrefixActionSession &session,
    const NaryCspPrefixActionLimits &limits,
    const NaryCspPrefixActionSelection &selection) {
  NaryCspPrefixActionResult output;
  output.committed_keys = session.committed_keys();
  output.retained_owners = session.retained_owners();
  output.key = make_key(selection);
  if (!nary_csp_prefix_action_guard(kernel)) {
    output.status = NaryCspPrefixActionStatus::Inapplicable;
    output.live.status = NaryCspPrefixLiveStatus::Inapplicable;
    return output;
  }
  if (!limits.valid()) return output;
  if (session.retained() && !session.belongs_to(kernel)) {
    output.status = NaryCspPrefixActionStatus::Stale;
    output.live.status = NaryCspPrefixLiveStatus::Stale;
    return output;
  }
  std::vector<NaryCspPrefixActionKey> merged_keys;
  const KeyStatus key_status =
      prepare_keys(session, output.key, limits, output.effort.session_work,
                   merged_keys);
  if (key_status == KeyStatus::Invalid) return output;
  if (key_status == KeyStatus::Exhausted) {
    output.status = NaryCspPrefixActionStatus::ResourceExhausted;
    output.live.status = NaryCspPrefixLiveStatus::ResourceExhausted;
    return output;
  }
  const auto *owner = session.snapshot(selection.domains);
  output.owner_retained = owner != nullptr;
  if (key_status == KeyStatus::Present) {
    if (!owner) return output;
    output.status = NaryCspPrefixActionStatus::AlreadyPublished;
    output.live.status = NaryCspPrefixLiveStatus::Duplicate;
    return output;
  }

  output.proposed_action_id = kernel.structural_action_sequence + 1;
  output.semantic_id = output.proposed_action_id;
  NaryCspPrefixLiveRequest request =
      make_request(kernel, limits, selection, output.proposed_action_id);
  NaryCspPrefixOwnerSnapshot captured;
  std::vector<NaryCspPrefixOwnerSnapshot> merged_owners;
  if (!owner) {
    const auto capture =
        nary_csp_prefix_live_capture_owner(kernel, request, captured);
    output.effort.source_clauses = capture.source_clauses;
    output.effort.source_literals = capture.source_literals;
    output.effort.owner_capture_work = capture.recognition_work;
    if (capture.status != NaryCspPrefixLiveStatus::Applied) {
      output.status = action_status(capture.status);
      output.live.status = capture.status;
      return output;
    }
    const OwnerStatus prepared = prepare_new_owner(
        session, captured, limits, output.effort.session_work, merged_owners);
    if (prepared == OwnerStatus::Invalid) return output;
    if (prepared == OwnerStatus::Exhausted) {
      output.status = NaryCspPrefixActionStatus::ResourceExhausted;
      output.live.status = NaryCspPrefixLiveStatus::ResourceExhausted;
      return output;
    }
    request.retained_owner = &captured;
  } else {
    request.retained_owner = owner;
  }

  output.live = nary_csp_prefix_live_apply(kernel, request);
  output.status = action_status(output.live.status);
  output.result = output.live.result;
  copy_live_effort(output, output.live);
  if (output.live.status != NaryCspPrefixLiveStatus::Applied ||
      output.live.result == ApplyResult::NoOp)
    return output;
  if (output.live.result != ApplyResult::Ok) std::abort();

  if (owner) {
    NaryCspPrefixActionSessionAccess::commit_existing(session,
                                                       std::move(merged_keys));
  } else {
    NaryCspPrefixActionSessionAccess::commit_new(
        session, kernel, std::move(merged_owners), std::move(merged_keys));
  }
  output.owner_retained = true;
  output.committed_keys = session.committed_keys();
  output.retained_owners = session.retained_owners();
  output.committed_action_id = output.proposed_action_id;
  publish_semantics(output);
  kernel.structural_action_sequence = output.proposed_action_id;
  return output;
}

} // namespace tactix
