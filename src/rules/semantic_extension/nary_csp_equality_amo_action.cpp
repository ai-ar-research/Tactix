#include "rules/semantic_extension/nary_csp_equality_amo_action.hpp"

#include "proof/proof.hpp"

#include <algorithm>
#include <array>
#include <climits>
#include <cstdlib>
#include <limits>
#include <string>
#include <utility>

namespace tactix {

struct NaryCspEqualityAmoSessionAccess {
  static void commit(NaryCspEqualityAmoSession &session, const Kernel &kernel,
                     std::vector<NaryCspEqualityAmoKey> keys) {
    session.owner_ = &kernel;
    session.keys_ = std::move(keys);
  }
};

namespace {

uint32_t external_variable(int32_t literal) {
  return external_variable_of(literal);
}

bool literal_less(int32_t first, int32_t second) {
  return std::pair(external_variable(first), first) <
         std::pair(external_variable(second), second);
}

NaryCspEqualityKey equality_key(
    const std::vector<std::vector<int32_t>> &domains, uint32_t residual) {
  NaryCspEqualityKey key;
  key.domains = domains;
  key.residual_domain = residual;
  std::array<uint32_t, 2> equal{};
  size_t next = 0;
  for (uint32_t domain = 0; domain != 3; ++domain)
    if (domain != residual) equal[next++] = domain;
  key.first_equal_domain = equal[0];
  key.second_equal_domain = equal[1];
  return key;
}

const NaryCspEqualityRecord *find_record(
    const NaryCspEqualitySession &session,
    const std::vector<std::vector<int32_t>> &domains, uint32_t residual) {
  const NaryCspEqualityKey key = equality_key(domains, residual);
  const auto found = std::lower_bound(
      session.records().begin(), session.records().end(), key,
      [](const NaryCspEqualityRecord &record, const NaryCspEqualityKey &value) {
        return record.key < value;
      });
  return found != session.records().end() && found->key == key ? &*found
                                                               : nullptr;
}

bool clause_live(const Kernel &kernel, const std::vector<int32_t> &external) {
  std::vector<Lit> wanted;
  wanted.reserve(external.size());
  for (int32_t literal : external) {
    const Lit internal = kernel_internalize(kernel, literal);
    if (internal == INVALID_LIT ||
        kernel_var_retired(kernel, var_of(internal)))
      return false;
    wanted.push_back(internal);
  }
  std::sort(wanted.begin(), wanted.end());
  if (wanted.size() == 2)
    return kernel_binary_live(kernel, wanted[0], wanted[1]);
  for (Ref ref = 0; ref < kernel.arena.size();) {
    const Clause *clause = clause_at(kernel.arena, ref);
    const uint32_t words = clause_actual_words(clause);
    if (!clause->garbage() && clause->size == wanted.size()) {
      std::vector<Lit> current(clause->lits, clause->lits + clause->size);
      std::sort(current.begin(), current.end());
      if (current == wanted) return true;
    }
    ref += words;
  }
  return false;
}

NaryCspRelationCaptureRequest capture_request(
    const NaryCspEqualityAmoSelection &selection,
    const NaryCspEqualityAmoLimits &limits) {
  NaryCspRelationCaptureRequest request;
  request.domains = selection.domains;
  request.maximum_source_clauses = limits.maximum_source_clauses;
  request.maximum_source_literals = limits.maximum_source_literals;
  request.maximum_recognition_work = limits.maximum_recognition_work;
  request.maximum_domains = 3;
  request.maximum_domain_values = limits.maximum_domain_values;
  request.maximum_forbidden_tuples = limits.maximum_forbidden_tuples;
  request.maximum_external_extent = limits.maximum_external_extent;
  return request;
}

bool append_bounded(std::string &journal, uint64_t limit,
                    const std::string &text) {
  if (text.size() > limit - std::min<uint64_t>(limit, journal.size()))
    return false;
  journal += text;
  return true;
}

std::string domains_json(
    const std::vector<std::vector<int32_t>> &domains) {
  std::string result = "[";
  for (size_t d = 0; d < domains.size(); ++d) {
    if (d) result += ',';
    result += '[';
    for (size_t i = 0; i < domains[d].size(); ++i) {
      if (i) result += ',';
      result += std::to_string(domains[d][i]);
    }
    result += ']';
  }
  return result + ']';
}

uint32_t shared_domain(const NaryCspEqualityRecord &first,
                       const NaryCspEqualityRecord &second) {
  const std::array<uint32_t, 2> a{first.key.first_equal_domain,
                                  first.key.second_equal_domain};
  const std::array<uint32_t, 2> b{second.key.first_equal_domain,
                                  second.key.second_equal_domain};
  for (uint32_t x : a)
    for (uint32_t y : b)
      if (x == y) return x;
  return UINT32_MAX;
}

} // namespace

bool NaryCspEqualityAmoLimits::valid() const {
  return maximum_source_clauses && maximum_source_literals &&
         maximum_recognition_work && maximum_domain_values >= 2 &&
         maximum_forbidden_tuples && maximum_proof_work &&
         maximum_proof_additions && maximum_proof_literal_occurrences &&
         maximum_journal_bytes && maximum_external_extent &&
         maximum_session_keys;
}

RuleEligibility nary_csp_equality_amo_guard(const Kernel &kernel) {
  return nary_csp_relation_owner_guard(kernel)
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

bool nary_csp_equality_amo_selection_valid(
    const NaryCspEqualityAmoSelection &selection) {
  if (selection.domains.size() != 3) return false;
  const size_t values = selection.domains.front().size();
  if (values < 2) return false;
  std::vector<uint32_t> variables;
  for (size_t domain = 0; domain < 3; ++domain) {
    const auto &literals = selection.domains[domain];
    if (literals.size() != values ||
        (domain && !(selection.domains[domain - 1] < literals)))
      return false;
    for (size_t index = 0; index < values; ++index) {
      const int32_t literal = literals[index];
      if (!literal || literal == INT32_MIN ||
          (index && !literal_less(literals[index - 1], literal)))
        return false;
      variables.push_back(external_variable(literal));
    }
  }
  std::sort(variables.begin(), variables.end());
  return std::adjacent_find(variables.begin(), variables.end()) ==
         variables.end();
}

bool nary_csp_equality_amo_selection_within_limits(
    const NaryCspEqualityAmoSelection &selection,
    const NaryCspEqualityAmoLimits &limits) {
  return limits.valid() && nary_csp_equality_amo_selection_valid(selection) &&
         selection.domains.front().size() <= limits.maximum_domain_values;
}

NaryCspEqualityAmoResult nary_csp_equality_amo_apply(
    Kernel &kernel, const NaryCspRelationOwnerSession &relation_session,
    const NaryCspEqualitySession &equality_session,
    NaryCspEqualityAmoSession &session,
    const NaryCspEqualityAmoLimits &limits,
    const NaryCspEqualityAmoSelection &selection, FILE *records) {
  NaryCspEqualityAmoResult result;
  if (!nary_csp_equality_amo_guard(kernel)) {
    result.status = NaryCspEqualityAmoStatus::Inapplicable;
    return result;
  }
  if (!nary_csp_equality_amo_selection_within_limits(selection, limits) ||
      !records || kernel.structural_action_sequence == UINT64_MAX)
    return result;
  if (!relation_session.belongs_to(kernel) ||
      !equality_session.belongs_to(kernel) ||
      (session.retained() && !session.belongs_to(kernel))) {
    result.status = NaryCspEqualityAmoStatus::Inapplicable;
    return result;
  }
  if (kernel.proof && (kernel.proof->lrat() || kernel.proof->terminal_pbp())) {
    result.status = NaryCspEqualityAmoStatus::UnsupportedProof;
    return result;
  }
  const NaryCspRelationOwnerSnapshot *owner =
      relation_session.snapshot(selection.domains);
  if (!owner) {
    result.status = NaryCspEqualityAmoStatus::NoStructure;
    return result;
  }
  const NaryCspEqualityAmoKey key{selection.domains};
  if (std::binary_search(session.keys().begin(), session.keys().end(), key)) {
    result.status = NaryCspEqualityAmoStatus::Duplicate;
    return result;
  }
  if (session.keys().size() >= limits.maximum_session_keys) {
    result.status = NaryCspEqualityAmoStatus::ResourceExhausted;
    return result;
  }
  std::array<const NaryCspEqualityRecord *, 3> equalities{};
  const uint64_t expected_definition_clauses =
      3 * static_cast<uint64_t>(selection.domains.front().size());
  for (uint32_t residual = 0; residual != 3; ++residual) {
    equalities[residual] =
        find_record(equality_session, selection.domains, residual);
    if (!equalities[residual]) {
      result.status = NaryCspEqualityAmoStatus::MissingEquality;
      return result;
    }
    if (!equalities[residual]->extension_variable ||
        equalities[residual]->definition_clauses.size() !=
            expected_definition_clauses) {
      result.status = NaryCspEqualityAmoStatus::Stale;
      return result;
    }
    for (const auto &clause : equalities[residual]->definition_clauses)
      if (!clause_live(kernel, clause)) {
        result.status = NaryCspEqualityAmoStatus::Stale;
        return result;
      }
  }
  NaryCspRelationOwnerSnapshot current;
  const auto captured = nary_csp_relation_capture_owner(
      kernel, capture_request(selection, limits), current);
  result.recognition_work = captured.recognition_work;
  if (captured.status != NaryCspRelationCaptureStatus::Applied) {
    result.status = captured.status == NaryCspRelationCaptureStatus::Interrupted
                        ? NaryCspEqualityAmoStatus::Interrupted
                    : captured.status ==
                              NaryCspRelationCaptureStatus::ResourceExhausted
                        ? NaryCspEqualityAmoStatus::ResourceExhausted
                        : NaryCspEqualityAmoStatus::Stale;
    return result;
  }
  current.capture_work = owner->capture_work;
  current.external_extent = owner->external_extent;
  if (current != *owner) {
    result.status = NaryCspEqualityAmoStatus::Stale;
    return result;
  }

  const uint64_t values = selection.domains.front().size();
  const uint64_t helper_count = 3 * values;
  const uint64_t additions = helper_count + 3;
  const uint64_t deletions = helper_count;
  const uint64_t literals = helper_count * 3 + 3 * 2 + helper_count * 3;
  const uint64_t work = additions + deletions + literals;
  if (additions > limits.maximum_proof_additions ||
      literals > limits.maximum_proof_literal_occurrences ||
      work > limits.maximum_proof_work ||
      kernel.next_clause_id > uint64_t(INT64_MAX) - additions - deletions) {
    result.status = NaryCspEqualityAmoStatus::ResourceExhausted;
    return result;
  }
  std::string journal;
  const uint64_t action_id = kernel.structural_action_sequence + 1;
  if (!append_bounded(
          journal, limits.maximum_journal_bytes,
          "{\"kind\":\"nary-csp-equality-amo\",\"action\":" +
              std::to_string(action_id) + ",\"domains\":" +
              domains_json(selection.domains) +
              ",\"projection\":\"identity\",\"new_variables\":0}\n")) {
    result.status = NaryCspEqualityAmoStatus::ResourceExhausted;
    return result;
  }
  if (kernel_terminating(kernel)) {
    result.status = NaryCspEqualityAmoStatus::Interrupted;
    return result;
  }

  result.proposed_action_id = action_id;
  result.proof_work = work;
  result.proof_additions = additions;
  result.proof_deletions = deletions;
  result.proof_literal_occurrences = literals;
  result.journal_bytes = journal.size();
  result.equality_variables = 3;
  kernel.close_startup_certificate();
  const std::array<std::pair<uint32_t, uint32_t>, 3> pairs{
      std::pair<uint32_t, uint32_t>{0, 1}, {0, 2}, {1, 2}};
  for (const auto &[a, b] : pairs) {
    const auto &first = *equalities[a];
    const auto &second = *equalities[b];
    const uint32_t shared = shared_domain(first, second);
    if (shared >= 3) std::abort();
    std::vector<std::pair<uint64_t, std::vector<int32_t>>> helpers;
    for (int32_t value : selection.domains[shared]) {
      std::vector<int32_t> helper{
          -static_cast<int32_t>(first.extension_variable),
          -static_cast<int32_t>(second.extension_variable), -value};
      const uint64_t id = ++kernel.next_clause_id;
      if (kernel.proof &&
          !kernel.proof->add_external_rup(id, helper.data(), helper.size()))
        std::abort();
      helpers.push_back({id, std::move(helper)});
    }
    std::vector<int32_t> external{
        -static_cast<int32_t>(first.extension_variable),
        -static_cast<int32_t>(second.extension_variable)};
    std::array<Lit, 2> internal{kernel_internalize(kernel, external[0]),
                                kernel_internalize(kernel, external[1])};
    if (internal[0] == INVALID_LIT || internal[1] == INVALID_LIT ||
        kernel_binary_live(kernel, internal[0], internal[1]))
      std::abort();
    const uint64_t id = ++kernel.next_clause_id;
    if (kernel.proof &&
        !kernel.proof->add_external_rup(id, external.data(), external.size()))
      std::abort();
    if (!result.first_published_clause_id)
      result.first_published_clause_id = id;
    result.last_published_clause_id = id;
    kernel_add_binary_clause(kernel, internal[0], internal[1], id);
    result.published_consequences.push_back(external);
    ++result.emitted_binary_clauses;
    for (const auto &[helper_id, helper] : helpers) {
      const uint64_t deletion_id = ++kernel.next_clause_id;
      if (kernel.proof &&
          !kernel.proof->del_external(deletion_id, helper_id, helper.data(),
                                      helper.size()))
        std::abort();
    }
  }
  if (std::fwrite(journal.data(), 1, journal.size(), records) !=
          journal.size() ||
      std::fflush(records) || std::ferror(records))
    std::abort();
  std::vector<NaryCspEqualityAmoKey> keys = session.keys();
  keys.insert(std::lower_bound(keys.begin(), keys.end(), key), key);
  NaryCspEqualityAmoSessionAccess::commit(session, kernel, std::move(keys));
  kernel.structural_action_sequence = action_id;
  result.committed_action_id = action_id;
  result.propagation_pending = !kernel_propagation_fixpoint(kernel);
  result.status = NaryCspEqualityAmoStatus::Applied;
  result.result = ApplyResult::Ok;
  return result;
}

bool NaryCspEqualityAmoOfferWindow::valid() const {
  return maximum_owner_candidates && maximum_offers &&
         maximum_enumeration_work &&
         (!have_after || after.domains.size() == 3);
}

NaryCspEqualityAmoOfferResult nary_csp_equality_amo_offer(
    Kernel &kernel, const NaryCspRelationOwnerSession &relation_session,
    const NaryCspEqualitySession &equality_session,
    const NaryCspEqualityAmoSession &session,
    const NaryCspEqualityAmoLimits &limits,
    const NaryCspEqualityAmoOfferWindow &window) {
  NaryCspEqualityAmoOfferResult result;
  if (!nary_csp_equality_amo_guard(kernel)) {
    result.status = NaryCspEqualityAmoOfferStatus::Inapplicable;
    return result;
  }
  if (!limits.valid() || !window.valid()) return result;
  if (!relation_session.belongs_to(kernel) ||
      !equality_session.belongs_to(kernel) ||
      (session.retained() && !session.belongs_to(kernel))) {
    result.status = NaryCspEqualityAmoOfferStatus::Stale;
    return result;
  }
  result.retained_owners = relation_session.retained_owners();
  for (const auto &owner : relation_session.snapshots()) {
    if (result.owner_candidates == window.maximum_owner_candidates ||
        result.enumeration_work == window.maximum_enumeration_work) {
      result.status = NaryCspEqualityAmoOfferStatus::ResourceExhausted;
      result.offers.clear();
      return result;
    }
    ++result.owner_candidates;
    ++result.enumeration_work;
    if (kernel_terminating(kernel)) {
      result.status = NaryCspEqualityAmoOfferStatus::Interrupted;
      result.offers.clear();
      return result;
    }
    std::vector<std::vector<int32_t>> domains;
    for (const auto &domain : owner.relation.domains)
      domains.push_back(domain.values);
    NaryCspEqualityAmoSelection selection{domains};
    if (!nary_csp_equality_amo_selection_within_limits(selection, limits))
      continue;
    bool complete = true;
    for (uint32_t residual = 0; residual != 3; ++residual)
      complete &= find_record(equality_session, domains, residual) != nullptr;
    if (!complete) {
      ++result.missing_equalities;
      continue;
    }
    const NaryCspEqualityAmoKey key{domains};
    if (window.have_after && !(window.after < key)) continue;
    if (std::binary_search(session.keys().begin(), session.keys().end(), key)) {
      ++result.committed_keys_skipped;
      continue;
    }
    if (result.offers.size() == window.maximum_offers) {
      result.status = NaryCspEqualityAmoOfferStatus::Ready;
      return result;
    }
    result.offers.push_back(std::move(selection));
    result.have_resume_after = true;
    result.resume_after = key;
  }
  result.complete = true;
  result.status = NaryCspEqualityAmoOfferStatus::Ready;
  return result;
}

} // namespace tactix
