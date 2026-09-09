#define NDEBUG 1
#include "internal.h"
#include "substitute.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
int main(void) {
  const unsigned counts[] = {1, 2, 5};
  for (unsigned j = 0; j < 3; ++j) {
    const unsigned copies = counts[j];
    kissat *s = kissat_init();
    kissat_set_option(s, "quiet", 1);
    kissat_set_option(s, "substituterounds", 1);
    const int eq[] = {-1, 2, 0, 1, -2, 0};
    for (unsigned i = 0; i < 6; ++i) kissat_add(s, eq[i]);
    for (unsigned i = 0; i < copies; ++i) {
      kissat_add(s, 1); kissat_add(s, 2); kissat_add(s, 3); kissat_add(s, 0);
    }
    for (unsigned i = 0; i < 3; ++i) s->flags[i].eliminate = false;
    s->probing = true; kissat_substitute(s, false); s->probing = false;
    assert(!s->inconsistent && s->flags[2].eliminate);
    printf("native copies=%u retained_endpoint_retry=%u\n", copies, s->flags[2].eliminate);
    kissat_release(s);
  }
}
