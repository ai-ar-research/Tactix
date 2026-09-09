/* Standalone read-only learning-stage trace; no solver state ownership. */
#ifndef LEARNED_STAGE_6030_H
#define LEARNED_STAGE_6030_H
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
static FILE *ls_file;
static uint64_t ls_from, ls_to, ls_sequence;
static uint64_t ls_bound(const char *name) {
  const char *s = getenv(name);
  if (!s || !*s) exit(2);
  for (const char *p = s; *p; ++p) if (*p < '0' || *p > '9') exit(2);
  char *end; errno = 0;
  const unsigned long long v = strtoull(s, &end, 10);
  if (errno || *end) exit(2);
  return (uint64_t)v;
}
static int ls_begin(const char *event, uint64_t conflicts, uint64_t decisions,
                    unsigned level, size_t size) {
  static int initialized;
  if (!initialized) {
    initialized = 1;
    const char *path = getenv("SAT_LEARNED_STAGE_TRACE");
    if (!path || !*path) return 0;
    ls_from = ls_bound("SAT_LEARNED_STAGE_FROM");
    ls_to = ls_bound("SAT_LEARNED_STAGE_TO");
    if (ls_from > ls_to) exit(2);
    ls_file = fopen(path, "w"); if (!ls_file) exit(2);
    fputs("# learned-stage\t1\n# seq\tevent\tconflicts\tdecisions\tlevel\tsize\tliterals\n", ls_file);
  }
  if (!ls_file || conflicts < ls_from || conflicts > ls_to) return 0;
  fprintf(ls_file, "%llu\t%s\t%llu\t%llu\t%u\t%zu\t",
          (unsigned long long)ls_sequence++, event,
          (unsigned long long)conflicts, (unsigned long long)decisions, level, size);
  if (!size) fputc('-', ls_file);
  return 1;
}
static void ls_end(void) { fputc('\n', ls_file); fflush(ls_file); }
#endif
