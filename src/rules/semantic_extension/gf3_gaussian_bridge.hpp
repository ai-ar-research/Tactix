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

  friend bool operator==(const DomainAuthentication &,
                         const DomainAuthentication &) = default;
};

struct TupleAuthentication {
  uint64_t tuple = 0;
  gf3_affine::SourceReference premise;

  friend bool operator==(const TupleAuthentication &,
                         const TupleAuthentication &) = default;
};

// Every exact-one and forbidden-tuple premise of one algebraic row.
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
  bool valid() const {
    return maximum_work && maximum_source_clauses &&
           maximum_source_literals && maximum_domains && maximum_equations &&
           maximum_terms && maximum_authentication_references &&
           maximum_references_per_equation && gaussian.valid();
  }
  friend bool operator==(const Limits &, const Limits &) = default;
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

// Reauthenticate the theory and reduce its equations; domain d is Gaussian
// variable d+1 and source row i has identity i+1.
Result build(const gf3_affine::Theory &, const Limits &, Charge);

// Reauthenticate the theory and replay every Source/Scale/Add node, its source
// combination and the reduced basis.
Result validate(const gf3_affine::Theory &, const Plan &, const Limits &,
                Charge);

} // namespace tactix::gf3_gaussian_bridge
