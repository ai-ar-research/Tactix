#pragma once

#include "rules/semantic_extension/action_epilogue.hpp"
#include "rules/semantic_extension/nary_csp_action_limits.hpp"
#include "rules/semantic_extension/nary_csp_equality_live.hpp"
#include "rules/semantic_extension/nary_csp_relation_owner.hpp"

#include <cstdint>
#include <vector>

namespace tactix {

struct NaryCspEqualityAmoKey {
  std::vector<std::vector<int32_t>> domains;
  auto operator<=>(const NaryCspEqualityAmoKey &) const = default;
};

struct NaryCspEqualityAmoSelection {
  std::vector<std::vector<int32_t>> domains;
  bool operator==(const NaryCspEqualityAmoSelection &) const = default;
};

struct NaryCspEqualityAmoSessionAccess;
class NaryCspEqualityAmoSession
    : public KernelKeyedSession<NaryCspEqualityAmoKey,
                                NaryCspEqualityAmoSessionAccess> {};

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
    Kernel &, const NaryCspRelationOwnerSession &,
    const NaryCspEqualitySession &, NaryCspEqualityAmoSession &,
    const NaryCspEqualityAmoLimits &, const NaryCspEqualityAmoSelection &,
    StructuralJournal records);

} // namespace tactix
