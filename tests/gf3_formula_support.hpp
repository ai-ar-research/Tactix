#pragma once
// GF(3) fixtures: one-hot domains, the forbidden-tuple clauses of linear
// equations over them, kernels holding them, ample limits, and the native
// identity discovery that callers of the selected public actions replay.
#ifdef NDEBUG
#error "gf3_formula_support.hpp asserts have side effects; #undef NDEBUG first"
#endif

#include "frontend/parse.hpp"
#include "proof/proof.hpp"
#include "rules/semantic_extension/gf3_gaussian_action.hpp"
#include "rules/semantic_extension/gf3_general_row_action.hpp"
#include "strategy/theory_action_parameters.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

using Gf3Clauses = std::vector<std::vector<int32_t>>;

struct Gf3Formula {
  Gf3Clauses clauses;
  // The same clauses as recognition premises; occurrence i has proof ID 9000 + i.
  std::vector<tactix::gf3_affine::Premise> source;
  Gf3Clauses values;

  void add(std::vector<int32_t> clause) {
    const uint64_t occurrence = source.size() + 1;
    clauses.push_back(clause);
    source.push_back({occurrence, std::move(clause), 9000 + occurrence});
  }

  // At-least-one over three selection literals, then the AMO binaries.
  uint32_t domain(std::vector<int32_t> literals) {
    assert(literals.size() == 3);
    const uint32_t index = static_cast<uint32_t>(values.size());
    values.push_back(literals);
    add(literals);
    for (unsigned first = 0; first < 3; ++first)
      for (unsigned second = first + 1; second < 3; ++second)
        add({-literals[first], -literals[second]});
    return index;
  }

  // One clause per tuple violating sum(coefficient * value) == rhs (mod 3).
  void equation(const std::vector<uint32_t> &domains,
                const std::vector<uint8_t> &coefficients, uint8_t rhs) {
    uint64_t tuples = 1;
    for (size_t i = 0; i < domains.size(); ++i) tuples *= 3;
    for (uint64_t tuple = 0; tuple < tuples; ++tuple) {
      uint64_t rest = tuple;
      unsigned sum = 0;
      std::vector<int32_t> clause;
      for (size_t i = 0; i < domains.size(); ++i) {
        const unsigned value = rest % 3;
        rest /= 3;
        sum = (sum + coefficients[i] * value) % 3;
        clause.push_back(-values[domains[i]][value]);
      }
      if (sum != rhs) add(std::move(clause));
    }
  }
};

// Domains d = 0..n-1 over variables 3d+1..3d+3.
inline Gf3Formula gf3_domains(uint32_t n) {
  Gf3Formula formula;
  for (uint32_t domain = 0; domain < n; ++domain) {
    const int32_t first = static_cast<int32_t>(3 * domain + 1);
    formula.domain({first, first + 1, first + 2});
  }
  return formula;
}

// gf3_domains(n) plus x + y + z = 0 per entry of equations. Incomplete drops
// the final forbidding clause.
inline Gf3Clauses gf3_formula(uint32_t domains,
                              const std::vector<std::vector<uint32_t>> &equations,
                              bool complete = true) {
  Gf3Formula formula = gf3_domains(domains);
  for (const auto &equation : equations)
    formula.equation(equation, {1, 1, 1}, 0);
  if (!complete) formula.clauses.pop_back();
  return formula.clauses;
}

// Domains x, y, z over variables 1..9 with x + y = 0 and y + z = 0.
inline Gf3Formula gf3_chain() {
  Gf3Formula formula = gf3_domains(3);
  formula.equation({0, 1}, {1, 1}, 0);
  formula.equation({1, 2}, {1, 1}, 0);
  return formula;
}

// The chain plus x + z = 1: Source/Scale/Add closure fixes x=2, y=1, z=2
// although no source pair alone has a unit result.
inline Gf3Formula gf3_chained_units() {
  Gf3Formula formula = gf3_chain();
  formula.equation({0, 2}, {1, 1}, 1);
  return formula;
}

// The chain implies x + 2z = 0; adding x + 2z = 1 makes it inconsistent.
inline Gf3Formula gf3_conflict() {
  Gf3Formula formula = gf3_chain();
  formula.equation({0, 2}, {1, 2}, 1);
  return formula;
}

// Four domains over 1..12 with x + y + u = 0 and y + z + u = 0: they imply
// the non-unit row x - z = 0 while y and u stay underdetermined, so the
// general-row planner needs its bounded case-split auxiliaries.
inline Gf3Formula gf3_row_fixture() {
  Gf3Formula formula = gf3_domains(4);
  formula.equation({0, 1, 3}, {1, 1, 1}, 0);
  formula.equation({1, 2, 3}, {1, 1, 1}, 0);
  return formula;
}

// The recognized theory of a fixture's source premises.
inline tactix::gf3_affine::Theory recognize(const Gf3Formula &formula) {
  const tactix::gf3_affine::RecognitionBounds bounds{1000, 10000, 100, 100, 8, 10000};
  auto result = tactix::gf3_affine::recognize(
      formula.source, bounds, [](uint64_t) { return true; });
  assert(result.status == tactix::gf3_affine::RecognitionStatus::Recognized);
  return std::move(result.theory);
}

// Ample Gaussian-bridge bounds.
inline tactix::gf3_gaussian_bridge::Limits bridge_limits() {
  tactix::gf3_gaussian_bridge::Limits result;
  result.maximum_work = 1000000;
  result.maximum_source_clauses = 10000;
  result.maximum_source_literals = 100000;
  result.maximum_domains = 100;
  result.maximum_equations = 100;
  result.maximum_terms = 1000;
  result.maximum_authentication_references = 10000;
  result.maximum_references_per_equation = 1000;
  result.gaussian = {100000, 100000, 32, 100, 1000, 1000000};
  return result;
}

// The complete bridge plan of a recognized theory.
inline tactix::gf3_gaussian_bridge::Plan bridge(const tactix::gf3_affine::Theory &theory) {
  auto result = tactix::gf3_gaussian_bridge::build(
      theory, bridge_limits(), [](uint64_t) { return true; });
  assert(result.status == tactix::gf3_gaussian_bridge::Status::Complete);
  return std::move(result.plan);
}

// Native clauses with IDs 1..n over variables 1..variables.
inline tactix::Kernel gf3_native_kernel(const Gf3Formula &formula,
                                        uint32_t variables) {
  tactix::Kernel kernel;
  tactix::kernel_reserve_vars(kernel, variables);
  for (const auto &external : formula.clauses) {
    std::vector<tactix::Lit> clause;
    for (int32_t literal : external)
      clause.push_back(tactix::kernel_internalize(kernel, literal));
    const uint64_t id = ++kernel.next_clause_id;
    if (clause.size() == 2)
      tactix::kernel_add_binary_clause(kernel, clause[0], clause[1], id);
    else
      tactix::kernel_add_large_clause(kernel, clause.data(), clause.size(),
                                      false, 0, id);
  }
  return kernel;
}

// The clauses loaded as the parser loads input.
inline tactix::Kernel gf3_parsed_kernel(const Gf3Clauses &clauses) {
  tactix::Kernel kernel;
  for (const auto &external : clauses) {
    std::vector<tactix::Lit> clause;
    for (int32_t literal : external) clause.push_back(tactix::internalize(literal));
    tactix::ingest_clause(kernel, clause);
  }
  tactix::finish_input(kernel);
  assert(kernel.trail.empty() && tactix::kernel_propagation_fixpoint(kernel));
  return kernel;
}
inline tactix::Kernel gf3_parsed_kernel(const Gf3Formula &formula) {
  return gf3_parsed_kernel(formula.clauses);
}

// Bind a fresh streaming-PBP proof at path to kernel.
inline void bind_pbp_streaming(tactix::Proof &proof, tactix::Kernel &kernel,
                               const char *path) {
  std::remove(path);
  assert(proof.open(path, tactix::ProofFormat::PbpStreaming));
  proof.finish_input_transaction();
  assert(proof.begin_pb());
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  kernel.proof = &proof;
}

// The complete-batch identity of the kernel's Gaussian closure, or empty.
inline std::string gf3_gaussian_identity(tactix::Kernel &kernel) {
  const auto l = tactix::strategy::action_limits(
      tactix::strategy::Gf3GaussianConsequenceParameters{});
  FILE *records = std::tmpfile();
  assert(records);
  const tactix::gf3_gaussian_live::Request request{
      l.recognition, l.certificate, l.maximum_capture_work,
      l.maximum_revalidation_work, l.maximum_proof_additions,
      l.maximum_proof_literal_occurrences, l.maximum_journal_bytes,
      l.maximum_identity_bytes, l.maximum_external_extent, 1, 1, records, {}};
  tactix::gf3_gaussian_live::Prepared prepared;
  const auto inspected = tactix::gf3_gaussian_live::inspect(kernel, request, prepared);
  std::fclose(records);
  return inspected.status == tactix::gf3_gaussian_live::Status::Prepared
             ? prepared.batch_identity()
             : std::string{};
}

struct Gf3RowCandidate {
  tactix::Gf3GeneralRowActionSelection selection;
  std::vector<int32_t> consequence;
};

// Every planned (outcome, tuple, identity) general-row selection over the
// kernel's captured or the retained owner, in lexicographic order.
inline std::vector<Gf3RowCandidate> gf3_row_candidates(
    tactix::Kernel &kernel, const tactix::Gf3GeneralRowLiveLimits &limits,
    const tactix::Gf3GeneralRowOwner *retained = nullptr) {
  using namespace tactix;
  Gf3GeneralRowOwner owner;
  Gf3GeneralRowLiveEffort effort;
  if (retained) owner = *retained;
  else
    assert(gf3_general_row_live_capture(kernel, limits, owner, effort) ==
           Gf3GeneralRowLiveStatus::Ready);
  std::vector<Gf3RowCandidate> result;
  const auto &outcomes = owner.bridge.gaussian.outcomes;
  for (uint32_t outcome = 0; outcome < outcomes.size(); ++outcome)
    for (uint64_t tuple = 0;
         outcomes[outcome].kind == gf3_gaussian::RowOutcomeKind::Derived; ++tuple) {
      const auto built = gf3_general_row::build(
          owner.affine.theory, owner.bridge, {outcome, tuple}, limits.planner,
          [](uint64_t) { return true; });
      if (built.status == gf3_general_row::Status::NoConsequence ||
          built.status == gf3_general_row::Status::Duplicate)
        continue;
      if (built.status != gf3_general_row::Status::Planned) break;
      Gf3RowCandidate candidate{{outcome, tuple, {}}, built.plan.consequence};
      assert(gf3_general_row_live_identity(owner, {outcome, tuple},
                                           limits.maximum_identity_bytes,
                                           candidate.selection.identity));
      result.push_back(std::move(candidate));
    }
  return result;
}
