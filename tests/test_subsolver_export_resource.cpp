#undef NDEBUG
#include "engine/subsolver.hpp"

#include <cassert>

extern "C" void tactix_test_fail_allocations(bool);

using namespace tactix;

int main() {
  SubSolver solver;
  solver.reset(1);
  solver.add_clause({sub_lit(0, false)});
  solver.add_clause({sub_lit(0, true)});
  assert(solver.solve({}, 0) == SubResult::Unsat);

  SubDerivation proof;
  tactix_test_fail_allocations(true);
  const bool exported = solver.derivation(proof, true);
  tactix_test_fail_allocations(false);
  assert(!exported && proof.resource_exhausted);
  assert(proof.steps.empty() && proof.core.empty() &&
         proof.core_clauses.empty());
  assert(solver.refutation_target());

  assert(solver.derivation(proof, true));
  assert(!proof.resource_exhausted && !proof.steps.empty());
  return 0;
}
