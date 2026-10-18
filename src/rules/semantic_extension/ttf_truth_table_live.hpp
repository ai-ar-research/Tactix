#pragma once

#include "rules/registry.hpp"
#include "rules/semantic_extension/ttf_truth_table.hpp"
#include "rules/semantic_extension/z3_amo_recognition.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace tactix {

struct TtfTruthTableLiveSource {
  uint64_t native_proof_id = 0;
  std::vector<int32_t> literals;
  bool operator==(const TtfTruthTableLiveSource &) const = default;
  bool operator<(const TtfTruthTableLiveSource &other) const {
    if (native_proof_id != other.native_proof_id)
      return native_proof_id < other.native_proof_id;
    return literals < other.literals;
  }
};

// Retained checker-formula view. This is deliberately supplied by an owner:
// import-time root units may simplify or discard live kernel clauses before an
// inprocessing action can inspect them.
struct TtfTruthTableLiveSnapshot {
  uint64_t generation = 0;
  uint32_t external_variables = 0;
  std::vector<TtfTruthTableLiveSource> clauses;
  bool operator==(const TtfTruthTableLiveSnapshot &) const = default;
};

using TtfTruthTableRefresh = bool (*)(
    void *context, TtfTruthTableLiveSnapshot &current);

struct TtfTruthTableAmoComponent {
  std::vector<int32_t> leaves;
  std::vector<TtfTruthTableLiveSource> sources;
};

struct TtfTruthTableAmoWitness {
  std::vector<TtfTruthTableAmoComponent> components;
};

struct TtfTruthTableLiveSessionAccess;

class TtfTruthTableLiveSession {
public:
  size_t committed_keys() const { return committed_keys_.size(); }

private:
  friend struct TtfTruthTableLiveSessionAccess;
  struct Key {
    std::vector<std::vector<int32_t>> coordinate_stars;
    std::vector<TtfTruthTableLiveSource> proof_sources;
    bool operator==(const Key &) const = default;
    bool operator<(const Key &other) const {
      if (coordinate_stars != other.coordinate_stars)
        return coordinate_stars < other.coordinate_stars;
      return proof_sources < other.proof_sources;
    }
  };
  std::vector<Key> committed_keys_;
};

struct TtfTruthTableLiveRequest {
  const TtfTruthTableLiveSnapshot *owner = nullptr;
  TtfTruthTableRefresh refresh = nullptr;
  void *refresh_context = nullptr;
  const TtfTruthTableAmoWitness *amo = nullptr;
  TtfTruthTableLiveSession *session = nullptr;
  ttf_truth_table::Limits theory_limits{};
  z3_amo_semantic::Limits amo_limits{};
  uint64_t maximum_source_clauses = 0;
  uint64_t maximum_source_literals = 0;
  uint64_t maximum_snapshot_work = 0;
  uint64_t maximum_recognition_work = 0;
  uint64_t maximum_proof_work = 0;
  uint64_t maximum_proof_additions = 0;
  uint64_t maximum_proof_literal_occurrences = 0;
  uint64_t maximum_formula_additions = 0;
  uint64_t maximum_formula_literal_occurrences = 0;
  uint64_t maximum_arena_words = 0;
  uint64_t maximum_journal_bytes = 0;
  uint64_t action_id = 0;
  uint64_t semantic_id = 0;
  FILE *records = nullptr;
};

enum class TtfTruthTableLiveStatus : uint8_t {
  Applied,
  Invalid,
  Inapplicable,
  NoStructure,
  NoParityFrontier,
  MissingAmoWitness,
  AlreadyPublished,
  ResourceExhausted,
  Interrupted,
  UnsupportedProof,
  Stale,
};

struct TtfTruthTableLiveResult {
  TtfTruthTableLiveStatus status = TtfTruthTableLiveStatus::Invalid;
  ApplyResult result = ApplyResult::NoOp;
  uint64_t source_clauses = 0;
  uint64_t source_literals = 0;
  uint64_t snapshot_work = 0;
  uint64_t recognition_work = 0;
  uint64_t plan_work = 0;
  uint64_t revalidation_work = 0;
  uint64_t proof_work = 0;
  uint64_t additions = 0;
  uint64_t literal_occurrences = 0;
  uint64_t journal_bytes = 0;
  uint32_t coordinate = 0;
  bool propagation_pending = false;
  bool complete_refutation = false;
  std::vector<uint64_t> published_clause_ids;
  // Existing, currently unassigned B variables appearing in the frontier.
  // The caller may stage these as ExistingTheory advice; this adapter does not
  // alter VMTF, EVSIDS, phase, or any kernel heuristic state.
  std::vector<int32_t> semantic_candidate_variables;
};

// Recognize, revalidate, and atomically publish one compact odd-coordinate
// frontier. DSR/DRAT/no-proof and streaming PBP are supported; LRAT and
// terminal-PBP decline before mutation.
// Propagation is deliberately left to a later controller action.
TtfTruthTableLiveResult ttf_truth_table_live_apply(
    Kernel &, const TtfTruthTableLiveRequest &);

} // namespace tactix
