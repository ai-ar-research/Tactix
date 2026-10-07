#pragma once

#include "rules/semantic_extension/direct_csp_support.hpp"

namespace tactix::direct_csp_guarded_cover {

using Premise = direct_csp_support::Premise;
using Addition = direct_csp_support::Addition;
using Charge = direct_csp_support::Charge;

struct ExactOr {
  int32_t output = 0;
  std::vector<int32_t> inputs;
  bool operator==(const ExactOr &) const = default;
};

struct Blocker {
  int32_t literal = 0;
  uint32_t domain = 0;
  uint32_t value = 0;
  uint32_t or_definition = 0;
  bool operator==(const Blocker &) const = default;
};

// The complete cover is an ordinary disjunction, never an exact-one domain.
// All selected domain values remain in the consequence; none is a hidden guard.
struct Selection {
  std::vector<std::vector<int32_t>> domains;
  std::vector<uint32_t> selected_values;
  std::vector<int32_t> cover;
  std::vector<ExactOr> exact_or;
  std::vector<Blocker> blockers;
  bool operator==(const Selection &) const = default;
};

struct Bounds {
  uint64_t maximum_source_clauses = 0;
  uint64_t maximum_source_literals = 0;
  uint32_t maximum_domains = 0;
  uint32_t maximum_domain_values = 0;
  uint32_t maximum_cover_literals = 0;
  uint32_t maximum_or_definitions = 0;
  uint32_t maximum_or_inputs = 0;
  uint32_t maximum_output_literals = 0;
  bool valid() const;
};

enum class Status : uint8_t {
  Planned, Invalid, IncompleteTheory, NoConsequence, ResourceExhausted,
  Interrupted,
};

struct Expected {
  Status status = Status::Invalid;
  std::vector<std::vector<int32_t>> clauses;
  uint64_t work = 0;
};

struct Plan {
  Status status = Status::Invalid;
  std::vector<Addition> additions;
  uint64_t work = 0;
  uint64_t source_literals = 0;
};

// Pure structural validation and exact requested existing-variable clause image.
// ResourceExhausted also denotes a false charge callback; live code preserves
// the separate Interrupted classification from its own monotone meter.
Expected expected_clauses(const Selection &, const Bounds &, Charge);

// Every blocker must be falsified under the negation of the selected tuple:
// b->unselected domain value closes with exact AMO, or b->~q closes with
// selected input->q. One-sided links suffice; no AND converse is assumed.
Plan plan(const std::vector<Premise> &, const Selection &, const Bounds &,
          Charge);

} // namespace tactix::direct_csp_guarded_cover
