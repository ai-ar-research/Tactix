// Exact, mutation-free recovery of a bounded affine image from CNF.
#pragma once

#include "rules/lucky/parity_certificate.hpp"
#include <algorithm>
#include <cstdint>
#include <functional>
#include <iterator>
#include <utility>
#include <vector>

namespace tactix::affine_semantic_extension {

using External = int32_t;

template <class T>
void xor_sorted(const std::vector<T> &first, const std::vector<T> &second,
                std::vector<T> &output) {
  output.clear();
  output.reserve(first.size() + second.size());
  std::set_symmetric_difference(first.begin(), first.end(), second.begin(),
                                second.end(), std::back_inserter(output));
}

struct Premise {
  uint64_t occurrence = 0;
  std::vector<External> literals;
};

struct Limits {
  uint64_t maximum_clauses = 0;
  uint64_t maximum_groups = 0;
  uint64_t maximum_patterns = 0;
  uint64_t maximum_elimination_work = 0;
  uint32_t maximum_arity = 0;
  uint32_t maximum_intermediate_width = 0;
  uint32_t maximum_sources = 0;
  uint32_t maximum_output_width = 0;
};

struct Equation {
  std::vector<External> variables;
  bool rhs = false;
  // One exact input occurrence for each forbidden assignment.
  std::vector<uint64_t> premises;
  std::vector<std::vector<External>> premise_clauses;

  friend bool operator==(const Equation &, const Equation &) = default;
};

struct ReducedEquation {
  std::vector<External> variables;
  bool rhs = false;
  std::vector<size_t> sources;
};

template <class Basis, class Consume>
bool reduce_equation(
    ReducedEquation &row, const Basis &basis, size_t maximum_width,
    size_t maximum_sources, bool drop_overflow, Consume consume) {
  while (!row.variables.empty()) {
    if (!consume(uint64_t{1} + row.variables.size() + row.sources.size()))
      return false;
    // Ascending pivots; the source XOR preserves the exact derivation.
    const auto found = basis.find(row.variables.front());
    if (found == basis.end()) return true;
    std::vector<External> variables;
    std::vector<size_t> sources;
    xor_sorted(row.variables, found->second.variables, variables);
    xor_sorted(row.sources, found->second.sources, sources);
    row.variables = std::move(variables);
    row.sources = std::move(sources);
    row.rhs ^= found->second.rhs;
    if (row.variables.size() <= maximum_width &&
        row.sources.size() <= maximum_sources)
      continue;
    if (!drop_overflow) return false;
    row.sources.clear();
    return true;
  }
  return true;
}

struct Consequence {
  std::vector<External> variables;
  bool rhs = false;
  // Sorted indices into Image::equations. Their XOR is this consequence.
  std::vector<size_t> sources;
  // Flattened exact source occurrences, in source-equation order.
  std::vector<uint64_t> premises;
};

struct Image {
  std::vector<Equation> equations;
  std::vector<Consequence> consequences;
  uint64_t clauses_seen = 0;
  uint64_t groups_seen = 0;
  uint64_t patterns_seen = 0;
  uint64_t elimination_work = 0;
  // All facts mention input variables only. No reconstruction witness exists.
  bool identity_model_projection = true;
};

enum class Status { Complete, NoStructure, Exhausted, Interrupted, Invalid };

// `charge` is caller-owned discovery accounting. On every result except
// Complete, `output` is unchanged. Recognition requires every CNF clause that
// forbids the wrong parity over one exact set of distinct variables. Extra CNF
// clauses merely remain outside the affine image.
Status recognize(const std::vector<Premise> &, const Limits &,
                 const std::function<bool(uint64_t)> &charge, Image &output);

// CNF for one recovered equation or consequence. The input must be sorted,
// distinct, positive external variables and is deliberately bounded to eight.
std::vector<std::vector<External>> clausal_form(
    const std::vector<External> &variables, bool rhs);

// Exact, order-independent validation of an equation's full XOR clausal form.
// Premise occurrence identity is checked at the live publication boundary.
bool authenticates_clausal_form(const Equation &);

struct ProofEvent {
  bool addition = true;
  parity_certificate::SmallClause clause;
};

struct ProofPlan {
  std::vector<ProofEvent> events;
  uint64_t additions = 0;
  uint64_t deletions = 0;
  // Exact source occurrences that must be live when the plan is replayed.
  std::vector<uint64_t> premises;
  std::vector<std::vector<External>> targets;
};

// Build, but do not emit, a RUP-only certificate for one small consequence.
// The existing parity Deriver supplies the checked branch-lifting steps. The
// source equations may have arity up to eight, provided their union contains at
// most sixteen variables (the shared small-clause proof bound). On failure the
// caller's plan is unchanged.
bool build_proof_plan(const Image &, size_t consequence,
                      uint64_t maximum_additions, ProofPlan &);

} // namespace tactix::affine_semantic_extension
