#pragma once

#include "kernel/kernel.hpp"
#include "rules/registry.hpp"

namespace tactix {

// Public graph-model action with two structurally authenticated forms:
//
//  * a layered exact-count independent-set encoding (LymphoSAT's owner), and
//  * a directed stable-extension core: negative conflict binaries, one
//    positive attacker-cover clause per argument, and optional guarded
//    resolvents of those two families.
//
// The latter proves every guarded clause is a single binary resolvent of an
// authenticated cover clause, then runs local search on that equivalent core
// without changing the live formula.  A candidate is published only after it
// satisfies the complete formula. Recognition never uses names/comments.
// The layered form constructs a K-vertex independent set generically,
// completes the state variables by propagation and validates the whole model.
RuleEligibility independent_set_model_guard(const Kernel &);
ApplyResult independent_set_model_apply(Kernel &);

} // namespace tactix
