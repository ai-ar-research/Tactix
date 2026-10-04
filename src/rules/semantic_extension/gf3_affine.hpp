#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace tactix::gf3_affine {

// Clause identity is local to one immutable source snapshot.  A native proof
// id is optional for proof formats that name premises explicitly.
struct Premise {
  uint64_t occurrence = 0;
  std::vector<int32_t> literals;
  uint64_t native_proof_id = 0;
};

// values[v] is the signed Boolean selection literal for field value v.
// Exactly one value is true.  at_most_one is a symmetric 3x3 matrix whose
// off-diagonal entries name (~values[i] v ~values[j]); its diagonal is zero.
struct Domain {
  std::vector<int32_t> values;
  uint64_t at_least_one = 0;
  std::vector<uint64_t> at_most_one;
};

// The explicit theory meaning is
//
//   sum coefficients[i] * value(domains[i]) = rhs (mod 3).
//
// domains are strictly increasing, coefficients are 1 or 2, and the first
// coefficient is normalized to 1.  forbidden_by_tuple is indexed in base 3,
// least-significant digit first.  A nonzero entry names the exact source
// clause forbidding that tuple; a zero entry denotes a satisfying tuple.
struct Equation {
  std::vector<uint32_t> domains;
  std::vector<uint8_t> coefficients;
  uint8_t rhs = 0;
  std::vector<uint64_t> forbidden_by_tuple;
};

struct Theory {
  std::vector<Premise> source;
  std::vector<Domain> domains;
  std::vector<Equation> equations;
  uint32_t maximum_variable = 0;
};

struct RecognitionBounds {
  uint64_t maximum_source_clauses = 0;
  uint64_t maximum_source_literals = 0;
  uint32_t maximum_domains = 0;
  uint32_t maximum_equations = 0;
  uint32_t maximum_equation_arity = 0;
  uint64_t maximum_tuples_per_equation = 0;
};

enum class RecognitionStatus : uint8_t {
  Recognized,
  InvalidRequest,
  IncompleteTheory,
  BoundExceeded,
  EffortExhausted,
};

struct RecognitionResult {
  RecognitionStatus status = RecognitionStatus::InvalidRequest;
  uint64_t consumed_work = 0;
  uint64_t consumed_source_clauses = 0;
  uint64_t consumed_source_literals = 0;
  Theory theory;
};

using Charge = std::function<bool(uint64_t)>;

// Recover a complete GF(3) affine slice.  Every supplied clause must be exactly
// one domain ALO/AMO premise or one member of a complete affine forbidden-tuple
// group.  Missing, duplicate, or unclassified premises decline atomically.
RecognitionResult recognize(const std::vector<Premise> &source,
                            const RecognitionBounds &bounds, Charge charge);

struct PairEliminationBounds {
  uint32_t maximum_result_arity = 0;
  uint64_t maximum_result_tuples = 0;
  uint64_t maximum_additions = 0;
  uint64_t maximum_addition_literals = 0;
};

enum class PlanStatus : uint8_t {
  Planned,
  NoConsequence,
  InvalidRequest,
  UnsupportedOverlap,
  IncompleteTheory,
  BoundExceeded,
  EffortExhausted,
};

struct SourceReference {
  uint64_t occurrence = 0;
  uint64_t native_proof_id = 0;
};

struct Addition {
  std::vector<int32_t> clause;
  // Every planned clause is RUP against the recognized source.  antecedents
  // contain the four parent forbidden tuples and the pivot-domain ALO used by
  // the local propagation argument.
  std::vector<SourceReference> antecedents;
};

struct EquationMeaning {
  std::vector<uint32_t> domains;
  std::vector<uint8_t> coefficients;
  uint8_t rhs = 0;
};

struct ProjectionObligation {
  // This increment introduces no extension variable.  The explicit fields
  // prevent a later live adapter from silently treating a semantic variable
  // as an original variable or omitting reconstruction work.
  std::vector<int32_t> extension_variables;
  bool external_extent_unchanged = true;
  bool reconstruction_witness_required = false;
};

struct PairEliminationPlan {
  PlanStatus status = PlanStatus::InvalidRequest;
  uint64_t consumed_work = 0;
  uint64_t consumed_result_tuples = 0;
  uint64_t consumed_addition_literals = 0;
  uint32_t first_equation = 0;
  uint32_t second_equation = 0;
  uint32_t pivot_domain = 0;
  EquationMeaning consequence;
  std::vector<Addition> additions;
  ProjectionObligation projection;
};

// Eliminate the sole shared domain from two complete recognized equations and
// publish the complete forbidden-tuple CNF of the resulting GF(3) equation.
// Declines are mutation-free and contain no additions or semantic consequence.
PairEliminationPlan
plan_pair_elimination(const Theory &theory, uint32_t first_equation,
                      uint32_t second_equation,
                      const PairEliminationBounds &bounds, Charge charge);

} // namespace tactix::gf3_affine
