#pragma once

#include "kernel/kernel.hpp"
#include "rules/lucky/root_guard.hpp"
#include "rules/registry.hpp"
#include "strategy/rule_parameters.hpp"

// Guards and actions of the native Lucky opportunities. Each action lives out
// of line in the opportunity's own translation unit.
namespace tactix {

// SAT-only: recover GF(2) equations from ternary CNF groups, search the
// residual space and publish only a model checked against every live clause.
inline RuleEligibility parity_model_guard(const Kernel &k) {
  return lucky_root_eligibility(k);
}
ApplyResult parity_model_apply(Kernel &);

// Extended-resolution refutation of complete bounded-arity XOR systems summing
// to 0 = 1 (the UNSAT half of parity); NoOp is a clean decline.
ApplyResult parity_refutation_apply(
    Kernel &, const strategy::ParityModelParameters &);
inline RuleEligibility startup_parity_guard(const Kernel &k) {
  return lucky_startup_eligibility(k);
}
ApplyResult startup_parity_apply(Kernel &);
// The parity refutation over caller-selected variable sets. Each set's XOR
// must be completely encoded by original input clauses, every variable must
// occur in an even number of sets, and the right-hand sides must sum to one.
ApplyResult parity_selection_apply(Kernel &, const std::vector<std::vector<int32_t>> &);

// Terminal count conservation: selector rows and guarded equivalences define
// a balanced network whose fixed boundary counts clash.
inline RuleEligibility count_preservation_guard(const Kernel &k) {
  return lucky_startup_eligibility(k);
}
ApplyResult count_preservation_apply(Kernel &);

// Disjoint at-least-one demands exceed the number of at-most-one pools.
ApplyResult matching_count_apply(Kernel &);

// The same count over caller-selected premises: `demands` are original input
// clauses, `failed` literals are refuted in order by unit propagation over the
// original binary and unit clauses, and `pools` partition the surviving demand
// literals into pairwise-conflicting groups. Declines unless every premise
// validates and there are more demands than pools.
struct MatchingSelection {
  std::vector<std::vector<int32_t>> demands, pools;
  std::vector<int32_t> failed;
};
ApplyResult matching_selection_apply(Kernel &, const MatchingSelection &);

// A sequential at-most-K counter (Sinz) over vertex literals x_1..x_n with
// register rows s_{i,1..K}, i < n, plus cliques of vertices whose pairs are
// covered by input clauses (x_a | x_b). Weights covering every vertex at least
// `coverage` times bound the false vertices by weight / coverage; fewer than
// n - K refutes the input. `excess` is filled by validation.
struct CounterCover {
  std::vector<int32_t> vertices;
  std::vector<std::vector<int32_t>> rows;
  std::vector<std::vector<int32_t>> cliques;
  std::vector<uint64_t> weights;
  uint64_t coverage = 0;
  std::vector<uint64_t> excess;
};
ApplyResult counter_cover_apply(Kernel &, CounterCover &);

// SAT-only Lucky sub-strategy for exact one-hot GF(3) affine encodings.
inline RuleEligibility onehot_model_guard(const Kernel &k) {
  return lucky_root_eligibility(k);
}
ApplyResult direct_csp_model_apply(Kernel &);
ApplyResult onehot_model_apply(Kernel &);

// Extended-resolution refutation for bipartite exact-one incidence systems
// whose two color classes have unequal cardinality modulo three.
ApplyResult exact_one_refutation_apply(
    Kernel &, const strategy::OnehotModelParameters &);

inline RuleEligibility affine_model_guard(const Kernel &k) {
  return parity_model_guard(k);
}
ApplyResult affine_model_apply(Kernel &);

// Collapse variables with identical occurrence sets when each block has two
// residual-clause signatures, and solve the resulting parity system.
inline RuleEligibility startup_affine_quotient_guard(const Kernel &k) {
  return lucky_startup_eligibility(k);
}
ApplyResult startup_affine_quotient_apply(Kernel &);

inline RuleEligibility balanced_signature_model_guard(const Kernel &k) {
  return lucky_root_eligibility(k);
}
ApplyResult balanced_signature_model_apply(Kernel &);

inline RuleEligibility fermat_model_guard(const Kernel &k) {
  return lucky_root_eligibility(k);
}
ApplyResult fermat_model_apply(Kernel &);

inline RuleEligibility cerny_model_guard(const Kernel &k) {
  return lucky_root_eligibility(k);
}
ApplyResult cerny_model_apply(Kernel &);

inline RuleEligibility lookup_circuit_model_guard(const Kernel &k) {
  return lucky_root_eligibility(k);
}
ApplyResult lookup_circuit_model_apply(Kernel &);

inline RuleEligibility simon_model_guard(const Kernel &k) {
  return lucky_root_eligibility(k);
}
ApplyResult simon_model_apply(Kernel &);

inline RuleEligibility stconn_proof_guard(const Kernel &k) {
  return lucky_root_eligibility(k);
}
ApplyResult stconn_proof_apply(Kernel &);

inline RuleEligibility signed_cardinality_proof_guard(const Kernel &k) {
  return lucky_root_eligibility(k);
}
ApplyResult signed_cardinality_proof_apply(Kernel &);

// SAT-only exact cover for signed AMO block formulas (LymphoSAT sgen): binary
// AMO edges partition items into blocks, each item lies in two positive
// covering clauses, and a model picks one item per block covering each clause
// exactly once.
inline RuleEligibility exact_cover_model_guard(const Kernel &k) {
  return lucky_root_eligibility(k);
}
ApplyResult exact_cover_model_apply(Kernel &);

// Graph models with two authenticated forms: a layered exact-count
// independent-set encoding (LymphoSAT), completed by propagation, and a
// stable-extension core (conflict binaries, attacker covers and their guarded
// resolvents) solved by local search. Candidates are published only after
// satisfying the whole formula.
inline RuleEligibility independent_set_model_guard(const Kernel &k) {
  return lucky_root_eligibility(k);
}
ApplyResult independent_set_model_apply(Kernel &);

} // namespace tactix
