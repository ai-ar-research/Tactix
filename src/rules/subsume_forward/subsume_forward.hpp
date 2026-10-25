// C1 SubsumeForward (plan §5.2): forward subsumption — delete a clause that a
// smaller live clause implies literal-for-literal, modulo root-falsified
// literals. Proof delta: `del` only; self-subsuming resolution is C2's
// add-RUP + delete territory.
// Precondition, effect, soundness: `docs/dev/calculus.md` §C1. The round is cadical
// `subsume.cpp subsume_round` transliterated; kissat forward.c presupposes
// dense mode, a kernel-wide watch replacement this kernel does not have.
#pragma once

#include "rules/registry.hpp"

#include <vector>

namespace tactix {

// Guard: quiescent root — fully propagated, consistent, level 0. That is what
// makes the semantic check sound as stated ("false" means root-falsified) and
// keeps deletion away from in-flight reasons.
RuleEligibility subsume_forward_guard(const Kernel &);

// Instance space (plan §5.1): payload = the arena Ref of a candidate clause, in
// canonical processing order (size ascending): live large clauses, not
// root-satisfied. `complete` selects the uncapped instance surface's
// set — any redundancy, every clause `subsume_forward_try` accepts; the default
// is the canonical round's, which is irredundant-only.
void subsume_forward_candidates(const Kernel &, std::vector<Ref> &out,
                                bool complete = false);

// Single-candidate check: Ok = deleted, NoOp = kept. Subsumers are discovered
// through the watch lists of the candidate's free literals, complete for pure
// subsumption since a subsumer's two watched literals are its own. A large
// subsumer must be irredundant and there is no promotion path (cadical turns a
// redundant subsumer irredundant, subsume.cpp:133-149); a binary subsumer needs
// no redundancy check, because binaries have an irredundant lifecycle and none
// can be forgotten. Only a rule giving binaries a redundant lifecycle would
// make the check mandatory, at all three binary-subsumer sites (here,
// `subsumed_by_binary`, strengthen.cpp's fused branch).
// Argument: `docs/dev/implementation-notes.md` §14.
ApplyResult subsume_forward_try(Kernel &, Ref candidate);

// Policy-internal SC2024 fast-elimination form for a clause start obtained from
// the live mixed occurrence index.  It preserves fastel.c's deliberately
// incomplete record-high marking and physical-occurrence cap while keeping the
// clause deletion inside C1.  Arena collection belongs to the enclosing dense
// pass.
ApplyResult subsume_forward_try_fastel(Kernel &, Ref candidate,
                                       uint32_t occurrence_limit,
                                       std::vector<uint8_t> &marks);
// The same check as a public instance (TACTIX_SUBSUME_FORWARD_FASTEL): the tag
// bit, the occurrence limit in bits 32..62 and the candidate's clause start in
// bits 0..31. It applies only inside an ordered (Eliminate) occurrence session
// and, like the native call, drains no root closure.
constexpr uint64_t SUBSUME_FORWARD_FASTEL_INSTANCE = uint64_t{1} << 63;
// After a fastel occurrence scan of `lit`, drop the inspected prefix's dead
// entries in place (fastel.c): garbage clauses, clauses no longer containing
// `lit` and binaries with an unsatisfied retired partner. Representation only.
bool subsume_forward_compact_fastel_prefix(Kernel &, Lit lit, size_t prefix);
constexpr uint32_t SUBSUME_FORWARD_FASTEL_LIMIT_SHIFT = 32;
constexpr uint64_t subsume_forward_fastel_instance(Ref candidate, uint32_t occurrence_limit) {
  return SUBSUME_FORWARD_FASTEL_INSTANCE |
         (uint64_t{occurrence_limit & 0x7fffffffu} << SUBSUME_FORWARD_FASTEL_LIMIT_SHIFT) |
         candidate;
}

// Canonical: the standalone diagnostic C1 round. The production fused
// Strengthen pass owns Kissat forward.c's round-entry duplicate-binary sweep;
// do not duplicate that consumer here. No effort budget in the rule.
ApplyResult subsume_forward_apply(Kernel &);

} // namespace tactix
