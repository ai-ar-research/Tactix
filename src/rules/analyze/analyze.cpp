// 1-UIP scheme after kissat/src/analyze.c + deduce.c, with recursive clause
// minimization after minimize.c (Sörensson–Biere), all-UIP block shrinking
// after shrink.c (Fleury–Biere, SAT'20) and on-the-fly subsumption after
// deduce.c + strengthen.c — all MIT, transliterated. Two deviations: the
// analysis runs at the maximum level of the conflict literals, since a legal
// N1 schedule can put a conflict entirely below the current level and
// soundness may not depend on the schedule; and singleton-level literals still
// get minimized (see the block pass).
#include "rules/analyze/analyze.hpp"
#include "policies/event_trace.hpp"
#include "policies/heuristics.hpp"

#include "proof/proof.hpp"
#include "rules/backjump/backjump.hpp" // backjump_apply: the fused rule's 2nd step

#include <algorithm>
#include <iterator>

namespace tactix {

// bit flags in Kernel::seen during analysis
static constexpr uint8_t SEEN = 1;      // literal is in the learned clause
static constexpr uint8_t REMOVABLE = 2; // proven redundant
static constexpr uint8_t POISONED = 4;  // proven not redundant
static constexpr uint8_t SHRINKABLE = 8; // in the current block-shrink walk
static constexpr uint8_t CHAINED = 16;   // antecedent recorded (LRAT)
static constexpr uint8_t REASON_BUMP = 32; // marked by reason-side bumping

// The bump set = SEEN ∪ REASON_BUMP. kissat keeps exactly this set on its
// `analyzed` stack; `k.analyzed` here also holds minimization scratch and LRAT
// antecedents, so membership is a flag test and the size is counted apart.
static constexpr uint8_t BUMPED = SEEN | REASON_BUMP;

// Kissat's conflict unwind is deliberately two-stage: first remove the
// current decision level, then snapshot target/best phases, then continue to
// the semantic conflict level. Besides phase history, this fixes the order in
// which lazily removed equal-score variables re-enter the stable heap.
static void unwind_after_conflict(Kernel &k, uint32_t target) {
  if (k.level) {
    kernel_backtrack(k, k.level - 1);
    if (k.h)
      heuristics_update_target_phases(k);
  }
  if (target < k.level)
    kernel_backtrack(k, target);
}

// --- native LRAT antecedent capture. CaDiCaL's id scheme with a simpler
// ordering: entries carry the trail position of the literal their antecedent
// propagated, and the chain is emitted ascending with the conflict last, so it
// unit-propagates for the checker by construction. LRAT-gated throughout.

static bool lrat_active(const Kernel &k) {
  return k.proof && k.proof->lrat();
}

// record var v's reason as an antecedent: the checker re-propagates v
static void chain_record(Kernel &k, Var v) {
  if (k.seen[v] & CHAINED)
    return;
  const Assigned &a = k.assigned[v];
  assert(a.reason != DECISION_REASON && a.reason != UNIT_REASON);
  uint64_t id;
  if (a.binary) {
    const Lit assigned_lit = make_lit(v, k.values[make_lit(v, true)] > 0);
    // A jumped reason names a virtual resolvent: cite the real first
    // antecedent and defer the resolved-away literal to
    // `chain_expand_jumped`, since whether it needs an antecedent depends on
    // the final clause — if it lands in it, the checker has it assigned and
    // its reason would be a satisfied, illegal hint. Its trail position is
    // smaller, so a deferred entry sorts ahead of this one.
    if (a.jumped) {
      // never a root literal: the jump is taken only when the resolved-away
      // literal sits above level 0 (kernel_assign_binary)
      assert(k.var_level(var_of(k.jumped_over[v])) != 0);
      k.jump_pending.push_back(var_of(k.jumped_over[v]));
      id = kernel_binary_id(k, assigned_lit, k.jumped_over[v]);
    } else {
      id = kernel_binary_id(k, assigned_lit, a.reason);
    }
  } else {
    id = clause_at(k.arena, a.reason)->id();
  }
  if (!k.seen[v])
    k.analyzed.push_back(v); // ensure flag cleanup
  k.seen[v] |= CHAINED;
  k.lrat_entries.emplace_back(a.trail, id);
}

// record a root-assigned var's UNIT clause as an antecedent
static void chain_record_root(Kernel &k, Var v) {
  if (k.seen[v] & CHAINED)
    return;
  if (!k.seen[v])
    k.analyzed.push_back(v);
  k.seen[v] |= CHAINED;
  k.lrat_entries.emplace_back(k.assigned[v].trail, k.unit_ids[v]);
}

// Close out the literals reason jumping resolved past, against the final
// clause: one that is in it is already assigned for the checker, so its reason
// would be a satisfied hint; one that is not gets chained, possibly deferring
// further (jump chains).
static void chain_expand_jumped(Kernel &k, const Lit *lits, uint32_t size) {
  while (!k.jump_pending.empty()) {
    const Var v = k.jump_pending.back();
    k.jump_pending.pop_back();
    if (k.seen[v] & CHAINED)
      continue;
    bool in_clause = false;
    for (uint32_t i = 0; i < size && !in_clause; i++)
      in_clause = var_of(lits[i]) == v;
    if (!in_clause)
      chain_record(k, v);
  }
}

// emit order: kernel_lrat_chain (ascending trail position, conflict last)
static void chain_emit(Kernel &k, uint64_t id, const Lit *lits,
                       uint32_t size, uint64_t conflict_id) {
  if (!k.jump_pending.empty())
    chain_expand_jumped(k, lits, size);
  kernel_lrat_chain(k, conflict_id);
  k.proof->add_rup(id, lits, size, k.lrat_chain.data(),
                   static_cast<uint32_t>(k.lrat_chain.size()));
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
      c->set_glue(glue);
      k.stats.promoted++;
    }
  }
  // per-mode histogram of the glues actually used, post-promotion (kissat
  // deduce.c:20-22); Reduce's dynamic tiers are quantiles of it
  const uint32_t mode = (k.h && k.h->stable) ? 1u : 0u;
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

// A learned literal l is redundant if every antecedent in its reason chain
// bottoms out in root assignments or literals already in the clause.  Kissat
// deliberately allows the walk to cross intermediate decision levels absent
// from the learned clause; requiring clause-level membership here only keeps
// literals whose redundancy the same reason graph proves.
// Kissat's shipped `minimizedepth=1000`; it rejects the node at depth 1000.
// Binary reason runs are iterative there and do not consume this recursion
// budget.  Keep the constant authored here: it is analysis mechanism, not a
// policy arm, and the former 900/override arm was measured neutral and closed.
static constexpr unsigned MINIMIZE_DEPTH_LIMIT = 1000;

static bool literal_redundant(Kernel &, Lit, unsigned);

// Kissat's `minimize_binary` walks a binary-reason run iteratively, then
// assigns the whole run the result of its first non-binary/root/cached end.
// Besides matching the reference depth accounting, this avoids consuming C++
// call stack on arbitrarily long implication chains.
static bool binary_chain_redundant(Kernel &k, Lit next, unsigned depth) {
  const size_t saved = k.minimize_scratch.size();
  bool ok = false;
  for (;;) {
    const Var v = var_of(next);
    const Assigned &a = k.assigned[v];
    if (a.level == 0) {
      if (lrat_active(k))
        chain_record_root(k, v);
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
    kernel_charge_ticks(k, 1);
    const Clause *reason = clause_at(k.arena, a.reason);
    const Lit assigned_lit = negate(next);
    ok = true;
    for (const Lit m : *reason) {
      if (m == assigned_lit)
        continue;
      const Var mv = var_of(m);
      if (k.var_level(mv) == 0) {
        if (lrat_active(k))
          chain_record_root(k, mv);
        continue;
      }
      if (k.seen[mv] & SEEN)
        continue;
      if (!literal_redundant(k, m, depth + 2)) {
        ok = false;
        break;
      }
    }
    break;
  }

  // Recursive Tactix used to mark these while unwinding, so retain the same
  // deepest-first order for cleanup and native-LRAT antecedent capture.
  for (size_t i = k.minimize_scratch.size(); i > saved; i--) {
    const Var v = k.minimize_scratch[i - 1];
    const uint8_t before = k.seen[v];
    k.seen[v] |= ok ? REMOVABLE : POISONED;
    if (!before)
      k.analyzed.push_back(v);
    if (ok && lrat_active(k))
      chain_record(k, v);
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
  if (a.reason == DECISION_REASON || depth >= MINIMIZE_DEPTH_LIMIT) {
    if (depth >= MINIMIZE_DEPTH_LIMIT)
      k.stats.minimize_depth_cutoffs++;
    const uint8_t before = k.seen[v];
    k.seen[v] |= POISONED;
    if (!before) // no other flag: newly touched
      k.analyzed.push_back(v); // ensure cleanup
    return false;
  }
  const Lit assigned_lit = negate(l); // l is false; the var's true literal
  bool ok = true;
  if (a.binary) {
    ok = binary_chain_redundant(k, a.reason, depth);
  } else {
    // One search tick per reason deref, as kissat minimize.c:52 charges for
    // the same walk. `search_ticks` denominates the stable-mode budget and
    // every effort limit, so omitting it stretches those windows.
    kernel_charge_ticks(k, 1);
    const Clause *reason = clause_at(k.arena, a.reason);
    for (const Lit m : *reason) {
      if (m == assigned_lit)
        continue;
      const Var mv = var_of(m);
      if (k.var_level(mv) == 0) {
        if (lrat_active(k))
          chain_record_root(k, mv);
        continue;
      }
      if (k.seen[mv] & SEEN)
        continue;
      if (!literal_redundant(k, m, depth + 1)) {
        ok = false;
        break;
      }
    }
  }
  const uint8_t before = k.seen[v];
  k.seen[v] |= ok ? REMOVABLE : POISONED;
  if (!before)
    k.analyzed.push_back(v); // newly touched: schedule flag cleanup
  if (ok && lrat_active(k))
    chain_record(k, v); // the checker re-propagates the removed literal
  return ok;
}

// LSD radix sort of `analyzed` by VMTF stamp, ascending (kissat sort.h
// RADIX_STACK); stamps are unique, so the order matches a comparison sort.
static void radix_sort_analyzed_by_stamp(Kernel &k) {
  std::vector<Var> &a = k.analyzed;
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

// Kissat's sort_deduced_clause orders decision-level buckets descending and
// preserves deduction order within each bucket.  Stable LSD radix sorting on
// the complemented level reproduces that order without per-conflict
// allocation; OTFS scratch is dead once the final learned clause is reached.
static void radix_sort_learned_by_level(Kernel &k, std::vector<Lit> &learned) {
  const size_t n = learned.size() - 1;
  if (n < 2)
    return;
  std::vector<Lit> &scratch = k.otfs_scratch;
  scratch.resize(n);
  Lit *const original = learned.data() + 1;
  Lit *src = original, *dst = scratch.data();
  for (uint32_t shift = 0; shift < 32; shift += 8) {
    uint32_t count[257] = {0};
    for (size_t i = 0; i < n; i++) {
      const uint32_t rank = ~k.var_level(var_of(src[i]));
      count[((rank >> shift) & 0xff) + 1]++;
    }
    for (uint32_t i = 1; i < 257; i++)
      count[i] += count[i - 1];
    for (size_t i = 0; i < n; i++) {
      const uint32_t rank = ~k.var_level(var_of(src[i]));
      dst[count[(rank >> shift) & 0xff]++] = src[i];
    }
    std::swap(src, dst);
  }
  if (src != original)
    std::copy(src, src + n, original);
}

static void reset_analyzed_without_bump(Kernel &k) {
  k.analyzed_seen = 0;
  k.reason_bumped.clear();
  for (const Var v : k.analyzed)
    k.seen[v] = 0;
  k.analyzed.clear();
}

// Bump analysis-side vars and clear all flags, minimization scratch included.
// Focused: VMTF move-to-front, stamp-sorted to preserve relative recency
// (kissat bump.c). Stable: EVSIDS bump + one decay, order-independent.
static void bump_and_reset_analyzed(Kernel &k) {
  k.analyzed_seen = 0;
  k.reason_bumped.clear();
  if (!k.h) {
    // Unguided (no `H`): nothing to bump, but the analysis scratch is kernel
    // state and must still be cleared.
    for (const Var v : k.analyzed)
      k.seen[v] = 0;
    k.analyzed.clear();
    return;
  }
  // attribute each bump to its channel and to whether it landed on an
  // extension variable (see Stats::bumps_extension)
  const Var ext_first = k.first_extension_var;
  auto count_bump = [&](Var v) {
    k.stats.bumps++;
    if (v >= ext_first) {
      k.stats.bumps_extension++;
      if (!(k.seen[v] & SEEN))
        k.stats.bumps_ext_reason_only++;
    }
  };
  event_trace_analyzed(k, k.analyzed, k.seen, BUMPED);
  if (k.h->stable) {
    const bool any_analyzed = !k.analyzed.empty();
    event_trace_heap_scores(k);
    for (const Var v : k.analyzed) {
      if (k.seen[v] & BUMPED) {
        count_bump(v);
        if (heuristics_var_active(k, v)) {
          heuristics_score_bump(k, v);
          event_trace_detail(k, "score_bump",
                             kernel_externalize(k, make_lit(v, false)));
          event_trace_heap_scores(k);
        }
      }
      // Preserve solver semantics while tracing: this clear is what makes
      // repeated scratch entries harmless. A logger must never extend the
      // lifetime of BUMPED or turn a duplicate into a second bump.
      k.seen[v] = 0;
    }
    event_trace_analyzed_post(k, k.analyzed);
    k.analyzed.clear();
    // Kissat advances EVSIDS only inside its `!EMPTY_STACK(analyzed)` guard.
    // Level-one/failed-literal analysis can leave that stack empty. Decaying
    // anyway puts Tactix's score increment one generation ahead; after enough
    // conflicts the resulting few-ULP differences change equal-score heap
    // order and the decision trajectory.
    if (any_analyzed)
      heuristics_scores_decay(k);
    return;
  }
  radix_sort_analyzed_by_stamp(k);
  for (const Var v : k.analyzed) {
    if (k.seen[v] & BUMPED) {
      count_bump(v);
      if (heuristics_var_active(k, v))
        heuristics_queue_bump(k, v);
    }
    k.seen[v] = 0;
  }
  k.analyzed.clear();
}

void analyze_bump_after_backjump(Kernel &k) { bump_and_reset_analyzed(k); }

// --- reason-side bumping (kissat analyze_reason_side_literals; cadical
// bump_also_all_reason_literals; MapleCOMSPS 2016) --------------------------
// Once the learned clause is final, also bump its literals' reasons, capped at
// `k.bump_reason_limit × |bump set|` new marks and
// all-or-nothing, so the set is never a truncation artefact of clause order.
// `H`-only: it changes nothing but which variables get bumped.
static bool mark_reason_side(Kernel &k, Lit m) {
  const Var v = var_of(m);
  if (k.var_level(v) == 0)
    return false; // root-fixed: kissat `a->level`
  // Optional arm (Kernel::no_ext_reason_bump): withhold extension vars from
  // this channel only, so they rise on conflict-side evidence rather than on
  // being a BVA hub.
  if (k.no_ext_reason_bump && v >= k.first_extension_var)
    return false;
  if (k.seen[v] & BUMPED)
    return false; // already in the bump set (kissat `a->analyzed`)
  // Not `if (k.seen[v])`: a var may sit in k.analyzed carrying only
  // REMOVABLE/POISONED/CHAINED, which kissat keeps on separate stacks and its
  // `!a->analyzed` test admits. Testing the whole byte bumps a smaller set.
  if (!k.seen[v])
    k.analyzed.push_back(v);
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
  kernel_charge_ticks(k, 1);
  const Clause *r = clause_at(k.arena, a.reason);
  const Lit self = negate(false_lit);
  for (const Lit m : *r) {
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
    k.stats.reason_bump_aborted++;
    // Vars newly pushed to k.analyzed stay there with seen == 0, harmlessly:
    // bump_and_reset_analyzed skips them and clears unconditionally. kissat
    // pops its stack instead, unavailable here — a reason-side var may already
    // be in k.analyzed as scratch, so these are not a suffix.
  }
  k.reason_bumped.clear();
}

// --- on-the-fly subsumption (kissat/src/deduce.c + strengthen.c) ----------
// A resolvent equal to reason ∖ {t} subsumes the reason and becomes the new
// conflict. Tactix cannot shrink a variable-size arena record in place, so it
// allocates the stronger clause while the old one remains t's valid reason.
// The fused Backjump unassigns t and then deletes the old clause; no duplicate
// crosses the public rule boundary. If the first resolvent also subsumes the
// conflict, that conflict is deleted immediately because it cannot be a trail
// reason. Evidence: benchmarks/reports/2026-08-09-tier-a-otfs-watch-repair.md.
static void otfs_strengthen(Kernel &k, Ref reason_ref, Lit t,
                            Ref subsumed_ref /* INVALID_REF if none */,
                            uint64_t conflict_chain_id) {
  const Clause *reason = clause_at(k.arena, reason_ref);
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
  const uint64_t id = ++k.next_clause_id;
  kernel_inherit_clause_order(k, id, reason->id());

  bool subsume = false, subsumed_irredundant = false;
  if (subsumed_ref != INVALID_REF) {
    const Clause *d = clause_at(k.arena, subsumed_ref);
    assert(!d->garbage());
    subsume = true;
    subsumed_irredundant = !d->redundant();
  }
  // the resolvent keeps the reason's redundancy, but must become irredundant
  // if it replaces an irredundant conflict (kissat otfs_subsume_clause)
  const bool redundant = reason->redundant() && !subsumed_irredundant;

  if (k.proof) {
    if (lrat_active(k)) // entries so far justify the resolvent (the
      chain_emit(k, id, lits.data(), static_cast<uint32_t>(lits.size()),
                 conflict_chain_id); // resolved reasons incl. t's)
    else
      k.proof->add_rup(0, lits.data(), static_cast<uint32_t>(lits.size()),
                       nullptr, 0);
  }
  k.stats.otfs_strengthened++;

  if (lits.size() == 2) {
    kernel_add_binary_clause(k, lits[0], lits[1], id);
    k.conflict_ref = INVALID_REF;
    k.conflict_bin[0] = lits[0];
    k.conflict_bin[1] = lits[1];
  } else {
    if (keep_surviving_watch) {
      // Kissat keeps the reason's other old watch and selects only its new
      // partner, taking the first literal on a level tie.  Re-selecting both
      // watches was not strictly better and changed later scan order.
      uint32_t highest = 1;
      for (uint32_t j = 2; j < lits.size(); j++)
        if (k.var_level(var_of(lits[j])) >
            k.var_level(var_of(lits[highest])))
          highest = j;
      std::swap(lits[1], lits[highest]);
    } else {
      // Tactix removes a root-false surviving watch, producing a strict
      // subclause.  With no reference watch left to preserve, choose the two
      // latest assignments so a compacting chronological backtrack cannot
      // strand the clause behind an older pair.
      for (uint32_t w = 0; w < 2; w++) {
        uint32_t best = w;
        for (uint32_t j = w + 1; j < lits.size(); j++) {
          const Assigned &aj = k.assigned[var_of(lits[j])];
          const Assigned &ab = k.assigned[var_of(lits[best])];
          if (aj.level > ab.level ||
              (aj.level == ab.level && aj.trail > ab.trail))
            best = j;
        }
        std::swap(lits[w], lits[best]);
      }
    }
    // The site kissat's strengthen.c:42-43 covers. The exact LBD is recomputed
    // where kissat clamps the old glue — a stronger input to the same
    // post-shrink promotion, so an all-distinct-level resolvent still lands a
    // band lower.
    const uint32_t glue = kernel_shrunken_glue(
        k, count_glue(k, lits.data(), static_cast<uint32_t>(lits.size())),
        static_cast<uint32_t>(lits.size()));
    // reason may dangle after this allocation; it is not used again
    k.conflict_ref = kernel_add_large_clause(
        k, lits.data(), static_cast<uint32_t>(lits.size()), redundant, glue,
        id);
    if (keep_surviving_watch)
      kernel_swap_large_watch_refs(k, surviving_watch, reason_ref,
                                   k.conflict_ref, t, lits[1]);
  }

  // The replacement assumes the old reason's eager-register identity.  A
  // binary has no arena Ref and simply vacates the slot.
  for (Ref &recent : k.eager_recent)
    if (recent == reason_ref)
      recent = k.conflict_ref;
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
      kernel_delete_clause(k, c);
    }
  }
  k.otfs_retired.clear();
}

// --- all-UIP block shrinking (kissat/src/shrink.c; Fleury–Biere SAT'20) ---
// A maximal group of learned literals on one decision level is replaced by the
// negation of that level's local UIP when the block's reason chains, walked
// down the trail, converge to a single literal.

// classify literal l (falsified) for the level-`blevel` walk: 1 = newly
// shrinkable (same level, walk continues through it), 0 = resolves away
// (root / already shrinkable / proven redundant), -1 = block can't shrink
static int shrink_literal(Kernel &k, Lit l, uint32_t blevel) {
  const Var v = var_of(l);
  const Assigned &a = k.assigned[v];
  if (a.level == 0) {
    if (lrat_active(k))
      chain_record_root(k, v);
    return 0;
  }
  if (k.seen[v] & SHRINKABLE)
    return 0;
  if (a.level < blevel) {
    // `kissat_minimize_clause` marks every literal already in the learned
    // clause removable before the default shrink=3 pass.  SEEN is Tactix's
    // equivalent membership bit; omitting it can reject a valid block collapse
    // when a reason reaches a learned literal on a lower level.
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
      kernel_charge_ticks(k, 1);
      const Clause *reason = clause_at(k.arena, a.reason);
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
  if (!(k.seen[uv] & BUMPED))
    k.analyzed_seen++;
  k.seen[uv] |= SEEN; // new clause literal: bump + minimization anchor
  // walked literals resolve to ¬uip, now in the clause: mark removable so
  // lower blocks' chains may bottom out in them
  const bool lrat = lrat_active(k);
  for (const Var v : k.shrinkable) {
    k.seen[v] &= static_cast<uint8_t>(~SHRINKABLE);
    k.seen[v] |= REMOVABLE;
    // the walked literals' reasons justify the collapse; the UIP stays out of
    // the chain, its literal joining the clause and so being a satisfied hint
    if (lrat && v != uv)
      chain_record(k, v);
  }
  k.shrinkable.clear();
  return true;
}

// --- watch repair for the conflict-reuse shortcut --------------------------
// Re-point the conflict clause's two watches at its two highest-level
// literals, transferring the watch entries (kissat analyze.c, the
// `conflict_size > 2` block). The invariant: after the shortcut assigns
// `forced` at `jump2` with this clause as reason, that is the only true
// literal, so the other watch must be a literal at exactly jump2 — otherwise a
// backtrack below jump2 unassigns `forced`, nothing rescans, and the clause
// sits unit at a claimed fixpoint. docs/implementation-notes.md §12.
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

// Kissat's single target calculation is shared by ordinary learned-clause
// backjumping and both forms of conflict reuse (original and OTFS replacement).
// Keeping it here prevents a reuse path from silently bypassing chronological
// retention, as the duplicate-OTFS path did on Fmla at conflict 844.
static uint32_t conflict_reuse_target(Kernel &k, uint32_t jump) {
  assert(k.level && jump < k.level);
  const uint32_t back = k.level - 1;
  if (back - jump > k.conflict_reuse_chrono_levels) {
    k.stats.chronological++;
    return back;
  }
  return jump;
}

// --- eager subsumption of recently learned clauses -------------------------
// Transliterated from kissat learn.c:70-187 (MIT). Not a registered rule: the
// clauses it deletes are redundant, so S2 Reduce's unconditional Forget
// licenses the deletion and the subsumption test only selects victims. A
// sub-step of M3; see docs/implementation-notes.md §15.

// Is `c` the reason of a trail literal? Kissat does not ask; this kernel must,
// or a garbage reason dangles for kernel_collect_garbage and a deleted id
// enters the next LRAT chain (N2). Reachable only under a chronological
// backjump target, which DefaultPolicy never picks, so the branch is pinned by
// tests/test_analyze.cpp test_eager_subsume case (c).
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

// Delete every register entry strictly subsumed by the just-learned clause.
// Strictly: candidates of size <= |learned| are skipped, so an identical
// clause is never deleted, nor is the clause about to be inserted.
static void eager_subsume_recent(Kernel &k, const Lit *lits, uint32_t size) {
  uint8_t *const marks = k.eager_marks.data();
  for (uint32_t i = 0; i < size; i++)
    marks[lits[i]] = 1;
  bool subsumed_any = false;
  for (uint32_t s = 0; s < k.eager_subsume; s++) {
    const Ref ref = k.eager_recent[s];
    if (ref == INVALID_REF)
      continue;
    Clause *c = clause_at(k.arena, ref);
    if (c->garbage() || !c->redundant() || c->size <= size)
      continue;
    k.stats.eager_tried++;
    // kissat's two-counter containment scan: `needed` counts unmatched
    // subsumer literals, `remain` bounds how many of c's can still match.
    // Neither underflows — each iteration decrements exactly one.
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
      k.stats.eager_reason_skipped++;
      continue;
    }
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

// kissat insert_last_learned: shift the new ref in at the front and let the
// oldest fall off the active prefix. Slots beyond `eager_subsume` are never
// read and stay INVALID_REF, which the debug register check relies on.
static void eager_insert_recent(Kernel &k, Ref ref) {
  Ref prev = ref;
  for (uint32_t i = 0; i < k.eager_subsume; i++) {
    const Ref tmp = k.eager_recent[i];
    k.eager_recent[i] = prev;
    prev = tmp;
  }
}

// The one entry point, called by backjump_apply_to: test the pending clause
// against the register, then insert it (kissat's order, learn.c:205-208), so a
// clause is never compared with itself. Units and binaries subsume too; only
// large clauses become future candidates, having a ref.
void analyze_eager_subsume(Kernel &k) {
  if (!k.eager_subsume || !k.pending.valid)
    return;
  const PendingLearned &p = k.pending;
  eager_subsume_recent(k, p.lits.data(), static_cast<uint32_t>(p.lits.size()));
  if (p.ref != INVALID_REF)
    eager_insert_recent(k, p.ref);
}

ApplyResult analyze_apply(Kernel &k) {
  assert(analyze_guard(k));
  k.analysis_conflict_checkpoints.clear();
  std::vector<Lit> &learned = k.learned_scratch; // no per-conflict alloc
  const bool lrat = lrat_active(k);
  uint64_t conflict_chain_id = 0; // the round's conflict, emitted last
  Lit uip = INVALID_LIT;
  bool otfs_round = false;
  for (;;) { // one round per conflict; OTFS restarts with a stronger one
    uint32_t csize;
    const Lit *clits = conflict_lits(k, &csize);
    if (lrat) {
      k.lrat_entries.clear();
      k.jump_pending.clear(); // paired with lrat_entries: same round scope
      conflict_chain_id =
          k.conflict_ref == INVALID_REF
              ? kernel_binary_id(k, k.conflict_bin[0], k.conflict_bin[1])
              : clause_at(k.arena, k.conflict_ref)->id();
    }
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
      if (k.proof) {
        if (lrat) { // chain: the conflict lits' root units + the conflict
          for (uint32_t i = 0; i < csize; i++)
            chain_record_root(k, var_of(clits[i]));
          chain_emit(k, ++k.next_clause_id, nullptr, 0, conflict_chain_id);
        } else {
          k.proof->add_empty(0, nullptr, 0);
        }
      }
      return ApplyResult::Unsat;
    }
    // Backtrack to the conflict level (kissat analyze.c
    // one_literal_on_conflict_level). Chronological backtracking leaves the
    // trail unsorted by level, so `alevel` can sit below the current decision
    // level, and everything downstream — the backjump gap, the 1-UIP walk's
    // start, the asserting literal's staleness test — is defined relative to
    // the level the conflict actually lives at. Analysing from higher
    // overstates the gap and can leave the asserting literal assigned, which
    // reads as a stale pending clause and drops the conflict. A no-op with
    // chrono off, where alevel == k.level always.
    if (alevel < k.level) {
      unwind_after_conflict(k, alevel);
    }

    // A conflict with one literal at `alevel` is already asserting after a
    // backtrack to `jump2`; reuse it without learning. Apply the ordinary
    // chronological-gap target, and let jump2==0 use the LRAT unit path.
    // Canonicalize watches first because even a declined conflict can survive
    // the backtrack and its pose controls its next observation. Repeat this at
    // every OTFS pass. See docs/implementation-notes.md §12.
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
      // Diagnostic arm (k.conflict_reuse_no_repair): decline instead of
      // repairing, so reuse can be measured without the watch-repair
      // primitive. Sound, and inert unless the knob is set.
      bool crippled_declines = false;
      if (k.conflict_reuse_no_repair && at_alevel == 1 &&
          k.conflict_ref != INVALID_REF) {
        const Clause *cc = clause_at(k.arena, k.conflict_ref);
        if (cc->size > 2)
          crippled_declines =
              !((cc->lits[0] == forced || cc->lits[1] == forced) &&
                k.var_level(var_of(cc->lits[0] == forced ? cc->lits[1]
                                                         : cc->lits[0])) ==
                    jump2);
      }
      if (at_alevel == 1 && jump2 > 0 && !crippled_declines) {
        const Ref cref = k.conflict_ref;
        const Lit other = cref == INVALID_REF
                              ? (k.conflict_bin[0] ^ k.conflict_bin[1] ^ forced)
                              : INVALID_LIT;
        // Reuse as a reason works only if the clause watches the literal it
        // now implies and a false literal at exactly `jump2` (see
        // repair_conflict_watches). Repairing rather than declining lets the
        // shortcut fire on every such conflict.
#ifndef NDEBUG
        if (cref != INVALID_REF) { // post-repair: exactly the invariant above
          const Clause *cc = clause_at(k.arena, cref);
          if (cc->size > 2) {
            assert(cc->lits[0] == forced);
            assert(k.var_level(var_of(cc->lits[1])) == jump2);
          }
        }
#endif
        const uint32_t target = conflict_reuse_target(k, jump2);
        unwind_after_conflict(k, target);
        analyze_retire_otfs_reasons(k);
        if (cref == INVALID_REF)
          // Kissat routes reused binary conflicts through
          // `kissat_assign_binary`, whose reason-jump path is intentionally
          // not guarded by the big/big formula classification.  Storing the
          // intermediate antecedent here makes the next analysis bump a
          // variable Kissat resolves straight past.
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

    // Kissat marks the conflict as used inside
    // `kissat_deduce_first_uip_clause`, after the already-asserting conflict
    // shortcut above.  Doing this at Analyze entry counted reused conflicts
    // in the dynamic glue histogram even though Kissat never entered Deduce;
    // that one observation can move the next Reduce tier quantile.
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
      if (lv == 0) { // root literals resolve away for free — but the
        if (lrat)    // checker needs their unit ids in the chain
          chain_record_root(k, v);
        return;
      }
      if (k.seen[v])
        return;
      k.seen[v] = SEEN;
      k.analyzed.push_back(v);
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
      // Resolve only assertion-level literals: `open` counts exactly the seen
      // alevel vars, mark() pushing lower-level ones straight to `learned`.
      // The `var_level == alevel` filter (D-LEVEL-2) keeps this correct on a
      // non-level-sorted trail; on a sorted one it skips nothing.
      do {
        t = k.trail[--i];
      } while (!(k.seen[var_of(t)] && k.var_level(var_of(t)) == alevel));
      if (open == 1) {
        uip = t;
        break;
      }
      open--;
      resolved++;
      if (lrat)
        chain_record(k, var_of(t)); // the checker re-propagates t
      const Assigned &a = k.assigned[var_of(t)];
      if (a.binary) {
        mark(a.reason); // the other literal of the binary reason clause
        resolvent_size--;
      } else {
        assert(a.reason != DECISION_REASON && a.reason != UNIT_REASON);
        Clause *reason = clause_at(k.arena, a.reason);
        mark_used(k, reason);
        uint32_t antecedent = 1; // t's own slot in the reason
        for (const Lit l : *reason)
          if (l != t) {
            if (k.var_level(var_of(l)) > 0)
              antecedent++;
            mark(l);
          }
        resolvent_size--; // t leaves the resolvent
        // OTFS (kissat deduce.c): resolvent ⊆ reason ∖ {t} — since the
        // resolvent contains all non-root reason literals except t, a
        // strictly smaller size means equality
        if (antecedent > 2 && resolvent_size < antecedent) {
          const bool subsumes_conflict = resolved == 1 &&
                                         conflict_ref0 != INVALID_REF &&
                                         resolvent_size < conflict_nonroot;
          otfs_strengthen(k, a.reason, t,
                          subsumes_conflict ? conflict_ref0 : INVALID_REF,
                          conflict_chain_id);
          restart_analysis = true;
          break;
        }
      }
    }
    if (!restart_analysis)
      break;
    // Kissat counts the strengthened clause as a fresh conflict before
    // re-running 1-UIP analysis.  This is not cosmetic: restart, reduction,
    // mode, rephase, reorder, and inprocessing deadlines all read this clock.
    k.stats.conflicts++;
    AnalysisConflictCheckpoint checkpoint;
    checkpoint.conflicts = k.stats.conflicts;
    checkpoint.mode = k.h && k.h->stable ? 1u : 0u;
    std::copy(std::begin(k.stats.used_glue[checkpoint.mode]),
              std::end(k.stats.used_glue[checkpoint.mode]),
              std::begin(checkpoint.used_glue));
    k.analysis_conflict_checkpoints.push_back(checkpoint);
    // This is a policy observation at an internal atomic-rule boundary.
    // Kissat tests and applies its glue-tier deadline here, before bumping the
    // aborted analysis and before the next OTFS round can mark another clause
    // used.  The hook keeps the proof rule policy-neutral while preserving
    // that decision-observable order for DefaultPolicy.
    if (k.analysis_conflict_hook)
      k.analysis_conflict_hook(k, k.analysis_conflict_hook_state);
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

  // shrink + minimize (kissat analyze.c at its default shrink=3): per decision
  // level, first try to collapse the whole block to its local UIP; if that
  // fails, minimize its literals individually.
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
    // Kissat consumes the sorted clause from its tail: lower-level blocks run
    // first, and every successful walk leaves removable intermediates for the
    // later, higher-level blocks.  The clause itself stays in descending-level
    // order; only application order is reversed.
    size_t e = learned.size();
    while (e > 1) {
      const uint32_t blevel = k.var_level(var_of(learned[e - 1]));
      size_t b = e - 1;
      while (b > 1 && k.var_level(var_of(learned[b - 1])) == blevel)
        b--;
      bool minimize_block = true;
      if (e - b >= 2 && shrink_block(k, learned, b, e, blevel)) {
        k.stats.shrunken += (e - b) - 1;
        minimize_block = false;
      }
      if (minimize_block) {
        // Minimize literal by literal, singleton blocks included. kissat
        // skips singleton blocks; Tactix always includes them because,
        // under a free N1 schedule, a literal's chain can bottom out without
        // another literal on its own level.  Removing a proven-redundant
        // singleton yields a strict subclause, so this is a specified stronger
        // deviation rather than a reference-parity arm.
        for (size_t j = b; j < e; j++)
          if (literal_redundant(k, learned[j], 0)) {
            learned[j] = INVALID_LIT;
            k.stats.minimized++;
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

  // Kissat reuses an OTFS replacement immediately when it has one literal on
  // the highest level.  Tactix first runs its separately specified stronger
  // minimization.  If that produced a strict subclause, keep and learn it.  If
  // the final clause is exactly the replacement already in the formula,
  // learning it adds no strength and only creates a duplicate.
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
    const uint32_t target = conflict_reuse_target(k, jump);
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

  // reason-side bumping, on the final clause and before the bump (kissat
  // analyze.c:566). Off unless a policy set the limit, and never on the OTFS
  // restart path or the root-refutation exit.
  if (k.bump_reason_limit)
    bump_reason_side(k, learned);

  // proof delta first (add-RUP), then the clause joins R
  const uint64_t learned_id = ++k.next_clause_id;
  if (k.proof) {
    if (lrat)
      chain_emit(k, learned_id, learned.data(),
                 static_cast<uint32_t>(learned.size()), conflict_chain_id);
    else
      k.proof->add_rup(0, learned.data(),
                       static_cast<uint32_t>(learned.size()), nullptr, 0);
  }
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
  } else if (learned.size() == 2) {
    kernel_add_binary_clause(k, p.lits[0], p.lits[1], learned_id);
  } else {
    // learned unit: give the var its unit id (Backjump asserts it)
    k.unit_ids[var_of(p.lits[0])] = learned_id;
  }

  k.conflict_valid = false;
  p.valid = true;
  return ApplyResult::Ok;
}

// ConflictAnalysis — the atomic learn+backjump rule (docs/calculus.md §3 M3).
// One transition from a recorded conflict: derive and emit the learned clause,
// then, unless that was a root refutation, assert it by backjumping. Fusing
// the two removes the `pending` control state that was observable at a rule
// boundary, so no guard needs a ¬pending.valid conjunct. The step functions
// stay internal, driven white-box by DefaultPolicy and the analyze tests.
static ApplyResult conflictanalysis_apply(Kernel &k) {
  const ApplyResult r = analyze_apply(k);
  if (r != ApplyResult::Ok)
    return r; // root refutation (Unsat) — analyze set no pending clause
  if (!k.pending.valid)
    return ApplyResult::Ok; // conflict reused as its own driver: the implied
                            // literal is already assigned
  return backjump_apply(k); // asserts the UIP at jump_level; clears pending
}

// Instance surface (D-LEVEL-2): the backjump target, any level in
// [jump_level, level-1]. jump_level is unknown until analyze runs, so
// instances() offers every level 0..level-1 and apply_instance clamps
// afterwards. INSTANCE_CANONICAL = jump_level.
static uint32_t conflictanalysis_instances(Kernel &k, uint64_t *out,
                                           uint32_t cap) {
  const uint32_t n = std::min<uint32_t>(cap, k.level); // levels 0..level-1
  for (uint32_t i = 0; i < n; i++)
    out[i] = i;
  return n;
}

static ApplyResult conflictanalysis_apply_instance(Kernel &k, uint64_t inst) {
  const ApplyResult r = analyze_apply(k);
  if (r != ApplyResult::Ok)
    return r; // root refutation
  if (!k.pending.valid)
    return ApplyResult::Ok; // conflict reused as its own driver: no clause to
                            // assert and no target to honour. Must stay in
                            // step with the canonical path above; the random
                            // policy drives only this one.
  if (inst == INSTANCE_CANONICAL)
    return backjump_apply(k); // jump_level
  const uint32_t target =
      inst > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(inst);
  return backjump_apply_to(k, target); // clamps into [jump_level, level-1]
}

TACTIX_REGISTER_RULE(ConflictAnalysis, RuleTier::Micro, analyze_guard,
                     conflictanalysis_apply, conflictanalysis_instances,
                     conflictanalysis_apply_instance);

} // namespace tactix
