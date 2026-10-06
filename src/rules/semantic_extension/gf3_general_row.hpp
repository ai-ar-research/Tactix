#pragma once

#include "rules/semantic_extension/gf3_gaussian_bridge.hpp"

#include <cstdint>
#include <functional>
#include <vector>

namespace tactix::gf3_general_row {

// One public action can eventually name exactly one Boolean clause from one
// non-unit Gaussian row.  The row/tuple pair is the semantic duplicate key;
// the derivation and owner authentication remain separate replay evidence.
struct Request {
  uint32_t outcome = 0;
  uint64_t tuple = 0;

  friend bool operator==(const Request &, const Request &) = default;
};

struct DuplicateKey {
  std::vector<gf3_gaussian::Term> row;
  uint8_t rhs = 0;
  uint64_t tuple = 0;

  friend bool operator==(const DuplicateKey &, const DuplicateKey &) = default;
};

struct AuthenticatedSource {
  gf3_gaussian::ProvenanceTerm combination;
  uint32_t authentication_index = 0;

  friend bool operator==(const AuthenticatedSource &,
                         const AuthenticatedSource &) = default;
};

enum class EventRole : uint8_t { Auxiliary, Consequence };

struct Event {
  EventRole role = EventRole::Auxiliary;
  std::vector<int32_t> clause;

  friend bool operator==(const Event &, const Event &) = default;
};

struct ProjectionObligation {
  std::vector<int32_t> extension_variables;
  bool external_extent_unchanged = true;
  bool reconstruction_witness_required = false;

  friend bool operator==(const ProjectionObligation &,
                         const ProjectionObligation &) = default;
};

struct Limits {
  gf3_gaussian_bridge::Limits bridge;
  uint64_t maximum_work = 0;
  uint64_t maximum_events = 0;
  uint64_t maximum_event_literals = 0;
  uint64_t maximum_active_clauses = 0;
  uint64_t maximum_retained_source_references = 0;
  uint64_t maximum_row_tuples = 0;
  uint32_t maximum_event_width = 0;
  uint32_t maximum_branch_domains = 0;
  uint32_t maximum_row_width = 0;
};

enum class LimitKind : uint8_t {
  None,
  Bridge,
  Work,
  Events,
  EventLiterals,
  ActiveClauses,
  RetainedSourceReferences,
  RowTuples,
  EventWidth,
  BranchDomains,
  RowWidth,
};

enum class Status : uint8_t {
  Planned,
  NoConsequence,
  Duplicate,
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
  uint64_t row_tuples = 0;
  uint32_t branch_domains = 0;
  uint32_t maximum_event_width = 0;

  friend bool operator==(const Statistics &, const Statistics &) = default;
};

struct Plan {
  Request request;
  DuplicateKey duplicate_key;
  uint32_t derivation = 0;
  std::vector<AuthenticatedSource> sources;
  std::vector<int32_t> consequence;
  std::vector<Event> events;
  ProjectionObligation projection;
  Statistics statistics;

  friend bool operator==(const Plan &, const Plan &) = default;
};

struct Result {
  Status status = Status::InvalidRequest;
  LimitKind limit = LimitKind::None;
  gf3_gaussian_bridge::LimitKind bridge_limit =
      gf3_gaussian_bridge::LimitKind::None;
  Statistics consumed;
  Plan plan;
};

using Charge = std::function<bool(uint64_t)>;

// The cheap causal predicate.  It checks only the authenticated Gaussian
// outcome shape and tuple arithmetic.  It allocates no proof plan and touches
// no solver state.  A Planned result means that full proof construction is
// worth attempting; it is not an admission result.
Result inspect(const gf3_affine::Theory &,
               const gf3_gaussian_bridge::Plan &, const Request &,
               const Limits &);

// Revalidate the exact theory and Gaussian derivation, then derive one
// forbidden-tuple clause of one non-unit reduced row.  Auxiliary clauses and
// the final consequence are witness-free RUP additions.  The consequence uses
// only existing literals, so model projection is the identity.
Result build(const gf3_affine::Theory &, const gf3_gaussian_bridge::Plan &,
             const Request &, const Limits &, Charge);

// Independently rebuild and exact-compare every source reference, event,
// duplicate identity, projection obligation, and statistic.
Result validate(const gf3_affine::Theory &,
                const gf3_gaussian_bridge::Plan &, const Plan &,
                const Limits &, Charge);

} // namespace tactix::gf3_general_row
