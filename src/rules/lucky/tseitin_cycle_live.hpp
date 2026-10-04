// Cautious live-kernel adapter for one authenticated Tseitin-cycle X7 step.
//
// This layer deliberately registers no public action.  A caller first captures
// a complete bounded formula snapshot, selects certified XOR groups from that
// snapshot, and prepares one immutable transaction.  Publication re-captures
// the formula and applies the already authenticated tuple through the ordinary
// SubstitutionRedundancy implementation.
#pragma once

#include "kernel/kernel.hpp"
#include "rules/lucky/tseitin_cycle_transaction.hpp"
#include "rules/substitution_redundancy/substitution_redundancy.hpp"

#include <cstdint>
#include <vector>

namespace tactix::tseitin_cycle_live {

enum class SourceKind : uint8_t { Large, Binary };

struct Source {
  uint64_t occurrence = 0;
  uint64_t native_proof_id = 0;
  SourceKind kind = SourceKind::Large;
  Ref ref = INVALID_REF;
  Lit first = INVALID_LIT;
  Lit second = INVALID_LIT;
  std::vector<int32_t> clause;
  bool operator==(const Source &) const = default;
};

struct Snapshot {
  std::vector<Source> sources;
  std::vector<tseitin_cycle::LiveClause> clauses;
  std::vector<Var> internal_to_external;
  std::vector<Var> external_to_internal;
  // Exact {-1,0,1} root assignment image in internal-variable order.
  std::vector<int8_t> root_values;
  uint64_t arena_generation = 0;
  uint64_t touch_epoch = 0;
  uint64_t root_assignments = 0;
  uint64_t next_clause_id = 0;
  uint32_t nvars = 0;
  uint64_t literals = 0;
  uint64_t work = 0;
  bool operator==(const Snapshot &other) const {
    if (sources != other.sources ||
        internal_to_external != other.internal_to_external ||
        external_to_internal != other.external_to_internal ||
        root_values != other.root_values ||
        arena_generation != other.arena_generation ||
        touch_epoch != other.touch_epoch ||
        root_assignments != other.root_assignments ||
        next_clause_id != other.next_clause_id || nvars != other.nvars ||
        literals != other.literals || work != other.work ||
        clauses.size() != other.clauses.size())
      return false;
    for (size_t index = 0; index < clauses.size(); ++index) {
      const auto &left = clauses[index], &right = other.clauses[index];
      if (left.occurrence != right.occurrence || left.proof != right.proof ||
          left.literals != right.literals)
        return false;
    }
    return true;
  }
};

struct SnapshotLimits {
  uint64_t maximum_clauses = 0;
  uint64_t maximum_literals = 0;
  uint64_t maximum_work = 0;
  uint32_t maximum_variables = 0;
  tseitin_cycle::InterruptPoll interrupted = nullptr;
  void *interrupt_state = nullptr;
};

struct Request {
  SnapshotLimits snapshot;
  tseitin_cycle::AdmissionLimits admission;
  tseitin_cycle::Limits planning;
  tseitin_cycle::TransactionLimits transaction;
  uint64_t maximum_validation_ticks = 0;
};

enum class Status : uint8_t {
  Invalid,
  Inapplicable,
  ResourceExhausted,
  Interrupted,
  UnsupportedProof,
  Stale,
  Declined,
  Captured,
  Prepared,
  Applied,
};

enum class Reason : uint8_t {
  None,
  InvalidRequest,
  InvalidKernelPopulation,
  StateNotApplicable,
  SnapshotClauseLimit,
  SnapshotLiteralLimit,
  SnapshotWorkLimit,
  SnapshotVariableLimit,
  Interrupted,
  UnsupportedProof,
  SnapshotMismatch,
  AdmissionDeclined,
  PlanningDeclined,
  TransactionDeclined,
  ExternalMapping,
  AssignedPivot,
  ProofNamespace,
  SemanticPreflight,
  ApplicationMismatch,
};

struct Result {
  Status status = Status::Invalid;
  Reason reason = Reason::None;
  tseitin_cycle::AdmissionReason admission_reason =
      tseitin_cycle::AdmissionReason::None;
  tseitin_cycle::DeclineReason planning_reason =
      tseitin_cycle::DeclineReason::None;
  tseitin_cycle::TransactionReason transaction_reason =
      tseitin_cycle::TransactionReason::None;
  SubstitutionRedundancyApplicationStatus substitution_status =
      SubstitutionRedundancyApplicationStatus::Invalid;
  uint64_t snapshot_clauses = 0;
  uint64_t snapshot_literals = 0;
  uint64_t snapshot_work = 0;
  uint64_t admission_work = 0;
  uint64_t planning_work = 0;
  uint64_t transaction_work = 0;
  uint64_t replay_admission_work = 0;
  uint64_t replay_planning_work = 0;
  uint64_t replay_transaction_work = 0;
  uint64_t validation_ticks = 0;
  uint64_t witness_literals = 0;
  ApplyResult result = ApplyResult::NoOp;
};

class Prepared {
public:
  bool ready() const { return ready_; }
  const Snapshot &snapshot() const { return snapshot_; }
  const tseitin_cycle::ProofTransaction &transaction() const {
    return transaction_;
  }
  const SubstitutionRedundancyInstance &instance() const { return instance_; }

private:
  friend Result prepare(Kernel &, const Snapshot &,
                        const std::vector<tseitin_cycle::CertifiedGroup> &,
                        const Request &, Prepared &);
  friend Result apply(Kernel &, const Prepared &, const Request *);
  bool ready_ = false;
  Snapshot snapshot_;
  Request request_;
  tseitin_cycle::ProofTransaction transaction_;
  SubstitutionRedundancyInstance instance_;
  std::vector<tseitin_cycle::CertifiedGroup> groups_;
  Proof *proof_ = nullptr;
};

// Captures every live binary and large clause.  Occurrence identities are the
// one-based positions in Snapshot::sources/clauses after canonical ordering by
// native proof id.  Duplicate binary occurrences or proof identities decline.
Result capture(Kernel &, const SnapshotLimits &, Snapshot &);

// The selected groups must cite occurrence and proof identities from snapshot.
// Preparation re-captures before authentication and again after semantic
// preflight, so no mutable live-state claim is retained unchecked.
Result prepare(Kernel &, const Snapshot &,
               const std::vector<tseitin_cycle::CertifiedGroup> &,
               const Request &, Prepared &);

// Applies exactly one negative pivot unit.  Search propagation and discovery
// of a later cycle are intentionally separate controller actions.
// `current_callbacks` is optional, synchronous callback state for this apply
// only. Prepared never retains it; its numeric bounds remain the authenticated
// preparation bounds.
Result apply(Kernel &, const Prepared &, const Request *current_callbacks = nullptr);

const char *reason_name(Reason);

} // namespace tactix::tseitin_cycle_live
