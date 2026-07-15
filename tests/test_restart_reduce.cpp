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
  // guard (soundness) holds at any level > 0; scheduling lives in the
  // policy object (plan rev 17), not the rule
  assert(restart_guard(k));
  DefaultPolicy p;
  assert(!p.want_restart(k)); // conflicts 0 < the initial limit
  p.averages[0].initialized = true; // pose degrading focused search:
  p.averages[0].fast_glue.value = 10.0; // fast >> margin * slow
  p.averages[0].slow_glue.value = 4.0;
  k.stats.conflicts = p.restart.next_conflicts;
  assert(p.want_restart(k));
  assert(restart_apply(k) == ApplyResult::Ok);
  assert(k.level == 0);
  assert(k.values[internalize(1)] == 0); // decision undone
  assert(k.stats.restarts == 1);
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
  assert(proof.open(path, ProofFormat::DratAscii));
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
  DefaultPolicy rp;
  k.stats.conflicts = rp.reduce.next_conflicts;
  assert(reduce_guard(k) && rp.want_reduce(k));
  assert(reduce_apply(k) == ApplyResult::Ok);
  assert(k.stats.deleted == 3); // 75% of the four candidates
  proof.flush();
  FILE *f = fopen(path, "r");
  char buf[256];
  int dels = 0;
  while (fgets(buf, sizeof buf, f))
    if (buf[0] == 'd')
      dels++;
  fclose(f);
  remove(path);
  assert(dels == 3); // every deletion hit the proof
  // the policy owns the budget now: advancing it stops the advice
  rp.reduce.next_conflicts = k.stats.conflicts + 2000 + 300 * k.stats.reductions;
  assert(!rp.want_reduce(k)); // budget pushed out (guard still holds)
}

static void test_reduce_used_retention() {
  // kissat reduce.c recency scheme: tier2 (glue <= 6) survives while
  // used within the last two reductions; aging happens per application
  Kernel k;
  kernel_reserve_vars(k, 6);
  const Lit c1[3] = {internalize(1), internalize(2), internalize(3)};
  const Ref tier2 = kernel_add_large_clause(k, c1, 3, true, 4);
  const Lit c2[3] = {internalize(4), internalize(5), internalize(6)};
  kernel_add_large_clause(k, c2, 3, true, 9); // high glue: never protected
  assert(clause_at(k.arena, tier2)->used() == Clause::MAX_USED);

  std::vector<Ref> cands;
  reduce_candidates(k, cands);
  assert(cands.size() == 1); // only the high-glue clause; tier2 protected
  // enumeration must be side-effect-free (N1 introspection surface)
  assert(clause_at(k.arena, tier2)->used() == Clause::MAX_USED);

  // two applications age used MAX -> MAX-2: tier2 protection lapses
  reduce_apply_refs(k, nullptr, 0);
  reduce_candidates(k, cands);
  assert(cands.size() == 1); // used == MAX-1: still protected
  reduce_apply_refs(k, nullptr, 0);
  reduce_candidates(k, cands);
  assert(cands.size() == 2); // protection lapsed; both are candidates
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
  // restart storm: permanently-degraded EMAs + zero floor, posed on the
  // policy instance (rev 17: schedules are policy state)
  DefaultPolicy storm;
  // frozen degraded EMAs: alpha stays 0 (never init'ed), so updates
  // cannot wash the posed biased values out — permanent restart pressure
  storm.averages[0].initialized = true;
  storm.averages[0].fast_glue.biased = 100.0;
  storm.averages[0].fast_glue.value = 100.0;
  storm.averages[0].slow_glue.biased = 1.0;
  storm.averages[0].slow_glue.value = 1.0;
  storm.restart.next_conflicts = 0;
  storm.reduce.next_conflicts = 4; // and aggressive reduction
  assert(storm.solve(k) == 20);
}

static void test_restart_reuses_trail() {
  // vars 0,1,2 get VMTF stamps 1,2,3; bumping var 0 restamps it to 4.
  // Decide var 0 (stamp 4), then var 1 (stamp 2). Next decision candidate
  // is var 2 (stamp 3), so a restart keeps level 1 (4 > 3) and abandons
  // level 2 (2 <= 3).
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_queue_bump(k, 0);
  kernel_assign_decision(k, internalize(1)); // var 0
  kernel_assign_decision(k, internalize(2)); // var 1
  assert(restart_apply(k) == ApplyResult::Ok);
  assert(k.level == 1);
  assert(k.values[internalize(1)] > 0);  // level-1 prefix kept
  assert(k.values[internalize(2)] == 0); // level 2 abandoned
  assert(k.stats.restarts_reused == 1);

  // with var 1 also fresher than any candidate, the whole trail survives
  kernel_queue_bump(k, 1); // stamp 5 > var 2's 3
  kernel_assign_decision(k, internalize(2));
  assert(restart_apply(k) == ApplyResult::Ok);
  assert(k.level == 2); // full reuse: no backtrack at all
  assert(k.stats.restarts_reused == 2);
  assert(k.stats.restarts == 2);
}

static void test_restart_apply_to() {
  // N1 instance surface: the keep-level is the policy's choice; the
  // canonical instance keeps restart_reuse_level(k)
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_queue_bump(k, 0); // var 0 freshest (stamp 4 > var 2's 3)
  kernel_assign_decision(k, internalize(1)); // var 0 at level 1
  kernel_assign_decision(k, internalize(2)); // var 1 at level 2
  assert(restart_reuse_level(k) == 1); // canonical would keep level 1...
  assert(restart_apply_to(k, 0) == ApplyResult::Ok); // ...full restart anyway
  assert(k.level == 0);
  assert(k.values[internalize(1)] == 0); // nothing kept
  assert(k.stats.restarts == 1);
  assert(k.stats.restarts_reused == 0);
  // keep-levels above the current level clamp to it (nothing abandoned)
  kernel_assign_decision(k, internalize(3));
  assert(restart_apply_to(k, 5) == ApplyResult::Ok);
  assert(k.level == 1);
  assert(k.values[internalize(3)] > 0);
  assert(k.stats.restarts == 2);
  assert(k.stats.restarts_reused == 1);
}

int main() {
  test_restart();
  test_restart_apply_to();
  test_restart_reuses_trail();
  test_collect_preserves_propagation();
  test_reduce_deletes_and_logs();
  test_reduce_used_retention();
  test_solve_with_restarts_still_correct();
  puts("test_restart_reduce OK");
  return 0;
}
