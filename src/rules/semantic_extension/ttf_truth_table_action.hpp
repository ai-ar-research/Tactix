// Controller-facing action boundary for the certified TTF parity frontier.
#pragma once

#include "rules/semantic_extension/ttf_truth_table_live.hpp"
#include "strategy/semantic_variable.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tactix {

// The current certified planner exposes exactly one deterministic member: the
// first odd coordinate whose forced atoms are all nonzero.  Keeping this typed
// selector in the retained configuration gives replay a stable extension point
// without pretending that arbitrary-coordinate planning is implemented.
enum class TtfTruthTableFrontierSelection : uint8_t { FirstEligible };

struct TtfTruthTableActionLimits {
  ttf_truth_table::Limits theory;
  z3_amo_semantic::Limits amo;
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
  uint32_t maximum_amo_components = 0;
  uint64_t maximum_amo_sources = 0;
  uint64_t maximum_amo_source_literals = 0;
  uint64_t maximum_session_commits = 0;
  uint64_t maximum_session_work = 0;

  bool valid() const;
  bool operator==(const TtfTruthTableActionLimits &) const;
};

struct TtfTruthTableActionParameters {
  TtfTruthTableFrontierSelection selection =
      TtfTruthTableFrontierSelection::FirstEligible;
  TtfTruthTableActionLimits limits;

  bool valid() const;
  bool operator==(const TtfTruthTableActionParameters &) const = default;
};

struct TtfTruthTableActionIdentity {
  uint64_t action_id = 0;
  uint64_t semantic_id = 0;
  uint64_t source_generation = 0;
  uint32_t coordinate = 0;

  bool operator==(const TtfTruthTableActionIdentity &) const = default;
};

enum class TtfTruthTableSessionStatus : uint8_t {
  Configured,
  Invalid,
  AlreadyConfigured,
  ResourceExhausted,
};

struct TtfTruthTableSessionResult {
  TtfTruthTableSessionStatus status = TtfTruthTableSessionStatus::Invalid;
  uint64_t source_clauses = 0;
  uint64_t source_literals = 0;
  uint64_t amo_components = 0;
  uint64_t amo_sources = 0;
  uint64_t amo_source_literals = 0;
  uint64_t session_work = 0;
};

struct TtfTruthTableActionSessionAccess;

// One controller-owned session is bound to one Kernel.  It owns the exact
// checker-formula source and AMO witnesses rather than borrowing parser or
// detector storage.  Its immutable source is also the safely recapturable
// refresh provider used by the live transaction.  The embedded live session
// retains exact duplicate keys even if published frontier clauses are deleted.
class TtfTruthTableActionSession {
public:
  bool configured() const { return owner_ != nullptr; }
  bool belongs_to(const Kernel &kernel) const { return owner_ == &kernel; }
  size_t committed_keys() const { return live_.committed_keys(); }
  size_t committed_identities() const { return identities_.size(); }
  const TtfTruthTableLiveSnapshot *snapshot() const {
    return configured() ? &snapshot_ : nullptr;
  }
  const TtfTruthTableAmoWitness *amo() const {
    return configured() ? &amo_ : nullptr;
  }
  const TtfTruthTableActionParameters *parameters() const {
    return configured() ? &parameters_ : nullptr;
  }
  const std::vector<TtfTruthTableActionIdentity> &identities() const {
    return identities_;
  }

private:
  friend struct TtfTruthTableActionSessionAccess;
  const Kernel *owner_ = nullptr;
  TtfTruthTableLiveSnapshot snapshot_;
  TtfTruthTableAmoWitness amo_;
  TtfTruthTableActionParameters parameters_;
  TtfTruthTableLiveSession live_;
  std::vector<TtfTruthTableActionIdentity> identities_;
};

// Transactionally take ownership of the exact source, witnesses and replay
// parameters.  A rejected configuration leaves `session` unchanged.
TtfTruthTableSessionResult ttf_truth_table_action_configure(
    const Kernel &, TtfTruthTableActionSession &,
    const TtfTruthTableLiveSnapshot &, const TtfTruthTableAmoWitness &,
    const TtfTruthTableActionParameters &);

enum class TtfTruthTableActionStatus : uint8_t {
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

struct TtfTruthTableActionResult {
  TtfTruthTableActionStatus status = TtfTruthTableActionStatus::Invalid;
  ApplyResult result = ApplyResult::NoOp;
  uint64_t proposed_action_id = 0;
  uint64_t committed_action_id = 0;
  uint64_t semantic_id = 0;
  uint64_t session_work = 0;
  uint64_t committed_keys = 0;
  uint64_t committed_identities = 0;
  bool advice_ready = false;
  bool identity_projection = true;
  bool propagation_pending = false;
  bool complete_refutation = false;
  std::vector<strategy::SemanticVariable> existing_variable_advice;
  TtfTruthTableLiveResult live;
};

// Formula/proof-state eligibility only.  Recognition, selection, source data
// and all effort limits remain explicit retained session inputs.
RuleEligibility ttf_truth_table_action_guard(const Kernel &);

// Apply the retained FirstEligible frontier at an assignment-free propagated
// root.  Publication, proof output and provenance are atomic; propagation and
// application of returned ExistingTheory advice are separate controller acts.
TtfTruthTableActionResult ttf_truth_table_action_apply(
    Kernel &, TtfTruthTableActionSession &);

} // namespace tactix
