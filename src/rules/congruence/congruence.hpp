// V6 Congruence — gate congruence closure over AND/XOR/ITE gates: two gates
// with the same canonical inputs have equivalent outputs, so add the two
// equivalence binaries. It does not substitute; V3 Substitute, run next in the
// phase, collapses the equivalences. Precondition, effect, proof delta and
// soundness: `docs/calculus.md` V6 (and V6a for the binary-resolution unit
// channel); deviations: `docs/implementation-notes.md` §2; transliterated from
// kissat congruence.c (MIT).
#pragma once

#include "rules/registry.hpp"

#include <cstdint>
#include <vector>

namespace tactix {

enum class GateKind : uint8_t { And, Xor, Ite };

// Extracted gate, output = f(inputs) per kind (`docs/calculus.md` V6 gives each
// kind's defining clauses). Storage conventions the closure relies on:
//   And: `inputs` sorted, `input_bin_ids` the (¬output ∨ inputᵢ) ids, parallel.
//   Xor: `inputs` sorted and all-positive — the clause parity is absorbed into
//        `output`'s sign; `base_id` is the all-positive base.
//   Ite: `inputs` = [cond, then, else] in that fixed order, not sorted;
//        `normalize_ite` puts cond and then positive, absorbing any flip into
//        the output sign.
struct CongGate {
  GateKind kind = GateKind::And;
  Lit output = 0;
  std::vector<Lit> inputs;             // canonical (sorted) — the structural key
  uint64_t base_id = 0;                // AND/XOR base clause proof id
  std::vector<uint64_t> input_bin_ids; // And: (¬output ∨ inputᵢ) ids ∥ inputs
};

// Extract AND/OR gates from the irredundant large clauses (bounded arity). Rule-
// local; uses the arena scan (dense index is per-literal, not all-clauses).
void congruence_extract(const Kernel &, std::vector<CongGate> &out);

// The extraction half of `congruence_apply`: AND gates always, XOR and ITE when
// `with_xor_ite`. Exposed so a test can pin the `output` sign convention
// (output must evaluate to f(inputs)): the closure only compares two gates'
// outputs to each other, where a shared mis-signing cancels, but a unit/merge
// channel reading that polarity has no such luxury.
void congruence_extract_gates(const Kernel &, std::vector<CongGate> &out,
                              bool with_xor_ite);

bool congruence_guard(const Kernel &); // pure quiescent-root precondition

// The canonical full-round apply: extract AND/XOR/ITE gates and run the union-find
// congruence closure, adding equivalence binaries (+ proof). Exposed for tests.
ApplyResult congruence_apply(Kernel &);

// Replay-based RUP chain builder: assume the negation of `lits` at a probing
// scope, propagate, and read the antecedents off the conflict's reason walk, so
// the chain comes out in the checker's own propagation order by construction.
// Returns false when the lemma is not RUP over the current DB, in which case
// the caller must emit nothing. Shared with Sweep's RUP fast path.
bool congruence_replay_rup(Kernel &, const Lit *lits, uint32_t n,
                           std::vector<uint64_t> &chain);

} // namespace tactix
