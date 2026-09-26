#include "rules/support/failed_literal.hpp"

#include "proof/proof.hpp"
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

void failed_literal_candidates(const Kernel &k, std::vector<Lit> &out) {
  out.clear();
  for (Var v = 0; v < k.nvars; v++) {
    if (k.value(make_lit(v, false)) != 0)
      continue;
    for (int neg = 0; neg <= 1; neg++) {
      const Lit p = make_lit(v, neg != 0);
      for (const WatchEntry w : k.watches.entries(negate(p)))
        if (w.binary()) {
          out.push_back(p);
          break;
        }
    }
  }
}

static void assume(Kernel &k, Lit lit) {
  k.level++;
  k.frames.push_back(static_cast<uint32_t>(k.trail.size()));
  kernel_assign(k, lit, k.level, DECISION_REASON, false);
}

static void unwind(Kernel &k, FailedLiteralPropagation propagation) {
  k.conflict_valid = false;
  k.conflict_ref = INVALID_REF;
  if (k.level > 0)
    kernel_backtrack(k, 0, /*update_phases=*/false);
  if (propagation == FailedLiteralPropagation::Full)
    kernel_probing_end(k);
}

static bool probe_one(Kernel &k, Lit p, Lit *unit_out,
                      FailedLiteralPropagation propagation,
                      const FailedLiteralStats &stats) {
  const bool lrat = k.proof && k.proof->lrat();
  k.stats.*stats.probed += 1;
  if (propagation == FailedLiteralPropagation::Full)
    kernel_probing_begin(k);
  assume(k, p);
  const bool conflict = propagation == FailedLiteralPropagation::Full
                            ? kernel_propagate_probing(k)
                            : kernel_propagate_binary(k);
  if (!conflict) {
    unwind(k, propagation);
    return false;
  }
  k.stats.*stats.failed += 1;
  const Lit unit = negate(p);
  uint64_t last = 0;
  std::vector<std::pair<uint32_t, uint64_t>> entries;
  if (lrat) {
    std::vector<uint8_t> seen(k.nvars, 0);
    last = kernel_conflict_reason_walk(k, seen, entries);
  }
  unwind(k, propagation);
  const uint64_t id = ++k.next_clause_id;
  if (k.proof) {
    if (lrat) {
      kernel_build_lrat_chain(entries, last, k.lrat_chain);
      k.proof->add_rup(id, &unit, 1, k.lrat_chain.data(),
                       static_cast<uint32_t>(k.lrat_chain.size()));
    } else {
      k.proof->add_rup(id, &unit, 1, nullptr, 0);
    }
  }
  k.unit_ids[var_of(unit)] = id;
  *unit_out = unit;
  return true;
}

// Analyze a binary conflict on the accumulated Backbone trail.  For an
// assignment `lit`, Tactix stores the *false* other endpoint of its binary
// reason in `Assigned::reason`.  Walking the trail backwards and stopping when
// that predecessor has already been reached is therefore the Kissat/CaDiCaL
// binary-UIP algorithm; the repeated false endpoint itself is the derived
// unit.  The reversed clause-id list is an LRAT RUP chain for that unit.
static Lit chained_binary_uip(Kernel &k, size_t speculative_begin,
                              std::vector<uint64_t> &chain) {
  assert(k.conflict_valid && k.conflict_ref == INVALID_REF);
  std::vector<uint8_t> seen(k.nvars, 0);
  seen[var_of(k.conflict_bin[0])] = 1;
  seen[var_of(k.conflict_bin[1])] = 1;
  chain.clear();
  if (k.proof && k.proof->lrat())
    chain.push_back(
        kernel_binary_id(k, k.conflict_bin[0], k.conflict_bin[1]));

  for (size_t i = k.trail.size(); i-- > speculative_begin;) {
    const Lit lit = k.trail[i];
    const Var v = var_of(lit);
    if (!seen[v])
      continue;
    const Assigned &a = k.assigned[v];
    // Backbone propagation suppresses reason jumping, so every implication on
    // this slice names its actual binary antecedent.  A decision/root marker
    // means no binary UIP was found; fail closed rather than emit a unit.
    if (!a.binary || a.jumped || a.reason == DECISION_REASON ||
        a.reason == UNIT_REASON)
      return INVALID_LIT;
    const Lit predecessor = a.reason;
    if (k.proof && k.proof->lrat())
      chain.push_back(kernel_binary_id(k, lit, predecessor));
    const Var pv = var_of(predecessor);
    if (!seen[pv]) {
      seen[pv] = 1;
      continue;
    }
    std::reverse(chain.begin(), chain.end());
    return predecessor;
  }
  return INVALID_LIT;
}

// Kissat's Backbone trail is deliberately separate from ordinary search
// backtracking: even a derived unit stamped at saved level zero remains
// speculative until the whole round finishes.  Clear the complete round
// suffix by position, then replay the accumulated unit batch at root.  A
// generic backtrack-to-zero would retain those temporary level-zero entries.
// The retraction itself is the kernel's, not this rule's: unassigning a
// literal has to move the ranking structures with the value and the counters.
static void rewind_chained_binary_round(Kernel &k, size_t speculative_begin,
                                        uint32_t unflushed_root_begin) {
  kernel_unwind_speculation(k, static_cast<uint32_t>(speculative_begin));
  // The round's level-zero stamps are retracted with everything else, so the
  // pending-root marker goes back to what the round inherited; the replay
  // below re-raises it for the units it actually commits.
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
  std::vector<uint64_t> lrat_chain;
  std::vector<Var> candidate_order;
  kernel_vars_in_import_order(k, candidate_order);

  // `flags.backbone[01]` is persistent in Kissat.  A computation offers its
  // surviving priority polarities first, then every other active polarity.
  // The local stack is retained between rounds: successful assumptions prune
  // it, while literals falsified by those assumptions become the next round's
  // candidates.  Rebuilding a fresh var/polarity scan each round loses both
  // pieces of the reference algorithm.
  std::vector<Lit> candidates;
  candidates.reserve(2 * candidate_order.size());
  for (int prioritized = 1; prioritized >= 0; prioritized--) {
    for (const Var v : candidate_order) {
      if (kernel_var_retired(k, v))
        continue;
      // Kissat's ACTIVE_VAR schedule excludes variables fixed at root.  They
      // are not retired in Tactix's representation, so filter them explicitly
      // when rebuilding a later computation instead of carrying two dead
      // polarities through its first round.
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
  // Scheduled-list rows share the dormant per-probe effort journal so one
  // campaign can compare candidate order and priority bits across solvers.
  if (const char *bt_path = std::getenv("TACTIX_BACKBONE_TICKS_TRACE")) {
    const char *bt_conflict = std::getenv("SAT_BACKBONE_CONFLICT");
    if (*bt_path &&
        (!bt_conflict || !*bt_conflict ||
         std::strtoull(bt_conflict, nullptr, 10) == k.stats.conflicts)) {
      static std::FILE *sched_out = nullptr;
      if (!sched_out) {
        const std::string sched_path = std::string(bt_path) + ".sched";
        sched_out = std::fopen(sched_path.c_str(), "w");
        if (sched_out) {
          std::setvbuf(sched_out, nullptr, _IOLBF, 0);
          std::fputs("# backbone-scheduled\t1\n", sched_out);
          std::fputs("# solver\tordinal\texternal\tprioritized\n", sched_out);
        }
      }
      if (sched_out)
        for (size_t i = 0; i < candidates.size(); i++) {
          const Lit lit = candidates[i];
          std::fprintf(sched_out, "tactix\t%zu\t%d\t%u\n", i,
                       kernel_externalize(k, lit),
                       !!(k.backbone_priority[var_of(lit)] &
                          (1u << negated(lit))));
        }
    }
  }
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
    // A proved unit and the id of its lemma. `Kernel::unit_ids[v]` must name
    // the clause for the literal that IS root-true (corpus 023), so the id
    // rides the batch and is installed at assert time, not at record time:
    // between the two the unit is only speculative.
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
#ifdef TACTIX_ENABLE_EVENT_TRACE
      // Standalone per-probe effort journal for budget-parity diagnosis;
      // dormant unless TACTIX_BACKBONE_TICKS_TRACE names a path.
      if (const char *bt_path = std::getenv("TACTIX_BACKBONE_TICKS_TRACE")) {
        const char *bt_conflict = std::getenv("SAT_BACKBONE_CONFLICT");
        if (*bt_path &&
            (!bt_conflict || !*bt_conflict ||
             std::strtoull(bt_conflict, nullptr, 10) == k.stats.conflicts)) {
          static std::FILE *bt_out = nullptr;
          static uint64_t bt_ordinal = 0;
          if (!bt_out) {
            bt_out = std::fopen(bt_path, "w");
            if (bt_out) {
              std::setvbuf(bt_out, nullptr, _IOLBF, 0);
              std::fputs("# backbone-probe-ticks\t1\n", bt_out);
              std::fputs("# solver\tordinal\texternal\tticks_delta\tbudget\n",
                         bt_out);
            }
          }
          if (bt_out)
            std::fprintf(bt_out, "tactix\t%llu\t%d\t%llu\t%llu\n",
                         (unsigned long long)bt_ordinal++,
                         kernel_externalize(k, p),
                         (unsigned long long)(k.stats.backbone_ticks -
                                              meter_begin),
                         (unsigned long long)k.work_budget);
        }
      }
#endif
      assume(k, p);
      const bool saved_jump_reasons = k.jump_reasons;
      k.jump_reasons = false;
      const bool conflict = kernel_propagate_binary(
          k, /*use_current_level=*/true);
      k.jump_reasons = saved_jump_reasons;
      if (!conflict)
        continue;

      retained.pop_back();
      const Lit unit = chained_binary_uip(k, speculative_begin, lrat_chain);
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
      if (k.proof) {
        if (k.proof->lrat())
          k.proof->add_rup(id, &unit, 1, lrat_chain.data(),
                           static_cast<uint32_t>(lrat_chain.size()));
        else
          k.proof->add_rup(0, &unit, 1, nullptr, 0);
      }
      units.emplace_back(unit, id);

      // The derived unit is sound, but remains part of the speculative
      // Backbone trail through the rest of this round.  In particular, a
      // falsified candidate sees the saved assumption level here, exactly as
      // in Kissat; committing at root early changes candidate retention.
      //
      // At saved level zero the stamp IS a root assertion, so the id belongs
      // here by the same assert-time rule: the binary closure propagated below
      // is then root-level too, and `kernel_emit_root_units` names this literal
      // as its antecedent.  A root literal whose id is still 0 would truncate
      // that chain.  Above the root the literal is not a root value and the id
      // stays with the batch until the replay commits it.
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

    // Match Kissat's post-round flush: candidates satisfied by the retained
    // trail are retired, while falsified non-root candidates survive in their
    // existing order for the next round.
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
          // The batch contradicts the root. Refuting here without the lemma
          // the two units resolve to leaves an UNSATISFIABLE answer whose
          // proof never derives the empty clause (corpus 023).  Read the
          // opposite literal's id BEFORE overwriting the entry.
          kernel_emit_empty_from_pair(k, proved.second, k.unit_ids[v]);
          k.unit_ids[v] = proved.second;
          return ApplyResult::Unsat;
        }
        // Assert time, not record time: only now is `unit` the root value.
        k.unit_ids[v] = proved.second;
        if (!value)
          kernel_assign_unit(k, unit);
      }
      // Kissat's `backbone_propagate` charges the technique-private meter,
      // but the post-round root replay uses `kissat_probing_propagate` and
      // charges only the shared probing/global meters.  The scheduler's sink
      // spans the complete Tactix rule call, so pause it around that replay.
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
  // The public candidate collector remains available for N1 enumeration, but
  // this canonical one-round application can consume the same var/polarity
  // order directly.  Units are deliberately committed only after the scan,
  // so probing one literal cannot change a later candidate's eligibility; a
  // per-variable local bit is therefore enough to suppress the opposite
  // polarity after a failed probe.  This removes two dense temporary arrays
  // at the post-preprocessing memory high-water mark.
  for (Var v = 0; v < k.nvars; v++) {
    if (k.value(make_lit(v, false)) != 0)
      continue;
    bool forced = false;
    for (int neg = 0; neg <= 1 && !forced; neg++) {
      // Probing propagation may derive a true level-zero fact below the
      // assumption.  Backtracking keeps it, so close that root debt before
      // starting another independent assumption episode.
      if (inprocess_reach_root(k))
        return ApplyResult::Unsat;
      const Lit p = make_lit(v, neg != 0);
      bool candidate = false;
      for (const WatchEntry w : k.watches.entries(negate(p)))
        if (w.binary()) {
          candidate = true;
          break;
        }
      if (!candidate || k.value(p) != 0)
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
      for (const WatchEntry w : k.watches.entries(negate(p)))
        if (w.binary()) {
          out[n++] = lit_instance(k, p);
          break;
        }
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
