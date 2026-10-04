// Controller-facing action boundary for certified GF(3) elimination.
#pragma once

#include "rules/semantic_extension/gf3_affine_live.hpp"
#include "strategy/semantic_variable.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tactix {

// Equation-pair identities are canonical within one retained owner.  Reversing
// the two parents therefore replays the same semantic action identity.
struct Gf3AffineActionKey {
  uint32_t first_equation = 0;
  uint32_t second_equation = 0;

  bool operator==(const Gf3AffineActionKey &) const = default;
  bool operator<(const Gf3AffineActionKey &other) const {
    return first_equation < other.first_equation ||
           (first_equation == other.first_equation &&
            second_equation < other.second_equation);
  }
};

// The controller owns one session for the lifetime of one Kernel.  The first
// committed action retains the exact pre-publication source image; later
// actions revalidate that same owner rather than recognizing their own output.
// Committed pair identities survive deletion of published clauses.
class Gf3AffineActionSession {
public:
  bool retained() const { return owner_ != nullptr; }
  bool belongs_to(const Kernel &kernel) const { return owner_ == &kernel; }
  size_t committed_keys() const { return keys_.size(); }
  const Gf3AffineOwnerSnapshot *snapshot() const {
    return retained() ? &snapshot_ : nullptr;
  }
  const std::vector<Gf3AffineActionKey> &keys() const { return keys_; }

private:
  friend struct Gf3AffineActionSessionAccess;
  const Kernel *owner_ = nullptr;
  Gf3AffineOwnerSnapshot snapshot_;
  std::vector<Gf3AffineActionKey> keys_;
};

// Every scan, derivation, proof delta, allocation and retained-session update
// has an explicit caller limit.  These are feature-local until the shared
// replay/configuration schema assigns the action a public rule id.
struct Gf3AffineActionLimits {
  gf3_affine::RecognitionBounds recognition;
  gf3_affine::PairEliminationBounds elimination;
  uint64_t maximum_recognition_work = 0;
  uint64_t maximum_planning_work = 0;
  uint64_t maximum_duplicate_scan_work = 0;
  uint64_t maximum_revalidation_work = 0;
  uint64_t maximum_proof_work = 0;
  uint64_t maximum_proof_additions = 0;
  uint64_t maximum_proof_literal_occurrences = 0;
  uint64_t maximum_arena_words = 0;
  uint64_t maximum_journal_bytes = 0;
  uint32_t maximum_external_extent = 0;
  uint64_t maximum_session_keys = 0;
  uint64_t maximum_session_work = 0;

  bool valid() const;
};

enum class Gf3AffineActionStatus : uint8_t {
  Invalid,
  Inapplicable,
  NoStructure,
  NoConsequence,
  ResourceExhausted,
  Interrupted,
  UnsupportedProof,
  Stale,
  AlreadyPublished,
  Applied,
};

struct Gf3AffineActionEffort {
  uint64_t source_clauses = 0;
  uint64_t source_literals = 0;
  uint64_t equations = 0;
  uint64_t owner_capture_work = 0;
  uint64_t recognition_work = 0;
  uint64_t planning_work = 0;
  uint64_t duplicate_scan_work = 0;
  uint64_t revalidation_work = 0;
  uint64_t proof_work = 0;
  uint64_t planned_additions = 0;
  uint64_t duplicate_additions = 0;
  uint64_t emitted_additions = 0;
  uint64_t proof_literal_occurrences = 0;
  uint64_t arena_words = 0;
  uint64_t journal_work = 0;
  uint64_t journal_bytes = 0;
  uint64_t session_work = 0;
};

struct Gf3AffineActionResult {
  Gf3AffineActionStatus status = Gf3AffineActionStatus::Invalid;
  ApplyResult result = ApplyResult::NoOp;
  uint64_t proposed_action_id = 0;
  uint64_t committed_action_id = 0;
  uint64_t semantic_id = 0;
  Gf3AffineActionKey key;
  uint64_t committed_keys = 0;
  bool owner_retained = false;
  bool semantic_candidate_ready = false;
  std::vector<strategy::SemanticVariable> semantic_candidates;
  Gf3AffineActionEffort effort;
  Gf3AffineLiveResult live;
};

// Formula/proof-state eligibility only.  Selection, effort and session state
// remain explicit inputs and this guard performs no recognition or mutation.
RuleEligibility gf3_affine_action_guard(const Kernel &);

// Apply one caller-selected pair at an assignment-free root.  Productive
// publication leaves propagation to its own controller action and returns
// neutral ExistingTheory candidates for the shared dispatcher to stage under
// the distinct GF(3) producer identity.  Every recoverable decline leaves the
// kernel, session, provenance stream and global action sequence unchanged.
Gf3AffineActionResult gf3_affine_action_apply(
    Kernel &, Gf3AffineActionSession &, const Gf3AffineActionLimits &,
    uint32_t first_equation, uint32_t second_equation);

} // namespace tactix
