// Transliterated from kissat/src/lucky.c (MIT). Deviations (recorded):
// (1) kissat's failed-literal handling inside probes (analyze -> learned
// unit) is dropped — a conflicting probe simply abandons and restores the
// entry state, keeping this rule's proof delta exactly "none" per the
// catalog row; failed-literal machinery arrives properly with Probe (V1,
// phase 3). (2) kissat runs lucky under its probing mode with separate
// statistics; our probes share the single propagations/ticks counters
// (honest memory traffic — documented so the policy's tick budget is
// understood to include them). Probes never update saved/target/best
// phases (kernel_backtrack update_phases=false, kissat
// backtrack_without_updating_phases).
#include "rules/lucky/lucky.hpp"

namespace tactix {

bool lucky_guard(const Kernel &k) {
  // quiescent root state: see lucky.hpp
  return !k.inconsistent && !k.conflict_valid && !k.pending.valid &&
         k.level == 0 && k.propagate_head == k.trail.size();
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

// no clause forces a literal of the given polarity to be its last hope:
// returns true when every irredundant clause has a non-false literal of
// polarity `want_negated` == false ... i.e. the constant assignment with
// polarity !want_negated satisfies everything (kissat
// no_all_negative_clauses / no_all_positive_clauses)
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
    const WatchHead &h = k.watches.heads[lit];
    const uint32_t *ws = k.watches.arena.data() + h.offset;
    for (uint32_t i = 0; i < h.size;) {
      if (Watch{ws[i]}.binary()) {
        if (negated(Watch{ws[i]}.other()) == want_negated)
          return false;
        i += 1;
      } else {
        i += 2;
      }
    }
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
  assert(k.unassigned == 0);
  return ApplyResult::Sat;
}

// sweep the variables in `forward` order assuming phase `phase`; on a
// conflict deeper than the first probe level, retry the flipped literal
// once; any conflict that sticks abandons the whole probe (deviation:
// kissat learns failed-literal units here — see the header comment)
static ApplyResult sweep_probe(Kernel &k, int8_t phase, bool forward) {
  for (uint32_t i = 0; i < k.nvars; i++) {
    const Var v = forward ? i : k.nvars - 1 - i;
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
    }
    abandon_probe(k);
    return ApplyResult::Ok;
  }
  assert(k.unassigned == 0);
  return ApplyResult::Sat;
}

ApplyResult lucky_apply_probe(Kernel &k, uint64_t probe) {
  assert(lucky_guard(k));
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
  static constexpr uint64_t CASCADE[6] = {
      LUCKY_ALL_TRUE,       LUCKY_ALL_FALSE,    LUCKY_FORWARD_FALSE,
      LUCKY_FORWARD_TRUE,   LUCKY_BACKWARD_FALSE, LUCKY_BACKWARD_TRUE};
  for (const uint64_t probe : CASCADE) {
    const ApplyResult r = lucky_apply_probe(k, probe);
    if (r == ApplyResult::Sat)
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
  if (inst == INSTANCE_CANONICAL)
    return lucky_apply(k);
  return lucky_apply_probe(k, inst);
}

TACTIX_REGISTER_RULE(Lucky, RuleTier::Macro, lucky_guard, lucky_apply,
                     lucky_instances, lucky_apply_instance);

} // namespace tactix
