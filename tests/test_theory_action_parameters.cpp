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
