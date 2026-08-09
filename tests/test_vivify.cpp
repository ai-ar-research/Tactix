// Vivify unit tests: the three probe outcomes, root-satisfied deletion, tier
// scope, the instance surface, and exact state restoration.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/vivify/vivify.hpp"

#include <cassert>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <unistd.h>
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
  // assuming ¬1 propagates 2, so C distills to a fresh binary (1 2)
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
  // assuming ¬1 propagates ¬2, so literal 2 is redundant in C
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
  // assuming ¬1 and ¬2 conflicts, so the RUP clause (1 2) replaces C
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
  // Scope 1 (the LCM band) admits tier-2 and nothing beyond.
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  const Ref r1 = kernel_add_large_clause(k, c, 3, true, /*glue=*/2);
  const Lit d[3] = {internalize(1), internalize(3), internalize(4)};
  kernel_add_large_clause(k, d, 3, true, /*glue=*/9); // beyond tier2
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  std::vector<Ref> cands;
  k.vivify_scope = 1;
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

// Each scope selects exactly its glue band, tracking the live tier bounds.
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
  only(5, r3);        // tier3: glue > tier2
  k.vivify_scope = 1; // redundant band stops at tier2
  vivify_candidates(k, cands);
  assert(cands.size() == 2);
  k.vivify_scope = 0;          // unscoped spans every band, so that no band
  vivify_candidates(k, cands); // is API-unreachable (N1)
  assert(cands.size() == 3);
  k.tier2 = 10; // the bands move with the live tier bound, emptying tier3
  k.vivify_scope = 5;
  vivify_candidates(k, cands);
  assert(cands.empty());
}

// The leftover bit selects the untried tail, and re-arms when empty.
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
  // nothing armed: the round arms everything, so the bit never suppresses
  vivify_apply(k);
  assert(k.stats.vivify_reprioritized == 1);
  assert(k.stats.vivify_prioritized == 3);
  for (const Ref r : refs)
    assert(!clause_at(k.arena, r)->vivify());
  clause_at(k.arena, refs[2])->set_vivify(true); // armed candidates come first
  vivify_candidates(k, cands);
  assert(cands.size() == 3 && cands[0] == refs[2]);
}

// Candidates sharing a leading literal reuse the probe assumption; with
// reuse off every candidate returns to the root instead.
static void test_trail_reuse() {
  Kernel k;
  kernel_reserve_vars(k, 8);
  // a shared head, so the global literal order puts 1 and 2 first in all three
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
  assert(k.stats.vivify_reused > 0);            // but the prefix was shared
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
  assert(k.level == 0 && k2.level == 0 && !k.conflict_valid); // root is clean
}

static void test_schedule_prefix_flush() {
  const Lit short_clause[3] = {internalize(1), internalize(2), internalize(3)};
  const Lit long_clause[4] = {internalize(1), internalize(2), internalize(3),
                              internalize(4)};

  Kernel k;
  kernel_reserve_vars(k, 4);
  k.vivify_flush = true;
  kernel_add_large_clause(k, short_clause, 3, false, 0);
  kernel_add_large_clause(k, long_clause, 4, false, 0);
  char path[] = "/tmp/tactix_test_vivify_flush_XXXXXX";
  const int fd = mkstemp(path);
  assert(fd >= 0);
  close(fd);
  Proof proof;
  assert(proof.open(path, ProofFormat::DratAscii));
  k.proof = &proof;
  std::vector<Ref> cands;
  vivify_candidates(k, cands);
  assert(cands.size() == 2); // N1: scheduling never hides either instance
  volatile sig_atomic_t stop = 1;
  k.stop = &stop; // isolate the pre-propagation schedule flush
  assert(vivify_apply(k) == ApplyResult::Ok);
  assert(k.stats.vivify_flushed == 1);
  assert(k.stats.vivify_checked == 0);
  assert(live_large(k, short_clause, 3) == 1);
  proof.flush();
  k.proof = nullptr;
  FILE *f = fopen(path, "r");
  assert(f);
  char line[128];
  bool saw_delete = false;
  while (fgets(line, sizeof line, f))
    saw_delete |= line[0] == 'd' && std::strstr(line, "4") != nullptr;
  fclose(f);
  remove(path);
  assert(saw_delete);

  Kernel off;
  off.vivify_flush = false;
  kernel_reserve_vars(off, 4);
  kernel_add_large_clause(off, short_clause, 3, false, 0);
  kernel_add_large_clause(off, long_clause, 4, false, 0);
  off.stop = &stop;
  assert(vivify_apply(off) == ApplyResult::NoOp);
  assert(off.stats.vivify_flushed == 0 && live_large(off) == 2);
}

static void test_schedule_flush_partition() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  const Lit redundant_prefix[3] = {internalize(1), internalize(2),
                                   internalize(3)};
  const Lit irredundant_clause[4] = {internalize(1), internalize(2),
                                     internalize(3), internalize(4)};
  const Lit tier2_clause[4] = {internalize(1), internalize(2), internalize(3),
                               internalize(5)};
  kernel_add_large_clause(k, redundant_prefix, 3, true, 2);
  kernel_add_large_clause(k, irredundant_clause, 4, false, 0);
  kernel_add_large_clause(k, tier2_clause, 4, true, 5);
  k.vivify_scope = 0;
  volatile sig_atomic_t stop = 1;
  k.stop = &stop;
  assert(vivify_apply(k) == ApplyResult::NoOp);
  assert(k.stats.vivify_flushed == 0);
  assert(live_large(k) == 3); // neither partitions nor tier bands are crossed
}

// The termination poll is cadence-free: a raised stop is both reached and
// honoured on the first candidate, before any work.
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
  test_schedule_prefix_flush();
  test_schedule_flush_partition();
  test_termination_poll();
  puts("test_vivify OK");
  return 0;
}
