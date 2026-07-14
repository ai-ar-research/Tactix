// Selection scheme after kissat/src/reduce.c (MIT): candidates are aged
// (used counter decremented) and protected by recency — tier 1
// (glue <= 2) survives while used at all, tier 2 (glue <= 6) survives
// while used within the last two reductions; survivors are ranked
// worst-first by (glue, size). Fixed tier thresholds are kissat's option
// defaults (tier1=2, tier2=6); kissat's dynamic glue-distribution tiers
// ablated flat on our instances (2026-07-13) and stay out for now. The
// rule owns the RANKING and every soundness filter (reason locks, pending
// learned clause, tiers); how MUCH of the ranked list dies is the
// policy's choice (plan §5 rev 17: ReductionPolicy sub-policy — the
// canonical instance deletes kissat's default 75%). WHEN is likewise the
// policy's.
#include "rules/reduce/reduce.hpp"

#include "proof/proof.hpp"

#include <algorithm>

namespace tactix {

static constexpr uint32_t TIER1_GLUE = 2;
static constexpr uint32_t TIER2_GLUE = 6;

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
  for (Ref ref = 0; ref < k.arena.size();) {
    Clause *c = clause_at(k.arena, ref);
    if (c->redundant() && !c->garbage()) {
      // recency protection (kissat reduce.c; the aging tick lives in
      // reduce_apply_refs so enumeration stays side-effect-free for
      // introspecting policies)
      const uint32_t used = c->used();
      const uint32_t glue = c->glue();
      const bool keep =
          c->reason() || (glue <= TIER1_GLUE && used) ||
          (glue <= TIER2_GLUE && used >= Clause::MAX_USED - 1);
      if (!keep)
        candidates.push_back({ref, glue, c->size});
    }
    ref += clause_words(c->size);
  }
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
  // read-then-decrement order
  for (Ref ref = 0; ref < k.arena.size();) {
    Clause *c = clause_at(k.arena, ref);
    if (c->redundant() && !c->garbage()) {
      const uint32_t used = c->used();
      if (used)
        c->set_used(used - 1);
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
    if (k.proof)
      k.proof->del(c->lits, c->size);
    c->flags |= Clause::GARBAGE;
    deleted++;
  }
  k.stats.deleted += deleted;
  lock_reasons(k, false);
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
