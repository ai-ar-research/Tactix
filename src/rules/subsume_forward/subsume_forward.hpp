// C1 SubsumeForward (plan §5.2): delete a clause a smaller live clause implies
// literal-for-literal, modulo root-false literals. Proof delta: `del` only.
// Contract: `docs/dev/calculus.md` §C1; round from cadical `subsume.cpp
// subsume_round` (kissat forward.c needs dense mode).
#pragma once

#include "rules/registry.hpp"

#include <vector>

namespace tactix {

// Guard: quiescent root (sound "root-falsified" semantics, no in-flight
// reasons).
RuleEligibility subsume_forward_guard(const Kernel &);

// Instance space (plan §5.1): payload = candidate arena Ref, size ascending:
// live large clauses, not root-satisfied. `complete` gives the uncapped surface
// (any redundancy); the default is the irredundant canonical round.
void subsume_forward_candidates(const Kernel &, std::vector<Ref> &out,
                                bool complete = false);

// Single-candidate check: Ok = deleted, NoOp = kept. Subsumers come from the
// free literals' watch lists. A large subsumer must be irredundant (no
// promotion, unlike cadical subsume.cpp:133-149); a binary subsumer needs no
// check while binaries are never forgotten (implementation-notes §14).
ApplyResult subsume_forward_try(Kernel &, Ref candidate);

// SC2024 fast-elimination form for a clause start from the mixed occurrence
// index, with fastel.c's record-high marking and occurrence cap. The enclosing
// dense pass owns collection.
ApplyResult subsume_forward_try_fastel(Kernel &, Ref candidate,
                                       uint32_t occurrence_limit,
                                       std::vector<uint8_t> &marks);
// Public instance form (TACTIX_SUBSUME_FORWARD_FASTEL): tag bit, occurrence
// limit in bits 32..62, clause start in bits 0..31. Only inside an Eliminate
// session; drains no root closure.
constexpr uint64_t SUBSUME_FORWARD_FASTEL_INSTANCE = uint64_t{1} << 63;
// After a fastel scan of `lit`, drop dead prefix entries in place (fastel.c).
bool subsume_forward_compact_fastel_prefix(Kernel &, Lit lit, size_t prefix);
constexpr uint32_t SUBSUME_FORWARD_FASTEL_LIMIT_SHIFT = 32;
constexpr uint64_t subsume_forward_fastel_instance(Ref candidate, uint32_t occurrence_limit) {
  return SUBSUME_FORWARD_FASTEL_INSTANCE |
         (uint64_t{occurrence_limit & 0x7fffffffu} << SUBSUME_FORWARD_FASTEL_LIMIT_SHIFT) |
         candidate;
}

// Canonical: the standalone C1 round. The fused Strengthen pass owns Kissat
// forward.c's duplicate-binary sweep. No effort budget.
ApplyResult subsume_forward_apply(Kernel &);

} // namespace tactix
