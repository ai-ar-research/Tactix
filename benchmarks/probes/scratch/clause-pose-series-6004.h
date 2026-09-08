/* Shared read-only diagnostic framing, not a solver mechanism. */
#ifndef CLAUSE_POSE_SERIES_6004_H
#define CLAUSE_POSE_SERIES_6004_H
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

static FILE *pose_series_file;
static int pose_series_selected[16];
static unsigned pose_series_size;
static uint64_t pose_series_sequence, pose_series_from, pose_series_to = UINT64_MAX;
static unsigned pose_series_rows;

static int pose_series_begin(uint64_t conflicts, uint64_t decisions,
                             const char *event, const char *payload) {
  static int initialized;
  if (!initialized) {
    initialized = 1;
    const char *path = getenv("SAT_CLAUSE_POSE_SERIES_PATH");
    if (!path || !*path) return 0;
    const char *spec = getenv("SAT_CLAUSE_POSE_SERIES_LITS");
    if (!spec || !*spec) exit(2);
    while (*spec) {
      char *end;
      const long value = strtol(spec, &end, 10);
      if (end == spec || !value || value <= INT_MIN || value > INT_MAX ||
          pose_series_size == 16 || (*end && *end != ',')) exit(2);
      for (unsigned i = 0; i < pose_series_size; ++i)
        if (abs(pose_series_selected[i]) == abs((int)value)) exit(2);
      pose_series_selected[pose_series_size++] = (int)value;
      spec = *end ? end + 1 : end;
      if (*end && !*spec) exit(2);
    }
    if (pose_series_size < 3) exit(2);
    const char *s = getenv("SAT_CLAUSE_POSE_SERIES_FROM");
    if (s) pose_series_from = strtoull(s, 0, 10);
    s = getenv("SAT_CLAUSE_POSE_SERIES_TO");
    if (s) pose_series_to = strtoull(s, 0, 10);
    if (pose_series_from > pose_series_to) exit(2);
    pose_series_file = fopen(path, "w");
    if (!pose_series_file) exit(2);
    fputs("# clause-pose-series\t1\n", pose_series_file);
  }
  if (!pose_series_file || conflicts < pose_series_from || conflicts > pose_series_to ||
      strcmp(event, "slot_boundary") || !payload) return 0;
  const char *start = strstr(payload, "name=");
  const char *stage = strstr(payload, "stage=");
  if (!start || !stage) return 0;
  start += 5; stage += 6;
  char name[64]; unsigned n = 0;
  while (*start && *start != '|' && n + 1 < sizeof name)
    name[n++] = (char)tolower((unsigned char)*start++);
  name[n] = 0;
  const char *names = getenv("SAT_CLAUSE_POSE_SERIES_NAMES");
  if (names && *names) {
    int selected = 0;
    while (*names) {
      const char *end = strchr(names, ',');
      const size_t len = end ? (size_t)(end - names) : strlen(names);
      if (len == n && !strncmp(names, name, n)) selected = 1;
      names += len; if (*names) ++names;
    }
    if (!selected) return 0;
  }
  if (strcmp(stage, "begin") && strcmp(stage, "end")) return 0;
  fprintf(pose_series_file, "# begin\t%llu\t%llu\t%llu\t%s\t%s\n",
          (unsigned long long)pose_series_sequence, (unsigned long long)conflicts,
          (unsigned long long)decisions, name, stage);
  pose_series_rows = 0;
  return 1;
}

static int pose_series_matches(const int *lits) {
  unsigned mask = 0;
  for (unsigned i = 0; i < pose_series_size; ++i) {
    unsigned j = 0;
    while (j < pose_series_size && pose_series_selected[j] != lits[i]) ++j;
    if (j == pose_series_size || (mask & (1u << j))) return 0;
    mask |= 1u << j;
  }
  return mask == (1u << pose_series_size) - 1;
}

static void pose_series_row(uint64_t ref, unsigned red, unsigned searched,
                             const int *lits, const int *values) {
  fprintf(pose_series_file, "%llu\t%llu\t%u\t%u\t",
          (unsigned long long)pose_series_sequence, (unsigned long long)ref, red, searched);
  for (unsigned i = 0; i < pose_series_size; ++i)
    fprintf(pose_series_file, "%s%d", i ? "," : "", lits[i]);
  fputc('\t', pose_series_file);
  for (unsigned i = 0; i < pose_series_size; ++i)
    fprintf(pose_series_file, "%s%d", i ? "," : "", values[i]);
  fputc('\n', pose_series_file);
  ++pose_series_rows;
}

static void pose_series_end(void) {
  fprintf(pose_series_file, "# end\t%llu\t%u\n",
          (unsigned long long)pose_series_sequence++, pose_series_rows);
  fflush(pose_series_file);
}
#endif
