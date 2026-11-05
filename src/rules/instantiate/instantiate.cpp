// See instantiate.hpp for the rule's contract.
#include "rules/instantiate/instantiate.hpp"

#include "rules/instance_helpers.hpp"
#include "rules/propagate/propagate.hpp"

#include "proof/proof.hpp"

#include <algorithm>

namespace tactix {

RuleEligibility instantiate_guard(const Kernel &k) {
  // Declared quiescent-root boundary. A pure F-rewrite, so a recorded conflict does not
  // bar it; level == 0 is what the occurrence and unassigned filters need.
  return root_level_guard(k);
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
  for (const auto [ref, c] : arena_records(k.arena)) {
    if (!c->garbage() && !c->redundant())
      for (const Lit l : *c)
        occ[l]++;
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

// The candidates over precomputed occurrence counts `occ`, in arena order.
static void collect_candidates(const Kernel &k,
                               const std::vector<uint32_t> &occ,
                               std::vector<Ref> &out) {
  out.clear();
  for (const auto [ref, c] : arena_records(k.arena)) {
    if (is_candidate(k, c) && has_eligible(k, c, occ))
      out.push_back(ref);
  }
}

void instantiate_candidates(const Kernel &k, std::vector<Ref> &out) {
  std::vector<uint32_t> occ;
  occurrence_counts(k, occ);
  collect_candidates(k, occ, out);
}

// probe one (clause, literal) pair. Returns true when the assumption set
// ¬D ∧ l propagated to a conflict, i.e. D is provable. On true the trail is
// still loaded and the probing scope is still open; on false everything is already unwound.
static bool probe_pair(Kernel &k, Ref cref, Lit lit) {
  const Clause *c = clause_at(k.arena, cref);
  kernel_probing_begin(k);
  if (k.value(lit) != 0) { // defensive: duplicate literal in the clause
    probe_unwind(k);
    return false;
  }
  // l TRUE — the assumption D's negation does NOT contain
  kernel_assign_decision(k, lit);
  for (const Lit m : *c) {
    if (m == lit)
      continue;
    const int8_t v = k.value(m);
    if (v < 0)
      continue; // already false (root, or an earlier assumption)
    if (v > 0) {
      // a literal of D is true under the assumption set itself: only reachable
      // when the clause holds m and a ¬lit-style duplicate. Abandon.
      probe_unwind(k);
      return false;
    }
    kernel_assign_decision(k, negate(m)); // every other literal FALSE
  }
  k.stats.instantiate_checked++;
  // the candidate is satisfied by `lit` under this assumption set, so it
  // cannot propagate — no `ignore` needed (C3 Vivify's circularity worry does
  // not arise here)
  if (kernel_propagate_probing(k))
    return true;
  probe_unwind(k);
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
  if (selected &&
      (!clause_contains(c, *selected) || k.value(*selected) != 0))
    return ApplyResult::NoOp;
  std::vector<Lit> cand_lits(c->begin(), c->end());
  std::vector<Lit> next;
  for (const Lit lit : cand_lits) {
    // §6 safe point by construction: a failed `probe_pair` has already unwound
    // the trail and closed the probing scope and a successful one returns
    // outright — so the clause is either fully
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
    if (!probe_pair(k, cref, lit))
      continue; // instantiation failed: the pair proves nothing
    // CONFLICT: F ⊨ ¬lit ∨ D, and with C = lit ∨ D that gives D.
    next.clear();
    for (const Lit m : cand_lits)
      if (m != lit)
        next.push_back(m);
    probe_unwind(k); // trail down + probing scope closed BEFORE the arena moves
    // rewrite: ADD the strengthened clause, then delete the original
    Clause *cc = clause_at(k.arena, cref);
    const uint64_t id = ++k.next_clause_id;
    emit_rup_lemma(k, id, next);
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
  collect_candidates(k, occ, candidates);
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

static uint32_t instantiate_semantic_instances(Kernel &k, uint64_t *out,
                                               uint32_t cap) {
  uint32_t count = 0;
  for (const auto [ref, c] : arena_records(k.arena)) {
    if (count == cap)
      break;
    if (!c->garbage())
      for (const Lit literal : *c) {
        if (k.value(literal) == 0)
          out[count++] = clause_literal_instance(ref, literal);
        if (count == cap)
          break;
      }
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
  clause_literal_of_instance(instance, &selected_ref, &selected_literal);
  if (selected_ref == INVALID_REF || selected_literal >= 2 * k.nvars)
    return ApplyResult::NoOp;
  const Ref ref = arena_clause_instance(k, selected_ref);
  return ref == INVALID_REF
             ? ApplyResult::NoOp
             : instantiate_try_literal(k, ref, selected_literal);
}

TACTIX_REGISTER_RULE(Instantiate, RuleTier::Inprocess, instantiate_guard,
                     instantiate_apply,
                     arena_list_instances<instantiate_candidates>,
                     (apply_root_arena_instance<instantiate_apply,
                                                instantiate_try>),
                     instantiate_semantic_instances,
                     instantiate_apply_semantic_instance);

} // namespace tactix
