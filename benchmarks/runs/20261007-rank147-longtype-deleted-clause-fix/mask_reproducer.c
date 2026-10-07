#include <limits.h>
#include <stdio.h>

typedef long long srid_t;

#define SRID_MSB LLONG_MIN
#define BROKEN_IS_DELETED_CLAUSE(x) ((x) & SRID_MSB)
#define FIXED_IS_DELETED_CLAUSE(x) (((x) & SRID_MSB) != 0)

static int broken(srid_t value) {
  return BROKEN_IS_DELETED_CLAUSE(value);
}

static int fixed(srid_t value) {
  return FIXED_IS_DELETED_CLAUSE(value);
}

int main(void) {
  srid_t marked_pointer = ((srid_t) 21450) | SRID_MSB;
  int broken_value = broken(marked_pointer);
  int fixed_value = fixed(marked_pointer);
  printf("sizeof(srid_t)=%zu raw=%lld broken=%d fixed=%d\n",
         sizeof(srid_t), marked_pointer, broken_value, fixed_value);
  return !(broken_value == 0 && fixed_value == 1);
}
