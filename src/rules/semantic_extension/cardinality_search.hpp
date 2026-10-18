#pragma once

#include "proof/cardinality_certificate.hpp"

#include <cstdint>
#include <vector>

namespace tactix::cardinality_search {

using Clause = cardinality_certificate::Clause;

// Each premise is a physical CNF clause. The complete subset encoding is
// required: at least lower and at most upper true signed literals.
struct Constraint {
  std::vector<int32_t> literals;
  uint32_t lower = 0;
  uint32_t upper = 0;
  std::vector<Clause> premises;
};

struct Limits {
  uint32_t maximum_support_variables = 16;
  uint32_t maximum_constraints = 16;
  uint32_t maximum_source_clauses = 4096;
  uint32_t maximum_reason_width = 17;
  uint64_t maximum_work = 1000000;
};

enum class Status : uint8_t {
  Invalid,
  ResourceExhausted,
  Interrupted,
  NoEvent,
  Conflict,
  Implication,
};

struct Result {
  Status status = Status::Invalid;
  Clause reason;
  uint64_t work = 0;
  uint32_t models = 0;
};

enum class ConstructStatus : uint8_t { Invalid, ResourceExhausted, Constructed };

struct ConstructResult {
  ConstructStatus status = ConstructStatus::Invalid;
  Constraint constraint;
  uint64_t work = 0;
};

struct ResolutionLimits {
  uint64_t maximum_work = 1000000;
  uint32_t maximum_additions = 10000;
  uint64_t maximum_added_literals = 100000;
};

struct ResolutionPlan {
  Status status = Status::Invalid;
  std::vector<Clause> additions;
  uint64_t work = 0;
  uint64_t added_literals = 0;
};

// trail contains true signed literals. Assignments outside the admitted
// support are ignored; the reason contains only the relevant antecedents.
// Conflict takes precedence over the lowest-variable implication.
Result inspect(const std::vector<Constraint> &constraints,
               const std::vector<int32_t> &trail, const Limits &limits);

// Build the complete source-CNF demand from a typed signed count bound.
// The live adapter must then match every demanded clause to a physical source.
ConstructResult construct(std::vector<int32_t> literals, uint32_t lower,
                          uint32_t upper, const Limits &limits);

// Davis--Putnam elimination derives only RUP resolvents from the exact source
// clauses. The last addition is always the requested reason, ready for live
// installation. A bounded failure leaves the caller's proof and Kernel intact.
ResolutionPlan plan_resolution(const std::vector<Constraint> &constraints,
                               const Clause &reason,
                               const ResolutionLimits &limits);

// The sink follows cardinality_certificate::BddProof's contract. It must
// reserve fresh proof variables above every input variable. Count-only
// preflight is required before any live proof writer invokes this function.
template <class Sink>
bool certify(const std::vector<Constraint> &constraints,
             const Clause &reason, Sink &sink) {
  cardinality_certificate::BddProof<Sink> engine(sink);
  std::vector<uint32_t> roots;
  for (const Constraint &constraint : constraints)
    for (const Clause &premise : constraint.premises) {
      uint32_t root = 0;
      if (!engine.validate_clause(premise, root)) return false;
      roots.push_back(root);
    }
  if (roots.empty()) return false;
  const uint32_t conjunction = engine.conjunction(std::move(roots));
  if (sink.stopped) return false;
  const uint32_t target = engine.build_clause(reason);
  return !sink.stopped &&
         engine.imply_and_validate(conjunction, engine.truth(), target) &&
         sink.add(reason, false) && !sink.stopped;
}

} // namespace tactix::cardinality_search
