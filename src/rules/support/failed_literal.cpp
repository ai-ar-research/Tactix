#include "rules/support/failed_literal.hpp"

#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"

#include <algorithm>

namespace tactix {

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
      k.proof->add_rup(0, &unit, 1, nullptr, 0);
    }
  }
  k.unit_ids[var_of(unit)] = id;
  *unit_out = unit;
  return true;
}

ApplyResult failed_literal_apply(Kernel &k,
                                 FailedLiteralPropagation propagation,
                                 const FailedLiteralStats &stats) {
  assert(failed_literal_guard(k));
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  k.stats.*stats.rounds += 1;
  std::vector<Lit> candidates;
  failed_literal_candidates(k, candidates);
  std::vector<uint8_t> forced(k.nvars, 0);
  std::vector<Lit> units;
  for (const Lit p : candidates) {
    if (forced[var_of(p)] || k.value(p) != 0)
      continue;
    Lit unit;
    if (probe_one(k, p, &unit, propagation, stats)) {
      forced[var_of(p)] = 1;
      units.push_back(unit);
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
  if (var_of(p) >= k.nvars || k.value(p) != 0)
    return ApplyResult::NoOp;
  Lit unit;
  if (!probe_one(k, p, &unit, propagation, stats))
    return ApplyResult::NoOp;
  kernel_assign_unit(k, unit);
  return ApplyResult::Ok;
}

uint32_t failed_literal_instances(Kernel &k, uint64_t *out, uint32_t cap) {
  std::vector<Lit> candidates;
  failed_literal_candidates(k, candidates);
  const uint32_t n =
      std::min<uint32_t>(cap, static_cast<uint32_t>(candidates.size()));
  for (uint32_t i = 0; i < n; i++)
    out[i] = lit_instance(k, candidates[i]);
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
