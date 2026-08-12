// Transliterated from kissat/src/lucky.c (MIT). Probes share the single
// propagations/ticks counters rather than kissat's separate probing-mode
// statistics, so a policy's tick budget includes them. Probes never update
// saved/target/best phases (the public probe entry suppresses assign-time
// saving, matching kissat's `solver->probing`).
#include "rules/lucky/lucky.hpp"

#include "rules/propagate/propagate.hpp"

#include "proof/proof.hpp"

#include <algorithm>

namespace tactix {

RuleEligibility lucky_guard(const Kernel &k) {
  // quiescent root state: see lucky.hpp
  RuleEligibility result;
  return result.fact("consistent", !k.inconsistent)
      .fact("no-conflict", !k.conflict_valid)
      .fact("root-level", k.level == 0);
}

// abandon an in-flight probe: clear the recorded conflict (its clause
// stays in F; the assignment that falsified it is being undone) and
// restore the entry state without touching phases
static void abandon_probe(Kernel &k) {
  k.conflict_valid = false;
  k.conflict_ref = INVALID_REF;
  if (k.level > 0)
    kernel_backtrack(k, 0, /*update_phases=*/false);
}

// decision-free probe assumption (kissat kissat_internal_assume)
static void assume(Kernel &k, Lit lit) {
  k.level++;
  k.frames.push_back(static_cast<uint32_t>(k.trail.size()));
  kernel_assign(k, lit, k.level, DECISION_REASON, false);
}

// A conflict under the very first Lucky assumption proves its negation at the
// root. Kissat analyzes and commits that failed literal before continuing the
// same directional sweep. Keeping this consequence is essential: abandoning
// it left FmlaEquivChain with 30 extra active variables before Factor and
// changed the mutable Factor heap even though all later grid guards matched.
static ApplyResult commit_failed_literal(Kernel &k, Lit assumed) {
  assert(k.level == 1 && k.conflict_valid);
  const Lit unit = negate(assumed);
  const bool lrat = k.proof && k.proof->lrat();
  uint64_t closing = 0;
  std::vector<std::pair<uint32_t, uint64_t>> entries;
  if (lrat) {
    std::vector<uint8_t> seen(k.nvars, 0);
    closing = kernel_conflict_reason_walk(k, seen, entries);
  }

  k.conflict_valid = false;
  k.conflict_ref = INVALID_REF;
  kernel_backtrack(k, 0, /*update_phases=*/false);

  const uint64_t id = ++k.next_clause_id;
  if (k.proof) {
    if (lrat) {
      std::sort(entries.begin(), entries.end());
      std::vector<uint64_t> chain;
      chain.reserve(entries.size() + 1);
      for (const auto &entry : entries)
        chain.push_back(entry.second);
      chain.push_back(closing);
      k.proof->add_rup(id, &unit, 1, chain.data(),
                       static_cast<uint32_t>(chain.size()));
    } else {
      k.proof->add_rup(0, &unit, 1, nullptr, 0);
    }
  }
  k.unit_ids[var_of(unit)] = id;
  kernel_assign_unit(k, unit);
  return propagate_service(k);
}

// True iff the constant assignment of polarity !want_negated satisfies every
// irredundant clause, i.e. none of them needs a `want_negated` literal (kissat
// no_all_negative_clauses / no_all_positive_clauses).
static bool no_all_polarity_clauses(const Kernel &k, bool want_negated) {
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage() && !c->redundant()) {
      bool ok = false;
      for (const Lit l : *c)
        if (negated(l) != want_negated && k.value(l) >= 0) {
          ok = true;
          break;
        }
      if (!ok)
        return false; // this clause needs the other polarity
    }
    ref += clause_words(c->size);
  }
  // binaries live only in watch lists: scan each variable's
  // `want_negated`-polarity literal for a partner of the same polarity
  for (Var v = 0; v < k.nvars; v++) {
    const Lit lit = make_lit(v, want_negated);
    for (const WatchEntry w : k.watches.entries(lit))
      if (w.binary() && negated(w.other()) == want_negated)
        return false;
  }
  return true;
}

// assign every unassigned variable to `phase` (+1/-1), propagating after
// each; kissat asserts no conflict is possible after the clause scan
static ApplyResult constant_probe(Kernel &k, int8_t phase) {
  if (!no_all_polarity_clauses(k, /*want_negated=*/phase > 0))
    return ApplyResult::Ok; // some clause defeats this constant
  for (Var v = 0; v < k.nvars; v++) {
    const Lit lit = make_lit(v, phase < 0);
    if (k.value(lit) != 0)
      continue;
    assume(k, lit);
    if (kernel_propagate(k)) { // defensive: abandon (kissat asserts !c)
      abandon_probe(k);
      return ApplyResult::Ok;
    }
  }
  return k.unassigned_active ? ApplyResult::Ok : ApplyResult::Sat;
}

// Sweep Kissat's external-variable import table in `forward` order while
// assuming phase `phase`.  Tumble makes internal identities follow first
// occurrence, but Lucky deliberately does not use that order.  On a conflict
// deeper than the first probe level, retry the flipped literal once; if both
// phases conflict, abandon the whole probe.
static ApplyResult sweep_probe(Kernel &k, int8_t phase, bool forward) {
  std::vector<Var> order;
  kernel_vars_in_external_order(k, order);
  for (uint32_t i = 0; i < order.size(); i++) {
    const Var v = forward ? order[i] : order[order.size() - 1 - i];
    Lit lit = make_lit(v, phase < 0);
    if (k.value(lit) != 0)
      continue;
    assume(k, lit);
    if (!kernel_propagate(k))
      continue;
    if (k.level > 1) { // retry the flipped assumption once
      k.conflict_valid = false;
      k.conflict_ref = INVALID_REF;
      kernel_backtrack(k, k.level - 1, /*update_phases=*/false);
      assume(k, negate(lit));
      if (!kernel_propagate(k))
        continue;
      // Both phases failed above the first probe level. Neither polarity is a
      // root consequence; retain earlier committed units, abandon this sweep,
      // and let the next Lucky direction try from the clean root.
      abandon_probe(k);
      return ApplyResult::Ok;
    }
    const ApplyResult committed = commit_failed_literal(k, lit);
    if (committed == ApplyResult::Unsat)
      return committed;
  }
  return k.unassigned_active ? ApplyResult::Ok : ApplyResult::Sat;
}

ApplyResult lucky_apply_probe(Kernel &k, uint64_t probe) {
  assert(lucky_guard(k));
  // kissat_lucky runs with solver->probing set, so its speculative decisions
  // and implications must not overwrite search phases.
  const bool saved_suppression = k.phase_saving_suppressed;
  k.phase_saving_suppressed = true;
  struct RestorePhaseSaving {
    Kernel &k;
    bool saved;
    ~RestorePhaseSaving() { k.phase_saving_suppressed = saved; }
  } restore{k, saved_suppression};
  k.stats.lucky_probes++;
  switch (probe) {
  case LUCKY_ALL_TRUE:
    return constant_probe(k, 1);
  case LUCKY_ALL_FALSE:
    return constant_probe(k, -1);
  case LUCKY_FORWARD_FALSE:
    return sweep_probe(k, -1, true);
  case LUCKY_FORWARD_TRUE:
    return sweep_probe(k, 1, true);
  case LUCKY_BACKWARD_FALSE:
    return sweep_probe(k, -1, false);
  case LUCKY_BACKWARD_TRUE:
    return sweep_probe(k, 1, false);
  default:
    k.stats.lucky_probes--; // unknown payload: nothing ran
    return ApplyResult::NoOp;
  }
}

// canonical instance: kissat's cascade, first success wins
ApplyResult lucky_apply(Kernel &k) {
  // D-LEVEL guard purification: complete the root closure U (no-op at a
  // fixpoint); a root conflict during the drain refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  static constexpr uint64_t CASCADE[6] = {
      LUCKY_ALL_TRUE,       LUCKY_ALL_FALSE,    LUCKY_FORWARD_FALSE,
      LUCKY_FORWARD_TRUE,   LUCKY_BACKWARD_FALSE, LUCKY_BACKWARD_TRUE};
  for (const uint64_t probe : CASCADE) {
    const ApplyResult r = lucky_apply_probe(k, probe);
    if (r == ApplyResult::Sat || r == ApplyResult::Unsat)
      return r;
  }
  return ApplyResult::Ok;
}

static uint32_t lucky_instances(Kernel &, uint64_t *out, uint32_t cap) {
  const uint32_t n = cap < 6 ? cap : 6;
  for (uint32_t i = 0; i < n; i++)
    out[i] = i;
  return n;
}

static ApplyResult lucky_apply_instance(Kernel &k, uint64_t inst) {
  // D-LEVEL guard purification: complete the root closure U (no-op at a
  // fixpoint); a root conflict during the drain refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  if (inst == INSTANCE_CANONICAL)
    return lucky_apply(k);
  return lucky_apply_probe(k, inst);
}

TACTIX_REGISTER_RULE(Lucky, RuleTier::Macro, lucky_guard, lucky_apply,
                     lucky_instances, lucky_apply_instance);

} // namespace tactix
