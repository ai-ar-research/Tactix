// Selection scheme after kissat/src/reduce.c + tiers (MIT), simplified for
// phase 1: glue <= 2 ("tier 1") is always kept; the rest are ranked
// worst-first by (glue, size). The rule owns the RANKING and every
// soundness filter (reason locks, pending learned clause, keep tier); how
// MUCH of the ranked list dies is the policy's choice (plan §5 rev 17:
// ReductionPolicy sub-policy — the canonical instance deletes kissat's
// default 75%). WHEN to reduce is likewise the policy's.
#include "rules/reduce/reduce.hpp"

#include "proof/proof.hpp"

#include <algorithm>

namespace tactix {

static constexpr uint32_t KEEP_GLUE = 2;

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
    if (c->redundant() && !c->garbage() && !c->reason() &&
        c->glue() > KEEP_GLUE)
      candidates.push_back({ref, c->glue(), c->size});
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
                           (3 * candidates.size()) / 4);
}

TACTIX_REGISTER_RULE(Reduce, RuleTier::Macro, reduce_guard,
                     reduce_apply);

} // namespace tactix
