#include "rules/semantic_extension/nary_csp_prefix_action.hpp"

#include "proof/proof.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <map>
#include <set>

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
    if (values.size() < 2 ||
        (domain && !(key.domains[domain - 1] < values)))
      return false;
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

struct OfferMeter {
  Kernel &kernel;
  uint64_t maximum = 0;
  uint64_t used = 0;
  bool interrupted = false;
  bool exhausted = false;

  bool charge(uint64_t amount) {
    if (kernel_terminating(kernel)) {
      interrupted = true;
      return false;
    }
    if (used > maximum || amount > maximum - used) {
      exhausted = true;
      return false;
    }
    used += amount;
    return true;
  }
  bool absorb(uint64_t amount) {
    if (used > maximum || amount > maximum - used) {
      exhausted = true;
      return false;
    }
    used += amount;
    return true;
  }
  uint64_t remaining() const { return maximum - used; }
};

uint64_t saturated_product(uint64_t first, uint64_t second) {
  return second && first > UINT64_MAX / second ? UINT64_MAX : first * second;
}

uint64_t comparison_levels(uint64_t entries) {
  uint64_t levels = 1;
  while (entries > 1) {
    entries = (entries + 1) / 2;
    ++levels;
  }
  return levels;
}

bool canonical_domain_candidate(const Kernel &kernel, const Lit *literals,
                                uint32_t size, uint32_t maximum_size,
                                std::vector<int32_t> &output) {
  output.clear();
  if (size < 2 || size > maximum_size) return false;
  output.reserve(size);
  for (uint32_t index = 0; index < size; ++index) {
    const Var var = var_of(literals[index]);
    if (kernel_var_retired(kernel, var) || kernel.value(literals[index]))
      return false;
    output.push_back(kernel_externalize(kernel, literals[index]));
  }
  std::sort(output.begin(), output.end(), literal_less);
  for (size_t index = 1; index < output.size(); ++index)
    if (variable(output[index - 1]) == variable(output[index])) return false;
  return true;
}

enum class BinaryStatus : uint8_t { Absent, Present, Stopped };

BinaryStatus binary_present(Kernel &kernel, Lit first, Lit second,
                            OfferMeter &meter) {
  if (!meter.charge(1)) return BinaryStatus::Stopped;
  if (kernel.watches.binary_size(second) < kernel.watches.binary_size(first))
    std::swap(first, second);
  const bool stop_at_large = kernel.watches.heads[first].binaries_first();
  for (const WatchEntry watch : kernel.watches.entries(first)) {
    if (!meter.charge(1)) return BinaryStatus::Stopped;
    if (!watch.binary()) {
      if (stop_at_large) break;
      continue;
    }
    if (watch.other() == second) return BinaryStatus::Present;
  }
  return BinaryStatus::Absent;
}

BinaryStatus complete_amo(Kernel &kernel,
                          const std::vector<int32_t> &domain,
                          OfferMeter &meter) {
  for (size_t first = 0; first < domain.size(); ++first)
    for (size_t second = first + 1; second < domain.size(); ++second) {
      const Lit left = kernel_internalize(kernel, -domain[first]);
      const Lit right = kernel_internalize(kernel, -domain[second]);
      if (left == INVALID_LIT || right == INVALID_LIT)
        return BinaryStatus::Absent;
      const BinaryStatus status = binary_present(kernel, left, right, meter);
      if (status != BinaryStatus::Present) return status;
    }
  return BinaryStatus::Present;
}

bool disjoint_domains(const std::vector<std::vector<int32_t>> &domains) {
  std::set<uint32_t> seen;
  for (const auto &domain : domains)
    for (int32_t literal : domain)
      if (!seen.insert(variable(literal)).second) return false;
  return true;
}

NaryCspPrefixActionSelection selection_from_key(
    const NaryCspPrefixActionKey &key) {
  return {key.domains, key.prefix};
}

} // namespace

bool nary_csp_prefix_action_selection_valid(
    const NaryCspPrefixActionSelection &selection) {
  return identity_valid(make_key(selection));
}

bool nary_csp_prefix_action_selection_within_limits(
    const NaryCspPrefixActionSelection &selection,
    const NaryCspPrefixActionLimits &limits) {
  const NaryCspPrefixActionKey key = make_key(selection);
  return identity_valid(key) && key_within_limits(key, limits);
}

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

bool NaryCspPrefixActionOfferWindow::valid() const {
  return relation_arity >= 3 && maximum_domain_candidates &&
         maximum_relation_candidates && maximum_prefix_candidates &&
         maximum_offers && maximum_enumeration_work &&
         (!have_after || (identity_valid(after) &&
                          after.domains.size() == relation_arity));
}

RuleEligibility nary_csp_prefix_action_guard(const Kernel &kernel) {
  return RuleEligibility{}
      .fact("consistent", !kernel.inconsistent)
      .fact("root", !kernel.level)
      .fact("root-assignments-only",
            std::all_of(kernel.trail.begin(), kernel.trail.end(),
                        [&](Lit literal) {
                          return kernel.assigned[var_of(literal)].level == 0;
                        }))
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

NaryCspPrefixActionOfferResult nary_csp_prefix_action_offer(
    Kernel &kernel, const NaryCspPrefixActionSession &session,
    const NaryCspPrefixActionLimits &limits,
    const NaryCspPrefixActionOfferWindow &window) {
  NaryCspPrefixActionOfferResult output;
  if (!nary_csp_prefix_action_guard(kernel)) {
    output.status = NaryCspPrefixActionOfferStatus::Inapplicable;
    return output;
  }
  if (!limits.valid() || !window.valid() ||
      window.relation_arity > limits.maximum_domains)
    return output;
  if (session.retained() && !session.belongs_to(kernel)) {
    output.status = NaryCspPrefixActionOfferStatus::Stale;
    return output;
  }
  const SessionStatus session_status =
      validate_session(session, limits, output.effort.session_work);
  if (session_status == SessionStatus::Invalid) return output;
  if (session_status == SessionStatus::Exhausted ||
      session.committed_keys() >= limits.maximum_session_keys) {
    output.status = NaryCspPrefixActionOfferStatus::ResourceExhausted;
    return output;
  }

  OfferMeter meter{kernel, window.maximum_enumeration_work};
  auto finish = [&](NaryCspPrefixActionOfferStatus status, bool complete) {
    output.status = status;
    output.complete = complete;
    output.effort.enumeration_work = meter.used;
    return output;
  };
  auto stopped = [&]() {
    output.offers.clear();
    output.have_resume_after = false;
    return finish(meter.interrupted
                      ? NaryCspPrefixActionOfferStatus::Interrupted
                      : NaryCspPrefixActionOfferStatus::ResourceExhausted,
                  false);
  };

  // A retained source remains authoritative only while an immediate exact
  // recapture has the same clauses, native identities and relation tables.
  // Do this independently of fresh discovery so deleting an ALO/AMO premise
  // cannot turn a stale owner into a misleading NoStructure result.
  for (const auto &retained : session.snapshots()) {
    if (retained.relation.domains.size() != window.relation_arity) continue;
    NaryCspPrefixActionSelection placeholder;
    placeholder.domains = owner_domains(retained);
    placeholder.prefix.residual_domain = window.relation_arity - 1;
    for (uint32_t domain = 0; domain + 1 < window.relation_arity; ++domain)
      placeholder.prefix.prefix.push_back({domain, 0});
    NaryCspPrefixLiveRequest request =
        make_request(kernel, limits, placeholder,
                     kernel.structural_action_sequence + 1);
    request.maximum_recognition_work =
        std::min(request.maximum_recognition_work, meter.remaining());
    if (!request.maximum_recognition_work) return stopped();
    NaryCspPrefixOwnerSnapshot current;
    const auto capture =
        nary_csp_prefix_live_capture_owner(kernel, request, current);
    if (!meter.absorb(capture.recognition_work)) return stopped();
    output.effort.owner_capture_work += capture.recognition_work;
    if (capture.status == NaryCspPrefixLiveStatus::Interrupted) {
      meter.interrupted = true;
      return stopped();
    }
    if (capture.status == NaryCspPrefixLiveStatus::ResourceExhausted) {
      if (request.maximum_recognition_work < limits.maximum_recognition_work)
        meter.exhausted = true;
      return stopped();
    }
    if (capture.status != NaryCspPrefixLiveStatus::Applied)
      return finish(NaryCspPrefixActionOfferStatus::Stale, false);
    current.external_extent = retained.external_extent;
    current.capture_work = retained.capture_work;
    if (current != retained)
      return finish(NaryCspPrefixActionOfferStatus::Stale, false);
    ++output.effort.retained_owners_revalidated;
  }

  const uint64_t candidate_scan_begin = meter.used;
  std::set<std::vector<int32_t>> candidates;
  auto observe_domain = [&](const Lit *literals, uint32_t size) {
    if (!meter.charge(uint64_t{1} + size)) return false;
    std::vector<int32_t> domain;
    if (!canonical_domain_candidate(kernel, literals, size,
                                    limits.maximum_domain_values, domain))
      return true;
    const BinaryStatus exact = complete_amo(kernel, domain, meter);
    if (exact == BinaryStatus::Stopped) return false;
    if (exact == BinaryStatus::Absent) return true;
    const uint64_t set_work = saturated_product(
        uint64_t{1} + domain.size(),
        comparison_levels(uint64_t{1} + candidates.size()));
    if (!meter.charge(set_work)) return false;
    if (candidates.contains(domain)) return true;
    if (candidates.size() == window.maximum_domain_candidates) {
      meter.exhausted = true;
      return false;
    }
    const auto [where, inserted] = candidates.insert(std::move(domain));
    (void)where;
    if (!inserted) std::abort();
    return true;
  };
  for (Ref reference = 0; reference < kernel.arena.size();) {
    const Clause *clause = clause_at(kernel.arena, reference);
    reference += clause_actual_words(clause);
    if (!meter.charge(1)) return stopped();
    if (clause->garbage()) continue;
    ++output.effort.formula_clauses;
    output.effort.formula_literals += clause->size;
    if (!observe_domain(clause->lits, clause->size)) return stopped();
  }
  for (Lit first = 0; first < 2 * kernel.nvars; ++first)
    for (const WatchEntry watch : kernel.watches.entries(first)) {
      if (!meter.charge(1)) return stopped();
      if (!watch.binary() || first >= watch.other()) continue;
      ++output.effort.formula_clauses;
      output.effort.formula_literals += 2;
      const Lit pair[] = {first, watch.other()};
      if (!observe_domain(pair, 2)) return stopped();
    }
  output.effort.candidate_scan_work = meter.used - candidate_scan_begin;
  output.effort.domain_candidates = candidates.size();
  output.effort.exact_domains = candidates.size();
  std::vector<std::vector<int32_t>> domains(candidates.begin(),
                                             candidates.end());
  if (domains.size() < window.relation_arity)
    return finish(NaryCspPrefixActionOfferStatus::NoStructure, true);

  // Index only clause-backed relations.  The former producer enumerated every
  // C(|domains|, arity) combination and then rescanned the whole formula to
  // discover that almost all combinations had no tuple clause.  Here each
  // eligible tuple clause names its domains directly through a deterministic
  // literal -> (domain,value) map.  The map is limited to unambiguous domain
  // variables; overlapping exact-one candidates remain legal explicit X17
  // inputs but are deliberately not guessed by native discovery.
  std::vector<uint32_t> domain_variable_occurrences(kernel.nvars, 0);
  for (const auto &domain : domains)
    for (int32_t external : domain) {
      const Lit literal = kernel_internalize(kernel, external);
      if (literal == INVALID_LIT) std::abort();
      ++domain_variable_occurrences[var_of(literal)];
    }
  std::map<int32_t, nary_csp_prefix::DomainValue> tuple_literals;
  for (uint32_t domain = 0; domain < domains.size(); ++domain)
    for (uint32_t value = 0; value < domains[domain].size(); ++value) {
      const int32_t external = domains[domain][value];
      const Lit literal = kernel_internalize(kernel, external);
      if (domain_variable_occurrences[var_of(literal)] == 1)
        tuple_literals.emplace(
            -external, nary_csp_prefix::DomainValue{domain, value});
    }

  using RelationIndexKey = std::vector<uint32_t>;
  using TupleCoordinates = std::vector<uint32_t>;
  using IndexedTuples = std::map<TupleCoordinates, uint64_t>;
  std::map<RelationIndexKey, IndexedTuples> relations;
  uint64_t relation_capacity = window.maximum_relation_candidates;
  const uint64_t index_slack = 1 + uint64_t{window.have_after};
  if (relation_capacity <= UINT64_MAX - index_slack)
    relation_capacity += index_slack;
  else
    relation_capacity = UINT64_MAX;
  bool more_relations = false;
  bool duplicate_tuple = false;
  const auto domains_for = [&](const RelationIndexKey &indices) {
    std::vector<std::vector<int32_t>> result;
    result.reserve(indices.size());
    for (uint32_t index : indices) result.push_back(domains[index]);
    return result;
  };
  const auto maximum_key_for = [&](const RelationIndexKey &indices,
                                   const IndexedTuples &tuples) {
    NaryCspPrefixActionKey maximum;
    maximum.domains = domains_for(indices);
    bool have_maximum = false;
    for (const auto &[tuple, native_id] : tuples) {
      (void)native_id;
      for (uint32_t residual = 0; residual < window.relation_arity;
           ++residual) {
        NaryCspPrefixActionKey key;
        key.domains = maximum.domains;
        key.prefix.residual_domain = residual;
        for (uint32_t domain = 0; domain < window.relation_arity; ++domain)
          if (domain != residual)
            key.prefix.prefix.push_back({domain, tuple[domain]});
        if (!have_maximum || maximum < key) {
          maximum = std::move(key);
          have_maximum = true;
        }
      }
    }
    if (!have_maximum) std::abort();
    return maximum;
  };
  const auto observe_tuple = [&](const Clause &clause) {
    if (clause.size != window.relation_arity) return true;
    if (!meter.charge(uint64_t{1} + clause.size)) return false;
    std::vector<std::pair<uint32_t, uint32_t>> mapped;
    mapped.reserve(clause.size);
    for (Lit literal : clause) {
      const int32_t external = kernel_externalize(kernel, literal);
      const auto found = tuple_literals.find(external);
      if (found == tuple_literals.end()) return true;
      mapped.push_back({found->second.domain, found->second.value});
    }
    std::sort(mapped.begin(), mapped.end());
    for (size_t index = 1; index < mapped.size(); ++index)
      if (mapped[index - 1].first == mapped[index].first) return true;
    RelationIndexKey relation;
    TupleCoordinates coordinates;
    relation.reserve(mapped.size());
    coordinates.reserve(mapped.size());
    for (const auto &[domain, value] : mapped) {
      relation.push_back(domain);
      coordinates.push_back(value);
    }
    const auto relation_domains = domains_for(relation);
    if (window.have_after && relation_domains < window.after.domains)
      return true;
    auto found = relations.find(relation);
    if (found == relations.end()) {
      if (relations.size() == relation_capacity) {
        auto largest = std::prev(relations.end());
        if (!(relation < largest->first)) {
          more_relations = true;
          return true;
        }
        relations.erase(largest);
        more_relations = true;
      }
      found = relations.emplace(relation, IndexedTuples{}).first;
    }
    if (!found->second.emplace(std::move(coordinates), clause.id()).second)
      duplicate_tuple = true;
    return true;
  };
  for (Ref reference = 0; reference < kernel.arena.size();) {
    const Clause *clause = clause_at(kernel.arena, reference);
    reference += clause_actual_words(clause);
    if (!meter.charge(1)) return stopped();
    if (!clause->garbage() && !observe_tuple(*clause)) return stopped();
  }
  if (duplicate_tuple)
    return finish(NaryCspPrefixActionOfferStatus::Invalid, false);
  if (window.have_after)
    for (auto relation = relations.begin(); relation != relations.end();) {
      const auto maximum = maximum_key_for(relation->first, relation->second);
      if (!(window.after < maximum))
        relation = relations.erase(relation);
      else
        ++relation;
    }
  if (relations.size() > window.maximum_relation_candidates)
    more_relations = true;
  output.effort.indexed_relation_keys =
      std::min<uint64_t>(relations.size(),
                         window.maximum_relation_candidates);
  output.effort.indexed_relation_lookahead_keys =
      relations.size() - output.effort.indexed_relation_keys;
  for (const auto &[relation, tuples] : relations) {
    (void)relation;
    output.effort.indexed_tuple_occurrences += tuples.size();
    output.effort.indexed_prefix_candidates +=
        tuples.size() * window.relation_arity;
  }
  if (relations.empty())
    return finish(NaryCspPrefixActionOfferStatus::NoStructure,
                  !more_relations);

  std::set<NaryCspPrefixActionKey> keys;
  std::vector<NaryCspPrefixOwnerSnapshot> owners;
  bool fatal = false;
  bool invalid = false;
  bool have_last_examined = false;
  NaryCspPrefixActionKey last_examined;
  uint64_t processed_relations = 0;
  for (const auto &[relation, indexed_tuples] : relations) {
    if (processed_relations == window.maximum_relation_candidates) break;
    if (fatal || invalid) break;
    ++processed_relations;
    const auto relation_domains = domains_for(relation);
    last_examined = maximum_key_for(relation, indexed_tuples);
    have_last_examined = true;
    if (!disjoint_domains(relation_domains)) continue;
    if (!meter.charge(1)) {
      fatal = true;
      break;
    }
    ++output.effort.relation_candidates;
    NaryCspPrefixOwnerSnapshot captured;
    const auto *retained = session.snapshot(relation_domains);
    if (retained) {
      captured = *retained;
    } else {
      NaryCspPrefixActionSelection placeholder;
      placeholder.domains = relation_domains;
      placeholder.prefix.residual_domain = window.relation_arity - 1;
      for (uint32_t domain = 0; domain + 1 < window.relation_arity;
           ++domain)
        placeholder.prefix.prefix.push_back({domain, 0});
      NaryCspPrefixLiveRequest request =
          make_request(kernel, limits, placeholder,
                       kernel.structural_action_sequence + 1);
      request.maximum_recognition_work =
          std::min(request.maximum_recognition_work, meter.remaining());
      if (!request.maximum_recognition_work) {
        meter.exhausted = true;
        fatal = true;
        break;
      }
      const auto capture =
          nary_csp_prefix_live_capture_owner(kernel, request, captured);
      if (!meter.absorb(capture.recognition_work)) {
        fatal = true;
        break;
      }
      output.effort.owner_capture_work += capture.recognition_work;
      if (capture.status == NaryCspPrefixLiveStatus::Interrupted) {
        meter.interrupted = true;
        fatal = true;
        break;
      }
      if (capture.status != NaryCspPrefixLiveStatus::Applied) {
        if (capture.status == NaryCspPrefixLiveStatus::ResourceExhausted) {
          if (request.maximum_recognition_work <
              limits.maximum_recognition_work) {
            meter.exhausted = true;
            fatal = true;
          } else {
            ++output.effort.resource_rejected_relations;
          }
        } else if (capture.status == NaryCspPrefixLiveStatus::Invalid ||
                   capture.status == NaryCspPrefixLiveStatus::Inapplicable) {
          invalid = true;
        }
        continue;
      }
    }
    // The indexed coordinates are the cheap discovery certificate.  Exact
    // native clause IDs and the complete ALO/AMO source slice come from the
    // existing capture path, and preflight below immediately recaptures them
    // before any offer can be published.
    IndexedTuples captured_tuples;
    for (const auto &tuple : captured.relation.forbidden) {
      if (!tuple.occurrence || tuple.occurrence > captured.source.size()) {
        invalid = true;
        break;
      }
      captured_tuples.emplace(
          tuple.value_indices,
          captured.source[tuple.occurrence - 1].native_proof_id);
    }
    if (captured_tuples != indexed_tuples) {
      invalid = true;
      break;
    }
    output.effort.forbidden_tuples += captured.relation.forbidden.size();
    for (const auto &tuple : captured.relation.forbidden)
      for (uint32_t residual = 0; residual < window.relation_arity;
           ++residual) {
        if (output.effort.prefix_candidates >=
                window.maximum_prefix_candidates ||
            !meter.charge(uint64_t{1} + window.relation_arity)) {
          meter.exhausted = true;
          fatal = true;
          break;
        }
        ++output.effort.prefix_candidates;
        NaryCspPrefixActionKey key;
        key.domains = relation_domains;
        key.prefix.residual_domain = residual;
        for (uint32_t domain = 0; domain < window.relation_arity; ++domain)
          if (domain != residual)
            key.prefix.prefix.push_back(
                {domain, tuple.value_indices[domain]});
        keys.insert(std::move(key));
      }
    if (fatal) break;
    owners.push_back(std::move(captured));
  }
  if (invalid)
    return finish(NaryCspPrefixActionOfferStatus::Invalid, false);
  if (fatal) return stopped();
  if (keys.empty() && more_relations && have_last_examined) {
    output.have_resume_after = true;
    output.resume_after = last_examined;
    return finish(NaryCspPrefixActionOfferStatus::Ready, false);
  }
  if (keys.empty() && output.effort.resource_rejected_relations)
    return finish(NaryCspPrefixActionOfferStatus::ResourceExhausted, false);
  if (keys.empty())
    return finish(NaryCspPrefixActionOfferStatus::NoStructure, true);

  for (const auto &key : keys) {
    if (window.have_after && !(window.after < key)) continue;
    if (std::binary_search(session.keys().begin(), session.keys().end(), key)) {
      ++output.effort.committed_prefixes_skipped;
      continue;
    }
    const auto owner = std::find_if(
        owners.begin(), owners.end(), [&](const auto &candidate) {
          return owner_matches_domains(candidate, key.domains);
        });
    if (owner == owners.end()) {
      output.status = NaryCspPrefixActionOfferStatus::Invalid;
      output.complete = false;
      output.offers.clear();
      output.effort.enumeration_work = meter.used;
      return output;
    }

    uint64_t candidate_session_work = 0;
    std::vector<NaryCspPrefixActionKey> merged_keys;
    const KeyStatus key_status = prepare_keys(
        session, key, limits, candidate_session_work, merged_keys);
    if (key_status == KeyStatus::Invalid) {
      output.offers.clear();
      output.have_resume_after = false;
      return finish(NaryCspPrefixActionOfferStatus::Invalid, false);
    }
    if (key_status == KeyStatus::Exhausted) {
      if (!meter.absorb(candidate_session_work)) return stopped();
      output.effort.session_preflight_work += candidate_session_work;
      ++output.effort.resource_rejected_prefixes;
      continue;
    }
    if (key_status == KeyStatus::Present) {
      if (!meter.absorb(candidate_session_work)) return stopped();
      output.effort.session_preflight_work += candidate_session_work;
      ++output.effort.committed_prefixes_skipped;
      continue;
    }
    const auto *retained = session.snapshot(key.domains);
    if (!retained) {
      std::vector<NaryCspPrefixOwnerSnapshot> merged_owners;
      const OwnerStatus owner_status = prepare_new_owner(
          session, *owner, limits, candidate_session_work, merged_owners);
      if (owner_status == OwnerStatus::Invalid) {
        output.offers.clear();
        output.have_resume_after = false;
        return finish(NaryCspPrefixActionOfferStatus::Invalid, false);
      }
      if (owner_status == OwnerStatus::Exhausted) {
        if (!meter.absorb(candidate_session_work)) return stopped();
        output.effort.session_preflight_work += candidate_session_work;
        ++output.effort.resource_rejected_prefixes;
        continue;
      }
    }
    if (!meter.absorb(candidate_session_work)) return stopped();
    output.effort.session_preflight_work += candidate_session_work;

    NaryCspPrefixLiveRequest request = make_request(
        kernel, limits, selection_from_key(key),
        kernel.structural_action_sequence + 1);
    request.retained_owner = retained ? retained : &*owner;
    const bool recognition_page_limited =
        meter.remaining() < limits.maximum_recognition_work;
    request.maximum_recognition_work =
        std::min(request.maximum_recognition_work, meter.remaining());
    if (!request.maximum_recognition_work) return stopped();
    const auto preflight = nary_csp_prefix_live_preflight(kernel, request);
    if (!meter.absorb(preflight.recognition_work)) return stopped();
    output.effort.preflight_work += preflight.recognition_work;
    output.effort.planning_work += preflight.planning_work;
    output.effort.revalidation_work += preflight.revalidation_work;
    output.effort.proof_work += preflight.proof_work;
    output.effort.journal_work += preflight.journal_work;
    output.effort.planned_additions += preflight.planned_additions;
    output.effort.proof_literal_occurrences +=
        preflight.proof_literal_occurrences;
    for (uint64_t work :
         {uint64_t{1}, preflight.proof_work, preflight.journal_work,
          preflight.planned_additions,
          preflight.proof_literal_occurrences}) {
      if (!meter.charge(work)) return stopped();
      output.effort.preflight_work += work;
    }
    if (preflight.status == NaryCspPrefixLiveStatus::Interrupted) {
      meter.interrupted = true;
      return stopped();
    }
    if (preflight.status == NaryCspPrefixLiveStatus::Stale)
      return finish(NaryCspPrefixActionOfferStatus::Stale, false);
    if (preflight.status != NaryCspPrefixLiveStatus::Applied) {
      if (preflight.status == NaryCspPrefixLiveStatus::ResourceExhausted &&
          preflight.recognition_limit_exhausted &&
          recognition_page_limited) {
        meter.exhausted = true;
        return stopped();
      }
      switch (preflight.status) {
      case NaryCspPrefixLiveStatus::NoConsequence:
        ++output.effort.consequence_free_prefixes;
        continue;
      case NaryCspPrefixLiveStatus::Duplicate:
        ++output.effort.live_duplicate_prefixes;
        continue;
      case NaryCspPrefixLiveStatus::ResourceExhausted:
        ++output.effort.resource_rejected_prefixes;
        continue;
      case NaryCspPrefixLiveStatus::Inapplicable:
        output.offers.clear();
        return finish(NaryCspPrefixActionOfferStatus::Inapplicable, false);
      case NaryCspPrefixLiveStatus::Invalid:
      case NaryCspPrefixLiveStatus::NoStructure:
      case NaryCspPrefixLiveStatus::UnsupportedProof:
        output.offers.clear();
        return finish(NaryCspPrefixActionOfferStatus::Invalid, false);
      case NaryCspPrefixLiveStatus::Interrupted:
      case NaryCspPrefixLiveStatus::Stale:
      case NaryCspPrefixLiveStatus::Applied:
        std::abort();
      }
    }
    if (output.offers.size() == window.maximum_offers) {
      output.status = NaryCspPrefixActionOfferStatus::Ready;
      output.complete = false;
      output.have_resume_after = true;
      output.resume_after =
          make_key(output.offers.back());
      output.effort.enumeration_work = meter.used;
      return output;
    }
    output.offers.push_back(selection_from_key(key));
  }
  if (output.offers.empty() && output.effort.resource_rejected_prefixes)
    return finish(NaryCspPrefixActionOfferStatus::ResourceExhausted, false);
  if (more_relations) {
    if (!output.offers.empty()) {
      output.have_resume_after = true;
      output.resume_after = make_key(output.offers.back());
    } else if (have_last_examined) {
      output.have_resume_after = true;
      output.resume_after = last_examined;
    }
  }
  return finish(NaryCspPrefixActionOfferStatus::Ready, !more_relations);
}

} // namespace tactix
