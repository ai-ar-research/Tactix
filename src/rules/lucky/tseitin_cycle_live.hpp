// Live adapter for one authenticated Tseitin-cycle X7 step (no public action):
// capture a bounded snapshot, select certified XOR groups, prepare an immutable
// transaction; apply re-captures and goes through SubstitutionRedundancy.
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
  bool operator==(const Snapshot &) const = default;
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

// Interrupt state that reports kernel termination before a nested poll.
struct KernelPoll {
  Kernel *kernel = nullptr;
  tseitin_cycle::InterruptPoll nested = nullptr;
  void *nested_state = nullptr;
};

inline bool kernel_poll(void *opaque) {
  auto &context = *static_cast<KernelPoll *>(opaque);
  return (context.kernel && kernel_terminating(*context.kernel)) ||
         (context.nested && context.nested(context.nested_state));
}

// A copy of a stage's limits whose callback also observes kernel termination.
// `poll` must outlive every use of the returned limits.
template <class Limits>
Limits with_kernel_poll(Kernel &kernel, const Limits &source,
                        KernelPoll &poll) {
  poll = {&kernel, source.interrupted, source.interrupt_state};
  auto result = source;
  result.interrupted = kernel_poll;
  result.interrupt_state = &poll;
  return result;
}

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

// Captures every live clause; occurrences are one-based positions after
// ordering by proof id. Duplicate binaries or proof ids decline.
Result capture(Kernel &, const SnapshotLimits &, Snapshot &);

// Groups cite snapshot identities; preparation re-captures before and after
// semantic preflight.
Result prepare(Kernel &, const Snapshot &,
               const std::vector<tseitin_cycle::CertifiedGroup> &,
               const Request &, Prepared &);

// Applies exactly one negative pivot unit. `current_callbacks` is used for
// this apply only; bounds remain those of the preparation.
Result apply(Kernel &, const Prepared &, const Request *current_callbacks = nullptr);

} // namespace tactix::tseitin_cycle_live
