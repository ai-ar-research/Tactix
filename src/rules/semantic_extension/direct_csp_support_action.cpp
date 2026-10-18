#include "rules/semantic_extension/direct_csp_support_action.hpp"

#include "proof/proof.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <set>
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

bool add_work(uint64_t amount, uint64_t maximum, uint64_t &work,
              bool *exhausted = nullptr) {
  if (work > maximum || amount > maximum - work) {
    if (exhausted) *exhausted = true;
    return false;
  }
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
                         const std::vector<int32_t> &right) {
  if (left.size() < 2 || right.size() < 2) return false;
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

bool key_valid(const DirectCspSupportActionKey &key) {
  return pair_identity_valid(key.left_values, key.right_values) &&
         key.selected_left_value < key.left_values.size() &&
         key.selected_left_literal ==
             key.left_values[key.selected_left_value];
}

bool pair_within_limits(const std::vector<int32_t> &left,
                        const std::vector<int32_t> &right,
                        const DirectCspSupportActionLimits &limits) {
  return left.size() <= limits.maximum_domain_values &&
         right.size() <= limits.maximum_domain_values;
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

bool owner_payload_valid(const DirectCspSupportOwnerSnapshot &owner) {
  if (!owner.external_extent || owner.source.empty() ||
      !pair_identity_valid(owner.relation.left.values,
                           owner.relation.right.values) ||
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
            std::numeric_limits<uint64_t>::max() - source_literals)
      return false;
    source_literals += premise.literals.size();
  }
  if (source_literals != owner.source_literals) return false;
  for (uint64_t occurrence : owner.relation.forbidden)
    if (occurrence && !occurrence_valid(occurrence, owner.source.size()))
      return false;
  return true;
}

bool owner_within_limits(const DirectCspSupportOwnerSnapshot &owner,
                         const DirectCspSupportActionLimits &limits) {
  return owner.source.size() <= limits.maximum_source_clauses &&
         owner.source_literals <= limits.maximum_source_literals &&
         pair_within_limits(owner.relation.left.values,
                            owner.relation.right.values, limits);
}

bool charge_owner(const DirectCspSupportOwnerSnapshot &owner,
                  uint64_t maximum, uint64_t &work,
                  bool *exhausted = nullptr) {
  return add_work(1, maximum, work, exhausted) &&
         add_work(owner.relation.left.values.size(), maximum, work,
                  exhausted) &&
         add_work(owner.relation.right.values.size(), maximum, work,
                  exhausted) &&
         add_work(owner.source.size(), maximum, work, exhausted) &&
         add_work(owner.source_literals, maximum, work, exhausted) &&
         add_work(owner.relation.left.at_most_one.size(), maximum, work,
                  exhausted) &&
         add_work(owner.relation.right.at_most_one.size(), maximum, work,
                  exhausted) &&
         add_work(owner.relation.forbidden.size(), maximum, work, exhausted);
}

enum class SessionStatus : uint8_t { Valid, Invalid, Exhausted };

SessionStatus validate_session(const DirectCspSupportActionSession &session,
                               const DirectCspSupportActionLimits &limits,
                               uint64_t &work,
                               bool *work_exhausted = nullptr) {
  if (session.retained() != (session.retained_owners() != 0) ||
      session.committed_keys() < session.retained_owners())
    return SessionStatus::Invalid;
  const auto &owners = session.snapshots();
  for (size_t index = 0; index < owners.size(); ++index) {
    const auto &owner = owners[index];
    if (!charge_owner(owner, limits.maximum_session_work, work,
                      work_exhausted))
      return SessionStatus::Exhausted;
    if (!owner_payload_valid(owner) ||
        (index && !owner_less(owners[index - 1], owner)))
      return SessionStatus::Invalid;
  }
  const auto &keys = session.keys();
  for (size_t index = 0; index < keys.size(); ++index) {
    const auto &key = keys[index];
    if (!add_work(uint64_t{1} + key.left_values.size() +
                      key.right_values.size(),
                  limits.maximum_session_work, work, work_exhausted) ||
        !add_work(owners.size(), limits.maximum_session_work, work,
                  work_exhausted))
      return SessionStatus::Exhausted;
    if (!key_valid(key) ||
        (index && !(keys[index - 1] < key)) ||
        std::none_of(owners.begin(), owners.end(),
                     [&](const DirectCspSupportOwnerSnapshot &owner) {
                       return owner_matches_key(owner, key);
                     }))
      return SessionStatus::Invalid;
  }
  for (const DirectCspSupportOwnerSnapshot &owner : owners) {
    if (!add_work(keys.size(), limits.maximum_session_work, work,
                  work_exhausted))
      return SessionStatus::Exhausted;
    if (std::none_of(keys.begin(), keys.end(),
                     [&](const DirectCspSupportActionKey &key) {
                       return owner_matches_key(owner, key);
                     }))
      return SessionStatus::Invalid;
  }

  // Invocation limits are resource policy, not part of a retained identity.
  // Once the metered integrity scan completes, corruption remains Invalid even
  // when the caller also supplies caps smaller than the retained state.
  if (keys.size() > limits.maximum_session_keys)
    return SessionStatus::Exhausted;
  for (const DirectCspSupportOwnerSnapshot &owner : owners)
    if (!owner_within_limits(owner, limits))
      return SessionStatus::Exhausted;
  for (const DirectCspSupportActionKey &key : keys)
    if (!pair_within_limits(key.left_values, key.right_values, limits))
      return SessionStatus::Exhausted;
  return SessionStatus::Valid;
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
  return first && second > UINT64_MAX / first ? UINT64_MAX : first * second;
}

uint64_t comparison_levels(uint64_t entries) {
  uint64_t result = 1;
  while (entries > 1) {
    entries = (entries + 1) / 2;
    ++result;
  }
  return result;
}

bool charge_traversal(OfferMeter &meter,
                      DirectCspSupportActionOfferEffort &effort,
                      uint64_t amount) {
  if (!meter.charge(amount)) return false;
  effort.traversal_work += amount;
  return true;
}

enum class DomainScanStatus : uint8_t {
  Complete,
  Exhausted,
  Interrupted,
};

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

enum class BinaryLookupStatus : uint8_t { Absent, Present, Stopped };

BinaryLookupStatus binary_present(Kernel &kernel, Lit first, Lit second,
                                  OfferMeter &meter) {
  for (const WatchEntry watch : kernel.watches.entries(first)) {
    if (!meter.charge(1)) return BinaryLookupStatus::Stopped;
    if (watch.binary() && watch.other() == second)
      return BinaryLookupStatus::Present;
  }
  return BinaryLookupStatus::Absent;
}

BinaryLookupStatus complete_pairwise_amo(Kernel &kernel, const Lit *literals,
                                         uint32_t size,
                                         OfferMeter &meter) {
  for (uint32_t first = 0; first < size; ++first)
    for (uint32_t second = first + 1; second < size; ++second) {
      const BinaryLookupStatus present =
          binary_present(kernel, negate(literals[first]),
                         negate(literals[second]), meter);
      if (present != BinaryLookupStatus::Present) return present;
    }
  return BinaryLookupStatus::Present;
}

DomainScanStatus discover_domain_candidates(
    Kernel &kernel, const DirectCspSupportActionLimits &limits,
    const DirectCspSupportActionOfferWindow &window, OfferMeter &meter,
    DirectCspSupportActionOfferEffort &effort,
    std::vector<std::vector<int32_t>> &domains) {
  const uint64_t begin = meter.used;
  std::set<std::vector<int32_t>> distinct;
  std::vector<int32_t> candidate;
  const auto insert = [&](const Lit *literals, uint32_t size) {
    const uint64_t normalization =
        saturated_product(uint64_t{1} + size, comparison_levels(size));
    if (!meter.charge(normalization)) return false;
    if (!canonical_domain_candidate(kernel, literals, size,
                                    limits.maximum_domain_values, candidate))
      return true;
    const BinaryLookupStatus amo =
        complete_pairwise_amo(kernel, literals, size, meter);
    if (amo == BinaryLookupStatus::Stopped) return false;
    if (amo == BinaryLookupStatus::Absent) return true;
    const uint64_t set_work = saturated_product(
        uint64_t{1} + candidate.size(),
        comparison_levels(uint64_t{1} + distinct.size()));
    if (!meter.charge(set_work)) return false;
    if (distinct.contains(candidate)) return true;
    if (distinct.size() == window.maximum_domain_candidates) {
      meter.exhausted = true;
      return false;
    }
    distinct.insert(candidate);
    return true;
  };

  for (Ref reference = 0; reference < kernel.arena.size();) {
    const Clause *clause = clause_at(kernel.arena, reference);
    reference += clause_actual_words(clause);
    if (!meter.charge(1)) break;
    if (clause->garbage()) continue;
    if (!meter.charge(uint64_t{1} + clause->size)) break;
    ++effort.formula_clauses;
    effort.formula_literals += clause->size;
    if (!insert(clause->lits, clause->size)) break;
  }
  if (!meter.interrupted && !meter.exhausted)
    for (Lit first = 0; first < 2 * kernel.nvars; ++first) {
      for (const WatchEntry watch : kernel.watches.entries(first)) {
        if (!meter.charge(1)) break;
        if (!watch.binary() || first >= watch.other()) continue;
        if (!meter.charge(3)) break;
        const Lit pair[] = {first, watch.other()};
        ++effort.formula_clauses;
        effort.formula_literals += 2;
        if (!insert(pair, 2)) break;
      }
      if (meter.interrupted || meter.exhausted) break;
    }
  effort.candidate_scan_work += meter.used - begin;
  effort.domain_candidates = distinct.size();
  if (meter.interrupted) return DomainScanStatus::Interrupted;
  if (meter.exhausted) return DomainScanStatus::Exhausted;
  domains.assign(distinct.begin(), distinct.end());
  return DomainScanStatus::Complete;
}

bool disjoint_domains(const std::vector<int32_t> &left,
                      const std::vector<int32_t> &right) {
  size_t first = 0, second = 0;
  while (first < left.size() && second < right.size()) {
    const uint32_t a = variable(left[first]);
    const uint32_t b = variable(right[second]);
    if (a == b) return false;
    if (a < b)
      ++first;
    else
      ++second;
  }
  return true;
}

bool same_owner_image(const DirectCspSupportOwnerSnapshot &first,
                      const DirectCspSupportOwnerSnapshot &second) {
  if (first.source_literals != second.source_literals ||
      first.source.size() != second.source.size())
    return false;
  for (size_t index = 0; index < first.source.size(); ++index) {
    const auto &a = first.source[index];
    const auto &b = second.source[index];
    if (a.occurrence != b.occurrence ||
        a.native_proof_id != b.native_proof_id || a.literals != b.literals)
      return false;
  }
  const auto same_domain = [](const direct_csp_support::Domain &a,
                              const direct_csp_support::Domain &b) {
    return a.values == b.values && a.at_least_one == b.at_least_one &&
           a.at_most_one == b.at_most_one;
  };
  return same_domain(first.relation.left, second.relation.left) &&
         same_domain(first.relation.right, second.relation.right) &&
         first.relation.forbidden == second.relation.forbidden;
}

bool committed(const DirectCspSupportActionSession &session,
               const DirectCspSupportActionKey &key) {
  const auto &keys = session.keys();
  const auto position = std::lower_bound(keys.begin(), keys.end(), key);
  return position != keys.end() && *position == key;
}

DirectCspSupportActionKey row_key(const std::vector<int32_t> &left,
                                  const std::vector<int32_t> &right,
                                  uint32_t row) {
  return {left, right, row, left[row]};
}

enum class CaptureStatus : uint8_t {
  Complete,
  NoStructure,
  RowResourceExhausted,
  ActionResourceExhausted,
  PageExhausted,
  Interrupted,
  Inapplicable,
  Invalid,
};

DirectCspSupportLiveRequest make_request(
    const Kernel &, const DirectCspSupportActionLimits &,
    const DirectCspSupportActionSelection &, uint64_t);

CaptureStatus capture_owner_for_row(
    Kernel &kernel, const DirectCspSupportActionLimits &limits,
    const std::vector<int32_t> &left, const std::vector<int32_t> &right,
    uint32_t row, OfferMeter &meter,
    DirectCspSupportActionOfferEffort &effort,
    DirectCspSupportOwnerSnapshot &owner) {
  if (!meter.remaining()) {
    meter.exhausted = true;
    return CaptureStatus::PageExhausted;
  }
  DirectCspSupportActionSelection selection{left, right, row};
  DirectCspSupportLiveRequest request =
      make_request(kernel, limits, selection,
                   kernel.structural_action_sequence + 1);
  request.maximum_recognition_work =
      std::min(request.maximum_recognition_work, meter.remaining());
  DirectCspSupportOwnerSnapshot captured;
  const DirectCspSupportOwnerCaptureResult result =
      direct_csp_support_live_capture_owner(kernel, request, captured);
  if (!meter.absorb(result.recognition_work))
    return CaptureStatus::PageExhausted;
  effort.owner_capture_work += result.recognition_work;
  switch (result.status) {
  case DirectCspSupportLiveStatus::Applied:
    owner = std::move(captured);
    return CaptureStatus::Complete;
  case DirectCspSupportLiveStatus::NoStructure:
    return CaptureStatus::NoStructure;
  case DirectCspSupportLiveStatus::ResourceExhausted:
    if (result.recognition_limit_exhausted)
      return request.maximum_recognition_work <
                     limits.maximum_recognition_work
                 ? CaptureStatus::PageExhausted
                 : CaptureStatus::ActionResourceExhausted;
    return CaptureStatus::RowResourceExhausted;
  case DirectCspSupportLiveStatus::Interrupted:
    return CaptureStatus::Interrupted;
  case DirectCspSupportLiveStatus::Inapplicable:
    return CaptureStatus::Inapplicable;
  case DirectCspSupportLiveStatus::Invalid:
    return CaptureStatus::Invalid;
  case DirectCspSupportLiveStatus::NoConsequence:
  case DirectCspSupportLiveStatus::Duplicate:
  case DirectCspSupportLiveStatus::UnsupportedProof:
  case DirectCspSupportLiveStatus::Stale:
    return CaptureStatus::Invalid;
  }
  std::abort();
}

CaptureStatus capture_owner_for_pair(
    Kernel &kernel, const DirectCspSupportActionLimits &limits,
    const std::vector<int32_t> &left, const std::vector<int32_t> &right,
    OfferMeter &meter, DirectCspSupportActionOfferEffort &effort,
    DirectCspSupportOwnerSnapshot &owner) {
  bool row_exhausted = false;
  for (uint32_t row = 0; row < left.size(); ++row) {
    const CaptureStatus status = capture_owner_for_row(
        kernel, limits, left, right, row, meter, effort, owner);
    if (status == CaptureStatus::Complete ||
        status == CaptureStatus::NoStructure ||
        status == CaptureStatus::ActionResourceExhausted ||
        status == CaptureStatus::PageExhausted ||
        status == CaptureStatus::Interrupted ||
        status == CaptureStatus::Inapplicable ||
        status == CaptureStatus::Invalid)
      return status;
    row_exhausted = true;
    if (!meter.remaining()) break;
  }
  return row_exhausted ? CaptureStatus::ActionResourceExhausted
                       : CaptureStatus::Invalid;
}

KeyStatus prepare_keys(const DirectCspSupportActionSession &session,
                       const DirectCspSupportActionKey &key,
                       const DirectCspSupportActionLimits &limits,
                       uint64_t &work,
                       std::vector<DirectCspSupportActionKey> &merged,
                       bool *work_exhausted = nullptr) {
  if (!add_work(uint64_t{1} + key.left_values.size() +
                    key.right_values.size(),
                limits.maximum_session_work, work, work_exhausted))
    return KeyStatus::Exhausted;
  if (!key_valid(key)) return KeyStatus::Invalid;
  const SessionStatus session_status =
      validate_session(session, limits, work, work_exhausted);
  if (session_status == SessionStatus::Invalid) return KeyStatus::Invalid;
  if (session_status == SessionStatus::Exhausted)
    return KeyStatus::Exhausted;
  if (!pair_within_limits(key.left_values, key.right_values, limits))
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
                limits.maximum_session_work, work, work_exhausted))
    return KeyStatus::Exhausted;
  for (const DirectCspSupportActionKey &retained : old)
    if (!add_work(uint64_t{1} + retained.left_values.size() +
                      retained.right_values.size(),
                  limits.maximum_session_work, work, work_exhausted))
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
    std::vector<DirectCspSupportOwnerSnapshot> &merged,
    bool *work_exhausted = nullptr) {
  const auto &old = session.snapshots();
  if (!charge_owner(incoming, limits.maximum_session_work, work,
                    work_exhausted))
    return OwnerStatus::Exhausted;
  if (!owner_payload_valid(incoming))
    return OwnerStatus::Invalid;
  const auto position =
      std::lower_bound(old.begin(), old.end(), incoming, owner_less);
  if (position != old.end() && !owner_less(incoming, *position) &&
      !owner_less(*position, incoming))
    return OwnerStatus::Invalid;
  if (old.size() >= limits.maximum_session_keys ||
      !owner_within_limits(incoming, limits))
    return OwnerStatus::Exhausted;
  // Charge separately for materializing the post-commit copy after the
  // integrity scan above.
  if (!charge_owner(incoming, limits.maximum_session_work, work,
                    work_exhausted))
    return OwnerStatus::Exhausted;
  for (const DirectCspSupportOwnerSnapshot &retained : old)
    if (!charge_owner(retained, limits.maximum_session_work, work,
                      work_exhausted))
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

bool DirectCspSupportActionOfferWindow::valid() const {
  return maximum_domain_candidates && maximum_pair_candidates &&
         maximum_row_candidates && maximum_offers &&
         maximum_enumeration_work && (!have_after || key_valid(after));
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
            !kernel.proof || (!kernel.proof->lrat() && !kernel.proof->terminal_pbp()));
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

DirectCspSupportActionOfferResult direct_csp_support_action_offer(
    Kernel &kernel, const DirectCspSupportActionSession &session,
    const DirectCspSupportActionLimits &limits,
    const DirectCspSupportActionOfferWindow &window) {
  DirectCspSupportActionOfferResult output;
  if (!direct_csp_support_action_guard(kernel)) {
    output.status = DirectCspSupportActionOfferStatus::Inapplicable;
    return output;
  }
  if (!limits.valid() || !window.valid()) return output;
  if (session.retained() && !session.belongs_to(kernel)) {
    output.status = DirectCspSupportActionOfferStatus::Stale;
    return output;
  }

  const SessionStatus session_status =
      validate_session(session, limits, output.effort.session_work);
  if (session_status == SessionStatus::Invalid) return output;
  if (session_status == SessionStatus::Exhausted ||
      session.committed_keys() >= limits.maximum_session_keys) {
    output.status = DirectCspSupportActionOfferStatus::ResourceExhausted;
    return output;
  }

  OfferMeter meter{kernel, window.maximum_enumeration_work};
  const auto stopped = [&](DirectCspSupportActionOfferStatus status) {
    output.status = status;
    output.complete = false;
    output.have_resume_after = false;
    output.offers.clear();
    output.effort.enumeration_work = meter.used;
    return output;
  };
  const auto page = [&](const DirectCspSupportActionKey &last) {
    output.status = DirectCspSupportActionOfferStatus::Ready;
    output.complete = false;
    output.have_resume_after = true;
    output.resume_after = last;
    output.effort.enumeration_work = meter.used;
    return output;
  };

  bool have_last = false;
  DirectCspSupportActionKey last;
  const auto page_or_resource = [&]() {
    if (meter.interrupted)
      return stopped(DirectCspSupportActionOfferStatus::Interrupted);
    return have_last ? page(last)
                     : stopped(DirectCspSupportActionOfferStatus::
                                   ResourceExhausted);
  };

  // A retained owner is authoritative only while a fresh exact census still
  // has identical premise literals, native identities, and relation tables.
  // This check is independent of the requested page cursor.
  for (const DirectCspSupportOwnerSnapshot &retained : session.snapshots()) {
    DirectCspSupportOwnerSnapshot current;
    const CaptureStatus capture = capture_owner_for_pair(
        kernel, limits, retained.relation.left.values,
        retained.relation.right.values, meter, output.effort, current);
    if (capture == CaptureStatus::Interrupted)
      return stopped(DirectCspSupportActionOfferStatus::Interrupted);
    if (capture == CaptureStatus::PageExhausted ||
        capture == CaptureStatus::ActionResourceExhausted ||
        capture == CaptureStatus::RowResourceExhausted)
      return stopped(DirectCspSupportActionOfferStatus::ResourceExhausted);
    if (capture == CaptureStatus::Inapplicable)
      return stopped(DirectCspSupportActionOfferStatus::Inapplicable);
    if (capture == CaptureStatus::Invalid)
      return stopped(DirectCspSupportActionOfferStatus::Invalid);
    if (capture == CaptureStatus::NoStructure ||
        !same_owner_image(current, retained))
      return stopped(DirectCspSupportActionOfferStatus::Stale);
    ++output.effort.retained_owners_revalidated;
  }

  std::vector<std::vector<int32_t>> domains;
  const DomainScanStatus scan = discover_domain_candidates(
      kernel, limits, window, meter, output.effort, domains);
  if (scan == DomainScanStatus::Interrupted)
    return stopped(DirectCspSupportActionOfferStatus::Interrupted);
  if (scan == DomainScanStatus::Exhausted)
    return stopped(DirectCspSupportActionOfferStatus::ResourceExhausted);
  if (domains.size() < 2) {
    output.status = DirectCspSupportActionOfferStatus::NoStructure;
    output.complete = true;
    output.effort.enumeration_work = meter.used;
    return output;
  }

  uint64_t certified_pairs = 0;
  for (const std::vector<int32_t> &left : domains)
    for (const std::vector<int32_t> &right : domains) {
      if (!charge_traversal(meter, output.effort,
                            uint64_t{3} + left.size() + right.size()))
        return page_or_resource();
      if (left == right || !disjoint_domains(left, right)) continue;
      if (!charge_traversal(meter, output.effort,
                            uint64_t{1} + left.size() + right.size()))
        return page_or_resource();
      const DirectCspSupportActionKey pair_last =
          row_key(left, right, static_cast<uint32_t>(left.size() - 1));
      if (window.have_after) {
        if (!charge_traversal(
                meter, output.effort,
                uint64_t{1} + left.size() + right.size() +
                    window.after.left_values.size() +
                    window.after.right_values.size()))
          return page_or_resource();
        if (!(window.after < pair_last)) continue;
      }
      if (output.effort.pair_candidates == window.maximum_pair_candidates) {
        if (!have_last) std::abort();
        return page(last);
      }
      ++output.effort.pair_candidates;

      const uint64_t owner_lookup_work = saturated_product(
          uint64_t{1} + left.size() + right.size(),
          uint64_t{1} + session.retained_owners());
      if (!charge_traversal(meter, output.effort, owner_lookup_work))
        return page_or_resource();
      const DirectCspSupportOwnerSnapshot *retained =
          session.snapshot(left, right);
      DirectCspSupportOwnerSnapshot captured;
      const DirectCspSupportOwnerSnapshot *owner = retained;
      if (!owner) {
        const CaptureStatus capture = capture_owner_for_pair(
            kernel, limits, left, right, meter, output.effort, captured);
        if (capture == CaptureStatus::Interrupted)
          return stopped(DirectCspSupportActionOfferStatus::Interrupted);
        if (capture == CaptureStatus::Inapplicable)
          return stopped(DirectCspSupportActionOfferStatus::Inapplicable);
        if (capture == CaptureStatus::Invalid)
          return stopped(DirectCspSupportActionOfferStatus::Invalid);
        if (capture == CaptureStatus::PageExhausted) {
          ++output.effort.page_limited_captures;
          return page_or_resource();
        }
        if (capture == CaptureStatus::ActionResourceExhausted ||
            capture == CaptureStatus::RowResourceExhausted) {
          output.effort.resource_rejected_rows += left.size();
          last = pair_last;
          have_last = true;
          continue;
        }
        if (capture == CaptureStatus::NoStructure) {
          last = pair_last;
          have_last = true;
          continue;
        }
        owner = &captured;
      }
      ++certified_pairs;
      for (uint32_t row = 0; row < left.size(); ++row) {
        if (!charge_traversal(meter, output.effort,
                              uint64_t{2} + left.size() + right.size()))
          return page_or_resource();
        const DirectCspSupportActionKey key = row_key(left, right, row);
        if (window.have_after) {
          if (!charge_traversal(
                  meter, output.effort,
                  uint64_t{1} + left.size() + right.size() +
                      window.after.left_values.size() +
                      window.after.right_values.size()))
            return page_or_resource();
          if (!(window.after < key)) continue;
        }
        if (output.effort.row_candidates == window.maximum_row_candidates) {
          if (!have_last) std::abort();
          return page(last);
        }
        ++output.effort.row_candidates;
        const uint64_t key_lookup_work = saturated_product(
            uint64_t{1} + left.size() + right.size(),
            comparison_levels(uint64_t{1} + session.committed_keys()));
        if (!charge_traversal(meter, output.effort, key_lookup_work))
          return page_or_resource();
        if (committed(session, key)) {
          ++output.effort.committed_rows_skipped;
          last = key;
          have_last = true;
          continue;
        }

        DirectCspSupportActionLimits bounded = limits;
        const bool session_page_limited =
            meter.remaining() < limits.maximum_session_work;
        bounded.maximum_session_work =
            std::min(limits.maximum_session_work, meter.remaining());
        uint64_t candidate_session_work = 0;
        bool candidate_work_exhausted = false;
        std::vector<DirectCspSupportActionKey> merged_keys;
        const KeyStatus key_status =
            prepare_keys(session, key, bounded, candidate_session_work,
                         merged_keys, &candidate_work_exhausted);
        if (key_status == KeyStatus::Invalid) {
          if (!meter.absorb(candidate_session_work)) return page_or_resource();
          output.effort.session_preflight_work += candidate_session_work;
          return stopped(DirectCspSupportActionOfferStatus::Invalid);
        }
        if (key_status == KeyStatus::Exhausted) {
          if (!meter.absorb(candidate_session_work)) return page_or_resource();
          output.effort.session_preflight_work += candidate_session_work;
          if (session_page_limited && candidate_work_exhausted)
            return page_or_resource();
          ++output.effort.resource_rejected_rows;
          last = key;
          have_last = true;
          continue;
        }

        // Apply charges a complete retained-owner lookup after preparing the
        // key, before it decides whether that key was already committed. Keep
        // this in the same cumulative allowance as both session copies.
        if (!add_work(session.retained_owners(),
                      bounded.maximum_session_work, candidate_session_work,
                      &candidate_work_exhausted)) {
          if (!meter.absorb(candidate_session_work)) return page_or_resource();
          output.effort.session_preflight_work += candidate_session_work;
          if (session_page_limited) return page_or_resource();
          ++output.effort.resource_rejected_rows;
          last = key;
          have_last = true;
          continue;
        }
        if (key_status == KeyStatus::Present) {
          if (!meter.absorb(candidate_session_work)) return page_or_resource();
          output.effort.session_preflight_work += candidate_session_work;
          ++output.effort.committed_rows_skipped;
          last = key;
          have_last = true;
          continue;
        }
        if (!retained) {
          std::vector<DirectCspSupportOwnerSnapshot> merged_owners;
          const OwnerStatus owner_status = prepare_new_owner(
              session, *owner, bounded, candidate_session_work, merged_owners,
              &candidate_work_exhausted);
          if (owner_status == OwnerStatus::Invalid) {
            if (!meter.absorb(candidate_session_work))
              return page_or_resource();
            output.effort.session_preflight_work += candidate_session_work;
            return stopped(DirectCspSupportActionOfferStatus::Invalid);
          }
          if (owner_status == OwnerStatus::Exhausted) {
            if (!meter.absorb(candidate_session_work))
              return page_or_resource();
            output.effort.session_preflight_work += candidate_session_work;
            if (session_page_limited && candidate_work_exhausted)
              return page_or_resource();
            ++output.effort.resource_rejected_rows;
            last = key;
            have_last = true;
            continue;
          }
        }
        if (!meter.absorb(candidate_session_work)) return page_or_resource();
        output.effort.session_preflight_work += candidate_session_work;

        DirectCspSupportActionSelection selection{left, right, row};
        DirectCspSupportLiveRequest request = make_request(
            kernel, limits, selection, kernel.structural_action_sequence + 1);
        request.retained_owner = owner;
        const bool recognition_page_limited =
            meter.remaining() < limits.maximum_recognition_work;
        request.maximum_recognition_work = std::min(
            limits.maximum_recognition_work, meter.remaining());
        const DirectCspSupportLiveResult preflight =
            direct_csp_support_live_preflight(kernel, request);
        if (!meter.absorb(preflight.recognition_work))
          return page_or_resource();
        output.effort.planning_work += preflight.planning_work;
        output.effort.duplicate_scan_work += preflight.revalidation_work;
        for (uint64_t materialization_work :
             {uint64_t{1}, preflight.proof_work, preflight.journal_work,
              preflight.planned_additions,
              preflight.proof_literal_occurrences})
          if (!charge_traversal(meter, output.effort,
                                materialization_work))
            return page_or_resource();

        switch (preflight.status) {
        case DirectCspSupportLiveStatus::Applied:
          break;
        case DirectCspSupportLiveStatus::NoConsequence:
          ++output.effort.consequence_free_rows;
          break;
        case DirectCspSupportLiveStatus::Duplicate:
          ++output.effort.live_consequences_skipped;
          break;
        case DirectCspSupportLiveStatus::ResourceExhausted:
          if (preflight.recognition_limit_exhausted &&
              recognition_page_limited)
            return page_or_resource();
          ++output.effort.resource_rejected_rows;
          break;
        case DirectCspSupportLiveStatus::Interrupted:
          return stopped(DirectCspSupportActionOfferStatus::Interrupted);
        case DirectCspSupportLiveStatus::Stale:
          return stopped(DirectCspSupportActionOfferStatus::Stale);
        case DirectCspSupportLiveStatus::Inapplicable:
          return stopped(DirectCspSupportActionOfferStatus::Inapplicable);
        case DirectCspSupportLiveStatus::Invalid:
        case DirectCspSupportLiveStatus::NoStructure:
        case DirectCspSupportLiveStatus::UnsupportedProof:
          return stopped(DirectCspSupportActionOfferStatus::Invalid);
        }
        last = key;
        have_last = true;
        if (preflight.status == DirectCspSupportLiveStatus::Applied)
          output.offers.push_back(std::move(selection));
        if (output.offers.size() == window.maximum_offers) return page(last);
      }
    }

  output.complete = true;
  output.effort.enumeration_work = meter.used;
  if (output.offers.empty() && output.effort.resource_rejected_rows) {
    output.status = DirectCspSupportActionOfferStatus::ResourceExhausted;
    output.complete = false;
    return output;
  }
  if (!certified_pairs && !window.have_after && !session.retained()) {
    output.status = DirectCspSupportActionOfferStatus::NoStructure;
    output.offers.clear();
  } else {
    output.status = DirectCspSupportActionOfferStatus::Ready;
  }
  return output;
}

} // namespace tactix
