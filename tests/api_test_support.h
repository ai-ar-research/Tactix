#ifndef TACTIX_TESTS_API_TEST_SUPPORT_H
#define TACTIX_TESTS_API_TEST_SUPPORT_H
/* Plain-C helpers shared by the public-API C clients. Include after
 * `#undef NDEBUG`: rule() asserts. */
#include "tactix.h"

#include <assert.h>
#include <string.h>

/* Add one zero-terminated clause. */
static inline void add_clause(tactix *solver, const int *literals) {
  while (*literals)
    tactix_add(solver, *literals++);
  tactix_add(solver, 0);
}

/* The registry id of `name`, or -1 when the registry has no such rule. */
static inline int rule_id(const char *name) {
  int id;
  for (id = 0; id < tactix_rule_count(); ++id)
    if (!strcmp(tactix_rule_name(id), name))
      return id;
  return -1;
}

/* The registry id of `name`, which must exist. */
static inline int rule(const char *name) {
  const int id = rule_id(name);
  assert(id >= 0);
  return id;
}

/* One application of the step, with no work budget. */
static inline void step_once(tactix_online_step *step) {
  step->repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step->repeat_limit = 1;
  step->work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  step->budget_unlimited = 1;
}

/* Something for every structural rule: the same AND gate twice (congruence),
 * an implication chain with its transitive shortcut, a 2x3 factor grid, a
 * subsumed and a strengthenable pair, and blocked clauses on 24. */
static inline void fixture(tactix *s) {
  /* the same AND gate twice, so a congruence has something to merge */
  add_clause(s, (const int[]){-10, 1, 0});
  add_clause(s, (const int[]){-10, 2, 0});
  add_clause(s, (const int[]){10, -1, -2, 0});
  add_clause(s, (const int[]){-11, 1, 0});
  add_clause(s, (const int[]){-11, 2, 0});
  add_clause(s, (const int[]){11, -1, -2, 0});
  /* implication chain 5 -> 6 -> 7 with the transitive shortcut 5 -> 7 */
  add_clause(s, (const int[]){-5, 6, 0});
  add_clause(s, (const int[]){-6, 7, 0});
  add_clause(s, (const int[]){-5, 7, 0});
  /* 2x3 grid over factors {12,13} and remainders {14,15,16} */
  add_clause(s, (const int[]){12, 14, 0});
  add_clause(s, (const int[]){12, 15, 0});
  add_clause(s, (const int[]){12, 16, 0});
  add_clause(s, (const int[]){13, 14, 0});
  add_clause(s, (const int[]){13, 15, 0});
  add_clause(s, (const int[]){13, 16, 0});
  /* (17 18 19) subsumes; (21 22 23) and (21 22 -23) strengthen to (21 22) */
  add_clause(s, (const int[]){17, 18, 19, 20, 0});
  add_clause(s, (const int[]){17, 18, 19, 0});
  add_clause(s, (const int[]){21, 22, 23, 0});
  add_clause(s, (const int[]){21, 22, -23, 0});
  /* 24 occurs positively only: every clause holding it is blocked */
  add_clause(s, (const int[]){24, 25, 26, 0});
  add_clause(s, (const int[]){24, -25, 0});
}

/* The 3x3 exact cover: at most one per block, every position and every
 * diagonal covered. */
static inline void exact_cover_fixture(tactix *s) {
  int block, first, second, position, diagonal;
  for (block = 0; block < 3; ++block)
    for (first = 0; first < 3; ++first)
      for (second = first + 1; second < 3; ++second)
        add_clause(s, (const int[]){-(3 * block + first + 1),
                                    -(3 * block + second + 1), 0});
  for (position = 0; position < 3; ++position)
    add_clause(s, (const int[]){position + 1, position + 4,
                                position + 7, 0});
  for (diagonal = 0; diagonal < 3; ++diagonal) {
    int clause[4];
    for (block = 0; block < 3; ++block)
      clause[block] = 3 * block + (block + diagonal) % 3 + 1;
    clause[3] = 0;
    add_clause(s, clause);
  }
}

#endif
