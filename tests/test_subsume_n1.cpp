// N1 REGRESSION for C1/C2's VICTIM scope, plus SCHED-F7's band scope and the
// isolation the band rests on.
//
// A round-scope narrowing is legal (it is the `touch_filter` category: WHAT TO
// TRY is policy). A narrowing of `instances()` or of `apply_instance` is not —
// it removes an inference no API caller can get back, which is the shape
// SCHED-N1FIX had to repair in Factor and RV-S10 found again in Vivify. C1/C2
// used to reject every REDUNDANT clause as a victim on all three paths at once,
// so nothing distinguished the two categories; F7 separates them and this test
// is what keeps them separated:
//   (a) `instances()` offers a redundant victim and `apply_instance` accepts it,
//       with the band OFF — the API is complete regardless of the round's scope;
//   (b) the canonical ROUND with the band off leaves that same clause alone,
//       i.e. the narrowing really is round-only;
//   (c) with the band ON the round acts on it, and stops acting the moment
//       `k.tier2` moves it out of the band — the scope tracks the LIVE tier
//       rather than a frozen constant, which one count could not show;
//   (d) a band stamp writes `touched_kept` and never `touched`, which is what
//       makes the ON arm inert for the five consumers that cannot act on the
//       band (the isolation claim at Kernel::touched_kept).
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/registry.hpp"
#include "rules/strengthen/strengthen.hpp"
#include "rules/subsume_forward/subsume_forward.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace tactix;

static Ref add(Kernel &k, std::initializer_list<int> elits, bool redundant,
               uint32_t glue) {
  std::vector<Lit> lits;
  for (int e : elits)
    lits.push_back(internalize(e));
  return kernel_add_large_clause(k, lits.data(),
                                 static_cast<uint32_t>(lits.size()), redundant,
                                 glue, ++k.next_clause_id);
}

static const RuleVTable *rule(const char *name) {
  for (const RuleVTable &rv : tactix_rules())
    if (!std::strcmp(rv.name, name))
      return &rv;
  return nullptr;
}

static bool offered(const RuleVTable *rv, Kernel &k, uint64_t want) {
  uint64_t buf[256];
  const uint32_t n = rv->instances(k, buf, 256);
  for (uint32_t i = 0; i < n; i++)
    if (buf[i] == want)
      return true;
  return false;
}

static bool garbage_at(const Kernel &k, Ref ref) {
  return clause_at(k.arena, ref)->garbage();
}

// (a) + (b) for C1: a redundant clause subsumed by an irredundant one.
static void test_c1_redundant_victim_is_reachable() {
  const RuleVTable *c1 = rule("SubsumeForward");
  assert(c1);

  { // (b) the canonical round declines it — the band is off and C1's round is
    // irredundant-only by design, since C2 absorbs its subsumptions.
    Kernel k;
    kernel_reserve_vars(k, 4);
    add(k, {1, 2, 3}, /*redundant=*/false, 3); // the subsumer
    const Ref victim = add(k, {1, 2, 3, 4}, /*redundant=*/true, 2);
    assert(!k.touch_redundant);
    assert(c1->guard(k));
    assert(c1->apply(k) == ApplyResult::NoOp);
    assert(!garbage_at(k, victim) && "the ROUND may decline; that is policy");
  }
  { // (a) ...and the public surface does not. Same formula, driven through the
    // registry exactly as tactix_apply_instance routes an external policy.
    Kernel k;
    kernel_reserve_vars(k, 4);
    add(k, {1, 2, 3}, false, 3);
    const Ref victim = add(k, {1, 2, 3, 4}, true, 2);
    assert(offered(c1, k, victim) &&
           "instances() must offer every victim apply_instance accepts");
    assert(c1->apply_instance(k, victim) == ApplyResult::Ok);
    // the Ref is stale after the collect; count instead
    size_t live = 0;
    for (Ref r = 0; r < k.arena.size();) {
      const Clause *c = clause_at(k.arena, r);
      live += !c->garbage();
      r += clause_words(c->size);
    }
    assert(live == 1 && "the redundant victim was subsumed away");
    assert(k.stats.forward_subsumed == 1);
  }
}

// (a) for C2, on the STRENGTHEN branch: the redundant clause loses a literal to
// an irredundant partner. The partner-side redundancy check is untouched.
static void test_c2_redundant_victim_is_reachable() {
  const RuleVTable *c2 = rule("Strengthen");
  assert(c2);
  Kernel k;
  kernel_reserve_vars(k, 4);
  add(k, {1, 2, 3}, /*redundant=*/false, 3);       // partner, has +3
  const Ref victim = add(k, {1, 2, -3, 4}, true, 2); // has -3: drop it
  assert(!k.touch_redundant);
  assert(offered(c2, k, victim));
  assert(c2->apply_instance(k, victim) == ApplyResult::Ok);
  assert(k.stats.strengthened == 1);
  // ...and the result is still REDUNDANT: strengthening a forgettable clause
  // must not smuggle it into the irredundant set.
  bool saw_red3 = false;
  for (Ref r = 0; r < k.arena.size();) {
    const Clause *c = clause_at(k.arena, r);
    if (!c->garbage() && c->redundant() && c->size == 3)
      saw_red3 = true;
    r += clause_words(c->size);
  }
  assert(saw_red3);
}

// A redundant clause must never be a PARTNER for an IRREDUNDANT victim: it is
// forgettable, so the deletion could be stranded by Reduce. This is the check F7
// does NOT relax, and after F7 it has TWO independent defences — the round never
// CONNECTS a redundant survivor into `occs`, and the candidate order puts every
// irredundant clause ahead of the whole band, so no band clause is even indexed
// yet when an irredundant candidate is checked.
static void test_redundant_partner_is_still_refused() {
  const RuleVTable *c2 = rule("Strengthen");
  { // the instance surface, where discovery is through the watch lists
    Kernel k;
    kernel_reserve_vars(k, 4);
    add(k, {1, 2, 3}, /*redundant=*/true, 2);         // would-be subsumer
    const Ref victim = add(k, {1, 2, 3, 4}, false, 3); // irredundant candidate
    assert(subsume_forward_try(k, victim) == ApplyResult::NoOp);
    assert(!garbage_at(k, victim));
    assert(strengthen_try(k, victim) == ApplyResult::NoOp);
  }
  { // ...and the ROUND with the band ON, where the band clause IS a candidate
    Kernel k;
    kernel_reserve_vars(k, 4);
    k.tier2 = 2;
    k.touch_redundant = true;
    const Ref subsumer = add(k, {1, 2, 3}, /*redundant=*/true, 2);
    const Ref victim = add(k, {1, 2, 3, 4}, false, 3);
    assert(c2->apply(k) == ApplyResult::NoOp);
    assert(!garbage_at(k, victim) && "no deletion may rest on a band clause");
    assert(!garbage_at(k, subsumer));
    assert(k.stats.strengthen_red_checked == 1 &&
           "...and the band clause really was in scope, so this is not vacuous");
  }
}

// (c) SCHED-F7's round scope: live with the band on, and it MOVES with k.tier2.
static void test_f7_band_scope_is_live_and_dynamic() {
  const RuleVTable *c2 = rule("Strengthen");
  auto build = [](Kernel &k, uint32_t glue) {
    kernel_reserve_vars(k, 4);
    k.tier2 = 2;
    k.kept_glue = 0; // no reduce envelope: tier2 is the only live disjunct
    add(k, {1, 2, 3}, /*redundant=*/false, 3);
    return add(k, {1, 2, -3, 4}, /*redundant=*/true, glue);
  };
  { // in band (glue 2 <= tier2 2): the round acts
    Kernel k;
    const Ref victim = build(k, 2);
    k.touch_redundant = true;
    assert(clause_at(k.arena, victim)->redundant());
    assert(c2->apply(k) == ApplyResult::Ok);
    assert(k.stats.strengthen_red_checked > 0 &&
           "the band scope must actually admit the clause");
    assert(k.stats.strengthen_red_strengthened == 1);
  }
  { // OUT of band (glue 5 > tier2 2, no envelope): same formula, same switch,
    // and the round declines — so the scope is the live predicate, not a
    // constant, which is the half a single `checked` count cannot show.
    // The repetition filter is off here so this isolates the SCOPE; the gap
    // between the two is the next block.
    Kernel k;
    const Ref victim = build(k, 5);
    k.touch_redundant = true;
    k.touch_filter = false;
    assert(c2->apply(k) == ApplyResult::NoOp);
    assert(!garbage_at(k, victim));
    assert(k.stats.strengthen_red_checked == 0);
    // widening the live tier readmits it without touching anything else
    k.tier2 = 5;
    assert(c2->apply(k) == ApplyResult::Ok);
    assert(k.stats.strengthen_red_checked > 0);
  }
  { // THE RECORDED YIELD GAP, pinned so it cannot drift into a soundness claim.
    // The scope is evaluated live but the STAMPS are written at creation, so a
    // clause that enters the band after its own creation was out of it carries
    // no band stamp and the filter skips it until something in it moves. It is a
    // lost strengthening, never an unjustified one — and it is exactly why the
    // ledger row's removal path is "re-arm the band when the tier grows".
    Kernel k;
    const Ref victim = build(k, 5);
    k.touch_redundant = true;
    assert(c2->apply(k) == ApplyResult::NoOp); // out of band, watermark advances
    k.tier2 = 5;                               // ...now in band, but unstamped
    assert(c2->apply(k) == ApplyResult::NoOp);
    assert(k.stats.strengthen_red_skipped > 0 && "skipped, not checked");
    assert(!garbage_at(k, victim));
    // any movement in the clause re-offers it
    k.strengthen_touch_mark = 0;
    assert(c2->apply(k) == ApplyResult::Ok);
  }
  { // the band OFF is the shipped default and must decline both
    Kernel k;
    const Ref victim = build(k, 2);
    assert(!k.touch_redundant);
    assert(c2->apply(k) == ApplyResult::NoOp);
    assert(!garbage_at(k, victim));
    assert(k.stats.strengthen_red_checked == 0);
  }
}

// (d) ISOLATION. A band stamp writes `touched_kept` and leaves `touched` and
// `touch_marks` alone, so every consumer that reads `touched` answers exactly as
// it did with the band off. That is what makes turning F6 on a change to C2 only.
static void test_band_stamps_do_not_touch_the_irredundant_substrate() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  k.tier2 = 2;
  k.touch_redundant = true;
  const Var v = var_of(internalize(1));
  const uint64_t touched_before = k.touched[v];
  const uint64_t marks_before = k.stats.touch_marks;
  const uint64_t kept_before = k.touched_kept[v];
  const uint64_t wm = k.touch_epoch; // a consumer's watermark, taken here

  add(k, {1, 2, 3}, /*redundant=*/true, 2); // in band: stamps

  assert(k.touched[v] == touched_before && "the band must not write `touched`");
  assert(k.stats.touch_marks == marks_before);
  assert(k.touched_kept[v] > kept_before && "...but it must write its own array");
  assert(k.stats.touch_kept_marks == 3);
  // The reads the five band-blind consumers make are unchanged; C2's widened
  // read is the only one that sees it.
  assert(!kernel_touched_since(k, v, wm));
  assert(!kernel_changed_since(k, v, wm)); // BCE's two-sided read
  assert(kernel_kept_since(k, v, wm));
}

int main() {
  test_c1_redundant_victim_is_reachable();
  test_c2_redundant_victim_is_reachable();
  test_redundant_partner_is_still_refused();
  test_f7_band_scope_is_live_and_dynamic();
  test_band_stamps_do_not_touch_the_irredundant_substrate();
  printf("test_subsume_n1: all passed\n");
  return 0;
}
