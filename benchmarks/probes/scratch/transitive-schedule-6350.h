#ifndef TACTIX_TRANSITIVE_SCHEDULE_6350_H
#define TACTIX_TRANSITIVE_SCHEDULE_6350_H
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Read-only diagnostic writer, shared by C/C++ adapters. No sorting here. */
static FILE *transitive_schedule_begin_6350(const char *stage, uint64_t conflict,
    uint64_t decisions, unsigned stable, uint64_t ticks, size_t size) {
  const char *path = getenv("SAT_TRANSITIVE_SCHEDULE_PATH");
  const char *selected = getenv("SAT_TRANSITIVE_SCHEDULE_CONFLICT");
  if (!path || !*path || !selected || !*selected) return 0;
  for (const char *p = selected; *p; ++p)
    if (*p < '0' || *p > '9') return 0;
  char *end;
  errno = 0;
  const unsigned long long wanted = strtoull(selected, &end, 10);
  if (errno || *end || wanted != conflict) return 0;
  static FILE *out;
  if (!out) {
    out = fopen(path, "w");
    if (!out) { perror("transitive schedule"); exit(2); }
    fputs("# transitive-schedule\t1\n", out);
  }
  fprintf(out, "# snapshot\t%s\t%llu\t%llu\t%u\t%llu\t%zu\n", stage,
      (unsigned long long) conflict, (unsigned long long) decisions, stable,
      (unsigned long long) ticks, size);
  fputs("# ordinal\tcompact\texternal\tactive\tvalue\tkey_compact\tkey_external\tscore_bits\tstamp\tpriority\n", out);
  return out;
}

static void transitive_schedule_row_6350(FILE *out, size_t ordinal,
    unsigned compact, int external, unsigned active, int value,
    unsigned key_compact, int key_external, double score, uint64_t stamp,
    unsigned priority) {
  uint64_t bits;
  memcpy(&bits, &score, sizeof bits);
  fprintf(out, "%zu\t%u\t%d\t%u\t%d\t%u\t%d\t%016llx\t%llu\t%u\n",
      ordinal, compact, external, active, value, key_compact, key_external,
      (unsigned long long) bits, (unsigned long long) stamp, priority);
}

static void transitive_schedule_end_6350(FILE *out) {
  fputs("# end\n", out);
  if (fflush(out) || ferror(out)) { perror("transitive schedule write"); exit(2); }
}
#endif
