#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace tactix::direct_csp_support {

// Clause identity is local to the immutable source snapshot supplied to the
// planner.  native_proof_id is optional because DSR/DRAT modes can name a
// premise by its literals while an LRAT integration needs the retained id.
struct Premise {
  uint64_t occurrence = 0;
  std::vector<int32_t> literals;
  uint64_t native_proof_id = 0;

  friend bool operator==(const Premise &, const Premise &) = default;
};

// A complete exactly-one domain.  Values are strictly ordered by
// (absolute-variable, signed-literal) and no variable occurs twice.
// at_most_one is a square row-major matrix.
// Its diagonal is zero and symmetric off-diagonal entries name the same exact
// binary occurrence (~value[i] v ~value[j]).
struct Domain {
  std::vector<int32_t> values;
  uint64_t at_least_one = 0;
  std::vector<uint64_t> at_most_one;

  friend bool operator==(const Domain &, const Domain &) = default;
};

// A complete directed binary-compatibility table.  A nonzero row-major entry
// names the exact source occurrence (~left[i] v ~right[j]); zero denotes a
// compatible pair.  The source snapshot must consist exactly of both complete
// domain encodings and these forbidden-pair occurrences.
struct Relation {
  Domain left;
  Domain right;
  std::vector<uint64_t> forbidden;

  friend bool operator==(const Relation &, const Relation &) = default;
};

struct Bounds {
  uint64_t maximum_source_clauses = 0;
  uint64_t maximum_source_literals = 0;
  uint32_t maximum_domain_values = 0;
  // A support clause no wider than this is emitted directly.  A wider clause
  // may use one exact OR extension when allow_extension is true.
  uint32_t maximum_direct_clause_literals = 0;
  uint32_t maximum_definition_clause_literals = 0;
  uint64_t maximum_additions = 0;
  uint64_t maximum_addition_literals = 0;
  bool allow_extension = false;
  // Caller-reserved positive external variable.  It must be strictly above
  // every variable in the source snapshot.  It is consumed only when an OR
  // extension is actually planned.
  int32_t extension_variable = 0;
};

enum class Status : uint8_t {
  Planned,
  NoConsequence,
  InvalidRequest,
  IncompleteTheory,
  BoundExceeded,
  EffortExhausted,
};

enum class ReferenceKind : uint8_t { SourceOccurrence, PlannedAddition };

struct Reference {
  ReferenceKind kind = ReferenceKind::SourceOccurrence;
  uint64_t id = 0;
  uint64_t native_proof_id = 0;
};

enum class ProofStep : uint8_t {
  // RAT on the positive fresh variable before any negative occurrence exists.
  FreshPositiveRat,
  // RAT on the negative fresh variable; the referenced positive definition
  // clauses make every resolvent tautological.
  DefinitionReverseRat,
  Rup,
};

struct Addition {
  std::vector<int32_t> clause;
  ProofStep proof = ProofStep::Rup;
  int32_t pivot = 0;
  std::vector<Reference> antecedents;
};

struct ProjectionObligation {
  int32_t extension_variable = 0;
  std::vector<int32_t> disjuncts;
  // The plan defines extension_variable iff OR(disjuncts).  It is omitted
  // from the external model; no reconstruction-stack witness is required.
  bool exact_or_definition = false;
  bool project_from_external_model = false;
  bool reconstruction_witness_required = false;
};

struct Plan {
  Status status = Status::InvalidRequest;
  uint64_t consumed_work = 0;
  uint64_t consumed_source_clauses = 0;
  uint64_t consumed_source_literals = 0;
  uint32_t selected_left_value = 0;
  std::vector<int32_t> compatible_right_values;
  std::vector<uint64_t> forbidden_premises;
  std::vector<Addition> additions;
  ProjectionObligation projection;
};

// Charge is monotone and is called before each bounded unit of recognition or
// planning work.  Returning false aborts without publishing a partial plan.
using Charge = std::function<bool(uint64_t)>;

// Validate an exact two-domain DirectCSP slice and plan the consequence
//
//   ~left[selected] v OR { right[j] : (selected,j) is compatible }.
//
// It follows by resolving right's at-least-one premise with every exact
// forbidden-pair premise in the selected row.  The returned plan is immutable
// and performs no kernel or proof-stream mutation.  On any decline additions
// and projection are empty, while consumed effort remains truthful.
Plan plan_support(const std::vector<Premise> &source, const Relation &relation,
                  uint32_t selected_left_value, const Bounds &bounds,
                  Charge charge);

} // namespace tactix::direct_csp_support
