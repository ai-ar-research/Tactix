// See backbone.hpp for the reference/deviation record. V2 Backbone is V1
// Probe with binary-only propagation; the structure mirrors probe.cpp and the
// per-rule scaffold (candidates, assume/unwind, the batch round) is duplicated
// per §5.3 self-containment — the ONLY semantic difference is the propagation
// engine (kernel_propagate_binary vs kernel_propagate). The proof-critical
// reason walk is the shared kernel helper, not duplicated.
#include "rules/backbone/backbone.hpp"

#include "proof/proof.hpp"

#include <algorithm>

namespace tactix {

bool backbone_guard(const Kernel &k) {
  // quiescent root (C1/C2/C3/V1's argument)
  return !k.inconsistent && !k.conflict_valid && !k.pending.valid &&
         k.level == 0 && k.propagate_head == k.trail.size();
}

// BIG-head candidates: assuming p propagates (through the binary graph) iff
// negate(p) carries a binary watch. Same set as V1 Probe; var-index
// ascending, positive phase first.
void backbone_candidates(const Kernel &k, std::vector<Lit> &out) {
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

// decision-free probe assumption / abandonment (Lucky/Vivify/Probe primitives)
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
}

// emit the add-RUP unit {lit} with the sorted reason chain closed by `last`
// (the conflicting binary's id). LRAT mode only; the caller handles DRAT.
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

// binary-probe one literal against the current clean root. If p fails, emit
// the {¬p} add-RUP unit, record its id, and return true with *unit_out set —
// NOT asserted (the caller asserts, keeping the root clean for the remaining
// probes; backbone.hpp deviation 8). If p survives, restore the root, false.
static bool backbone_one(Kernel &k, Lit p, Lit *unit_out) {
  const bool lrat = k.proof && k.proof->lrat();
  k.stats.backbone_probed++;
  assume(k, p);
  if (!kernel_propagate_binary(k)) { // no conflict: p not a failed literal
    unwind(k);
    return false;
  }
  k.stats.backbone_failed++;
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
ApplyResult backbone_apply(Kernel &k) {
  assert(backbone_guard(k));
  k.stats.backbone_rounds++;
  std::vector<Lit> candidates;
  backbone_candidates(k, candidates);
  std::vector<uint8_t> forced(k.nvars, 0);
  std::vector<Lit> units; // failed-literal units, asserted after the scan
  for (const Lit p : candidates) {
    if (forced[var_of(p)] || k.value(p) != 0)
      continue; // variable already forced this round / fixed at entry
    Lit unit;
    if (backbone_one(k, p, &unit)) {
      forced[var_of(p)] = 1;
      units.push_back(unit);
    }
  }
  if (units.empty())
    return ApplyResult::NoOp;
  // assert the batch at root, un-propagated; M1 Propagate runs each unit's
  // cascade (root-unit id emission + refutation on a both-fail conflict)
  for (const Lit u : units)
    kernel_assign_unit(k, u);
  return ApplyResult::Ok;
}

ApplyResult backbone_try(Kernel &k, Lit p) {
  assert(backbone_guard(k));
  if (var_of(p) >= k.nvars || k.value(p) != 0)
    return ApplyResult::NoOp;
  Lit unit;
  if (!backbone_one(k, p, &unit))
    return ApplyResult::NoOp;
  kernel_assign_unit(k, unit);
  return ApplyResult::Ok;
}

// instance surface: payload = probe literal, the shared lit_instance encoding
// (like Decide / V1 Probe); INSTANCE_CANONICAL = the full round
static uint32_t backbone_instances(Kernel &k, uint64_t *out, uint32_t cap) {
  std::vector<Lit> cands;
  backbone_candidates(k, cands);
  const uint32_t n =
      std::min<uint32_t>(cap, static_cast<uint32_t>(cands.size()));
  for (uint32_t i = 0; i < n; i++)
    out[i] = lit_instance(cands[i]);
  return n;
}

static ApplyResult backbone_apply_instance(Kernel &k, uint64_t inst) {
  if (inst == INSTANCE_CANONICAL)
    return backbone_apply(k);
  Lit p;
  if (!lit_of_instance(inst, &p))
    return ApplyResult::NoOp;
  return backbone_try(k, p);
}

TACTIX_REGISTER_RULE(Backbone, RuleTier::Inprocess, backbone_guard,
                     backbone_apply, backbone_instances,
                     backbone_apply_instance);

} // namespace tactix
