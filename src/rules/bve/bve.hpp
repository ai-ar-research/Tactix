// E1 BVE (plan §5.2): bounded variable elimination by resolution. Full design in
// `docs/e1-bve-design.md`. Eliminates a variable x by replacing all clauses on x
// with the non-tautological resolvents Occ⁺⊗Occ⁻ (bounded), pushing the removed
// clauses onto the reconstruction stack ρ. Proof delta: add-RUP (resolvents) +
// delete + push-witness — RUP-only, no add_rat (a resolvent is RUP from its two
// parents). Second variable-eliminating rule after V3 Substitute, which it
// reuses wholesale (ρ, add/delete, GC, the record_unit contradictory-units
// guard).
//
// Scope so far:
//   E1a (this commit) — the rule skeleton (purified quiescent-root guard,
//     registration) + the round-local occurrence index + candidate scheduling.
//     NO elimination yet: apply/apply_instance are NoOp, exactly as V3a shipped
//     detection-only. The index + schedule are the testable surface.
//   E1b (next) — resolution + bound + RUP resolvent emission + delete + witness.
//   E1c — syntactic gate extraction (gate×antecedent resolvent reduction).
//   E1d — fuzz-harden + regression corpus + micro proof tests.
#pragma once

#include "rules/registry.hpp"

#include <vector>

namespace tactix {

// Pure quiescent-root precondition (D-LEVEL purified, rev 43 — no fixpoint
// conjunct; the elimination round will drain the root closure U at apply entry).
bool bve_guard(const Kernel &);

// Round-local occurrence index over the elimination clause set — the IRREDUNDANT
// large clauses plus all binaries (binaries are irredundant-lifecycle here) —
// indexed by literal for BOTH polarities, over active-variable literals only.
// Rule-local by design (NOT a kernel "dense mode" — design §1 B1: the
// deferred-slot, correctness-first choice, matching Subsume/Strengthen's occs).
// Large clauses are arena Refs; a binary (l ∨ o) is recorded in `binary[l]` as
// the partner literal o (its id is kernel_binary_id(k, l, o)).
struct BveOccurrences {
  std::vector<std::vector<Ref>> large;  // by lit
  std::vector<std::vector<Lit>> binary; // by lit: partner of each (lit ∨ partner)
  uint32_t count(Lit l) const {
    return static_cast<uint32_t>(large[l].size() + binary[l].size());
  }
};
void bve_build_occurrences(const Kernel &, BveOccurrences &);

// Candidate variables to eliminate, ranked fewest-resolutions-first (ascending
// occ(x)·occ(¬x); Kissat `variable_score`): active vars that occur in the
// elimination set, skipping those whose occurrences blow up past the occurrence
// limit. The BVE instance space (each instance = one candidate variable).
void bve_schedule(const Kernel &, std::vector<Var> &out);

} // namespace tactix
