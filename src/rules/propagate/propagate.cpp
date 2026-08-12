// Propagate: a thin wrapper over the kernel propagation engine
// (kernel_propagate, a kissat proplit.h transliteration kept as a §5.3 kernel
// primitive so probing rules can share it). This rule owns the search
// semantics: the conflict count, the root-refutation proof delta, and the
// ApplyResult mapping.
#include "rules/propagate/propagate.hpp"

#include "proof/proof.hpp"

#include <cassert>

namespace tactix {

RuleEligibility propagate_guard(const Kernel &k) {
  // Pure abstract precondition (docs/calculus.md §3 M1): not refuted, no
  // unresolved conflict, propagation work remains. No ¬pending.valid conjunct:
  // with Analyze+Backjump fused, `pending` never crosses a rule boundary, so
  // the state it would exclude is unrepresentable (D-SPLIT-1 closed).
  RuleEligibility result;
  return result.fact("no-conflict", !k.conflict_valid)
      .fact("consistent", !k.inconsistent)
      .fact("pending-propagation", k.propagate_head < k.trail.size());
}

ApplyResult propagate_service(Kernel &k) {
  if (k.inconsistent)
    return ApplyResult::Unsat;
  const bool lrat = k.proof && k.proof->lrat();
  const size_t assigned_before = k.trail.size();
  const bool conflict = kernel_propagate(k);
  // Deliberately not gated on `k.level == 0`. A forced literal is stamped at
  // its true level (D-LEVEL-2), so a clause whose other literals are all
  // root-false forces a literal at level 0 while the solver stands higher;
  // gating on the solver's level would skip those, leave `unit_ids[v] == 0` —
  // LRAT's chain terminator — and truncate every later chain citing them.
  // `kernel_emit_root_units` applies the correct per-literal test. Only
  // reachable with chrono on, and invisible to DRAT, so an LRAT fuzz leg pins
  // it.
  if (lrat)
    kernel_emit_root_units(k, assigned_before);
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
  // `kissat_update_conflicts_and_trail` consumes the unflushed root prefix
  // after a successful root-level propagation.  Tactix keeps those literals
  // in its stable trail representation, but must consume the matching
  // lifecycle marker here.  Root facts discovered while the solver stands
  // above level zero remain pending until a later Reduce/Rephase/phase entry.
  if (k.level == 0)
    k.unflushed_root = 0;
  return ApplyResult::Ok;
}

// M1 Propagate's registered apply is exactly the propagation service.
ApplyResult propagate_apply(Kernel &k) { return propagate_service(k); }

TACTIX_REGISTER_RULE(Propagate, RuleTier::Micro, propagate_guard,
                     propagate_apply);

} // namespace tactix
