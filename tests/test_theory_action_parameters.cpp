#undef NDEBUG
#include "kernel/kernel.hpp"
#include "strategy/theory_action_parameters.hpp"

#include <cassert>

using namespace tactix;
using namespace tactix::strategy;

int main() {
  Kernel kernel;

  AffineConsequenceParameters affine;
  affine.maximum_clauses = 100;
  affine.maximum_arity = 8;
  affine.maximum_intermediate_width = 16;
  affine.maximum_output_width = 4;
  affine.maximum_proof_additions = 11;
  affine.maximum_proof_deletions = 12;
  affine.consequence = 7;
  assert(affine.valid());
  install_rule_parameters(kernel, affine);
  assert(AffineConsequenceParameters::from_kernel(kernel).consequence == 7);
  assert(AffineConsequenceParameters::from_kernel(kernel)
             .maximum_proof_deletions == 12);
  affine.maximum_output_width = 5;
  assert(!affine.valid());

  AffineConsequenceParameters basis;
  basis.maximum_clauses = 100;
  basis.maximum_snapshot_literals = 1000;
  basis.maximum_snapshot_work = 2000;
  basis.maximum_groups = 100;
  basis.maximum_patterns = 1000;
  basis.maximum_elimination_work = 10000;
  basis.maximum_arity = 8;
  basis.maximum_intermediate_width = 32;
  basis.maximum_sources = 32;
  basis.maximum_output_width = 4;
  basis.maximum_proof_additions = 10000;
  basis.maximum_journal_bytes = 10000;
  basis.semantics = AffineConsequenceSemantics::CertifiedBasisV2;
  basis.basis_maximum_work = 10000;
  basis.basis_maximum_intermediate_width = 32;
  basis.basis_maximum_sources = 32;
  basis.basis_maximum_premises = 1000;
  basis.basis_maximum_nodes = 1000;
  basis.basis_maximum_direct_width = 3;
  basis.maximum_external_extent = 10000;
  basis.maximum_session_keys = 1000;
  basis.maximum_session_work = 10000;
  assert(basis.valid());
  install_rule_parameters(kernel, basis);
  assert(AffineConsequenceParameters::from_kernel(kernel).semantics ==
         AffineConsequenceSemantics::CertifiedBasisV2);
  basis.maximum_session_work = 0;
  assert(!basis.valid());
  basis.maximum_session_work = 10000;
  basis.basis_maximum_direct_width = 1;
  assert(!basis.valid());

  ExactCoverExtensionParameters exact;
  exact.arity = 3;
  exact.maximum_definitions = 9;
  exact.maximum_journal_bytes = 4096;
  assert(exact.valid());
  install_rule_parameters(kernel, exact);
  assert(ExactCoverExtensionParameters::from_kernel(kernel).arity == 3);
  exact.arity = 2;
  assert(!exact.valid());

  IndependentSetCoverParameters independent;
  independent.maximum_vertices = 64;
  independent.maximum_journal_bytes = 8192;
  assert(independent.valid());
  install_rule_parameters(kernel, independent);
  assert(IndependentSetCoverParameters::from_kernel(kernel)
             .maximum_vertices == 64);

  SemanticVariableAdviceParameters advice;
  advice.maximum_variables = 16;
  advice.maximum_priority_sum = 32;
  advice.existing_theory_priority = 1;
  advice.introduced_theory_priority = 2;
  assert(advice.valid());
  install_rule_parameters(kernel, advice);
  assert(SemanticVariableAdviceParameters::from_kernel(kernel)
             .introduced_theory_priority == 2);
}
