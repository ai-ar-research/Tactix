// S1 Restart / S2 Reduce / arena collection unit tests.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/reduce/reduce.hpp"
#include "rules/restart/restart.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>

using namespace tactix;

static void test_restart() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_assign_decision(k, internalize(1));
  assert(propagate_apply(k) == ApplyResult::Ok);
  assert(!restart_guard(k)); // EMAs not initialized yet
  k.restart.ema_initialized = true;
  k.restart.fast_glue = 10.0; // degrading search: fast >> slow
  k.restart.slow_glue = 4.0;
  k.stats.conflicts = k.restart.floor_conflicts;
  assert(restart_guard(k));
  assert(restart_apply(k) == ApplyResult::Ok);
  assert(k.level == 0);
  assert(k.values[internalize(1)] == 0); // decision undone
  assert(k.stats.restarts == 1);
  assert(k.restart.floor_conflicts > k.stats.conflicts); // floor pushed out
  assert(!restart_guard(k)); // level 0 now
}

static void test_collect_preserves_propagation() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit c1[3] = {internalize(1), internalize(2), internalize(3)};
  const Lit c2[3] = {internalize(1), internalize(2), internalize(4)};
  const Ref r1 = kernel_add_large_clause(k, c1, 3, true, 5);
  kernel_add_large_clause(k, c2, 3, true, 5);
  clause_at(k.arena, r1)->flags |= Clause::GARBAGE;
  kernel_collect_garbage(k);
  // c2 must still propagate: set 1,2 false -> forces 4 (and only 4)
  kernel_assign_unit(k, internalize(-1));
  kernel_assign_unit(k, internalize(-2));
  assert(propagate_apply(k) == ApplyResult::Ok);
  assert(k.values[internalize(4)] > 0);
  assert(k.values[internalize(3)] == 0); // c1 is gone
}

static void test_reduce_deletes_and_logs() {
  Kernel k;
  Proof proof;
  char path[] = "/tmp/tactix_reduce_test.drat";
  assert(proof.open(path, /*binary=*/false));
  k.proof = &proof;
  kernel_reserve_vars(k, 12);
  // four high-glue redundant clauses on distinct vars, plus one glue-1 keeper
  for (int i = 0; i < 4; i++) {
    const Lit c[3] = {internalize(3 * i + 1), internalize(3 * i + 2),
                      internalize(3 * i + 3)};
    kernel_add_large_clause(k, c, 3, true, 8 + i);
  }
  const Lit keeper[3] = {internalize(1), internalize(5), internalize(9)};
  const Ref kref = kernel_add_large_clause(k, keeper, 3, true, 1);
  (void)kref;
  k.stats.conflicts = k.reduce.next_conflicts;
  assert(reduce_guard(k));
  assert(reduce_apply(k) == ApplyResult::Ok);
  assert(k.stats.deleted == 2); // half of the four candidates
  proof.flush();
  FILE *f = fopen(path, "r");
  char buf[256];
  int dels = 0;
  while (fgets(buf, sizeof buf, f))
    if (buf[0] == 'd')
      dels++;
  fclose(f);
  remove(path);
  assert(dels == 2); // every deletion hit the proof
  assert(!reduce_guard(k)); // budget pushed out
}

static void test_solve_with_restarts_still_correct() {
  // PHP(3) pigeons=4 holes=3: UNSAT even with tiny restart intervals
  Kernel k;
  kernel_reserve_vars(k, 12);
  auto var = [](int p, int h) { return p * 3 + h + 1; };
  std::vector<Lit> scratch;
  for (int p = 0; p < 4; p++) {
    scratch.clear();
    for (int h = 0; h < 3; h++)
      scratch.push_back(internalize(var(p, h)));
    kernel_add_large_clause(k, scratch.data(), 3, false, 0);
  }
  for (int h = 0; h < 3; h++)
    for (int p1 = 0; p1 < 4; p1++)
      for (int p2 = p1 + 1; p2 < 4; p2++)
        kernel_add_binary_clause(k, internalize(-var(p1, h)),
                                 internalize(-var(p2, h)));
  // restart storm: permanently-degraded EMAs + zero floor
  k.restart.ema_initialized = true;
  k.restart.fast_glue = 100.0;
  k.restart.slow_glue = 1.0;
  k.restart.floor_conflicts = 0;
  k.reduce.next_conflicts = 4; // and aggressive reduction
  assert(default_solve(k) == 20);
}

int main() {
  test_restart();
  test_collect_preserves_propagation();
  test_reduce_deletes_and_logs();
  test_solve_with_restarts_still_correct();
  puts("test_restart_reduce OK");
  return 0;
}
