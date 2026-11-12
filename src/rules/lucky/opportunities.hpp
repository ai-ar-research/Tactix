#pragma once

#include "kernel/kernel.hpp"
#include "rules/registry.hpp"
#include "strategy/rule_parameters.hpp"

// Guards and actions of the native Lucky opportunities. Every definition lives
// out of line in the opportunity's own translation unit.
namespace tactix {

// SAT-only: recover GF(2) equations from ternary CNF groups, search the
// residual space and publish only a model checked against every live clause.
RuleEligibility parity_model_guard(const Kernel &);
ApplyResult parity_model_apply(Kernel &);

// Extended-resolution refutation of complete bounded-arity XOR systems summing
// to 0 = 1 (the UNSAT half of parity); NoOp is a clean decline.
ApplyResult parity_refutation_apply(
    Kernel &, const strategy::ParityModelParameters &);
RuleEligibility startup_parity_guard(const Kernel &);
ApplyResult startup_parity_apply(Kernel &);

// Terminal count conservation: selector rows and guarded equivalences define
// a balanced network whose fixed boundary counts clash.
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

// SAT-only exact cover for signed AMO block formulas (LymphoSAT sgen): binary
// AMO edges partition items into blocks, each item lies in two positive
// covering clauses, and a model picks one item per block covering each clause
// exactly once.
RuleEligibility exact_cover_model_guard(const Kernel &);
ApplyResult exact_cover_model_apply(Kernel &);

// Graph models with two authenticated forms: a layered exact-count
// independent-set encoding (LymphoSAT), completed by propagation, and a
// stable-extension core (conflict binaries, attacker covers and their guarded
// resolvents) solved by local search. Candidates are published only after
// satisfying the whole formula.
RuleEligibility independent_set_model_guard(const Kernel &);
ApplyResult independent_set_model_apply(Kernel &);

} // namespace tactix
