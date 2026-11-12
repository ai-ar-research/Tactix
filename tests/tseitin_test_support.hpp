#pragma once
// XOR-equation fixtures shared by the Tseitin-cycle tests.
#include "kernel/kernel.hpp"
#include "proof/proof.hpp"
#include "rules/lucky/parity_certificate.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace tseitin_test {

struct Equation {
  bool rhs = false;
  std::vector<int32_t> variables;
};

// An odd-charge triangle: the smallest cycle with a breaking unit.
inline const std::vector<Equation> triangle = {
    {true, {1, 3}}, {false, {1, 2}}, {false, {2, 3}}};
// Width-three equations on K4 edges.
inline const std::vector<Equation> width_three = {
    {true, {1, 2, 3}}, {false, {1, 4, 5}},
    {false, {2, 4, 6}}, {false, {3, 5, 6}}};

// Adds the CNF of each equation with variables shifted by offset, each clause
// under a fresh id; reverse flips both clause and literal order.
inline void add_equations(tactix::Kernel &kernel,
                          const std::vector<Equation> &equations,
                          int32_t offset = 0, bool reverse = false) {
  std::vector<std::vector<int32_t>> encoded;
  for (const Equation &equation : equations) {
    std::vector<int32_t> variables = equation.variables;
    for (int32_t &variable : variables) variable += offset;
    for (const auto &clause : tactix::parity_certificate::xor_equation(
             variables.data(), variables.size(), equation.rhs))
      encoded.emplace_back(clause.lits.begin(), clause.lits.begin() + clause.size);
  }
  if (reverse) std::reverse(encoded.begin(), encoded.end());
  for (auto clause : encoded) {
    if (reverse) std::reverse(clause.begin(), clause.end());
    if (clause.size() == 2) {
      tactix::kernel_add_binary_clause(kernel, tactix::internalize(clause[0]),
                                       tactix::internalize(clause[1]),
                                       ++kernel.next_clause_id);
      continue;
    }
    std::vector<tactix::Lit> literals;
    for (int32_t literal : clause) literals.push_back(tactix::internalize(literal));
    tactix::kernel_add_large_clause(kernel, literals.data(), literals.size(),
                                    false, 0, ++kernel.next_clause_id);
  }
}

inline tactix::Kernel formula(const std::vector<Equation> &equations,
                              uint32_t variables, int32_t offset = 0) {
  tactix::Kernel kernel;
  tactix::kernel_reserve_vars(kernel, variables);
  add_equations(kernel, equations, offset);
  return kernel;
}

// Interrupts at the stop-th poll.
struct InterruptState {
  uint64_t calls = 0;
  uint64_t stop = UINT64_MAX;
};

inline bool interrupt_after(void *opaque) {
  auto &state = *static_cast<InterruptState *>(opaque);
  return ++state.calls >= state.stop;
}

// Binds proof to kernel's variable maps and attaches it.
inline void attach(tactix::Proof &proof, tactix::Kernel &kernel) {
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  kernel.proof = &proof;
}

} // namespace tseitin_test
