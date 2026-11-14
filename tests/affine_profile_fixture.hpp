#pragma once
// Shared CertifiedBasisV2 profile for persistent matrix and online-action tests.
#include "strategy/theory_action_parameters.hpp"

#include <cassert>

inline tactix::strategy::AffineConsequenceParameters matrix_affine_parameters() {
  tactix::strategy::AffineConsequenceParameters result;
  result.maximum_clauses = 10000;
  result.maximum_snapshot_literals = 100000;
  result.maximum_snapshot_work = 1000000;
  result.maximum_groups = 1000;
  result.maximum_patterns = 10000;
  result.maximum_elimination_work = 10000000;
  result.maximum_arity = 8;
  result.maximum_intermediate_width = 32;
  result.maximum_sources = 32;
  result.maximum_output_width = 4;
  result.maximum_proof_additions = 1000000;
  result.maximum_proof_deletions = 1000000;
  result.maximum_journal_bytes = 10000000;
  result.semantics = tactix::strategy::AffineConsequenceSemantics::CertifiedBasisV2;
  result.basis_maximum_work = 10000000;
  result.basis_maximum_intermediate_width = 32;
  result.basis_maximum_sources = 32;
  result.basis_maximum_premises = 10000;
  result.basis_maximum_nodes = 10000;
  result.basis_maximum_direct_width = 3;
  result.maximum_external_extent = 100000;
  result.maximum_session_keys = 10000;
  result.maximum_session_work = 1000000;
  assert(result.valid());
  return result;
}
