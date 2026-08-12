// Selection scheme after kissat/src/reduce.c (MIT): candidates are aged and
// protected by recency — tier 1 survives while used at all, tier 2 while used
// within the last two reductions — and survivors are ranked worst-first by
// (glue, size). The rule owns the ranking and every soundness filter; how much
// of the ranked list dies, and when, is the policy's.
#include "rules/reduce/reduce.hpp"
#include "rules/propagate/propagate.hpp"

#include "proof/proof.hpp"

#include <algorithm>

namespace tactix {

// kissat 4.0's dynamic tier limits (tiers.c compute_tier_limits): quantiles of
// the glue distribution of the clauses actually used in conflict analysis,
// configurable tier1/tier2 quantiles; with no data the option defaults stand.
// Focused and stable histograms remain separate. Integer thresholds, not
// floating-point products, keep the result independent of host libm rounding
// (benchmarks/reports/2026-07-24-battleship-divergence.md).  Kissat converts
// the positive floating product to uint64_t, which truncates: the exact
// integer equivalent is floor(total * permille / 1000), not a ceiling.
// When to recompute is the policy's.
void reduce_retier_from_histogram(Kernel &k, uint32_t mode,
                                  const uint64_t *used,
                                  uint32_t tier1_default,
                                  uint32_t tier2_default,
                                  uint32_t tier1_relative_permille,
                                  uint32_t tier2_relative_permille) {
  assert(mode < 2);
  uint64_t total = 0;
  for (uint32_t g = 0; g <= Stats::MAX_GLUE_USED; g++)
    total += used[g];
  k.stats.retiered++;
  if (!total) {
    k.tier1_by_mode[mode] = tier1_default;
    k.tier2_by_mode[mode] = std::max(tier1_default, tier2_default);
    return;
  }
  constexpr uint64_t scale = 1000;
  const uint64_t lim1 = total * tier1_relative_permille / scale;
  const uint64_t lim2 = total * tier2_relative_permille / scale;
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
  k.tier1_by_mode[mode] = t1;
  k.tier2_by_mode[mode] = t2;
}

void reduce_retier(Kernel &k, uint32_t mode, uint32_t tier1_default,
                   uint32_t tier2_default,
                   uint32_t tier1_relative_permille,
                   uint32_t tier2_relative_permille) {
  assert(mode < 2);
  reduce_retier_from_histogram(
      k, mode, k.stats.used_glue[mode], tier1_default, tier2_default,
      tier1_relative_permille, tier2_relative_permille);
}

// Deleting redundant clauses is sound as long as reason-locked clauses and the
// pending learned clause survive; only a recorded-but-unanalyzed conflict pins
// the arena, through its stale conflict ref.
RuleEligibility reduce_guard(const Kernel &k) {
  RuleEligibility result;
  return result.fact("no-conflict", !k.conflict_valid)
      .fact("consistent", !k.inconsistent);
}

void reduce_prepare(Kernel &k) {
  if (!k.unflushed_root)
    return;
  if (k.level) {
    kernel_update_target_phases(k);
    kernel_backtrack(k, 0);
  }
  // `kissat_backtrack_propagate_and_flush_trail` reaches a root fixpoint
  // before sparse collection.  This is semantically visible even when it
  // derives no new units: scanning the newly root-false literals repairs
  // large-clause watches before the collector decides which prefix watches to
  // preserve.  Merely clearing the marker here left those scans pending until
  // the following Walk warm-up and caused the first Phase-4 divergence.
  if (!k.inconsistent && k.propagate_head < k.trail.size())
    propagate_service(k);
  if (!k.inconsistent)
    k.unflushed_root = 0;
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

void reduce_candidates(Kernel &k, std::vector<Ref> &out, bool measure) {
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
  // (reduce.c:161) would skip — an upper bound, since a garbage redundant
  // clause does not pin the watermark here though a maintained one could not
  // know that.
  bool seen_redundant = false;
  // Measured on the apply path only: `instances()` runs the same scan to rank
  // its payloads, and an enumeration that moves a counter breaks replay
  // identity (plan §8.5).
  if (measure) {
    k.stats.reduce_scans++;
    k.stats.reduce_scan_words += k.arena.size();
  }
  for (Ref ref = 0; ref < k.arena.size();) {
    Clause *c = clause_at(k.arena, ref);
    if (c->redundant() && !c->garbage()) {
      if (!seen_redundant) {
        seen_redundant = true;
        if (measure)
          k.stats.reduce_skippable_words += ref;
      }
      // recency protection (kissat reduce.c); the aging tick lives in
      // reduce_apply_refs, so enumeration stays side-effect-free
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
  if (!seen_redundant && measure) // no live redundant clause: all skippable
    k.stats.reduce_skippable_words += k.arena.size();
  lock_reasons(k, false);
  // canonical worst-first ranking: highest (glue, size) first
  std::sort(candidates.begin(), candidates.end(),
            [](const Victim &a, const Victim &b) {
              if (a.glue != b.glue)
                return a.glue > b.glue;
              if (a.size != b.size)
                return a.size > b.size;
              // Kissat's stable LSD radix sort preserves the arena scan order
              // when both rank components tie.
              return a.ref < b.ref;
            });
  out.reserve(candidates.size());
  for (const Victim &v : candidates)
    out.push_back(v.ref);
}

ApplyResult reduce_apply_refs(Kernel &k, const Ref *refs, size_t n) {
  assert(reduce_guard(k));
  reduce_prepare(k);
  if (k.inconsistent)
    return ApplyResult::Unsat;
  k.stats.reductions++;
  Ref first_reducible = INVALID_REF;
  // aging tick (kissat reduce.c decrements `used` once per reduction):
  // selection already happened on the pre-aging values, matching kissat's
  // read-then-decrement order. The same walk samples the tier2 retention band
  // both ways (Stats::tier2_band_dynamic/static), live tier against static
  // default — what a `likely_to_be_kept` consumer would branch on.
  for (Ref ref = 0; ref < k.arena.size();) {
    Clause *c = clause_at(k.arena, ref);
    if (first_reducible == INVALID_REF && c->redundant())
      first_reducible = ref;
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
  // Publish the survivor envelope (cadical `lim.keptglue`/`lim.keptsize`): the
  // widest (glue, size) still present, and the third disjunct of
  // `kernel_likely_kept`. Taken over survivors, not victims, and not reset
  // when nothing was deleted — cadical's envelope likewise persists until the
  // next reduction. Diagnostic until a consumer reads it.
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
  if (deleted) {
    assert(first_reducible != INVALID_REF);
#ifdef TACTIX_ENABLE_EVENT_TRACE
    event_trace_detail(k, "reason_reduce_first_id",
                       clause_at(k.arena, first_reducible)->id());
#endif
    // Kissat reduce.c starts sparse collection at `first_reducible`, retaining
    // the already-mutated watch order of the irredundant prefix.  Its sparse
    // sweep also strips root-false literals and deletes root-satisfied large
    // and binary clauses; merely reclaiming selected garbage leaves a
    // different formula for the immediately following Rephase.
    if (kernel_has_root_assignment(k))
      kernel_sparse_collect_root(k, first_reducible);
    else
      kernel_collect_garbage(k, first_reducible);
  }
  // Kissat keeps every live trail reason marked throughout sparse collection:
  // root-false stripping may relocate a large reason or turn it into a binary.
  // The collector updates that assignment before this final unmarking pass.
  lock_reasons(k, false);
  return deleted ? ApplyResult::Ok : ApplyResult::NoOp;
}

// canonical instance: kissat's default fraction of the ranked candidates
ApplyResult reduce_apply(Kernel &k) {
  assert(reduce_guard(k));
  reduce_prepare(k);
  if (k.inconsistent)
    return ApplyResult::Unsat;
  std::vector<Ref> candidates;
  reduce_candidates(k, candidates, /*measure=*/true);
  return reduce_apply_refs(k, candidates.data(),
                           reduce_canonical_victims(candidates.size()));
}

// instance surface: payload = how many of the ranked candidates to delete,
// enumerated canonical volume first, then descending, ending at 0
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
  reduce_prepare(k);
  if (k.inconsistent)
    return ApplyResult::Unsat;
  std::vector<Ref> candidates;
  reduce_candidates(k, candidates, /*measure=*/true);
  const size_t n =
      inst < candidates.size() ? static_cast<size_t>(inst) : candidates.size();
  return reduce_apply_refs(k, candidates.data(), n);
}

TACTIX_REGISTER_RULE(Reduce, RuleTier::Macro, reduce_guard,
                     reduce_apply, reduce_instances,
                     reduce_apply_instance);

} // namespace tactix
