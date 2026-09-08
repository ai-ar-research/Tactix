#include "internal.h"
#include "inlineheap.h"
#include "import.h"
#include "eventtrace.h"
#include "kissat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main (int argc, char **argv) {
  if (argc != 3) return 3;
  kissat *solver = kissat_init ();
  for (int i = 1; i <= 4; ++i) (void) kissat_import_literal (solver, i);
  heap *h = &solver->schedule;
  // The elimination caller reserves the schedule before HEAP_IMPORT; this
  // internal API does not allocate capacity when importing a scored member.
  kissat_resize_heap (solver, h, solver->vars);
  for (unsigned i = 0; i != 4; ++i) {
    kissat_update_heap (solver, h, i, 4.0-i);
    kissat_push_heap (solver, h, i);
  }
  setenv ("KISSAT_BVE_HEAP_TRACE", argv[1], 1);
  setenv ("SAT_BVE_HEAP_CONFLICT", "271079", 1);
  setenv ("SAT_BVE_HEAP_SUBJECTS", argv[2], 1);
  kissat_event_trace_bve_heap (solver, "initial", h, UINT_MAX);
  solver->statistics.conflicts = 271079;
  if (!strcmp (argv[2], "missed-initial")) {
    kissat_event_trace_bve_heap (solver, "pop", h, 0);
    return 4;
  }
  kissat_event_trace_bve_heap (solver, "initial", h, UINT_MAX);
  const unsigned popped = kissat_pop_max_heap (solver, h);
  if (popped != 0) return 5;
  kissat_event_trace_bve_heap (solver, "pop", h, popped);
  // Exercise remapping on a new round using the actual export table. This
  // is an observer fixture, not a formula transformation or solver call.
  POKE_STACK (solver->export, 0, 3);
  POKE_STACK (solver->export, 2, 1);
  kissat_event_trace_bve_heap (solver, "initial", h, UINT_MAX);
  kissat_release (solver);
  return 0;
}
