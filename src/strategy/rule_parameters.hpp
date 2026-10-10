#pragma once

#include <cstdint>
#include <vector>
#include "strategy/local_search.hpp"
#include "strategy/graph_labeling_parameters.hpp"
#include "strategy/ramsey_startup_parameters.hpp"

namespace tactix {
struct Kernel;

namespace strategy {

struct BceParameters {
  uint32_t occurrence_limit = 0;
  constexpr bool valid() const { return true; }
  static BceParameters from_kernel(const Kernel &);
};

struct TernaryParameters {
  uint32_t occurrence_limit = 100;
  uint32_t maximum_add_percent = 1000;
  constexpr bool valid() const { return true; }
  static TernaryParameters from_kernel(const Kernel &);
};

struct FactorParameters {
  uint32_t maximum_clause_size = 5;
  uint32_t growth_bound = 0;
  uint32_t candidate_rounds = 2;
  bool composite = false;
  bool demote_fresh = true;
  constexpr bool valid() const { return true; }
  static FactorParameters from_kernel(const Kernel &);
};

struct TransitiveReduceParameters {
  bool early_exit = true;
  bool live_meter = true;
  bool checked_budget_meter = false;
  constexpr bool valid() const { return true; }
  static TransitiveReduceParameters from_kernel(const Kernel &);
};

struct SubstituteParameters {
  bool demote_substituted = false;
  // Round batch inside one watch session (0: one canonical round). A further
  // round runs only after a round that retired a variable; with
  // `round_effort_ceiling`, also only while that round's own Substitute effort
  // stayed within the application's rule work budget (substitute.c rounds).
  // Per application only: installs and from_kernel leave both at zero.
  uint32_t max_rounds = 0;
  bool round_effort_ceiling = false;
  constexpr bool valid() const { return max_rounds || !round_effort_ceiling; }
  static SubstituteParameters from_kernel(const Kernel &);
};

struct ReduceParameters {
  static constexpr uint32_t VICTIM_NUMERATOR_DEFAULT = 3;
  static constexpr uint32_t VICTIM_DENOMINATOR_DEFAULT = 4;
  uint32_t tier1 = 2;
  uint32_t tier2 = 6;
  uint32_t victim_numerator = VICTIM_NUMERATOR_DEFAULT;
  uint32_t victim_denominator = VICTIM_DENOMINATOR_DEFAULT;
  bool protect_tiers = true;
  bool compact_collection = false;
  bool reduction_bookkeeping = true;
  constexpr bool valid() const {
    return tier1 <= tier2 && victim_denominator &&
           victim_numerator <= victim_denominator;
  }
  static ReduceParameters from_kernel(const Kernel &);
};

struct WalkParameters {
  static constexpr uint64_t STEP_BUDGET_DEFAULT = 1000000;
  static constexpr uint32_t BREAK_BASE_SCALE = 1000;
  static constexpr uint32_t EVEN_BREAK_BASE_DEFAULT = 2000;

  struct ScorePoint {
    uint32_t average_clause_size_milli = 0;
    uint32_t break_base_milli = EVEN_BREAK_BASE_DEFAULT;

    friend bool operator==(const ScorePoint &, const ScorePoint &) = default;
  };

  static std::vector<ScorePoint> reference_score_curve();

  // Native occurrence-list allowance, also charged for seeded preparation's
  // phase/identity visits and evaluated literals. Zero requests no seed work.
  uint64_t step_budget = STEP_BUDGET_DEFAULT;
  uint64_t random_seed = 0;
  // Kissat alternates this fixed base with the fitted curve on successive
  // Walk calls. Both are heuristic choices, so the shared typed invocation
  // surface owns them rather than the Walk implementation.
  uint32_t even_break_base_milli = EVEN_BREAK_BASE_DEFAULT;
  std::vector<ScorePoint> score_curve = reference_score_curve();
  // Exact retained candidate identity, or zero for native phase seeding.
  uint64_t seed_candidate = 0;
  enum class Objective : uint32_t { LegacyWalk = 0, FullCurrentCnf = 1 };
  enum class Admission : uint32_t { Legacy = 0, StrictlyBetter = 1 };
  Objective objective = Objective::LegacyWalk;
  Admission admission = Admission::Legacy;
  uint64_t preparation_budget = 0;
  bool valid() const;
  static WalkParameters from_kernel(const Kernel &);
};

struct ReorderParameters {
  static constexpr uint32_t MAXIMUM_CLAUSE_SIZE_DEFAULT = 100;
  static constexpr uint32_t WEIGHT_SCALE = 1000;
  static constexpr uint32_t CLAUSE_DECAY_DEFAULT = 500;
  static constexpr uint32_t BINARY_WEIGHT_DEFAULT = 1000;
  static constexpr uint32_t BALANCED_POLARITY_DEFAULT = 2000;
  static constexpr uint32_t MAXIMUM_WEIGHT_PARAMETER = 1000000;
  uint32_t maximum_clause_size = MAXIMUM_CLAUSE_SIZE_DEFAULT;
  uint32_t clause_decay_milli = CLAUSE_DECAY_DEFAULT;
  uint32_t binary_weight_milli = BINARY_WEIGHT_DEFAULT;
  uint32_t balanced_polarity_milli = BALANCED_POLARITY_DEFAULT;
  constexpr bool valid() const {
    return maximum_clause_size >= 2 &&
           clause_decay_milli && clause_decay_milli <= WEIGHT_SCALE &&
           binary_weight_milli &&
           binary_weight_milli <= MAXIMUM_WEIGHT_PARAMETER &&
           balanced_polarity_milli &&
           balanced_polarity_milli <= MAXIMUM_WEIGHT_PARAMETER;
  }
  static ReorderParameters from_kernel(const Kernel &);
};

// Structural XOR reasoning. Exact complete bounded-arity encodings can yield
// an independently checkable even-incidence refutation; otherwise inferred
// affine restrictions are SAT-search heuristics whose candidates are accepted
// only after the complete live CNF validates them.
struct ParityModelParameters {
  static constexpr uint32_t MAXIMUM_VARIABLES_DEFAULT = 4096;
  static constexpr uint32_t MAXIMUM_GROUPS_DEFAULT = 65536;
  static constexpr uint32_t MAXIMUM_CONSTRAINTS_DEFAULT = 262144;
  static constexpr uint32_t MINIMUM_GROUP_CLAUSES_DEFAULT = 3;
  static constexpr uint32_t MINIMUM_EQUATIONS_DEFAULT = 1;
  static constexpr uint32_t MAXIMUM_FREE_VARIABLES_DEFAULT = 24;
  static constexpr uint64_t CANDIDATE_BUDGET_DEFAULT = 1024;
  static constexpr uint32_t REFUTATION_MAXIMUM_VARIABLES_DEFAULT = 524288;
  static constexpr uint32_t REFUTATION_MAXIMUM_GROUPS_DEFAULT = 262144;
  static constexpr uint32_t REFUTATION_MAXIMUM_CONSTRAINTS_DEFAULT = 2000000;
  static constexpr uint32_t REFUTATION_MAXIMUM_ARITY_DEFAULT = 4;
  static constexpr uint32_t REFUTATION_PROBE_CONSTRAINTS_DEFAULT = 4096;
  static constexpr uint32_t REFUTATION_MINIMUM_PROBE_REPEATS_DEFAULT = 8;
  // The exact recognizer and interruptible count-only preflight bound default
  // work from formula structure and the caller's deadline.  Do not impose a
  // target-shaped certificate cutoff; callers that need one can pose it.
  static constexpr uint64_t REFUTATION_MAXIMUM_PROOF_ADDITIONS_DEFAULT =
      UINT64_MAX;

  uint32_t maximum_variables = MAXIMUM_VARIABLES_DEFAULT;
  uint32_t maximum_groups = MAXIMUM_GROUPS_DEFAULT;
  uint32_t maximum_constraints = MAXIMUM_CONSTRAINTS_DEFAULT;
  uint32_t minimum_group_clauses = MINIMUM_GROUP_CLAUSES_DEFAULT;
  uint32_t minimum_equations = MINIMUM_EQUATIONS_DEFAULT;
  uint32_t maximum_free_variables = MAXIMUM_FREE_VARIABLES_DEFAULT;
  uint64_t candidate_budget = CANDIDATE_BUDGET_DEFAULT;
  uint32_t refutation_maximum_variables =
      REFUTATION_MAXIMUM_VARIABLES_DEFAULT;
  uint32_t refutation_maximum_groups = REFUTATION_MAXIMUM_GROUPS_DEFAULT;
  uint32_t refutation_maximum_constraints =
      REFUTATION_MAXIMUM_CONSTRAINTS_DEFAULT;
  uint32_t refutation_maximum_arity = REFUTATION_MAXIMUM_ARITY_DEFAULT;
  uint32_t refutation_probe_constraints =
      REFUTATION_PROBE_CONSTRAINTS_DEFAULT;
  uint32_t refutation_minimum_probe_repeats =
      REFUTATION_MINIMUM_PROBE_REPEATS_DEFAULT;
  uint64_t refutation_maximum_proof_additions =
      REFUTATION_MAXIMUM_PROOF_ADDITIONS_DEFAULT;
  constexpr bool valid() const {
    return maximum_variables && maximum_groups && maximum_constraints &&
           minimum_group_clauses >= 1 && minimum_group_clauses <= 4 &&
           maximum_free_variables <= 63 && refutation_maximum_variables &&
           refutation_maximum_groups && refutation_maximum_constraints &&
           refutation_maximum_arity >= 2 && refutation_maximum_arity <= 4 &&
           refutation_probe_constraints &&
           refutation_minimum_probe_repeats &&
           refutation_minimum_probe_repeats < refutation_probe_constraints &&
           refutation_maximum_proof_additions;
  }
  static ParityModelParameters from_kernel(const Kernel &);
};

// SAT-only search over exact one-hot encodings of affine equations in GF(3).
// Recognition is structural and candidates are accepted only after a complete
// scan of the live Boolean formula.
struct OnehotModelParameters {
  static constexpr uint32_t MAXIMUM_VARIABLES_DEFAULT = 4096;
  static constexpr uint32_t MAXIMUM_DOMAINS_DEFAULT = 2048;
  static constexpr uint32_t MAXIMUM_CONSTRAINTS_DEFAULT = 262144;
  static constexpr uint32_t MAXIMUM_EQUATION_ARITY_DEFAULT = 8;
  static constexpr uint32_t MINIMUM_DOMAINS_DEFAULT = 1;
  static constexpr uint32_t MINIMUM_EQUATIONS_DEFAULT = 1;
  static constexpr uint32_t MAXIMUM_FREE_DOMAINS_DEFAULT = 64;
  static constexpr uint64_t CANDIDATE_BUDGET_DEFAULT = 1024;
  static constexpr uint32_t REFUTATION_MAXIMUM_VARIABLES_DEFAULT = 65536;
  static constexpr uint32_t REFUTATION_MAXIMUM_GROUPS_DEFAULT = 65536;
  static constexpr uint32_t REFUTATION_MAXIMUM_CONSTRAINTS_DEFAULT = 262144;
  static constexpr uint32_t REFUTATION_MAXIMUM_ARITY_DEFAULT = 4;
  static constexpr uint64_t REFUTATION_MAXIMUM_PROOF_ADDITIONS_DEFAULT =
      UINT64_MAX;

  uint32_t maximum_variables = MAXIMUM_VARIABLES_DEFAULT;
  uint32_t maximum_domains = MAXIMUM_DOMAINS_DEFAULT;
  uint32_t maximum_constraints = MAXIMUM_CONSTRAINTS_DEFAULT;
  uint32_t maximum_equation_arity = MAXIMUM_EQUATION_ARITY_DEFAULT;
  uint32_t minimum_domains = MINIMUM_DOMAINS_DEFAULT;
  uint32_t minimum_equations = MINIMUM_EQUATIONS_DEFAULT;
  uint32_t maximum_free_domains = MAXIMUM_FREE_DOMAINS_DEFAULT;
  uint64_t candidate_budget = CANDIDATE_BUDGET_DEFAULT;
  uint32_t refutation_maximum_variables =
      REFUTATION_MAXIMUM_VARIABLES_DEFAULT;
  uint32_t refutation_maximum_groups = REFUTATION_MAXIMUM_GROUPS_DEFAULT;
  uint32_t refutation_maximum_constraints =
      REFUTATION_MAXIMUM_CONSTRAINTS_DEFAULT;
  uint32_t refutation_maximum_arity = REFUTATION_MAXIMUM_ARITY_DEFAULT;
  uint64_t refutation_maximum_proof_additions =
      REFUTATION_MAXIMUM_PROOF_ADDITIONS_DEFAULT;
  constexpr bool valid() const {
    return maximum_variables && maximum_domains && maximum_constraints &&
           maximum_equation_arity && minimum_domains <= maximum_domains &&
           refutation_maximum_variables && refutation_maximum_groups &&
           refutation_maximum_constraints && refutation_maximum_arity >= 2 &&
           refutation_maximum_arity <= 4 &&
           refutation_maximum_proof_additions;
  }
  static OnehotModelParameters from_kernel(const Kernel &);
};

// SAT-only construction for balance designs encoded by a block of pair
// choices and a block of signed ternary signatures.  The reference work
// controls are the LymphoSAT owner strategy's complete neighborhood schedule;
// keeping them typed lets online policies shorten, lengthen, or reseed the
// search without changing recognition or model validation.
struct BalancedSignatureModelParameters {
  static constexpr uint32_t MAXIMUM_BALLS_DEFAULT = 256;
  static constexpr uint32_t MAXIMUM_WIDTH_DEFAULT = 8;
  static constexpr uint64_t RESTARTS_DEFAULT = 160;
  static constexpr uint64_t STEPS_PER_RESTART_DEFAULT = 140000;
  static constexpr uint32_t SAMPLES_PER_STEP_DEFAULT = 36;
  static constexpr uint64_t SEED_DEFAULT = 0xabcdef1234567890ULL;
  static constexpr uint32_t COMPLETION_ROUNDS_DEFAULT = 64;

  uint32_t maximum_balls = MAXIMUM_BALLS_DEFAULT;
  uint32_t maximum_width = MAXIMUM_WIDTH_DEFAULT;
  uint64_t restarts = RESTARTS_DEFAULT;
  uint64_t steps_per_restart = STEPS_PER_RESTART_DEFAULT;
  uint32_t samples_per_step = SAMPLES_PER_STEP_DEFAULT;
  uint64_t seed = SEED_DEFAULT;
  uint32_t completion_rounds = COMPLETION_ROUNDS_DEFAULT;
  constexpr bool valid() const {
    return maximum_balls >= 3 && maximum_width && maximum_width <= 12 &&
           restarts && steps_per_restart && samples_per_step &&
           completion_rounds;
  }
  static BalancedSignatureModelParameters from_kernel(const Kernel &);
};

// SAT-only reconstruction for arithmetic circuits encoding
// (a-b)(a+b)=N.  The owner recovers N from the final comparator and factors it
// with deterministic 64-bit Miller--Rabin/Pollard Rho before validating the
// completed assignment against the entire live CNF.  These bounds are the
// owner's algorithmic limits, exposed so an online caller can dose the action.
struct FermatModelParameters {
  static constexpr uint32_t MAXIMUM_BITS_DEFAULT = 63;
  static constexpr uint32_t COMPARATOR_TAIL_CLAUSES_DEFAULT = 80;
  static constexpr uint64_t RHO_CONSTANTS_DEFAULT = 199;
  static constexpr uint64_t RHO_ITERATIONS_DEFAULT = 200000;

  uint32_t maximum_bits = MAXIMUM_BITS_DEFAULT;
  uint32_t comparator_tail_clauses = COMPARATOR_TAIL_CLAUSES_DEFAULT;
  uint64_t rho_constants = RHO_CONSTANTS_DEFAULT;
  uint64_t rho_iterations = RHO_ITERATIONS_DEFAULT;
  constexpr bool valid() const {
    return maximum_bits && maximum_bits <= 63 &&
           comparator_tail_clauses >= 2 &&
           comparator_tail_clauses <= COMPARATOR_TAIL_CLAUSES_DEFAULT &&
           rho_constants && rho_iterations;
  }
  static FermatModelParameters from_kernel(const Kernel &);
};

// SAT-only reconstruction of feed-forward lookup-table circuits.  The owner
// enumerates the structural primary-input support with byte-lane SIMD and uses
// occurrence-queued gate-CSP search when that finite enumeration is not
// admitted.  The limits are public work controls, independent of recognition.
struct LookupCircuitModelParameters {
  static constexpr uint32_t MAXIMUM_FANIN_DEFAULT = 8;
  static constexpr uint32_t MAXIMUM_FANOUT_DEFAULT = 8;
  static constexpr uint32_t MAXIMUM_PRIMARY_INPUTS_DEFAULT = 64;
  static constexpr uint32_t MAXIMUM_SUPPORT_DEFAULT = 32;
  static constexpr uint64_t MAXIMUM_VECTOR_GATE_EVALUATIONS_DEFAULT =
      6000000000ULL;

  uint32_t maximum_fanin = MAXIMUM_FANIN_DEFAULT;
  uint32_t maximum_fanout = MAXIMUM_FANOUT_DEFAULT;
  uint32_t maximum_primary_inputs = MAXIMUM_PRIMARY_INPUTS_DEFAULT;
  uint32_t maximum_support = MAXIMUM_SUPPORT_DEFAULT;
  uint64_t maximum_vector_gate_evaluations =
      MAXIMUM_VECTOR_GATE_EVALUATIONS_DEFAULT;
  constexpr bool valid() const {
    return maximum_fanin && maximum_fanin <= 8 && maximum_fanout &&
           maximum_fanout <= 8 && maximum_primary_inputs &&
           maximum_primary_inputs <= 64 && maximum_support &&
           maximum_support <= 63 && maximum_vector_gate_evaluations;
  }
  static LookupCircuitModelParameters from_kernel(const Kernel &);
};

// SAT-only key search for Simon-family round circuits. Recognition recovers
// the key permutation and per-round key selection from the CNF itself; these
// controls bound the admitted circuit depth and exhaustive 32-bit key work.
struct SimonModelParameters {
  static constexpr uint32_t MAXIMUM_ROUNDS_DEFAULT = 64;
  static constexpr uint64_t MAXIMUM_CANDIDATES_DEFAULT = uint64_t{1} << 32;

  uint32_t maximum_rounds = MAXIMUM_ROUNDS_DEFAULT;
  uint64_t maximum_candidates = MAXIMUM_CANDIDATES_DEFAULT;
  constexpr bool valid() const {
    return maximum_rounds && maximum_rounds <= 64 && maximum_candidates;
  }
  static SimonModelParameters from_kernel(const Kernel &);
};

// SAT-only exact-cover model for signed at-most-one block formulas
// (LymphoSAT's sgen owner). Recognition is exact on the live clause set; the
// search is the owner's deterministic minimum-remaining-rows DFS, then its
// randomized restarts. The node bounds are the owner's per-attempt node limit;
// the owner's phases are otherwise wall-clock bounded, so restarts default to
// none and are dosed by the caller.
struct ExactCoverModelParameters {
  static constexpr uint32_t BLOCK_ARITY_DEFAULT = 5;
  static constexpr uint64_t MAXIMUM_NODES_DEFAULT = 2000000;
  static constexpr uint64_t RESTARTS_DEFAULT = 0;
  static constexpr uint64_t RESTART_NODES_DEFAULT = 2000000;
  static constexpr uint64_t SEED_DEFAULT = 0x123456789ULL;

  uint32_t block_arity = BLOCK_ARITY_DEFAULT;
  uint64_t maximum_nodes = MAXIMUM_NODES_DEFAULT;
  uint64_t restarts = RESTARTS_DEFAULT;
  uint64_t restart_nodes = RESTART_NODES_DEFAULT;
  uint64_t seed = SEED_DEFAULT;
  constexpr bool valid() const {
    return block_arity >= 3 && block_arity <= 16 && maximum_nodes &&
           restart_nodes;
  }
  static ExactCoverModelParameters from_kernel(const Kernel &);
};

// SAT-only independent-set model for the layered exact-count encoding
// (LymphoSAT's independent-set owner). Recognition is the owner's count
// equation; the search is a generic minimum-degree greedy plus swap local
// search rather than the owner's family-specific constructions. Zero steps or
// rounds select the vertex count (one step per vertex, one propagation round
// per encoding layer).
struct IndependentSetModelParameters {
  static constexpr uint32_t MAXIMUM_VERTICES_DEFAULT = UINT32_MAX;
  static constexpr uint64_t MAXIMUM_STEPS_DEFAULT = 0;
  static constexpr uint64_t COMPLETION_ROUNDS_DEFAULT = 0;
  static constexpr uint64_t SEED_DEFAULT = 0x9e3779b97f4a7c15ULL;

  uint32_t maximum_vertices = MAXIMUM_VERTICES_DEFAULT;
  uint64_t maximum_steps = MAXIMUM_STEPS_DEFAULT;
  uint64_t completion_rounds = COMPLETION_ROUNDS_DEFAULT;
  uint64_t seed = SEED_DEFAULT;
  constexpr bool valid() const { return maximum_vertices >= 2; }
  static IndependentSetModelParameters from_kernel(const Kernel &);
};

// Native RUP refutation of structurally exact bounded-width StConn grids.
// Width controls the crossing-sequence state space; the other bounds expose
// formula extent and emitted-proof work without changing recognition.
struct StconnProofParameters {
  static constexpr uint32_t MAXIMUM_WIDTH_DEFAULT = 9;
  static constexpr uint32_t MAXIMUM_COLUMNS_DEFAULT = UINT32_MAX;
  static constexpr uint64_t MAXIMUM_RUP_CLAUSES_DEFAULT = UINT64_MAX;

  uint32_t maximum_width = MAXIMUM_WIDTH_DEFAULT;
  uint32_t maximum_columns = MAXIMUM_COLUMNS_DEFAULT;
  uint64_t maximum_rup_clauses = MAXIMUM_RUP_CLAUSES_DEFAULT;
  constexpr bool valid() const {
    return maximum_width >= 2 && maximum_width <= 9 &&
           maximum_columns >= 3 && maximum_rup_clauses;
  }
  static StconnProofParameters from_kernel(const Kernel &);
};

// Proof-generating cutting-planes reasoning for complete signed-ternary
// cardinality blocks.  Each block is translated to a reduced BDD and the
// resulting inequalities are combined by minimum-fill Fourier--Motzkin
// elimination.  The bounds govern recognition and certificate construction;
// they do not identify a benchmark or prescribe a particular block count.
struct SignedCardinalityProofParameters {
  static constexpr uint32_t MAXIMUM_VARIABLES_DEFAULT = 4096;
  static constexpr uint32_t MAXIMUM_GROUPS_DEFAULT = 4096;
  static constexpr uint32_t MAXIMUM_CONSTRAINTS_DEFAULT = 262144;
  static constexpr uint32_t MAXIMUM_GROUP_ARITY_DEFAULT = 8;
  static constexpr uint64_t MAXIMUM_BDD_NODES_DEFAULT = UINT64_MAX;
  static constexpr uint64_t MAXIMUM_PROOF_ADDITIONS_DEFAULT = UINT64_MAX;

  uint32_t maximum_variables = MAXIMUM_VARIABLES_DEFAULT;
  uint32_t maximum_groups = MAXIMUM_GROUPS_DEFAULT;
  uint32_t maximum_constraints = MAXIMUM_CONSTRAINTS_DEFAULT;
  uint32_t maximum_group_arity = MAXIMUM_GROUP_ARITY_DEFAULT;
  uint64_t maximum_bdd_nodes = MAXIMUM_BDD_NODES_DEFAULT;
  uint64_t maximum_proof_additions = MAXIMUM_PROOF_ADDITIONS_DEFAULT;
  constexpr bool valid() const {
    return maximum_variables && maximum_groups && maximum_constraints &&
           maximum_group_arity >= 3 && maximum_group_arity <= 16 &&
           maximum_bdd_nodes && maximum_proof_additions;
  }
  static SignedCardinalityProofParameters from_kernel(const Kernel &);
};

// SAT-only low-weight search of an exact projected affine image. Coordinates
// use stable external DIMACS identities; an explicit empty projection is legal.
enum class AffineProjectionMode : uint8_t { Boundary, AllParity, Explicit };
struct AffineSearchScope {
  // AUTO_SPLIT selects half of the current coordinate count. An explicit
  // split is interpreted after the caller-selected coordinate permutation.
  static constexpr uint64_t AUTO_SPLIT = UINT64_MAX;
  uint64_t left_size = AUTO_SPLIT;
  uint64_t left_weight = 0, right_weight = 0, total_weight = 0;
  friend bool operator==(const AffineSearchScope &, const AffineSearchScope &) = default;
};
struct AffineModelParameters {
  static constexpr uint64_t DEFAULT_MAXIMUM_WEIGHT = 9;
  static constexpr uint64_t DEFAULT_IMBALANCE = 2;
  static std::vector<AffineSearchScope> reference_scopes();
  uint64_t maximum_variables = 100000, maximum_constraints = 1000000;
  uint64_t maximum_arity = 8, maximum_projection = 128;
  // Recovered nonunit equations; mandatory root rows do not satisfy this
  // strategy admission threshold. Zero enables even a root-only model search.
  uint64_t minimum_equations = 1;
  uint64_t maximum_entries = 5000000;
  // Separate declared payload budgets; allocator overhead and existing Kernel
  // storage are not part of these bounds. SLURM enforces total process memory.
  uint64_t import_bytes = 268435456, linear_bytes = 268435456;
  uint64_t table_bytes = 268435456, maximum_work = 2000000000;
  uint64_t candidate_budget = 10000, permutations = 5, seed = 1;
  AffineProjectionMode projection_mode = AffineProjectionMode::Boundary;
  bool automatic_negated = false, shuffle_first = false;
  bool free_phase = false, residual_phase = false;
  std::vector<int> projection;
  std::vector<AffineSearchScope> scopes = reference_scopes();
  bool valid() const;
  static AffineModelParameters from_kernel(const Kernel &);
};

void install_rule_parameters(Kernel &, const BceParameters &);
void install_rule_parameters(Kernel &, const TernaryParameters &);
void install_rule_parameters(Kernel &, const FactorParameters &);
void install_rule_parameters(Kernel &, const TransitiveReduceParameters &);
void install_rule_parameters(Kernel &, const SubstituteParameters &);
void install_rule_parameters(Kernel &, const ReduceParameters &);
void install_rule_parameters(Kernel &, const WalkParameters &);
void install_walk_guidance(Kernel &, uint32_t even_break_base_milli,
                           const std::vector<WalkParameters::ScorePoint> &);
void install_rule_parameters(Kernel &, const ReorderParameters &);
void install_rule_parameters(Kernel &, const ParityModelParameters &);
void install_rule_parameters(Kernel &, const OnehotModelParameters &);
void install_rule_parameters(Kernel &, const BalancedSignatureModelParameters &);
void install_rule_parameters(Kernel &, const FermatModelParameters &);
void install_rule_parameters(Kernel &, const LookupCircuitModelParameters &);
void install_rule_parameters(Kernel &, const SimonModelParameters &);
void install_rule_parameters(Kernel &, const ExactCoverModelParameters &);
void install_rule_parameters(Kernel &, const IndependentSetModelParameters &);
void install_rule_parameters(Kernel &, const GraphLabelingParameters &);
void install_rule_parameters(Kernel &, const StconnProofParameters &);
void install_rule_parameters(Kernel &, const SignedCardinalityProofParameters &);
void install_rule_parameters(Kernel &, const AffineModelParameters &);

template <class Parameters> class ScopedRuleParameters {
public:
  ScopedRuleParameters(Kernel &kernel, const Parameters &parameters)
      : kernel_(kernel), saved_(Parameters::from_kernel(kernel)) {
    install_rule_parameters(kernel_, parameters);
  }
  ScopedRuleParameters(const ScopedRuleParameters &) = delete;
  ScopedRuleParameters &operator=(const ScopedRuleParameters &) = delete;
  ~ScopedRuleParameters() { install_rule_parameters(kernel_, saved_); }

private:
  Kernel &kernel_;
  Parameters saved_;
};

using ScopedBceParameters = ScopedRuleParameters<BceParameters>;
using ScopedTernaryParameters = ScopedRuleParameters<TernaryParameters>;
using ScopedFactorParameters = ScopedRuleParameters<FactorParameters>;
using ScopedTransitiveReduceParameters =
    ScopedRuleParameters<TransitiveReduceParameters>;
using ScopedSubstituteParameters = ScopedRuleParameters<SubstituteParameters>;
using ScopedReduceParameters = ScopedRuleParameters<ReduceParameters>;
using ScopedReorderParameters = ScopedRuleParameters<ReorderParameters>;
using ScopedParityModelParameters = ScopedRuleParameters<ParityModelParameters>;
using ScopedOnehotModelParameters = ScopedRuleParameters<OnehotModelParameters>;
using ScopedAffineModelParameters = ScopedRuleParameters<AffineModelParameters>;
using ScopedBalancedSignatureModelParameters =
    ScopedRuleParameters<BalancedSignatureModelParameters>;
using ScopedFermatModelParameters = ScopedRuleParameters<FermatModelParameters>;
using ScopedLookupCircuitModelParameters =
    ScopedRuleParameters<LookupCircuitModelParameters>;
using ScopedSimonModelParameters = ScopedRuleParameters<SimonModelParameters>;
using ScopedExactCoverModelParameters =
    ScopedRuleParameters<ExactCoverModelParameters>;
using ScopedIndependentSetModelParameters =
    ScopedRuleParameters<IndependentSetModelParameters>;
using ScopedStconnProofParameters = ScopedRuleParameters<StconnProofParameters>;
using ScopedSignedCardinalityProofParameters =
    ScopedRuleParameters<SignedCardinalityProofParameters>;

class ScopedWalkParameters {
public:
  ScopedWalkParameters(Kernel &, const WalkParameters &);
  ScopedWalkParameters(const ScopedWalkParameters &) = delete;
  ScopedWalkParameters &operator=(const ScopedWalkParameters &) = delete;
  ~ScopedWalkParameters();

private:
  Kernel &kernel_;
  uint64_t saved_budget_;
  uint64_t saved_seed_;
  uint64_t saved_candidate_;
  uint64_t saved_preparation_budget_;
  uint32_t saved_objective_, saved_admission_;
  uint32_t saved_even_break_base_milli_;
  std::vector<WalkParameters::ScorePoint> saved_score_curve_;
  bool saved_bounded_;
};

} // namespace strategy
} // namespace tactix
