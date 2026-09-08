/* Opt-in, read-only candidate-level diagnostic shared by both adapters. */
#ifndef VIVIFY_POSE_6016_H
#define VIVIFY_POSE_6016_H
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
static FILE *vp_file;
static uint64_t vp_conflict, vp_sequence;
static unsigned vp_size;
static int vp_lits[16];
static uint32_t vp_cached = UINT32_MAX;
static int vp_enabled(uint64_t conflict) {
  static int initialized;
  if (!initialized) {
    initialized = 1;
    const char *path = getenv("SAT_VIVIFY_POSE_TRACE");
    if (!path || !*path) return 0;
    const char *spec = getenv("SAT_VIVIFY_POSE_LITS");
    if (!spec || !*spec) exit(2);
    while (*spec) {
      char *end; long v = strtol(spec, &end, 10);
      if (end == spec || !v || v <= INT_MIN || v > INT_MAX || vp_size == 16 ||
          (*end && *end != ',')) exit(2);
      for (unsigned i = 0; i < vp_size; ++i) if (abs(vp_lits[i]) == abs((int)v)) exit(2);
      vp_lits[vp_size++] = (int)v;
      spec = *end ? end + 1 : end;
      if (*end && !*spec) exit(2);
    }
    const char *at = getenv("SAT_VIVIFY_POSE_CONFLICT"); char *end;
    if (!at || !*at || vp_size < 3) exit(2);
    vp_conflict = strtoull(at, &end, 10); if (*end) exit(2);
    vp_file = fopen(path, "w"); if (!vp_file) exit(2);
    fputs("# vivify-pose\t1\n", vp_file);
    fputs("# seq\tevent\tconflicts\tscope\tordinal\tticks\tlimit\tselected_ref\tsearched\tpose\tvalues\tcandidate_ref\tredundant\tgarbage\tcandidate\n", vp_file);
  }
  return vp_file && conflict == vp_conflict;
}
static int vp_matches(const int *lits) {
  unsigned mask = 0;
  for (unsigned i = 0; i < vp_size; ++i) {
    unsigned j = 0; while (j < vp_size && vp_lits[j] != lits[i]) ++j;
    if (j == vp_size || (mask & (1u << j))) return 0;
    mask |= 1u << j;
  }
  return mask == (1u << vp_size) - 1;
}
static void vp_list(const int *lits, unsigned size) {
  if (!size) { fputc('-', vp_file); return; }
  for (unsigned i = 0; i < size; ++i) fprintf(vp_file, "%s%d", i ? "," : "", lits[i]);
}
static void vp_start(const char *event, uint64_t conflict, unsigned scope, uint64_t ordinal,
                      uint64_t ticks, uint64_t limit, uint32_t selected, unsigned searched,
                      const int *pose, const int *values) {
  fprintf(vp_file, "%llu\t%s\t%llu\t%u\t%llu\t%llu\t%llu\t%u\t%u\t",
          (unsigned long long)vp_sequence++, event, (unsigned long long)conflict, scope,
          (unsigned long long)ordinal, (unsigned long long)ticks, (unsigned long long)limit,
          selected, searched);
  vp_list(pose, selected == UINT32_MAX ? 0 : vp_size); fputc('\t', vp_file);
  vp_list(values, selected == UINT32_MAX ? 0 : vp_size); fputc('\t', vp_file);
}
#endif
