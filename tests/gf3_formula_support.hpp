#pragma once
// GF(3) fixtures: one-hot domains, the forbidden-tuple clauses of linear
// equations over them, kernels holding them, and ample action limits.
#include "gf3_test_support.hpp"
#include "frontend/parse.hpp"
#include "rules/semantic_extension/gf3_gaussian_action.hpp"
#include "rules/semantic_extension/gf3_general_row_action.hpp"

#include <cassert>
#include <cstdint>
#include <utility>
#include <vector>

struct Gf3Formula {
  std::vector<std::vector<int32_t>> clauses;
  // The same clauses as recognition premises; occurrence i has proof ID 9000 + i.
  std::vector<tactix::gf3_affine::Premise> source;
  std::vector<std::vector<int32_t>> values;

  void add(std::vector<int32_t> clause) {
    const uint64_t occurrence = source.size() + 1;
    clauses.push_back(clause);
    source.push_back({occurrence, std::move(clause), 9000 + occurrence});
  }

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

// Domains x, y, z over variables 1..9 with x + y = 0 and y + z = 0.
inline Gf3Formula gf3_chain() {
  Gf3Formula formula;
  for (int32_t first = 1; first <= 7; first += 3)
    formula.domain({first, first + 1, first + 2});
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
  Gf3Formula formula;
  for (int32_t first = 1; first <= 10; first += 3)
    formula.domain({first, first + 1, first + 2});
  formula.equation({0, 1, 3}, {1, 1, 1}, 0);
  formula.equation({1, 2, 3}, {1, 1, 1}, 0);
  return formula;
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
inline tactix::Kernel gf3_parsed_kernel(const Gf3Formula &formula) {
  tactix::Kernel kernel;
  for (const auto &external : formula.clauses) {
    std::vector<tactix::Lit> clause;
    for (int32_t literal : external) clause.push_back(tactix::internalize(literal));
    tactix::ingest_clause(kernel, clause);
  }
  tactix::finish_input(kernel);
  return kernel;
}

inline tactix::Gf3GaussianActionLimits gf3_gaussian_action_limits(
    const tactix::strategy::Gf3GaussianConsequenceParameters &p = {}) {
  return {{p.maximum_source_clauses, p.maximum_source_literals,
           p.maximum_domains, p.maximum_equations,
           p.maximum_equation_arity, p.maximum_tuples_per_equation},
          {{p.maximum_bridge_work, p.maximum_bridge_source_clauses,
            p.maximum_bridge_source_literals, p.maximum_bridge_domains,
            p.maximum_bridge_equations, p.maximum_bridge_terms,
            p.maximum_authentication_references,
            p.maximum_references_per_equation,
            {p.maximum_gaussian_work, p.maximum_fill_entries,
             p.maximum_intermediate_width, p.maximum_provenance_terms,
             p.maximum_derivations, p.maximum_retained_plan_bytes}},
           p.maximum_certificate_work, p.maximum_events,
           p.maximum_event_literals, p.maximum_active_clauses,
           p.maximum_retained_source_references, p.maximum_event_width,
           p.maximum_branch_domains, p.maximum_consequences},
          p.maximum_capture_work, p.maximum_revalidation_work,
          p.maximum_proof_additions, p.maximum_proof_literal_occurrences,
          p.maximum_journal_bytes, p.maximum_identity_bytes,
          p.maximum_external_extent, p.maximum_session_keys,
          p.maximum_session_work, p.maximum_session_identity_bytes};
}

inline tactix::Gf3GeneralRowActionLimits gf3_general_row_action_limits(
    const tactix::strategy::Gf3GeneralRowConsequenceParameters &p = {}) {
  const auto g = gf3_gaussian_action_limits(p.gaussian);
  const auto &c = g.certificate;
  return {{g.recognition,
           {c.bridge, c.maximum_work, c.maximum_events,
            c.maximum_event_literals, c.maximum_active_clauses,
            c.maximum_retained_source_references, p.maximum_row_tuples,
            c.maximum_event_width, c.maximum_branch_domains,
            p.maximum_row_width},
           g.maximum_capture_work, p.maximum_duplicate_scan_work,
           g.maximum_revalidation_work, g.maximum_proof_additions,
           g.maximum_proof_literal_occurrences, p.maximum_arena_words,
           g.maximum_journal_bytes, g.maximum_identity_bytes,
           g.maximum_external_extent},
          g.maximum_session_keys, g.maximum_session_work,
          g.maximum_session_identity_bytes};
}

#ifdef TACTIX_H_INCLUDED
// The same Gaussian bounds through the C ABI.
inline tactix_online_gf3_gaussian_consequence_parameters gf3_c_gaussian(
    const tactix::strategy::Gf3GaussianConsequenceParameters &p) {
  return {p.maximum_source_clauses, p.maximum_source_literals,
          p.maximum_domains, p.maximum_equations, p.maximum_equation_arity,
          p.maximum_tuples_per_equation, p.maximum_bridge_work,
          p.maximum_bridge_source_clauses, p.maximum_bridge_source_literals,
          p.maximum_bridge_domains, p.maximum_bridge_equations,
          p.maximum_bridge_terms, p.maximum_authentication_references,
          p.maximum_references_per_equation, p.maximum_gaussian_work,
          p.maximum_fill_entries, p.maximum_intermediate_width,
          p.maximum_provenance_terms, p.maximum_derivations,
          p.maximum_retained_plan_bytes, p.maximum_certificate_work,
          p.maximum_events, p.maximum_event_literals, p.maximum_active_clauses,
          p.maximum_retained_source_references, p.maximum_event_width,
          p.maximum_branch_domains, p.maximum_consequences,
          p.maximum_capture_work, p.maximum_revalidation_work,
          p.maximum_proof_additions, p.maximum_proof_literal_occurrences,
          p.maximum_journal_bytes, p.maximum_identity_bytes,
          p.maximum_external_extent, p.maximum_session_keys,
          p.maximum_session_work, p.maximum_session_identity_bytes};
}
#endif
