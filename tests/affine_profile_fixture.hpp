#pragma once
// CertifiedBasisV2 profiles shared by affine and persistent matrix tests.
#include "strategy/theory_action_parameters.hpp"

#include <cassert>

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

inline tactix::strategy::AffineConsequenceParameters matrix_affine_parameters() {
  auto result = parameters();
  result.maximum_clauses = 10000;
  result.maximum_snapshot_literals = 100000;
  result.maximum_groups = 1000;
  result.maximum_patterns = 10000;
  result.maximum_elimination_work = 10000000;
  result.maximum_arity = 8;
  result.maximum_intermediate_width = 32;
  result.maximum_proof_additions = 1000000;
  result.maximum_proof_deletions = 1000000;
  result.maximum_journal_bytes = 10000000;
  result.basis_maximum_intermediate_width = 32;
  result.maximum_external_extent = 100000;
  result.maximum_session_keys = 10000;
  assert(result.valid());
  return result;
}
