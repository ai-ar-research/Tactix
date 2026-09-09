#include "transitive-schedule-6350.h"
#include <assert.h>
#include <unistd.h>

int main(int argc, char **argv) {
  assert(argc == 2);
  unsetenv("SAT_TRANSITIVE_SCHEDULE_PATH");
  assert(!transitive_schedule_begin_6350("before", 7, 9, 1, 11, 2));
  setenv("SAT_TRANSITIVE_SCHEDULE_PATH", argv[1], 1);
  setenv("SAT_TRANSITIVE_SCHEDULE_CONFLICT", "7x", 1);
  assert(!transitive_schedule_begin_6350("before", 7, 9, 1, 11, 2));
  setenv("SAT_TRANSITIVE_SCHEDULE_CONFLICT", "18446744073709551616", 1);
  assert(!transitive_schedule_begin_6350("before", 7, 9, 1, 11, 2));
  setenv("SAT_TRANSITIVE_SCHEDULE_CONFLICT", "7", 1);
  assert(!transitive_schedule_begin_6350("before", 8, 9, 1, 11, 2));
  assert(access(argv[1], F_OK) != 0);
  FILE *out = transitive_schedule_begin_6350("before", 7, 9, 1, 11, 2);
  assert(out);
  transitive_schedule_row_6350(out, 0, 4, 936, 1, 0, 2, 99, 1.0, 17, 0);
  transitive_schedule_row_6350(out, 1, 5, 937, 1, 0, 2, 99, 1.0, 17, 0);
  transitive_schedule_end_6350(out);
  out = transitive_schedule_begin_6350("after", 7, 9, 1, 11, 2);
  assert(out);
  transitive_schedule_row_6350(out, 0, 5, 937, 1, 0, 2, 99, 1.0, 17, 0);
  transitive_schedule_row_6350(out, 1, 4, 936, 1, 0, 2, 99, 1.0, 17, 0);
  transitive_schedule_end_6350(out);
  puts("schedule writer: disabled/malformed/overflow/conflict filters, exact tie keys, repeated snapshots PASS");
  return 0;
}
