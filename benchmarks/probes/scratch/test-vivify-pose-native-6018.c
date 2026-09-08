/* Match the Release library's internal layout; fixed callback control only. */
#ifndef NDEBUG
#define NDEBUG
#endif
#include "internal.h"
#include "inline.h"
#include "eventtrace.h"
#include "kissat.h"
#include <stdlib.h>
int main(int argc, char **argv) {
  if (argc != 3) return 3;
  setenv("SAT_VIVIFY_POSE_TRACE", argv[1], 1);
  setenv("SAT_VIVIFY_POSE_LITS", argv[2], 1);
  setenv("SAT_VIVIFY_POSE_CONFLICT", "1", 1);
  kissat *solver = kissat_init();
  const int input[] = {1, -2, 3, 0, 1, 2, 4, 0};
  for (unsigned i = 0; i < sizeof input / sizeof *input; ++i) kissat_add(solver, input[i]);
  clause *first = (clause *)BEGIN_STACK(solver->arena);
  clause *candidate = kissat_next_clause(first);
  reference candidate_ref = kissat_reference_clause(solver, candidate);
  kissat_vivify_pose_candidate(solver, "vivify_candidate", 0, candidate, 4, 20, 1);
  solver->statistics.conflicts = 1;
  kissat_vivify_pose_candidate(solver, "vivify_candidate", 0, candidate, 4, 20, 1);
  unsigned tmp = first->lits[0]; first->lits[0] = first->lits[1]; first->lits[1] = tmp;
  kissat_vivify_pose_candidate(solver, "vivify_candidate_end", 0, candidate, 5, 20, 1);
  kissat_mark_clause_as_garbage(solver, first);
  kissat_vivify_pose_candidate(solver, "vivify_candidate", 1, candidate, 5, 20, 1);
  const int replacement[] = {3, 1, -2, 0};
  for (unsigned i = 0; i < 4; ++i) kissat_add(solver, replacement[i]);
  candidate = kissat_dereference_clause(solver, candidate_ref);
  kissat_vivify_pose_candidate(solver, "vivify_candidate_end", 1, candidate, 7, 20, 1);
  kissat_vivify_pose_candidate(solver, "vivify_candidate", 0, candidate, 7, 25, 2);
  kissat_release(solver); return 0;
}
