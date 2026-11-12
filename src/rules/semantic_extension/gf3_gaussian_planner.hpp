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

  friend bool operator==(const SourceRow &, const SourceRow &) = default;
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

// Hard limits; work is charged before each operation. Fill counts nonzero
// entries retained by Scale/Add. The retained-plan limit bounds the encoded
// nodes and outcomes, not heap use.
struct Limits {
  uint64_t maximum_work = 0;
  uint64_t maximum_fill_entries = 0;
  uint32_t maximum_intermediate_width = 0;
  uint32_t maximum_provenance_terms = 0;
  uint32_t maximum_derivations = 0;
  uint64_t maximum_retained_plan_bytes = 0;
  bool valid() const {
    return maximum_work && maximum_fill_entries &&
           maximum_intermediate_width && maximum_provenance_terms &&
           maximum_derivations && maximum_retained_plan_bytes;
  }
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

  friend bool operator==(const Statistics &, const Statistics &) = default;
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
  // Present only for Complete.
  Plan plan;
};

using Charge = std::function<bool(uint64_t)>;

// Retained-plan bytes of one outcome, and of one derivation node (false on
// overflow).
inline constexpr uint64_t retained_outcome_bytes = 16;
bool retained_derivation_bytes(const Derivation &node, uint64_t &bytes);

// A reduced row-echelon basis independent of input order (rows sorted by
// equation, then source); derivation indices are one-based.
Result build_plan(const std::vector<SourceRow> &source, const Limits &limits,
                  Charge charge);

} // namespace tactix::gf3_gaussian
