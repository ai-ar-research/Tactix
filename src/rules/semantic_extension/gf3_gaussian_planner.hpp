#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace tactix::gf3_gaussian {

struct Term {
  uint32_t variable = 0;
  uint8_t coefficient = 0;

  friend bool operator==(const Term &, const Term &) = default;
};

// Terms are strictly ordered by variable, coefficients and rhs are in GF(3),
// and source_id identifies one immutable recognized theory equation.
struct SourceRow {
  uint64_t source_id = 0;
  std::vector<Term> terms;
  uint8_t rhs = 0;
};

struct ProvenanceTerm {
  uint64_t source_id = 0;
  uint8_t coefficient = 0;

  friend bool operator==(const ProvenanceTerm &,
                         const ProvenanceTerm &) = default;
};

enum class DerivationKind : uint8_t { Source, Scale, Add };

struct Derivation {
  DerivationKind kind = DerivationKind::Source;
  // Source: source_id is set and parents are zero.  Scale: first_parent and
  // factor are set.  Add: both parents are set and factor is one.
  uint64_t source_id = 0;
  uint32_t first_parent = 0;
  uint32_t second_parent = 0;
  uint8_t factor = 1;
  std::vector<Term> row;
  uint8_t rhs = 0;
  // The exact linear combination of original source rows represented here.
  // Terms are strictly ordered and have nonzero GF(3) coefficients.
  std::vector<ProvenanceTerm> provenance;
};

enum class RowOutcomeKind : uint8_t {
  // A non-unit row in the final deterministic reduced basis.
  Derived,
  // A one-variable row in that basis.
  Unit,
  // The inconsistent row 0 = 1 or 0 = 2.
  ZeroRowConflict,
};

struct RowOutcome {
  RowOutcomeKind kind = RowOutcomeKind::Derived;
  uint32_t derivation = 0;
};

// Every limit is hard for its named metric.  Work is charged before each
// bounded operation.  Fill is the cumulative number of nonzero row entries
// retained by Scale/Add derivations.  The retained-plan limit bounds the
// deterministic encoding of published nodes and outcomes.  It is deliberately
// not advertised as an allocator or RSS cap: STL capacity, temporary merge
// buffers, the basis, and indexing containers are excluded.  Their entry
// counts are structurally bounded by the derivation, width, and provenance
// limits; an exact heap cap requires a later budgeted-arena adapter.
struct Limits {
  uint64_t maximum_work = 0;
  uint64_t maximum_fill_entries = 0;
  uint32_t maximum_intermediate_width = 0;
  uint32_t maximum_provenance_terms = 0;
  uint32_t maximum_derivations = 0;
  uint64_t maximum_retained_plan_bytes = 0;
};

enum class LimitKind : uint8_t {
  None,
  Work,
  Fill,
  IntermediateWidth,
  Provenance,
  Derivations,
  RetainedPlan,
};

enum class PlanStatus : uint8_t {
  Complete,
  NoStructure,
  InvalidInput,
  LimitExceeded,
  Interrupted,
};

struct Statistics {
  uint64_t work = 0;
  uint64_t fill_entries = 0;
  uint64_t retained_plan_bytes = 0;
};

struct Plan {
  std::vector<Derivation> derivations;
  std::vector<RowOutcome> outcomes;
  Statistics statistics;
};

struct Result {
  PlanStatus status = PlanStatus::InvalidInput;
  LimitKind limit = LimitKind::None;
  Statistics consumed;
  // Present only for Complete.  Declines and interruptions publish no partial
  // derivation graph.
  Plan plan;
};

using Charge = std::function<bool(uint64_t)>;

// Build a deterministic reduced row-echelon basis.  Input order has no effect:
// rows are processed by their normalized equation and then source identity.
// Derivation indices are one-based so zero is an invalid parent/outcome.
Result build_plan(const std::vector<SourceRow> &source, const Limits &limits,
                  Charge charge);

} // namespace tactix::gf3_gaussian
