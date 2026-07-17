// M1 Propagate: a thin wrapper over the kernel propagation engine
// (kernel_propagate, kissat proplit.h transliteration — moved into the
// kernel 2026-07-14 as a reviewed §5.3 primitive so probing rules can
// share it). This rule owns the search semantics: the conflict COUNT,
// the root-refutation proof delta, and the ApplyResult mapping.
#include "rules/propagate/propagate.hpp"

#include "proof/proof.hpp"

#include <algorithm>

namespace tactix {

bool propagate_guard(const Kernel &k) {
  // !pending.valid (plan rev 35, corpus 006): the Analyze -> Backjump learned-
  // clause handoff must be CONSUMED before propagation resumes. Propagating an
  // unpropagated trail suffix while a clause is pending can find a SECOND
  // conflict; that state (conflict_valid && pending.valid) blocks Backjump
  // (which needs !conflict_valid) and leaves only Analyze applicable, which
  // OVERWRITES pending. The first learned clause is then orphaned: already
  // allocated, watched and emitted to the proof, but its asserting literal is
  // never assigned — and since propagation is watch-driven, nothing ever
  // revisits it. It sits unit-with-unassigned at the next fixpoint (an escaped
  // unit, the corpus-004/005 family). Blocking Propagate here makes
  // (conflict_valid && pending.valid) unreachable rather than merely
  // unpleasant; Backjump is always applicable in its place, so no schedule
  // loses progress and N1 is intact. Never fires under DefaultPolicy, which
  // always backjumps before propagating again.
  return !k.conflict_valid && !k.inconsistent && !k.pending.valid &&
         k.propagate_head < k.trail.size();
}

// LRAT: every root-level propagation-derived literal becomes an explicit
// unit clause in the proof (CaDiCaL's scheme) so later chains can cite it
static void emit_root_units(Kernel &k, size_t from) {
  for (size_t i = from; i < k.trail.size(); i++) {
    const Lit lit = k.trail[i];
    const Var v = var_of(lit);
    const Assigned &a = k.assigned[v];
    if (a.level != 0 || a.reason == UNIT_REASON ||
        a.reason == DECISION_REASON)
      continue; // asserted units got their ids at the source
    k.lrat_entries.clear();
    uint64_t reason_id;
    if (a.binary) {
      reason_id = kernel_binary_id(k, lit, a.reason);
      const Var ov = var_of(a.reason);
      k.lrat_entries.emplace_back(k.assigned[ov].trail, k.unit_ids[ov]);
    } else {
      const Clause *c = clause_at(k.arena, a.reason);
      reason_id = c->id();
      for (const Lit l : *c)
        if (l != lit) {
          const Var ov = var_of(l);
          k.lrat_entries.emplace_back(k.assigned[ov].trail,
                                      k.unit_ids[ov]);
        }
    }
    kernel_lrat_chain(k, reason_id);
    const uint64_t id = ++k.next_clause_id;
    k.unit_ids[v] = id;
    k.proof->add_derived_unit(id, lit, k.lrat_chain.data(),
                              static_cast<uint32_t>(k.lrat_chain.size()));
  }
}

ApplyResult propagate_apply(Kernel &k) {
  if (k.inconsistent)
    return ApplyResult::Unsat;
  const bool lrat = k.proof && k.proof->lrat();
  const size_t assigned_before = k.trail.size();
  const bool conflict = kernel_propagate(k);
  if (lrat && k.level == 0)
    emit_root_units(k, assigned_before);
  if (conflict) {
    k.stats.conflicts++;
    if (k.level == 0) { // root conflict: the formula is refuted
      k.inconsistent = true;
      k.conflict_valid = false;
      if (k.proof) {
        if (lrat) { // chain: the falsified clause's lits' units + itself
          k.lrat_entries.clear();
          uint64_t falsified_id;
          if (k.conflict_ref == INVALID_REF) {
            falsified_id =
                kernel_binary_id(k, k.conflict_bin[0], k.conflict_bin[1]);
            for (int i = 0; i < 2; i++) {
              const Var v = var_of(k.conflict_bin[i]);
              k.lrat_entries.emplace_back(k.assigned[v].trail,
                                          k.unit_ids[v]);
            }
          } else {
            const Clause *c = clause_at(k.arena, k.conflict_ref);
            falsified_id = c->id();
            for (const Lit l : *c) {
              const Var v = var_of(l);
              k.lrat_entries.emplace_back(k.assigned[v].trail,
                                          k.unit_ids[v]);
            }
          }
          kernel_lrat_chain(k, falsified_id);
          k.proof->add_empty(++k.next_clause_id, k.lrat_chain.data(),
                             static_cast<uint32_t>(k.lrat_chain.size()));
        } else {
          k.proof->add_empty(0, nullptr, 0);
        }
      }
      return ApplyResult::Unsat;
    }
    return ApplyResult::Conflict;
  }
  return ApplyResult::Ok;
}

TACTIX_REGISTER_RULE(Propagate, RuleTier::Micro, propagate_guard,
                     propagate_apply);

} // namespace tactix
