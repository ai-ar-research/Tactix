#pragma once

#include "rules/semantic_extension/gf3_gaussian_bridge.hpp"

#include <cstdint>
#include <functional>
#include <vector>

namespace tactix::gf3_gaussian_certificate {

// Witness-free RUP additions only; no new variables.
enum class EventRole : uint8_t { Auxiliary, Consequence, Conflict };

struct Event {
  EventRole role = EventRole::Auxiliary;
  uint32_t consequence = 0;
  std::vector<int32_t> clause;

  friend bool operator==(const Event &, const Event &) = default;
};

enum class ConsequenceKind : uint8_t { UnitEquation, Conflict };

struct AuthenticatedSource {
  gf3_gaussian::ProvenanceTerm combination;
  // Zero-based index into the bridge authentication table.
  uint32_t authentication_index = 0;

  friend bool operator==(const AuthenticatedSource &,
                         const AuthenticatedSource &) = default;
};

struct Consequence {
  ConsequenceKind kind = ConsequenceKind::UnitEquation;
  uint32_t derivation = 0;
  std::vector<gf3_gaussian::Term> row;
  uint8_t rhs = 0;
  // The nonzero source combination, with premise authentication indices.
  std::vector<AuthenticatedSource> sources;
  // Solver clauses (auxiliaries are proof-only): a unit equation's two
  // forbidden-value units and selected-value unit, or the empty clause.
  std::vector<std::vector<int32_t>> clauses;
  uint64_t first_event = 0;
  uint64_t event_count = 0;

  friend bool operator==(const Consequence &, const Consequence &) = default;
};

struct Limits {
  gf3_gaussian_bridge::Limits bridge;
  uint64_t maximum_work = 0;
  uint64_t maximum_events = 0;
  uint64_t maximum_event_literals = 0;
  uint64_t maximum_active_clauses = 0;
  // AuthenticatedSource records across consequences, repeats included.
  uint64_t maximum_retained_source_references = 0;
  uint32_t maximum_event_width = 0;
  uint32_t maximum_branch_domains = 0;
  uint32_t maximum_consequences = 0;
  // Validates this level's bounds; callers validate `bridge` separately.
  bool valid() const {
    return maximum_work && maximum_events && maximum_event_literals &&
           maximum_active_clauses && maximum_retained_source_references &&
           maximum_event_width && maximum_branch_domains &&
           maximum_consequences;
  }
};

enum class LimitKind : uint8_t {
  None,
  Bridge,
  Work,
  Events,
  EventLiterals,
  ActiveClauses,
  RetainedSourceReferences,
  EventWidth,
  BranchDomains,
  Consequences,
};

enum class Status : uint8_t {
  Complete,
  NoSupportedOutcome,
  InvalidRequest,
  InvalidTheory,
  InvalidBridge,
  InvalidCertificate,
  Uncertifiable,
  LimitExceeded,
  Interrupted,
};

struct Statistics {
  uint64_t bridge_work = 0;
  uint64_t work = 0;
  uint64_t rup_checks = 0;
  uint64_t propagated_literals = 0;
  uint64_t events = 0;
  uint64_t event_literals = 0;
  uint64_t retained_source_references = 0;
  uint64_t supported_outcomes = 0;
  uint64_t unsupported_outcomes = 0;
  uint32_t maximum_event_width = 0;

  friend bool operator==(const Statistics &, const Statistics &) = default;
};

struct Plan {
  uint32_t version = 2;
  std::vector<Consequence> consequences;
  std::vector<Event> events;
  Statistics statistics;

  friend bool operator==(const Plan &, const Plan &) = default;
};

struct Result {
  Status status = Status::InvalidRequest;
  LimitKind limit = LimitKind::None;
  gf3_gaussian_bridge::LimitKind bridge_limit =
      gf3_gaussian_bridge::LimitKind::None;
  Statistics consumed;
  // Published only on Complete.  Every decline is atomic.
  Plan plan;
};

using Charge = std::function<bool(uint64_t)>;

// Validate the theory and derivation graph, then certify every unit row or
// zero-row conflict by a bounded RUP decision tree; non-unit rows are counted
// and declined. Touches no solver state.
Result build(const gf3_affine::Theory &, const gf3_gaussian_bridge::Plan &,
             const Limits &, Charge);

// Rebuild from theory and bridge and compare every field.
Result validate(const gf3_affine::Theory &,
                const gf3_gaussian_bridge::Plan &, const Plan &,
                const Limits &, Charge);

} // namespace tactix::gf3_gaussian_certificate
