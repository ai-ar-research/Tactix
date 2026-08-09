// Sweep's sub-solver engine and its refutation derivation. See subsolver.hpp
// for the contract and docs/design/v5-sweep-design.md §3/§4 for why it exists.
// Ordinary CDCL, written small on purpose.
#include "engine/subsolver.hpp"

#include <algorithm>
#include <cassert>

namespace tactix {

// Where this deviates from the kernel, and why:
//  - Watch lists are a `vector<vector<SubWatch>>`, not a flat arena: the flat
//    layout buys cache density in the hot loop at the price of the
//    delayed-relocation discipline (watch.hpp), which an environment of a few
//    dozen clauses gains nothing from.
//  - Binaries are not inlined into them: every clause needs an id to appear in
//    a proof chain, and uniformity beats the binary fast path at this scale.
//  - Decisions run in fixed variable order with saved phases, no activity
//    heuristic; determinism and a bounded budget are the requirement (§3).
//  - Assumptions are MiniSat-shaped: each becomes its own decision level, an
//    already-true one gets an empty level so `level` keeps indexing the
//    assumption list, and a falsified assumption ends the solve as `Unsat`.

namespace {
constexpr SubLit NO_LIT = UINT32_MAX;
// Restart cadence. Environments are tiny, so this only has to stop the solver
// burning its whole budget down one bad subtree.
constexpr uint64_t RESTART_INTERVAL = 100;
} // namespace

void SubSolver::reset(uint32_t nvars) {
  nvars_ = nvars;
  inconsistent_ = false;
  solved_ = false;
  arena_.clear();
  watches_.assign(2 * static_cast<size_t>(nvars), {});
  occurrences_.assign(2 * static_cast<size_t>(nvars), {});
  values_.assign(2 * static_cast<size_t>(nvars), 0);
  model_.assign(nvars, 1);
  phase_.assign(nvars, 1); // positive initial phase (the project convention)
  trail_.clear();
  frames_.assign(1, 0);
  level_.assign(nvars, 0);
  trail_pos_.assign(nvars, 0);
  reason_.assign(nvars, NO_REF);
  propagate_head_ = 0;
  conflict_ref_ = NO_REF;
  pending_refutation_ = NO_REF;
  seen_.assign(nvars, 0);
  analyzed_.clear();
  learned_.clear();
  cone_seen_.assign(nvars, 0);
  cone_touched_.clear();
  cone_stack_.clear();
  chain_entries_.clear();
  derived_.clear();
  target_ = {};
  have_derivation_ = false;
  next_id_ = 1;
  original_clauses_ = 0;
  decision_cursor_ = 0;
  ticks_ = 0;
  budget_ = 0;
  conflicts_ = 0;
}

void SubSolver::attach(SubRef ref) {
  const SubLit *c = clause_lits(ref);
  watches_[c[0]].push_back({c[1], ref});
  watches_[c[1]].push_back({c[0], ref});
}

void SubSolver::assign(SubLit l, SubRef reason) {
  assert(values_[l] == 0);
  values_[l] = 1;
  values_[sub_negate(l)] = -1;
  const SubVar v = sub_var_of(l);
  level_[v] = static_cast<uint32_t>(frames_.size()) - 1;
  trail_pos_[v] = static_cast<uint32_t>(trail_.size());
  reason_[v] = reason;
  trail_.push_back(l);
}

uint32_t SubSolver::add_clause(const std::vector<SubLit> &lits) {
  assert(!solved_ && "original clauses must all precede the first solve");
  const uint32_t id = next_id_++;
  if (inconsistent_)
    return id;
  // Dedup and drop tautologies. Nothing is shrunk against the root assignment:
  // the clause DB must hold the clauses the caller named, since the returned
  // core indexes them.
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
        return id; // tautology: keeps its id, never attached, never cited
    }
    if (!dup)
      c.push_back(l);
  }
  arena_.push_back(id);
  arena_.push_back(static_cast<SubLit>(c.size()));
  const SubRef ref = static_cast<SubRef>(arena_.size());
  arena_.insert(arena_.end(), c.begin(), c.end());
  for (const SubLit l : c)
    occurrences_[l].push_back(ref);
  if (c.empty()) { // the empty clause, stated outright
    inconsistent_ = true;
    pending_refutation_ = ref; // the target is built at the first solve, once
    return id;                 // the original-id watershed is fixed
  }
  if (c.size() == 1) {
    // a unit is never watched (it can never propagate again); it is the reason
    // for its own literal, which is what puts its id into later chains
    if (values_[c[0]] > 0)
      return id;
    if (values_[c[0]] < 0) {
      inconsistent_ = true;
      pending_refutation_ = ref;
      return id;
    }
    assign(c[0], ref);
    return id;
  }
  attach(ref);
  return id;
}

SubSolver::SubRef SubSolver::add_derived(const std::vector<SubLit> &lits,
                                         const std::vector<uint32_t> &chain) {
  const uint32_t id = next_id_++;
  derived_.push_back({id, lits, chain});
  arena_.push_back(id);
  arena_.push_back(static_cast<SubLit>(lits.size()));
  const SubRef ref = static_cast<SubRef>(arena_.size());
  arena_.insert(arena_.end(), lits.begin(), lits.end());
  for (const SubLit l : lits)
    occurrences_[l].push_back(ref);
  if (lits.size() >= 2)
    attach(ref);
  return ref;
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
      if (values_[w.blocking] > 0) { // blocking literal already satisfies it
        ws[j++] = w;
        continue;
      }
      SubLit *c = clause_lits(w.ref);
      const uint32_t size = clause_size(w.ref);
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
        conflict_ref_ = w.ref;
        while (i < n)
          ws[j++] = ws[i++];
        ws.resize(j);
        return false;
      }
      assign(c[0], w.ref);
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
    phase_[v] = (l & 1u) ? -1 : 1; // save the phase being given up
    values_[l] = 0;
    values_[sub_negate(l)] = 0;
    reason_[v] = NO_REF;
  }
  frames_.resize(level + 1);
  propagate_head_ = keep;
  decision_cursor_ = 0; // fixed order: rescan from the start
}

// --- proof plumbing ---------------------------------------------------------
// The kernel's construction, transplanted: antecedents are collected as (trail
// position, clause id) pairs and emitted ascending with the conflicting clause
// last, the order a checker's own propagation visits them
// (`kernel_lrat_chain`). Every lemma follows from the environment alone —
// assumptions enter as decisions and leave as literals of the learned clause,
// never as premises — so lemmas stay valid across assumption sets and restarts.

void SubSolver::chain_clear() {
  chain_entries_.clear();
  for (const SubVar v : cone_touched_)
    cone_seen_[v] = 0;
  cone_touched_.clear();
}

void SubSolver::record_cone(SubVar v, std::vector<SubLit> *decisions) {
  cone_stack_.clear();
  cone_stack_.push_back(v);
  while (!cone_stack_.empty()) {
    const SubVar w = cone_stack_.back();
    cone_stack_.pop_back();
    if (cone_seen_[w])
      continue;
    cone_seen_[w] = 1;
    cone_touched_.push_back(w);
    const SubRef reason = reason_[w];
    if (reason == NO_REF) { // a decision: an assumption the target negates
      if (decisions)
        decisions->push_back(trail_[trail_pos_[w]]);
      continue;
    }
    chain_entries_.emplace_back(trail_pos_[w], clause_id(reason));
    const uint32_t size = clause_size(reason);
    const SubLit *c = clause_lits(reason);
    for (uint32_t i = 0; i < size; i++)
      if (sub_var_of(c[i]) != w)
        cone_stack_.push_back(sub_var_of(c[i]));
  }
}

std::vector<uint32_t> SubSolver::chain_build(SubRef closing) {
  std::sort(chain_entries_.begin(), chain_entries_.end());
  std::vector<uint32_t> chain;
  chain.reserve(chain_entries_.size() + 1);
  for (const auto &e : chain_entries_)
    chain.push_back(e.second);
  if (closing != NO_REF)
    chain.push_back(clause_id(closing));
  return chain;
}

void SubSolver::refute_empty(SubRef conflict) {
  // Every literal of `conflict` is false at level 0, so the empty clause
  // follows from their reason cones plus the conflicting clause itself.
  inconsistent_ = true;
  chain_clear();
  const uint32_t size = clause_size(conflict);
  const SubLit *c = clause_lits(conflict);
  for (uint32_t i = 0; i < size; i++)
    record_cone(sub_var_of(c[i]), nullptr);
  target_ = {next_id_++, {}, chain_build(conflict)};
  have_derivation_ = true;
}

bool SubSolver::analyze() {
  const uint32_t current = static_cast<uint32_t>(frames_.size()) - 1;
  assert(current > 0);
  learned_.clear();
  learned_.push_back(0); // placeholder for the asserting literal
  for (const SubVar v : analyzed_)
    seen_[v] = 0;
  analyzed_.clear();
  chain_clear();

  uint32_t count = 0; // seen, unresolved, at the current level
  const auto see = [&](SubLit l) {
    const SubVar v = sub_var_of(l);
    if (seen_[v])
      return;
    if (level_[v] == 0) {
      // Dropped from the clause, so the checker must re-derive it and its whole
      // reason cone joins the chain. The kernel emits a unit lemma per root
      // literal instead; at this scale the cone is cheaper.
      record_cone(v, nullptr);
      return;
    }
    seen_[v] = 1;
    analyzed_.push_back(v);
    if (level_[v] == current)
      count++;
    else
      learned_.push_back(l);
  };
  {
    const uint32_t size = clause_size(conflict_ref_);
    const SubLit *c = clause_lits(conflict_ref_);
    for (uint32_t i = 0; i < size; i++)
      see(c[i]);
  }
  // Unreachable with correct two-watch propagation: a conflict whose literals
  // all sit below the current level would have fired at that lower level. If it
  // ever happens the caller must draw no conclusion rather than report Unsat —
  // a refutation from an unanalyzed conflict is the one way this engine could
  // lie to its caller.
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
    // Resolve on v: every other literal of its reason joins the clause, and the
    // reason itself joins the chain at v's trail position.
    const SubLit resolved = trail_[idx];
    assert(reason_[v] != NO_REF); // a decision is never resolved on
    chain_entries_.emplace_back(trail_pos_[v], clause_id(reason_[v]));
    const uint32_t size = clause_size(reason_[v]);
    const SubLit *c = clause_lits(reason_[v]);
    for (uint32_t i = 0; i < size; i++)
      if (c[i] != resolved)
        see(c[i]);
  }
  learned_[0] = sub_negate(uip);
  const std::vector<uint32_t> chain = chain_build(conflict_ref_);

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
  const SubRef ref = add_derived(learned_, chain);
  assign(learned_[0], ref);
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
  if (!solved_) {
    // The original-id watershed is fixed here, not per add_clause: every id
    // handed out from now on names a lemma.
    solved_ = true;
    original_clauses_ = next_id_ - 1;
  }
  budget_ = ticks_budget;
  conflicts_ = 0;
  if (inconsistent_) {
    if (!have_derivation_ && pending_refutation_ != NO_REF)
      refute_empty(pending_refutation_); // stated empty clause / clashing unit
    return SubResult::Unsat;
  }
  backtrack(0);
  uint64_t next_restart = RESTART_INTERVAL;
  uint64_t restarts = 0;
  for (;;) {
    if (!propagate()) {
      const uint32_t current = static_cast<uint32_t>(frames_.size()) - 1;
      conflicts_++;
      if (current == 0) {
        // Level 0 carries no assumptions, so this is a global refutation.
        refute_empty(conflict_ref_);
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
      if (values_[a] < 0) {
        // Refuted under the assumptions: the target clause negates exactly `a`
        // plus every assumption decision in the reason cone of `¬a`.
        have_derivation_ = false;
        const SubVar v = sub_var_of(a);
        if (reason_[v] == NO_REF)
          return SubResult::Unsat; // an earlier assumption was `¬a`: the two
                                   // contradict each other, not the formula
        chain_clear();
        std::vector<SubLit> decisions;
        record_cone(v, &decisions);
        std::vector<SubLit> lits{sub_negate(a)};
        for (const SubLit d : decisions) {
          const SubLit n = sub_negate(d);
          if (std::find(lits.begin(), lits.end(), n) == lits.end())
            lits.push_back(n);
        }
        // The reason of `¬a` sits at the highest trail position in its own
        // cone, so sorting ascending already leaves it closing the chain.
        target_ = {next_id_++, lits, chain_build(NO_REF)};
        have_derivation_ = true;
        return SubResult::Unsat;
      }
      frames_.push_back(static_cast<uint32_t>(trail_.size()));
      if (values_[a] == 0)
        assign(a, NO_REF); // else: an empty level, keeping the index
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
    assign(d, NO_REF);
  }
}

bool SubSolver::flip_model_literal(SubLit lit) {
  assert(solved_ && "a flip requires a SAT model");
  const SubVar v = sub_var_of(lit);
  if (v >= nvars_ || level_[v] == 0)
    return false;
  lit = sub_lit(v, model_[v] < 0);
  assert(((lit & 1u) == 0) == (model_[v] > 0));
  for (const SubRef ref : occurrences_[lit]) {
    bool other_true = false;
    const SubLit *c = clause_lits(ref);
    const uint32_t size = clause_size(ref);
    ticks_ += size;
    for (uint32_t i = 0; i < size; i++) {
      const SubLit o = c[i];
      if (sub_var_of(o) == v)
        continue;
      const int8_t value = model_[sub_var_of(o)];
      if ((value > 0) != ((o & 1u) != 0)) {
        other_true = true;
        break;
      }
    }
    if (!other_true)
      return false;
  }
  model_[v] = static_cast<int8_t>(-model_[v]);
  return true;
}

bool SubSolver::derivation(SubDerivation &out) const {
  out.steps.clear();
  out.core.clear();
  if (!have_derivation_)
    return false;
  // Walk back from the target, marking the lemmas and originals it needs.
  std::vector<uint8_t> needed(next_id_ + 1, 0);
  std::vector<uint32_t> todo{target_.id};
  needed[target_.id] = 1;
  const auto step_of = [&](uint32_t id) -> const SubStep * {
    if (id == target_.id)
      return &target_;
    // derived_ is in ascending id order, so a binary search finds it
    const auto it = std::lower_bound(
        derived_.begin(), derived_.end(), id,
        [](const SubStep &s, uint32_t v) { return s.id < v; });
    if (it == derived_.end() || it->id != id)
      return nullptr;
    return &*it;
  };
  while (!todo.empty()) {
    const uint32_t id = todo.back();
    todo.pop_back();
    const SubStep *s = step_of(id);
    if (!s) { // an original clause: it is part of the core
      out.core.push_back(id);
      continue;
    }
    for (const uint32_t a : s->chain)
      if (!needed[a]) {
        needed[a] = 1;
        todo.push_back(a);
      }
  }
  for (const SubStep &s : derived_)
    if (needed[s.id])
      out.steps.push_back(s);
  out.steps.push_back(target_);
  std::sort(out.core.begin(), out.core.end());
  out.core.erase(std::unique(out.core.begin(), out.core.end()), out.core.end());
  return true;
}

} // namespace tactix
