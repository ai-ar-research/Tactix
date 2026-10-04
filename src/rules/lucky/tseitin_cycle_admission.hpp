// Exact, read-only admission from authenticated XOR premise groups to the pure
// Tseitin cycle planner.  No live clause, proof stream, or Kernel is mutated.
#pragma once

#include "rules/lucky/parity_premises.hpp"
#include "rules/lucky/tseitin_cycle_plan.hpp"

#include <cstdint>
#include <vector>

namespace tactix::tseitin_cycle {

struct LiveClause {
  uint64_t occurrence = 0;
  uint64_t proof = 0;
  std::vector<parity_premises::External> literals;
};

struct CertifiedGroup {
  // Stable producer identity.  Vertices are assigned by ascending identity,
  // independently of discovery order.
  uint64_t identity = 0;
  parity_premises::CertifiedEquation certified;
  // Aligned with certified.premises (ascending forbidden-assignment mask).
  std::vector<uint64_t> premise_proofs;
};

struct AdmissionRequest {
  std::vector<CertifiedGroup> groups;
  // Complete live-CNF snapshot.  Unclaimed clauses may not touch admitted
  // edge variables.
  std::vector<LiveClause> live_clauses;
  // Read-only exact-root query supplied by the live adapter.  Admission calls
  // it only for authenticated component edges, under its ordinary work meter.
  bool (*edge_unavailable)(void *, Edge) = nullptr;
  void *edge_unavailable_state = nullptr;
};

using InterruptPoll = bool (*)(void *);

struct AdmissionLimits {
  uint64_t work = 0;
  InterruptPoll interrupted = nullptr;
  void *interrupt_state = nullptr;
};

enum class AdmissionStatus : uint8_t {
  Admitted,
  Declined,
  ResourceExhausted,
  Interrupted,
  InvalidInput,
};

enum class AdmissionReason : uint8_t {
  None,
  EmptySystem,
  InvalidStableIdentity,
  DuplicateGroupIdentity,
  InvalidEquation,
  DuplicateEdgeInEquation,
  PartialPremiseGroup,
  InvalidLiveClause,
  DuplicateOccurrenceIdentity,
  DuplicateProofIdentity,
  PremiseIdentityMismatch,
  PremiseClauseMismatch,
  PremiseReused,
  ResidualEdgeTouch,
  EdgeNotInExactlyTwoEquations,
  EvenCharge,
  Disconnected,
  WorkLimit,
  Interrupted,
};

struct StablePremise {
  uint64_t occurrence = 0;
  uint64_t proof = 0;
};

struct EquationProvenance {
  uint64_t group_identity = 0;
  Vertex vertex = 0;
  std::vector<StablePremise> premises;
};

struct AdmissionResult {
  AdmissionStatus status = AdmissionStatus::Declined;
  AdmissionReason reason = AdmissionReason::None;
  uint64_t work = 0;
  // Populated atomically only for Admitted.
  Input input;
  std::vector<EquationProvenance> provenance;
  bool admitted() const { return status == AdmissionStatus::Admitted; }
};

// Authentication is transactional: every failure returns empty input and
// provenance, so a caller cannot accidentally use a partial graph.
AdmissionResult admit(const AdmissionRequest &, AdmissionLimits);

const char *admission_reason_name(AdmissionReason);

} // namespace tactix::tseitin_cycle
