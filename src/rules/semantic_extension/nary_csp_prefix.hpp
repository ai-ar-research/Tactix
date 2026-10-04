#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace tactix::nary_csp_prefix {

// Clause identity is local to the immutable source slice.  The optional
// native id lets a later live adapter retain exact proof provenance.
struct Premise {
  uint64_t occurrence = 0;
  std::vector<int32_t> literals;
  uint64_t native_proof_id = 0;
};

// A complete exactly-one domain.  Values are strictly ordered by
// (absolute-variable, signed-literal).  at_most_one is a square row-major
// matrix whose diagonal is zero and whose symmetric off-diagonal entries name
// the exact binary occurrence (~value[i] v ~value[j]).
struct Domain {
  std::vector<int32_t> values;
  uint64_t at_least_one = 0;
  std::vector<uint64_t> at_most_one;
};

// One authenticated forbidden tuple.  value_indices has one coordinate per
// domain and occurrence names exactly the clause containing the complements
// of those values.  A relation stores tuples in strict lexicographic coordinate
// order.  It may be sparse: omitted tuples are simply not used as premises and
// are never assumed to be allowed.
struct ForbiddenTuple {
  std::vector<uint32_t> value_indices;
  uint64_t occurrence = 0;
};

struct Relation {
  std::vector<Domain> domains;
  std::vector<ForbiddenTuple> forbidden;
};

struct DomainValue {
  uint32_t domain = 0;
  uint32_t value = 0;

  bool operator==(const DomainValue &) const = default;
};

// Select exactly one value in every domain except residual_domain.  Entries
// must be ordered by increasing domain, making the helper meaning and replay
// identity independent of container iteration.
struct Selection {
  uint32_t residual_domain = 0;
  std::vector<DomainValue> prefix;

  bool operator==(const Selection &) const = default;
};

struct Bounds {
  uint64_t maximum_source_clauses = 0;
  uint64_t maximum_source_literals = 0;
  uint32_t maximum_domains = 0;
  uint32_t maximum_domain_values = 0;
  uint64_t maximum_forbidden_tuples = 0;
  uint32_t maximum_prefix_arity = 0;
  uint32_t maximum_definition_clause_literals = 0;
  uint64_t maximum_additions = 0;
  uint64_t maximum_addition_literals = 0;
  // Caller-reserved positive external variable, strictly above every source
  // variable.  A planned result always consumes it.
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
  // RAT on -p while no positive p occurrence exists.
  FreshNegativeRat,
  // RAT on p; each resolvent with a preceding (-p v value) is tautological.
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
  std::vector<int32_t> conjuncts;
  // p iff AND(conjuncts).  Projecting an extended model omits p; extending an
  // original model assigns exactly this conjunction.  No reconstruction-stack
  // witness is required.
  bool exact_and_definition = false;
  bool project_from_external_model = false;
  bool reconstruction_witness_required = false;
};

struct Plan {
  Status status = Status::InvalidRequest;
  uint64_t consumed_work = 0;
  uint64_t consumed_source_clauses = 0;
  uint64_t consumed_source_literals = 0;
  Selection selection;
  std::vector<int32_t> prefix_literals;
  std::vector<uint64_t> matching_forbidden_premises;
  std::vector<int32_t> excluded_residual_values;
  std::vector<Addition> additions;
  ProjectionObligation projection;
};

using Charge = std::function<bool(uint64_t)>;

// Validate complete one-hot domains and exact forbidden-tuple premises, then
// define p iff AND(selected prefix values) and derive
//
//   (~p v ~residual_value)
//
// for every authenticated forbidden tuple sharing the prefix.  If all values
// of the residual domain are excluded, also derive (~p) by RUP using its ALO.
// The immutable plan performs no formula, proof-stream, namespace, heuristic,
// or reconstruction mutation.  Every decline has empty additions/projection.
Plan plan_prefix(const std::vector<Premise> &source,
                 const Relation &relation, const Selection &selection,
                 const Bounds &bounds, Charge charge);

} // namespace tactix::nary_csp_prefix
