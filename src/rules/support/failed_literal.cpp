#include "rules/support/failed_literal.hpp"

#include "diagnostics/event_trace.hpp"
#include "proof/proof.hpp"
#include "rules/instance_helpers.hpp"
#include "rules/propagate/propagate.hpp"

#include <algorithm>
#include <utility>

namespace tactix {

#ifdef TACTIX_ENABLE_EVENT_TRACE
static uint64_t backbone_candidate_hash(const Kernel &k,
                                        const std::vector<Lit> &candidates) {
  uint64_t hash = 1469598103934665603ull;
  for (const Lit lit : candidates) {
    const int64_t external = kernel_externalize(k, lit);
    hash ^= static_cast<uint64_t>(external);
    hash *= 1099511628211ull;
  }
  return hash;
}

static void trace_backbone_candidates(const Kernel &k, const char *event,
                                      const std::vector<Lit> &candidates) {
  const uint64_t hash = backbone_candidate_hash(k, candidates);
  size_t prioritized = 0;
  for (const Lit lit : candidates)
    prioritized += !!(k.backbone_priority[var_of(lit)] &
                       (1u << negated(lit)));
  event_trace_detail(k, event, static_cast<int64_t>(candidates.size()));
  event_trace_detail(k, "backbone_prioritized",
                     static_cast<int64_t>(prioritized));
  event_trace_detail(k, "backbone_hash_lo",
                     static_cast<int32_t>(hash));
  event_trace_detail(k, "backbone_hash_hi",
                     static_cast<int32_t>(hash >> 32));
}
#endif

RuleEligibility failed_literal_guard(const Kernel &k) {
  RuleEligibility result;
  return result.fact("consistent", !k.inconsistent)
      .fact("no-conflict", !k.conflict_valid)
      .fact("root-level", k.level == 0);
}

// Probe's schedule: a literal is worth assuming only if its negation carries a
// binary watch, i.e. it heads the binary implication graph.
static inline bool binary_implication_head(const Kernel &k, Lit p) {
  for (const WatchEntry w : k.watches.entries(negate(p)))
    if (w.binary())
      return true;
  return false;
}

void failed_literal_candidates(const Kernel &k, std::vector<Lit> &out) {
  out.clear();
  for (Var v = 0; v < k.nvars; v++) {
    if (k.value(make_lit(v, false)) != 0)
      continue;
    for (int neg = 0; neg <= 1; neg++) {
      const Lit p = make_lit(v, neg != 0);
      if (binary_implication_head(k, p))
        out.push_back(p);
    }
  }
}

static bool probe_one(Kernel &k, Lit p, Lit *unit_out,
                      FailedLiteralPropagation propagation,
                      const FailedLiteralStats &stats) {
  k.stats.*stats.probed += 1;
  if (propagation == FailedLiteralPropagation::Full)
    kernel_probing_begin(k);
  kernel_assign_decision(k, p);
  const bool conflict = propagation == FailedLiteralPropagation::Full
                            ? kernel_propagate_probing(k)
                            : kernel_propagate_binary(k);
  if (!conflict) {
    probe_unwind(k, propagation == FailedLiteralPropagation::Full);
    return false;
  }
  k.stats.*stats.failed += 1;
  const Lit unit = negate(p);
  probe_unwind(k, propagation == FailedLiteralPropagation::Full);
  const uint64_t id = ++k.next_clause_id;
  if (k.proof)
    k.proof->add_rup(id, &unit, 1);
  k.unit_ids[var_of(unit)] = id;
  *unit_out = unit;
  return true;
}

// Binary UIP on the Backbone trail: `Assigned::reason` holds the false other
// endpoint, so walking back until a predecessor repeats is the Kissat/CaDiCaL
// binary-UIP; the repeated endpoint is the unit.
static Lit chained_binary_uip(Kernel &k, size_t speculative_begin) {
  assert(k.conflict_valid && k.conflict_ref == INVALID_REF);
  std::vector<uint8_t> seen(k.nvars, 0);
  seen[var_of(k.conflict_bin[0])] = 1;
  seen[var_of(k.conflict_bin[1])] = 1;

  for (size_t i = k.trail.size(); i-- > speculative_begin;) {
    const Lit lit = k.trail[i];
    const Var v = var_of(lit);
    if (!seen[v])
      continue;
    const Assigned &a = k.assigned[v];
    // Reason jumping is off here; a decision/root marker means no UIP: fail
    // closed.
    if (!a.binary || a.jumped || a.reason == DECISION_REASON ||
        a.reason == UNIT_REASON)
      return INVALID_LIT;
    const Lit predecessor = a.reason;
    const Var pv = var_of(predecessor);
    if (!seen[pv]) {
      seen[pv] = 1;
      continue;
    }
    return predecessor;
  }
  return INVALID_LIT;
}

// Kissat's Backbone trail stays speculative until the round ends, even at saved
// level zero: clear the round suffix by position (kernel retraction), then
// replay the unit batch at root.
static void rewind_chained_binary_round(Kernel &k, size_t speculative_begin,
                                        uint32_t unflushed_root_begin) {
  kernel_unwind_speculation(k, static_cast<uint32_t>(speculative_begin));
  // Restore the inherited pending-root marker; the replay re-raises it.
  k.unflushed_root = unflushed_root_begin;
}

ApplyResult failed_literal_apply_chained_binary(
    Kernel &k, const FailedLiteralStats &stats) {
  assert(failed_literal_guard(k));
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;

  // The caller owns Kissat's growing-round schedule. Saturation keeps every
  // uint32 configuration total without changing the reference defaults.
  const uint64_t computation = ++k.stats.backbone_computations;
  const strategy::BackboneParameters &parameters = k.backbone_parameters;
  const uint64_t grown =
      !parameters.initial_rounds
          ? 0
          : computation > UINT64_MAX / parameters.initial_rounds
                ? UINT64_MAX
                : computation * parameters.initial_rounds;
  const uint32_t round_limit = static_cast<uint32_t>(
      std::min<uint64_t>(grown, parameters.maximum_rounds));
  const uint64_t meter_begin = k.stats.backbone_ticks;
  bool changed = false;
  std::vector<Var> candidate_order;
  kernel_vars_in_import_order(k, candidate_order);

  // Kissat's persistent `flags.backbone[01]`: offer surviving priority
  // polarities first, then the rest. The stack persists between rounds: proved
  // assumptions prune it, falsified literals become the next candidates.
  std::vector<Lit> candidates;
  candidates.reserve(2 * candidate_order.size());
  for (int prioritized = 1; prioritized >= 0; prioritized--) {
    for (const Var v : candidate_order) {
      if (kernel_var_retired(k, v))
        continue;
      // Kissat's ACTIVE_VAR schedule excludes root-fixed variables.
      if (k.value(make_lit(v, false))) {
        k.backbone_priority[v] = 0;
        continue;
      }
      for (uint32_t neg = 0; neg != 2; neg++) {
        const uint8_t bit = static_cast<uint8_t>(1u << neg);
        if (static_cast<bool>(k.backbone_priority[v] & bit) ==
            static_cast<bool>(prioritized))
          candidates.push_back(make_lit(v, neg != 0));
      }
    }
    if (!prioritized)
      break;
  }
#ifdef TACTIX_ENABLE_EVENT_TRACE
  event_trace_detail(k, "backbone_computation",
                     static_cast<int64_t>(computation));
  trace_backbone_candidates(k, "backbone_scheduled", candidates);
#endif

  for (uint32_t round = 0; round < round_limit; round++) {
    if (candidates.empty())
      break;
    if (kernel_terminating(k) ||
        kernel_work_budget_exceeded(
            k, k.stats.backbone_ticks - meter_begin))
      break;

    k.stats.*stats.rounds += 1;
#ifdef TACTIX_ENABLE_EVENT_TRACE
    event_trace_detail(k, "backbone_round_begin",
                       static_cast<int64_t>(candidates.size()));
#endif
    const size_t speculative_begin = k.trail.size();
    const uint32_t unflushed_root_begin = k.unflushed_root;
    // A proved unit and its lemma id; the id is installed at assert time, when
    // the literal is root-true (corpus 023).
    std::vector<std::pair<Lit, uint64_t>> units;
    std::vector<Lit> retained;
    retained.reserve(candidates.size());
    bool budget_hit = false;
    bool inconsistent = false;

    size_t next = 0;
    for (; next < candidates.size() && !inconsistent; next++) {
      const Lit p = candidates[next];
      const Var v = var_of(p);
      const uint8_t bit = static_cast<uint8_t>(1u << negated(p));
      if (kernel_var_retired(k, v)) {
        k.backbone_priority[v] &= static_cast<uint8_t>(~bit);
        continue;
      }
      const int8_t value = k.value(p);
      if (value > 0) {
        k.backbone_priority[v] &= static_cast<uint8_t>(~bit);
        continue;
      }
      if (value < 0) {
        if (k.assigned[v].level)
          retained.push_back(p);
        else
          k.backbone_priority[v] &= static_cast<uint8_t>(~bit);
        continue;
      }
      if (kernel_terminating(k) ||
          kernel_work_budget_exceeded(
              k, k.stats.backbone_ticks - meter_begin)) {
        budget_hit = true;
        break;
      }

      retained.push_back(p);
      const uint32_t saved_level = k.level;
      k.stats.*stats.probed += 1;
      kernel_assign_decision(k, p);
      const bool saved_jump_reasons = k.jump_reasons;
      k.jump_reasons = false;
      const bool conflict = kernel_propagate_binary(
          k, /*use_current_level=*/true);
      k.jump_reasons = saved_jump_reasons;
      if (!conflict)
        continue;

      retained.pop_back();
      const Lit unit = chained_binary_uip(k, speculative_begin);
      k.conflict_valid = false;
      k.conflict_ref = INVALID_REF;
      kernel_backtrack(k, saved_level, /*update_phases=*/false);
      if (unit == INVALID_LIT) {
        budget_hit = true; // fail closed; do not repeat this conflict
        next++;
        break;
      }

      k.stats.*stats.failed += 1;
      const uint64_t id = ++k.next_clause_id;
      emit_rup_lemma(k, id, &unit, 1);
      units.emplace_back(unit, id);

      // The unit stays on the speculative trail for the rest of the round
      // (Kissat's saved assumption level). At saved level zero the stamp is a
      // root assertion, so the id is installed now; otherwise the replay
      // installs it.
      if (!saved_level)
        k.unit_ids[var_of(unit)] = id;
      kernel_assign(k, unit, saved_level, UNIT_REASON, false);
      const bool saved_jump_after_uip = k.jump_reasons;
      k.jump_reasons = false;
      inconsistent = kernel_propagate_binary(
          k, /*use_current_level=*/true);
      k.jump_reasons = saved_jump_after_uip;
      if (inconsistent) {
        k.conflict_valid = false;
        k.conflict_ref = INVALID_REF;
      }
    }

    if (next < candidates.size())
      retained.insert(retained.end(), candidates.begin() + next,
                      candidates.end());

    // Kissat's post-round flush: satisfied candidates retire, falsified
    // non-root ones survive in order.
    candidates.clear();
    candidates.reserve(retained.size());
    if (!inconsistent) {
      for (const Lit p : retained) {
        const Var v = var_of(p);
        const uint8_t bit = static_cast<uint8_t>(1u << negated(p));
        if (kernel_var_retired(k, v) || k.value(p) > 0) {
          k.backbone_priority[v] &= static_cast<uint8_t>(~bit);
          continue;
        }
        if (k.value(p) < 0 && !k.assigned[v].level) {
          k.backbone_priority[v] &= static_cast<uint8_t>(~bit);
          continue;
        }
        candidates.push_back(p);
      }
    }

    rewind_chained_binary_round(k, speculative_begin,
                                unflushed_root_begin);

    if (!units.empty()) {
      changed = true;
      for (const auto &proved : units) {
        const Lit unit = proved.first;
        const Var v = var_of(unit);
        const int8_t value = k.value(unit);
        if (value < 0) {
          // The batch contradicts the root: derive the empty clause (corpus
          // 023).
          kernel_emit_empty(k);
          k.unit_ids[v] = proved.second;
          return ApplyResult::Unsat;
        }
        // Assert time, not record time: only now is `unit` the root value.
        k.unit_ids[v] = proved.second;
        if (!value)
          kernel_assign_unit(k, unit);
      }
      // Kissat's `backbone_propagate` charges the private meter but the replay
      // (`kissat_probing_propagate`) only the shared ones; pause the sink.
      uint64_t *const saved_tick_sink = k.tick_sink;
      k.tick_sink = nullptr;
      const ApplyResult replay = propagate_apply(k);
      k.tick_sink = saved_tick_sink;
      if (replay == ApplyResult::Unsat)
        return ApplyResult::Unsat;
    }
#ifdef TACTIX_ENABLE_EVENT_TRACE
    trace_backbone_candidates(k, "backbone_round_end", candidates);
    event_trace_detail(k, "backbone_round_units",
                       static_cast<int64_t>(units.size()));
#endif
    if (budget_hit || inconsistent)
      break;
  }

  size_t prioritized = 0;
  for (const Lit p : candidates) {
    const Var v = var_of(p);
    if (!kernel_var_retired(k, v) &&
        (k.backbone_priority[v] & (1u << negated(p))))
      prioritized++;
  }
  if (!prioritized)
    for (const Lit p : candidates)
      if (!kernel_var_retired(k, var_of(p)))
        k.backbone_priority[var_of(p)] |=
            static_cast<uint8_t>(1u << negated(p));

  return changed ? ApplyResult::Ok : ApplyResult::NoOp;
}

ApplyResult failed_literal_apply(Kernel &k,
                                 FailedLiteralPropagation propagation,
                                 const FailedLiteralStats &stats) {
  assert(failed_literal_guard(k));
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  k.stats.*stats.rounds += 1;
  std::vector<Lit> units;
  // The canonical one-round application walks the var/polarity order directly.
  // Units commit after the scan, so a per-variable bit suppresses the opposite
  // polarity after a failed probe.
  for (Var v = 0; v < k.nvars; v++) {
    if (k.value(make_lit(v, false)) != 0)
      continue;
    bool forced = false;
    for (int neg = 0; neg <= 1 && !forced; neg++) {
      // Close root facts kept below the assumption before the next episode.
      if (inprocess_reach_root(k))
        return ApplyResult::Unsat;
      const Lit p = make_lit(v, neg != 0);
      if (!binary_implication_head(k, p) || k.value(p) != 0)
        continue;
      Lit unit;
      if (probe_one(k, p, &unit, propagation, stats)) {
        forced = true;
        units.push_back(unit);
      }
    }
  }
  if (units.empty())
    return ApplyResult::NoOp;
  for (const Lit unit : units)
    kernel_assign_unit(k, unit);
  return ApplyResult::Ok;
}

ApplyResult failed_literal_try(Kernel &k, Lit p,
                               FailedLiteralPropagation propagation,
                               const FailedLiteralStats &stats) {
  assert(failed_literal_guard(k));
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  if (var_of(p) >= k.nvars || k.value(p) != 0)
    return ApplyResult::NoOp;
  Lit unit;
  if (!probe_one(k, p, &unit, propagation, stats))
    return ApplyResult::NoOp;
  kernel_assign_unit(k, unit);
  return ApplyResult::Ok;
}

uint32_t failed_literal_instances(Kernel &k, uint64_t *out, uint32_t cap) {
  uint32_t n = 0;
  for (Var v = 0; v < k.nvars && n < cap; v++) {
    if (k.value(make_lit(v, false)) != 0)
      continue;
    for (int neg = 0; neg <= 1 && n < cap; neg++) {
      const Lit p = make_lit(v, neg != 0);
      if (binary_implication_head(k, p))
        out[n++] = lit_instance(k, p);
    }
  }
  return n;
}

ApplyResult failed_literal_apply_instance(
    Kernel &k, uint64_t inst, FailedLiteralPropagation propagation,
    const FailedLiteralStats &stats) {
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  if (inst == INSTANCE_CANONICAL)
    return failed_literal_apply(k, propagation, stats);
  Lit p;
  if (!lit_of_instance(k, inst, &p))
    return ApplyResult::NoOp;
  return failed_literal_try(k, p, propagation, stats);
}

} // namespace tactix
