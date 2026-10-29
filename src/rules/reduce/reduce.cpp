// Selection scheme after kissat/src/reduce.c (MIT): candidates are aged and
// protected by recency — tier 1 survives while used at all, tier 2 while used
// within the last two reductions — and survivors are ranked worst-first by
// (glue, size). The rule owns the ranking and every soundness filter; how much
// of the ranked list dies, and when, is the policy's.
#include "rules/reduce/reduce.hpp"
#include "strategy/representation_execution.hpp"
#include "rules/propagate/propagate.hpp"
#include "diagnostics/event_trace.hpp"

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
void reduce_retier_from_histogram(Kernel &k, RetierSchedule &schedule,
                                  uint32_t mode, const uint64_t *used) {
  assert(mode < 2);
  uint64_t total = 0;
  for (uint32_t g = 0; g <= Stats::MAX_GLUE_USED; g++)
    total += used[g];
  k.stats.retiered++;
  if (!total) {
    schedule.tier1_by_mode[mode] = schedule.tier1_default;
    schedule.tier2_by_mode[mode] =
        std::max(schedule.tier1_default, schedule.tier2_default);
    return;
  }
  constexpr uint64_t scale = 1000;
  const uint64_t lim1 = total * schedule.tier1_relative_permille / scale;
  const uint64_t lim2 = total * schedule.tier2_relative_permille / scale;
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
  schedule.tier1_by_mode[mode] = t1;
  schedule.tier2_by_mode[mode] = t2;
}

void reduce_retier(Kernel &k, RetierSchedule &schedule, uint32_t mode) {
  assert(mode < 2);
  reduce_retier_from_histogram(k, schedule, mode, k.stats.used_glue[mode]);
}

bool reduce_retier_at_conflict(Kernel &k, RetierSchedule &schedule,
                               uint32_t mode) {
  assert(mode < 2);
  if (!schedule.enabled || k.stats.conflicts <= schedule.next_conflicts)
    return false;
  reduce_retier(k, schedule, mode);
  k.tier1 = schedule.tier1_by_mode[mode];
  k.tier2 = std::max(k.tier1, schedule.tier2_by_mode[mode]);
  if (schedule.interval < schedule.max_interval)
    schedule.interval *= schedule.growth;
  schedule.next_conflicts = k.stats.conflicts + schedule.interval;
  event_trace_retier(k, schedule, mode, k.stats.used_glue[mode]);
  return true;
}

// Deleting redundant clauses is sound as long as trail reasons, a pending
// learned clause, and any recorded conflict antecedent survive.
RuleEligibility reduce_guard(const Kernel &k) {
  RuleEligibility result;
  return result.fact("consistent", !k.inconsistent);
}

void reduce_prepare(Kernel &k) {
  // A recorded search conflict is a legal Reduce boundary.  Preserve its
  // trail and antecedent for the subsequent ConflictAnalysis action; the
  // collection path below protects and forwards a large conflict clause.
  if (k.conflict_valid)
    return;
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
  if (!k.inconsistent && !kernel_propagation_fixpoint(k))
    propagate_service(k);
  if (!k.inconsistent)
    k.unflushed_root = 0;
}

static void lock_reasons(Kernel &k, bool lock) {
  // Reduce may be explicitly composed before ConflictAnalysis.  Its conflict
  // clause is then just as live as a trail reason: it may neither be selected
  // nor lose its Ref when collection packs the arena.
  if (k.conflict_valid && k.conflict_ref != INVALID_REF) {
    Clause *c = clause_at(k.arena, k.conflict_ref);
    c->flags = lock ? (c->flags | Clause::REASON)
                    : (c->flags & ~Clause::REASON);
  }
  // the pending learned clause (Analyze -> Backjump handoff) must survive
  // collection under arbitrary legal rule orders, like any trail reason
  if (k.pending.valid && k.pending.ref != INVALID_REF) {
    Clause *c = clause_at(k.arena, k.pending.ref);
    c->flags = lock ? (c->flags | Clause::REASON)
                    : (c->flags & ~Clause::REASON);
  }
  for (const Lit lit : k.trail) {
    const Assigned &a = k.assigned[var_of(lit)];
    // Kissat's reduction trail contains only non-root assignments after its
    // root flush. Retained root identities do not need clause-reason locks:
    // LRAT propagation has named the implied unit, and
    // collection retires a deleted root reason to UNIT_REASON. Keep pending
    // learned clauses protected independently above, even at the root.
    if (!a.level)
      continue;
    if (!a.binary && a.reason != DECISION_REASON &&
        a.reason != UNIT_REASON) {
      Clause *c = clause_at(k.arena, a.reason);
      c->flags = lock ? (c->flags | Clause::REASON)
                      : c->detached_reason()
                            ? c->flags
                            : (c->flags & ~Clause::REASON);
    }
  }
}

void reduce_candidates(Kernel &k, std::vector<Ref> &out, bool measure,
                       bool policy_protection) {
  out.clear();
  lock_reasons(k, true);
  struct Victim {
    Ref ref;
    uint64_t position;
    uint32_t glue, size;
  };
  std::vector<Victim> candidates;
  const uint32_t t1 = k.tier1;              // kissat reduce.c:62-63
  const uint32_t t2 = std::max(t1, k.tier2);
  // How much of this whole-arena walk kissat's `first_reducible` watermark
  // (reduce.c:161) would skip — an upper bound, since a garbage redundant
  // clause does not pin the watermark here though a maintained one could not
  // know that.
  uint64_t first_redundant_position = UINT64_MAX;
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
      first_redundant_position =
          std::min(first_redundant_position, kernel_clause_position(k, ref));
      // recency protection (kissat reduce.c); the aging tick lives in
      // reduce_apply_refs, so enumeration stays side-effect-free
      const uint32_t used = c->used();
      const uint32_t glue = c->glue();
      const bool keep =
          c->reason() ||
          (policy_protection &&
           ((glue <= t1 && used) ||
            (glue <= t2 && used >= Clause::MAX_USED - 1)));
      if (!keep)
        candidates.push_back(
            {ref, kernel_clause_position(k, ref), glue, c->size});
    }
    ref += clause_actual_words(c);
  }
  if (measure)
    k.stats.reduce_skippable_words +=
        first_redundant_position == UINT64_MAX ? k.arena.size()
                                               : first_redundant_position;
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
              return a.position < b.position;
            });
  out.reserve(candidates.size());
  for (const Victim &v : candidates)
    out.push_back(v.ref);
}

ApplyResult reduce_apply_refs(Kernel &k, const Ref *refs, size_t n,
                              bool compact_collect) {
  assert(reduce_guard(k));
  reduce_prepare(k);
  if (k.inconsistent)
    return ApplyResult::Unsat;
  if (k.reduce_bookkeeping)
    k.stats.reductions++;
  // Kissat captures the collection start before collect_reducibles advances
  // its stored marker past clauses promoted to irredundant. Those clauses
  // still belong to this call's rewatched tail, not its retained prefix.
  Ref first_reducible = k.first_reducible;
  Ref next_reducible = INVALID_REF;
  // aging tick (kissat reduce.c decrements `used` once per reduction):
  // selection already happened on the pre-aging values, matching kissat's
  // read-then-decrement order.  The explicit CaDiCaL redundant-band arm also
  // samples its live and static tier predicates in this existing walk; the
  // Kissat-default path pays no diagnostic counter traffic.
  for (Ref ref = 0; ref < k.arena.size();) {
    Clause *c = clause_at(k.arena, ref);
    if (c->redundant()) {
      const auto position = static_cast<Ref>(kernel_clause_position(k, ref));
      first_reducible = std::min(first_reducible, position);
      next_reducible = std::min(next_reducible, position);
    }
    if (k.reduce_bookkeeping && c->redundant() && !c->garbage()) {
      const uint32_t used = c->used();
      if (used)
        c->set_used(used - 1);
      if (k.touch_redundant) {
        const uint32_t glue = c->glue();
        k.stats.tier2_band_dynamic += (glue <= k.tier2);
        k.stats.tier2_band_static += (glue <= TIER2_STATIC);
      }
    }
    ref += clause_actual_words(c);
  }
  k.first_reducible = next_reducible;
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
  if (k.reduce_bookkeeping) {
    uint32_t g = 0, sz = 0;
    for (Ref ref = 0; ref < k.arena.size();) {
      const Clause *c = clause_at(k.arena, ref);
      if (c->redundant() && !c->garbage()) {
        if (c->glue() > g)
          g = c->glue();
        if (c->size > sz)
          sz = c->size;
      }
      ref += clause_actual_words(c);
    }
    k.kept_glue = g;
    k.kept_size = sz;
  }
  RootCollectOutcome root_outcome = RootCollectOutcome::NoChange;
  if (deleted || compact_collect) {
    assert(compact_collect || first_reducible != INVALID_REF);
    const Ref collect_start = compact_collect ? 0 : first_reducible;
#ifdef TACTIX_ENABLE_EVENT_TRACE
    if (first_reducible != INVALID_REF)
      event_trace_detail(k, "reason_reduce_first_id",
                         clause_at(k.arena, first_reducible)->id());
#endif
    // Ordinary Reduce starts at `first_reducible`, retaining the already-
    // mutated watch order of the irredundant prefix.  When compactlim fires,
    // Kissat instead sets start=0 before sparse collection, so root-satisfied
    // irredundant clauses and root-false literals are swept too.
    if (kernel_has_root_assignment(k) && !k.conflict_valid)
      root_outcome = execution::execute_action(
          k, execution::SparseCollectRequest{collect_start});
    else
      execution::execute_action(
          k, execution::CollectGarbageRequest{collect_start, true});
  }
  // Kissat keeps every live trail reason marked throughout sparse collection:
  // root-false stripping may relocate a large reason or turn it into a binary.
  // The collector updates that assignment before this final unmarking pass.
  lock_reasons(k, false);
  if (root_outcome == RootCollectOutcome::Refuted)
    return ApplyResult::Unsat;
  // A caller can request zero learned victims and still simplify the root
  // formula. Scoped fixpoints must observe that semantic progress; moving
  // storage or updating reduction bookkeeping alone is not progress.
  return deleted || root_outcome != RootCollectOutcome::NoChange
             ? ApplyResult::Ok
             : ApplyResult::NoOp;
}

// canonical instance: kissat's default fraction of the ranked candidates
ApplyResult reduce_apply(Kernel &k) {
  assert(reduce_guard(k));
  reduce_prepare(k);
  if (k.inconsistent)
    return ApplyResult::Unsat;
  std::vector<Ref> candidates;
  reduce_candidates(k, candidates, /*measure=*/true,
                    k.reduce_protect_tiers);
  const size_t victims = reduce_fractional_victims(
      candidates.size(), k.reduce_victim_numerator,
      k.reduce_victim_denominator);
  return reduce_apply_refs(k, candidates.data(), victims,
                           k.reduce_compact_collection);
}

// instance surface: payload = how many of the ranked candidates to delete,
// enumerated canonical volume first, then descending, ending at 0
static uint32_t reduce_instances(Kernel &k, uint64_t *out, uint32_t cap) {
  if (cap == 0)
    return 0;
  std::vector<Ref> candidates;
  reduce_candidates(k, candidates, false, k.reduce_protect_tiers);
  const uint64_t total = candidates.size();
  const uint64_t canonical = reduce_fractional_victims(
      candidates.size(), k.reduce_victim_numerator,
      k.reduce_victim_denominator);
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

// Additive semantic surface: v1 above is frozen as ranked victim counts.
// Online composition can independently select every legally deletable clause,
// including clauses excluded only by the default policy's protection bands.
static uint32_t reduce_semantic_instances(Kernel &k, uint64_t *out,
                                          uint32_t cap) {
  std::vector<Ref> legal;
  reduce_candidates(k, legal, false, false);
  const uint32_t count = std::min<uint32_t>(
      cap, static_cast<uint32_t>(legal.size()));
  for (uint32_t i = 0; i < count; ++i)
    out[i] = REDUCE_CLAUSE_INSTANCE | legal[i];
  return count;
}

static ApplyResult reduce_apply_instance(Kernel &k, uint64_t inst) {
  if (inst == INSTANCE_CANONICAL)
    return reduce_apply(k);
  reduce_prepare(k);
  if (k.inconsistent)
    return ApplyResult::Unsat;
  if (inst & REDUCE_CLAUSE_INSTANCE) {
    const uint64_t raw = inst & ~REDUCE_CLAUSE_INSTANCE;
    // Validate by membership, not clause_at(raw): an arbitrary payload may
    // point into the middle of a clause or outside the arena.
    std::vector<Ref> legal;
    reduce_candidates(k, legal, false, false);
    for (const Ref ref : legal)
      if (raw == ref)
        return reduce_apply_refs(k, &ref, 1,
                                 k.reduce_compact_collection);
    return ApplyResult::NoOp;
  }
  std::vector<Ref> candidates;
  reduce_candidates(k, candidates, /*measure=*/true,
                    k.reduce_protect_tiers);
  const size_t n =
      inst < candidates.size() ? static_cast<size_t>(inst) : candidates.size();
  return reduce_apply_refs(k, candidates.data(), n,
                           k.reduce_compact_collection);
}

TACTIX_REGISTER_RULE(Reduce, RuleTier::Macro, reduce_guard,
                     reduce_apply, reduce_instances,
                     reduce_apply_instance,
                     reduce_semantic_instances,
                     reduce_apply_instance);

} // namespace tactix
