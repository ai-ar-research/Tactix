// 1-UIP scheme after kissat/src/analyze.c + deduce.c, with recursive
// clause minimization after kissat/src/minimize.c (Sörensson–Biere
// self-subsuming minimization), all-UIP block shrinking after
// kissat/src/shrink.c (Fleury–Biere, SAT'20), and on-the-fly subsumption
// after kissat/src/deduce.c + strengthen.c — all MIT, transliterated.
// Reference survey (plan §5 rev 14): kissat-sc2024 and AE-Kissat-MAB
// (SC25 winner) ship these files byte-identical to kissat 4.0.4;
// CaDiCaL-SC2025 implements the same pair independently — 4.0.4 is the
// state of the art here. Deliberate deviations: (1) the analysis runs at
// the *maximum level of the conflict literals*, not blindly at the current
// decision level — under exotic-but-legal rule orders (N1: e.g. several
// Decides between propagations) a conflict may live entirely below the
// current level, and soundness may not depend on the schedule;
// (2) singleton-level literals still get minimized (see the block pass).
#include "rules/analyze/analyze.hpp"
#include "policies/heuristics.hpp"

#include "proof/proof.hpp"
#include "rules/backjump/backjump.hpp" // backjump_apply: the fused rule's 2nd step

#include <algorithm>
#include <cstdlib>

namespace tactix {

// bit flags in Kernel::seen during analysis
static constexpr uint8_t SEEN = 1;      // literal is in the learned clause
static constexpr uint8_t REMOVABLE = 2; // proven redundant
static constexpr uint8_t POISONED = 4;  // proven not redundant
static constexpr uint8_t SHRINKABLE = 8; // in the current block-shrink walk
static constexpr uint8_t CHAINED = 16;   // antecedent recorded (LRAT)
static constexpr uint8_t REASON_BUMP = 32; // marked by reason-side bumping

// the BUMP set = SEEN (resolution-visited / in the clause) ∪ REASON_BUMP.
// kissat keeps exactly this set on its `analyzed` stack and bumps all of it;
// our k.analyzed is wider (minimization scratch + LRAT antecedents), which is
// why the predicate is a flag test and the size is counted separately.
static constexpr uint8_t BUMPED = SEEN | REASON_BUMP;

// --- native LRAT antecedent capture (plan O4; CaDiCaL's id scheme with a
// simpler ordering: entries carry the trail position of the literal their
// antecedent propagated; the emitted chain is sorted ascending with the
// conflict last, which unit-propagates for the checker by construction).
// All capture is gated on LRAT mode — DRAT/proofs-off pay one branch.

static bool lrat_active(const Kernel &k) {
  return k.proof && k.proof->lrat();
}

// record var v's REASON as an antecedent (v resolved away or proven
// redundant: the checker re-propagates it from the chain)
static void chain_record(Kernel &k, Var v) {
  if (k.seen[v] & CHAINED)
    return;
  const Assigned &a = k.assigned[v];
  assert(a.reason != DECISION_REASON && a.reason != UNIT_REASON);
  uint64_t id;
  if (a.binary) {
    const Lit assigned_lit = make_lit(v, k.values[make_lit(v, true)] > 0);
    id = kernel_binary_id(k, assigned_lit, a.reason);
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

// emit order: kernel_lrat_chain (ascending trail position, conflict last)
static void chain_emit(Kernel &k, uint64_t id, const Lit *lits,
                       uint32_t size, uint64_t conflict_id) {
  kernel_lrat_chain(k, conflict_id);
  k.proof->add_rup(id, lits, size, k.lrat_chain.data(),
                   static_cast<uint32_t>(k.lrat_chain.size()));
}

// glue (LBD): distinct decision levels among lits[0..n), capped at
// `limit` (kissat promote.h kissat_recompute_glue's early exit — at the
// cap no improvement is possible). Uses the marked_levels scratch and
// leaves it clean.
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

// mark a redundant clause recently-used (Reduce protects it) and
// recompute+promote its glue over the current levels — every literal is
// assigned at both call sites (conflict: all false; reason: all assigned)
// (kissat deduce.c mark_clause_as_used + recompute_and_promote)
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
  // per-mode histogram of the glues that ACTUALLY get used in analysis, with
  // the post-promotion glue (kissat deduce.c:20-22). Reduce's dynamic tier
  // limits are quantiles of this distribution; the counters are free when
  // nothing reads them.
  const uint32_t mode = (k.h && k.h->stable) ? 1u : 0u;
  k.stats.used_glue[mode][std::min<uint32_t>(c->glue(), Stats::MAX_GLUE_USED)]++;
}

bool analyze_guard(const Kernel &k) {
  return k.conflict_valid && !k.inconsistent;
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
// bottoms out in root assignments or literals already in the clause.
// Level filter: a chain can only bottom out in the clause if it stays on
// levels the clause touches (marked_levels).
//
// Recursion cap: kissat's minimizedepth option defaults to 1000 (poisoned
// at depth >= 1000); ours has been 900 with `>` since phase 1 — an
// unrecorded near-miss found in the 2026-07-15 §5.5 sweep. The VALUE is
// trajectory-affecting, so aligning it is a measured fidelity question
// (PROGRESS phase-3 deferred items), not a simplification.
static unsigned minimize_depth_limit() {
  // Probe knob (2026-07-28): the clause-quality probe found minimize+shrink
  // removing only 27.2 % of resolved literals on FmlaEquiv against ~38 % on
  // 9vliw/x9, and this cap is a standing deferred item (900 with `>` vs
  // kissat's minimizedepth 1000). Env-gated so the question is measurable
  // rather than argued.
  static const unsigned v = [] {
    if (const char *e = std::getenv("TACTIX_MINIMIZE_DEPTH"))
      return static_cast<unsigned>(std::strtoul(e, nullptr, 10));
    return 900u;
  }();
  return v;
}

static bool literal_redundant(Kernel &k, Lit l, unsigned depth) {
  const Var v = var_of(l);
  const Assigned &a = k.assigned[v];
  if (k.seen[v] & REMOVABLE)
    return true;
  if (k.seen[v] & POISONED)
    return false;
  if (a.reason == DECISION_REASON || depth > minimize_depth_limit() ||
      !k.marked_levels[a.level]) {
    const uint8_t before = k.seen[v];
    k.seen[v] |= POISONED;
    if (!before) // no other flag: newly touched
      k.analyzed.push_back(v); // ensure cleanup
    return false;
  }
  const Lit assigned_lit = negate(l); // l is false; the var's true literal
  bool ok = true;
  if (a.binary) {
    const Lit m = a.reason;
    const Var mv = var_of(m);
    if (k.var_level(mv) == 0) {
      if (lrat_active(k))
        chain_record_root(k, mv);
    } else if (!(k.seen[mv] & SEEN) && !literal_redundant(k, m, depth + 1))
      ok = false;
  } else {
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

// LSD radix sort of analyzed by VMTF stamp, ascending (kissat sort.h
// RADIX_STACK idea): on industrial instances the analyzed sets are large
// and the former std::sort was ~11% of all samples. Stamps are unique, so
// the order is identical to the comparison sort (trajectory unchanged).
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

// bump analysis-side vars (SEEN); clear all flags incl. minimization
// scratch. Focused: VMTF move-to-front, sorted by stamp to preserve
// relative recency (kissat bump.c). Stable: EVSIDS score bump + one
// increment decay per conflict — order-independent, so no sort.
static void bump_and_reset_analyzed(Kernel &k) {
  k.analyzed_seen = 0;
  k.reason_bumped.clear();
  if (!k.h) {
    // Unguided (no `H`, H5): no scores/queue to bump, but the analysis scratch
    // (SEEN flags + `analyzed`) is kernel state and must still be cleared.
    for (const Var v : k.analyzed)
      k.seen[v] = 0;
    k.analyzed.clear();
    return;
  }
  // Task #11 increment 3: attribute each bump to its channel and to whether it
  // landed on an extension variable (see Stats::bumps_extension).
  const Var ext_first = k.first_extension_var;
  auto count_bump = [&](Var v) {
    k.stats.bumps++;
    if (v >= ext_first) {
      k.stats.bumps_extension++;
      if (!(k.seen[v] & SEEN))
        k.stats.bumps_ext_reason_only++;
    }
  };
  if (k.h->stable) {
    for (const Var v : k.analyzed) {
      if (k.seen[v] & BUMPED) {
        count_bump(v);
        heuristics_score_bump(k, v);
      }
      k.seen[v] = 0;
    }
    k.analyzed.clear();
    heuristics_scores_decay(k);
    return;
  }
  radix_sort_analyzed_by_stamp(k);
  for (const Var v : k.analyzed) {
    if (k.seen[v] & BUMPED) {
      count_bump(v);
      heuristics_queue_bump(k, v);
    }
    k.seen[v] = 0;
  }
  k.analyzed.clear();
}

// --- reason-side bumping (kissat analyze.c analyze_reason_side_literals;
// cadical analyze.cpp bump_also_all_reason_literals; MapleCOMSPS 2016) ------
// After the learned clause is final, ALSO bump the literals of the reasons of
// the clause's own literals — one level deep, capped at
// `k.bump_reason_limit × |bump set|` new marks, all-or-nothing (over the cap
// the whole round rolls back, so the bump set is never a truncation artefact
// of clause order). `H`-only: it changes which variables get a VMTF
// move-to-front / EVSIDS increment, nothing else.
static void mark_reason_side(Kernel &k, Lit m) {
  const Var v = var_of(m);
  if (k.var_level(v) == 0)
    return; // root-fixed: kissat `a->level`
  // Task #11 arm: withhold extension vars from THIS channel only (see
  // Kernel::no_ext_reason_bump). The conflict-side channel still bumps them,
  // so they rise on evidence rather than on being a BVA hub.
  if (k.no_ext_reason_bump && v >= k.first_extension_var)
    return;
  if (k.seen[v] & BUMPED)
    return; // already in the bump set (kissat `a->analyzed`)
  // NB: NOT `if (k.seen[v])` — a var may sit in k.analyzed carrying only
  // REMOVABLE/POISONED/CHAINED. kissat keeps those on separate stacks, so its
  // `!a->analyzed` test admits them; testing the whole byte would silently
  // bump a much smaller set than the reference.
  if (!k.seen[v])
    k.analyzed.push_back(v);
  k.seen[v] |= REASON_BUMP;
  k.reason_bumped.push_back(v);
  k.analyzed_seen++;
  k.stats.reason_bumped++;
}

static void bump_reason_side(Kernel &k, const std::vector<Lit> &learned) {
  const uint32_t saved = k.analyzed_seen;
  const uint64_t limit =
      static_cast<uint64_t>(k.bump_reason_limit) * saved;
  k.reason_bumped.clear();
  k.stats.reason_bump_rounds++;
  for (const Lit l : learned) {
    const Var v = var_of(l);
    const Assigned &a = k.assigned[v];
    if (a.reason == DECISION_REASON || a.reason == UNIT_REASON)
      continue; // no reason side (kissat returns on DECISION_REASON)
    if (a.binary) {
      mark_reason_side(k, a.reason);
    } else {
      k.stats.ticks++; // kissat charges one search tick per reason deref
      const Clause *r = clause_at(k.arena, a.reason);
      const Lit self = negate(l); // `l` is false in the clause; the var's
                                  // true literal is the one `r` propagated
      for (const Lit m : *r) {
        if (m == self)
          continue;
        mark_reason_side(k, m);
        if (k.analyzed_seen > limit)
          break;
      }
    }
    if (k.analyzed_seen > limit)
      break;
  }
  if (k.analyzed_seen > limit) { // over budget: undo the whole round
    for (const Var v : k.reason_bumped)
      k.seen[v] &= static_cast<uint8_t>(~REASON_BUMP);
    k.stats.reason_bumped -= k.reason_bumped.size();
    k.analyzed_seen = saved;
    k.stats.reason_bump_aborted++;
    // vars newly pushed to k.analyzed stay there with seen == 0 — harmless,
    // bump_and_reset_analyzed skips them and clears unconditionally. (kissat
    // pops its stack; ours is not a suffix, since a reason-side var may have
    // been in k.analyzed already as minimization scratch.)
  }
  k.reason_bumped.clear();
}

// --- on-the-fly subsumption (kissat/src/deduce.c + strengthen.c) ----------
// When, after resolving trail literal t's reason, the resolvent is exactly
// reason ∖ {t} (all non-root reason literals and nothing else), the
// resolvent subsumes the reason: materialize it as a stronger clause and
// restart the analysis with it as the conflict. If that happened on the
// very first resolution and the resolvent is also smaller than the
// conflict, the conflict is subsumed too and is deleted.
//
// Deviation from kissat (plan §5 rev 14): kissat strengthens the reason
// clause IN PLACE, which (a) transiently leaves t's reason not containing
// t and (b) requires shrunken-clause padding in every arena walk (GC,
// Reduce, the invariant checker). Our N1 contract returns control to the
// policy between rules, so such states would be visible to arbitrary legal
// schedules — instead we allocate the strengthened clause fresh and leave
// the original untouched as t's reason (Reduce reclaims it later if
// redundant). The subsumed CONFLICT clause, which is never a reason (a
// clause that propagated a literal has that literal true, so it cannot be
// all-false), IS deleted immediately; its watches keep the garbage ref
// until the next collection, which stays sound and proof-safe because the
// strictly stronger resolvent is added before the delete.
static void otfs_strengthen(Kernel &k, Ref reason_ref, Lit t,
                            Ref subsumed_ref /* INVALID_REF if none */,
                            uint64_t conflict_chain_id) {
  const Clause *reason = clause_at(k.arena, reason_ref);
  std::vector<Lit> &lits = k.otfs_scratch; // no per-event alloc
  lits.clear();
  lits.reserve(reason->size - 1);
  for (const Lit l : *reason)
    if (l != t && k.var_level(var_of(l)) > 0)
      lits.push_back(l);
  assert(lits.size() >= 2);
  const uint64_t id = ++k.next_clause_id;

  bool subsume = false, subsumed_irredundant = false;
  if (subsumed_ref != INVALID_REF) {
    const Clause *d = clause_at(k.arena, subsumed_ref);
    assert(!d->garbage());
    subsume = true;
    subsumed_irredundant = !d->redundant();
  }
  // the resolvent keeps the reason's redundancy; if it replaces an
  // irredundant conflict it must itself become irredundant (kissat
  // otfs_subsume_clause)
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
    // watch the two highest-LEVEL literals (ties by trail); glue = distinct
    // levels. Lexicographic (level, trail) (rev 38, D-LEVEL-2): a persistent
    // strengthened clause must be watched on its two highest-LEVEL literals or
    // a compacting backtrack can strand it unit with no watch firing. On a
    // level-sorted trail higher level <=> higher trail, so this reduces
    // exactly to the old highest-trail selection. That equivalence held while
    // chrono was off; with chrono ON by default (2026-07-22) the trail is not
    // level-sorted and the level-based selection is the one that matters.
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
    const uint32_t glue =
        count_glue(k, lits.data(), static_cast<uint32_t>(lits.size()));
    // reason may dangle after this allocation; it is not used again
    k.conflict_ref = kernel_add_large_clause(
        k, lits.data(), static_cast<uint32_t>(lits.size()), redundant, glue,
        id);
  }

  if (subsume) {
    Clause *d = clause_at(k.arena, subsumed_ref); // re-deref: arena moved
    if (k.proof)
      k.proof->del(k.next_clause_id, d->id(), d->lits, d->size);
    kernel_mark_garbage(k, d);
    k.stats.otfs_subsumed++;
  }
}

// --- all-UIP block shrinking (kissat/src/shrink.c; Fleury–Biere SAT'20) ---
// A maximal group of learned literals on one decision level (a "block") is
// replaced by the negation of that level's local UIP when the block's
// reason chains, walked down the trail, converge to a single literal.

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
    if (k.seen[v] & REMOVABLE)
      return 0;
    // kissat (option shrink > 2, the default) falls back to recursive
    // minimization for lower-level literals
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
  // walked literals resolve to ¬uip, now in the clause — mark removable so
  // lower blocks' chains may bottom out in them (kissat
  // mark_shrinkable_as_removable)
  const bool lrat = lrat_active(k);
  for (const Var v : k.shrinkable) {
    k.seen[v] &= static_cast<uint8_t>(~SHRINKABLE);
    k.seen[v] |= REMOVABLE;
    // the walked literals' reasons justify the collapse for the checker;
    // the UIP itself stays out of the chain (its literal joins the
    // clause, so its reason would be a satisfied — illegal — hint)
    if (lrat && v != uv)
      chain_record(k, v);
  }
  k.shrinkable.clear();
  return true;
}

// --- watch repair for the conflict-reuse shortcut (TB4b) -------------------
// Re-point the conflict clause's two WATCHES at its two highest-LEVEL literals,
// transferring the watch entries as it goes (kissat analyze.c, the
// `conflict_size > 2` block; two passes, each promoting the highest remaining
// literal into watched slot i). Returns the number of watch entries actually
// moved — 0 when the clause already had them in the right place, which is the
// shape the crippled version was restricted to.
//
// WHY THIS IS THE INVARIANT THE SHORTCUT NEEDS. After the shortcut backtracks
// to `jump2` and assigns `forced` with this clause as its reason, every other
// literal is false at some level <= jump2 and at least one is at exactly jump2.
// The two-watched-literal scheme survives backtracking because a FALSE watched
// literal at level d is always paired with a watch that is TRUE at level <= d:
// unassigning can then only ever remove the false watch first. Here `forced` is
// the only true literal (so it must be one watch, which is also the reason-
// clause requirement), and its level is jump2 — so the other watch must be a
// literal at exactly jump2, not merely <= it. Otherwise a backtrack below jump2
// unassigns `forced` while the other watch stays false, and nothing rescans the
// clause: it sits unit at a claimed fixpoint. That is exactly what aborted
// regression corpus 002 and 005 on the first version of this shortcut ("unit
// large clause escaped propagation"), and it is what the crippled version
// avoided by DECLINING instead of repairing.
//
// FITS-OUR-ARCHITECTURE (plan §5). The level ranking is analysis logic and
// stays in this rule; the watch transfer is a kernel primitive
// (kernel_promote_watched_lit) because it edits the watch arena, whose
// relocation hazards only the kernel knows about. Nothing here is scheduling:
// the repair fires exactly when the shortcut fires, so it changes no state on
// any path the shortcut declines. Legal at any N1 schedule point that can reach
// conflict analysis — it needs only a recorded conflict and no in-flight scan,
// both guaranteed by analyze_guard.
//
// DEVIATION from kissat, deliberately NARROWER: kissat runs this repair for
// EVERY conflict of size > 2, before it even knows whether the shortcut
// applies, so its non-shortcut analysis also starts from a re-sorted clause. We
// run it only when the shortcut is about to fire, which keeps the ordinary
// learning path bit-identical to before this change. Widening it is a separate
// TB4b divergence with its own measurement.
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

ApplyResult analyze_apply(Kernel &k) {
  assert(analyze_guard(k));
  std::vector<Lit> &learned = k.learned_scratch; // no per-conflict alloc
  const bool lrat = lrat_active(k);
  uint64_t conflict_chain_id = 0; // the round's conflict, emitted last
  Lit uip = INVALID_LIT;
  bool first_round = true; // TB4b(A) fires only here — see below
  for (;;) { // one round per conflict; OTFS restarts with a stronger one
    uint32_t csize;
    const Lit *clits = conflict_lits(k, &csize);
    if (lrat) {
      k.lrat_entries.clear();
      conflict_chain_id =
          k.conflict_ref == INVALID_REF
              ? kernel_binary_id(k, k.conflict_bin[0], k.conflict_bin[1])
              : clause_at(k.arena, k.conflict_ref)->id();
    }
    if (k.conflict_ref != INVALID_REF) // conflict clause is "used" too
      mark_used(k, clause_at(k.arena, k.conflict_ref));

    // analysis level = max level among conflict literals
    uint32_t alevel = 0;
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
    // CONFLICT-LEVEL BACKTRACK (kissat analyze.c one_literal_on_conflict_level:
    // `if (conflict_level < solver->level) backtrack_after_conflict(...)`).
    // Under chronological backtracking the trail is no longer level-sorted, so
    // the conflict's highest literal level `alevel` can sit BELOW the current
    // decision level. Everything downstream is defined relative to the level the
    // conflict actually lives at: the backjump gap `back - jump` that decides
    // chronological-vs-normal, the 1-UIP walk's starting point, and the
    // asserting literal's staleness test. Analysing from a higher level made all
    // three wrong at once — the gap was overstated so the chronological branch
    // fired far too often; the target could land at or above `alevel`, where the
    // conflict is STILL falsified; and `backjump_apply_to` then found the
    // asserting literal still assigned, declared the pending clause stale and
    // returned NoOp, so the conflict was silently dropped and re-derived one
    // level at a time — duplicate learned clauses, duplicate proof lines and
    // bogus glue samples on every repeat. Measured cost: Tier B +13 % and a
    // 7-win/7-lose bimodality that tracked trail depth exactly.
    // No-op when chrono is off: complete propagation on a level-sorted trail
    // always puts a conflict literal at the current level, so alevel == k.level.
    if (alevel < k.level)
      kernel_backtrack(k, alevel);

    // TB4b(A) · REUSE THE CONFLICT AS THE DRIVING CLAUSE (kissat analyze.c
    // `one_literal_on_conflict_level`, the `literals_on_conflict_level == 1`
    // tail). When exactly ONE conflict literal sits at the conflict level, the
    // conflict clause is ALREADY an asserting clause: backtrack to the
    // second-highest level `jump2` and every other literal stays false while
    // that one becomes unassigned, so the clause is unit and implies it. kissat
    // then assigns it with the conflict as its reason and learns NOTHING. We
    // learn a fresh clause in that situation, which is why our learned count
    // (and proof size) runs ahead of kissat's on conflict-heavy instances —
    // FmlaEquiv's 1.53x conflicts is the target.
    //
    // Deviation: kissat routes the target through `determine_new_level`
    // (its chrono decision) — ours is the plain non-chronological `jump2`,
    // because the chrono target is POLICY territory here (rev 38: the backjump
    // target is M3's instance surface, chosen in default_policy.cpp) and this
    // path bypasses that surface. Chronological reuse is a later TB4b item.
    // Deviation: `jump2 == 0` falls through to normal learning. The shortcut
    // would assign a ROOT unit, which under LRAT needs a derived-unit line with
    // its own chain; the normal path already learns that unit correctly, so the
    // restriction costs nothing but the proof machinery.
    // Restricted to the FIRST round. An OTFS round's conflict is a clause this
    // analysis just strengthened, and reusing it as a REASON requires it to be
    // a real, watched, id-carrying clause; the original propagation conflict
    // always is. kissat takes the shortcut on OTFS rounds too — porting that
    // needs the OTFS product's clause identity pinned down first, so it is a
    // separate TB4b step rather than a silent widening.
    if (alevel > 0 && first_round && k.conflict_reuse) {
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
      // DIAGNOSTIC ARM (k.conflict_reuse_no_repair): the CRIPPLED decline test,
      // kept only so a tier window can attribute the arm's verdict between the
      // crippling and the mechanism. Sound, and inert unless the knob is set.
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
        // WATCH REPAIR (TB4b, the divergence this shortcut is coupled to).
        // Reusing the conflict as a REASON only works if the clause watches the
        // literal it now implies AND a false literal at exactly `jump2`; see
        // repair_conflict_watches above for why, and for what the first version
        // of this shortcut did to regression corpus 002/005 without it. Until
        // the kernel had an unwatch primitive this code DECLINED whenever the
        // invariant did not already hold (the crippled version whose negative
        // tier verdict is on record); it now repairs instead, so the shortcut
        // fires on every one-literal-on-conflict-level conflict rather than
        // only the subset whose propagation happened to leave the right pair
        // watched.
        if (cref != INVALID_REF && repair_conflict_watches(k, cref, alevel))
          k.stats.conflict_watch_repaired++;
#ifndef NDEBUG
        if (cref != INVALID_REF) { // post-repair: exactly the invariant above
          const Clause *cc = clause_at(k.arena, cref);
          if (cc->size > 2) {
            assert(cc->lits[0] == forced);
            assert(k.var_level(var_of(cc->lits[1])) == jump2);
          }
        }
#endif
        kernel_backtrack(k, jump2);
        if (cref == INVALID_REF)
          kernel_assign(k, forced, jump2, other, /*binary=*/true);
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

    first_round = false;
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
      // resolve only assertion-level literals: `open` counts exactly the seen
      // alevel vars (mark() pushes lower-level seen lits straight to `learned`).
      // The `var_level == alevel` filter (rev 38, D-LEVEL-2) makes this robust
      // to a non-level-sorted trail — a lower-level seen literal can now sit
      // below an alevel one, and must be skipped, not resolved. On a
      // level-sorted trail the alevel block is contiguous at the top and the
      // loop breaks at open==1 before reaching any lower-level literal, so the
      // filter skips nothing and the round is byte-identical.
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
        // recently-used + glue recompute/promote (kissat deduce.c;
        // completes the retention scheme — the deferred-promotion
        // deviation closed 2026-07-14)
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
    bump_and_reset_analyzed(k); // kissat bumps each aborted round too
  }
  learned[0] = negate(uip);

  // shrink + minimize (kissat analyze.c with option shrink=3, the default,
  // where full minimization is subsumed by the block pass): per decision
  // level, first try to collapse the whole block to its local UIP; if that
  // fails, minimize the block's literals individually. Level marks gate
  // literal_redundant's chains throughout.
  if (learned.size() > 2) {
    for (uint32_t j = 1; j < learned.size(); j++) {
      const uint32_t lv = k.var_level(var_of(learned[j]));
      if (!k.marked_levels[lv]) {
        k.marked_levels[lv] = 1;
        k.touched_levels.push_back(lv);
      }
    }
    // group into blocks: sort by level, descending (kissat counting-sorts
    // via the frames; std::sort once per conflict at clause size is fine).
    // The trail tie-break is LOAD-BEARING: with the bare level key,
    // equal-level literals sit in std::sort's implementation-defined order,
    // which differs between libstdc++ and libc++ — and since this order
    // decides the watched literals of every learned clause, the whole search
    // trajectory forked across toolchains (battleship 1.95 M → 27.1 M
    // conflicts). (level desc, trail desc) is a total order (trail positions
    // are unique among assigned vars), allocation-free, and recency-first
    // matches the conventional second-watch choice; the stable/asc
    // alternatives measured equivalent-in-noise on Tier B
    // (`benchmarks/reports/2026-07-24-battleship-divergence.md`).
    std::sort(learned.begin() + 1, learned.end(), [&](Lit a, Lit b) {
      const uint32_t la = k.var_level(var_of(a));
      const uint32_t lb = k.var_level(var_of(b));
      if (la != lb)
        return la > lb;
      return k.assigned[var_of(a)].trail > k.assigned[var_of(b)].trail;
    });
    size_t b = 1;
    while (b < learned.size()) {
      const uint32_t blevel = k.var_level(var_of(learned[b]));
      size_t e = b + 1;
      while (e < learned.size() &&
             k.var_level(var_of(learned[e])) == blevel)
        e++;
      if (e - b >= 2 && shrink_block(k, learned, b, e, blevel)) {
        k.stats.shrunken += (e - b) - 1;
      } else {
        // minimize the block literal by literal — including singleton
        // blocks: kissat skips those ("singleton frame" shortcut), but
        // under N1's exotic schedules a literal's chain can bottom out
        // without another literal on its own level, so they are removable
        // more often for us
        for (size_t j = b; j < e; j++)
          if (literal_redundant(k, learned[j], 0)) {
            learned[j] = INVALID_LIT;
            k.stats.minimized++;
          }
      }
      b = e;
    }
    size_t kept = 1;
    for (size_t j = 1; j < learned.size(); j++)
      if (learned[j] != INVALID_LIT)
        learned[kept++] = learned[j];
    learned.resize(kept);
    for (const uint32_t lv : k.touched_levels)
      k.marked_levels[lv] = 0;
    k.touched_levels.clear();
  }

  // reason-side bumping, on the FINAL clause and before the bump (kissat
  // analyze.c:566, between shrink/minimize and learn). Off unless a policy
  // set the limit; never on the OTFS restart path (kissat skips it there too)
  // nor on the root-refutation exit above.
  if (k.bump_reason_limit)
    bump_reason_side(k, learned);

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

  bump_and_reset_analyzed(k);

  k.conflict_valid = false;
  p.valid = true;
  return ApplyResult::Ok;
}

// ConflictAnalysis (rev 37) — the atomic learn+backjump rule (calculus §8,
// D-SPLIT). One transition from a recorded conflict: derive+emit the learned
// clause (analyze_apply) and, unless that was a root refutation, assert it by
// backjumping (backjump_apply). Fusing the two removes the `pending` control
// state that was observable at a rule boundary — so no guard needs
// `¬pending.valid` any more, and the corpus-006 (conflict_valid ∧ pending.valid)
// state is unrepresentable rather than merely guarded against. The guard is the
// already-pure analyze_guard. analyze_apply / backjump_apply stay as internal
// step functions (DefaultPolicy and the analyze tests drive them white-box).
static ApplyResult conflictanalysis_apply(Kernel &k) {
  const ApplyResult r = analyze_apply(k);
  if (r != ApplyResult::Ok)
    return r; // root refutation (Unsat) — analyze set no pending clause
  if (!k.pending.valid)
    return ApplyResult::Ok; // TB4b(A): conflict reused as its own driver —
                            // the implied literal is already assigned
  return backjump_apply(k); // asserts the UIP at jump_level; clears pending
}

// instance surface (rev 38, D-LEVEL-2): the backjump TARGET. Since true-level
// assignment makes any level in [jump_level, level-1] a sound target, a policy
// may pick a chronological one. jump_level is not known until analyze runs, so
// instances() offers every level below the conflict level (0..level-1) and
// apply_instance clamps into [jump_level, level-1] after analysis.
// INSTANCE_CANONICAL = jump_level (the non-chronological backjump).
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
    return ApplyResult::Ok; // TB4b(A): conflict reused as its own driver, so
                            // there is no clause to assert and no target to
                            // honour — the instance payload is moot. Missing
                            // this is what aborted corpus 002/005: the canonical
                            // path was patched, the INSTANCE path (which the
                            // random policy drives) was not.
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
