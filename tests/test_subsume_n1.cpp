// N1 regression for C1/C2's victim scope. Narrowing the round is policy
// (`touch_filter`); narrowing `instances()` or `apply_instance` removes an
// inference no API caller can get back. Pinned: (a) the instance surface offers
// and accepts a redundant victim; (b) the round declines it, so the narrowing
// is round-only; (c) with the band on the round acts and tracks `k.tier2` live;
// (d) a band stamp writes `touched_kept`, never `touched`, so band-blind
// consumers are unaffected. See docs/implementation-notes.md.
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
    k.touch_redundant = false;
    kernel_reserve_vars(k, 4);
    add(k, {1, 2, 3}, /*redundant=*/false, 3); // the subsumer
    const Ref victim = add(k, {1, 2, 3, 4}, /*redundant=*/true, 2);
    assert(!k.touch_redundant);
    assert(c1->guard(k));
    assert(c1->apply(k) == ApplyResult::NoOp);
    assert(!garbage_at(k, victim) && "the ROUND may decline; that is policy");
  }
  { // (a) ...and the public surface does not. Same formula, driven through the
    // registry, as `tactix_apply_instance` routes an external policy
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

// (a) for C2: the redundant clause loses a literal to an irredundant partner.
static void test_c2_redundant_victim_is_reachable() {
  const RuleVTable *c2 = rule("Strengthen");
  assert(c2);
  Kernel k;
  k.touch_redundant = false;
  kernel_reserve_vars(k, 4);
  add(k, {1, 2, 3}, /*redundant=*/false, 3);       // partner, has +3
  const Ref victim = add(k, {1, 2, -3, 4}, true, 2); // has -3: drop it
  assert(!k.touch_redundant);
  assert(offered(c2, k, victim));
  assert(c2->apply_instance(k, victim) == ApplyResult::Ok);
  assert(k.stats.strengthened == 1);
  // strengthening a forgettable clause must not make it irredundant
  bool saw_red3 = false;
  for (Ref r = 0; r < k.arena.size();) {
    const Clause *c = clause_at(k.arena, r);
    if (!c->garbage() && c->redundant() && c->size == 3)
      saw_red3 = true;
    r += clause_words(c->size);
  }
  assert(saw_red3);
}

// A redundant clause is never a partner for an irredundant victim: it is
// forgettable, so Reduce could strand the deletion. Two defences — no redundant
// survivor is connected into `occs`, and irredundant candidates come first.
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
  { // ...and the round with the band on, where the band clause is a candidate
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

// (c) the round scope with the band on is live: it moves with `k.tier2`.
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
  { // out of band (glue 5 > tier2 2, no envelope): same formula, same switch,
    // and the round declines; the repetition filter is off to isolate scope
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
  { // The yield gap, pinned so it cannot drift into a soundness claim: the
    // scope is live but the stamps are written at creation, so a clause that
    // enters the band later is skipped until something in it moves — a lost
    // strengthening, never an unjustified one.
    Kernel k;
    const Ref victim = build(k, 5);
    k.touch_redundant = true;
    assert(c2->apply(k) == ApplyResult::NoOp); // out of band; mark advances
    k.tier2 = 5;                               // ...now in band, but unstamped
    assert(c2->apply(k) == ApplyResult::NoOp);
    assert(k.stats.strengthen_red_skipped > 0 && "skipped, not checked");
    assert(!garbage_at(k, victim));
    k.strengthen_touch_mark = 0; // any movement re-offers the clause
    assert(c2->apply(k) == ApplyResult::Ok);
  }
  { // the explicit off arm must decline both
    Kernel k;
    k.touch_redundant = false;
    const Ref victim = build(k, 2);
    assert(!k.touch_redundant);
    assert(c2->apply(k) == ApplyResult::NoOp);
    assert(!garbage_at(k, victim));
    assert(k.stats.strengthen_red_checked == 0);
  }
}

// (d) Isolation: a band stamp writes `touched_kept` and leaves `touched` and
// `touch_marks` alone, so turning the band on changes C2 and nothing else.
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
  // band-blind consumers read as before; only C2's widened read sees the stamp
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
