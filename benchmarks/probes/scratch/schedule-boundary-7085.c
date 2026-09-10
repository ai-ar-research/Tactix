// Compile with the reference's NDEBUG layout, against fresh actual sources.
#include "internal.h"
#include "probe.h"
#include "eliminate.h"
#include <stdio.h>
#include <stdlib.h>
int main(void) {
  for (unsigned conflicts = 99; conflicts <= 101; ++conflicts)
    for (unsigned same_reduce = 0; same_reduce <= 1; ++same_reduce)
      for (unsigned material = 0; material <= 1; ++material)
        for (unsigned reduced = 0; reduced <= 1; ++reduced) {
          kissat *solver = calloc(1, sizeof *solver);
          if (!solver) return 1;
          solver->enabled.probe = solver->enabled.eliminate = true;
          solver->statistics.clauses_irredundant = 1;
          solver->statistics.conflicts = conflicts;
          solver->statistics.reductions = reduced;
          solver->statistics.variables_eliminate = material;
          solver->last.conflicts.reduce = same_reduce ? conflicts : conflicts - 1;
          solver->limits.probe.conflicts = solver->limits.eliminate.conflicts = 100;
          printf("%u\t%u\t%u\t%u\t%d\t%d\n", conflicts, same_reduce,
                 material, reduced, kissat_probing(solver), kissat_eliminating(solver));
          free(solver);
        }
}
