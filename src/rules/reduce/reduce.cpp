// Selection scheme after kissat/src/reduce.c (MIT): candidates are aged
// (used counter decremented) and protected by recency — tier 1
// (glue <= 2) survives while used at all, tier 2 (glue <= 6) survives
// while used within the last two reductions; survivors are ranked
// worst-first by (glue, size). Fixed tier thresholds are kissat's option
// defaults (tier1=2, tier2=6); a policy that wants kissat's DYNAMIC
// glue-distribution tiers calls reduce_retier, which overwrites them. The
// rule owns the RANKING and every soundness filter (reason locks, pending
// learned clause, tiers); how MUCH of the ranked list dies is the policy's
// choice (ReductionPolicy sub-policy — the canonical instance deletes
// kissat's default 75%). WHEN is likewise the policy's.
#include "rules/reduce/reduce.hpp"

#include "proof/proof.hpp"

#include <algorithm>

namespace tactix {

// kissat 4.0's DYNAMIC tier limits (tiers.c compute_tier_limits): quantiles
// of the glue distribution of the clauses that were actually USED in conflict
// analysis — tier1 at 50 %, tier2 at 90 % (options tier1relative/
// tier2relative). With no data yet, the option defaults 2/6 stand, which is
// exactly the pair we have been using as a constant. The histogram read is
// the FOCUSED one in both modes (kissat internal.h TIEDX with
// focusedtiers = 1): the stable-mode limits are computed and never read.
// Integer thresholds, not `total * 0.5`, so the result cannot depend on the
// libm/rounding of the host (the battleship cross-toolchain determinism
// lesson). The rule owns the computation; WHEN to recompute is the policy's.
void reduce_retier(Kernel &k) {
  const uint64_t *used = k.stats.used_glue[0];
  uint64_t total = 0;
  for (uint32_t g = 0; g <= Stats::MAX_GLUE_USED; g++)
    total += used[g];
  k.stats.retiered++;
  if (!total) {
    k.tier1 = TIER1_STATIC;
    k.tier2 = TIER2_STATIC;
    return;
  }
  const uint64_t lim1 = total / 2;           // tier1relative 500 per mille
  const uint64_t lim2 = (total * 9 + 9) / 10; // tier2relative 900, rounded up
  uint64_t acc = 0;
  uint32_t t1 = 0;
  for (uint32_t g = 0;; g++) {
    acc += used[g];
    if (acc >= lim1 || g == Stats::MAX_GLUE_USED) {
      t1 = g;
      break;
    }
  }
  uint32_t t2 = t1;
  if (acc < lim2)
    for (uint32_t g = t1 + 1; g <= Stats::MAX_GLUE_USED; g++) {
      acc += used[g];
      if (acc >= lim2) {
        t2 = g;
        break;
      }
    }
  k.tier1 = t1;
  k.tier2 = t2;
}

// soundness: deleting redundant clauses is always sound as long as
// reason-locked clauses and the pending learned clause survive; only a
// recorded-but-unanalyzed conflict pins the arena (stale conflict ref)
bool reduce_guard(const Kernel &k) {
  return !k.conflict_valid && !k.inconsistent;
}

static void lock_reasons(Kernel &k, bool lock) {
  // the pending learned clause (Analyze -> Backjump handoff) must survive
  // collection under arbitrary legal rule orders, like any trail reason
  if (k.pending.valid && k.pending.ref != INVALID_REF) {
    Clause *c = clause_at(k.arena, k.pending.ref);
    c->flags = lock ? (c->flags | Clause::REASON)
                    : (c->flags & ~Clause::REASON);
  }
  for (const Lit lit : k.trail) {
    const Assigned &a = k.assigned[var_of(lit)];
    if (!a.binary && a.reason != DECISION_REASON &&
        a.reason != UNIT_REASON) {
      Clause *c = clause_at(k.arena, a.reason);
      c->flags = lock ? (c->flags | Clause::REASON)
                      : (c->flags & ~Clause::REASON);
    }
  }
}

void reduce_candidates(Kernel &k, std::vector<Ref> &out) {
  out.clear();
  lock_reasons(k, true);
  struct Victim {
    Ref ref;
    uint32_t glue, size;
  };
  std::vector<Victim> candidates;
  const uint32_t t1 = k.tier1;              // kissat reduce.c:62-63
  const uint32_t t2 = std::max(t1, k.tier2);
  // How much of this whole-arena walk kissat's `first_reducible` watermark
  // (reduce.c:161, maintained in collect.c:153-196) would skip — the counters
  // that priced the bound and decided against implementing it.
  // Deliberately the OPTIMISTIC reading — a garbage redundant
  // clause does not pin the watermark here, though an incrementally maintained
  // one could not know that — so `reduce_skippable_words` is an UPPER bound on
  // what the bound would buy.
  bool seen_redundant = false;
  k.stats.reduce_scans++;
  k.stats.reduce_scan_words += k.arena.size();
  for (Ref ref = 0; ref < k.arena.size();) {
    Clause *c = clause_at(k.arena, ref);
    if (c->redundant() && !c->garbage()) {
      if (!seen_redundant) {
        seen_redundant = true;
        k.stats.reduce_skippable_words += ref;
      }
      // recency protection (kissat reduce.c; the aging tick lives in
      // reduce_apply_refs so enumeration stays side-effect-free for
      // introspecting policies)
      const uint32_t used = c->used();
      const uint32_t glue = c->glue();
      const bool keep =
          c->reason() || (glue <= t1 && used) ||
          (glue <= t2 && used >= Clause::MAX_USED - 1);
      if (!keep)
        candidates.push_back({ref, glue, c->size});
    }
    ref += clause_words(c->size);
  }
  if (!seen_redundant) // no live redundant clause: the whole walk was skippable
    k.stats.reduce_skippable_words += k.arena.size();
  lock_reasons(k, false);
  // canonical worst-first ranking: highest (glue, size) first
  std::sort(candidates.begin(), candidates.end(),
            [](const Victim &a, const Victim &b) {
              if (a.glue != b.glue)
                return a.glue > b.glue;
              if (a.size != b.size)
                return a.size > b.size;
              return a.ref > b.ref; // total order for determinism
            });
  out.reserve(candidates.size());
  for (const Victim &v : candidates)
    out.push_back(v.ref);
}

ApplyResult reduce_apply_refs(Kernel &k, const Ref *refs, size_t n) {
  assert(reduce_guard(k));
  k.stats.reductions++;
  // aging tick (kissat reduce.c decrements `used` once per reduction):
  // selection already happened on the pre-aging values, matching kissat's
  // read-then-decrement order.
  // The same walk samples the tier2 retention band both ways (Stats
  // ::tier2_band_dynamic/static): the live tier against the static option
  // default, which is what a `likely_to_be_kept` consumer would branch on.
  for (Ref ref = 0; ref < k.arena.size();) {
    Clause *c = clause_at(k.arena, ref);
    if (c->redundant() && !c->garbage()) {
      const uint32_t used = c->used();
      if (used)
        c->set_used(used - 1);
      const uint32_t glue = c->glue();
      k.stats.tier2_band_dynamic += (glue <= k.tier2);
      k.stats.tier2_band_static += (glue <= TIER2_STATIC);
    }
    ref += clause_words(c->size);
  }
  lock_reasons(k, true);
  size_t deleted = 0;
  for (size_t i = 0; i < n; i++) {
    Clause *c = clause_at(k.arena, refs[i]);
    // re-validate: the state may have changed since enumeration, and a
    // policy may pass any subset — soundness stays the rule's
    if (!c->redundant() || c->garbage() || c->reason())
      continue;
    kernel_delete_clause(k, c);
    deleted++;
  }
  k.stats.deleted += deleted;
  lock_reasons(k, false);
  // SCHED-F6 · publish the SURVIVOR ENVELOPE (cadical `lim.keptglue` /
  // `lim.keptsize`, reduce.cpp:147-156) — the widest (glue, size) still present
  // after this reduction. It is the third disjunct of
  // `kernel_likely_kept`/`likely_to_be_kept_clause`, and the one we had no
  // counterpart for: without it the predicate collapses to "irredundant or in
  // tier2", which is not the reference's set. Deliberately taken over the
  // SURVIVORS, not over the victims, and deliberately not reset when nothing
  // was deleted — cadical's envelope likewise persists until the next
  // reduction republishes it. Diagnostic-only until a consumer reads it
  // (Kernel::touch_redundant, off by default).
  {
    uint32_t g = 0, sz = 0;
    for (Ref ref = 0; ref < k.arena.size();) {
      const Clause *c = clause_at(k.arena, ref);
      if (c->redundant() && !c->garbage()) {
        if (c->glue() > g)
          g = c->glue();
        if (c->size > sz)
          sz = c->size;
      }
      ref += clause_words(c->size);
    }
    k.kept_glue = g;
    k.kept_size = sz;
  }
  if (deleted)
    kernel_collect_garbage(k); // forwards trail reasons AND pending.ref
  return deleted ? ApplyResult::Ok : ApplyResult::NoOp;
}

// canonical instance: kissat's default fraction of the ranked candidates
ApplyResult reduce_apply(Kernel &k) {
  assert(reduce_guard(k));
  std::vector<Ref> candidates;
  reduce_candidates(k, candidates);
  return reduce_apply_refs(k, candidates.data(),
                           reduce_canonical_victims(candidates.size()));
}

// generic instance surface (plan §5.1 v1): payload = how many of the
// ranked candidates to delete. Ranked: canonical volume first, then the
// remaining counts descending (more deletion first), ending at 0
static uint32_t reduce_instances(Kernel &k, uint64_t *out, uint32_t cap) {
  if (cap == 0)
    return 0;
  std::vector<Ref> candidates;
  reduce_candidates(k, candidates);
  const uint64_t total = candidates.size();
  const uint64_t canonical = reduce_canonical_victims(candidates.size());
  uint32_t n = 0;
  out[n++] = canonical;
  for (uint64_t count = total;; count--) {
    if (n >= cap)
      break;
    if (count != canonical)
      out[n++] = count;
    if (count == 0)
      break;
  }
  return n;
}

static ApplyResult reduce_apply_instance(Kernel &k, uint64_t inst) {
  if (inst == INSTANCE_CANONICAL)
    return reduce_apply(k);
  std::vector<Ref> candidates;
  reduce_candidates(k, candidates);
  const size_t n =
      inst < candidates.size() ? static_cast<size_t>(inst) : candidates.size();
  return reduce_apply_refs(k, candidates.data(), n);
}

TACTIX_REGISTER_RULE(Reduce, RuleTier::Macro, reduce_guard,
                     reduce_apply, reduce_instances,
                     reduce_apply_instance);

} // namespace tactix
