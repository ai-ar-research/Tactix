/* Fixed mechanism replay; includes the unmodified reference Sweep unit so
 * its static connected-rewrite primitive is tested directly, not mirrored. */
#include "sweep.c"
#include "kissat.h"
#include "import.h"
#include <stdio.h>

static void input_binary(kissat *solver, int a, int b) {
  kissat_add(solver, a);
  kissat_add(solver, b);
  kissat_add(solver, 0);
}

static unsigned copies(kissat *solver, unsigned a, unsigned b) {
  unsigned result = 0;
  for (all_binary_large_watches(w, WATCHES(a)))
    if (w.type.binary && w.binary.lit == b)
      ++result;
  return result;
}

int main(void) {
  kissat *solver = kissat_init();
  /* Import 1 first; every subsequently replayed equivalence
     is entailed by these six initial binaries. */
  input_binary(solver, 1, -4);
  input_binary(solver, -1, 4);
  input_binary(solver, 4, -2);
  input_binary(solver, -4, 2);
  input_binary(solver, 4, -3);
  input_binary(solver, -4, 3);
  const unsigned one = kissat_import_literal(solver, 1);
  const unsigned two = kissat_import_literal(solver, 2);
  const unsigned three = kissat_import_literal(solver, 3);
  const unsigned four = kissat_import_literal(solver, 4);
  sweeper sweep = {0};
  init_sweeper(solver, &sweep);
  /* Fresh first bridge, then the same native pair-wise connected rewrite. */
  add_binary(solver, one, NOT(four));
  add_binary(solver, NOT(one), four);
  substitute_connected_clauses(&sweep, four, one);
  substitute_connected_clauses(&sweep, NOT(four), NOT(one));
  printf("before later bridge: %u %u\n", copies(solver, one, NOT(two)),
         copies(solver, one, NOT(three)));
  /* Kissat's actual add_binary does not deduplicate an already-live pair. */
  add_binary(solver, one, NOT(two));
  add_binary(solver, NOT(one), two);
  substitute_connected_clauses(&sweep, two, one);
  substitute_connected_clauses(&sweep, NOT(two), NOT(one));
  add_binary(solver, one, NOT(three));
  add_binary(solver, NOT(one), three);
  substitute_connected_clauses(&sweep, three, one);
  substitute_connected_clauses(&sweep, NOT(three), NOT(one));
  printf("after later bridges: %u %u\n", copies(solver, one, NOT(two)),
         copies(solver, one, NOT(three)));
  const int ok = copies(solver, one, NOT(two)) == 2 &&
                 copies(solver, one, NOT(three)) == 2;
  release_sweeper(&sweep);
  kissat_release(solver);
  return ok ? 0 : 1;
}
