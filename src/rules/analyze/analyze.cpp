// 1-UIP scheme after kissat/src/analyze.c + deduce.c, with recursive
// clause minimization after kissat/src/minimize.c (MIT; the algorithm is
// Sörensson–Biere self-subsuming minimization). One deliberate
// generalization: the analysis runs at the *maximum level of the conflict
// literals*, not blindly at the current decision level — under
// exotic-but-legal rule orders (N1: e.g. several Decides between
// propagations) a conflict may live entirely below the current level, and
// soundness may not depend on the schedule.
#include "rules/analyze/analyze.hpp"

#include "proof/proof.hpp"

#include <algorithm>

namespace tactix {

// bit flags in Kernel::seen during analysis
static constexpr uint8_t SEEN = 1;      // literal is in the learned clause
static constexpr uint8_t REMOVABLE = 2; // proven redundant
static constexpr uint8_t POISONED = 4;  // proven not redundant

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
    if (!k.seen[v] || k.seen[v] == POISONED)
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

ApplyResult analyze_apply(Kernel &k) {
  assert(analyze_guard(k));
  uint32_t csize;
  const Lit *clits = conflict_lits(k, &csize);

  // analysis level = max level among conflict literals
  uint32_t alevel = 0;
  for (uint32_t i = 0; i < csize; i++)
    alevel = std::max(alevel, k.var_level(var_of(clits[i])));
  if (alevel == 0) { // conflict entirely at the root: refutation done
    k.inconsistent = true;
    k.conflict_valid = false;
    if (k.proof)
      k.proof->add_empty();
    return ApplyResult::Unsat;
  }

  std::vector<Lit> learned;
  learned.push_back(INVALID_LIT); // slot 0: the asserting literal (UIP)
  uint32_t open = 0;              // marked vars at the analysis level

  auto mark = [&](Lit l) {
    const Var v = var_of(l);
    if (k.seen[v])
      return;
    const uint32_t lv = k.var_level(v);
    if (lv == 0)
      return; // root-assigned literals resolve away for free
    k.seen[v] = 1;
    k.analyzed.push_back(v);
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
  Lit uip = INVALID_LIT;
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
    const Assigned &a = k.assigned[var_of(t)];
    if (a.binary) {
      mark(a.reason); // the other literal of the binary reason clause
    } else {
      assert(a.reason != DECISION_REASON && a.reason != UNIT_REASON);
      const Clause *reason = clause_at(k.arena, a.reason);
      for (const Lit l : *reason)
        if (l != t)
          mark(l);
    }
  }
  learned[0] = negate(uip);

  // minimization: drop literals whose reason chains bottom out in the
  // clause itself or in root assignments (level filter first)
  if (learned.size() > 2) {
    for (uint32_t j = 1; j < learned.size(); j++) {
      const uint32_t lv = k.var_level(var_of(learned[j]));
      if (!k.marked_levels[lv]) {
        k.marked_levels[lv] = 1;
        k.touched_levels.push_back(lv);
      }
    }
    size_t kept = 1;
    for (size_t j = 1; j < learned.size(); j++)
      if (!literal_redundant(k, learned[j], 0))
        learned[kept++] = learned[j];
    k.stats.minimized += learned.size() - kept;
    learned.resize(kept);
    for (const uint32_t lv : k.touched_levels)
      k.marked_levels[lv] = 0;
    k.touched_levels.clear();
  }

  // glue (LBD) + backjump level from the non-asserting literals
  uint32_t jump = 0, glue = 0;
  uint32_t high_idx = 0; // index of a literal at the jump level
  for (uint32_t j = 1; j < learned.size(); j++) {
    const uint32_t lv = k.var_level(var_of(learned[j]));
    if (!k.marked_levels[lv]) {
      k.marked_levels[lv] = 1;
      k.touched_levels.push_back(lv);
      glue++;
    }
    if (lv > jump) {
      jump = lv;
      high_idx = j;
    }
  }
  for (const uint32_t lv : k.touched_levels)
    k.marked_levels[lv] = 0;
  k.touched_levels.clear();

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

  // bump analysis-side vars (SEEN), preserving relative recency (kissat
  // sorts by stamp); clear all flags incl. minimization scratch
  std::sort(k.analyzed.begin(), k.analyzed.end(),
            [&](Var a, Var b) { return k.links[a].stamp < k.links[b].stamp; });
  for (const Var v : k.analyzed) {
    if (k.seen[v] & SEEN)
      kernel_queue_bump(k, v);
    k.seen[v] = 0;
  }
  k.analyzed.clear();

  k.conflict_valid = false;
  p.valid = true;
  return ApplyResult::Ok;
}

TACTIX_REGISTER_RULE(Analyze, RuleTier::Micro, analyze_guard, analyze_apply);

} // namespace tactix
