#pragma once

#include "rules/registry.hpp"
#include "rules/semantic_extension/gf3_affine_live.hpp"
#include "rules/semantic_extension/gf3_general_row.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace tactix {

struct Gf3GeneralRowOwner {
  Gf3AffineOwnerSnapshot affine;
  gf3_gaussian_bridge::Plan bridge;
};

struct Gf3GeneralRowLiveLimits {
  gf3_affine::RecognitionBounds recognition;
  gf3_general_row::Limits planner;
  uint64_t maximum_capture_work = 0;
  uint64_t maximum_duplicate_scan_work = 0;
  uint64_t maximum_revalidation_work = 0;
  uint64_t maximum_proof_additions = 0;
  uint64_t maximum_proof_literal_occurrences = 0;
  uint64_t maximum_arena_words = 0;
  uint64_t maximum_journal_bytes = 0;
  uint64_t maximum_identity_bytes = 0;
  uint32_t maximum_external_extent = 0;

  bool valid() const;
};

struct Gf3GeneralRowActionSelection {
  uint32_t outcome = 0;
  uint64_t tuple = 0;
  std::string identity;

  friend bool operator==(const Gf3GeneralRowActionSelection &,
                         const Gf3GeneralRowActionSelection &) = default;
  bool operator<(const Gf3GeneralRowActionSelection &other) const {
    if (outcome != other.outcome) return outcome < other.outcome;
    if (tuple != other.tuple) return tuple < other.tuple;
    return identity < other.identity;
  }
};

enum class Gf3GeneralRowLiveStatus : uint8_t {
  Invalid,
  Inapplicable,
  NoStructure,
  NoConsequence,
  Duplicate,
  ResourceExhausted,
  Interrupted,
  UnsupportedProof,
  Stale,
  Ready,
  Applied,
};

struct Gf3GeneralRowLiveEffort {
  uint64_t source_clauses = 0;
  uint64_t source_literals = 0;
  uint64_t capture_work = 0;
  uint64_t bridge_work = 0;
  uint64_t planning_work = 0;
  uint64_t validation_work = 0;
  uint64_t duplicate_scan_work = 0;
  uint64_t revalidation_work = 0;
  uint64_t proof_additions = 0;
  uint64_t proof_literal_occurrences = 0;
  uint64_t arena_words = 0;
  uint64_t journal_bytes = 0;
  uint64_t identity_bytes = 0;
};

struct Gf3GeneralRowLiveResult {
  Gf3GeneralRowLiveStatus status = Gf3GeneralRowLiveStatus::Invalid;
  ApplyResult result = ApplyResult::NoOp;
  Gf3GeneralRowLiveEffort effort;
  Gf3GeneralRowOwner owner;
  gf3_general_row::Plan plan;
  std::string identity;
  std::vector<uint64_t> proof_clause_ids;
  uint64_t installed_clause_id = 0;
  bool propagation_pending = false;
  std::vector<int32_t> semantic_variables;
};

struct Gf3GeneralRowLiveRequest {
  Gf3GeneralRowLiveLimits limits;
  gf3_general_row::Request selection;
  std::string selected_identity;
  uint64_t action_id = 0;
  uint64_t semantic_id = 0;
  FILE *records = nullptr;
  const Gf3GeneralRowOwner *retained_owner = nullptr;
};

Gf3GeneralRowLiveStatus gf3_general_row_live_capture(
    Kernel &, const Gf3GeneralRowLiveLimits &, Gf3GeneralRowOwner &,
    Gf3GeneralRowLiveEffort &);

Gf3GeneralRowLiveStatus gf3_general_row_live_validate_owner(
    Kernel &, const Gf3GeneralRowLiveLimits &, const Gf3GeneralRowOwner &,
    Gf3GeneralRowLiveEffort &);

// Canonical identity binds the selected outcome/tuple, row, source
// combination, exact authentication references and source clauses.
bool gf3_general_row_live_identity(const Gf3GeneralRowOwner &,
                                   const gf3_general_row::Request &,
                                   uint64_t maximum_bytes,
                                   std::string &identity);

// Mutation-free full admission, including proof construction, duplicate scan
// and exact owner revalidation. Apply repeats this preflight and commits once.
Gf3GeneralRowLiveResult gf3_general_row_live_inspect(
    Kernel &, const Gf3GeneralRowLiveRequest &);
Gf3GeneralRowLiveResult gf3_general_row_live_apply(
    Kernel &, const Gf3GeneralRowLiveRequest &);

} // namespace tactix
