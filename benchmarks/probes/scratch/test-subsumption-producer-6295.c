#define NDEBUG 1
#include "internal.h"
#include "inline.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main(void) {
  char path[160]; snprintf(path, sizeof path, "/tmp/native-subsumption-%ld.tsv", (long)getpid());
  setenv("KISSAT_SUBSUMPTION_PRODUCER_TRACE", path, 1);
  setenv("SAT_SUBSUMPTION_PRODUCER_LOW", "100", 1);
  setenv("SAT_SUBSUMPTION_PRODUCER_HIGH", "102", 1);
  kissat *solver = kissat_init();
  kissat_add(solver, 1); kissat_add(solver, 2); kissat_add(solver, 0);
  solver->statistics.variables_subsume = 100;
  solver->flags[0].subsume = false;
  kissat_mark_added_literal(solver, 0);
  assert(solver->statistics.variables_subsume == 101);
  kissat_mark_added_literal(solver, 1);
  assert(solver->statistics.variables_subsume == 101);
  solver->flags[0].subsume = false;
  kissat_mark_added_literal(solver, 1);
  assert(solver->statistics.variables_subsume == 102);
  solver->flags[0].subsume = false;
  kissat_mark_added_literal(solver, 0);
  assert(solver->statistics.variables_subsume == 103);
  kissat_release(solver);
  FILE *in = fopen(path, "r"); assert(in); char line[2048]; unsigned count = 0;
  while (fgets(line, sizeof line, in)) {
    assert(strstr(line, count ? "\tcount=102\tliteral=-1\tsource=main" : "\tcount=101\tliteral=1\tsource=main"));
    ++count;
  }
  assert(count == 2); fclose(in); assert(remove(path) == 0);
  puts("PASS: actual added-literal flips, repeated/opposite polarity, clear/rearm and counter-range filters");
}
