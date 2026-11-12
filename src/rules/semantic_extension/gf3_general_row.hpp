#pragma once

#include "rules/semantic_extension/gf3_gaussian_bridge.hpp"

#include <cstdint>
#include <functional>
#include <vector>

namespace tactix::gf3_general_row {

// One clause of one non-unit Gaussian row; row and tuple are the duplicate key.
struct Request {
  uint32_t outcome = 0;
  uint64_t tuple = 0;
};

struct DuplicateKey {
  std::vector<gf3_gaussian::Term> row;
  uint8_t rhs = 0;
  uint64_t tuple = 0;
};

struct AuthenticatedSource {
  gf3_gaussian::ProvenanceTerm combination;
  uint32_t authentication_index = 0;
};

enum class EventRole : uint8_t { Auxiliary, Consequence };

struct Event {
  EventRole role = EventRole::Auxiliary;
  std::vector<int32_t> clause;
};

struct ProjectionObligation {
  std::vector<int32_t> extension_variables;
  bool external_extent_unchanged = true;
  bool reconstruction_witness_required = false;
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
  // Validates this level's bounds; callers validate `bridge` separately.
  bool valid() const {
    return maximum_work && maximum_events && maximum_event_literals &&
           maximum_active_clauses && maximum_retained_source_references &&
           maximum_row_tuples && maximum_event_width &&
           maximum_branch_domains && maximum_row_width;
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

// Revalidate, then derive one forbidden-tuple clause of a non-unit row by
// witness-free RUP additions over existing literals.
Result build(const gf3_affine::Theory &, const gf3_gaussian_bridge::Plan &,
             const Request &, const Limits &, Charge);

} // namespace tactix::gf3_general_row
