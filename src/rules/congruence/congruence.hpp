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

enum class GateKind : uint8_t { And, Xor, Ite };

// Extracted gate, output = f(inputs), where f is the kind:
//   And: output = AND(inputs), long base clause (output ∨ ¬in₁ ∨ … ∨ ¬inₖ) plus a
//        binary (¬output ∨ inputᵢ) per input. OR is the dual (found by scanning
//        every polarity). `inputs` sorted; `input_bin_ids` the (¬output∨inputᵢ) ids.
//   Xor: output = in₁ ⊕ … ⊕ inₖ, encoded by all 2^k parity clauses (each size k+1).
//        `inputs` stored sorted & ALL-POSITIVE — the clause parity is absorbed into
//        `output`'s sign. `side_ids` holds the 2^k side-clause ids (parity-walk
//        order) for the match proof chain. The structural key is (kind, sorted
//        representative inputs); XOR re-canonicalization folds input-sign into the
//        output parity under the union-find. base_id is the all-positive base.
//   Ite: output = cond ? then : else, `inputs` = [cond, then, else] in that fixed
//        order (NOT sorted — the key is the normalized triple; normalize_ite puts
//        cond & then positive, absorbing any flip into the output sign).
struct CongGate {
  GateKind kind = GateKind::And;
  Lit output = 0;
  std::vector<Lit> inputs;             // canonical (sorted) — the structural key
  uint64_t base_id = 0;                // AND/XOR base clause proof id
  std::vector<uint64_t> input_bin_ids; // And: (¬output ∨ inputᵢ) ids ∥ inputs
  std::vector<uint64_t> side_ids;      // Xor: the 2^k parity side-clause ids
};

// Extract AND/OR gates from the irredundant large clauses (bounded arity). Rule-
// local; uses the arena scan (dense index is per-literal, not all-clauses).
void congruence_extract(const Kernel &, std::vector<CongGate> &out);

bool congruence_guard(const Kernel &); // pure quiescent-root precondition

} // namespace tactix
