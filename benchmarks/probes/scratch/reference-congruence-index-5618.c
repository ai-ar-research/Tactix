// Fixed fixture calls the actual pinned XOR candidate initializer.
#include "congruence.c"
#include "import.h"

int main(void) {
  for (unsigned width = 3; width <= 7; width++)
    for (unsigned position = 0; position < width; position++)
      for (int value = -1; value <= 1; value++) {
        kissat *solver = kissat_init();
        kissat_set_option(solver, "quiet", 1);
        for (unsigned v = 1; v <= width; v++)
          (void) kissat_import_literal(solver, v);
        for (unsigned v = 1; v <= width; v++) kissat_add(solver, v);
        kissat_add(solver, 0);
        clause *target = kissat_dereference_clause(solver, 0);
        if (value) {
          kissat_add(solver, value * (int)(position + 1));
          kissat_add(solver, 0);
        }
        closure closure = {0};
        closure.solver = solver;
        references candidates;
        INIT_STACK(candidates);
        kissat_enter_dense_mode(solver, &closure.binaries);
        init_xor_gate_extraction(&closure, &candidates);
        printf("%u %u %d %u\n", width, position, value, target->garbage);
        reset_xor_gate_extraction(&closure);
        RELEASE_STACK(candidates);
        RELEASE_STACK(closure.binaries);
        kissat_release(solver);
      }
  return 0;
}
