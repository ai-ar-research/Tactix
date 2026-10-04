#pragma once

#include "rules/semantic_extension/gf3_affine.hpp"
#include "rules/semantic_extension/gf3_gaussian_planner.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

namespace tactix::gf3_gaussian_bridge {

struct DomainAuthentication {
  uint32_t domain = 0;
  gf3_affine::SourceReference at_least_one;
  // Pair order is (0,1), (0,2), (1,2).
  std::array<gf3_affine::SourceReference, 3> at_most_one{};

  friend bool operator==(const DomainAuthentication &first,
                         const DomainAuthentication &second) {
    const auto same = [](const gf3_affine::SourceReference &a,
                         const gf3_affine::SourceReference &b) {
      return a.occurrence == b.occurrence &&
             a.native_proof_id == b.native_proof_id;
    };
    if (first.domain != second.domain ||
        !same(first.at_least_one, second.at_least_one))
      return false;
    for (size_t i = 0; i < first.at_most_one.size(); ++i)
      if (!same(first.at_most_one[i], second.at_most_one[i])) return false;
    return true;
  }
};

struct TupleAuthentication {
  uint64_t tuple = 0;
  gf3_affine::SourceReference premise;

  friend bool operator==(const TupleAuthentication &first,
                         const TupleAuthentication &second) {
    return first.tuple == second.tuple &&
           first.premise.occurrence == second.premise.occurrence &&
           first.premise.native_proof_id ==
               second.premise.native_proof_id;
  }
};

// This is the complete source handoff for one algebraic row.  A later clausal
// adapter can recover every exact-one and forbidden-tuple premise without
// asking the SAT solver to rediscover the GF(3) equation.
struct SourceAuthentication {
  uint64_t source_id = 0;
  uint32_t equation_index = 0;
  std::vector<DomainAuthentication> domains;
  std::vector<TupleAuthentication> forbidden_tuples;

  friend bool operator==(const SourceAuthentication &,
                         const SourceAuthentication &) = default;
};

struct Limits {
  uint64_t maximum_work = 0;
  uint64_t maximum_source_clauses = 0;
  uint64_t maximum_source_literals = 0;
  uint32_t maximum_domains = 0;
  uint32_t maximum_equations = 0;
  uint64_t maximum_terms = 0;
  uint64_t maximum_authentication_references = 0;
  uint64_t maximum_references_per_equation = 0;
  gf3_gaussian::Limits gaussian;
};

enum class LimitKind : uint8_t {
  None,
  Work,
  SourceClauses,
  SourceLiterals,
  Domains,
  Equations,
  Terms,
  AuthenticationReferences,
  ReferencesPerEquation,
  Gaussian,
};

enum class Status : uint8_t {
  Complete,
  InvalidTheory,
  InvalidPlan,
  LimitExceeded,
  Interrupted,
};

struct Statistics {
  uint64_t work = 0;
  uint64_t source_clauses = 0;
  uint64_t source_literals = 0;
  uint64_t terms = 0;
  uint64_t authentication_references = 0;
};

struct Plan {
  std::vector<gf3_gaussian::SourceRow> sources;
  std::vector<SourceAuthentication> authentication;
  gf3_gaussian::Plan gaussian;
};

struct Result {
  Status status = Status::InvalidTheory;
  LimitKind limit = LimitKind::None;
  gf3_gaussian::LimitKind gaussian_limit =
      gf3_gaussian::LimitKind::None;
  Statistics consumed;
  // Published only on Complete.  Failure never exposes a partial proof plan.
  Plan plan;
};

using Charge = std::function<bool(uint64_t)>;

// Reauthenticate the recognized Boolean theory and reduce its exact equations.
// Domain d is represented by Gaussian variable d+1; source row i has bridge-
// local identity i+1.  No clause or auxiliary Boolean variable is produced.
Result build(const gf3_affine::Theory &, const Limits &, Charge);

// Independently reauthenticate the theory and replay every Source/Scale/Add
// node, its exact GF(3) source combination, and the final reduced basis.  This
// is the validation boundary intended for the later clausal proof adapter.
Result validate(const gf3_affine::Theory &, const Plan &, const Limits &,
                Charge);

} // namespace tactix::gf3_gaussian_bridge
