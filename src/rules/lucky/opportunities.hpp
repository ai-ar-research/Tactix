#pragma once

#include "kernel/kernel.hpp"
#include "rules/registry.hpp"
#include "strategy/rule_parameters.hpp"

// Guards and actions of the native Lucky opportunities. Every definition lives
// out of line in the opportunity's own translation unit.
namespace tactix {

// SAT-only Lucky sub-strategy: recover Boolean affine equations from ternary
// CNF groups, search the residual GF(2) space, and accept only a total model
// independently checked against every live clause.
RuleEligibility parity_model_guard(const Kernel &);
ApplyResult parity_model_apply(Kernel &);

// Exact extended-resolution refutation for complete bounded-arity XOR systems
// whose selected equations sum to 0 = 1.  It is the UNSAT half of Lucky's
// public parity strategy; NoOp means either no such contradiction or a clean
// resource/proof-format decline.
ApplyResult parity_refutation_apply(
    Kernel &, const strategy::ParityModelParameters &);
RuleEligibility startup_parity_guard(const Kernel &);
ApplyResult startup_parity_apply(Kernel &);

// Terminal original-input count conservation. Complete selector rows and
// guarded equivalences define a balanced network; fixed boundary counts clash.
// Work is caller-bounded by Kernel::work_budget and the shared stop contract.
RuleEligibility count_preservation_guard(const Kernel &);
ApplyResult count_preservation_apply(Kernel &);

// SAT-only Lucky sub-strategy for exact one-hot GF(3) affine encodings.
RuleEligibility onehot_model_guard(const Kernel &);
ApplyResult direct_csp_model_apply(Kernel &);
ApplyResult onehot_model_apply(Kernel &);

// Extended-resolution refutation for bipartite exact-one incidence systems
// whose two color classes have unequal cardinality modulo three.
ApplyResult exact_one_refutation_apply(
    Kernel &, const strategy::OnehotModelParameters &);

RuleEligibility affine_model_guard(const Kernel &);
ApplyResult affine_model_apply(Kernel &);

RuleEligibility balanced_signature_model_guard(const Kernel &);
ApplyResult balanced_signature_model_apply(Kernel &);

RuleEligibility fermat_model_guard(const Kernel &);
ApplyResult fermat_model_apply(Kernel &);

RuleEligibility cerny_model_guard(const Kernel &);
ApplyResult cerny_model_apply(Kernel &);

RuleEligibility lookup_circuit_model_guard(const Kernel &);
ApplyResult lookup_circuit_model_apply(Kernel &);

RuleEligibility simon_model_guard(const Kernel &);
ApplyResult simon_model_apply(Kernel &);

RuleEligibility stconn_proof_guard(const Kernel &);
ApplyResult stconn_proof_apply(Kernel &);

RuleEligibility signed_cardinality_proof_guard(const Kernel &);
ApplyResult signed_cardinality_proof_apply(Kernel &);

// SAT-only exact-cover model construction for signed at-most-one block
// formulas (LymphoSAT's sgen owner): every clause is a binary at-most-one edge
// or a positive covering clause of the block arity, the binary edges partition
// the signed items into blocks of that arity, and each item lies in one block
// and two covering clauses. A model chooses one item per block so that every
// covering clause holds exactly one chosen item.
RuleEligibility exact_cover_model_guard(const Kernel &);
ApplyResult exact_cover_model_apply(Kernel &);

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
