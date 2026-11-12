#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace tactix::gf3_affine {

// A source-snapshot clause; the native proof id is optional.
struct Premise {
  uint64_t occurrence = 0;
  std::vector<int32_t> literals;
  uint64_t native_proof_id = 0;
  friend bool operator==(const Premise &, const Premise &) = default;
};

// values[v] selects field value v (exactly one true); at_most_one is a
// symmetric zero-diagonal 3x3 matrix naming (~values[i] v ~values[j]).
struct Domain {
  std::vector<int32_t> values;
  uint64_t at_least_one = 0;
  std::vector<uint64_t> at_most_one;
  friend bool operator==(const Domain &, const Domain &) = default;
};

//   sum coefficients[i] * value(domains[i]) = rhs (mod 3)
// over increasing domains, coefficients 1 or 2 with the first 1.
// forbidden_by_tuple is indexed in base 3, least-significant digit first; a
// nonzero entry names the clause forbidding that tuple, zero a solution.
struct Equation {
  std::vector<uint32_t> domains;
  std::vector<uint8_t> coefficients;
  uint8_t rhs = 0;
  std::vector<uint64_t> forbidden_by_tuple;
  friend bool operator==(const Equation &, const Equation &) = default;
};

struct Theory {
  std::vector<Premise> source;
  std::vector<Domain> domains;
  std::vector<Equation> equations;
  uint32_t maximum_variable = 0;
  // Semantic identity for revalidating retained owners.
  friend bool operator==(const Theory &, const Theory &) = default;
};

struct RecognitionBounds {
  uint64_t maximum_source_clauses = 0;
  uint64_t maximum_source_literals = 0;
  uint32_t maximum_domains = 0;
  uint32_t maximum_equations = 0;
  uint32_t maximum_equation_arity = 0;
  uint64_t maximum_tuples_per_equation = 0;
  bool valid() const {
    return maximum_source_clauses && maximum_source_literals &&
           maximum_domains && maximum_equations &&
           maximum_equation_arity >= 2 && maximum_tuples_per_equation;
  }
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

// Recover a GF(3) affine slice: every clause must be a domain ALO/AMO or part
// of a complete forbidden-tuple group, else the recognition declines.
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
  friend bool operator==(const SourceReference &,
                         const SourceReference &) = default;
};

struct Addition {
  std::vector<int32_t> clause;
  // RUP antecedents: four parent forbidden tuples and the pivot-domain ALO.
  std::vector<SourceReference> antecedents;
};

struct EquationMeaning {
  std::vector<uint32_t> domains;
  std::vector<uint8_t> coefficients;
  uint8_t rhs = 0;
};

struct ProjectionObligation {
  // No extension variable is introduced; the fields keep it explicit.
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

// Eliminate the shared domain of two equations and plan the forbidden-tuple
// CNF of the result; a decline has no additions.
PairEliminationPlan
plan_pair_elimination(const Theory &theory, uint32_t first_equation,
                      uint32_t second_equation,
                      const PairEliminationBounds &bounds, Charge charge);

} // namespace tactix::gf3_affine
