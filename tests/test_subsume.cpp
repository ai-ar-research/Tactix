// C1 SubsumeForward unit tests: guard, root-modulo semantics, redundant
// exclusion, reason safety, the registry instance surface, enumeration
// purity, and the emitted proof deltas.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "proof/proof.hpp"
#include "rules/registry.hpp"
#include "rules/substitute/substitute.hpp"
#include "rules/subsume_forward/subsume_forward.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <vector>

using namespace tactix;

static Ref add(Kernel &k, std::initializer_list<int> elits,
               bool redundant = false) {
  std::vector<Lit> lits;
  for (int e : elits)
    lits.push_back(internalize(e));
  return kernel_add_large_clause(k, lits.data(),
                                 static_cast<uint32_t>(lits.size()),
                                 redundant, /*glue=*/2,
                                 ++k.next_clause_id);
}

static size_t live_clauses(const Kernel &k) {
  size_t n = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage())
      n++;
    ref += clause_words(c->size);
  }
  return n;
}

static void test_guard() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  assert(subsume_forward_guard(k)); // quiescent root
  kernel_assign_decision(k, internalize(1));
  assert(!subsume_forward_guard(k)); // level > 0 (also unpropagated)
  kernel_backtrack(k, 0);
  assert(subsume_forward_guard(k));
  kernel_assign_unit(k, internalize(2));
  // D-LEVEL guard purification: the guard is a pure precondition — it HOLDS off
  // a propagation fixpoint (the rule drains the root closure U at apply entry,
  // so an un-propagated suffix is no longer an applicability barrier).
  assert(subsume_forward_guard(k)); // pure: pending propagation is fine
  assert(subsume_forward_apply(k) != ApplyResult::Unsat); // apply drains first
  assert(k.propagate_head == k.trail.size()); // ...to the root fixpoint
  assert(subsume_forward_guard(k));
  // empty kernel: guard holds, apply is a no-op (corpus regression 001
  // lesson — rules must tolerate the variable-free state)
  Kernel e;
  assert(subsume_forward_guard(e));
  assert(subsume_forward_apply(e) == ApplyResult::NoOp);
}

static void test_basic_subsume() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  add(k, {1, 2, 3, 4}); // added first, still checked size-ascending
  const Ref small = add(k, {1, 2, 3});
  assert(subsume_forward_apply(k) == ApplyResult::Ok);
  assert(k.stats.forward_subsumptions == 1);
  assert(k.stats.forward_checked == 2);
  assert(k.stats.forward_subsumed == 1);
  assert(live_clauses(k) == 1);
  assert(clause_at(k.arena, 0)->size == 3); // the subsumer survived
  (void)small;
  kernel_check_invariants(k);
  // idempotent: nothing left to subsume
  assert(subsume_forward_apply(k) == ApplyResult::NoOp);
  assert(k.stats.forward_subsumed == 1);
}

static void test_duplicates_and_binary() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  add(k, {1, 2, 3});
  add(k, {1, 2, 3}); // exact duplicate: second is subsumed by the first
  kernel_add_binary_clause(k, internalize(1), internalize(3),
                           ++k.next_clause_id);
  assert(subsume_forward_apply(k) == ApplyResult::Ok);
  // the binary subsumes BOTH copies; order-independent here
  assert(k.stats.forward_subsumed == 2);
  assert(live_clauses(k) == 0);
  kernel_check_invariants(k);
}

static void test_redundant_excluded() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  add(k, {1, 2, 3}, /*redundant=*/true); // would subsume, but may be
                                         // deleted by Reduce later
  add(k, {1, 2, 3, 4});                  // irredundant
  assert(subsume_forward_apply(k) == ApplyResult::NoOp);
  assert(live_clauses(k) == 2);
  // and a redundant clause is not a candidate either (Reduce's turf)
  Kernel k2;
  kernel_reserve_vars(k2, 4);
  add(k2, {1, 2, 3});
  add(k2, {1, 2, 3, 4}, /*redundant=*/true);
  assert(subsume_forward_apply(k2) == ApplyResult::NoOp);
  assert(live_clauses(k2) == 2);
}

static void test_root_modulo_semantics() {
  Kernel k;
  kernel_reserve_vars(k, 6);
  const Ref d = add(k, {1, 2, 4});    // free part {1,2} once 4 is false
  const Ref c = add(k, {1, 2, 5, 4}); // free part {1,2,5}
  add(k, {6, 1, 5});                  // root-satisfied once 6 is true
  kernel_assign_unit(k, internalize(-4));
  kernel_assign_unit(k, internalize(6));
  assert(!kernel_propagate(k));
  assert(subsume_forward_guard(k));
  std::vector<Ref> cands;
  subsume_forward_candidates(k, cands);
  // the satisfied clause is excluded; d before c (size 3 < 4)
  assert(cands.size() == 2 && cands[0] == d && cands[1] == c);
  assert(subsume_forward_apply(k) == ApplyResult::Ok);
  // {1,2,-} subsumes {1,2,5,-} modulo the root-false literal 4
  assert(k.stats.forward_subsumed == 1);
  assert(live_clauses(k) == 2);
  kernel_check_invariants(k);
}

static void test_reason_never_victim() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Ref r = add(k, {1, 2, 3});
  add(k, {1, 2, 3}); // duplicate — would subsume r if r were a candidate
  kernel_assign_unit(k, internalize(-1));
  kernel_assign_unit(k, internalize(-2));
  assert(!kernel_propagate(k)); // 3 propagates with r as reason
  assert(!k.assigned[var_of(internalize(3))].binary);
  assert(k.assigned[var_of(internalize(3))].reason == r);
  // r now contains the root-true literal 3 => not a candidate; the
  // duplicate is satisfied too — nothing is deleted, reasons are safe
  assert(subsume_forward_apply(k) == ApplyResult::NoOp);
  assert(live_clauses(k) == 2);
  kernel_check_invariants(k);
}

static void test_registry_instance_surface() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  const Ref big = add(k, {1, 2, 3, 4});
  const Ref small = add(k, {2, 3, 4});
  add(k, {1, 2, 5}); // unrelated survivor
  const int id = tactix_rule_id("SubsumeForward");
  assert(id >= 0);
  const RuleVTable &r = tactix_rules()[id];
  assert(r.guard(k) && r.instances && r.apply_instance);
  uint64_t insts[8];
  assert(r.instances(k, insts, 8) == 3);
  assert(insts[0] == small); // canonical processing order: size ascending
  // single-instance apply: exactly the chosen candidate is examined
  assert(r.apply_instance(k, big) == ApplyResult::Ok);
  assert(k.stats.forward_subsumed == 1);
  assert(live_clauses(k) == 2);
  // stale/garbage payloads are rejected, never dereferenced
  assert(r.apply_instance(k, big + 1) == ApplyResult::NoOp);   // mid-clause
  assert(r.apply_instance(k, 1u << 30) == ApplyResult::NoOp);  // out of range
  assert(r.apply_instance(k, UINT64_MAX - 1) == ApplyResult::NoOp);
  // canonical sentinel drives the full round (nothing left to delete)
  assert(r.apply_instance(k, INSTANCE_CANONICAL) == ApplyResult::NoOp);
  kernel_check_invariants(k);
}

static void test_enumeration_purity() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  add(k, {1, 2, 3, 4});
  add(k, {1, 2, 3});
  const Stats before = k.stats;
  std::vector<Ref> a, b;
  subsume_forward_candidates(k, a);
  subsume_forward_candidates(k, b);
  assert(a == b && a.size() == 2);
  assert(std::memcmp(&before, &k.stats, sizeof(Stats)) == 0);
  assert(live_clauses(k) == 2); // enumeration deleted nothing
}

static void test_proof_deltas() {
  char path[] = "/tmp/tactix_test_subsume_XXXXXX";
  const int fd = mkstemp(path);
  assert(fd >= 0);
  close(fd);
  Kernel k;
  kernel_reserve_vars(k, 4);
  Proof p;
  assert(p.open(path, ProofFormat::DratAscii));
  k.proof = &p;
  add(k, {1, 2, 3, 4});
  add(k, {1, 2, 3});
  assert(subsume_forward_apply(k) == ApplyResult::Ok);
  p.flush();
  k.proof = nullptr;
  FILE *f = fopen(path, "r");
  assert(f);
  char line[128];
  bool saw_delete = false;
  while (fgets(line, sizeof line, f))
    saw_delete |= line[0] == 'd' && std::strstr(line, "4") != nullptr;
  fclose(f);
  remove(path);
  assert(saw_delete); // exactly the subsumed clause was deleted
}

static bool has_binary(const Kernel &k, Lit a, Lit b) {
  const WatchHead &h = k.watches.heads[a];
  for (uint32_t i = 0; i < h.size;) {
    const Watch w{k.watches.arena[h.offset + i]};
    if (!w.binary()) {
      i += 2;
      continue;
    }
    if (w.other() == b)
      return true;
    i += 1;
  }
  return false;
}

// C1 -> V3 composition. C1 deletes an irredundant clause on a BINARY's
// strength with no redundancy check; V3 Substitute then deletes that very
// binary, replacing it with its rewrite onto an SCC representative and
// pushing a reconstruction witness for the substituted variable. The
// subsumed clause carries no witness of its own, so rho is the only thing
// that can restore it. Pinned here for the WORST search assignment to the
// substituted variable (the one falsifying both original clauses).
static void test_binary_subsumer_then_substitute() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit l1 = internalize(1), l2 = internalize(2), l4 = internalize(4);
  add(k, {1, 4, 3}); // C, irredundant, subsumed by D
  kernel_add_binary_clause(k, l1, l4, ++k.next_clause_id);              // D
  kernel_add_binary_clause(k, negate(l4), l2, ++k.next_clause_id);      // 4 -> 2
  kernel_add_binary_clause(k, negate(l2), l4, ++k.next_clause_id);      // 2 -> 4
  assert(subsume_forward_apply(k) == ApplyResult::Ok);
  assert(k.stats.forward_subsumed == 1 && live_clauses(k) == 0);
  // repr of the SCC {lit(2), lit(4)} is the smaller literal, lit(2), so
  // variable 4 is the one eliminated and D is the binary that dies.
  assert(substitute_apply(k) == ApplyResult::Ok);
  assert(k.stats.substitute_vars == 1);
  assert(!has_binary(k, l1, l4)); // the subsumer is GONE
  assert(has_binary(k, l1, l2));  // only its rewrite survives
  // adversarial model of the FINAL formula: (1 v 2) satisfied through 2, and
  // the eliminated variable assigned so that the ORIGINAL C and D are false.
  const int8_t model[4] = {-1, 1, -1, -1}; // vars 1..4
  for (int v = 0; v < 4; v++) {
    k.values[make_lit(v, false)] = model[v];
    k.values[make_lit(v, true)] = static_cast<int8_t>(-model[v]);
  }
  kernel_extend_model(k);
  assert(k.values[l4] > 0); // rho recovered 4 from its representative
  assert(k.values[l1] > 0 || k.values[l4] > 0);                     // D
  assert(k.values[l1] > 0 || k.values[l4] > 0 || k.values[internalize(3)] > 0); // C
}

int main() {
  test_guard();
  test_basic_subsume();
  test_duplicates_and_binary();
  test_redundant_excluded();
  test_root_modulo_semantics();
  test_reason_never_victim();
  test_registry_instance_surface();
  test_enumeration_purity();
  test_proof_deltas();
  test_binary_subsumer_then_substitute();
  printf("test_subsume: all ok\n");
  return 0;
}
