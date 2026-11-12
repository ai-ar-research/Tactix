#pragma once
// One-hot GF(3) domains and affine-equation clauses, the parsed formula and a
// streaming-PBP proof binding, shared by the gf3_affine tests.
#ifdef NDEBUG
#error "gf3_affine_support.hpp asserts have side effects; #undef NDEBUG first"
#endif

#include "dimacs_text.hpp"
#include "kernel/kernel.hpp"
#include "proof/proof.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <utility>
#include <vector>

using Gf3Clauses = std::vector<std::vector<int32_t>>;

// Emit one domain over three selection literals: ALO, then the AMO binaries.
template <class Emit>
void gf3_domain_clauses(const std::vector<int32_t> &values, Emit emit) {
  assert(values.size() == 3);
  emit(values);
  for (uint8_t first = 0; first < 3; ++first)
    for (uint8_t second = first + 1; second < 3; ++second)
      emit({-values[first], -values[second]});
}

// Emit one clause per domain tuple violating sum(coefficient * value) == rhs
// (mod 3).
template <class Emit>
void gf3_equation_clauses(const Gf3Clauses &values,
                          const std::vector<uint32_t> &domains,
                          const std::vector<uint8_t> &coefficients, uint8_t rhs,
                          Emit emit) {
  assert(domains.size() >= 2 && domains.size() == coefficients.size());
  uint64_t tuples = 1;
  for (size_t i = 0; i < domains.size(); ++i) tuples *= 3;
  for (uint64_t tuple = 0; tuple < tuples; ++tuple) {
    uint64_t rest = tuple;
    uint8_t sum = 0;
    std::vector<int32_t> clause;
    for (size_t i = 0; i < domains.size(); ++i) {
      const uint8_t value = static_cast<uint8_t>(rest % 3);
      rest /= 3;
      sum = static_cast<uint8_t>((sum + coefficients[i] * value) % 3);
      clause.push_back(-values[domains[i]][value]);
    }
    if (sum != rhs) emit(std::move(clause));
  }
}

// Domains d = 0..n-1 over variables 3d+1..3d+3, then one three-domain
// equation x + y + z = 0 per entry of equations. Incomplete drops the final
// forbidding clause.
inline Gf3Clauses gf3_formula(uint32_t domains,
                              const std::vector<std::vector<uint32_t>> &equations,
                              bool complete = true) {
  Gf3Clauses result, values;
  const auto emit = [&](std::vector<int32_t> clause) {
    result.push_back(std::move(clause));
  };
  for (uint32_t domain = 0; domain < domains; ++domain) {
    const int32_t first = static_cast<int32_t>(3 * domain + 1);
    values.push_back({first, first + 1, first + 2});
    gf3_domain_clauses(values.back(), emit);
  }
  for (const auto &equation : equations)
    gf3_equation_clauses(values, equation, {1, 1, 1}, 0, emit);
  if (!complete) result.pop_back();
  return result;
}

inline tactix::Kernel gf3_parse(uint32_t domains, const Gf3Clauses &clauses) {
  return parse_dimacs_text(dimacs_text(3 * domains, clauses),
                           ParsedRoot::EmptyTrail);
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
