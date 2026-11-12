// V4 TransitiveReduce (plan §5.2): delete a binary (src ∨ dst) when dst stays
// reachable from ¬src along other binaries. Proof delta: `del`; a walk reaching
// both polarities adds the RUP unit {src}. Contract: `docs/dev/calculus.md`
// §V4; binary lifecycle: implementation-notes §14.
#pragma once

#include "rules/registry.hpp"

#include <vector>

namespace tactix {

// Guard: quiescent root (graph reflects every unit; in-place compaction safe).
RuleEligibility transitive_reduce_guard(const Kernel &);

// Instance space (plan §5.1): payload = source literal; one instance is all its
// candidate edges. Both unfixed polarities of every active variable, including
// edgeless ones (Kissat charges their scan). A pair is owned by its lower
// compact endpoint (Kernel's compact import map, else import order).
// INSTANCE_CANONICAL = the full round.
void transitive_reduce_sources(const Kernel &, std::vector<Lit> &out);

// One source: delete transitively redundant candidates. The round-local
// tombstone is required for soundness ((a ∨ b),(a ∨ c) with b ≡ c must not
// delete each other) and is scoped to one source. The candidate is excluded by
// literal pair in both directions (kissat transitive.c:126 excludes the slot).
ApplyResult transitive_reduce_try(Kernel &, Lit src);

// Canonical: the full round. Failed-source units are committed and propagated
// before the next source (Kissat); duplicate copies are independent. The walk
// charges stats.transitive_ticks and stops at dst;
// `Kernel::transitive_early_exit` is a non-reference exhaustive arm.
ApplyResult transitive_reduce_apply(Kernel &);

} // namespace tactix
