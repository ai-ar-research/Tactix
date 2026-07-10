// Selection scheme after kissat/src/reduce.c + tiers (MIT), simplified to
// Glucose-style for phase 1: glue <= 2 ("tier 1") is always kept; the rest
// are ranked by (glue, size) and the worse half deleted. Reason-locked
// clauses are never deleted. Schedule: first at 2000 conflicts, interval
// growing by 300 per reduction ("Glucose increments").
#include "rules/reduce/reduce.hpp"

#include "proof/proof.hpp"

#include <algorithm>

namespace tactix {

static constexpr uint32_t KEEP_GLUE = 2;

bool reduce_guard(const Kernel &k) {
  return !k.conflict_valid && !k.inconsistent && !k.pending.valid &&
         k.stats.conflicts >= k.reduce.next_conflicts;
}

ApplyResult reduce_apply(Kernel &k) {
  assert(reduce_guard(k));
  k.stats.reductions++;
  k.reduce.next_conflicts =
      k.stats.conflicts + 2000 + 300 * k.stats.reductions;

  // lock clauses currently used as reasons
  for (const Lit lit : k.trail) {
    const Assigned &a = k.assigned[var_of(lit)];
    if (!a.binary && a.reason != DECISION_REASON && a.reason != UNIT_REASON)
      clause_at(k.arena, a.reason)->flags |= Clause::REASON;
  }

  // candidates: redundant, not locked, glue above the keep tier
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

  size_t deleted = 0;
  if (!candidates.empty()) {
    // delete the worse half: highest (glue, size) first
    std::sort(candidates.begin(), candidates.end(),
              [](const Victim &a, const Victim &b) {
                if (a.glue != b.glue)
                  return a.glue > b.glue;
                if (a.size != b.size)
                  return a.size > b.size;
                return a.ref > b.ref; // total order for determinism
              });
    const size_t target = candidates.size() / 2;
    for (size_t i = 0; i < target; i++) {
      Clause *c = clause_at(k.arena, candidates[i].ref);
      if (k.proof)
        k.proof->del(c->lits, c->size);
      c->flags |= Clause::GARBAGE;
      deleted++;
    }
  }
  k.stats.deleted += deleted;

  // unlock reasons, then compact + rewatch
  for (const Lit lit : k.trail) {
    const Assigned &a = k.assigned[var_of(lit)];
    if (!a.binary && a.reason != DECISION_REASON && a.reason != UNIT_REASON)
      clause_at(k.arena, a.reason)->flags &= ~Clause::REASON;
  }
  if (deleted)
    kernel_collect_garbage(k);
  return deleted ? ApplyResult::Ok : ApplyResult::NoOp;
}

TACTIX_REGISTER_RULE(Reduce, RuleTier::Macro, reduce_guard, reduce_apply);

} // namespace tactix
