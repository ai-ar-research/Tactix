// V6 Congruence — gate congruence closure (design `docs/v6-congruence-design.md`).
// Extracts AND/OR gate definitions, structurally hashes them by (sorted inputs),
// and when two gates share inputs — so their outputs are equivalent — ADDS the two
// equivalence binaries (¬a∨b),(a∨¬b) to the formula (proof-delta add-RUP; the
// AND/OR equivalence binaries are single-step RUP from the gate clauses). It does
// NOT substitute: V3 Substitute, run next in the phase, collapses the equivalences
// (its SCC now sees a↔b in the binary graph). Transliterated from kissat
// congruence.c (MIT). XOR/ITE gates + their bounded RUP lemma chains are a later
// increment.
#pragma once

#include "rules/registry.hpp"

#include <cstdint>
#include <vector>

namespace tactix {

// Extracted AND gate: output = AND(inputs), encoded by a long base clause
// (output ∨ ¬input₁ ∨ … ∨ ¬inputₖ) plus a binary (¬output ∨ inputᵢ) per input.
// The ids are for the equivalence RUP proof. OR gates are the dual and are found
// by the same scan across polarities.
struct CongGate {
  Lit output = 0;
  std::vector<Lit> inputs;             // canonical (sorted) — the structural key
  uint64_t base_id = 0;                // long base clause's proof id
  std::vector<uint64_t> input_bin_ids; // (¬output ∨ inputᵢ) ids, parallel to inputs
};

// Extract AND/OR gates from the irredundant large clauses (bounded arity). Rule-
// local; uses the arena scan (dense index is per-literal, not all-clauses).
void congruence_extract(const Kernel &, std::vector<CongGate> &out);

bool congruence_guard(const Kernel &); // pure quiescent-root precondition

} // namespace tactix
