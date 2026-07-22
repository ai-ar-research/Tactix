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

// Enabled-gate bitmask for `Kernel::bve_gate_mask` (E1d): which syntactic gate
// types `bve_detect_gate` may recognize. Every combination is SOUND — a disabled
// type only falls BVE back to the full resolvent cross product, never changes a
// verdict — so this is a pure performance/tuning knob (and a future RL
// action-space axis), not an N1 applicability control. Default `~0u` = all on.
enum BveGate : uint32_t {
  BVE_GATE_EQUIV = 1u << 0,  // equivalence   x ↔ ¬y   (two binaries)
  BVE_GATE_AND_OR = 1u << 1, // AND/OR         x ↔ OR/AND(a₁…aₖ)  (base + binaries)
};

// Pure quiescent-root precondition (D-LEVEL purified, rev 43 — no fixpoint
// conjunct; the elimination round will drain the root closure U at apply entry).
// Maximum growth slack (kissat option eliminatebound). `Kernel::bve_bound`
// defaults to this; a policy ramps it 0,1,2,4,8,16 for kissat's fine-grained
// dose. Keep in step with the Kernel field's default.
constexpr uint32_t BVE_MAX_BOUND = 16;

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
//
// It does NOT skip on the retry filter (`Kernel::bve_retry`, kissat's
// flags->eliminate), even though this rule MAINTAINS that bit: reading it here
// would narrow the instance surface RandomPolicy and the §5.6 learned policy
// enumerate, and "when to retry" is scheduling, which belongs to the policy.
// DefaultPolicy declines the filtered variables in its dosing loop instead.
//
// COMPLETENESS of the filter (why it does not cost eliminations, and why N1
// holds). Whether `bve_eliminate(x)` succeeds is a function of exactly: the
// irredundant clauses containing x or ¬x, the root values of the literals in
// them (root-true clauses are delete-only, root-false literals are stripped),
// and the static configuration (`bve_gate_mask`, the bound/limits). Every
// mutation of that function's input sets the bit again:
//   - an irredundant clause on x added or deleted (kernel_add_large_clause /
//     kernel_add_binary_clause / kernel_mark_garbage / kernel_delete_binary*) —
//     which also covers a literal being removed from a clause, since every rule
//     that shortens a clause (Strengthen, Substitute, Vivify) does it as a
//     delete + add of the rewritten clause, not in place;
//   - a literal in such a clause becoming root-fixed
//     (kernel_bve_retry_sync_fixed, called by the policy before it doses BVE);
//   - a fresh variable (kernel_reserve_vars initialises the bit to 1).
// So a cleared bit means "the same attempt would run on the same inputs and
// return the same NoOp". Not scheduling state: the bit records a fact about the
// formula, and it gates nothing here — `instances()` offers every schedulable
// variable and `apply_instance` accepts any variable payload, so any policy can
// eliminate a filtered-out variable at any time (N1).
void bve_schedule(Kernel &, std::vector<Var> &out);

} // namespace tactix
