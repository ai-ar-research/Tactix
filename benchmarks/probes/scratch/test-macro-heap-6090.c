#undef NDEBUG
#include "macro-heap-6090.h"
#include <assert.h>
#include <string.h>
#include <unistd.h>

static unsigned calls;
static struct macro_heap_row_6090 row(const void *context, size_t pos) {
  ++calls;
  return ((const struct macro_heap_row_6090 *)context)[pos];
}
int main(void) {
  char path[] = "/tmp/tactix-macro-heap-6090-XXXXXX";
  const int fd = mkstemp(path); assert(fd >= 0); close(fd);
  const struct macro_heap_row_6090 data[] = {{7, 1.5, 0, 1}, {3, 1.5, 1, 0}};
  unsetenv("SAT_MACRO_HEAP_PATH");
  macro_heap_emit_6090("disabled", 17, 20, 2, data, row);
  setenv("SAT_MACRO_HEAP_PATH", path, 1);
  const char *invalid[] = {"", "-1", "+17", "17x", " 17", "18446744073709551616", "18"};
  for (unsigned i=0; i<sizeof invalid/sizeof *invalid; ++i) {
    setenv("SAT_MACRO_HEAP_CONFLICT", invalid[i], 1);
    macro_heap_emit_6090("filtered", 17, 20, 2, data, row);
  }
  assert(!calls);
  setenv("SAT_MACRO_HEAP_CONFLICT", "17", 1);
  macro_heap_emit_6090("begin", 17, 20, 2, data, row);
  macro_heap_emit_6090("empty", 17, 20, 0, data, row);
  macro_heap_emit_6090("end", 17, 21, 2, data, row);
  assert(calls == 4 && data[0].external == 7 && data[1].assigned == 1);
  FILE *in = fopen(path, "r"); assert(in);
  char buf[1024]; size_t n = fread(buf, 1, sizeof buf-1, in); buf[n] = 0; fclose(in);
  const char *expected = "# macro-heap-series\t1\n"
    "# snapshot\tbegin\t17\t20\t2\n7\t0\t1.5\t0\t1\n3\t1\t1.5\t1\t0\n# end\n"
    "# snapshot\tempty\t17\t20\t0\n# end\n"
    "# snapshot\tend\t17\t21\t2\n7\t0\t1.5\t0\t1\n3\t1\t1.5\t1\t0\n# end\n";
  assert(!strcmp(buf, expected));
  assert(!unlink(path));
  puts("PASS: disabled/malformed/overflow/mismatched selection; repeated and empty snapshots; exact rows; no state mutation");
}
