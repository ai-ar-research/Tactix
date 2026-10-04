// Transactional live publication of a compact certified affine basis.
#pragma once

#include "rules/semantic_extension/affine_basis_plan.hpp"
#include "rules/semantic_extension/affine_live.hpp"

#include <cstdio>
#include <string>

namespace tactix {

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
  FILE *records = nullptr;
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
  uint32_t extent_growth = 0;
  bool contradiction = false;
  bool propagation_pending = false;
  // Existing variables participating in published basis rows and the fresh
  // extension variables allocated for exact XOR definitions.  The adapter
  // reports these only; it never changes phase, VMTF, EVSIDS, or scheduling.
  std::vector<int32_t> existing_theory_variables;
  std::vector<int32_t> introduced_theory_variables;
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

// Exact semantic and proof-source identity of one row actually published by a
// transaction.  Sessions retain these keys after clauses are later removed.
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
  uint32_t extent_growth() const { return extent_growth_; }

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
  std::string journal_;
  std::vector<int32_t> existing_;
  std::vector<int32_t> introduced_;
  std::vector<AffineBasisConsumedKey> consumed_keys_;
  AffineBasisLiveResult prepared_;
};

// Pure with respect to the kernel, proof stream, and record stream.  It owns a
// complete source snapshot and precomputes every proof event, identifier,
// allocation, arena word, and journal byte needed by publication.
AffineBasisLiveResult prepare_affine_basis_live(
    Kernel &, const AffineBasisLiveRequest &, AffineBasisLiveTransaction &);

// Revalidate the exact snapshot and transaction identity, then commit the
// already bounded transaction.  Publication leaves propagation to the caller.
AffineBasisLiveResult publish_affine_basis_live(
    Kernel &, const AffineBasisLiveRequest &,
    const AffineBasisLiveTransaction &);

AffineBasisLiveResult apply_affine_basis_live(
    Kernel &, const AffineBasisLiveRequest &);

} // namespace tactix
