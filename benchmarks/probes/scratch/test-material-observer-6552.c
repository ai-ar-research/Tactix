#define NDEBUG 1
#include "internal.h"
#include "inline.h"
#include "import.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
  assert(argc == 2);
  setenv("KISSAT_SUBSUME_MATERIAL_TRACE", argv[1], 1);
  setenv("SAT_SUBSUME_MATERIAL_FROM", "2", 1);
  setenv("SAT_SUBSUME_MATERIAL_TO", "3", 1);
  setenv("SAT_SUBSUME_MATERIAL_CONFLICT_TO", "9", 1);
  kissat *s = kissat_init();
  kissat *solver = s; /* native literal macros capture this exact identifier */
  unsigned a = kissat_import_literal(s, 17), b = kissat_import_literal(s, 23);
  s->statistics.conflicts = 8;
  kissat_mark_added_literal_with_source(s, a, "below");
  s->flags[IDX(a)].subsume = false;
  s->statistics.conflicts = 9;
  s->limits.eliminate.variables.subsume = 1;
  kissat_mark_added_literal_with_source(s, NOT(a), "selected_first");
  kissat_mark_added_literal_with_source(s, a, "repeat");
  s->flags[IDX(a)].subsume = false;
  s->flags[IDX(a)].subsume = true;
  kissat_mark_added_literal_with_source(s, a, "rearm_repeat");
  s->flags[IDX(b)].fixed = true;
  kissat_mark_added_literal_with_source(s, b, "selected_last");
  s->flags[IDX(b)].subsume = false;
  kissat_mark_added_literal_with_source(s, b, "above");
  assert(s->statistics.variables_subsume == 4);
  s->statistics.variables_subsume = 3;
  s->statistics.conflicts = 10;
  kissat snapshot = *s;
  flags before = s->flags[IDX(b)];
  kissat_event_trace_subsume_material(s, b, "past_conflict");
  assert(!memcmp(s, &snapshot, sizeof snapshot));
  assert(!memcmp(s->flags + IDX(b), &before, sizeof before));
  FILE *in = fopen(argv[1], "r");
  assert(in);
  char line[1024];
  assert(fgets(line, sizeof line, in));
  assert(!strcmp(line, "kissat\tconflicts=9\tdecisions=0\tcount=2\tsaved=1\texternal=-17\tsource=selected_first\tbefore=0\tafter=1\tfixed=0\tactive=0\tprobing=0\n"));
  assert(fgets(line, sizeof line, in));
  assert(!strcmp(line, "kissat\tconflicts=9\tdecisions=0\tcount=3\tsaved=1\texternal=23\tsource=selected_last\tbefore=0\tafter=1\tfixed=1\tactive=0\tprobing=0\n"));
  assert(!fgets(line, sizeof line, in));
  fclose(in);
  kissat_release(s);
  puts("PASS actual producer callback: inclusive counters/conflict, no repeat/rearm rows, signed export, fixed flag and no observer mutation");
  return 0;
}
