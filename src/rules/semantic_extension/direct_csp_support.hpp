#pragma once

#include "rules/semantic_extension/csp_exact_theory.hpp"

#include <cstdint>
#include <functional>
#include <vector>

namespace tactix::direct_csp_support {

// A source-snapshot clause; native_proof_id is optional under DSR.
struct Premise {
  uint64_t occurrence = 0;
  std::vector<int32_t> literals;
  uint64_t native_proof_id = 0;

  friend bool operator==(const Premise &, const Premise &) = default;
};

// An exactly-one domain with values ordered by (variable, literal), distinct
// variables, and a symmetric zero-diagonal at_most_one matrix naming each
// binary occurrence (~value[i] v ~value[j]).
struct Domain {
  std::vector<int32_t> values;
  uint64_t at_least_one = 0;
  std::vector<uint64_t> at_most_one;

  friend bool operator==(const Domain &, const Domain &) = default;
};

// A directed compatibility table: a nonzero entry names the occurrence
// (~left[i] v ~right[j]), zero a compatible pair. The snapshot holds exactly
// both domain encodings and these occurrences.
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
  // Reserved variable above the snapshot, used only by an OR extension.
  int32_t extension_variable = 0;
};

using Status = csp_exact_theory::PlanStatus;
using ReferenceKind = csp_exact_theory::ReferenceKind;
using Reference = csp_exact_theory::Reference;

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

// Called before each unit of work; false aborts the plan.
using Charge = std::function<bool(uint64_t)>;

// Validate a two-domain DirectCSP slice and plan
//   ~left[selected] v OR { right[j] : (selected,j) is compatible },
// resolving right's at-least-one premise with the selected row's forbidden
// pairs. A decline has empty additions and projection.
Plan plan_support(const std::vector<Premise> &source, const Relation &relation,
                  uint32_t selected_left_value, const Bounds &bounds,
                  Charge charge);

} // namespace tactix::direct_csp_support
