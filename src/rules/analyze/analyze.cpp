// 1-UIP after kissat analyze.c + deduce.c, recursive minimization after
// minimize.c, all-UIP shrinking after shrink.c (Fleury–Biere SAT'20) and
// on-the-fly subsumption after deduce.c + strengthen.c (MIT, transliterated).
// Deviations: analysis runs at the maximum conflict level (an N1 schedule may
// place a conflict below the current level), and singleton-level literals are
// still minimized.
#include "rules/analyze/analyze.hpp"
#include "rules/analyze/learned_sort.hpp"
#include "diagnostics/event_trace.hpp"
#include "policies/heuristics.hpp"

#include "proof/proof.hpp"
#include "rules/backjump/backjump.hpp" // backjump_apply: the fused rule's 2nd step
#include "rules/reduce/reduce.hpp"     // the retier schedule at OTFS conflicts
#include "rules/semantic_extension/affine_matrix_search.hpp"
#include "strategy/conflict_analysis_execution.hpp"

#include <algorithm>
#include <cstdlib>
#include <iterator>

namespace tactix {

// bit flags in Kernel::seen during analysis
static constexpr uint8_t SEEN = 1;      // literal is in the learned clause
static constexpr uint8_t REMOVABLE = 2; // proven redundant
static constexpr uint8_t POISONED = 4;  // proven not redundant
static constexpr uint8_t SHRINKABLE = 8; // in the current block-shrink walk
static constexpr uint8_t REASON_BUMP = 32; // marked by reason-side bumping

// Bump set = SEEN ∪ REASON_BUMP (kissat's `analyzed` stack); `k.analyzed` also
// holds scratch, so membership is a flag test.
static constexpr uint8_t BUMPED = SEEN | REASON_BUMP;

// Kissat unwinds in two stages: the current level, then a target/best phase
// snapshot, then the conflict level. This fixes heap re-entry order.
static void unwind_after_conflict(Kernel &k, uint32_t target) {
  if (k.level) {
    kernel_backtrack(k, k.level - 1);
    if (k.h)
      heuristics_update_target_phases(k);
  }
  if (target < k.level)
    kernel_backtrack(k, target);
}

// Affine handles are not clauses; they are certified before the analysis walk
// takes reason pointers, so every reason here is clausal.
static Clause *analysis_reason_clause(Kernel &k, Var v) {
  Assigned &a = k.assigned[v];
  assert(!a.affine && !a.binary && a.reason != DECISION_REASON &&
         a.reason != UNIT_REASON);
  Clause *reason = clause_at(k.arena, a.reason);
  return reason;
}

// glue (LBD): distinct decision levels among lits[0..n), capped at `limit`
// (kissat kissat_recompute_glue's early exit); leaves marked_levels clean
static uint32_t count_glue(Kernel &k, const Lit *lits, uint32_t n,
                           uint32_t limit = UINT32_MAX) {
  uint32_t glue = 0;
  for (uint32_t i = 0; i < n && glue < limit; i++) {
    const uint32_t lv = k.var_level(var_of(lits[i]));
    if (!k.marked_levels[lv]) {
      k.marked_levels[lv] = 1;
      k.touched_levels.push_back(lv);
      glue++;
    }
  }
  for (const uint32_t lv : k.touched_levels)
    k.marked_levels[lv] = 0;
  k.touched_levels.clear();
  return glue;
}

// Mark a redundant clause recently-used (Reduce then protects it) and
// recompute+promote its glue; every literal is assigned at both call sites.
static void mark_used(Kernel &k, Clause *c) {
  if (!c->redundant())
    return;
  c->set_used(Clause::MAX_USED);
  const uint32_t old_glue = c->glue();
  if (old_glue > 1) {
    const uint32_t glue = count_glue(k, c->lits, c->size, old_glue);
    if (glue < old_glue) {
      event_trace_clause_glue(k, "analyze", c, old_glue, glue);
      c->set_glue(glue);
      k.stats.promoted++;
    }
  }
  // per-mode histogram of the glues actually used, post-promotion (kissat
  // deduce.c:20-22); Reduce's dynamic tiers are quantiles of it
  const uint32_t mode = (k.h && k.h->stable) ? 1u : 0u;
  event_trace_clause_used(k, c, mode);
  k.stats.used_glue[mode][std::min<uint32_t>(c->glue(), Stats::MAX_GLUE_USED)]++;
}

RuleEligibility analyze_guard(const Kernel &k) {
  RuleEligibility result;
  return result.fact("conflict-valid", k.conflict_valid)
      .fact("consistent", !k.inconsistent);
}

// literals of the conflict clause (binary conflicts live in conflict_bin)
static const Lit *conflict_lits(const Kernel &k, uint32_t *size) {
  if (k.conflict_ref == INVALID_REF) {
    *size = 2;
    return k.conflict_bin;
  }
  const Clause *c = clause_at(k.arena, k.conflict_ref);
  *size = c->size;
  return c->lits;
}

// Inspect the reason ancestry read-only, then certify only its native affine
// handles in trail order (a conservative pre-analysis form of CryptoMiniSat's
// get_reason boundary). False when the proof budget cannot pay for a handle.
static bool materialize_affine_ancestors_before_analysis(Kernel &k) {
  AffineMatrixSearchSession *const session =
      k.affine_matrix_search_session;
  if (!session || !session->lazy_reason_count())
    return true;

  uint32_t conflict_size = 0;
  const Lit *const conflict = conflict_lits(k, &conflict_size);
  std::vector<Var> pending;
  pending.reserve(conflict_size);
  for (uint32_t i = 0; i < conflict_size; ++i)
    pending.push_back(var_of(conflict[i]));
  std::vector<uint8_t> seen(k.nvars, 0);
  std::vector<Var> lazy;
  while (!pending.empty()) {
    const Var v = pending.back();
    pending.pop_back();
    if (v >= seen.size() || seen[v])
      continue;
    seen[v] = 1;
    if (!k.values[make_lit(v, false)])
      continue;
    const Assigned &a = k.assigned[v];
    if (a.affine) {
      ++k.stats.affine_matrix_analyze_handles;
      lazy.push_back(v);
      if (!session->inspect_lazy_reason(k, v, pending))
        std::abort();
      continue;
    }
    if (!a.level || a.reason == DECISION_REASON || a.reason == UNIT_REASON)
      continue;
    if (a.binary) {
      pending.push_back(var_of(a.reason));
      if (a.jumped)
        pending.push_back(var_of(k.jumped_over[v]));
      continue;
    }
    const Clause *const reason = clause_at(k.arena, a.reason);
    for (const Lit antecedent : *reason)
      if (var_of(antecedent) != v)
        pending.push_back(var_of(antecedent));
  }
  std::sort(lazy.begin(), lazy.end(), [&k](Var a, Var b) {
    return k.assigned[a].trail < k.assigned[b].trail;
  });
  for (const Var v : lazy) {
    const auto result =
        affine_matrix_materialize_lazy_reason(k, *session, v, false);
    if (result.status == AffineMatrixServiceStatus::ResourceExhausted)
      return false;
    if (result.status != AffineMatrixServiceStatus::Applied ||
        !result.reason_materialized)
      std::abort();
    ++k.stats.affine_matrix_analyze_materializations;
    if (result.reason_vector_constructed) {
      ++k.stats.affine_matrix_analyze_vectors_constructed;
      k.stats.affine_matrix_analyze_literals_constructed +=
          result.installed_reason.size();
    }
  }
  return true;
}

// Unaffordable handles are withdrawn: backtrack below them and drop the
// conflict. Parity reasoning ends for the run; nothing is learned.
static void retract_unaffordable_affine_reasons(Kernel &k) {
  AffineMatrixSearchSession &session = *k.affine_matrix_search_session;
  const uint32_t lowest = session.lowest_lazy_reason_level(k);
  assert(lowest);
  kernel_backtrack(k, lowest - 1);
  k.conflict_valid = false;
  k.conflict_ref = INVALID_REF;
  k.pending.valid = false;
  assert(!session.lazy_reason_count());
  session.disable(k);
  k.affine_reasons_unaffordable = true;
  ++k.stats.affine_matrix_analyze_retractions;
}

// A learned literal is redundant if every reason chain bottoms out in root
// assignments or clause literals; like Kissat the walk may cross levels absent
// from the clause. Binary runs are iterative and do not consume the depth
// budget; reaching the depth keeps the literal.

static bool literal_redundant(Kernel &, Lit, unsigned);

// Every non-root, non-SEEN reason literal of `v` other than `assigned_lit` is
// redundant at `depth`; stops at the first that is not. A recursive child may
// certify a lazy affine reason and reallocate the arena, so copy the literals.
[[gnu::always_inline]] static inline bool
reason_literals_redundant(Kernel &k, Var v, Lit assigned_lit, unsigned depth) {
  const Clause *reason = analysis_reason_clause(k, v);
  const std::vector<Lit> reason_lits(reason->begin(), reason->end());
  for (const Lit m : reason_lits) {
    if (m == assigned_lit)
      continue;
    const Var mv = var_of(m);
    if (k.var_level(mv) == 0)
      continue;
    if (k.seen[mv] & SEEN)
      continue;
    if (!literal_redundant(k, m, depth))
      return false;
  }
  return true;
}

// Kissat's `minimize_binary`: walk the binary run iteratively, then assign the
// whole run its end's result.
static bool binary_chain_redundant(Kernel &k, Lit next, unsigned depth) {
  const size_t saved = k.minimize_scratch.size();
  bool ok = false;
  for (;;) {
    const Var v = var_of(next);
    const Assigned &a = k.assigned[v];
    if (a.level == 0) {
      ok = true;
      break;
    }
    if (k.seen[v] & (SEEN | REMOVABLE)) {
      ok = true;
      break;
    }
    if ((k.seen[v] & POISONED) || a.reason == DECISION_REASON) {
      ok = false;
      break;
    }
    k.minimize_scratch.push_back(v);
    if (a.binary) {
      next = a.reason;
      continue;
    }

    // The chain node itself is one level below the caller; literals in its
    // large reason are two.  Binary nodes before it do not deepen the walk.
    event_trace_detail(k, "analysis_tick_minimize_binary", 1);
    kernel_charge_ticks(k, 1);
    ok = reason_literals_redundant(k, v, negate(next), depth + 2);
    break;
  }

  // Deepest-first cleanup order.
  for (size_t i = k.minimize_scratch.size(); i > saved; i--) {
    const Var v = k.minimize_scratch[i - 1];
    const uint8_t before = k.seen[v];
    k.seen[v] |= ok ? REMOVABLE : POISONED;
    if (!before)
      k.analyzed.push_back(v);
  }
  k.minimize_scratch.resize(saved);
  return ok;
}

static bool literal_redundant(Kernel &k, Lit l, unsigned depth) {
  const Var v = var_of(l);
  const Assigned &a = k.assigned[v];
  if (k.seen[v] & REMOVABLE)
    return true;
  if (k.seen[v] & POISONED)
    return false;
  if (a.reason == DECISION_REASON ||
      depth >= k.analyze_parameters.minimize_depth_limit) {
    if (depth >= k.analyze_parameters.minimize_depth_limit)
      k.stats.minimize_depth_cutoffs++;
    const uint8_t before = k.seen[v];
    k.seen[v] |= POISONED;
    if (!before) // no other flag: newly touched
      k.analyzed.push_back(v); // ensure cleanup
    return false;
  }
  bool ok = true;
  if (a.binary) {
    ok = binary_chain_redundant(k, a.reason, depth);
  } else {
    // One search tick per reason deref (kissat minimize.c:52).
    event_trace_detail(k, "analysis_tick_minimize_reference", 1);
    kernel_charge_ticks(k, 1);
    ok = reason_literals_redundant(k, v, negate(l), depth + 1);
  }
  const uint8_t before = k.seen[v];
  k.seen[v] |= ok ? REMOVABLE : POISONED;
  if (!before)
    k.analyzed.push_back(v); // newly touched: schedule flag cleanup
  return ok;
}

// LSD radix sort of a bump set by VMTF stamp, ascending (kissat sort.h
// RADIX_STACK); stamps are unique, so the order matches a comparison sort.
static void radix_sort_bumps_by_stamp(Kernel &k, std::vector<Var> &a) {
  const size_t n = a.size();
  if (n < 32) { // small sets: std::sort's insertion regime wins
    std::sort(a.begin(), a.end(), [&](Var x, Var y) {
      return k.h->links[x].stamp < k.h->links[y].stamp;
    });
    return;
  }
  std::vector<Var> &b = k.radix_scratch;
  b.resize(n);
  std::vector<Var> *src = &a, *dst = &b;
  for (uint32_t shift = 0; shift < 32; shift += 8) {
    uint32_t count[257] = {0};
    const uint32_t first = (k.h->links[(*src)[0]].stamp >> shift) & 0xff;
    bool constant = true;
    for (const Var v : *src) {
      const uint32_t byte = (k.h->links[v].stamp >> shift) & 0xff;
      constant &= (byte == first);
      count[byte + 1]++;
    }
    if (constant) // all keys share this byte: pass is a no-op
      continue;
    for (uint32_t i = 1; i < 257; i++)
      count[i] += count[i - 1];
    for (const Var v : *src)
      (*dst)[count[(k.h->links[v].stamp >> shift) & 0xff]++] = v;
    std::swap(src, dst);
  }
  if (src != &a)
    a.swap(b);
}

static void clear_analyzed(Kernel &k) {
  for (const Var v : k.analyzed)
    k.seen[v] = 0;
  k.analyzed.clear();
  k.bump_order.clear();
}

static void reset_analyzed_without_bump(Kernel &k) {
  k.analyzed_seen = 0;
  k.reason_bumped.clear();
  clear_analyzed(k);
}

// Bump analysis-side vars and clear all flags. Focused: VMTF move-to-front in
// stamp order (kissat bump.c). Stable: EVSIDS in admission order + one decay.
static void bump_and_reset_analyzed(Kernel &k) {
  k.analyzed_seen = 0;
  k.reason_bumped.clear();
  if (!k.h) { // unguided: nothing to bump, but the scratch must be cleared
    clear_analyzed(k);
    return;
  }
  // attribute each bump to its channel and to whether it landed on an
  // extension variable (see Stats::bumps_extension)
  const Var ext_first = k.first_extension_var;
  auto count_bump = [&](Var v) {
    k.stats.bumps++;
    if (v >= ext_first && kernel_is_solver_extension(k, v)) {
      k.stats.bumps_extension++;
      if (!(k.seen[v] & SEEN))
        k.stats.bumps_ext_reason_only++;
    }
  };
  event_trace_analyzed(k, k.bump_order, k.seen, BUMPED);
  if (k.h->stable) {
    const bool any_analyzed = !k.bump_order.empty();
    event_trace_heap_scores(k);
    for (const Var v : k.bump_order) {
      if (k.seen[v] & BUMPED) {
        count_bump(v);
        if (heuristics_var_active(k, v)) {
          heuristics_score_bump(k, v);
          event_trace_detail(k, "score_bump",
                             kernel_externalize(k, make_lit(v, false)));
          event_trace_heap_scores(k);
        }
      }
    }
    event_trace_analyzed_post(k, k.bump_order);
    clear_analyzed(k);
    // Kissat decays EVSIDS only when `analyzed` is non-empty; otherwise the
    // increment drifts and changes equal-score heap order.
    if (any_analyzed)
      heuristics_scores_decay(k);
    return;
  }
  radix_sort_bumps_by_stamp(k, k.bump_order);
  for (const Var v : k.bump_order) {
    if (k.seen[v] & BUMPED) {
      count_bump(v);
      if (heuristics_var_active(k, v))
        heuristics_queue_bump(k, v);
    }
  }
  clear_analyzed(k);
}

void analyze_bump_after_backjump(Kernel &k) { bump_and_reset_analyzed(k); }

// --- reason-side bumping (kissat analyze_reason_side_literals; cadical
// bump_also_all_reason_literals; MapleCOMSPS 2016) ---
// Bump the final clause's reasons too, capped at `k.bump_reason_limit ×
// |bump set|` new marks, all-or-nothing. Affects only which variables bump.
static bool mark_reason_side(Kernel &k, Lit m) {
  const Var v = var_of(m);
  if (k.var_level(v) == 0)
    return false; // root-fixed: kissat `a->level`
  // Optional arm (Kernel::no_ext_reason_bump): exclude extension vars here.
  if (k.no_ext_reason_bump && v >= k.first_extension_var && kernel_is_solver_extension(k, v))
    return false;
  if (k.seen[v] & BUMPED)
    return false; // already in the bump set (kissat `a->analyzed`)
  // Test SEEN only: a var carrying just REMOVABLE/POISONED is admitted, as by
  // kissat's `!a->analyzed`.
  if (!k.seen[v])
    k.analyzed.push_back(v);
  k.bump_order.push_back(v);
  k.seen[v] |= REASON_BUMP;
  k.reason_bumped.push_back(v);
  k.analyzed_seen++;
  k.stats.reason_bumped++;
  return true;
}

static void mark_reason_tree(Kernel &k, Lit false_lit, uint32_t depth,
                             uint64_t limit) {
  if (!depth || k.analyzed_seen > limit)
    return;
  const Assigned &a = k.assigned[var_of(false_lit)];
  if (a.reason == DECISION_REASON || a.reason == UNIT_REASON)
    return;
  if (a.binary) {
    const Lit m = a.reason;
    if (mark_reason_side(k, m) && depth > 1)
      mark_reason_tree(k, m, depth - 1, limit);
    return;
  }
  event_trace_detail(k, "analysis_tick_reason_bump", 1);
  kernel_charge_ticks(k, 1);
  const Clause *r = analysis_reason_clause(k, var_of(false_lit));
  const std::vector<Lit> reason_lits(r->begin(), r->end());
  const Lit self = negate(false_lit);
  for (const Lit m : reason_lits) {
    if (m == self)
      continue;
    if (mark_reason_side(k, m) && depth > 1)
      mark_reason_tree(k, m, depth - 1, limit);
    if (k.analyzed_seen > limit)
      break;
  }
}

static void bump_reason_side(Kernel &k, const std::vector<Lit> &learned) {
  event_trace_detail(k, "reason_bump_begin", k.bump_reason_limit);
  const uint32_t saved = k.analyzed_seen;
  const size_t saved_bump_order = k.bump_order.size();
  const uint64_t limit =
      static_cast<uint64_t>(k.bump_reason_limit) * saved;
  k.reason_bumped.clear();
  k.stats.reason_bump_rounds++;
  if (k.bump_reason_depth > 1)
    k.stats.reason_bump_depth2++;
  for (const Lit l : learned) {
    mark_reason_tree(k, l, k.bump_reason_depth, limit);
    if (k.analyzed_seen > limit)
      break;
  }
  if (k.analyzed_seen > limit) { // over budget: undo the whole round
    for (const Var v : k.reason_bumped)
      k.seen[v] &= static_cast<uint8_t>(~REASON_BUMP);
    k.stats.reason_bumped -= k.reason_bumped.size();
    k.analyzed_seen = saved;
    k.bump_order.resize(saved_bump_order);
    k.stats.reason_bump_aborted++;
    // Newly pushed vars stay in k.analyzed with seen == 0;
    // bump_and_reset_analyzed skips and clears them.
  }
  k.reason_bumped.clear();
}

// --- on-the-fly subsumption (kissat deduce.c + strengthen.c) ---
// A resolvent equal to reason ∖ {t} subsumes the reason and becomes the
// conflict. Large path: shrink in place as large_on_the_fly_strengthen (same
// record, first watch kept, t's watch dropped; t keeps the reason until
// Backjump). Binary path: publish a new binary, retire the old at Backjump. A
// conflict also subsumed is deleted at once
// (benchmarks/reports/2026-08-09-tier-a-otfs-watch-repair.md).
static void otfs_strengthen(Kernel &k, Ref reason_ref, Lit t,
                            Ref subsumed_ref /* INVALID_REF if none */) {
  const Clause *reason = clause_at(k.arena, reason_ref);
  const bool reason_irredundant = !reason->redundant();
  std::vector<Lit> &lits = k.otfs_scratch; // no per-event alloc
  lits.clear();
  lits.reserve(reason->size - 1);
  assert(reason->lits[0] == t || reason->lits[1] == t);
  const Lit surviving_watch =
      reason->lits[0] == t ? reason->lits[1] : reason->lits[0];
  const bool keep_surviving_watch =
      k.var_level(var_of(surviving_watch)) > 0;
  if (keep_surviving_watch)
    lits.push_back(surviving_watch);
  for (uint32_t i = 2; i < reason->size; i++) {
    const Lit l = reason->lits[i];
    if (k.var_level(var_of(l)) > 0)
      lits.push_back(l);
  }
  assert(lits.size() >= 2);
  const bool binary = lits.size() == 2;
  // The binary path drops root-false literals; the large path keeps the
  // surviving first watch even when root-false (strengthen.c:18-45).
  if (!binary && !keep_surviving_watch)
    lits.insert(lits.begin(), surviving_watch);
  const uint64_t id = ++k.next_clause_id;

  bool subsume = false, subsumed_irredundant = false;
  uint32_t subsumed_glue = UINT32_MAX;
  if (subsumed_ref != INVALID_REF) {
    const Clause *d = clause_at(k.arena, subsumed_ref);
    assert(!d->garbage());
    assert(subsumed_ref != reason_ref);
    subsume = true;
    subsumed_irredundant = !d->redundant();
    if (!subsumed_irredundant)
      subsumed_glue = d->glue();
  }
  // the resolvent keeps the reason's redundancy, but must become irredundant
  // if it replaces an irredundant conflict (kissat otfs_subsume_clause)
  const bool redundant = reason->redundant() && !subsumed_irredundant;

  if (k.proof) // streaming PBP tracks the resolvent's id for `del id`
    k.proof->add_rup(id, lits.data(),
                     static_cast<uint32_t>(lits.size()));
  k.stats.otfs_strengthened++;

  // kissat_on_the_fly_strengthen marks only the resolved literal removed;
  // binary OTFS deletes the old reason below.
  if (reason_irredundant)
    kernel_mark_bve_retry(k, var_of(t));
  kernel_removed_var(k, var_of(t));
  if (reason_irredundant)
    kernel_notify_bve_score(k, var_of(t), true);

  if (binary) {
    kernel_add_binary_clause(k, lits[0], lits[1], id);
    for (const Lit old : *reason)
      if (old != t) {
        if (reason_irredundant)
          kernel_mark_bve_retry(k, var_of(old));
        kernel_removed_var(k, var_of(old));
        if (reason_irredundant)
          kernel_notify_bve_score(k, var_of(old), true);
      }
    k.conflict_ref = INVALID_REF;
    k.conflict_bin[0] = lits[0];
    k.conflict_bin[1] = lits[1];
  } else {
    // Keep the other old watch even when root-false; choose only its new
    // partner, first literal on a level tie (Kissat).
    uint32_t highest = 1;
    for (uint32_t j = 2; j < lits.size(); j++)
      if (k.var_level(var_of(lits[j])) >
          k.var_level(var_of(lits[highest])))
        highest = j;
    std::swap(lits[1], lits[highest]);
    // Kissat shortens the reason in place and keeps its existing glue unless
    // the new physical size forces the size-1 clamp (strengthen.c:35-43).
    const uint32_t glue = kernel_shrunken_glue(
        k, reason->glue(), static_cast<uint32_t>(lits.size()));
    // strengthen.c large_on_the_fly_strengthen: the record, its arena
    // position and its first watch survive; the header keeps its bits.
    kernel_replace_large_clause_in_place(
        k, reason_ref, id, lits.data(), static_cast<uint32_t>(lits.size()),
        glue, "analyze_strengthen", /*mark_added_material=*/false,
        /*retained_watch=*/surviving_watch);
    k.conflict_ref = reason_ref;
    Clause *replacement = clause_at(k.arena, reason_ref);
    // kissat_on_the_fly_subsume turns a redundant reason irredundant when it
    // subsumes an irredundant conflict.
    if (!redundant && replacement->redundant()) {
      // Publish the promotion before a later OTFS rewrite, for Eliminate's
      // dense image under an N1 schedule.
      kernel_promote_large_irredundant(k, reason_ref);
      replacement = clause_at(k.arena, reason_ref);
    }
    // Kissat marks only the surviving old tail (positions 2..), excluding the
    // first watch and a reason that was redundant when strengthen.c decided.
    if (reason_irredundant)
      for (uint32_t i = 1; i < lits.size(); ++i) {
        kernel_touch_var(k, var_of(lits[i]));
        kernel_mark_subsume_material(k, var_of(lits[i]));
        kernel_mark_factor_literal(k, lits[i], "analyze_strengthen");
      }

    // `kissat_on_the_fly_subsume` moves a better LBD from a redundant conflict
    // to the strengthened redundant reason after its size clamp.
    if (subsumed_glue < replacement->glue()) {
      const uint32_t old_glue = replacement->glue();
      event_trace_clause_glue(k, "analyze", replacement, old_glue,
                              subsumed_glue);
      replacement->set_glue(subsumed_glue);
      k.stats.promoted++;
    }
  }

  // Large path: the eager-register slot is untouched (kissat). Binary path
  // (strengthen.c binary_on_the_fly_strengthen): the dead entry keeps its
  // last_learned slot; the old record retires at Backjump.
  if (binary)
    k.otfs_retired.push_back(reason_ref);

  if (subsume) {
    Clause *d = clause_at(k.arena, subsumed_ref); // re-deref: arena moved
    kernel_delete_clause(k, d);
    k.stats.otfs_subsumed++;
  }
}

void analyze_retire_otfs_reasons(Kernel &k) {
  for (const Ref ref : k.otfs_retired) {
#ifndef NDEBUG
    // A clause can antecede at most one current assignment: once it propagates
    // that literal it is satisfied. Backjump has just removed that assignment.
    for (const Lit assigned : k.trail) {
      const Assigned &a = k.assigned[var_of(assigned)];
      assert(a.binary || a.reason != ref);
    }
#endif
    Clause *c = clause_at(k.arena, ref);
    if (!c->garbage()) {
      const Lit first = c->lits[0], second = c->lits[1];
      kernel_unwatch_large(k, first, ref);
      kernel_unwatch_large(k, second, ref);
      c = clause_at(k.arena, ref);
      // The large source was shortened in place; the packed old record is only
      // representation garbage.
      kernel_delete_clause(k, c, false);
    }
  }
  k.otfs_retired.clear();
}

// --- all-UIP block shrinking (kissat shrink.c; Fleury–Biere SAT'20) ---
// A block of learned literals on one level becomes the negated local UIP when
// its reason chains converge.

// Classify falsified l for the `blevel` walk: 1 = newly shrinkable, 0 =
// resolves away (root / shrinkable / redundant), -1 = block can't shrink.
static int shrink_literal(Kernel &k, Lit l, uint32_t blevel) {
  const Var v = var_of(l);
  const Assigned &a = k.assigned[v];
  if (a.level == 0)
    return 0;
  if (k.seen[v] & SHRINKABLE)
    return 0;
  if (a.level < blevel) {
    // `kissat_minimize_clause` marks learned literals removable before
    // shrink=3; SEEN is the equivalent bit.
    if (k.seen[v] & (SEEN | REMOVABLE))
      return 0;
    // kissat's default shrink level falls back to recursive minimization here
    return literal_redundant(k, l, 1) ? 0 : -1;
  }
  if (!k.seen[v])
    k.analyzed.push_back(v); // newly touched: schedule flag cleanup
  k.seen[v] |= SHRINKABLE;
  k.shrinkable.push_back(v);
  return 1;
}

// try to collapse the block learned[b..e) (all at level blevel, size >= 2)
// to its local UIP; on success the block becomes {¬uip, INVALID_LIT...}
static bool shrink_block(Kernel &k, std::vector<Lit> &learned, size_t b,
                         size_t e, uint32_t blevel) {
  assert(k.shrinkable.empty());
  uint32_t open = static_cast<uint32_t>(e - b);
  uint32_t max_trail = 0;
  for (size_t j = b; j < e; j++) {
    const Var v = var_of(learned[j]);
    assert(k.assigned[v].level == blevel);
    max_trail = std::max(max_trail, k.assigned[v].trail);
    k.seen[v] |= SHRINKABLE; // clause literals: SEEN already set
    k.shrinkable.push_back(v);
  }

  bool failed = false;
  Lit uip = INVALID_LIT;
  size_t t = max_trail;
  for (;;) {
    Lit x;
    do {
      assert(t < k.trail.size());
      x = k.trail[t--];
    } while (!(k.seen[var_of(x)] & SHRINKABLE));
    if (open == 1) {
      uip = x;
      break;
    }
    const Assigned &a = k.assigned[var_of(x)];
    if (a.reason == DECISION_REASON) {
      // unreachable while open > 1: all shrinkable literals are at blevel,
      // whose frame starts at its decision — defensive under N1 schedules
      assert(false);
      failed = true;
      break;
    }
    if (a.binary) {
      const int tmp = shrink_literal(k, a.reason, blevel);
      if (tmp < 0)
        failed = true;
      else
        open += tmp;
    } else {
      // kissat shrink.c:180, same `minimizeticks` charge as minimize above.
      event_trace_detail(k, "analysis_tick_shrink", 1);
      kernel_charge_ticks(k, 1);
      const Clause *reason = analysis_reason_clause(k, var_of(x));
      for (const Lit m : *reason) {
        if (m == x)
          continue;
        const int tmp = shrink_literal(k, m, blevel);
        if (tmp < 0) {
          failed = true;
          break;
        }
        open += (tmp > 0);
      }
    }
    if (failed)
      break;
    open--;
  }

  if (failed) { // reset the walk marks, keep the block as is
    for (const Var v : k.shrinkable)
      k.seen[v] &= static_cast<uint8_t>(~SHRINKABLE);
    k.shrinkable.clear();
    return false;
  }

  for (size_t j = b; j < e; j++)
    learned[j] = INVALID_LIT;
  learned[b] = negate(uip);
  const Var uv = var_of(uip);
  if (!k.seen[uv])
    k.analyzed.push_back(uv);
  if (!(k.seen[uv] & BUMPED)) {
    k.analyzed_seen++;
    k.bump_order.push_back(uv);
  }
  k.seen[uv] |= SEEN; // new clause literal: bump + minimization anchor
  // walked literals resolve to ¬uip, now in the clause: mark removable so
  // lower blocks' chains may bottom out in them
  for (const Var v : k.shrinkable) {
    k.seen[v] &= static_cast<uint8_t>(~SHRINKABLE);
    k.seen[v] |= REMOVABLE;
  }
  k.shrinkable.clear();
  return true;
}

// --- watch repair for the conflict-reuse shortcut ---
// Re-point the conflict clause's watches at its two highest-level literals
// (kissat analyze.c, `conflict_size > 2`): the second watch must be at exactly
// jump2, or a backtrack below jump2 leaves the clause unit at a claimed
// fixpoint (implementation-notes §12).
static uint32_t repair_conflict_watches(Kernel &k, Ref ref, uint32_t alevel) {
  const uint32_t size = clause_at(k.arena, ref)->size;
  if (size <= 2)
    return 0; // binaries watch both literals permanently: nothing to repair
  uint32_t moved = 0;
  for (uint32_t slot = 0; slot < 2; slot++) {
    const Clause *c = clause_at(k.arena, ref);
    uint32_t best = slot;
    uint32_t best_level = k.var_level(var_of(c->lits[slot]));
    for (uint32_t j = slot + 1; j < size; j++) {
      const uint32_t lv = k.var_level(var_of(c->lits[j]));
      if (best_level >= lv)
        continue; // first maximum wins (kissat's `>=` tie-break)
      best = j;
      best_level = lv;
      if (best_level == alevel)
        break; // nothing can beat the conflict level
    }
    if (best == slot)
      continue;
    if (best > 1) {
      kernel_promote_watched_lit(k, ref, slot, best);
      moved++;
    } else { // slot 0 vs slot 1: both are watched already, so only the literal
      Clause *cw = clause_at(k.arena, ref); // order changes, not the watches
      std::swap(cw->lits[0], cw->lits[1]);
    }
  }
  return moved;
}

// Kissat's shared target calculation for backjumping and both conflict-reuse
// forms (chronological retention).
static uint32_t conflict_reuse_target(Kernel &k, uint32_t jump) {
  assert(k.level && jump < k.level);
  const uint32_t back = k.level - 1;
  if (back - jump > k.conflict_reuse_chrono_levels) {
    k.stats.chronological++;
    return back;
  }
  return jump;
}

// --- eager subsumption of recently learned clauses ---
// From kissat learn.c:70-187 (MIT). A sub-step of M3: victims are redundant, so
// Reduce's Forget licenses the deletion (implementation-notes §15).

// Is `c` a trail reason? Kissat does not ask; a garbage reason would dangle
// (N2). Only under a chronological target (tests/test_analyze.cpp
// test_eager_subsume case (c)).
static bool eager_is_reason(const Kernel &k, Ref ref, const Clause *c) {
  for (const Lit l : *c) {
    if (k.values[l] <= 0)
      continue; // a reason's forced literal is TRUE
    const Assigned &a = k.assigned[var_of(l)];
    if (!a.binary && a.reason == ref)
      return true;
  }
  return false;
}

// Delete register entries strictly subsumed by the learned clause (sizes <=
// |learned| are skipped).
static void eager_subsume_recent(Kernel &k, const Lit *lits, uint32_t size) {
  uint8_t *const marks = k.eager_marks.data();
#ifdef TACTIX_ENABLE_EVENT_TRACE
  const bool trace_eager = std::getenv("TACTIX_EVENT_TRACE_EAGER_SUBSUME");
#endif
  for (uint32_t i = 0; i < size; i++)
    marks[lits[i]] = 1;
  bool subsumed_any = false;
  for (uint32_t s = 0; s < k.eager_subsume; s++) {
    const Ref ref = k.eager_recent[s];
    if (ref == INVALID_REF)
      continue;
    Clause *c = clause_at(k.arena, ref);
#ifdef TACTIX_ENABLE_EVENT_TRACE
    if (trace_eager) {
      event_trace_detail(k, "eager_slot", s);
      event_trace_detail(k, "eager_ref", ref);
      event_trace_detail(k, "eager_id", c->id());
      event_trace_detail(k, "eager_size", c->size);
    }
#endif
    if (c->garbage() || !c->redundant() || c->size <= size)
      continue;
    k.stats.eager_tried++;
    // kissat's two-counter containment scan (`needed`, `remain`).
    uint32_t needed = size, remain = c->size;
    for (const Lit l : *c) {
      if (marks[l]) {
        if (!--needed)
          break;
      } else if (--remain < needed) {
        break;
      }
    }
    if (needed)
      continue;
    if (eager_is_reason(k, ref, c)) {
#ifdef TACTIX_ENABLE_EVENT_TRACE
      if (trace_eager)
        event_trace_detail(k, "eager_reason_skip", c->id());
#endif
      k.stats.eager_reason_skipped++;
      continue;
    }
#ifdef TACTIX_ENABLE_EVENT_TRACE
    if (trace_eager)
      event_trace_detail(k, "eager_delete", c->id());
#endif
    kernel_delete_clause(k, c);
    k.eager_recent[s] = INVALID_REF;
    k.stats.eager_subsumed++;
    subsumed_any = true;
  }
  for (uint32_t i = 0; i < size; i++)
    marks[lits[i]] = 0; // touched-restore: the array is all-zero between uses
  if (subsumed_any) { // kissat flush_last_learned: compact the live entries
    uint32_t q = 0;   // to the front so an insert evicts a dead slot first
    for (uint32_t p = 0; p < Kernel::EAGER_SUBSUME_SLOTS; p++)
      if (k.eager_recent[p] != INVALID_REF)
        k.eager_recent[q++] = k.eager_recent[p];
    while (q < Kernel::EAGER_SUBSUME_SLOTS)
      k.eager_recent[q++] = INVALID_REF;
  }
}

// kissat insert_last_learned: shift in at the front; slots beyond
// `eager_subsume` stay INVALID_REF.
static void eager_insert_recent(Kernel &k, Ref ref) {
  Ref prev = ref;
  for (uint32_t i = 0; i < k.eager_subsume; i++) {
    const Ref tmp = k.eager_recent[i];
    k.eager_recent[i] = prev;
    prev = tmp;
  }
}

// Called by backjump_apply_to: test against the register, then insert (kissat
// learn.c:205-208). Only large clauses become candidates.
void analyze_eager_subsume(Kernel &k) {
  if (!k.eager_subsume || !k.pending.valid)
    return;
  const PendingLearned &p = k.pending;
  eager_subsume_recent(k, p.lits.data(), static_cast<uint32_t>(p.lits.size()));
  if (p.ref != INVALID_REF) {
#ifdef TACTIX_ENABLE_EVENT_TRACE
    if (std::getenv("TACTIX_EVENT_TRACE_EAGER_SUBSUME")) {
      const Clause *const c = clause_at(k.arena, p.ref);
      event_trace_detail(k, "eager_insert_ref", p.ref);
      event_trace_detail(k, "eager_insert_id", c->id());
      event_trace_detail(k, "eager_insert_size", c->size);
    }
#endif
    eager_insert_recent(k, p.ref);
  }
}

// Kissat analyze.c:390-520 treats a level-one conflict as a failed-literal
// traversal: each singleton frontier is a root unit, the negated decision the
// last one. This also defines the VMTF-bumped set (N4).
static ApplyResult analyze_failed_level_one(Kernel &k, const Lit *clits,
                                            uint32_t csize) {
  assert(k.level == 1);
  assert(k.frames.size() == 2);
  assert(k.frames[1] < k.trail.size());
  assert(k.analyzed.empty());
  assert(k.bump_order.empty());

  const Lit failed = k.trail[k.frames[1]];
  const Lit not_failed = negate(failed);
  std::vector<std::pair<Lit, uint64_t>> units;
  uint32_t unresolved = 0;
  bool direct_failed = false;

  auto mark = [&](Lit lit) {
    const Var v = var_of(lit);
    const uint32_t level = k.var_level(v);
    if (!level)
      return;
    assert(level == 1);
    if (k.seen[v] & SEEN)
      return;
    k.seen[v] |= SEEN;
    k.analyzed.push_back(v);
    k.bump_order.push_back(v);
    k.analyzed_seen++;
    unresolved++;
  };

  for (uint32_t i = 0; i < csize; i++) {
    if (clits[i] == not_failed) {
      direct_failed = true;
      break;
    }
    mark(clits[i]);
  }

  auto emit_unit = [&](Lit unit) {
    const uint64_t id = ++k.next_clause_id;
    if (k.proof)
      k.proof->add_rup(0, &unit, 1);
    units.emplace_back(unit, id);
  };

  if (!direct_failed) {
    size_t pos = k.trail.size();
    bool done = false;
    while (!done) {
      Lit lit = INVALID_LIT;
      do {
        assert(pos > 0);
        lit = k.trail[--pos];
      } while (!(k.seen[var_of(lit)] & SEEN));

      // The frontier is this true literal, so its negation is a root unit.
      if (unresolved == 1)
        emit_unit(negate(lit));

      const Var v = var_of(lit);
      const Assigned &a = k.assigned[v];
      assert(a.reason != DECISION_REASON && a.reason != UNIT_REASON);

      if (a.binary) {
        const Lit other = a.reason;
        if (other == not_failed) {
          done = true;
        } else {
          mark(other);
        }
      } else {
        const Clause *reason = analysis_reason_clause(k, v);
        for (const Lit other : *reason) {
          if (other == lit)
            continue;
          if (other == not_failed) {
            done = true;
            break;
          }
          mark(other);
        }
      }
      assert(unresolved > 0);
      unresolved--;
    }
  }

  emit_unit(not_failed);

  // Kissat learns the whole failed-literal batch as one size-one/glue-zero
  // conflict result, even when the backward traversal found extra units.
  k.stats.learned++;
  k.stats.learned_lits++;

  // Unlike the ordinary conflict path, level-one failed analysis returns
  // directly to the root without taking a target/best phase snapshot.
  kernel_backtrack(k, 0);
  k.conflict_valid = false;
  k.conflict_ref = INVALID_REF;
  k.pending.valid = false;
  // A failed-literal batch is Kissat's one size-one/glue-zero learned event.
  k.pending.glue = 0;

  for (const auto &[unit, id] : units) {
    const int8_t value = k.value(unit);
    if (value < 0) {
      kernel_emit_empty(k);
      bump_and_reset_analyzed(k);
      return ApplyResult::Unsat;
    }
    if (value > 0)
      continue;
    k.unit_ids[var_of(unit)] = id;
    backjump_assign_learned_unit(k, unit);
  }

  // Reference order: learn/backtrack/assign, then bump.
  bump_and_reset_analyzed(k);
  return ApplyResult::Ok;
}

// Target modes (analyze.hpp): Explicit clamps a pre-chosen target. Otherwise
// reuse paths use conflict_reuse_target, as does the learned clause when
// `chosen` is set (Chrono); with null `chosen` (Advised) the caller picks.
// Chrono and Advised share one instantiation to keep inlining.
template <bool ExplicitTarget>
static ApplyResult analyze_apply_internal(Kernel &k, uint32_t requested_target,
                                          RetierSchedule *retier,
                                          uint32_t *chosen) {
  assert(analyze_guard(k));
  if (!materialize_affine_ancestors_before_analysis(k)) {
    retract_unaffordable_affine_reasons(k);
    return ApplyResult::Ok;
  }
  assert(k.analyzed.empty());
  assert(k.bump_order.empty());
  std::vector<Lit> &learned = k.learned_scratch; // no per-conflict alloc
  Lit uip = INVALID_LIT;
  bool otfs_round = false;
  for (;;) { // one round per conflict; OTFS restarts with a stronger one
    uint32_t csize;
    const Lit *clits = conflict_lits(k, &csize);
    uint32_t alevel = 0;         // analysis level: max over conflict literals
    uint32_t conflict_nonroot = 0; // OTFS-subsume compares against this
    for (uint32_t i = 0; i < csize; i++) {
      alevel = std::max(alevel, k.var_level(var_of(clits[i])));
      if (k.var_level(var_of(clits[i])) > 0)
        conflict_nonroot++;
    }
    if (alevel == 0) { // conflict entirely at the root: refutation done
      k.inconsistent = true;
      k.conflict_valid = false;
      if (k.proof)
        k.proof->add_empty(0);
      return ApplyResult::Unsat;
    }
    // Backtrack to the conflict level (kissat analyze.c
    // one_literal_on_conflict_level): with chronological backtracking `alevel`
    // can be below the current level, and the gap, walk start and staleness
    // test are relative to it. No-op with chrono off.
    if (alevel < k.level) {
      unwind_after_conflict(k, alevel);
    }

    // One literal at `alevel` is already asserting after backtracking to
    // `jump2`: reuse it with the chronological-gap target. Canonicalize watches
    // first, also at every OTFS pass (implementation-notes §12).
    if (alevel > 0 && k.conflict_ref != INVALID_REF &&
        repair_conflict_watches(k, k.conflict_ref, alevel))
      k.stats.conflict_watch_repaired++;

    // Test reuse at every OTFS pass, before another 1-UIP/minimize round can
    // change the replacement (implementation-notes.md §12).
    if (alevel > 0 && k.conflict_reuse) {
      uint32_t at_alevel = 0, jump2 = 0;
      Lit forced = INVALID_LIT;
      for (uint32_t i = 0; i < csize; i++) {
        const uint32_t lv = k.var_level(var_of(clits[i]));
        if (lv == alevel) {
          at_alevel++;
          forced = clits[i];
        } else if (lv > jump2) {
          jump2 = lv;
        }
      }
      if (at_alevel == 1) {
        const Ref cref = k.conflict_ref;
        const Lit other = cref == INVALID_REF
                              ? (k.conflict_bin[0] ^ k.conflict_bin[1] ^ forced)
                              : INVALID_LIT;
        // Reuse needs watches on the implied literal and a false literal at
        // exactly `jump2` (repair_conflict_watches).
#ifndef NDEBUG
        if (cref != INVALID_REF) { // post-repair: exactly the invariant above
          const Clause *cc = clause_at(k.arena, cref);
          if (cc->size > 2) {
            assert(cc->lits[0] == forced);
            assert(k.var_level(var_of(cc->lits[1])) == jump2);
          }
        }
#endif
        const uint32_t target =
            ExplicitTarget ? std::clamp(requested_target, jump2, k.level - 1)
                           : conflict_reuse_target(k, jump2);
        unwind_after_conflict(k, target);
        analyze_retire_otfs_reasons(k);
        if (!jump2) {
          // A reused root implication is propagation (Kissat inlineassign.h):
          // no Deduce, no EVSIDS advance.
          const uint64_t id = ++k.next_clause_id;
          if (k.proof)
            k.proof->add_rup(id, &forced, 1);
          k.unit_ids[var_of(forced)] = id;
          kernel_assign_unit(k, forced);
          heuristics_deactivate_var(k, var_of(forced));
        } else if (cref == INVALID_REF)
          // Kissat's `kissat_assign_binary` reason-jumps without the big/big
          // check.
          kernel_assign_binary(k, forced, other, /*require_bigbig=*/false);
        else
          kernel_assign(k, forced, jump2, cref, /*binary=*/false);
        k.conflict_valid = false;
        k.conflict_ref = INVALID_REF;
        k.pending.valid = false; // nothing to assert: no clause was learned
        k.stats.conflict_reused++;
        bump_and_reset_analyzed(k);
        return ApplyResult::Ok;
      }
    }

    // More than one literal at level one: Kissat's failed-literal case.
    if (alevel == 1) {
      uint32_t on_level_one = 0;
      for (uint32_t i = 0; i < csize; i++)
        on_level_one += k.var_level(var_of(clits[i])) == 1;
      if (on_level_one > 1)
        return analyze_failed_level_one(k, clits, csize);
    }

    // Kissat marks the conflict used inside `kissat_deduce_first_uip_clause`,
    // after the reuse shortcut.
    if (k.conflict_ref != INVALID_REF)
      mark_used(k, clause_at(k.arena, k.conflict_ref));

    const Ref conflict_ref0 = k.conflict_ref; // may be garbage'd by OTFS

    learned.clear();
    learned.push_back(INVALID_LIT); // slot 0: the asserting literal (UIP)
    uint32_t open = 0;           // marked vars at the analysis level
    uint32_t resolvent_size = 0; // marked, not yet resolved-away vars

    auto mark = [&](Lit l) {
      const Var v = var_of(l);
      const uint32_t lv = k.var_level(v);
      if (lv == 0) // root literals resolve away for free
        return;
      if (k.seen[v])
        return;
      k.seen[v] = SEEN;
      event_trace_detail(k, "analysis_mark", kernel_externalize(k, l));
      k.analyzed.push_back(v);
      k.bump_order.push_back(v);
      k.analyzed_seen++;
      resolvent_size++;
      if (lv == alevel)
        open++;
      else
        learned.push_back(l);
    };

    for (uint32_t i = 0; i < csize; i++)
      mark(clits[i]);
    assert(open > 0);

    // resolve backwards over the trail until one marked literal remains
    size_t i = k.trail.size();
    uint32_t resolved = 0;
    bool restart_analysis = false;
    for (;;) {
      assert(i > 0);
      Lit t;
      // Resolve only assertion-level literals (`var_level == alevel`,
      // D-LEVEL-2), so an unsorted trail is handled.
      do {
        t = k.trail[--i];
      } while (!(k.seen[var_of(t)] && k.var_level(var_of(t)) == alevel));
      if (open == 1) {
        uip = t;
        break;
      }
      open--;
      resolved++;
      const Assigned &a = k.assigned[var_of(t)];
      event_trace_detail(k, "analysis_resolve", kernel_externalize(k, t));
      if (a.binary) {
        event_trace_detail(k, "analysis_binary_reason",
                           kernel_externalize(k, a.reason));
        if (a.jumped)
          event_trace_detail(k, "analysis_jumped_over",
                             kernel_externalize(k,
                                                k.jumped_over[var_of(t)]));
        mark(a.reason); // the other literal of the binary reason clause
        resolvent_size--;
      } else {
        assert(a.reason != DECISION_REASON && a.reason != UNIT_REASON);
        Clause *reason = analysis_reason_clause(k, var_of(t));
        const Ref reason_ref = k.assigned[var_of(t)].reason;
        mark_used(k, reason);
        event_trace_detail(k, "analysis_reason_ref", reason_ref);
        event_trace_detail(k, "analysis_reason_id", reason->id());
        event_trace_detail(k, "analysis_reason_order",
                           kernel_clause_position(k, reason_ref));
        event_trace_detail(k, "analysis_reason_size", reason->size);
        uint32_t antecedent = 1; // t's own slot in the reason
        for (const Lit l : *reason)
          if (l != t) {
            event_trace_detail(k, "analysis_reason_lit",
                               kernel_externalize(k, l));
            if (k.var_level(var_of(l)) > 0)
              antecedent++;
            mark(l);
          }
        resolvent_size--; // t leaves the resolvent
        // OTFS (kissat deduce.c): resolvent ⊆ reason ∖ {t}, so a strictly
        // smaller size means equality. Detached theory reasons are not
        // strengthened (as in CryptoMiniSat).
        if (!reason->detached_reason() && antecedent > 2 &&
            resolvent_size < antecedent) {
          const bool subsumes_conflict = resolved == 1 &&
                                         conflict_ref0 != INVALID_REF &&
                                         resolvent_size < conflict_nonroot;
          otfs_strengthen(k, reason_ref, t,
                          subsumes_conflict ? conflict_ref0 : INVALID_REF);
          restart_analysis = true;
          break;
        }
      }
    }
    if (!restart_analysis)
      break;
    // Kissat counts the strengthened clause as a conflict before re-analysis;
    // every deadline reads this clock.
    k.stats.conflicts++;
    // Kissat applies the glue-tier deadline here, before the bump and the next
    // OTFS round.
    if (retier)
      reduce_retier_at_conflict(k, *retier, k.h && k.h->stable ? 1u : 0u);
    bump_and_reset_analyzed(k); // kissat bumps each aborted round too
    otfs_round = true;
  }
  learned[0] = negate(uip);
#ifdef TACTIX_ENABLE_EVENT_TRACE
  std::vector<Lit> otfs_raw_trace, otfs_sorted_trace;
  bool learned_sorted_traced = false;
  if (otfs_round)
    otfs_raw_trace = learned;
  else
#endif
    event_trace_learned_stage(k, "learned_raw", learned);

  // shrink + minimize (kissat analyze.c, shrink=3): collapse each level block
  // to its local UIP, else minimize literals individually.
  if (learned.size() > 2) {
    // Group into blocks by descending level while preserving deduction order
    // within each level, matching Kissat's stable bucket sort.
    radix_sort_learned_by_level(k, learned);
#ifdef TACTIX_ENABLE_EVENT_TRACE
    if (otfs_round)
      otfs_sorted_trace = learned;
    else {
      learned_sorted_traced = true;
#endif
      event_trace_learned_stage(k, "learned_sorted", learned);
#ifdef TACTIX_ENABLE_EVENT_TRACE
    }
#endif
    // Kissat processes blocks from the tail (lower levels first); the clause
    // keeps descending-level order.
    size_t e = learned.size();
    while (e > 1) {
      const uint32_t blevel = k.var_level(var_of(learned[e - 1]));
      size_t b = e - 1;
      while (b > 1 && k.var_level(var_of(learned[b - 1])) == blevel)
        b--;
      // shrink=3 opens the pipeline only for blocks of at least two literals.
      if (e - b >= 2) {
        if (shrink_block(k, learned, b, e, blevel)) {
          k.stats.shrunken += (e - b) - 1;
        } else {
          for (size_t j = b; j < e; j++)
            if (literal_redundant(k, learned[j], 0)) {
              learned[j] = INVALID_LIT;
              k.stats.minimized++;
            }
        }
      }
      e = b;
    }
    size_t kept = 1;
    for (size_t j = 1; j < learned.size(); j++)
      if (learned[j] != INVALID_LIT)
        learned[kept++] = learned[j];
    learned.resize(kept);
  }
  if (learned.size() <= 2) {
#ifdef TACTIX_ENABLE_EVENT_TRACE
    if (otfs_round)
      otfs_sorted_trace = learned;
    else if (!learned_sorted_traced)
#endif
      event_trace_learned_stage(k, "learned_sorted", learned);
  }

  // glue (LBD) + backjump level from the non-asserting literals
  const uint32_t glue = count_glue(k, learned.data() + 1,
                                   static_cast<uint32_t>(learned.size()) - 1);
  uint32_t jump = 0;
  uint32_t high_idx = 0; // index of a literal at the jump level
  for (uint32_t j = 1; j < learned.size(); j++) {
    const uint32_t lv = k.var_level(var_of(learned[j]));
    if (lv > jump) {
      jump = lv;
      high_idx = j;
    }
  }

  // Learn a strict subclause of an OTFS replacement; an identical one would
  // only duplicate it.
  bool duplicate_otfs_replacement = false;
  if (otfs_round && jump > 0) {
    uint32_t csize;
    const Lit *clits = conflict_lits(k, &csize);
    if (csize == learned.size()) {
      duplicate_otfs_replacement = true;
      for (const Lit l : learned) {
        bool found = false;
        for (uint32_t i = 0; i < csize; i++)
          found |= clits[i] == l;
        if (!found) {
          duplicate_otfs_replacement = false;
          break;
        }
      }
    }
  }
  if (duplicate_otfs_replacement) {
    const Ref cref = k.conflict_ref;
    const Lit forced = learned[0];
    const Lit other = cref == INVALID_REF
                          ? (k.conflict_bin[0] ^ k.conflict_bin[1] ^ forced)
                          : INVALID_LIT;
    if (cref != INVALID_REF && repair_conflict_watches(k, cref, k.level))
      k.stats.conflict_watch_repaired++;
    const uint32_t target =
        ExplicitTarget ? std::clamp(requested_target, jump, k.level - 1)
                       : conflict_reuse_target(k, jump);
    unwind_after_conflict(k, target);
    analyze_retire_otfs_reasons(k);
    if (cref == INVALID_REF)
      kernel_assign_binary(k, forced, other, /*require_bigbig=*/false);
    else
      kernel_assign(k, forced, jump, cref, /*binary=*/false);
    k.conflict_valid = false;
    k.conflict_ref = INVALID_REF;
    k.pending.valid = false;
    k.stats.conflict_reused++;
    reset_analyzed_without_bump(k);
    return ApplyResult::Ok;
  }

#ifdef TACTIX_ENABLE_EVENT_TRACE
  if (otfs_round) {
    event_trace_learned_stage(k, "learned_raw", otfs_raw_trace);
    event_trace_learned_stage(k, "learned_sorted", otfs_sorted_trace);
  }
#endif
  event_trace_learned_stage(k, "learned_shrunk", learned);

  // Reason-side bumping on the final clause, before the bump (kissat
  // analyze.c:566); never on the OTFS restart or root refutation path.
  if (k.bump_reason_limit)
    bump_reason_side(k, learned);

  // proof delta first (add-RUP), then the clause joins R
  const uint64_t learned_id = ++k.next_clause_id;
  if (k.proof) // streaming PBP tracks the learned id for `del id`
    k.proof->add_rup(learned_id, learned.data(),
                     static_cast<uint32_t>(learned.size()));
  k.stats.learned++;
  k.stats.learned_lits += learned.size();
  k.stats.learned_glue_sum += glue;

  PendingLearned &p = k.pending;
  p.lits = learned;
  p.ref = INVALID_REF;
  p.glue = glue;
  p.jump_level = jump;
  p.id = learned_id;
  if (learned.size() >= 3) {
    // watch invariant after backjump: lits[0] asserting, lits[1] at the
    // jump level (the highest remaining false literal)
    std::swap(p.lits[1], p.lits[high_idx]);
    p.ref = kernel_add_large_clause(k, p.lits.data(),
                                    static_cast<uint32_t>(p.lits.size()),
                                    true, glue, learned_id);
    k.watches.defrag_if_needed();
  } else if (learned.size() == 2) {
    kernel_add_binary_clause(k, p.lits[0], p.lits[1], learned_id);
    k.watches.defrag_if_needed();
  } else {
    // learned unit: give the var its unit id (Backjump asserts it)
    k.unit_ids[var_of(p.lits[0])] = learned_id;
  }

  k.conflict_valid = false;
  p.valid = true;
  if (!ExplicitTarget && chosen)
    *chosen = conflict_reuse_target(k, jump);
  return ApplyResult::Ok;
}

static RetierSchedule *installed_retier(Kernel &k) {
  return k.retier_schedule.enabled ? &k.retier_schedule : nullptr;
}

ApplyResult analyze_apply(Kernel &k) {
  return analyze_apply_internal<false>(k, 0, installed_retier(k), nullptr);
}

ApplyResult analyze_apply_with_target(Kernel &k, uint32_t target_level) {
  return analyze_apply_internal<true>(k, target_level, installed_retier(k),
                                      nullptr);
}

ApplyResult analyze_apply_native(Kernel &k, AnalysisTarget mode,
                                 RetierSchedule *retier, uint32_t *chosen) {
  assert(mode != AnalysisTarget::Explicit);
  return analyze_apply_internal<false>(
      k, 0, retier, mode == AnalysisTarget::Chrono ? chosen : nullptr);
}

ApplyResult analyze_apply_chrono(Kernel &k, uint32_t *chosen) {
  return analyze_apply_internal<false>(k, 0, installed_retier(k), chosen);
}

// ConflictAnalysis, the atomic learn+backjump rule (calculus §3 M3): derive and
// emit the learned clause, then backjump unless it refuted. No `pending` state
// is visible at a rule boundary.
static ApplyResult conflictanalysis_apply_to(Kernel &k, uint32_t target) {
  return execution::execute_action(
      k, execution::ExplicitConflictAnalysisRequest{target});
}

static ApplyResult conflictanalysis_apply(Kernel &k) {
  return conflictanalysis_apply_to(k, 0); // clamps up to the true jump level
}

// Instance surface (D-LEVEL-2): every level 0..level-1 is offered and clamped
// to [jump_level, level-1] after analysis. INSTANCE_CANONICAL = jump_level.
static uint32_t conflictanalysis_instances(Kernel &k, uint64_t *out,
                                           uint32_t cap) {
  const uint32_t n = std::min<uint32_t>(cap, k.level); // levels 0..level-1
  for (uint32_t i = 0; i < n; i++)
    out[i] = i;
  return n;
}

static ApplyResult conflictanalysis_apply_instance(Kernel &k, uint64_t inst) {
  if (inst == INSTANCE_CANONICAL)
    return conflictanalysis_apply(k);
  const uint32_t target =
      inst > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(inst);
  return conflictanalysis_apply_to(k, target);
}

// ANALYZE_TARGET_CHRONO selects conflict_reuse_target after learning; other
// payloads are the v1 level.
static ApplyResult conflictanalysis_apply_semantic_instance(Kernel &k,
                                                            uint64_t inst) {
  if (inst == ANALYZE_TARGET_CHRONO)
    return execution::execute_action(
        k, execution::ChronoConflictAnalysisRequest{});
  return conflictanalysis_apply_instance(k, inst);
}

TACTIX_REGISTER_RULE(ConflictAnalysis, RuleTier::Micro, analyze_guard,
                     conflictanalysis_apply, conflictanalysis_instances,
                     conflictanalysis_apply_instance, nullptr,
                     conflictanalysis_apply_semantic_instance);

} // namespace tactix
