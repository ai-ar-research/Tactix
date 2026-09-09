#undef NDEBUG
#include "internal.h"
#include "eventtrace.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
  for (int resume = 0; resume != 2; ++resume) {
    char path[160];
    snprintf(path, sizeof path, "/tmp/tactix-native-schedule-%ld-%d.tsv",
             (long)getpid(), resume);
    pid_t child = fork();
    assert(child >= 0);
    if (!child) {
      unsetenv("KISSAT_EVENT_TRACE");
      setenv("KISSAT_BVE_SCHEDULE_TRACE", path, 1);
      setenv("SAT_BVE_SCHEDULE_CONFLICT", "1", 1);
      setenv("SAT_BVE_SCHEDULE_ORDINAL", "1", 1);
      setenv("SAT_BVE_SCHEDULE_MEMBERS", "1", 1);
      if (resume) setenv("SAT_BVE_SCHEDULE_CONTINUE", "1", 1);
      else unsetenv("SAT_BVE_SCHEDULE_CONTINUE");
      kissat *solver = kissat_init();
      kissat_event_trace_bve_schedule(solver, &solver->schedule);
      assert(access(path, F_OK) != 0);
      solver->statistics.conflicts = 1;
      kissat_event_trace_bve_schedule(solver, &solver->schedule);
      assert(access(path, F_OK) != 0);
      kissat_event_trace_bve_schedule(solver, &solver->schedule);
      // Only the opted-in path may return; an unselected later call must
      // not replace the selected snapshot with a changed bound.
      solver->bounds.eliminate.additional_clauses = 99;
      kissat_event_trace_bve_schedule(solver, &solver->schedule);
      assert(solver->statistics.conflicts == 1);
      assert(EMPTY_STACK(solver->schedule.stack));
      kissat_release(solver);
      _exit(99);
    }
    int status;
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status));
    assert(WEXITSTATUS(status) == (resume ? 99 : 0));
    FILE *file = fopen(path, "r");
    assert(file);
    char buffer[2048];
    size_t n = fread(buffer, 1, sizeof buffer - 1, file);
    assert(feof(file)); buffer[n] = 0; fclose(file);
    assert(strstr(buffer, "# bve-schedule-observer\t1\n"));
    assert(strstr(buffer, "scheduled=0|"));
    assert(strstr(buffer, "conflicts=1|"));
    assert(!strstr(buffer, "bound=99|"));
    assert(remove(path) == 0);
  }
  puts("PASS: native stopped/continued empty snapshots, conflict/ordinal filters and no overwrite");
  return 0;
}
