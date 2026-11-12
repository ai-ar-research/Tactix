#pragma once

#include "rules/semantic_extension/csp_exact_theory.hpp"
#include "rules/semantic_extension/nary_csp_relation.hpp"

#include <cstdint>
#include <functional>
#include <vector>

namespace tactix::nary_csp_equality {

using Premise = nary_csp_relation::Premise;
using Domain = nary_csp_relation::Domain;
using ForbiddenTuple = nary_csp_relation::ForbiddenTuple;
using Relation = nary_csp_relation::Relation;

// A permutation of a ternary relation's coordinates; value i has one label in
// all three domains, authenticated by the forbidden tuple (i,i,i).
struct Selection {
  uint32_t left_domain = 0;
  uint32_t right_domain = 1;
  uint32_t residual_domain = 2;

  bool operator==(const Selection &) const = default;
};

struct Bounds {
  uint64_t maximum_source_clauses = 0;
  uint64_t maximum_source_literals = 0;
  uint32_t maximum_domain_values = 0;
  uint64_t maximum_forbidden_tuples = 0;
  uint64_t maximum_additions = 0;
  uint64_t maximum_addition_literals = 0;
  // Reserved variable above the source; a plan always consumes it.
  int32_t extension_variable = 0;
};

using Status = csp_exact_theory::PlanStatus;
using ReferenceKind = csp_exact_theory::ReferenceKind;
using Reference = csp_exact_theory::Reference;

enum class ProofStep : uint8_t {
  // RAT on positive e while no negative e occurrence exists.
  FreshPositiveRat,
  // RAT on negative e.  Every non-tautological resolvent is subsumed by an
  // authenticated left-domain AMO binary.
  EqualityReverseRat,
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
  std::vector<int32_t> left_values;
  std::vector<int32_t> right_values;
  // e iff the two exact-one domains select the same aligned value index.
  bool exact_aligned_equality = false;
  bool project_from_external_model = false;
  bool reconstruction_witness_required = false;
};

struct Plan {
  Status status = Status::InvalidRequest;
  uint64_t consumed_work = 0;
  uint64_t consumed_source_clauses = 0;
  uint64_t consumed_source_literals = 0;
  Selection selection;
  std::vector<uint64_t> diagonal_tuple_premises;
  std::vector<Addition> additions;
  ProjectionObligation projection;
};

using Charge = std::function<bool(uint64_t)>;

// Over a ternary diagonal-nogood slice of three equal exact-one domains,
// define the branchable bit e with 2k RAT clauses and transfer each diagonal
// tuple with k RUP clauses:
//   e <-> OR_i (left_i AND right_i)
//   (~e OR ~left_i OR ~residual_i), for every (i,i,i) nogood.
// Under e, right_i propagates from left_i, so each transfer is RUP.
Plan plan_equality(const std::vector<Premise> &source,
                   const Relation &relation, const Selection &selection,
                   const Bounds &bounds, Charge charge);

} // namespace tactix::nary_csp_equality
