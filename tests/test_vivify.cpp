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
  assert(k.stats.vivify_subsumed == 1);
  assert(live_large(k) == 0);
  assert(k.level == 0 && !k.conflict_valid); // state restored
  assert(default_solve(k) == 10);
}

// Reason jumping records the virtual resolvent (1 v 2) while retaining 4 as
// the resolved-away proof antecedent.  Vivify must use the virtual reason for
// semantic subsumption and expand through 4 only when emitting a proof chain.
static void test_jumped_binary_reason_subsumes_candidate() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  k.jump_reasons_bigbig = true;
  const Lit candidate_lits[3] = {internalize(1), internalize(2),
                                 internalize(3)};
  kernel_add_large_clause(k, candidate_lits, 3, false, 0);
  kernel_add_binary_clause(k, internalize(1), internalize(4));
  kernel_add_binary_clause(k, internalize(-4), internalize(2));

  assert(vivify_apply(k) == ApplyResult::Ok);
  assert(k.stats.vivify_subsumed == 1);
  assert(k.stats.vivify_shrunken == 0 && k.stats.vivified == 0);
  assert(live_large(k) == 0 && k.level == 0 && !k.conflict_valid);
  assert(default_solve(k) == 10);
}

static void test_implied_negation_removes() {
  // assuming ¬1 propagates ¬2, so literal 2 is redundant in C
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  const Ref candidate = kernel_add_large_clause(k, c, 3, false, 0);
  kernel_add_binary_clause(k, internalize(1), internalize(-2));
  // This is a terminal-arm unit, not a scheduling test.  The canonical round
  // sorts equal occurrence counts by descending raw literal like Kissat,
  // whereas the instance surface deliberately probes the requested clause
  // pose and isolates the implied-negation transformation.
  assert(vivify_try(k, candidate) == ApplyResult::Ok);
  assert(k.stats.vivified == 1 && k.stats.vivify_shrunken == 1);
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
  assert(k.stats.vivified == 1 && k.stats.vivify_shrunken == 1);
  assert(live_large(k) == 0);
  assert(k.level == 0 && !k.conflict_valid);
  assert(default_solve(k) == 10);
}

static void add_clause(Kernel &k, std::initializer_list<int> xs,
                       bool redundant = false, uint32_t glue = 0) {
  std::vector<Lit> lits;
  for (const int x : xs)
    lits.push_back(internalize(x));
  kernel_add_large_clause(k, lits.data(), static_cast<uint32_t>(lits.size()),
                          redundant, glue);
}

// One focused witness for each terminal arm in kissat vivify.c:1124-1200.
// All use the instance surface so candidate scheduling cannot hide an arm.
static void test_terminal_dispatch() {
  { // irredundant candidate, redundant large subsumer: promote then delete
    Kernel k;
    kernel_reserve_vars(k, 4);
    const Lit lits[3] = {internalize(1), internalize(2), internalize(3)};
    const Ref candidate = kernel_add_large_clause(k, lits, 3, false, 0);
    kernel_add_large_clause(k, lits, 3, true, 2);
    assert(vivify_try(k, candidate) == ApplyResult::Ok);
    assert(k.stats.vivify_subsumed == 1 && k.stats.vivify_promoted == 1);
    bool saw_irredundant = false;
    for (Ref r = 0; r < k.arena.size();) {
      const Clause *c = clause_at(k.arena, r);
      saw_irredundant |= !c->garbage() && !c->redundant();
      r += clause_words(c->size);
    }
    assert(saw_irredundant && default_solve(k) == 10);
  }
  { // no strict shrink: a redundant independently-implied candidate dies
    Kernel k;
    k.vivify_reuse = false;
    kernel_reserve_vars(k, 4);
    const Lit lits[3] = {internalize(1), internalize(2), internalize(3)};
    const Ref candidate = kernel_add_large_clause(k, lits, 3, true, 2);
    kernel_add_binary_clause(k, internalize(1), internalize(-4));
    add_clause(k, {2, 3, 4});
    assert(vivify_try(k, candidate) == ApplyResult::Ok);
    assert(k.stats.vivify_implied == 1 && default_solve(k) == 10);
  }
  { // the same implication graph without redundant reasons is asymmetric
    Kernel k;
    k.vivify_reuse = false;
    kernel_reserve_vars(k, 4);
    const Lit lits[3] = {internalize(1), internalize(2), internalize(3)};
    const Ref candidate = kernel_add_large_clause(k, lits, 3, false, 0);
    kernel_add_binary_clause(k, internalize(1), internalize(-4));
    add_clause(k, {2, 3, 4});
    assert(vivify_try(k, candidate) == ApplyResult::Ok);
    assert(k.stats.vivify_asymmetric == 1 && default_solve(k) == 10);
  }
  { // a redundant antecedent forbids the asymmetric deletion: explicit no-op
    Kernel k;
    k.vivify_reuse = false;
    kernel_reserve_vars(k, 4);
    const Lit lits[3] = {internalize(1), internalize(2), internalize(3)};
    const Ref candidate = kernel_add_large_clause(k, lits, 3, false, 0);
    kernel_add_binary_clause(k, internalize(1), internalize(-4));
    add_clause(k, {2, 3, 4}, true, 2);
    assert(vivify_try(k, candidate) == ApplyResult::NoOp);
    assert(k.stats.vivify_noop == 1 && default_solve(k) == 10);
  }
  { // reversing the final assumption conflicts and removes that literal
    Kernel k;
    k.vivify_reuse = false;
    kernel_reserve_vars(k, 4);
    const Lit lits[3] = {internalize(1), internalize(2), internalize(3)};
    const Ref candidate = kernel_add_large_clause(k, lits, 3, false, 0);
    kernel_add_binary_clause(k, internalize(-3), internalize(4));
    kernel_add_binary_clause(k, internalize(-3), internalize(-4));
    assert(vivify_try(k, candidate) == ApplyResult::Ok);
    assert(k.stats.vivify_instantiated == 1 && default_solve(k) == 10);
  }
  { // with no consequence the reversed final assumption is a terminal no-op
    Kernel k;
    k.vivify_reuse = false;
    kernel_reserve_vars(k, 3);
    const Lit lits[3] = {internalize(1), internalize(2), internalize(3)};
    const Ref candidate = kernel_add_large_clause(k, lits, 3, false, 0);
    assert(vivify_try(k, candidate) == ApplyResult::NoOp);
    assert(k.stats.vivify_noop == 1 && default_solve(k) == 10);
  }
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
  assert(k.stats.vivify_subsumed == 1);

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
  assert(k.stats.vivify_subsumed == 1);
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

// A successful LARGE shrink allocates a replacement in Tactix.  Kissat edits
// the candidate in place, so parity requires explicitly removing the old
// candidate's watch pair before the arena record becomes garbage.  The public
// transaction compacts the arena before returning; validate the resulting
// live-watch graph (the journal regression separately pins the pre-collection
// propagation tick boundary).
static void test_large_rewrite_unwatches_candidate() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit candidate[4] = {internalize(1), internalize(2), internalize(3),
                            internalize(4)};
  const Ref rewritten = kernel_add_large_clause(k, candidate, 4, false, 0);
  kernel_add_binary_clause(k, internalize(1), internalize(-2));

  assert(vivify_try(k, rewritten) == ApplyResult::Ok);
  assert(k.stats.vivify_shrunken == 1);
  assert(live_large(k) == 1);
  for (Lit lit = 0; lit < 2 * k.nvars; lit++)
    for (const WatchEntry watch : k.watches.entries(lit))
      if (!watch.binary()) {
        assert(watch.ref() < k.arena.size());
        assert(!clause_at(k.arena, watch.ref())->garbage());
      }
  assert(k.level == 0 && !k.conflict_valid);
  assert(default_solve(k) == 10);
}

// After a conflict shrink, literals above the retained level are unassigned.
// Kissat's watch selector nevertheless scans past the first such literal and
// lets a later unassigned literal, then a higher retained-level false literal,
// win.  Stopping at the first unassigned tail changed the replacement pose.
static void test_replacement_watch_pose_scans_unassigned_tail() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  k.level = 1;
  kernel_assign(k, internalize(-1), 1, DECISION_REASON, false);
  k.level = 2;
  kernel_assign(k, internalize(-2), 2, DECISION_REASON, false);
  k.level = 3;
  kernel_assign(k, internalize(-5), 3, DECISION_REASON, false);
  std::vector<Lit> replacement = {
      internalize(1), internalize(2), internalize(3), internalize(4),
      internalize(5)};
  vivify_pose_replacement_watches(k, replacement);
  const std::vector<Lit> expected = {
      internalize(5), internalize(4), internalize(3), internalize(2),
      internalize(1)};
  assert(replacement == expected);
}

// Large instantiation is not an ordinary learned-clause rebuild.  Kissat
// removes the reversed literal in the candidate's physical order, retains the
// first size-2 assumption levels, and chooses watches in that state.  This
// pins the pose independently of the equivalent clause's literal set.
static void test_large_instantiation_retains_candidate_pose() {
  Kernel k;
  k.vivify_reuse = false;
  kernel_reserve_vars(k, 5);
  const Lit lits[4] = {internalize(1), internalize(2), internalize(3),
                       internalize(4)};
  const Ref candidate = kernel_add_large_clause(k, lits, 4, false, 0);
  kernel_add_binary_clause(k, internalize(-4), internalize(5));
  kernel_add_binary_clause(k, internalize(-4), internalize(-5));
  assert(vivify_try(k, candidate) == ApplyResult::Ok);
  assert(k.stats.vivify_instantiated == 1);
  const Clause *replacement = nullptr;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage() && c->size == 3)
      replacement = c;
    ref += clause_words(c->size);
  }
  assert(replacement);
  assert(replacement->lits[0] == internalize(3));
  assert(replacement->lits[1] == internalize(2));
  assert(default_solve(k) == 10);
}

static void test_large_rewrite_trims_without_implied_literal() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  for (int i = 1; i <= 4; i++)
    kernel_assign_decision(k, internalize(-i));
  vivify_trim_replacement_prefix(k, INVALID_LIT, 5);
  assert(k.level == 3 && k.trail.size() == 3);

  // An implied-literal rewrite keeps the probing prefix, matching Kissat's
  // distinct branch in vivify_learn_large.
  vivify_trim_replacement_prefix(k, internalize(5), 4);
  assert(k.level == 3 && k.trail.size() == 3);
}

static void test_schedule_prefix_flush() {
  const Lit short_clause[3] = {internalize(1), internalize(2), internalize(3)};
  const Lit long_clause[4] = {internalize(1), internalize(2), internalize(3),
                              internalize(4)};

  Kernel k;
  kernel_reserve_vars(k, 4);
  assert(!k.vivify_flush &&
         "production follows Kissat: watch flushing is not clause prefix "
         "subsumption");
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

static void test_schedule_root_false_ternary_becomes_binary() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit clause[3] = {internalize(-1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, clause, 3, false, 0, 1);
  kernel_assign_unit(k, internalize(1));
  assert(!kernel_propagate(k));
  assert(vivify_apply(k) == ApplyResult::Ok);
  assert(k.stats.vivify_checked == 0); // simplified before candidate dispatch
  assert(live_large(k) == 0);
  assert(kernel_binary_live(k, internalize(2), internalize(3)));
}

static void test_kissat_candidate_count_vector_ties() {
  { // Irredundant uses Kissat's one-count key; the stable radix tie is ref.
    Kernel k;
    kernel_reserve_vars(k, 6);
    for (const Var v : {5u, 0u, 4u, 1u, 3u, 2u})
      kernel_record_import(k, v);
    const Lit low[3] = {internalize(6), internalize(1), internalize(5)};
    const Lit high[3] = {internalize(2), internalize(4), internalize(3)};
    const Ref lo = kernel_add_large_clause(k, low, 3, false, 0);
    const Ref hi = kernel_add_large_clause(k, high, 3, false, 0);
    std::vector<Ref> cands;
    vivify_candidates(k, cands);
    assert(cands.size() == 2 && cands[0] == hi && cands[1] == lo);
  }
  { // Only the maximum count and inverse size participate; clause pose stays.
    Kernel k;
    kernel_reserve_vars(k, 7);
    const Lit low[3] = {internalize(1), internalize(2), internalize(3)};
    const Lit high[4] = {internalize(4), internalize(5), internalize(6),
                         internalize(7)};
    const Ref lo = kernel_add_large_clause(k, low, 3, false, 0);
    const Ref hi = kernel_add_large_clause(k, high, 4, false, 0);
    // Raise literal 7's count above every literal in the shorter clause.
    const Lit bump1[3] = {internalize(7), internalize(1), internalize(4)};
    const Lit bump2[3] = {internalize(7), internalize(2), internalize(5)};
    kernel_add_large_clause(k, bump1, 3, true, 2);
    kernel_add_large_clause(k, bump2, 3, true, 2);
    std::vector<Ref> cands;
    k.vivify_scope = 2;
    vivify_candidates(k, cands);
    assert(cands.size() == 2 && cands[0] == hi && cands[1] == lo);
    assert(clause_at(k.arena, hi)->lits[0] == internalize(4));
  }
  { // Fully identical keys: Kissat pops the larger arena reference first.
    Kernel k;
    kernel_reserve_vars(k, 3);
    const Lit lits[3] = {internalize(1), internalize(2), internalize(3)};
    const Ref first = kernel_add_large_clause(k, lits, 3, false, 0);
    const Ref second = kernel_add_large_clause(k, lits, 3, false, 0);
    std::vector<Ref> cands;
    vivify_candidates(k, cands);
    assert(cands.size() == 2 && cands[0] == second && cands[1] == first);
  }
  { // An appended packed-arena replacement retains its in-place source order.
    Kernel k;
    kernel_reserve_vars(k, 6);
    const Lit old_lits[3] = {internalize(1), internalize(2), internalize(3)};
    const Lit later_lits[3] = {internalize(4), internalize(5), internalize(6)};
    const Ref old = kernel_add_large_clause(k, old_lits, 3, false, 0, 1);
    const Ref later = kernel_add_large_clause(k, later_lits, 3, false, 0, 2);
    kernel_inherit_clause_order(k, 3, clause_at(k.arena, old)->id());
    kernel_delete_clause(k, clause_at(k.arena, old));
    const Ref replacement =
        kernel_add_large_clause(k, old_lits, 3, false, 0, 3);
    assert(replacement > later); // physical append differs from logical pose
    std::vector<Ref> cands;
    vivify_candidates(k, cands);
    assert(cands.size() == 2);
    assert(cands[0] == later && cands[1] == replacement);
  }
}

// Kissat has no per-candidate clause-size cutoff: the propagation-tick budget
// bounds the round.  A skipped large probe would also skip its retained-prefix
// and watch-order effects on the next candidate, even if it derived nothing.
static void test_large_candidate_has_no_invented_cutoff() {
  Kernel k;
  static constexpr uint32_t size = 101;
  kernel_reserve_vars(k, size);
  std::vector<Lit> lits;
  for (uint32_t i = 1; i <= size; i++)
    lits.push_back(internalize(static_cast<int>(i)));
  const Ref candidate =
      kernel_add_large_clause(k, lits.data(), size, false, 0);
  k.vivify_scope = 2;
  std::vector<Ref> candidates;
  vivify_candidates(k, candidates);
  assert(candidates.size() == 1 && candidates[0] == candidate);
  assert(vivify_try(k, candidate) == ApplyResult::NoOp);
  assert(k.stats.vivify_checked == 1 && k.level == 0);
}

// A later tier can enter after an earlier tier exceeded the cumulative
// ceiling.  Kissat still builds that tier's schedule but tries no candidate;
// zero cannot fall back to Tactix's ordinary "unbounded" convention.
static void test_precharged_cumulative_budget_stops_before_candidate() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit lits[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, lits, 3, false, 0);
  k.vivify_scope = 2;
  k.work_budget_bounded = true;
  k.work_budget = 0;
  k.work_budget_offset = 1;
  assert(vivify_apply(k) == ApplyResult::NoOp);
  assert(k.stats.vivify_checked == 0 && k.level == 0);
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
  test_jumped_binary_reason_subsumes_candidate();
  test_implied_negation_removes();
  test_probe_conflict_strengthens();
  test_terminal_dispatch();
  test_lcm_scope_and_satisfied_delete();
  test_instance_surface();
  test_tier3_scope();
  test_leftover_priority();
  test_trail_reuse();
  test_large_rewrite_unwatches_candidate();
  test_replacement_watch_pose_scans_unassigned_tail();
  test_large_instantiation_retains_candidate_pose();
  test_large_rewrite_trims_without_implied_literal();
  test_schedule_prefix_flush();
  test_schedule_flush_partition();
  test_schedule_root_false_ternary_becomes_binary();
  test_kissat_candidate_count_vector_ties();
  test_large_candidate_has_no_invented_cutoff();
  test_precharged_cumulative_budget_stops_before_candidate();
  test_termination_poll();
  puts("test_vivify OK");
  return 0;
}
