#pragma once
// Clause input, XOR fixtures and journal streams shared by the affine tests.
#ifdef NDEBUG
#error "affine_formula_support.hpp asserts have side effects; #undef NDEBUG first"
#endif

#include "kernel/kernel.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/semantic_extension/affine_image.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <utility>
#include <vector>

using AffineCnf = std::vector<std::vector<int32_t>>;

// An irredundant input clause (root unit, binary or large) with the next id.
inline void add_clause(tactix::Kernel &kernel, const std::vector<int32_t> &external) {
  std::vector<tactix::Lit> clause;
  for (const int32_t literal : external) {
    const tactix::Lit internal = tactix::kernel_internalize(kernel, literal);
    assert(internal != tactix::INVALID_LIT);
    clause.push_back(internal);
  }
  const uint64_t id = ++kernel.next_clause_id;
  if (clause.size() == 1) {
    assert(!kernel.value(clause[0]));
    kernel.unit_ids[tactix::var_of(clause[0])] = id;
    tactix::kernel_assign_unit(kernel, clause[0]);
  } else if (clause.size() == 2) {
    tactix::kernel_add_binary_clause(kernel, clause[0], clause[1], id);
  } else {
    tactix::kernel_add_large_clause(kernel, clause.data(), clause.size(), false,
                                    0, id);
  }
}

// The complete clausal form of one XOR.
inline void add_equation(tactix::Kernel &kernel, const std::vector<int32_t> &variables,
                         bool rhs) {
  for (const auto &clause :
       tactix::affine_semantic_extension::clausal_form(variables, rhs))
    add_clause(kernel, clause);
}

// The concatenated clausal forms of several XORs.
inline AffineCnf xor_cnf(
    std::initializer_list<std::pair<std::vector<int32_t>, bool>> equations) {
  AffineCnf result;
  for (const auto &[variables, rhs] : equations)
    for (auto &clause : tactix::affine_semantic_extension::clausal_form(variables, rhs))
      result.push_back(std::move(clause));
  return result;
}

// x1..x5 = 0 and x1..x4 + x6 = 1, whose sum is x5 + x6 = 1. Incomplete drops
// the first XOR's final clause.
inline AffineCnf affine_formula(bool complete = true) {
  auto first = xor_cnf({{{1, 2, 3, 4, 5}, false}});
  const auto second = xor_cnf({{{1, 2, 3, 4, 6}, true}});
  if (!complete) first.pop_back();
  first.insert(first.end(), second.begin(), second.end());
  return first;
}

inline void install(tactix::Kernel &kernel, const AffineCnf &cnf,
                    uint32_t variables = 6) {
  tactix::kernel_reserve_vars(kernel, variables);
  for (const auto &clause : cnf) add_clause(kernel, clause);
  assert(tactix::kernel_propagation_fixpoint(kernel));
}

// x1 + x3 + x4 = 0 and x2 + x3 + x4 + x5 = 0: after x1 = x2 = 0 their sum
// implies x5 = 0 though neither row is unit.
inline void install_implication_formula(tactix::Kernel &kernel) {
  tactix::kernel_reserve_vars(kernel, 6);
  add_equation(kernel, {1, 3, 4}, false);
  add_equation(kernel, {2, 3, 4, 5}, false);
  assert(tactix::kernel_propagation_fixpoint(kernel));
}

// Decide each literal in turn, propagating to a conflict-free fixpoint.
inline void decide_all(tactix::Kernel &kernel, std::initializer_list<int32_t> decisions) {
  for (const int32_t decision : decisions) {
    tactix::kernel_assign_decision(kernel, tactix::kernel_internalize(kernel, decision));
    assert(tactix::propagate_apply(kernel) == tactix::ApplyResult::Ok);
  }
}

inline FILE *temporary_records() {
  FILE *records = std::tmpfile();
  assert(records);
  return records;
}

// Open a proof at path and bind it, with the variable maps, to kernel.
inline void bind_proof(tactix::Proof &proof, tactix::Kernel &kernel, const char *path,
                       tactix::ProofFormat format = tactix::ProofFormat::DsrAscii) {
  assert(proof.open(path, format));
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  kernel.proof = &proof;
}
