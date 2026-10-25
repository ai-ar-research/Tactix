// BVE, rule E1 (docs/dev/calculus.md §6b): eliminate a variable x by replacing
// every clause on x with the bounded non-tautological resolvents Occ⁺⊗Occ⁻,
// pushing the removed clauses onto the reconstruction stack ρ. Proof delta:
// add-RUP + delete + push-witness, RUP-only since a resolvent is RUP from its
// two parents. Reuses V3 Substitute's machinery wholesale.
// Full design: docs/design/e1-bve-design.md.
#pragma once

#include "rules/registry.hpp"

#include <vector>

namespace tactix {

// Enabled-gate bitmask for `Kernel::bve_gate_mask`. Every combination is sound
// — a disabled type only falls back to the full resolvent cross product — so
// this is a tuning knob, not an N1 applicability control. Default all on.
enum BveGate : uint32_t {
  BVE_GATE_EQUIV = 1u << 0,  // equivalence   x ↔ ¬y   (two binaries)
  BVE_GATE_AND_OR = 1u << 1, // AND/OR         x ↔ OR/AND(a₁…aₖ)  (base + binaries)
  BVE_GATE_ITE = 1u << 2,    // if-then-else   x ↔ (c ? t : e)   (four ternaries)
  BVE_GATE_XOR = 1u << 3,    // parity         x ↔ a₁⊕…⊕aₘ  (all 2^m clauses)
  // A general definition x ↔ φ, found by refuting the pivot's own reduced
  // occurrence lists in the sub-solver (kissat definition.c). The one
  // non-syntactic type, and the one whose gate×gate resolvents are not
  // tautologies, so it sets `resolve_gate`; see `bve_find_definition`.
  BVE_GATE_DEFINITION = 1u << 4,
};

// Gate families present in Kissat 4.0.4's elimination pipeline.  Tactix keeps
// its independently supported XOR detector selectable, but the shipped CLI
// strategy does not enable a recognizer whose reference has no analogue.
constexpr uint32_t BVE_GATE_KISSAT_DEFAULT =
    BVE_GATE_EQUIV | BVE_GATE_AND_OR | BVE_GATE_ITE | BVE_GATE_DEFINITION;

// XOR recognition enumerates all 2^m assignments.  The configurable strategy
// default is separate from this fixed-stack representation capacity; values
// outside the capacity are rejected by the typed invocation interface.
constexpr uint32_t BVE_XOR_ARITY_MIN = 2;
constexpr uint32_t BVE_XOR_ARITY_CAPACITY = 5;

// Pure quiescent-root precondition, D-LEVEL purified: no fixpoint conjunct,
// since the round drains the root closure U at apply entry.
RuleEligibility bve_guard(const Kernel &);

// Round-local occurrence index over the elimination clause set — irredundant
// large clauses plus all binaries — by literal, both polarities, active
// variables only. Rule-local by design, not a kernel dense mode
// (docs/design/e1-bve-design.md §1 B1). Flat counts only: the sole consumer is
// bve_schedule's ranking, so Ref/partner lists would be built and thrown away.
struct BveOccurrences {
  std::vector<uint32_t> counts; // by lit
  uint32_t count(Lit l) const { return counts[l]; }
};
void bve_build_occurrences(const Kernel &, BveOccurrences &);

// Pinned eliminate.c flushes every newly propagated root unit while the dense
// occurrence representation is still connected.  This retires satisfied large
// clauses and updates the mutable candidate heap before the next pop.
void bve_flush_units_while_connected(Kernel &, uint32_t trail_from);

// Mutable-schedule occurrence count in the current dense representation.
// During Kissat elimination, removed large-clause references remain connected
// until the round-level flush, while binaries are disconnected immediately.
// The policy heap therefore scores the raw dense list plus live binary watches,
// distinct from the legal/live occurrence count used by the public rule.
uint32_t bve_schedule_occurrence_count(const Kernel &, Lit);

// Candidate variables to eliminate, ranked by Kissat's policy-independent
// structural score occ(x)·occ(¬x)-occ(x)-occ(¬x), ascending. DefaultPolicy
// adds search relevancy and dynamic heap updates. Extension variables remain
// legal candidates: suppressing them would narrow N1's instance surface and
// diverge from Kissat, even though eliminating one can reverse Factor. The
// surface has one instance per candidate variable and does not consult retry
// (`Kernel::bve_retry`) even though this rule maintains that bit: reading it
// here would narrow the instance surface a policy enumerates, and "when to
// retry" is scheduling. See docs/dev/implementation-notes.md §6.
void bve_schedule(Kernel &, std::vector<Var> &out);
void bve_schedule_from_occurrences(const Kernel &, const BveOccurrences &,
                                   std::vector<Var> &out);

} // namespace tactix
