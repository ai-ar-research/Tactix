/* Match the NDEBUG-dependent native internal layout of the Release library. */
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
  setenv("SAT_CLAUSE_POSE_SERIES_PATH", argv[1], 1);
  setenv("SAT_CLAUSE_POSE_SERIES_LITS", argv[2], 1);
  setenv("SAT_CLAUSE_POSE_SERIES_FROM", "1", 1);
  setenv("SAT_CLAUSE_POSE_SERIES_TO", "1", 1);
  setenv("SAT_CLAUSE_POSE_SERIES_NAMES", "vivify", 1);
  kissat *s = kissat_init();
  const int lits[] = {1, -2, 3, 0, 1, -2, 3, 0, 1, 2, 4, 0};
  for (unsigned i = 0; i < sizeof lits / sizeof *lits; ++i) kissat_add(s, lits[i]);
  kissat_event_trace_eliminate_round_boundary(s, "vivify", "begin");
  s->statistics.conflicts = 1;
  kissat_event_trace_eliminate_round_boundary(s, "bve", "begin");
  kissat_event_trace_eliminate_round_boundary(s, "vivify", "begin");
  clause *first = (clause *)BEGIN_STACK(s->arena);
  unsigned tmp = first->lits[0]; first->lits[0] = first->lits[1]; first->lits[1] = tmp;
  kissat_event_trace_eliminate_round_boundary(s, "vivify", "end");
  unsigned deleted = 0;
  kissat *solver = s;
  for (all_clauses(c)) {
    if (deleted++ < 2) kissat_mark_clause_as_garbage(s, c);
  }
  kissat_event_trace_eliminate_round_boundary(s, "vivify", "begin");
  s->statistics.conflicts = 2;
  kissat_event_trace_eliminate_round_boundary(s, "vivify", "end");
  kissat_release(s);
  return 0;
}
