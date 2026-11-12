// Transactional live publication of a compact certified affine basis.
#pragma once

#include "rules/semantic_extension/affine_basis_plan.hpp"
#include "rules/semantic_extension/affine_live.hpp"
#include "rules/structural_text.hpp"


namespace tactix {

struct AffineBasisConsumedKey;

struct AffineBasisProjectionRecord {
  int32_t output = 0;
  int32_t first = 0;
  int32_t second = 0;
  bool operator==(const AffineBasisProjectionRecord &) const = default;
};

struct AffineBasisSourceIdentity {
  uint64_t native_clause_id = 0;
  std::vector<int32_t> clause;
  bool operator==(const AffineBasisSourceIdentity &) const = default;
  bool operator<(const AffineBasisSourceIdentity &other) const {
    if (native_clause_id != other.native_clause_id)
      return native_clause_id < other.native_clause_id;
    return clause < other.clause;
  }
};

struct AffineBasisEquationIdentity {
  std::vector<int32_t> variables;
  bool rhs = false;
  std::vector<AffineBasisSourceIdentity> premises;
  bool operator==(const AffineBasisEquationIdentity &) const = default;
  bool operator<(const AffineBasisEquationIdentity &other) const {
    if (variables != other.variables) return variables < other.variables;
    if (rhs != other.rhs) return rhs < other.rhs;
    return premises < other.premises;
  }
};

// Replay identity of one owner row, including its source equation ordinals.
struct AffineBasisRowSelection {
  uint64_t owner_version = 0;
  uint32_t elimination_profile = 1; // ascending external-variable pivots
  affine_semantic_extension::BasisRowKind kind =
      affine_semantic_extension::BasisRowKind::DirectSmall;
  std::vector<int32_t> variables;
  bool rhs = false;
  std::vector<size_t> source_equation_ordinals;
  // Source identities in ordinal order, binding the row to its owner.
  std::vector<AffineBasisEquationIdentity> source_equations;

  bool operator==(const AffineBasisRowSelection &) const = default;
  bool operator<(const AffineBasisRowSelection &other) const;
};

enum class AffineBasisLiveStatus : uint8_t {
  Invalid,
  Inapplicable,
  NoStructure,
  ResourceExhausted,
  Interrupted,
  UnsupportedProof,
  Stale,
  Prepared,
  Applied,
};

struct AffineBasisLiveRequest {
  affine_semantic_extension::Limits image;
  affine_semantic_extension::BasisPlanLimits basis;
  uint64_t maximum_snapshot_literals = 0;
  uint64_t maximum_snapshot_work = 0;
  uint64_t maximum_proof_additions = 0;
  uint64_t maximum_proof_deletions = 0;
  uint64_t maximum_journal_bytes = 0;
  uint32_t maximum_external_extent = 0;
  uint64_t action_id = 0;
  uint64_t semantic_id = 0;
  StructuralJournal records;
  // Required session-owned source view and selected row; only that row's
  // sources revalidate.
  const AffineLiveSnapshot *owner = nullptr;
  uint64_t owner_version = 0;
  const AffineBasisRowSelection *selection = nullptr;
  // Sorted keys already committed; planning removes their rows first.
  const std::vector<AffineBasisConsumedKey> *excluded_keys = nullptr;
  uint64_t maximum_exclusion_work = 0;
};

struct AffineBasisLiveResult {
  AffineBasisLiveStatus status = AffineBasisLiveStatus::Invalid;
  ApplyResult result = ApplyResult::NoOp;
  uint64_t snapshot_clauses = 0;
  uint64_t snapshot_literals = 0;
  uint64_t recognition_work = 0;
  uint64_t basis_work = 0;
  uint64_t equations = 0;
  uint64_t rows = 0;
  uint64_t definitions = 0;
  uint64_t preflight_additions = 0;
  uint64_t preflight_deletions = 0;
  uint64_t additions = 0;
  uint64_t deletions = 0;
  uint64_t published_clauses = 0;
  uint64_t journal_bytes = 0;
  uint64_t exclusion_work = 0;
  uint64_t excluded_rows = 0;
  uint32_t extent_growth = 0;
  bool contradiction = false;
  bool propagation_pending = false;
  // Variables of published rows and fresh XOR-definition extensions.
  std::vector<int32_t> existing_theory_variables;
  std::vector<int32_t> introduced_theory_variables;
  AffineBasisRowSelection selection;
};

// Identity of one published row; it outlives the clauses.
struct AffineBasisConsumedKey {
  std::vector<int32_t> variables;
  bool rhs = false;
  std::vector<AffineBasisEquationIdentity> source_equations;
  bool operator==(const AffineBasisConsumedKey &) const = default;
  bool operator<(const AffineBasisConsumedKey &other) const {
    if (variables != other.variables) return variables < other.variables;
    if (rhs != other.rhs) return rhs < other.rhs;
    return source_equations < other.source_equations;
  }
};

class AffineBasisLiveTransaction {
public:
  bool ready() const { return ready_; }
  uint64_t additions() const { return additions_; }
  uint64_t deletions() const { return deletions_; }

  struct Event {
    enum class Kind : uint8_t { Rat, Rup, Delete } kind = Kind::Rup;
    std::vector<int32_t> clause;
    uint64_t id = 0;
    uint64_t deleted_id = 0;
    bool install = false;
    bool definition = false;
  };

  const std::vector<Event> &proof_events() const { return events_; }
  const std::vector<AffineBasisConsumedKey> &consumed_keys() const {
    return consumed_keys_;
  }

private:
  friend AffineBasisLiveResult prepare_affine_basis_live(
      Kernel &, const AffineBasisLiveRequest &, AffineBasisLiveTransaction &);
  friend AffineBasisLiveResult publish_affine_basis_live(
      Kernel &, const AffineBasisLiveRequest &,
      const AffineBasisLiveTransaction &);
  friend bool make_affine_basis_journal(
      const AffineBasisLiveRequest &,
      const affine_semantic_extension::Image &,
      const affine_semantic_extension::BasisPlan &,
      AffineBasisLiveTransaction &);

  bool ready_ = false;
  uint64_t action_id_ = 0;
  uint64_t semantic_id_ = 0;
  uint64_t starting_clause_id_ = 0;
  uint64_t arena_generation_ = 0;
  uint32_t starting_nvars_ = 0;
  uint32_t starting_extent_ = 0;
  uint32_t extent_growth_ = 0;
  uint64_t additions_ = 0;
  uint64_t deletions_ = 0;
  uint64_t arena_words_ = 0;
  uint64_t published_ = 0;
  bool contradiction_ = false;
  AffineLiveSnapshot snapshot_;
  std::vector<uint64_t> source_occurrences_;
  std::vector<Event> events_;
  JournalText journal_;
  std::vector<int32_t> existing_;
  std::vector<int32_t> introduced_;
  std::vector<AffineBasisProjectionRecord> projection_;
  std::vector<int32_t> proof_only_helpers_;
  std::vector<AffineBasisConsumedKey> consumed_keys_;
  AffineBasisLiveResult prepared_;
};

// The durable identity of a planned row, shared by offerer and publisher.
bool affine_basis_consumed_key(
    const affine_semantic_extension::Image &, const AffineLiveSnapshot &,
    const affine_semantic_extension::BasisRow &, AffineBasisConsumedKey &);
// Charges the session work of reading one retained key, in key order; false
// when `maximum` is exhausted.
bool affine_basis_key_work(const AffineBasisConsumedKey &, uint64_t maximum,
                           uint64_t &work);
bool affine_basis_row_selection(
    uint64_t owner_version, const affine_semantic_extension::Image &,
    const AffineLiveSnapshot &, const affine_semantic_extension::BasisRow &,
    AffineBasisRowSelection &);

// Mutation-free: validate the owner snapshot and precompute every proof event,
// id, arena word and journal byte of publishing the selected row.
AffineBasisLiveResult prepare_affine_basis_live(
    Kernel &, const AffineBasisLiveRequest &, AffineBasisLiveTransaction &);

// Revalidate, then commit the bounded transaction; propagation is left over.
AffineBasisLiveResult publish_affine_basis_live(
    Kernel &, const AffineBasisLiveRequest &,
    const AffineBasisLiveTransaction &);

} // namespace tactix
