#pragma once
// Affine-consequence parameters and clause input shared by the affine tests.
#include "kernel/kernel.hpp"
#include "strategy/theory_action_parameters.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

// Ample, valid CertifiedBasisV2 bounds.
inline tactix::strategy::AffineConsequenceParameters parameters() {
  tactix::strategy::AffineConsequenceParameters p;
  p.maximum_clauses = 1000;
  p.maximum_snapshot_literals = 10000;
  p.maximum_snapshot_work = 1000000;
  p.maximum_groups = 100;
  p.maximum_patterns = 1000;
  p.maximum_elimination_work = 1000000;
  p.maximum_arity = 4;
  p.maximum_intermediate_width = 16;
  p.maximum_sources = 32;
  p.maximum_output_width = 4;
  p.maximum_proof_additions = 100000;
  p.maximum_proof_deletions = 100000;
  p.maximum_journal_bytes = 1000000;
  p.semantics = tactix::strategy::AffineConsequenceSemantics::CertifiedBasisV2;
  p.basis_maximum_work = 10000000;
  p.basis_maximum_intermediate_width = 16;
  p.basis_maximum_sources = 32;
  p.basis_maximum_premises = 10000;
  p.basis_maximum_nodes = 10000;
  p.basis_maximum_direct_width = 3;
  p.maximum_external_extent = 128;
  p.maximum_session_keys = 1000;
  p.maximum_session_work = 1000000;
  assert(p.valid());
  return p;
}

// The current internal literals of external clause literals.
inline std::vector<tactix::Lit> physical(tactix::Kernel &k,
                                         const std::vector<int32_t> &clause) {
  std::vector<tactix::Lit> result;
  for (int32_t x : clause) {
    const tactix::Lit l = tactix::kernel_internalize(k, x);
    assert(l != tactix::INVALID_LIT);
    result.push_back(l);
  }
  return result;
}

// Add an irredundant binary or large input clause with the next clause id.
inline void add(tactix::Kernel &k, const std::vector<int32_t> &clause) {
  const auto body = physical(k, clause);
  const uint64_t id = ++k.next_clause_id;
  if (body.size() == 2)
    tactix::kernel_add_binary_clause(k, body[0], body[1], id);
  else
    tactix::kernel_add_large_clause(k, body.data(), body.size(), false, 0, id);
}
