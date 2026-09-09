#define NDEBUG 1
#include "internal.h"
#include "inline.h"
#include "eventtrace.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
  for (unsigned mode = 0; mode != 4; ++mode) {
    char path[160];
    snprintf(path, sizeof path, "/tmp/tactix-removal-observer-%ld-%u.tsv", (long)getpid(), mode);
    pid_t pid = fork(); assert(pid >= 0);
    if (!pid) {
      if (mode != 3) setenv("KISSAT_REMOVED_CLAUSE_TRACE", path, 1);
      else unsetenv("KISSAT_REMOVED_CLAUSE_TRACE");
      setenv("SAT_REMOVED_CLAUSE_EXTERNAL", mode == 2 ? "4" : "2", 1);
      setenv("SAT_REMOVED_CLAUSE_CONFLICT", "7", 1);
      kissat *solver = kissat_init();
      kissat_set_option(solver, "quiet", 1);
      kissat_add(solver, 1); kissat_add(solver, -2); kissat_add(solver, 3); kissat_add(solver, 0);
      clause *c = kissat_dereference_clause(solver, 0);
      assert(c->size == 3 && !c->garbage && !c->redundant);
      solver->statistics.conflicts = mode == 1 ? 6 : 7;
      for (unsigned i = 0; i < 3; ++i) solver->flags[i].eliminate = false;
      kissat_event_trace_clause_garbage(solver, c, "fixture");
      assert(!c->garbage && c->size == 3);
      for (unsigned i = 0; i < 3; ++i) assert(!solver->flags[i].eliminate && !solver->values[2*i]);
      kissat_mark_clause_as_garbage_with_source(solver, c, "actual-delete");
      assert(c->garbage);
      for (unsigned i = 0; i < 3; ++i) assert(solver->flags[i].eliminate);
      kissat_release(solver);
      _exit(0);
    }
    int status; assert(waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    if (mode == 3) { assert(access(path, F_OK) != 0); continue; }
    FILE *f = fopen(path, "r"); assert(f);
    char buffer[4096]; size_t size = fread(buffer, 1, sizeof buffer - 1, f);
    assert(feof(f)); fclose(f); buffer[size] = 0;
    if (!mode) {
      assert(strstr(buffer, "source=fixture\tredundant=0\tsize=3\tliterals=1,-2,3,"));
      assert(strstr(buffer, "source=actual-delete"));
      assert(strstr(buffer, "values=0,0,0,\teliminate=0,0,0,\n"));
    } else assert(!size);
    assert(remove(path) == 0);
  }
  puts("PASS: actual native deletion, read-only snapshot, disabled/conflict/external filters (4 cases)");
}
