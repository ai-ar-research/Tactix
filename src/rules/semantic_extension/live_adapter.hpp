#pragma once

#include "kernel/kernel.hpp"
#include "proof/proof.hpp"
#include "rules/semantic_extension/interruptible_work.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace tactix {

// A live adapter publishes only into clausal proof formats and from a `ready`
// root. Polls termination only when the root is not ready.
template <class Status, class Ready>
bool live_admitted(const Kernel &kernel, Ready &&ready, Status &status) {
  if (kernel.proof && kernel.proof->startup_pb_written()) {
    status = Status::UnsupportedProof;
    return false;
  }
  return root_admitted(kernel, ready, status);
}

// Appends the external spelling of a physical clause whose variables are all
// live and unassigned; false at the first retired or assigned literal.
inline bool unassigned_external_clause(const Kernel &kernel,
                                       const Lit *literals, uint32_t size,
                                       std::vector<int32_t> &external) {
  external.reserve(size);
  for (uint32_t index = 0; index < size; ++index) {
    if (kernel_var_retired(kernel, var_of(literals[index])) ||
        kernel.value(literals[index]))
      return false;
    external.push_back(kernel_externalize(kernel, literals[index]));
  }
  return true;
}

// Whether a physical clause over live variables spells `expected` up to
// literal order; assignments do not change a retained source's identity.
inline bool live_clause_matches(const Kernel &kernel, const Lit *literals,
                                uint32_t size,
                                const std::vector<int32_t> &expected) {
  if (size != expected.size()) return false;
  std::vector<int32_t> external;
  external.reserve(size);
  for (uint32_t index = 0; index < size; ++index) {
    if (kernel_var_retired(kernel, var_of(literals[index]))) return false;
    external.push_back(kernel_externalize(kernel, literals[index]));
  }
  std::vector<int32_t> wanted = expected;
  std::sort(external.begin(), external.end());
  std::sort(wanted.begin(), wanted.end());
  return external == wanted;
}

// The internal literals of a planned external clause at a live adapter's
// commit boundary. Preflight has already authenticated every variable, so an
// unmapped or retired literal is an atomicity violation and aborts.
inline std::vector<Lit> internal_clause_or_abort(
    const Kernel &kernel, const std::vector<int32_t> &clause) {
  std::vector<Lit> literals;
  literals.reserve(clause.size());
  for (int32_t external : clause) {
    const Lit literal = kernel_internalize(kernel, external);
    if (literal == INVALID_LIT || kernel_var_retired(kernel, var_of(literal)))
      std::abort();
    literals.push_back(literal);
  }
  return literals;
}

// Installs a published redundant clause of at least two literals: binary, or
// large with glue equal to its size.
inline void install_redundant_clause(Kernel &kernel,
                                     const std::vector<Lit> &literals,
                                     uint64_t id) {
  if (literals.size() == 2)
    kernel_add_binary_clause(kernel, literals[0], literals[1], id);
  else
    kernel_add_large_clause(kernel, literals.data(), literals.size(), true,
                            literals.size(), id);
}

} // namespace tactix
