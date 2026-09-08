// Call the actual pinned native analysis routine on fixed, matching states.
#include "analyze.c"
#include "assign.h"
#include "flags.h"
#include "import.h"
#include "inlineframes.h"

static void decision(kissat *solver, unsigned lit) {
  solver->level++;
  kissat_push_frame(solver, lit);
  kissat_assign_decision(solver, lit);
}

static void roots(kissat *solver, unsigned size) {
  for (unsigned v = size - 1; v; --v)
    kissat_assign_unit(solver, NOT(LIT(v - 1)), "fixed fixture input");
}

int main(void) {
  const unsigned sizes[] = {2, 4}, depths[] = {1, 7, 103};
  for (unsigned si = 0; si < 2; ++si)
    for (unsigned di = 0; di < 3; ++di)
      for (unsigned stable = 0; stable < 2; ++stable)
        for (unsigned late = 0; late < 2; ++late) {
          const unsigned size = sizes[si], depth = depths[di];
          kissat *solver = kissat_init();
          kissat_set_option(solver, "quiet", 1);
          kissat_set_option(solver, "chrono", 1);
          kissat_set_option(solver, "chronolevels", 100);
          solver->stable = stable;
          for (unsigned v = 1; v <= size + depth + 2; ++v) {
            const unsigned lit = kissat_import_literal(solver, v);
            if (lit != LIT(v - 1)) return 2;
            kissat_activate_literal(solver, lit);
          }
          for (unsigned v = 1; v <= size; ++v) kissat_add(solver, v);
          kissat_add(solver, 0);
          const unsigned side = size + depth + 1;
          kissat_add(solver, -(int)size);kissat_add(solver, side);kissat_add(solver, 0);
          kissat_add(solver, -(int)size);kissat_add(solver, -(int)side);kissat_add(solver, 0);
          if (!late) roots(solver, size);
          for (unsigned i = 1; i < depth; ++i) decision(solver, LIT(size + i - 1));
          if (late) roots(solver, size);
          const unsigned forced = LIT(size - 1);
          decision(solver, NOT(forced));
          solver->statistics.conflicts = 1;
          solver->statistics.decisions = depth;
          solver->propagate = BEGIN_ARRAY(solver->trail);
          clause *conflict;
          if (size == 2) {
            conflict = &solver->conflict;
            conflict->lits[0] = LIT(0);
            conflict->lits[1] = forced;
          } else conflict = kissat_dereference_clause(solver, 0);
          if (kissat_analyze(solver, conflict)) return 3;
          const assigned *a = solver->assigned + IDX(forced);
          printf("PARITY %u %u %u %u %u %d %u %u %u %u %u %.17g %llu %llu\n",
              size, depth, stable, late, solver->level, VALUE(forced), a->level,
              a->reason == UNIT_REASON, a->binary, solver->flags[IDX(forced)].active,
              kissat_heap_contains(SCORES, IDX(forced)), solver->scinc,
              (unsigned long long)solver->statistics.clauses_learned,
              (unsigned long long)solver->statistics.chronological);
          kissat_release(solver);
        }
  return 0;
}
