// Fixed mechanism fixture: call the actual pinned native connection routine.
// Linking libkissat.a supplies its dependencies, not a copied implementation.
#include "factor.c"
#include "import.h"

static void add(kissat *solver, int a, int b, int c) {
  kissat_add(solver, a);
  kissat_add(solver, b);
  if (c) kissat_add(solver, c);
  kissat_add(solver, 0);
}

int main(void) {
  for (unsigned reverse = 0; reverse < 2; reverse++)
    for (unsigned signs = 0; signs < 4; signs++)
      for (unsigned fixed = 0; fixed < 2; fixed++)
        for (unsigned rounds = 1; rounds <= 2; rounds++)
          for (unsigned forced = 0; forced < 2; forced++) {
            kissat *solver = kissat_init();
            kissat_set_option(solver, "quiet", 1);
            kissat_set_option(solver, "factorsize", 3);
            kissat_set_option(solver, "factorcandrounds", rounds);
            (void)kissat_import_literal(solver, reverse ? 2 : 1);
            (void)kissat_import_literal(solver, reverse ? 1 : 2);
            for (int v = 3; v <= 6; v++) (void)kissat_import_literal(solver, v);
            const int u = signs & 1 ? -1 : 1;
            const int x = signs & 2 ? -2 : 2;
            add(solver, u, x, 0);
            add(solver, 3, 5, 0);
            add(solver, 4, 6, 0);
            add(solver, x, 3, 4);
            if (fixed) { kissat_add(solver, u); kissat_add(solver, 0); }
            const unsigned xu = kissat_import_literal(solver, x);
            // No unfixed literal is implied by the root-true binary endpoint.
            // Dense entry retains that satisfied pair, as the real Factor path.
            kissat_enter_dense_mode(solver, 0);
            connect_clauses_to_factor(solver);
            const size_t score = SIZE_WATCHES(WATCHES(xu));
            if (score != (fixed && !reverse ? 1u : 2u)) return 2;
            printf("%u %u %u %u %u %zu\n", reverse, signs, fixed, rounds, forced, score);
            kissat_release(solver);
          }
  return 0;
}
