// V5 Sweep · S-a: the embedded sub-solver (docs/design/v5-sweep-design.md §3).
//
// Ordinary CDCL, written small on purpose. The pieces that differ from the
// kernel's, and why:
//
//  - Watch lists are a `vector<vector<SubWatch>>`, not a flat arena. The
//    kernel's flat layout exists to keep the search's hot loop cache-dense and
//    pays for it with the delayed-relocation discipline (watch.hpp); an
//    environment here is a few dozen clauses, so the flat layout would buy
//    nothing and the per-list vectors remove the relocation hazard entirely.
//  - Decisions are in fixed variable order with saved phases — no activity
//    heuristic. The design's §3 requirement is determinism and a bounded
//    budget, not search quality: an environment that needs a good heuristic to
//    refute is an environment we should have given up on.
//  - Assumptions are MiniSat-shaped: each becomes its own decision level (an
//    already-true one gets an empty level so `level` keeps indexing the
//    assumption list), and a falsified assumption ends the solve as `Unsat`,
//    i.e. UNSAT *under the assumptions*.
//
// No antecedent recording and no proof emission — that is S-b, and until it
// exists nothing this solver concludes may justify a formula rewrite.
#include "rules/sweep/subsolver.hpp"

#include <algorithm>
#include <cassert>

namespace tactix {

namespace {
constexpr SubLit NO_LIT = UINT32_MAX;
// Restart cadence. Environments are tiny, so this only has to stop the solver
// from burning its whole budget down one bad subtree; arithmetic growth keeps
// it deterministic and needs no state beyond a counter.
constexpr uint64_t RESTART_INTERVAL = 100;
} // namespace

void SubSolver::reset(uint32_t nvars) {
  nvars_ = nvars;
  inconsistent_ = false;
  arena_.clear();
  watches_.assign(2 * static_cast<size_t>(nvars), {});
  values_.assign(2 * static_cast<size_t>(nvars), 0);
  model_.assign(nvars, 1);
  phase_.assign(nvars, 1); // positive initial phase (the project convention)
  trail_.clear();
  frames_.assign(1, 0);
  level_.assign(nvars, 0);
  reason_.assign(nvars, NO_REF);
  binary_reason_.assign(nvars, NO_LIT);
  propagate_head_ = 0;
  conflict_ref_ = NO_REF;
  conflict_is_binary_ = false;
  seen_.assign(nvars, 0);
  analyzed_.clear();
  learned_.clear();
  decision_cursor_ = 0;
  ticks_ = 0;
  budget_ = 0;
  conflicts_ = 0;
}

void SubSolver::attach(SubRef ref, SubLit a, SubLit b) {
  watches_[a].push_back({b, ref});
  watches_[b].push_back({a, ref});
}

void SubSolver::assign(SubLit l, SubRef reason, SubLit binary_reason) {
  assert(values_[l] == 0);
  values_[l] = 1;
  values_[sub_negate(l)] = -1;
  const SubVar v = sub_var_of(l);
  level_[v] = static_cast<uint32_t>(frames_.size()) - 1;
  reason_[v] = reason;
  binary_reason_[v] = binary_reason;
  trail_.push_back(l);
}

void SubSolver::add_clause(const std::vector<SubLit> &lits) {
  if (inconsistent_)
    return;
  // dedup + tautology check, and drop literals already false at level 0
  std::vector<SubLit> c;
  c.reserve(lits.size());
  for (const SubLit l : lits) {
    assert(sub_var_of(l) < nvars_);
    bool dup = false;
    for (const SubLit o : c) {
      if (o == l) {
        dup = true;
        break;
      }
      if (o == sub_negate(l))
        return; // tautology: nothing to add
    }
    if (dup)
      continue;
    if (frames_.size() == 1 && values_[l] > 0)
      return; // already satisfied at the root
    if (frames_.size() == 1 && values_[l] < 0)
      continue; // root-false: shrink
    c.push_back(l);
  }
  if (c.empty()) {
    inconsistent_ = true;
    return;
  }
  if (c.size() == 1) {
    assign(c[0], NO_REF, NO_LIT);
    return;
  }
  if (c.size() == 2) {
    // binaries live in the watch lists only, as in the kernel
    watches_[c[0]].push_back({c[1], NO_REF});
    watches_[c[1]].push_back({c[0], NO_REF});
    return;
  }
  arena_.push_back(static_cast<SubLit>(c.size())); // length prefix
  const SubRef ref = static_cast<SubRef>(arena_.size());
  arena_.insert(arena_.end(), c.begin(), c.end());
  attach(ref, c[0], c[1]);
}

bool SubSolver::propagate() {
  while (propagate_head_ < trail_.size()) {
    const SubLit lit = trail_[propagate_head_++];
    const SubLit falsified = sub_negate(lit);
    std::vector<SubWatch> &ws = watches_[falsified];
    ticks_ += ws.size(); // effort currency: watch entries inspected
    size_t i = 0, j = 0;
    const size_t n = ws.size();
    for (; i < n;) {
      const SubWatch w = ws[i++];
      if (w.ref == NO_REF) { // binary
        ws[j++] = w;
        const int8_t v = values_[w.blocking];
        if (v > 0)
          continue;
        if (v == 0) {
          assign(w.blocking, NO_REF, falsified);
          continue;
        }
        conflict_is_binary_ = true;
        conflict_bin_[0] = falsified;
        conflict_bin_[1] = w.blocking;
        while (i < n)
          ws[j++] = ws[i++];
        ws.resize(j);
        return false;
      }
      if (values_[w.blocking] > 0) { // blocking literal satisfies it
        ws[j++] = w;
        continue;
      }
      SubLit *c = &arena_[w.ref];
      const uint32_t size = arena_[w.ref - 1];
      if (c[0] == falsified)
        std::swap(c[0], c[1]);
      assert(c[1] == falsified);
      if (values_[c[0]] > 0) {
        ws[j++] = {c[0], w.ref};
        continue;
      }
      uint32_t r = 2;
      for (; r < size; r++)
        if (values_[c[r]] >= 0)
          break;
      ticks_ += size;
      if (r < size) { // re-watch: this entry leaves the list
        std::swap(c[1], c[r]);
        assert(c[1] != falsified);
        watches_[c[1]].push_back({c[0], w.ref});
        continue;
      }
      ws[j++] = {c[0], w.ref};
      if (values_[c[0]] < 0) {
        conflict_is_binary_ = false;
        conflict_ref_ = w.ref;
        while (i < n)
          ws[j++] = ws[i++];
        ws.resize(j);
        return false;
      }
      assign(c[0], w.ref, NO_LIT);
    }
    ws.resize(j);
  }
  return true;
}

void SubSolver::backtrack(uint32_t level) {
  // `frames_[i]` is the trail index where level `i` starts, so keeping levels
  // 0..level means truncating to where level+1 began (the kernel's convention;
  // frames_.size() == current level + 1).
  const uint32_t current = static_cast<uint32_t>(frames_.size()) - 1;
  if (level >= current)
    return;
  const uint32_t keep = frames_[level + 1];
  while (trail_.size() > keep) {
    const SubLit l = trail_.back();
    trail_.pop_back();
    const SubVar v = sub_var_of(l);
    phase_[v] = (l & 1u) ? -1 : 1; // save the phase we just gave up
    values_[l] = 0;
    values_[sub_negate(l)] = 0;
    reason_[v] = NO_REF;
    binary_reason_[v] = NO_LIT;
  }
  frames_.resize(level + 1);
  propagate_head_ = keep;
  decision_cursor_ = 0; // fixed order: rescan from the start
}

bool SubSolver::analyze() {
  const uint32_t current = static_cast<uint32_t>(frames_.size()) - 1;
  assert(current > 0);
  learned_.clear();
  learned_.push_back(0); // placeholder for the asserting literal
  for (const SubVar v : analyzed_)
    seen_[v] = 0;
  analyzed_.clear();

  uint32_t count = 0; // seen, unresolved, at the current level
  const auto see = [&](SubLit l) {
    const SubVar v = sub_var_of(l);
    if (seen_[v] || level_[v] == 0)
      return;
    seen_[v] = 1;
    analyzed_.push_back(v);
    if (level_[v] == current)
      count++;
    else
      learned_.push_back(l);
  };
  if (conflict_is_binary_) {
    see(conflict_bin_[0]);
    see(conflict_bin_[1]);
  } else {
    const uint32_t size = clause_size(conflict_ref_);
    const SubLit *c = clause_lits(conflict_ref_);
    for (uint32_t i = 0; i < size; i++)
      see(c[i]);
  }
  // Unreachable with correct two-watch propagation: a conflict whose literals
  // are all below the current level would have fired at that lower level. If it
  // ever happens, the caller must draw NO conclusion rather than report Unsat —
  // reporting a refutation from an unanalyzed conflict is the one way this
  // engine could lie to Sweep.
  assert(count > 0);
  if (count == 0)
    return false;

  size_t idx = trail_.size();
  SubLit uip = 0;
  for (;;) {
    SubVar v;
    do {
      assert(idx > 0);
      v = sub_var_of(trail_[--idx]);
    } while (!seen_[v]);
    count--;
    if (count == 0) {
      uip = trail_[idx];
      break;
    }
    // resolve on v: every other literal of its reason joins the clause
    const SubLit resolved = trail_[idx];
    if (reason_[v] != NO_REF) {
      const uint32_t size = clause_size(reason_[v]);
      const SubLit *c = clause_lits(reason_[v]);
      for (uint32_t i = 0; i < size; i++)
        if (c[i] != resolved)
          see(c[i]);
    } else {
      assert(binary_reason_[v] != NO_LIT); // a decision cannot be resolved on
      see(binary_reason_[v]);
    }
  }
  learned_[0] = sub_negate(uip);

  // second watch = the deepest of the remaining literals (the backjump level)
  uint32_t bj = 0;
  if (learned_.size() > 1) {
    size_t best = 1;
    for (size_t i = 2; i < learned_.size(); i++)
      if (level_[sub_var_of(learned_[i])] > level_[sub_var_of(learned_[best])])
        best = i;
    std::swap(learned_[1], learned_[best]);
    bj = level_[sub_var_of(learned_[1])];
  }
  backtrack(bj);
  if (learned_.size() == 1) {
    assign(learned_[0], NO_REF, NO_LIT);
  } else if (learned_.size() == 2) {
    watches_[learned_[0]].push_back({learned_[1], NO_REF});
    watches_[learned_[1]].push_back({learned_[0], NO_REF});
    assign(learned_[0], NO_REF, learned_[1]);
  } else {
    arena_.push_back(static_cast<SubLit>(learned_.size()));
    const SubRef ref = static_cast<SubRef>(arena_.size());
    arena_.insert(arena_.end(), learned_.begin(), learned_.end());
    attach(ref, learned_[0], learned_[1]);
    assign(learned_[0], ref, NO_LIT);
  }
  return true;
}

SubLit SubSolver::next_decision() {
  while (decision_cursor_ < nvars_) {
    const SubVar v = decision_cursor_;
    if (values_[sub_lit(v, false)] == 0)
      return sub_lit(v, phase_[v] < 0);
    decision_cursor_++;
  }
  return NO_LIT;
}

SubResult SubSolver::solve(const std::vector<SubLit> &assumptions,
                           uint64_t ticks_budget) {
  budget_ = ticks_budget;
  conflicts_ = 0;
  if (inconsistent_)
    return SubResult::Unsat;
  backtrack(0);
  uint64_t next_restart = RESTART_INTERVAL;
  uint64_t restarts = 0;
  for (;;) {
    if (!propagate()) {
      const uint32_t current = static_cast<uint32_t>(frames_.size()) - 1;
      conflicts_++;
      if (current == 0) {
        // level 0 carries no assumptions, so this is a GLOBAL refutation
        inconsistent_ = true;
        return SubResult::Unsat;
      }
      if (!analyze())
        return SubResult::Budget; // see analyze(): no conclusion, not Unsat
      if (budget_ && ticks_ > budget_)
        return SubResult::Budget;
      if (conflicts_ >= next_restart) {
        restarts++;
        next_restart = conflicts_ + RESTART_INTERVAL * (restarts + 1);
        backtrack(0); // the assumption prefix is re-decided below
      }
      continue;
    }
    const uint32_t current = static_cast<uint32_t>(frames_.size()) - 1;
    if (current < assumptions.size()) {
      // next assumption as its own decision level (MiniSat's shape)
      const SubLit a = assumptions[current];
      if (values_[a] < 0)
        return SubResult::Unsat; // refuted under the assumptions
      frames_.push_back(static_cast<uint32_t>(trail_.size()));
      if (values_[a] == 0)
        assign(a, NO_REF, NO_LIT); // else: an empty level, keeping the index
      continue;
    }
    if (budget_ && ticks_ > budget_)
      return SubResult::Budget;
    const SubLit d = next_decision();
    if (d == NO_LIT) { // every variable is assigned: a total model
      for (SubVar v = 0; v < nvars_; v++)
        model_[v] = values_[sub_lit(v, false)] > 0 ? 1 : -1;
      return SubResult::Sat;
    }
    frames_.push_back(static_cast<uint32_t>(trail_.size()));
    assign(d, NO_REF, NO_LIT);
  }
}

} // namespace tactix
