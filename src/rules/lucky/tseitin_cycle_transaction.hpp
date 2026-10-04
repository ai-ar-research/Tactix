// Pure preflight for one admitted Tseitin cycle X7 transaction.
//
// External literals are retained until the live adapter maps them to current
// internal literals and invokes substitution_redundancy_apply_detailed.  This
// layer neither writes a proof nor mutates a Kernel.
#pragma once

#include "rules/lucky/tseitin_cycle_admission.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace tactix::tseitin_cycle {

struct ExternalX7Instance {
  std::vector<int32_t> clause;
  std::vector<int32_t> witness_true;
  std::vector<std::pair<int32_t, int32_t>> substitution;
  bool operator==(const ExternalX7Instance &) const = default;
};

enum class ProofEventKind : uint8_t {
  AddNegativeHelper,
  AddPositiveHelper,
  AddCandidateRup,
  DeletePositiveHelper,
  DeleteNegativeHelper,
};

struct ProofAccounting {
  uint64_t preflight_additions = 0;
  uint64_t preflight_deletions = 0;
  uint64_t temporary_helpers = 0;
  uint64_t proof_only_variables = 0;
  uint64_t committed_solver_clauses = 0;
  uint64_t witness_literals = 0;
  bool operator==(const ProofAccounting &) const = default;
};

struct TransactionPremise {
  uint64_t occurrence = 0;
  uint64_t proof = 0;
  bool operator==(const TransactionPremise &) const = default;
};

struct TransactionSource {
  uint64_t group_identity = 0;
  Vertex vertex = 0;
  std::vector<TransactionPremise> premises;
  bool operator==(const TransactionSource &) const = default;
};

struct TransactionIdentity {
  Edge pivot = 0;
  std::vector<Edge> cycle_edges;
  // Exact authenticated identities, in stable group order.
  std::vector<TransactionSource> sources;
  bool operator==(const TransactionIdentity &) const = default;
};

struct ProofTransaction {
  uint32_t version = 0;
  TransactionIdentity identity;
  ExternalX7Instance x7;
  ProofAccounting accounting;
  std::vector<ProofEventKind> events;
  // Exact accounting assumes the admitted edge remains unassigned.  The live
  // adapter must decline/rebuild if root propagation changed it.
  bool requires_unassigned_pivot = false;
  bool requires_live_x7_validation = false;
  bool requires_post_apply_replan = false;
  bool operator==(const ProofTransaction &) const = default;
};

struct TransactionLimits {
  uint64_t work = 0;
  uint64_t additions = 0;
  uint64_t deletions = 0;
  uint64_t helpers = 0;
  uint64_t witness_literals = 0;
  InterruptPoll interrupted = nullptr;
  void *interrupt_state = nullptr;
};

enum class TransactionStatus : uint8_t {
  Ready,
  Declined,
  ResourceExhausted,
  Interrupted,
  InvalidInput,
};

enum class TransactionReason : uint8_t {
  None,
  AdmissionNotCommitted,
  PlanNotCommitted,
  InvalidCandidate,
  InvalidSourceLedger,
  AdditionLimit,
  DeletionLimit,
  HelperLimit,
  WitnessLimit,
  WorkLimit,
  Interrupted,
  ReplayAdmissionFailed,
  ReplayPlanFailed,
  ReplayMismatch,
};

struct TransactionResult {
  TransactionStatus status = TransactionStatus::Declined;
  TransactionReason reason = TransactionReason::None;
  uint64_t work = 0;
  uint64_t admission_work = 0;
  uint64_t planning_work = 0;
  uint64_t transaction_work = 0;
  // Populated atomically only for Ready.
  ProofTransaction transaction;
  bool ready() const { return status == TransactionStatus::Ready; }
};

TransactionResult build_transaction(const AdmissionResult &, const Result &,
                                    TransactionLimits);

struct ReplayLimits {
  AdmissionLimits admission;
  Limits plan;
  TransactionLimits transaction;
};

// Independently repeats authentication, cycle selection and proof preflight,
// then exact-compares the canonical transaction including every source ID.
TransactionResult replay_transaction(const AdmissionRequest &,
                                     const ProofTransaction &, ReplayLimits);

const char *transaction_reason_name(TransactionReason);

} // namespace tactix::tseitin_cycle
