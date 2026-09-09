#ifndef TACTIX_DIAGNOSTIC_MACRO_HEAP_6090_H
#define TACTIX_DIAGNOSTIC_MACRO_HEAP_6090_H
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

struct macro_heap_row_6090 {
  int external;
  double score;
  unsigned assigned, active;
};

/* Diagnostic-only streaming writer. The adapters read, never mutate, heaps. */
static void macro_heap_emit_6090(
    const char *label, uint64_t conflict, uint64_t decision, size_t size,
    const void *context,
    struct macro_heap_row_6090 (*read_row)(const void *, size_t)) {
  const char *path = getenv("SAT_MACRO_HEAP_PATH");
  const char *selected = getenv("SAT_MACRO_HEAP_CONFLICT");
  if (!path || !*path || !selected || !*selected)
    return;
  for (const char *p = selected; *p; ++p)
    if (*p < '0' || *p > '9') return;
  char *end;
  errno = 0;
  const unsigned long long wanted = strtoull(selected, &end, 10);
  if (errno || *end || wanted != conflict) return;
  static FILE *out;
  if (!out) {
    out = fopen(path, "w");
    if (!out) { perror("SAT_MACRO_HEAP_PATH"); exit(2); }
    setvbuf(out, 0, _IOFBF, 1 << 20);
    fputs("# macro-heap-series\t1\n", out);
  }
  fprintf(out, "# snapshot\t%s\t%llu\t%llu\t%zu\n", label,
          (unsigned long long)conflict, (unsigned long long)decision, size);
  for (size_t pos = 0; pos < size; ++pos) {
    const struct macro_heap_row_6090 r = read_row(context, pos);
    fprintf(out, "%d\t%zu\t%.17g\t%u\t%u\n", r.external, pos,
            r.score, r.assigned, r.active);
  }
  fputs("# end\n", out);
  if (fflush(out) || ferror(out)) { perror("macro heap write"); exit(2); }
}
#endif
