#pragma once

#include "rules/semantic_extension/nary_csp_equality_live.hpp"
#include "rules/semantic_extension/nary_csp_prefix_action.hpp"

#include <cstdint>
#include <vector>

namespace tactix {

struct NaryCspEqualityAmoKey {
  std::vector<std::vector<int32_t>> domains;
  bool operator==(const NaryCspEqualityAmoKey &) const = default;
  bool operator<(const NaryCspEqualityAmoKey &other) const {
    return domains < other.domains;
  }
};

struct NaryCspEqualityAmoSelection {
  std::vector<std::vector<int32_t>> domains;
  bool operator==(const NaryCspEqualityAmoSelection &) const = default;
};

struct NaryCspEqualityAmoLimits {
  uint64_t maximum_source_clauses = 0;
  uint64_t maximum_source_literals = 0;
  uint64_t maximum_recognition_work = 0;
  uint32_t maximum_domain_values = 0;
  uint64_t maximum_forbidden_tuples = 0;
  uint64_t maximum_proof_work = 0;
  uint64_t maximum_proof_additions = 0;
  uint64_t maximum_proof_literal_occurrences = 0;
  uint64_t maximum_journal_bytes = 0;
  uint32_t maximum_external_extent = 0;
  uint64_t maximum_session_keys = 0;
  bool valid() const;
};

class NaryCspEqualityAmoSession {
public:
  bool retained() const { return owner_ != nullptr; }
  bool belongs_to(const Kernel &kernel) const { return owner_ == &kernel; }
  const std::vector<NaryCspEqualityAmoKey> &keys() const { return keys_; }

private:
  friend struct NaryCspEqualityAmoSessionAccess;
  const Kernel *owner_ = nullptr;
  std::vector<NaryCspEqualityAmoKey> keys_;
};

enum class NaryCspEqualityAmoStatus : uint8_t {
  Invalid,
  Inapplicable,
  NoStructure,
  MissingEquality,
  Stale,
  Duplicate,
  ResourceExhausted,
  Interrupted,
  UnsupportedProof,
  Applied,
};

struct NaryCspEqualityAmoResult {
  NaryCspEqualityAmoStatus status = NaryCspEqualityAmoStatus::Invalid;
  ApplyResult result = ApplyResult::NoOp;
  uint64_t proposed_action_id = 0;
  uint64_t committed_action_id = 0;
  uint64_t recognition_work = 0;
  uint64_t proof_work = 0;
  uint64_t proof_additions = 0;
  uint64_t proof_deletions = 0;
  uint64_t proof_literal_occurrences = 0;
  uint64_t journal_bytes = 0;
  uint64_t first_published_clause_id = 0;
  uint64_t last_published_clause_id = 0;
  uint32_t equality_variables = 0;
  uint32_t emitted_binary_clauses = 0;
  bool propagation_pending = false;
  std::vector<std::vector<int32_t>> published_consequences;
};

RuleEligibility nary_csp_equality_amo_guard(const Kernel &);
bool nary_csp_equality_amo_selection_valid(
    const NaryCspEqualityAmoSelection &);
bool nary_csp_equality_amo_selection_within_limits(
    const NaryCspEqualityAmoSelection &, const NaryCspEqualityAmoLimits &);

NaryCspEqualityAmoResult nary_csp_equality_amo_apply(
    Kernel &, const NaryCspPrefixActionSession &,
    const NaryCspEqualitySession &, NaryCspEqualityAmoSession &,
    const NaryCspEqualityAmoLimits &, const NaryCspEqualityAmoSelection &,
    FILE *records);

struct NaryCspEqualityAmoOfferWindow {
  uint64_t maximum_owner_candidates = 0;
  uint64_t maximum_offers = 0;
  uint64_t maximum_enumeration_work = 0;
  bool have_after = false;
  NaryCspEqualityAmoKey after;
  bool valid() const;
};

enum class NaryCspEqualityAmoOfferStatus : uint8_t {
  Invalid,
  Inapplicable,
  ResourceExhausted,
  Interrupted,
  Stale,
  Ready,
};

struct NaryCspEqualityAmoOfferResult {
  NaryCspEqualityAmoOfferStatus status =
      NaryCspEqualityAmoOfferStatus::Invalid;
  bool complete = false;
  bool have_resume_after = false;
  NaryCspEqualityAmoKey resume_after;
  uint64_t retained_owners = 0;
  uint64_t owner_candidates = 0;
  uint64_t missing_equalities = 0;
  uint64_t committed_keys_skipped = 0;
  uint64_t enumeration_work = 0;
  std::vector<NaryCspEqualityAmoSelection> offers;
};

NaryCspEqualityAmoOfferResult nary_csp_equality_amo_offer(
    Kernel &, const NaryCspPrefixActionSession &,
    const NaryCspEqualitySession &, const NaryCspEqualityAmoSession &,
    const NaryCspEqualityAmoLimits &,
    const NaryCspEqualityAmoOfferWindow &);

} // namespace tactix
