// Transliterated from kissat/src/lucky.c (MIT). Probes share the ordinary
// propagation/tick counters and never update saved/target/best phases
// (kissat's `solver->probing`).
#include "rules/lucky/lucky.hpp"
#include "strategy/execution_scope.hpp"
#include "strategy/lucky_execution.hpp"

#include "rules/propagate/propagate.hpp"

#include "proof/proof.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace tactix {

using execution::LuckyOpportunity;

RuleEligibility lucky_guard(const Kernel &k) {
  // quiescent root state: see lucky.hpp
  RuleEligibility result;
  return result.fact("consistent", !k.inconsistent)
      .fact("no-conflict", !k.conflict_valid)
      .fact("root-level", k.level == 0);
}

// Abandon an in-flight probe: clear its conflict and return to the root
// without touching phases.
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

// A conflict under the first assumption proves its negation at the root;
// kissat commits that failed literal and continues the same sweep.
static ApplyResult commit_failed_literal(Kernel &k, Lit assumed) {
  assert(k.level == 1 && k.conflict_valid);
  const Lit unit = negate(assumed);

  k.conflict_valid = false;
  k.conflict_ref = INVALID_REF;
  kernel_backtrack(k, 0, /*update_phases=*/false);

  const uint64_t id = ++k.next_clause_id;
  if (k.proof)
    k.proof->add_rup(id, &unit, 1);
  k.unit_ids[var_of(unit)] = id;
  kernel_assign_unit(k, unit);
  return propagate_service(k);
}

// kissat ACTIVE (flags.h:23): neither fixed at the root nor retired. Mapping
// holes cannot stand in for this test, since compaction may be declined.
static bool lucky_active(const Kernel &k, Var v) {
  return !kernel_var_retired(k, v) &&
         kernel_root_value(k, make_lit(v, false)) == 0;
}

// True iff the constant assignment of polarity !want_negated satisfies every
// irredundant clause, i.e. none of them needs a `want_negated` literal (kissat
// no_all_negative_clauses / no_all_positive_clauses).
static bool no_all_polarity_clauses(const Kernel &k, bool want_negated) {
  for (const auto [ref, c] : arena_records(k.arena)) {
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
  }
  // Binaries live in watch lists; both endpoints must be active (kissat
  // lucky.c:29 and :37).
  for (Var v = 0; v < k.nvars; v++) {
    if (!lucky_active(k, v))
      continue;
    const Lit lit = make_lit(v, want_negated);
    for (const WatchEntry w : k.watches.entries(lit))
      if (w.binary() && negated(w.other()) == want_negated &&
          lucky_active(k, var_of(w.other())))
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
    if (!lucky_active(k, v)) // kissat lucky.c:325
      continue;
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

// Sweep kissat's external import table in `forward` order assuming `phase`.
// A conflict deeper than the first probe level retries the flipped literal
// once; if both phases conflict, the probe is abandoned.
static ApplyResult sweep_probe(Kernel &k, int8_t phase, bool forward) {
  const size_t count = k.external_to_internal.size();
  for (size_t i = 0; i < count; i++) {
    const size_t external = forward ? i : count - 1 - i;
    const Var v = k.external_to_internal[external];
    // kissat lucky.c:88-95 skips unimported and inactive identities.
    if (v == INVALID_VAR || !lucky_active(k, v))
      continue;
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
      // Both phases failed: keep earlier units and abandon this sweep.
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
  // kissat_lucky runs with solver->probing set: phases are not saved.
  execution::ExecutionScope<execution::ExecutionContext::Speculation> scope(k);
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
  // Complete the root closure first; a root conflict refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  static constexpr uint64_t CASCADE[6] = {
      LUCKY_ALL_TRUE,       LUCKY_ALL_FALSE,    LUCKY_FORWARD_FALSE,
      LUCKY_FORWARD_TRUE,   LUCKY_BACKWARD_FALSE, LUCKY_BACKWARD_TRUE};
  for (const uint64_t probe : CASCADE) {
    const ApplyResult r =
        execution::execute_action(k, execution::LuckyProbeRequest{probe});
    if (r == ApplyResult::Sat || r == ApplyResult::Unsat)
      return r;
  }
  return ApplyResult::Ok;
}

static uint32_t lucky_instances(Kernel &k, uint64_t *out, uint32_t cap) {
  (void)k;
  // Identities are stable (lucky.hpp).
  static constexpr uint64_t ACTIVE[LUCKY_ACTIVE_INSTANCE_COUNT] = {
      LUCKY_ALL_TRUE, LUCKY_ALL_FALSE, LUCKY_FORWARD_FALSE,
      LUCKY_FORWARD_TRUE, LUCKY_BACKWARD_FALSE, LUCKY_BACKWARD_TRUE,
      6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, LUCKY_STARTUP_PARITY,
      LUCKY_STARTUP_COUNT, LUCKY_GRAPH_LABELING, LUCKY_STARTUP_RAMSEY,
      LUCKY_PARITY_PROOF, LUCKY_EXACT_ONE_PROOF, LUCKY_GRAPH_AUTHENTICATE,
      LUCKY_GRAPH_CONTINUE};
  const uint32_t n = std::min<uint32_t>(cap, LUCKY_ACTIVE_INSTANCE_COUNT);
  std::copy_n(ACTIVE, n, out);
  return n;
}

// One Lucky opportunity behind its own guard: NoOp unless `guard` admits it.
template <LuckyOpportunity Opportunity>
static ApplyResult run_guarded(Kernel &k,
                               RuleEligibility (*guard)(const Kernel &)) {
  return guard(k) ? execution::execute_action(
                        k, execution::LuckyRequest<Opportunity>{})
                  : ApplyResult::NoOp;
}

template <LuckyOpportunity Opportunity>
static ApplyResult run_unguarded(Kernel &k) {
  return execution::execute_action(k, execution::LuckyRequest<Opportunity>{});
}

static ApplyResult lucky_apply_instance(Kernel &k, uint64_t inst) {
  using enum LuckyOpportunity;
  // SAT constructors (6-14, 17, 18, 21) are observational unless they publish
  // a model, so they never drain root propagation: a conflict there would give
  // a SAT-only identity refutation semantics.
  switch (inst) {
  case LUCKY_STARTUP_PARITY: return run_guarded<StartupParity>(k, startup_parity_guard);
  case LUCKY_STARTUP_RAMSEY: return run_unguarded<StartupRamsey>(k);
  case LUCKY_STARTUP_COUNT: return run_unguarded<StartupCount>(k);
  case INSTANCE_CANONICAL: return run_unguarded<Cascade>(k);
  case 6: return run_guarded<Parity>(k, parity_model_guard);
  case 7: return run_guarded<Onehot>(k, onehot_model_guard);
  case 8: return run_guarded<Affine>(k, affine_model_guard);
  case 9: return run_guarded<DirectCsp>(k, onehot_model_guard);
  case 10: return run_guarded<BalancedSignature>(k, balanced_signature_model_guard);
  case 11: return run_guarded<Fermat>(k, fermat_model_guard);
  case 12: return run_guarded<Cerny>(k, cerny_model_guard);
  case 13: return run_guarded<LookupCircuit>(k, lookup_circuit_model_guard);
  case 14: return run_guarded<Simon>(k, simon_model_guard);
  case 17: return run_guarded<ExactCover>(k, exact_cover_model_guard);
  case 18: return run_guarded<IndependentSet>(k, independent_set_model_guard);
  case LUCKY_GRAPH_LABELING: return run_guarded<GraphLabeling>(k, graph_labeling_model_guard);
  case LUCKY_GRAPH_AUTHENTICATE:
  case LUCKY_GRAPH_CONTINUE: {
    const auto out = execution::execute_action(k, execution::GraphLabelingActionRequest{
        inst == LUCKY_GRAPH_AUTHENTICATE
            ? execution::GraphLabelingOperation::Authenticate
            : execution::GraphLabelingOperation::Continue,
        strategy::GraphLabelingParameters::from_kernel(k)});
    k.graph_labeling_session.record_public_result(out.authentication, out.slice);
    return out.result;
  }
  }
  if (lucky_retired_constructor_instance(inst))
    return ApplyResult::NoOp;
  // Proof-capable and probe actions enter at a propagated root, where a
  // conflict is a legitimate proof result.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  switch (inst) {
  case 15: return run_guarded<Stconn>(k, stconn_proof_guard);
  case 16: return run_guarded<SignedCardinality>(k, signed_cardinality_proof_guard);
  case LUCKY_PARITY_PROOF: return run_guarded<ParityProof>(k, parity_model_guard);
  case LUCKY_EXACT_ONE_PROOF: return run_guarded<ExactOneProof>(k, onehot_model_guard);
  }
  return execution::execute_action(k, execution::LuckyProbeRequest{inst});
}

static bool lucky_startup_instance(uint64_t inst) {
  return inst == LUCKY_STARTUP_PARITY || inst == LUCKY_STARTUP_COUNT ||
         inst == LUCKY_STARTUP_RAMSEY;
}

TACTIX_REGISTER_RULE(Lucky, RuleTier::Macro, lucky_guard, lucky_apply,
                     lucky_instances, lucky_apply_instance, nullptr, nullptr,
                     ActionKind::ProofRule, 0, lucky_startup_instance);

} // namespace tactix
