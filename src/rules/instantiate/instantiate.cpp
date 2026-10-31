// See instantiate.hpp for the rule's contract.
#include "rules/instantiate/instantiate.hpp"

#include "rules/instance_helpers.hpp"
#include "rules/propagate/propagate.hpp"

#include "proof/proof.hpp"
#include "proof/rup_helpers.hpp"

#include <algorithm>

namespace tactix {

RuleEligibility instantiate_guard(const Kernel &k) {
  // Declared quiescent-root boundary. A pure F-rewrite, so a recorded conflict does not
  // bar it; level == 0 is what the occurrence and unassigned filters need.
  RuleEligibility result;
  return result.fact("consistent", !k.inconsistent)
      .fact("root-level", k.level == 0);
}

// Canonical candidate advice. None of these strategy limits constrains the
// explicit semantic (clause,literal) surface below.
static bool is_candidate(const Kernel &k, const Clause *c) {
  const strategy::InstantiateParameters &parameters =
      k.instantiate_parameters;
  if (c->garbage() || (!parameters.include_redundant && c->redundant()) ||
      (parameters.minimum_clause_size &&
       c->size < parameters.minimum_clause_size))
    return false;
  uint32_t unassigned = 0;
  for (const Lit l : *c) {
    const int8_t v = k.value(l);
    if (v > 0)
      return false; // root-satisfied
    if (v == 0)
      unassigned++;
  }
  return !parameters.minimum_unassigned ||
         unassigned >= parameters.minimum_unassigned;
}

// irredundant occurrence counts per literal, one arena pass. Per round, not per
// candidate: an O(nlits) fill inside the probe loop is the per-call
// linear-scratch hazard, which dominates the profile when it happens.
static void occurrence_counts(const Kernel &k, std::vector<uint32_t> &occ) {
  occ.assign(2 * k.nvars, 0);
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    const uint32_t words = clause_actual_words(c);
    if (!c->garbage() && !c->redundant())
      for (const Lit l : *c)
        occ[l]++;
    ref += words;
  }
}

// Canonical literal advice: unassigned and rare in its own polarity.
static bool eligible(const Kernel &k, const std::vector<uint32_t> &occ, Lit l) {
  return k.value(l) == 0 &&
         occ[l] <= k.instantiate_parameters.occurrence_limit;
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
    const uint32_t words = clause_actual_words(c);
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

// probe one (clause, literal) pair. Returns true when the assumption set
// ¬D ∧ l propagated to a conflict, i.e. D is provable. On true the trail is
// still loaded (the caller needs it for the chain) and the probing scope is
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
      // a literal of D is true under the assumption set itself: only reachable
      // when the clause holds m and a ¬lit-style duplicate. Abandon.
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

// try every eligible literal of one candidate; the first success rewrites.
// `ended_round` reports a root-value change.
static ApplyResult instantiate_one(Kernel &k, Ref cref,
                                   const std::vector<uint32_t> &occ,
                                   bool *ended_round,
                                   const Lit *selected = nullptr) {
  const Clause *c = clause_at(k.arena, cref);
  if ((!selected && !is_candidate(k, c)) ||
      (selected && c->garbage()))
    return ApplyResult::NoOp;
  if (selected) {
    bool present = false;
    for (const Lit lit : *c)
      present |= lit == *selected;
    if (!present || k.value(*selected) != 0)
      return ApplyResult::NoOp;
  }
  const bool lrat = k.proof && k.proof->lrat();
  // Persistent walk scratch (the per-call linear-scratch fix, shared in shape
  // with C3 Vivify's copy): grown on demand, restored all-zero in O(marked) at
  // the exits below.
  static thread_local std::vector<uint8_t> seen;
  static thread_local std::vector<Var> seen_touched;
  std::vector<Lit> cand_lits(c->begin(), c->end());
  std::vector<Lit> next;
  std::vector<std::pair<uint32_t, uint64_t>> entries;
  for (const Lit lit : cand_lits) {
    // §6 safe point by construction: a failed `probe_pair` has already unwound
    // the trail and closed the probing scope, a successful one returns
    // outright, and `seen` is all-zero here — so the clause is either fully
    // rewritten or untouched, never half-instantiated.
    if (kernel_rule_interrupt(k, k.stats.instantiate_interrupted))
      break;
    // A preceding probe can discover a level-zero implication while standing
    // under assumptions.  Backtracking preserves that fact but leaves its
    // closure pending.  Drain it before a new assumption can mask a watched
    // clause behind the propagation cursor.
    if (inprocess_reach_root(k))
      return ApplyResult::Unsat;
    if (selected ? lit != *selected : !eligible(k, occ, lit))
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
      // The checker reaches the probe state from ¬D alone by propagating `lit`
      // out of C itself, so C is an antecedent at the trail position where
      // `lit` entered. Cited only when the walk reached `lit` (cadical's
      // `flags (lit).seen` guard); otherwise the conflict never used `lit`.
      if (seen[var_of(lit)])
        entries.emplace_back(k.assigned[var_of(lit)].trail, cid);
    }
    next.clear();
    for (const Lit m : cand_lits)
      if (m != lit)
        next.push_back(m);
    if (lrat) {
      // The checker begins with the complete negated lemma cube. A root unit
      // that already makes one of D's literals false is an assumption there,
      // not a propagating hint, even if the probe's reason walk reached it.
      std::vector<uint64_t> cube_root_units;
      for (const Lit m : next)
        if (k.value(m) < 0 && k.assigned[var_of(m)].level == 0) {
          const uint64_t unit_id = k.unit_ids[var_of(m)];
          if (unit_id)
            cube_root_units.push_back(unit_id);
        }
      rup_helpers::remove_assumption_unit_hints(entries, cube_root_units);
    }
    unwind(k); // trail down + probing scope closed BEFORE the arena moves
    for (const Var tv : seen_touched)
      seen[tv] = 0;
    seen_touched.clear();
    // rewrite: ADD the strengthened clause, then delete the original
    Clause *cc = clause_at(k.arena, cref);
    const uint64_t id = ++k.next_clause_id;
    rup_helpers::emit(k, id, 0, next, entries, last);
    k.stats.instantiated++;
    const bool redundant = cc->redundant();
    const uint32_t glue = redundant ? cc->glue() : 0;
    switch (kernel_replace_clause(k, cc, id, next.data(),
                                  static_cast<uint32_t>(next.size()),
                                  redundant, glue, nullptr)) {
    case ReplaceOutcome::Refuted:
      return ApplyResult::Unsat;
    case ReplaceOutcome::Unit:
      // unreachable through is_candidate's `unassigned >= 3`, but a unit is
      // legal state under N1's exotic schedules: assert it and end the round,
      // as C3 Vivify does.
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
  // D-LEVEL: drain the root closure (no-op at a fixpoint); a conflict refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  std::vector<uint32_t> occ;
  occurrence_counts(k, occ);
  std::vector<Ref> candidates;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    const uint32_t words = clause_actual_words(c);
    if (is_candidate(k, c) && has_eligible(k, c, occ))
      candidates.push_back(ref);
    ref += words;
  }
  uint64_t changed = 0;
  bool ended = false;
  // budget_aware batch round: the policy's ledger is in propagation ticks, C3
  // Vivify's currency — these probes are propagation.
  const uint64_t tick_start = k.stats.ticks;
  // Repetition filter over the touched-epoch substrate. cadical's suppressor is
  // a per-clause bit set at the first attempt and never reset
  // (`instantiateonce`, instantiate.cpp:32,211); this one is deliberately
  // weaker, re-offering a clause as soon as anything in it changes, which
  // recovers what the once-ever bit forfeits. The reference need is one, unlike the
  // subsumption family's 2: the probe is a propagation, so one new clause over
  // one of c's variables can change the outcome.
  const bool filter = k.touch_filter;
  const uint64_t snapshot = k.touch_epoch;
  const uint64_t wm = k.instantiate_touch_mark;
  size_t stopped_at = candidates.size();
  for (size_t ci = 0; ci < candidates.size(); ci++) {
    const Ref cref = candidates[ci];
    if (kernel_work_budget_exceeded(k, k.stats.ticks - tick_start)) {
      stopped_at = ci;
      break;
    }
    // §6 termination, distinct from the tick budget above (which is 0 when the
    // policy leaves the round unmetered). Cut at the same point so the untried
    // tail is re-armed by the resume path below.
    if (kernel_rule_interrupt(k, k.stats.instantiate_interrupted)) {
      stopped_at = ci;
      break;
    }
    if (filter &&
        !kernel_clause_new_material(
            k, clause_at(k.arena, cref), wm,
            k.instantiate_parameters.new_material_threshold)) {
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
  // Resume, reachable here: this entry is budget_aware at 20 per mille and its
  // probes are propagations, so the budget binds.
  for (size_t ci = filter ? stopped_at : candidates.size();
       ci < candidates.size(); ci++) {
    const Clause *c = clause_at(k.arena, candidates[ci]);
    if (c->garbage() ||
        !kernel_clause_new_material(
            k, c, wm, k.instantiate_parameters.new_material_threshold))
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

ApplyResult instantiate_try_literal(Kernel &k, Ref candidate, Lit literal) {
  assert(instantiate_guard(k));
  const std::vector<uint32_t> unused;
  bool ended = false;
  const ApplyResult r =
      instantiate_one(k, candidate, unused, &ended, &literal);
  if (r == ApplyResult::Ok)
    kernel_collect_garbage(k);
  return r;
}

static uint32_t instantiate_instances(Kernel &k, uint64_t *out, uint32_t cap) {
  std::vector<Ref> candidates;
  instantiate_candidates(k, candidates);
  return copy_instance_prefix(candidates, out, cap);
}

static ApplyResult instantiate_apply_instance(Kernel &k, uint64_t inst) {
  return apply_root_arena_instance<instantiate_apply, instantiate_try>(k,
                                                                        inst);
}

static uint32_t instantiate_semantic_instances(Kernel &k, uint64_t *out,
                                               uint32_t cap) {
  uint32_t count = 0;
  for (Ref ref = 0; ref < k.arena.size() && count < cap;) {
    const Clause *c = clause_at(k.arena, ref);
    const uint32_t words = clause_actual_words(c);
    if (!c->garbage())
      for (const Lit literal : *c) {
        if (k.value(literal) == 0)
          out[count++] = instantiate_pair_instance(ref, literal);
        if (count == cap)
          break;
      }
    ref += words;
  }
  return count;
}

static ApplyResult instantiate_apply_semantic_instance(Kernel &k,
                                                        uint64_t instance) {
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  if (instance == INSTANCE_CANONICAL)
    return instantiate_apply(k);
  Ref selected_ref;
  Lit selected_literal;
  instantiate_pair_of_instance(instance, &selected_ref, &selected_literal);
  if (selected_ref == INVALID_REF || selected_literal >= 2 * k.nvars)
    return ApplyResult::NoOp;
  const Ref ref = arena_clause_instance(k, selected_ref);
  return ref == INVALID_REF
             ? ApplyResult::NoOp
             : instantiate_try_literal(k, ref, selected_literal);
}

TACTIX_REGISTER_RULE(Instantiate, RuleTier::Inprocess, instantiate_guard,
                     instantiate_apply, instantiate_instances,
                     instantiate_apply_instance,
                     instantiate_semantic_instances,
                     instantiate_apply_semantic_instance);

} // namespace tactix
