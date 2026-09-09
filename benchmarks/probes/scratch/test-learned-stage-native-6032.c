/* Match the Release library's internal layout. No real-instance solve. */
#ifndef NDEBUG
#define NDEBUG
#endif
#include "internal.h"
#include "inline.h"
#include "eventtrace.h"
#include "kissat.h"
#include <stdlib.h>
int main(int argc, char **argv) {
  if (argc != 4) return 3;
  setenv("SAT_LEARNED_STAGE_TRACE", argv[1], 1);
  setenv("SAT_LEARNED_STAGE_FROM", argv[2], 1); setenv("SAT_LEARNED_STAGE_TO", argv[3], 1);
  kissat *solver = kissat_init();
  for (int i = 1; i <= 2048; ++i) kissat_add(solver, i);
  kissat_add(solver, 0);
  CLEAR_STACK(solver->clause);
  PUSH_STACK(solver->clause, LIT(0)); PUSH_STACK(solver->clause, NOT(LIT(1))); PUSH_STACK(solver->clause, LIT(2));
  kissat_event_trace_clause(solver, "learned_raw");
  solver->statistics.conflicts = 1; solver->statistics.decisions = 5;
  kissat_event_trace_clause(solver, "learned_raw");
  unsigned tmp = solver->clause.begin[0]; solver->clause.begin[0] = solver->clause.begin[1]; solver->clause.begin[1] = tmp;
  kissat_event_trace_clause(solver, "learned_sorted");
  solver->statistics.conflicts = 2; CLEAR_STACK(solver->clause);
  PUSH_STACK(solver->clause, LIT(0)); PUSH_STACK(solver->clause, LIT(2));
  kissat_event_trace_clause(solver, "learned_shrunk"); kissat_event_trace_learned(solver);
  CLEAR_STACK(solver->clause);
  for (unsigned i = 0; i < 2048; ++i) PUSH_STACK(solver->clause, LIT(i));
  kissat_event_trace_clause(solver, "learned_raw");
  CLEAR_STACK(solver->clause); kissat_event_trace_clause(solver, "learned_shrunk");
  solver->statistics.conflicts = 3; kissat_event_trace_learned(solver);
  kissat_release(solver); return 0;
}
