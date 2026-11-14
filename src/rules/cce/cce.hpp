// CCE, rule X3 (plan §5.2): asymmetric covered clause elimination. Grow C by
// ALA and CLA until blocked (or falsified, the ATA exit), then delete C. Proof
// delta: `del` + one `push-witness` per CLA step plus the blocked exit's
// segment (`docs/dev/calculus.md` §X3). Reference: cadical `cover.cpp`
// (Heule-Järvisalo-Biere).
#pragma once

#include "rules/registry.hpp"

#include <utility>
#include <vector>

namespace tactix {

// Guard: quiescent root.
RuleEligibility cce_guard(const Kernel &);

// v1 instance space: payload = a literal (every candidate containing it). The
// semantic surface names one clause or binary pair; payloads are snapshots.
void cce_literals(const Kernel &, std::vector<Lit> &out);

constexpr uint64_t CCE_LARGE_TAG = uint64_t{UINT32_MAX} << 32;
inline uint64_t cce_large_instance(Ref ref) {
  return CCE_LARGE_TAG | ref;
}
inline uint64_t cce_binary_instance(Lit first, Lit second) {
  return kernel_binary_key(first, second);
}

// One literal: CCE every candidate containing it. Ok = something deleted. CLA
// quantifies over every live non-root-satisfied clause, redundant included
// (filtering it gave X1's fuzz-2204 wrong SAT); ALA uses irredundant clauses
// only (the closure lemma needs them at replay). Scope: implementation-notes §1
// (X3) and ledger rows D-CCE-*.
ApplyResult cce_try(Kernel &, Lit l);
ApplyResult cce_apply(Kernel &); // canonical: the full round

} // namespace tactix
