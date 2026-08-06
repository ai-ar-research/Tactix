// C3 Vivify unit tests: the three probe outcomes (implied literal,
// implied negation, probe conflict), root-satisfied deletion, LCM tier
// scope, instance surface, and exact state restoration.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/vivify/vivify.hpp"

#include <cassert>
#include <csignal>
#include <cstdio>
#include <vector>

using namespace tactix;

static uint32_t live_large(Kernel &k, const Lit *lits = nullptr,
                           uint32_t n = 0) {
  uint32_t count = 0;
  bool found = false;
  for (Ref ref = 0; ref < k.arena.size();) {
    Clause *c = clause_at(k.arena, ref);
    if (!c->garbage()) {
      count++;
      if (lits && c->size == n) {
        bool all = true;
        for (uint32_t i = 0; i < n; i++) {
          bool has = false;
          for (const Lit l : *c)
            has |= (l == lits[i]);
          all &= has;
        }
        found |= all;
      }
    }
    ref += clause_words(c->size);
  }
  if (lits)
    assert(found);
  return count;
}

static void test_implied_literal_strengthens() {
  // C = (1 2 3) with binary (1 2): assuming ¬1 propagates 2 — C distills
  // to (1 2), stored as a fresh binary; C dies
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, c, 3, false, 0);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  assert(vivify_guard(k));
  assert(vivify_apply(k) == ApplyResult::Ok);
  assert(k.stats.vivified == 1);
  assert(live_large(k) == 0);
  assert(k.level == 0 && !k.conflict_valid); // state restored
  assert(default_solve(k) == 10);
}

static void test_implied_negation_removes() {
  // C = (1 2 3) with binary (1 -2): assuming ¬1 propagates ¬2 — literal 2
  // is redundant in C; C distills to (1 3)
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, c, 3, false, 0);
  kernel_add_binary_clause(k, internalize(1), internalize(-2));
  assert(vivify_apply(k) == ApplyResult::Ok);
  assert(k.stats.vivified == 1);
  assert(live_large(k) == 0); // (1 3) became a binary
  assert(default_solve(k) == 10);
}

static void test_probe_conflict_strengthens() {
  // C = (1 2 3) with binaries (1 4) and (2 -4): assuming ¬1 gives 4,
  // assuming ¬2 gives ¬4 — conflict, so (1 2) is RUP and replaces C
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, c, 3, false, 0);
  kernel_add_binary_clause(k, internalize(1), internalize(4));
  kernel_add_binary_clause(k, internalize(2), internalize(-4));
  assert(vivify_apply(k) == ApplyResult::Ok);
  assert(k.stats.vivified == 1);
  assert(live_large(k) == 0);
  assert(k.level == 0 && !k.conflict_valid);
  assert(default_solve(k) == 10);
}

static void test_lcm_scope_and_satisfied_delete() {
  // a redundant tier-2 clause IS vivified (LCM scope); a redundant clause
  // beyond tier2 is not a candidate. A root-satisfied candidate is
  // deleted through the instance surface.
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  const Ref r1 = kernel_add_large_clause(k, c, 3, true, /*glue=*/2);
  const Lit d[3] = {internalize(1), internalize(3), internalize(4)};
  kernel_add_large_clause(k, d, 3, true, /*glue=*/9); // beyond tier2
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  std::vector<Ref> cands;
  vivify_candidates(k, cands);
  assert(cands.size() == 1 && cands[0] == r1); // LCM scope: tier2 only
  assert(vivify_apply(k) == ApplyResult::Ok);  // r1 distilled to (1 2)
  assert(k.stats.vivified == 1);

  // satisfied deletion via the instance surface
  Kernel k2;
  kernel_reserve_vars(k2, 3);
  kernel_assign_unit(k2, internalize(1));
  const Lit e[3] = {internalize(1), internalize(2), internalize(3)};
  const Ref r2 = kernel_add_large_clause(k2, e, 3, false, 0);
  assert(propagate_apply(k2) == ApplyResult::Ok);
  assert(vivify_try(k2, r2) == ApplyResult::Ok);
  assert(k2.stats.vivify_deleted == 1);
  assert(live_large(k2) == 0);
}

static void test_instance_surface() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  const Ref cref = kernel_add_large_clause(k, c, 3, false, 0);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  const std::vector<RuleVTable> &rules = tactix_rules();
  const int id = tactix_rule_id("Vivify");
  assert(id >= 0 && rules[id].tier == RuleTier::Inprocess);
  uint64_t insts[4];
  const uint32_t n = rules[id].instances(k, insts, 4);
  assert(n == 1 && insts[0] == cref);
  assert(rules[id].apply_instance(k, insts[0]) == ApplyResult::Ok);
  assert(k.stats.vivified == 1);
  assert(rules[id].apply_instance(k, 999999) == ApplyResult::NoOp);
}

// --- SCHED-G3 -------------------------------------------------------------

// C10 hand-off H3: a redundant clause above tier2 was never a candidate under
// ANY scope, although the policy sized the redundant share as kissat's 3+3+1.
static void test_tier3_scope() {
  Kernel k;
  kernel_reserve_vars(k, 6);
  const Lit t1[3] = {internalize(1), internalize(2), internalize(3)};
  const Ref r1 = kernel_add_large_clause(k, t1, 3, true, /*glue=*/2);
  const Lit t2[3] = {internalize(1), internalize(4), internalize(5)};
  const Ref r2 = kernel_add_large_clause(k, t2, 3, true, /*glue=*/5);
  const Lit t3[3] = {internalize(2), internalize(4), internalize(6)};
  const Ref r3 = kernel_add_large_clause(k, t3, 3, true, /*glue=*/9);
  std::vector<Ref> cands;
  auto only = [&](uint8_t scope, Ref want) {
    k.vivify_scope = scope;
    vivify_candidates(k, cands);
    assert(cands.size() == 1 && cands[0] == want);
  };
  only(3, r1); // tier1: glue <= k.tier1 (2)
  only(4, r2); // tier2: tier1 < glue <= k.tier2 (6)
  only(5, r3); // tier3: glue > tier2 — EMPTY before this item
  k.vivify_scope = 1; // legacy redundant band stops at tier2, as it always did
  vivify_candidates(k, cands);
  assert(cands.size() == 2);
  k.vivify_scope = 0; // and so does the unscoped N1 path
  vivify_candidates(k, cands);
  assert(cands.size() == 2);
  // and the band MOVES with the live tier (SCHED-C1), so tier3 is not a
  // constant set either: raising tier2 past 9 empties it
  k.tier2 = 10;
  k.vivify_scope = 5;
  vivify_candidates(k, cands);
  assert(cands.empty());
}

// SCHED-F5: the leftover bit selects the untried tail, and re-arms when empty.
static void test_leftover_priority() {
  Kernel k;
  kernel_reserve_vars(k, 9);
  std::vector<Ref> refs;
  for (uint32_t i = 0; i < 3; i++) {
    const Lit c[3] = {internalize(1 + 3 * (int)i), internalize(2 + 3 * (int)i),
                      internalize(3 + 3 * (int)i)};
    refs.push_back(kernel_add_large_clause(k, c, 3, false, 0));
  }
  std::vector<Ref> cands;
  // nothing is armed yet, so the round arms everything (never a suppressor)
  vivify_apply(k);
  assert(k.stats.vivify_reprioritized == 1);
  assert(k.stats.vivify_prioritized == 3);
  // ...and every candidate was tried, so the bits are all clear again
  for (const Ref r : refs)
    assert(!clause_at(k.arena, r)->vivify());
  // arm exactly one and it must come first
  clause_at(k.arena, refs[2])->set_vivify(true);
  vivify_candidates(k, cands);
  assert(cands.size() == 3 && cands[0] == refs[2]);
}

// The mechanism itself: two candidates sharing a leading literal reuse the
// assumption, and the off arm does not. A reuse rate of zero here would mean
// the loop never fires.
static void test_trail_reuse() {
  Kernel k;
  kernel_reserve_vars(k, 8);
  // three clauses over a shared, high-occurrence head (1 2 ...) so the global
  // literal order puts 1 and 2 first in all of them
  const Lit a[4] = {internalize(1), internalize(2), internalize(3),
                    internalize(4)};
  const Lit b[4] = {internalize(1), internalize(2), internalize(5),
                    internalize(6)};
  const Lit c[4] = {internalize(1), internalize(2), internalize(7),
                    internalize(8)};
  kernel_add_large_clause(k, a, 4, false, 0);
  kernel_add_large_clause(k, b, 4, false, 0);
  kernel_add_large_clause(k, c, 4, false, 0);
  assert(vivify_apply(k) == ApplyResult::NoOp); // nothing to derive here
  assert(k.stats.vivify_reused > 0);            // ...but the prefix was shared
  const uint64_t on = k.stats.vivify_reused;

  Kernel k2;
  kernel_reserve_vars(k2, 8);
  k2.vivify_reuse = false;
  kernel_add_large_clause(k2, a, 4, false, 0);
  kernel_add_large_clause(k2, b, 4, false, 0);
  kernel_add_large_clause(k2, c, 4, false, 0);
  assert(vivify_apply(k2) == ApplyResult::NoOp);
  assert(k2.stats.vivify_reused == 0); // the off arm returns to the root
  assert(on > 0 && k2.stats.vivify_probes > k.stats.vivify_probes);
  // and the round leaves the root clean either way
  assert(k.level == 0 && k2.level == 0 && !k.conflict_valid);
}

// SCHED-H1 (Vivify half). The poll has no cadence, so "the site was reached"
// and "the site fired" are both non-zero on the first round that sees a
// raised stop — the failure mode this replaces is a poll window an instance
// never reaches.
static void test_termination_poll() {
  Kernel k;
  kernel_reserve_vars(k, 9);
  for (uint32_t i = 0; i < 3; i++) {
    const Lit c[3] = {internalize(1 + 3 * (int)i), internalize(2 + 3 * (int)i),
                      internalize(3 + 3 * (int)i)};
    kernel_add_large_clause(k, c, 3, false, 0);
  }
  volatile sig_atomic_t stop = 1;
  k.stop = &stop;
  const ApplyResult r = vivify_apply(k);
  k.stop = nullptr;
  assert(r == ApplyResult::NoOp);
  assert(k.stats.vivify_polls == 1);      // reached on the first candidate
  assert(k.stats.vivify_terminated == 1); // and fired there
  assert(k.stats.vivify_checked == 0);    // before any work was done
  assert(k.level == 0);                   // and the root is clean
}

int main() {
  test_implied_literal_strengthens();
  test_implied_negation_removes();
  test_probe_conflict_strengthens();
  test_lcm_scope_and_satisfied_delete();
  test_instance_surface();
  test_tier3_scope();
  test_leftover_priority();
  test_trail_reuse();
  test_termination_poll();
  puts("test_vivify OK");
  return 0;
}
