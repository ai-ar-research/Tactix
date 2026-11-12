#undef NDEBUG
#include "engine/subsolver.hpp"
#include "rules/sweep/sweep.hpp"

#include <cassert>

extern "C" void tactix_test_fail_allocations(bool);
extern "C" void tactix_test_fail_allocation_after(size_t);

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
  SubDerivation capture;
  capture.steps.push_back(SubStep{3, {0}, {1}, {}});
  std::shared_ptr<SweepProofSteps> program;
  SweepDirProof saved;
  tactix_test_fail_allocations(true);
  const bool shared = sweep_share_steps(capture, program, saved);
  tactix_test_fail_allocations(false);
  assert(!shared && saved.steps_view().empty());
  assert(sweep_share_steps(capture, program, saved));
  assert(saved.steps_view().size() == 1 && saved.steps_view()[0].id == 3);

  SubDerivation later;
  later.steps.push_back(SubStep{4, {2}, {3}, {}});
  SweepDirProof next;
  tactix_test_fail_allocations(true);
  const bool extended = sweep_share_steps(later, program, next);
  tactix_test_fail_allocations(false);
  assert(!extended && next.steps_view().empty());
  assert(saved.steps_view()[0].lits[0] == 0);
  assert(sweep_share_steps(later, program, next));
  assert(next.steps_view()[0].id == 4 && saved.steps_view()[0].id == 3);
  unsigned failures = 0, partial_imports = 0;
  bool completed = false;
  for (size_t budget = 0; budget < 96; ++budget) {
    auto pool = std::make_shared<SweepProofSteps>();
    SubDerivation base;
    base.steps.push_back(SubStep{3, {0}, {1}, {}});
    SweepDirProof original;
    assert(sweep_share_steps(base, pool, original));
    SubDerivation source;
    for (uint32_t id = 3; id < 23; ++id)
      source.steps.push_back(SubStep{id, {id + 4}, {id - 1}, {}});
    source.steps.push_back(SubStep{4096, {30}, {22}, {}});
    auto attempt = source;
    SweepDirProof result;
    tactix_test_fail_allocation_after(budget);
    const bool ok = sweep_share_steps(attempt, pool, result);
    tactix_test_fail_allocations(false);
    assert(original.steps_view()[0].lits[0] == 0);
    if (ok) {
      assert(result.steps_view().size() == source.steps.size());
      completed = true;
      break;
    }
    ++failures;
    partial_imports += pool->steps.size() > 1;
    assert(result.steps_view().empty());
    // Partial import moved some payloads; a retry exports a fresh derivation.
    auto retry = source;
    assert(sweep_share_steps(retry, pool, result));
    const auto selected = result.steps_view();
    assert(selected.size() == source.steps.size());
    for (size_t i = 0; i < source.steps.size(); ++i)
      assert(selected[i].id == source.steps[i].id &&
             selected[i].lits == source.steps[i].lits &&
             selected[i].chain == source.steps[i].chain);
    assert(original.steps_view()[0].lits[0] == 0);
  }
  assert(completed && failures > 4 && partial_imports);
  return 0;
}
