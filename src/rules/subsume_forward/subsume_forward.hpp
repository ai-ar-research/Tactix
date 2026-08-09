// C1 SubsumeForward (plan §5.2): forward subsumption — delete a clause that a
// smaller live clause implies literal-for-literal, modulo root-falsified
// literals. Proof delta: `del` only; self-subsuming resolution is C2's
// add-RUP + delete territory.
// Precondition, effect, soundness: `docs/calculus.md` §C1. The round is cadical
// `subsume.cpp subsume_round` transliterated; kissat forward.c presupposes
// dense mode, a kernel-wide watch replacement this kernel does not have.
#pragma once

#include "rules/registry.hpp"

#include <vector>

namespace tactix {

constexpr uint32_t SUBSUME_CLSLIM = 1000;
constexpr uint32_t SUBSUME_OCCLIM = 100;
constexpr uint32_t SUBSUME_BINLIM = 10000;

// Guard: quiescent root — fully propagated, consistent, level 0. That is what
// makes the semantic check sound as stated ("false" means root-falsified) and
// keeps deletion away from in-flight reasons.
bool subsume_forward_guard(const Kernel &);

// Instance space (plan §5.1): payload = the arena Ref of a candidate clause, in
// canonical processing order (size ascending): live, 3 <= size <=
// SUBSUME_CLSLIM, not root-satisfied. `complete` selects the instance surface's
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
// Argument: `docs/implementation-notes.md` §14.
ApplyResult subsume_forward_try(Kernel &, Ref candidate);

// Canonical: the full round. kissat's round-entry duplicate-binary sweep stays
// dropped — it deletes binaries by multiset position, which `binary_ids` (one
// id per literal pair) cannot name. No effort budget in the rule.
ApplyResult subsume_forward_apply(Kernel &);

} // namespace tactix
