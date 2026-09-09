#define NDEBUG 1
#include "internal.h"
#include "eliminate.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main(void) {
  char path[160]; snprintf(path, sizeof path, "/tmp/native-admission-%ld.tsv", (long)getpid());
  setenv("KISSAT_ELIMINATE_ADMISSION_TRACE", path, 1);
  setenv("SAT_ELIMINATE_ADMISSION_CONFLICT", "9", 1);
  kissat *solver = kissat_init();
  solver->statistics.conflicts = 9;
  solver->enabled.eliminate = false;
  assert(!kissat_eliminating(solver));
  solver->enabled.eliminate = true;
  assert(!kissat_eliminating(solver));
  solver->statistics.clauses_irredundant = 1;
  solver->last.conflicts.reduce = 9;
  assert(!kissat_eliminating(solver));
  solver->last.conflicts.reduce = 8; solver->limits.eliminate.conflicts = 10;
  assert(!kissat_eliminating(solver));
  solver->limits.eliminate.conflicts = 9;
  assert(!kissat_eliminating(solver));
  ++solver->statistics.variables_eliminate;
  assert(kissat_eliminating(solver));
  solver->limits.eliminate.variables.eliminate = 1;
  ++solver->statistics.variables_subsume;
  assert(kissat_eliminating(solver));
  solver->statistics.conflicts = 10; assert(kissat_eliminating(solver));
  solver->statistics.clauses_irredundant = 0;
  kissat_release(solver);
  FILE *in = fopen(path, "r"); assert(in); char line[2048]; unsigned count = 0;
  while (fgets(line, sizeof line, in)) {
    assert(strstr(line, "kissat\tconflicts=9\t") == line);
    assert(strstr(line, count < 5 ? "\tanswer=0\t" : "\tanswer=1\t"));
    ++count;
  }
  assert(count == 7); fclose(in); assert(remove(path) == 0);
  puts("PASS: actual native admission branches, both watermark sources and conflict filter");
}
