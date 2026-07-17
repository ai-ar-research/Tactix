// See vivify.hpp for the reference/deviation record.
#include "rules/vivify/vivify.hpp"

#include "proof/proof.hpp"

#include <algorithm>

namespace tactix {

bool vivify_guard(const Kernel &k) {
  // quiescent root (C1/C2's argument)
  return !k.inconsistent && !k.conflict_valid &&
         k.level == 0 && k.propagate_head == k.trail.size();
}

// candidate: live, 3 <= size <= lim, not root-satisfied; irredundant or
// redundant within the LCM tier scope
static bool is_candidate(const Kernel &k, const Clause *c) {
  if (c->garbage() || c->size > VIVIFY_CLSLIM || c->size < 3)
    return false;
  if (c->redundant() && c->glue() > VIVIFY_TIER2)
    return false;
  for (const Lit l : *c)
    if (k.value(l) > 0)
      return false;
  return true;
}

void vivify_candidates(const Kernel &k, std::vector<Ref> &out) {
  out.clear();
  struct Cand {
    Ref ref;
    uint32_t redundant, size;
  };
  std::vector<Cand> cands;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (is_candidate(k, c))
      cands.push_back({ref, c->redundant() ? 1u : 0u, c->size});
    ref += clause_words(c->size);
  }
  std::sort(cands.begin(), cands.end(), [](const Cand &a, const Cand &b) {
    if (a.redundant != b.redundant)
      return a.redundant < b.redundant; // irredundant first
    if (a.size != b.size)
      return a.size < b.size;
    return a.ref < b.ref; // total order for determinism
  });
  out.reserve(cands.size());
  for (const Cand &c : cands)
    out.push_back(c.ref);
}

// decision-free probe assumption / abandonment (Lucky's primitives)
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

// The reason walk (kernel_reason_walk) and conflict seed (kernel_conflict_
// reason_walk) are shared kernel helpers (kernel.cpp) — the proof-critical
// DFS lives in exactly one place, used by C3 Vivify, V1 Probe and V2
// Backbone (review 2026-07-15 #1).

// last == 0 means the sorted entries already end in the conflicting hint
// (the implied literal's own reason); otherwise `last` closes the chain
static void emit_chain(Kernel &k, uint64_t id, const std::vector<Lit> &lits,
                       std::vector<std::pair<uint32_t, uint64_t>> &entries,
                       uint64_t last) {
  std::sort(entries.begin(), entries.end());
  std::vector<uint64_t> chain;
  chain.reserve(entries.size() + 1);
  for (const auto &e : entries)
    chain.push_back(e.second);
  if (last)
    chain.push_back(last);
  k.proof->add_rup(id, lits.data(), static_cast<uint32_t>(lits.size()),
                   chain.data(), static_cast<uint32_t>(chain.size()));
}

// replace `c` (at cref) by `lits` with proof add before delete. Returns
// ApplyResult::Unsat for an empty rewrite, Ok otherwise. `entries`+`last`
// form the LRAT chain; ignored when not in LRAT mode.
static ApplyResult rewrite(Kernel &k, Ref cref, const std::vector<Lit> &lits,
                           std::vector<std::pair<uint32_t, uint64_t>> &entries,
                           uint64_t last, bool *ended_round) {
  Clause *c = clause_at(k.arena, cref);
  const bool lrat = k.proof && k.proof->lrat();
  const bool redundant = c->redundant();
  const uint32_t glue =
      std::min<uint32_t>(c->glue(), static_cast<uint32_t>(lits.size()));
  const uint64_t id = ++k.next_clause_id;
  if (k.proof) {
    if (lrat)
      emit_chain(k, id, lits, entries, last);
    else
      k.proof->add_rup(0, lits.data(), static_cast<uint32_t>(lits.size()),
                       nullptr, 0);
    k.proof->del(id, c->id(), c->lits, c->size);
  }
  c->flags |= Clause::GARBAGE;
  k.stats.vivified++;
  if (lits.empty()) { // under-derived root only (corpus 002)
    k.inconsistent = true;
    return ApplyResult::Unsat;
  }
  if (lits.size() == 1) {
    k.unit_ids[var_of(lits[0])] = id;
    kernel_assign_unit(k, lits[0]);
    *ended_round = true; // values changed: the round's reasoning is stale
    return ApplyResult::Ok;
  }
  if (lits.size() == 2) {
    kernel_add_binary_clause(k, lits[0], lits[1], id);
    return ApplyResult::Ok;
  }
  kernel_add_large_clause(k, lits.data(), static_cast<uint32_t>(lits.size()),
                          redundant, glue, id);
  return ApplyResult::Ok;
}

// probe one candidate; on an actionable outcome, rewrite. Returns NoOp
// (nothing found), Ok (rewritten/deleted), or Unsat.
static ApplyResult vivify_one(Kernel &k, Ref cref, bool *ended_round) {
  Clause *c = clause_at(k.arena, cref);
  if (!is_candidate(k, c)) {
    // re-check root satisfaction for deletion (a satisfied candidate is
    // dead weight; kissat/CaDiCaL delete it here)
    if (!c->garbage() && c->size >= 3) {
      bool satisfied = false;
      for (const Lit l : *c)
        satisfied |= (k.value(l) > 0 && k.var_level(var_of(l)) == 0);
      if (satisfied &&
          (!c->redundant() || c->glue() <= VIVIFY_TIER2)) {
        if (k.proof)
          k.proof->del(k.next_clause_id, c->id(), c->lits, c->size);
        c->flags |= Clause::GARBAGE;
        k.stats.vivify_deleted++;
        return ApplyResult::Ok;
      }
    }
    return ApplyResult::NoOp;
  }
  k.stats.vivify_checked++;
  const bool lrat = k.proof && k.proof->lrat();
  // probe literals: the non-root-falsified ones, clause order
  std::vector<Lit> probes;
  std::vector<uint64_t> absorbed; // unit ids of root-falsified lits (LRAT)
  probes.reserve(c->size);
  for (const Lit l : *c) {
    if (k.value(l) < 0 && k.var_level(var_of(l)) == 0) {
      if (lrat)
        absorbed.push_back(k.unit_ids[var_of(l)]);
      continue;
    }
    probes.push_back(l);
  }
  if (probes.size() <= 2)
    return ApplyResult::NoOp; // effectively binary: C2 territory
  const uint64_t cid = c->id();
  std::vector<std::pair<uint32_t, uint64_t>> entries;
  std::vector<uint8_t> seen; // lrat-gated (review #9): only the reason walk
  if (lrat)                  // reads it, so a proofs-off round pays nothing
    seen.assign(k.nvars, 0);
  std::vector<Lit> next;
  ApplyResult res = ApplyResult::NoOp;
  for (uint32_t i = 0; i < probes.size(); i++) {
    const Lit li = probes[i];
    const int8_t v = k.value(li);
    if (v > 0) {
      // implied: (probes[0..i-1], li) subsumes the candidate — a strict
      // strengthening since i + 1 < probes.size() (v > 0 needs at least
      // one assumption, and the LAST probe reaching here means every
      // earlier one was assumed, i.e. i+1 == size only if li was
      // assigned by assumption — excluded by v > 0 with li unassumed)
      if (i + 1 >= probes.size())
        break; // no strict gain; defensive
      next.assign(probes.begin(), probes.begin() + i);
      next.push_back(li);
      // chain: li's derivation alone — its own reason (the max-trail
      // entry) is the conflicting hint under the ¬li assumption; the
      // candidate is NOT cited (a hint after the conflict is illegal)
      if (lrat)
        kernel_reason_walk(k, var_of(li), seen, entries);
      unwind(k);
      res = rewrite(k, cref, next, entries, 0, ended_round);
      break;
    }
    if (v < 0) {
      // implied negation: li is redundant in the candidate
      next.assign(probes.begin(), probes.end());
      next.erase(next.begin() + i);
      if (lrat) {
        kernel_reason_walk(k, var_of(li), seen, entries);
        for (const uint64_t u : absorbed)
          entries.emplace_back(0, u);
      }
      unwind(k);
      res = rewrite(k, cref, next, entries, cid, ended_round);
      break;
    }
    assume(k, negate(li));
    if (kernel_propagate(k, cref)) {
      // probe conflict: (probes[0..i]) is RUP — strict gain iff it drops
      // literals (later probes or root-falsified originals)
      const bool gain = i + 1 < probes.size() || probes.size() < c->size;
      if (!gain) {
        unwind(k);
        break;
      }
      next.assign(probes.begin(), probes.begin() + i + 1);
      uint64_t last = 0;
      if (lrat)
        last = kernel_conflict_reason_walk(k, seen, entries);
      unwind(k);
      res = rewrite(k, cref, next, entries, last, ended_round);
      break;
    }
  }
  if (k.level > 0 || k.conflict_valid)
    unwind(k);
  return res;
}

// canonical instance: one full round over the tier-aware schedule
ApplyResult vivify_apply(Kernel &k) {
  assert(vivify_guard(k));
  k.stats.vivify_rounds++;
  std::vector<Ref> candidates;
  vivify_candidates(k, candidates);
  uint64_t changed = 0;
  bool ended = false;
  for (const Ref cref : candidates) {
    const ApplyResult r = vivify_one(k, cref, &ended);
    if (r == ApplyResult::Unsat)
      return ApplyResult::Unsat;
    if (r == ApplyResult::Ok)
      changed++;
    if (ended)
      break;
  }
  if (!changed)
    return ApplyResult::NoOp;
  kernel_collect_garbage(k);
  return ApplyResult::Ok;
}

ApplyResult vivify_try(Kernel &k, Ref candidate) {
  assert(vivify_guard(k));
  bool ended = false;
  const ApplyResult r = vivify_one(k, candidate, &ended);
  if (r == ApplyResult::Ok)
    kernel_collect_garbage(k);
  return r;
}

static uint32_t vivify_instances(Kernel &k, uint64_t *out, uint32_t cap) {
  std::vector<Ref> candidates;
  vivify_candidates(k, candidates);
  const uint32_t n =
      std::min<uint32_t>(cap, static_cast<uint32_t>(candidates.size()));
  for (uint32_t i = 0; i < n; i++)
    out[i] = candidates[i];
  return n;
}

static ApplyResult vivify_apply_instance(Kernel &k, uint64_t inst) {
  if (inst == INSTANCE_CANONICAL)
    return vivify_apply(k);
  for (Ref ref = 0; ref < k.arena.size();) {
    if (ref == inst)
      return vivify_try(k, ref);
    if (ref > inst)
      break;
    ref += clause_words(clause_at(k.arena, ref)->size);
  }
  return ApplyResult::NoOp;
}

TACTIX_REGISTER_RULE(Vivify, RuleTier::Inprocess, vivify_guard,
                     vivify_apply, vivify_instances, vivify_apply_instance);

} // namespace tactix
