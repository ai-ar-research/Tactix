#pragma once

#include "kernel/kernel.hpp"
#include "rules/registry.hpp"

namespace tactix {

// SAT-only exact-cover model construction for signed at-most-one block
// formulas (LymphoSAT's sgen owner): every clause is a binary at-most-one edge
// or a positive covering clause of the block arity, the binary edges partition
// the signed items into blocks of that arity, and each item lies in one block
// and two covering clauses. A model chooses one item per block so that every
// covering clause holds exactly one chosen item.
RuleEligibility exact_cover_model_guard(const Kernel &);
ApplyResult exact_cover_model_apply(Kernel &);

} // namespace tactix
