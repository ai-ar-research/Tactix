// BVE, rule E1 (docs/dev/calculus.md §6b): replace every clause on x with the
// bounded non-tautological resolvents Occ⁺⊗Occ⁻ and push the removed clauses
// onto ρ. Proof delta: add-RUP + delete + push-witness.
// Design: docs/design/e1-bve-design.md.
#pragma once

#include "rules/registry.hpp"

#include <vector>

namespace tactix {

// Gate mask for `Kernel::bve_gate_mask`; every combination is sound (a tuning
// knob). Default all on.
enum BveGate : uint32_t {
  BVE_GATE_EQUIV = 1u << 0,  // equivalence   x ↔ ¬y   (two binaries)
  BVE_GATE_AND_OR = 1u << 1, // AND/OR         x ↔ OR/AND(a₁…aₖ)  (base + binaries)
  BVE_GATE_ITE = 1u << 2,    // if-then-else   x ↔ (c ? t : e)   (four ternaries)
  BVE_GATE_XOR = 1u << 3,    // parity         x ↔ a₁⊕…⊕aₘ  (all 2^m clauses)
  // General definition x ↔ φ via the sub-solver (kissat definition.c); its
  // gate×gate resolvents are real, so it sets `resolve_gate`.
  BVE_GATE_DEFINITION = 1u << 4,
};

// Gate families in Kissat 4.0.4's pipeline (XOR stays selectable).
constexpr uint32_t BVE_GATE_KISSAT_DEFAULT =
    BVE_GATE_EQUIV | BVE_GATE_AND_OR | BVE_GATE_ITE | BVE_GATE_DEFINITION;

// XOR enumerates 2^m assignments; arity is bounded by the stack capacity.
constexpr uint32_t BVE_XOR_ARITY_MIN = 2;
constexpr uint32_t BVE_XOR_ARITY_CAPACITY = 5;

// Pure quiescent-root precondition, D-LEVEL purified: no fixpoint conjunct,
// since the round drains the root closure U at apply entry.
RuleEligibility bve_guard(const Kernel &);

// Round-local flat occurrence counts (irredundant large clauses plus binaries,
// active variables) for bve_schedule's ranking (design §1 B1).
struct BveOccurrences {
  std::vector<uint32_t> counts; // by lit
  uint32_t count(Lit l) const { return counts[l]; }
};
void bve_build_occurrences(const Kernel &, BveOccurrences &);

// eliminate.c flushes new root units while dense occurrences are connected.
void bve_flush_units_while_connected(Kernel &, uint32_t trail_from);

// Schedule occurrence count: raw dense list plus live binary watches (Kissat
// keeps removed large refs connected until the round flush).
uint32_t bve_schedule_occurrence_count(const Kernel &, Lit);

// Candidates ranked by Kissat's structural score occ(x)·occ(¬x)-occ(x)-occ(¬x)
// ascending; DefaultPolicy adds relevancy. Extension variables and the retry
// bit do not narrow the surface (N1; implementation-notes §6).
void bve_schedule(Kernel &, std::vector<Var> &out);
void bve_schedule_from_occurrences(const Kernel &, const BveOccurrences &,
                                   std::vector<Var> &out);

} // namespace tactix
