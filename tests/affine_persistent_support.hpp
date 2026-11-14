#pragma once
// The proof-bound persistent affine-matrix fixture shared by the lifecycle and
// actual-geometry tests.
#include "affine_test_support.hpp"
#include "kernel/kernel.hpp"
#include "proof/proof.hpp"
#include "rules/semantic_extension/affine_matrix_search.hpp"
#include "strategy/theory_action_parameters.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

// Reserve variables and add original; open a proof at path seeded with
// original under ids 1..n and arm its population shadow; attach the returned
// journal and session to k.
inline FILE *attach_persistent(tactix::Kernel &k, tactix::Proof &proof,
                               tactix::AffineMatrixSearchSession &session,
                               uint32_t variables,
                               const std::vector<std::vector<int32_t>> &original,
                               const std::string &path,
                               tactix::ProofFormat format = tactix::ProofFormat::DsrAscii) {
  tactix::kernel_reserve_vars(k, variables);
  for (const auto &clause : original) add(k, clause);
  FILE *journal = std::tmpfile();
  assert(journal);
  k.structural_provenance = journal;
  assert(proof.open(path.c_str(), format));
  proof.bind_variable_map(&k.internal_to_external);
  proof.bind_external_variable_map(&k.external_to_internal);
  for (size_t i = 0; i < original.size(); ++i) {
    const auto body = physical(k, original[i]);
    proof.seed_population(i + 1, body.data(), body.size());
  }
  proof.arm_population_shadow();
  proof.finish_input_transaction();
  k.proof = &proof;
  k.affine_matrix_search_session = &session;
  return journal;
}
