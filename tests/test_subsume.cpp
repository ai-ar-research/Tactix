// SubsumeForward: guard, root-modulo semantics, redundant exclusion, reason
// safety, the instance surface, enumeration purity, and the proof deltas.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "proof/proof.hpp"
#include "rules/registry.hpp"
#include "rules/substitute/substitute.hpp"
#include "rules/strengthen/strengthen.hpp"
#include "rules/subsume_forward/subsume_forward.hpp"

#include <cassert>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <vector>

using namespace tactix;

static_assert(SUBSUME_OCCLIM == 100 && SUBSUME_BINLIM == 10000);

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
    ref += clause_actual_words(c);
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
  // The guard is a pure precondition, so it holds off a propagation fixpoint:
  // apply drains the root closure itself (N1).
  assert(subsume_forward_guard(k)); // pure: pending propagation is fine
  assert(subsume_forward_apply(k) != ApplyResult::Unsat); // apply drains first
  assert(k.propagate_head == k.trail.size()); // ...to the root fixpoint
  assert(subsume_forward_guard(k));
  // every rule must tolerate the variable-free state
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
  // the binary subsumes both copies; order-independent here
  assert(k.stats.forward_subsumed == 2);
  assert(live_clauses(k) == 0);
  kernel_check_invariants(k);
}

static void test_redundant_excluded() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  add(k, {1, 2, 3}, /*redundant=*/true); // would subsume, but Reduce may
                                         // delete it later
  add(k, {1, 2, 3, 4});                  // irredundant
  assert(subsume_forward_apply(k) == ApplyResult::NoOp);
  assert(live_clauses(k) == 2);
  // nor is a redundant clause a candidate (Reduce's turf)
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
  // r holds the root-true literal 3 and the duplicate is satisfied: no delete
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
  // single-instance apply examines exactly the chosen candidate
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

// N1 for the touched-epoch round filter: it decides what the canonical round
// tries and may never narrow what the public API can apply (compare
// tests/test_factor_n1.cpp). With the watermark past everything the round
// declines, yet `instances()` offers the candidate and applying it subsumes.
static void test_touch_filter_is_policy_only() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  const Ref big = add(k, {1, 2, 3, 4});
  add(k, {2, 3, 4}); // a genuine subsumer of `big`
  const int id = tactix_rule_id("SubsumeForward");
  const RuleVTable &r = tactix_rules()[id];

  // everything already seen: no clause holds new material
  k.subsume_touch_mark = k.touch_epoch;
  assert(!kernel_clause_new_material(k, clause_at(k.arena, big),
                                     k.subsume_touch_mark) &&
         "test is vacuous unless the filter actually suppresses this clause");

  // the round declines, reaching a fixpoint with nothing re-armed
  assert(r.apply_instance(k, INSTANCE_CANONICAL) == ApplyResult::NoOp);
  assert(k.stats.forward_mark_skipped >= 2);
  assert(live_clauses(k) == 2);
  assert(k.subsume_complete && k.stats.forward_rearmed == 0);

  // (a) the instance surface stays complete
  uint64_t insts[8];
  const uint32_t n = r.instances(k, insts, 8);
  assert(n == 2);
  bool offered = false;
  for (uint32_t i = 0; i < n; i++)
    offered |= insts[i] == big;
  assert(offered && "instances() must not be narrowed by a policy mark (N1)");

  // (b) and the advertised instance is still applicable
  assert(r.guard(k));
  assert(r.apply_instance(k, big) == ApplyResult::Ok);
  assert(k.stats.forward_subsumed == 1);
  assert(live_clauses(k) == 1);
  kernel_check_invariants(k);
}

// Why the filter cannot wedge: a cut round re-arms the current candidate and
// every candidate it never reached, so the next dose resumes from that tail.
static void test_touch_resume_rearms_the_tail() {
  Kernel k;
  kernel_reserve_vars(k, 8);
  for (int t : {3, 4, 5, 6}) // four short clauses over shared literals ...
    add(k, {1, 2, t});
  for (int t : {3, 4, 5, 6}) // ... each subsuming a longer one
    add(k, {1, 2, t, 7});
  k.midround_budget = true;
  k.work_budget = 1; // one partner visit, then stop
  assert(subsume_forward_apply(k) != ApplyResult::Unsat);
  assert(k.stats.budget_stops_c1 == 1);
  assert(!k.subsume_complete && "a cut round is not a fixpoint");
  assert(k.stats.forward_rearmed > 0 && "the unreached tail must be re-armed");
  // the watermark advanced and the tail is still visible through it
  assert(k.subsume_touch_mark > 0);
  assert(k.stats.forward_mark_advances == 1);
  size_t armed = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage() &&
        kernel_clause_new_material(k, c, k.subsume_touch_mark))
      armed++;
    ref += clause_actual_words(c);
  }
  assert(armed > 0 && "the next round must still see the tail");

  // and the resumed round finishes the job
  k.midround_budget = false;
  k.work_budget = 0;
  assert(subsume_forward_apply(k) == ApplyResult::Ok);
  assert(k.subsume_complete);
}

// The substrate's producers: every mutation that can create a new subsumer
// raises the epoch, and a deletion does not (it can only remove one).
static void test_touch_producers() {
  Kernel k;
  // Isolate the irredundant substrate; the dedicated test below covers the
  // default-on likely-kept redundant producer.
  k.touch_redundant = false;
  kernel_reserve_vars(k, 4); // fresh variables are touched
  assert(k.touch_epoch == 4 && k.stats.touch_marks == 4);
  const uint64_t after_vars = k.touch_epoch;

  add(k, {1, 2, 3});            // irredundant large: every variable
  assert(k.touch_epoch == after_vars + 3);
  const uint64_t after_large = k.touch_epoch;

  add(k, {1, 2, 4}, /*redundant=*/true); // learned: not this rule's material
  assert(k.touch_epoch == after_large);

  kernel_add_binary_clause(k, internalize(1), internalize(2), ++k.next_clause_id);
  assert(k.touch_epoch == after_large + 2);
  const uint64_t after_bin = k.touch_epoch;

  // Deletion goes to the other stamp (Kernel::removed), which BCE needs and
  // the subsumption family must not see; both share the clock.
  const Var v1 = var_of(internalize(1));
  const uint64_t touched_v1 = k.touched[v1];
  kernel_delete_binary(k, internalize(1), internalize(2));
  assert(k.touched[v1] == touched_v1 && "deletion creates no new subsumer");
  assert(k.removed[v1] > after_bin && "BCE must see the deletion");
  const uint64_t after_del = k.touch_epoch;

  // a root fix touches exactly the fixed variable (kernel_touch_sync_fixed)
  kernel_assign_unit(k, internalize(4));
  kernel_touch_sync_fixed(k);
  assert(k.touch_epoch == after_del + 1);
  kernel_touch_sync_fixed(k); // idempotent
  assert(k.touch_epoch == after_del + 1);
}

// The redundant half of the substrate, before a consumer is wired to it: the
// stamps fire only when TACTIX_TOUCH_REDUNDANT is on, the scope moves with
// `k.tier2`, and the third disjunct (the reduce survivor envelope) is live.
static void test_touch_redundant_scope() {
  Kernel k;
  kernel_reserve_vars(k, 6);
  assert(k.touch_redundant && "CaDiCaL's likely-kept band defaults on");
  k.tier2 = 2;
  const uint64_t e0 = k.touch_epoch;
  add(k, {1, 2, 3}, /*redundant=*/true); // glue 2 <= tier2: in scope
  assert(k.stats.touch_redundant_dynamic == 1);
  assert(k.touch_epoch == e0 + 3 && "stamping is on by default");

  k.touch_redundant = false;
  const uint64_t e1 = k.touch_epoch;
  add(k, {1, 2, 4}, /*redundant=*/true);
  assert(k.stats.touch_redundant_dynamic == 2);
  assert(k.touch_epoch == e1 && "the =0 arm suppresses redundant stamps");
  k.touch_redundant = true;

  // narrowing the live tier drops the creation out of scope; static-6 counts on
  k.tier2 = 1;
  k.kept_glue = 0; // no reduction has published an envelope yet
  const uint64_t dyn = k.stats.touch_redundant_dynamic;
  const uint64_t st = k.stats.touch_redundant_static6;
  const uint64_t e2 = k.touch_epoch;
  add(k, {1, 2, 5}, /*redundant=*/true); // glue 2 > tier2 = 1
  assert(k.stats.touch_redundant_dynamic == dyn && "(b) it tracks k.tier2");
  assert(k.stats.touch_redundant_static6 == st + 1);
  assert(k.touch_epoch == e2);

  // the survivor envelope readmits it without touching tier2
  k.kept_glue = 4;
  k.kept_size = 8;
  add(k, {1, 2, 6}, /*redundant=*/true);
  assert(k.stats.touch_redundant_dynamic == dyn + 1 &&
         "(c) the reduce survivor envelope is a live disjunct");
  assert(kernel_likely_kept(k, true, 4, 8));
  assert(!kernel_likely_kept(k, true, 5, 8)); // outside on glue
  assert(!kernel_likely_kept(k, true, 4, 9)); // outside on size
  assert(kernel_likely_kept(k, false, 99, 99)); // irredundant: always in
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

// SubsumeForward -> Substitute. A clause deleted on a binary's strength carries
// no witness, and Substitute then deletes that binary too, so rho is the only
// thing that can restore it under an adversarial model.
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
  // the SCC representative is lit(2), so variable 4 goes and D dies with it
  assert(substitute_apply(k) == ApplyResult::Ok);
  assert(k.stats.substitute_vars == 1);
  assert(!has_binary(k, l1, l4)); // the subsumer is gone
  assert(has_binary(k, l1, l2));  // only its rewrite survives
  // the eliminated variable is set so the original C and D are false
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

// The shadow ledger, and the capability-subset claim under it: SubsumeForward's
// per-pair test is Strengthen's with the flip count pinned to zero and the two
// share their limits, so anything the first deletes the second deletes too.
static void shadow_case(Kernel &k, bool binary_subsumer) {
  kernel_reserve_vars(k, 4);
  add(k, {1, 2, 3, 4});
  if (binary_subsumer)
    kernel_add_binary_clause(k, internalize(1), internalize(2),
                             ++k.next_clause_id);
  else
    add(k, {1, 2, 3});
}

static void test_shadow_ledger() {
  for (int bin = 0; bin < 2; bin++) {
    // (1) shipped: SubsumeForward deletes
    Kernel on;
    shadow_case(on, bin != 0);
    assert(subsume_forward_apply(on) == ApplyResult::Ok);
    assert(on.stats.forward_subsumed == 1);
    assert(on.stats.c1_shadow_observed == 0); // the hatch is off by default

    // (2) shadow: the same victim is reported, nothing is deleted
    Kernel sh;
    sh.subsume_shadow = true;
    shadow_case(sh, bin != 0);
    const size_t before = live_clauses(sh);
    assert(subsume_forward_apply(sh) == ApplyResult::NoOp);
    assert(sh.stats.forward_subsumed == 0);
    assert(sh.stats.c1_shadow_observed == 1);
    assert(live_clauses(sh) == before);
    // observing the same clause again must not double-count it
    assert(subsume_forward_apply(sh) == ApplyResult::NoOp);
    assert(sh.stats.c1_shadow_observed == 1);

    // (3) Strengthen alone disposes of it, and the ledger attributes that
    assert(strengthen_apply(sh) == ApplyResult::Ok);
    assert(sh.stats.c1_shadow_deleted + sh.stats.c1_shadow_rewritten == 1);
    assert(sh.shadow_victims.empty()); // no residual: Strengthen handled it all
    assert(live_clauses(sh) == before - 1);
    kernel_check_invariants(sh);
  }
}

// Both interrupt sites. There is no polling cadence, so a raised flag fires at
// the first poll of whichever entry point is driven, even in a short round.
static void test_interrupt() {
  volatile sig_atomic_t stop = 1;
  { // (1) the round's candidate loop, cut before the first candidate
    Kernel k;
    kernel_reserve_vars(k, 4);
    add(k, {1, 2, 3, 4});
    add(k, {1, 2, 3});
    KernelStopPublisher pub(k, &stop);
    assert(subsume_forward_apply(k) == ApplyResult::NoOp);
    assert(k.stats.forward_subsumed == 0 && k.stats.forward_checked == 0);
    assert(k.stats.subsume_interrupted == 1);
    assert(k.stats.rule_interrupted == 1);
    assert(!k.subsume_complete); // an abort is an incomplete round
    // ...and retires nothing: advancing the watermark would starve the tail
    assert(k.subsume_touch_mark == 0 && k.stats.forward_mark_advances == 0);
    kernel_check_invariants(k);
  }
  { // (2) the single-instance entry: `work_budget` is a round device, so no
    // budget governor reaches this one
    Kernel k;
    kernel_reserve_vars(k, 4);
    const Ref big = add(k, {1, 2, 3, 4});
    add(k, {1, 2, 3});
    KernelStopPublisher pub(k, &stop);
    assert(subsume_forward_try(k, big) == ApplyResult::NoOp);
    assert(k.stats.forward_subsumed == 0);
    assert(k.stats.subsume_interrupted == 1);
  }
  { // the negative control for both: same formulas, no interrupt, work happens
    Kernel k;
    kernel_reserve_vars(k, 4);
    add(k, {1, 2, 3, 4});
    add(k, {1, 2, 3});
    assert(subsume_forward_apply(k) == ApplyResult::Ok);
    assert(k.stats.forward_subsumed == 1 && k.stats.subsume_interrupted == 0);
    Kernel t;
    kernel_reserve_vars(t, 4);
    const Ref big = add(t, {1, 2, 3, 4});
    add(t, {1, 2, 3});
    assert(subsume_forward_try(t, big) == ApplyResult::Ok);
    assert(t.stats.forward_subsumed == 1 && t.stats.subsume_interrupted == 0);
  }
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
  test_touch_filter_is_policy_only();
  test_touch_resume_rearms_the_tail();
  test_touch_producers();
  test_touch_redundant_scope();
  test_proof_deltas();
  test_binary_subsumer_then_substitute();
  test_shadow_ledger();
  test_interrupt();
  printf("test_subsume: all ok\n");
  return 0;
}
