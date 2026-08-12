// BVE, rule E1 (docs/calculus.md §6b): eliminate a variable x by replacing
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

// XOR arity ceiling (cadical `elimxorlim`, default 5): a parity relation over
// the pivot and m inputs needs all 2^m clauses. Any value is sound.
constexpr uint32_t BVE_XOR_ARITY_LIMIT = 5;

// Maximum growth slack (kissat option eliminatebound). `Kernel::bve_bound`
// defaults to this; a policy ramps it 0,1,2,4,8,16 for kissat's fine-grained
// dose. Keep in step with the Kernel field's default.
constexpr uint32_t BVE_MAX_BOUND = 16;

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

// Candidate variables to eliminate, ranked fewest-resolutions-first (ascending
// occ(x)·occ(¬x); kissat `variable_score`) — the BVE instance space, one
// instance per candidate variable. It does not consult the retry filter
// (`Kernel::bve_retry`) even though this rule maintains that bit: reading it
// here would narrow the instance surface a policy enumerates, and "when to
// retry" is scheduling. See docs/implementation-notes.md §6.
void bve_schedule(Kernel &, std::vector<Var> &out);

} // namespace tactix
