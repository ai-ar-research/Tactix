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

#include "proof/proof.hpp"

#include <algorithm>

namespace tactix {

// bit flags in Kernel::seen during analysis
static constexpr uint8_t SEEN = 1;      // literal is in the learned clause
static constexpr uint8_t REMOVABLE = 2; // proven redundant
static constexpr uint8_t POISONED = 4;  // proven not redundant
static constexpr uint8_t SHRINKABLE = 8; // in the current block-shrink walk

// glue (LBD): distinct decision levels among lits[0..n). Uses the
// marked_levels scratch and leaves it clean.
static uint32_t count_glue(Kernel &k, const Lit *lits, uint32_t n) {
  uint32_t glue = 0;
  for (uint32_t i = 0; i < n; i++) {
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
static bool literal_redundant(Kernel &k, Lit l, unsigned depth) {
  const Var v = var_of(l);
  const Assigned &a = k.assigned[v];
  if (k.seen[v] & REMOVABLE)
    return true;
  if (k.seen[v] & POISONED)
    return false;
  if (a.reason == DECISION_REASON || depth > 900 ||
      !k.marked_levels[a.level]) {
    k.seen[v] |= POISONED;
    if (k.seen[v] == POISONED) // no other flag: newly touched
      k.analyzed.push_back(v); // ensure cleanup
    return false;
  }
  const Lit assigned_lit = negate(l); // l is false; the var's true literal
  bool ok = true;
  if (a.binary) {
    const Lit m = a.reason;
    const Var mv = var_of(m);
    if (k.var_level(mv) != 0 && !(k.seen[mv] & SEEN) &&
        !literal_redundant(k, m, depth + 1))
      ok = false;
  } else {
    const Clause *reason = clause_at(k.arena, a.reason);
    for (const Lit m : *reason) {
      if (m == assigned_lit)
        continue;
      const Var mv = var_of(m);
      if (k.var_level(mv) == 0 || (k.seen[mv] & SEEN))
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
      return k.links[x].stamp < k.links[y].stamp;
    });
    return;
  }
  std::vector<Var> &b = k.radix_scratch;
  b.resize(n);
  std::vector<Var> *src = &a, *dst = &b;
  for (uint32_t shift = 0; shift < 32; shift += 8) {
    uint32_t count[257] = {0};
    const uint32_t first = (k.links[(*src)[0]].stamp >> shift) & 0xff;
    bool constant = true;
    for (const Var v : *src) {
      const uint32_t byte = (k.links[v].stamp >> shift) & 0xff;
      constant &= (byte == first);
      count[byte + 1]++;
    }
    if (constant) // all keys share this byte: pass is a no-op
      continue;
    for (uint32_t i = 1; i < 257; i++)
      count[i] += count[i - 1];
    for (const Var v : *src)
      (*dst)[count[(k.links[v].stamp >> shift) & 0xff]++] = v;
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
  if (k.stable) {
    for (const Var v : k.analyzed) {
      if (k.seen[v] & SEEN)
        kernel_score_bump(k, v);
      k.seen[v] = 0;
    }
    k.analyzed.clear();
    kernel_scores_decay(k);
    return;
  }
  radix_sort_analyzed_by_stamp(k);
  for (const Var v : k.analyzed) {
    if (k.seen[v] & SEEN)
      kernel_queue_bump(k, v);
    k.seen[v] = 0;
  }
  k.analyzed.clear();
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
                            Ref subsumed_ref /* INVALID_REF if none */) {
  const Clause *reason = clause_at(k.arena, reason_ref);
  std::vector<Lit> &lits = k.otfs_scratch; // no per-event alloc
  lits.clear();
  lits.reserve(reason->size - 1);
  for (const Lit l : *reason)
    if (l != t && k.var_level(var_of(l)) > 0)
      lits.push_back(l);
  assert(lits.size() >= 2);

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

  if (k.proof)
    k.proof->add_rup(lits.data(), static_cast<uint32_t>(lits.size()));
  k.stats.otfs_strengthened++;

  if (lits.size() == 2) {
    kernel_add_binary_clause(k, lits[0], lits[1]);
    k.conflict_ref = INVALID_REF;
    k.conflict_bin[0] = lits[0];
    k.conflict_bin[1] = lits[1];
  } else {
    // watch the two highest-trail literals; glue = distinct levels
    for (uint32_t w = 0; w < 2; w++) {
      uint32_t best = w;
      for (uint32_t j = w + 1; j < lits.size(); j++)
        if (k.assigned[var_of(lits[j])].trail >
            k.assigned[var_of(lits[best])].trail)
          best = j;
      std::swap(lits[w], lits[best]);
    }
    const uint32_t glue =
        count_glue(k, lits.data(), static_cast<uint32_t>(lits.size()));
    // reason may dangle after this allocation; it is not used again
    k.conflict_ref = kernel_add_large_clause(
        k, lits.data(), static_cast<uint32_t>(lits.size()), redundant, glue);
  }

  if (subsume) {
    Clause *d = clause_at(k.arena, subsumed_ref); // re-deref: arena moved
    if (k.proof)
      k.proof->del(d->lits, d->size);
    d->flags |= Clause::GARBAGE;
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
  if (a.level == 0)
    return 0;
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
  k.seen[uv] |= SEEN; // new clause literal: bump + minimization anchor
  // walked literals resolve to ¬uip, now in the clause — mark removable so
  // lower blocks' chains may bottom out in them (kissat
  // mark_shrinkable_as_removable)
  for (const Var v : k.shrinkable) {
    k.seen[v] &= static_cast<uint8_t>(~SHRINKABLE);
    k.seen[v] |= REMOVABLE;
  }
  k.shrinkable.clear();
  return true;
}

ApplyResult analyze_apply(Kernel &k) {
  assert(analyze_guard(k));
  std::vector<Lit> &learned = k.learned_scratch; // no per-conflict alloc
  Lit uip = INVALID_LIT;
  for (;;) { // one round per conflict; OTFS restarts with a stronger one
    uint32_t csize;
    const Lit *clits = conflict_lits(k, &csize);
    if (k.conflict_ref != INVALID_REF) { // conflict clause is "used" too
      Clause *cc = clause_at(k.arena, k.conflict_ref);
      if (cc->redundant())
        cc->set_used(Clause::MAX_USED);
    }

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
      if (k.proof)
        k.proof->add_empty();
      return ApplyResult::Unsat;
    }
    const Ref conflict_ref0 = k.conflict_ref; // may be garbage'd by OTFS

    learned.clear();
    learned.push_back(INVALID_LIT); // slot 0: the asserting literal (UIP)
    uint32_t open = 0;           // marked vars at the analysis level
    uint32_t resolvent_size = 0; // marked, not yet resolved-away vars

    auto mark = [&](Lit l) {
      const Var v = var_of(l);
      if (k.seen[v])
        return;
      const uint32_t lv = k.var_level(v);
      if (lv == 0)
        return; // root-assigned literals resolve away for free
      k.seen[v] = 1;
      k.analyzed.push_back(v);
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
      do {
        t = k.trail[--i];
      } while (!k.seen[var_of(t)]);
      if (open == 1) {
        uip = t;
        break;
      }
      open--;
      resolved++;
      const Assigned &a = k.assigned[var_of(t)];
      if (a.binary) {
        mark(a.reason); // the other literal of the binary reason clause
        resolvent_size--;
      } else {
        assert(a.reason != DECISION_REASON && a.reason != UNIT_REASON);
        Clause *reason = clause_at(k.arena, a.reason);
        // mark the reason recently-used (kissat deduce.c
        // mark_clause_as_used) — Reduce protects active clauses.
        // Deviation (recorded): kissat also recomputes+promotes the glue
        // here (recompute_and_promote); retention lands first, promotion
        // is a follow-up if the evidence asks for it.
        if (reason->redundant())
          reason->set_used(Clause::MAX_USED);
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
                          subsumes_conflict ? conflict_ref0 : INVALID_REF);
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
    // via the frames; std::sort once per conflict at clause size is fine)
    std::sort(learned.begin() + 1, learned.end(), [&](Lit a, Lit b) {
      return k.var_level(var_of(a)) > k.var_level(var_of(b));
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
  if (k.proof)
    k.proof->add_rup(learned.data(),
                     static_cast<uint32_t>(learned.size()));
  k.stats.learned++;

  PendingLearned &p = k.pending;
  p.lits = learned;
  p.ref = INVALID_REF;
  p.glue = glue;
  p.jump_level = jump;
  if (learned.size() >= 3) {
    // watch invariant after backjump: lits[0] asserting, lits[1] at the
    // jump level (the highest remaining false literal)
    std::swap(p.lits[1], p.lits[high_idx]);
    p.ref = kernel_add_large_clause(k, p.lits.data(),
                                    static_cast<uint32_t>(p.lits.size()),
                                    true, glue);
  } else if (learned.size() == 2) {
    kernel_add_binary_clause(k, p.lits[0], p.lits[1]);
  } // units join the trail in Backjump; no stored clause

  bump_and_reset_analyzed(k);

  k.conflict_valid = false;
  p.valid = true;
  return ApplyResult::Ok;
}

TACTIX_REGISTER_RULE(Analyze, RuleTier::Micro, analyze_guard,
                     analyze_apply);

} // namespace tactix
