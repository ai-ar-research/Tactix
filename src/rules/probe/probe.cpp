// See probe.hpp for the reference/deviation record.
#include "rules/probe/probe.hpp"

#include "rules/propagate/propagate.hpp"

#include "proof/proof.hpp"

#include <algorithm>

namespace tactix {

bool probe_guard(const Kernel &k) {
  // quiescent root (C1/C2/C3's argument)
  return !k.inconsistent && !k.conflict_valid &&
         k.level == 0;
}

// BIG-head candidates: assuming p propagates iff negate(p) carries a binary
// watch (probe.hpp deviation 3). Var-index ascending, positive phase first.
void probe_candidates(const Kernel &k, std::vector<Lit> &out) {
  out.clear();
  for (Var v = 0; v < k.nvars; v++) {
    if (k.value(make_lit(v, false)) != 0)
      continue; // root-fixed variable
    for (int neg = 0; neg <= 1; neg++) {
      const Lit p = make_lit(v, neg != 0);
      const Lit nl = negate(p);
      const WatchHead &h = k.watches.heads[nl];
      const uint32_t *ws = k.watches.arena.data() + h.offset;
      bool big_head = false;
      for (uint32_t i = 0; i < h.size;) {
        if (Watch{ws[i]}.binary()) {
          big_head = true;
          break;
        }
        i += 2; // large watch: blocking word + ref word
      }
      if (big_head)
        out.push_back(p);
    }
  }
}

// decision-free probe assumption / abandonment (Lucky/Vivify primitives)
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
  // trail is back at the clean root: give search back the watch order and
  // `searched` hints this probe's propagation churned (kernel.hpp
  // kernel_probing_begin). Must run BEFORE the caller mutates the formula.
  kernel_probing_end(k);
}

// The reason walk (kernel_reason_walk) + conflict seed (kernel_conflict_
// reason_walk) are shared kernel helpers (kernel.cpp), used by C3 Vivify,
// V1 Probe and V2 Backbone — the proof-critical DFS lives in exactly one
// place (review 2026-07-15 #1).

// emit the add-RUP unit {lit} with the sorted reason chain closed by `last`
// (the conflicting clause id). LRAT mode only; the caller handles DRAT.
static void emit_unit_chain(Kernel &k, uint64_t id, Lit lit,
                            std::vector<std::pair<uint32_t, uint64_t>> &entries,
                            uint64_t last) {
  std::sort(entries.begin(), entries.end());
  std::vector<uint64_t> chain;
  chain.reserve(entries.size() + 1);
  for (const auto &e : entries)
    chain.push_back(e.second);
  chain.push_back(last);
  k.proof->add_rup(id, &lit, 1, chain.data(),
                   static_cast<uint32_t>(chain.size()));
}

// probe one literal against the current clean root. If p fails, emit the
// {¬p} add-RUP unit, record its id, and return true with *unit_out set — the
// unit is NOT asserted (the caller asserts it, so the root stays clean for
// the remaining probes; probe.hpp deviation 8). If p survives, restore the
// root and return false.
static bool probe_one(Kernel &k, Lit p, Lit *unit_out) {
  const bool lrat = k.proof && k.proof->lrat();
  k.stats.probed++;
  kernel_probing_begin(k); // speculative: bookkeeping rolled back by unwind()
  assume(k, p);
  if (!kernel_propagate_probing(k)) { // no conflict: p is not a failed literal
    unwind(k);
    return false;
  }
  k.stats.probe_failed++;
  const Lit unit = negate(p);
  uint64_t last = 0;
  std::vector<std::pair<uint32_t, uint64_t>> entries;
  if (lrat) {
    std::vector<uint8_t> seen(k.nvars, 0);
    last = kernel_conflict_reason_walk(k, seen, entries);
  }
  unwind(k); // back to the clean root; nothing asserted yet
  const uint64_t id = ++k.next_clause_id;
  if (k.proof) {
    if (lrat)
      emit_unit_chain(k, id, unit, entries, last);
    else
      k.proof->add_rup(0, &unit, 1, nullptr, 0);
  }
  k.unit_ids[var_of(unit)] = id;
  *unit_out = unit;
  return true;
}

// canonical instance: one full round over the BIG-head schedule
ApplyResult probe_apply(Kernel &k) {
  assert(probe_guard(k));
  // D-LEVEL guard purification: complete the root closure U if applied off a
  // fixpoint (no-op otherwise). A root conflict during the drain refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  k.stats.probe_rounds++;
  std::vector<Lit> candidates;
  probe_candidates(k, candidates);
  std::vector<uint8_t> forced(k.nvars, 0);
  std::vector<Lit> units; // failed-literal units, asserted after the scan
  for (const Lit p : candidates) {
    if (forced[var_of(p)] || k.value(p) != 0)
      continue; // variable already forced this round / fixed at entry
    Lit unit;
    if (probe_one(k, p, &unit)) {
      forced[var_of(p)] = 1;
      units.push_back(unit);
    }
  }
  if (units.empty())
    return ApplyResult::NoOp;
  // assert the batch at root, un-propagated; M1 Propagate runs each unit's
  // cascade (root-unit id emission + refutation on a both-fail conflict),
  // exactly as Strengthen/Vivify leave a derived unit for the policy
  for (const Lit u : units)
    kernel_assign_unit(k, u);
  return ApplyResult::Ok;
}

ApplyResult probe_try(Kernel &k, Lit p) {
  assert(probe_guard(k));
  if (var_of(p) >= k.nvars || k.value(p) != 0)
    return ApplyResult::NoOp;
  Lit unit;
  if (!probe_one(k, p, &unit))
    return ApplyResult::NoOp;
  kernel_assign_unit(k, unit);
  return ApplyResult::Ok;
}

// instance surface: payload = probe literal, the shared lit_instance encoding
// (like Decide); INSTANCE_CANONICAL = the full round
static uint32_t probe_instances(Kernel &k, uint64_t *out, uint32_t cap) {
  std::vector<Lit> cands;
  probe_candidates(k, cands);
  const uint32_t n = std::min<uint32_t>(cap, static_cast<uint32_t>(cands.size()));
  for (uint32_t i = 0; i < n; i++)
    out[i] = lit_instance(cands[i]);
  return n;
}

static ApplyResult probe_apply_instance(Kernel &k, uint64_t inst) {
  // D-LEVEL guard purification: complete the root closure U if applied off a
  // fixpoint (no-op otherwise). A root conflict during the drain refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  if (inst == INSTANCE_CANONICAL)
    return probe_apply(k);
  Lit p;
  if (!lit_of_instance(inst, &p))
    return ApplyResult::NoOp;
  return probe_try(k, p);
}

TACTIX_REGISTER_RULE(Probe, RuleTier::Inprocess, probe_guard, probe_apply,
                     probe_instances, probe_apply_instance);

} // namespace tactix
