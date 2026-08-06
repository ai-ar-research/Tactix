// See instantiate.hpp for the reference/deviation record.
#include "rules/instantiate/instantiate.hpp"

#include "rules/propagate/propagate.hpp"

#include "proof/proof.hpp"

#include <algorithm>

namespace tactix {

bool instantiate_guard(const Kernel &k) {
  // quiescent root (D-ROOT, C3 Vivify's argument verbatim): this is a pure
  // F-rewrite, so a recorded conflict does not bar it — level == 0 is what
  // the occurrence counts and the unassigned filters need.
  return !k.inconsistent && k.level == 0;
}

// candidate clause: live, irredundant (deviation 1), size >= INST_CLSLIM, not
// root-satisfied, and >= 3 unassigned literals so the rewrite cannot produce a
// unit (cadical's `unassigned < 3` filter — "avoid learning units")
static bool is_candidate(const Kernel &k, const Clause *c) {
  if (c->garbage() || c->redundant() || c->size < INST_CLSLIM)
    return false;
  uint32_t unassigned = 0;
  for (const Lit l : *c) {
    const int8_t v = k.value(l);
    if (v > 0)
      return false; // root-satisfied
    if (v == 0)
      unassigned++;
  }
  return unassigned >= 3;
}

// irredundant occurrence counts per literal — one arena pass.
// Per ROUND, not per candidate: an O(nlits) fill inside the probe loop is the
// per-call linear-scratch hazard, and it dominates the profile when it happens.
static void occurrence_counts(const Kernel &k, std::vector<uint32_t> &occ) {
  occ.assign(2 * k.nvars, 0);
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    const uint32_t words = clause_words(c->size);
    if (!c->garbage() && !c->redundant())
      for (const Lit l : *c)
        occ[l]++;
    ref += words;
  }
}

// eligible literal: unassigned and rare in its own polarity. `occ[l] >= 1`
// always (l occurs in the candidate), so INST_OCCLIM == 1 means "l occurs
// nowhere else".
static bool eligible(const Kernel &k, const std::vector<uint32_t> &occ, Lit l) {
  return k.value(l) == 0 && occ[l] <= INST_OCCLIM;
}

static bool has_eligible(const Kernel &k, const Clause *c,
                         const std::vector<uint32_t> &occ) {
  for (const Lit l : *c)
    if (eligible(k, occ, l))
      return true;
  return false;
}

void instantiate_candidates(const Kernel &k, std::vector<Ref> &out) {
  out.clear();
  std::vector<uint32_t> occ;
  occurrence_counts(k, occ);
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    const uint32_t words = clause_words(c->size);
    if (is_candidate(k, c) && has_eligible(k, c, occ))
      out.push_back(ref);
    ref += words;
  }
}

// decision-free probe assumption / abandonment (C3 Vivify's copies of the
// Lucky primitives; a rule may not call another rule, so these are local)
static void assume(Kernel &k, Lit lit) {
  k.level++;
  k.frames.push_back(static_cast<uint32_t>(k.trail.size()));
  kernel_assign(k, lit, k.level, DECISION_REASON, false);
}

static void unwind(Kernel &k) {
  k.conflict_valid = false;
  k.conflict_ref = INVALID_REF;
  if (k.level > 0)
    kernel_backtrack(k, 0, /*update_phases=*/false);
  // give search back the watch order / `searched` hints this probe churned;
  // MUST run before the arena is mutated (the log holds Refs)
  kernel_probing_end(k);
}

// emit the strengthened clause's ADD; entries are sorted by trail position so
// the hints replay as a unit propagation, `last` closes with the conflict
// (C3 Vivify's emit_chain).
static void emit_chain(Kernel &k, uint64_t id, const std::vector<Lit> &lits,
                       std::vector<std::pair<uint32_t, uint64_t>> &entries,
                       uint64_t last) {
  std::sort(entries.begin(), entries.end());
  std::vector<uint64_t> chain;
  chain.reserve(entries.size() + 1);
  for (const auto &e : entries)
    chain.push_back(e.second);
  if (last)
    chain.push_back(last);
  k.proof->add_rup(id, lits.data(), static_cast<uint32_t>(lits.size()),
                   chain.data(), static_cast<uint32_t>(chain.size()));
}

// probe one (clause, literal) pair. Returns true when the assumption set
// ¬D ∧ l propagated to a conflict, i.e. D is provable. On true the trail is
// STILL LOADED (the caller needs it for the chain) and the probing scope is
// still open; on false everything is already unwound.
static bool probe_pair(Kernel &k, Ref cref, Lit lit) {
  const Clause *c = clause_at(k.arena, cref);
  kernel_probing_begin(k);
  if (k.value(lit) != 0) { // defensive: duplicate literal in the clause
    unwind(k);
    return false;
  }
  assume(k, lit); // l TRUE — the assumption D's negation does NOT contain
  for (const Lit m : *c) {
    if (m == lit)
      continue;
    const int8_t v = k.value(m);
    if (v < 0)
      continue; // already false (root, or an earlier assumption)
    if (v > 0) {
      // a literal of D is TRUE under our own assumptions: only reachable if
      // the clause holds both m and ¬lit-style duplicates. Abandon.
      unwind(k);
      return false;
    }
    assume(k, negate(m)); // every other literal FALSE
  }
  k.stats.instantiate_checked++;
  // the candidate is satisfied by `lit` under this assumption set, so it
  // cannot propagate — no `ignore` needed (C3 Vivify's circularity worry does
  // not arise here)
  if (kernel_propagate_probing(k))
    return true;
  unwind(k);
  return false;
}

// try every eligible literal of one candidate; the first success rewrites
// (deviation 2). `ended_round` reports a root-value change.
static ApplyResult instantiate_one(Kernel &k, Ref cref,
                                   const std::vector<uint32_t> &occ,
                                   bool *ended_round) {
  const Clause *c = clause_at(k.arena, cref);
  if (!is_candidate(k, c))
    return ApplyResult::NoOp;
  const bool lrat = k.proof && k.proof->lrat();
  // Persistent walk scratch (C4-perf's shape, shared with C3 Vivify's copy):
  // grown on demand, restored all-zero in O(marked) at the exits below.
  static thread_local std::vector<uint8_t> seen;
  static thread_local std::vector<Var> seen_touched;
  std::vector<Lit> cand_lits(c->begin(), c->end());
  std::vector<Lit> next;
  std::vector<std::pair<uint32_t, uint64_t>> entries;
  for (const Lit lit : cand_lits) {
    if (!eligible(k, occ, lit))
      continue;
    if (lrat) {
      if (seen.size() < k.nvars)
        seen.resize(k.nvars, 0);
      seen_touched.clear();
      entries.clear();
    }
    if (!probe_pair(k, cref, lit))
      continue; // instantiation failed: the pair proves nothing
    // CONFLICT: F ⊨ ¬lit ∨ D, and with C = lit ∨ D that gives D.
    uint64_t last = 0;
    const uint64_t cid = clause_at(k.arena, cref)->id();
    if (lrat) {
      last = kernel_conflict_reason_walk(k, seen, entries, &seen_touched);
      // deviation 5: the checker reaches our probe state from ¬D alone by
      // propagating `lit` out of C itself, so C is an antecedent — at the
      // trail position where `lit` entered. Cited only when the walk reached
      // `lit` (cadical's `flags (lit).seen` guard); if it did not, the
      // conflict follows from ¬D without ever using `lit`.
      if (seen[var_of(lit)])
        entries.emplace_back(k.assigned[var_of(lit)].trail, cid);
    }
    next.clear();
    for (const Lit m : cand_lits)
      if (m != lit)
        next.push_back(m);
    unwind(k); // trail down + probing scope closed BEFORE the arena moves
    for (const Var tv : seen_touched)
      seen[tv] = 0;
    seen_touched.clear();
    // rewrite: ADD the strengthened clause, then delete the original
    Clause *cc = clause_at(k.arena, cref);
    const uint64_t id = ++k.next_clause_id;
    if (k.proof) {
      if (lrat)
        emit_chain(k, id, next, entries, last);
      else
        k.proof->add_rup(0, next.data(), static_cast<uint32_t>(next.size()),
                         nullptr, 0);
    }
    k.stats.instantiated++;
    switch (kernel_replace_clause(k, cc, id, next.data(),
                                  static_cast<uint32_t>(next.size()),
                                  /*redundant=*/false, /*glue=*/0, nullptr)) {
    case ReplaceOutcome::Refuted:
      return ApplyResult::Unsat;
    case ReplaceOutcome::Unit:
      // unreachable through is_candidate's `unassigned >= 3`, but a unit is
      // legal state under N1's exotic schedules: assert it and end the round
      // (C3 Vivify deviation 6).
      k.unit_ids[var_of(next[0])] = id;
      kernel_assign_unit(k, next[0]);
      *ended_round = true;
      return ApplyResult::Ok;
    default:
      return ApplyResult::Ok;
    }
  }
  for (const Var tv : seen_touched) // no success: restore the scratch
    seen[tv] = 0;
  seen_touched.clear();
  return ApplyResult::NoOp;
}

ApplyResult instantiate_apply(Kernel &k) {
  assert(instantiate_guard(k));
  // D-LEVEL guard purification: complete the root closure U if applied off a
  // fixpoint (no-op otherwise). A root conflict during the drain refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  std::vector<uint32_t> occ;
  occurrence_counts(k, occ);
  std::vector<Ref> candidates;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    const uint32_t words = clause_words(c->size);
    if (is_candidate(k, c) && has_eligible(k, c, occ))
      candidates.push_back(ref);
    ref += words;
  }
  uint64_t changed = 0;
  bool ended = false;
  // budget_aware batch round (deviation 4): the policy's ledger is in
  // propagation ticks, C3 Vivify's currency — these probes ARE propagation.
  const uint64_t tick_start = k.stats.ticks;
  // SCHED-F3 · repetition filter over the touched-epoch substrate. cadical's
  // own suppressor here is a per-clause `instantiated` bit set at the first
  // attempt and never reset (`instantiateonce`, instantiate.cpp:32,211) — "try
  // each clause at most once, ever". Ours is deliberately WEAKER: a clause is
  // re-offered as soon as anything in it changes, which recovers the
  // instantiations cadical's once-ever bit forfeits while still removing the
  // whole-formula rescan.
  //
  // `need = 1`, unlike the subsumption family's 2: the probe is a PROPAGATION,
  // so a single new clause over a single one of c's variables can change its
  // outcome. The 2-of-them argument (a new subsumer needs two shared
  // variables) does not transfer.
  // N1: `instantiate_candidates` (what `instances()` enumerates) and
  // `instantiate_try` (the `apply_instance` path) are untouched.
  const bool filter = k.touch_filter;
  const uint64_t snapshot = k.touch_epoch;
  const uint64_t wm = k.instantiate_touch_mark;
  size_t stopped_at = candidates.size();
  for (size_t ci = 0; ci < candidates.size(); ci++) {
    const Ref cref = candidates[ci];
    if (k.work_budget && k.stats.ticks - tick_start > k.work_budget) {
      stopped_at = ci;
      break;
    }
    if (filter &&
        !kernel_clause_new_material(k, clause_at(k.arena, cref), wm, 1)) {
      k.stats.instantiate_mark_skipped++;
      continue;
    }
    const ApplyResult r = instantiate_one(k, cref, occ, &ended);
    if (r == ApplyResult::Unsat)
      return ApplyResult::Unsat;
    if (r == ApplyResult::Ok)
      changed++;
    if (ended) {
      stopped_at = ci + 1;
      break;
    }
  }
  // RESUME (SCHED-F2's mechanism), reachable here: this entry is budget_aware
  // at 20 per mille and its probes are propagations, so the budget binds.
  for (size_t ci = filter ? stopped_at : candidates.size();
       ci < candidates.size(); ci++) {
    const Clause *c = clause_at(k.arena, candidates[ci]);
    if (c->garbage() || !kernel_clause_new_material(k, c, wm, 1))
      continue;
    for (const Lit l : *c)
      kernel_touch_var(k, var_of(l));
    k.stats.instantiate_rearmed++;
  }
  if (filter)
    k.instantiate_touch_mark = snapshot;
  if (!changed)
    return ApplyResult::NoOp;
  k.stats.instantiate_rounds++;
  kernel_collect_garbage(k);
  return ApplyResult::Ok;
}

ApplyResult instantiate_try(Kernel &k, Ref candidate) {
  assert(instantiate_guard(k));
  std::vector<uint32_t> occ;
  occurrence_counts(k, occ);
  bool ended = false;
  const ApplyResult r = instantiate_one(k, candidate, occ, &ended);
  if (r == ApplyResult::Ok)
    kernel_collect_garbage(k);
  return r;
}

static uint32_t instantiate_instances(Kernel &k, uint64_t *out, uint32_t cap) {
  std::vector<Ref> candidates;
  instantiate_candidates(k, candidates);
  const uint32_t n =
      std::min<uint32_t>(cap, static_cast<uint32_t>(candidates.size()));
  for (uint32_t i = 0; i < n; i++)
    out[i] = candidates[i];
  return n;
}

static ApplyResult instantiate_apply_instance(Kernel &k, uint64_t inst) {
  // D-LEVEL: complete the root closure if applied off a fixpoint (no-op
  // otherwise); a root conflict during the drain refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  if (inst == INSTANCE_CANONICAL)
    return instantiate_apply(k);
  for (Ref ref = 0; ref < k.arena.size();) {
    if (ref == inst)
      return instantiate_try(k, ref);
    if (ref > inst)
      break;
    ref += clause_words(clause_at(k.arena, ref)->size);
  }
  return ApplyResult::NoOp;
}

TACTIX_REGISTER_RULE(Instantiate, RuleTier::Inprocess, instantiate_guard,
                     instantiate_apply, instantiate_instances,
                     instantiate_apply_instance);

} // namespace tactix
