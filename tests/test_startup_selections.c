/* Caller-selected startup refutations: matching, parity and counter cover.
 * Each action refutes its selection, declines one that does not validate
 * (after which ordinary solving still decides the formula), and rejects
 * malformed arguments. */
#undef NDEBUG
#include "tactix.h"

#include <assert.h>
#include <stdlib.h>

static tactix *session(const int *lits, size_t n) {
  tactix *s = tactix_init();
  size_t i;
  for (i = 0; i < n; ++i) assert(!tactix_add_checked(s, lits[i]));
  return s;
}

#define SESSION(...) session((const int[]){__VA_ARGS__}, sizeof((const int[]){__VA_ARGS__}) / sizeof(int))

static void finish(tactix *s, int expected) {
  assert(tactix_solve(s) == expected);
  tactix_release(s);
}

/* PHP(3,2) with scrambled signs: pigeons {1,-2}, {-3,4}, {5,6}; holes
 * {1,-3,5} and {-2,4,6}. Option 7 of a fourth demand fails by propagation. */
#define PHP 1, -2, 0, -3, 4, 0, 5, 6, 0, -1, 3, 0, -1, -5, 0, 3, -5, 0, 2, -4, 0, 2, -6, 0, -4, -6, 0
static void matching(void) {
  const int demands[] = {1, -2, 0, -3, 4, 0, 5, 6, 0}, pools[] = {1, -3, 5, 0, -2, 4, 6, 0};
  const int with_failed[] = {1, -2, 0, -3, 4, 0, 5, 6, 7, 0}, failed[] = {7};
  tactix *s = SESSION(PHP);
  assert(tactix_apply_startup_matching(s, demands, 9, pools, 8, NULL, 0) == 20);
  assert(tactix_status(s) == 20);
  tactix_release(s);
  /* As many pools as demands: no count. */
  s = SESSION(PHP);
  assert(tactix_apply_startup_matching(s, demands, 6, pools, 8, NULL, 0) == 1);
  finish(s, 20);
  /* A literal in two pools. */
  s = SESSION(PHP);
  assert(tactix_apply_startup_matching(s, demands, 9, (const int[]){1, -3, 5, 0, 1, -2, 4, 6, 0}, 9,
                                       NULL, 0) == 1);
  finish(s, 20);
  /* Option 7 is uncovered unless its refutation is supplied. */
  s = SESSION(1, -2, 0, -3, 4, 0, 5, 6, 7, 0, -1, 3, 0, -1, -5, 0, 3, -5, 0, 2, -4, 0, 2, -6, 0,
              -4, -6, 0, -7, 8, 0, -7, -8, 0);
  assert(tactix_apply_startup_matching(s, with_failed, 10, pools, 8, NULL, 0) == 1);
  assert(tactix_apply_startup_matching(s, with_failed, 10, pools, 8, failed, 1) == 20);
  tactix_release(s);
  s = SESSION(PHP);
  assert(tactix_apply_startup_matching(s, demands, 8, pools, 8, NULL, 0) == -1);
  tactix_release(s);
}

/* Odd triangle: x1 ^ x2 = 1, x1 ^ x3 = 0, x2 ^ x3 = 0 sums to 0 = 1. */
#define TRIANGLE 1, 2, 0, -1, -2, 0, 1, -3, 0, -1, 3, 0, 2, -3, 0, -2, 3, 0
static void parity(void) {
  const int all[] = {1, 2, 0, 1, 3, 0, 2, 3, 0}, two[] = {1, 2, 0, 1, 3, 0};
  tactix *s = SESSION(TRIANGLE);
  assert(tactix_apply_startup_parity_selection(s, all, 9) == 20);
  assert(tactix_status(s) == 20);
  tactix_release(s);
  s = SESSION(TRIANGLE);
  assert(tactix_apply_startup_parity_selection(s, two, 6) == 1); /* x2 and x3 stay odd */
  finish(s, 20);
  s = SESSION(TRIANGLE, 1, 4, 0);
  assert(tactix_apply_startup_parity_selection(s, (const int[]){1, 4, 0}, 3) == 1); /* incomplete */
  finish(s, 20);
  s = SESSION(TRIANGLE);
  assert(tactix_apply_startup_parity_selection(s, all, 8) == -1);
  tactix_release(s);
}

/* At most two of K4's vertices are true, so two are false, but the clique
 * {1,2,3,4} holds at most one false vertex. Registers: s_{1,j} = 5,6;
 * s_{2,j} = 7,8; s_{3,j} = 9,10. Without edge {2,4}, cover {1,3} exists. */
#define COUNTER                                                                          \
  -1, 5, 0, -6, 0, -2, 7, 0, -5, 7, 0, -2, -5, 8, 0, -6, 8, 0, -2, -6, 0, -3, 9, 0, -7, 9, 0, \
      -3, -7, 10, 0, -8, 10, 0, -3, -8, 0, -4, -10, 0
#define K4_MINUS 1, 2, 0, 1, 3, 0, 1, 4, 0, 2, 3, 0, 3, 4, 0, COUNTER
static void counter_cover(void) {
  const int vertices[] = {1, 2, 3, 4}, registers[] = {5, 6, 7, 8, 9, 10};
  const int clique[] = {1, 2, 3, 4, 0}, halves[] = {1, 2, 3, 0, 4, 0}, weights[] = {1, 1};
  tactix *s = SESSION(K4_MINUS, 2, 4, 0);
  assert(tactix_apply_startup_counter_cover(s, vertices, 4, registers, 2, clique, 5, weights, 1) ==
         20);
  assert(tactix_status(s) == 20);
  tactix_release(s);
  /* Two cliques allow two false vertices: no contradiction. */
  s = SESSION(K4_MINUS, 2, 4, 0);
  assert(tactix_apply_startup_counter_cover(s, vertices, 4, registers, 2, halves, 6, weights, 1) ==
         1);
  finish(s, 20);
  /* Without edge {2,4} the four vertices are no clique, and the input is SAT. */
  s = SESSION(K4_MINUS);
  assert(tactix_apply_startup_counter_cover(s, vertices, 4, registers, 2, clique, 5, weights, 1) ==
         1);
  finish(s, 10);
  s = SESSION(K4_MINUS, 2, 4, 0);
  assert(tactix_apply_startup_counter_cover(s, vertices, 4, registers, 0, clique, 5, weights, 1) ==
         -1);
  tactix_release(s);
}

int main(void) {
  matching();
  parity();
  counter_cover();
  return 0;
}
