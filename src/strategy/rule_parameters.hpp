#pragma once

#include <cstdint>
#include <optional>
#include <vector>
#include "strategy/local_search.hpp"
#include "strategy/scoped_rule_parameters.hpp"
#include "strategy/graph_labeling_parameters.hpp"
#include "strategy/ramsey_startup_parameters.hpp"

namespace tactix {
struct Kernel;

namespace strategy {

struct BceParameters {
  uint32_t occurrence_limit = 0;
  bool preserve_query = false; // canonical batches; instances stay complete
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
  // Rounds within one watch session (0: one canonical round). Another round
  // runs only after one retired a variable and, with `round_effort_ceiling`,
  // only while its Substitute effort stayed within the rule work budget
  // (substitute.c rounds). Per application only: installs and from_kernel leave
  // both zero.
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

  // Occurrence-list allowance, also charged for seeding; zero means none.
  uint64_t step_budget = STEP_BUDGET_DEFAULT;
  uint64_t random_seed = 0;
  // Kissat alternates this base with the fitted curve between calls.
  uint32_t even_break_base_milli = EVEN_BREAK_BASE_DEFAULT;
  std::vector<ScorePoint> score_curve = reference_score_curve();
  // Exact retained candidate identity, or zero for native phase seeding.
  uint64_t seed_candidate = 0;
  enum class Objective : uint32_t { LegacyWalk = 0, FullCurrentCnf = 1 };
  enum class Admission : uint32_t { Legacy = 0, StrictlyBetter = 1 };
  enum class Algorithm : uint32_t {
    ProbSat = 0,
    ConfigurationChecking = 1,
  };
  enum class Initialization : uint32_t {
    BestPhases = 0,
    Negative = 1,
    Random = 2,
  };
  Objective objective = Objective::LegacyWalk;
  Admission admission = Admission::Legacy;
  uint64_t preparation_budget = 0;
  // Configuration-checking local search (CCLS), inert under Algorithm::ProbSat.
  // Defaults follow CCAnr/CryptoMiniSat: 100 tries, one million flips per try,
  // weight-50 smoothing threshold, 0.3/0.7 smoothing.
  Algorithm algorithm = Algorithm::ProbSat;
  Initialization initialization = Initialization::BestPhases;
  uint32_t maximum_tries = 100;
  uint64_t flips_per_try = 1000000;
  uint32_t weight_smoothing_threshold = 50;
  uint32_t smoothing_old_numerator = 3;
  uint32_t smoothing_denominator = 10;
  bool aspiration = true;
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

// Structural XOR reasoning: exact bounded-arity encodings can yield a checkable
// even-incidence refutation; otherwise inferred affine restrictions guide SAT
// search, accepted only after the live CNF validates a candidate.
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
  // Recognition and preflight bound default work; no certificate cutoff.
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

// SAT-only search over exact one-hot GF(3) affine encodings.
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

// SAT-only construction for balance designs encoded by pair choices and signed
// ternary signatures. The work controls are the LymphoSAT owner's neighborhood
// schedule, typed so online policies can shorten, lengthen or reseed it.
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

template <class Parameters, class Visitor>
auto visit_parameter_fields(FieldsOf<BalancedSignatureModelParameters>,
                            Parameters &p, Visitor visit) {
  return visit(p.maximum_balls, p.maximum_width, p.restarts,
               p.steps_per_restart, p.samples_per_step, p.seed,
               p.completion_rounds);
}

// SAT-only reconstruction for circuits encoding (a-b)(a+b)=N: recover N from
// the final comparator, factor with deterministic 64-bit Miller--Rabin/Pollard
// Rho, validate against the live CNF. The owner's limits, exposed for dosing.
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

template <class Parameters, class Visitor>
auto visit_parameter_fields(FieldsOf<FermatModelParameters>, Parameters &p,
                            Visitor visit) {
  return visit(p.maximum_bits, p.comparator_tail_clauses, p.rho_constants,
               p.rho_iterations);
}

// SAT-only reconstruction of feed-forward lookup-table circuits: byte-lane SIMD
// enumeration of the primary-input support, else occurrence-queued gate-CSP
// search. Public work controls.
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

template <class Parameters, class Visitor>
auto visit_parameter_fields(FieldsOf<LookupCircuitModelParameters>,
                            Parameters &p, Visitor visit) {
  return visit(p.maximum_fanin, p.maximum_fanout, p.maximum_primary_inputs,
               p.maximum_support, p.maximum_vector_gate_evaluations);
}

// SAT-only key search for Simon-family round circuits; recognition recovers key
// permutation and per-round selection, the controls bound depth and 32-bit key
// work.
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

template <class Parameters, class Visitor>
auto visit_parameter_fields(FieldsOf<SimonModelParameters>, Parameters &p,
                            Visitor visit) {
  return visit(p.maximum_rounds, p.maximum_candidates);
}

// SAT-only exact-cover model for signed at-most-one block formulas (LymphoSAT's
// sgen owner): exact recognition, deterministic minimum-remaining-rows DFS,
// then randomized restarts. Node bounds are per attempt; the owner's phases are
// wall-clock bounded, so restarts default to none.
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

template <class Parameters, class Visitor>
auto visit_parameter_fields(FieldsOf<ExactCoverModelParameters>,
                            Parameters &p, Visitor visit) {
  return visit(p.block_arity, p.maximum_nodes, p.restarts, p.restart_nodes,
               p.seed);
}

// Graph-model action for the layered exact-count independent-set encoding and
// the clause-authenticated directed stable-extension core. The stable-extension
// form searches a private projection after authenticating every omitted guarded
// resolvent, then validates on the live formula. Zero steps select the default
// dose or the vertex count; zero rounds one completion round per layer.
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

template <class Parameters, class Visitor>
auto visit_parameter_fields(FieldsOf<IndependentSetModelParameters>,
                            Parameters &p, Visitor visit) {
  return visit(p.maximum_vertices, p.maximum_steps, p.completion_rounds,
               p.seed);
}

// Native RUP refutation of exact bounded-width StConn grids; width sets the
// crossing-sequence state space, the other bounds formula extent and proof
// work.
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

template <class Parameters, class Visitor>
auto visit_parameter_fields(FieldsOf<StconnProofParameters>, Parameters &p,
                            Visitor visit) {
  return visit(p.maximum_width, p.maximum_columns, p.maximum_rup_clauses);
}

// Proof-generating cutting planes for complete signed-ternary cardinality
// blocks: each block becomes a reduced BDD, combined by minimum-fill
// Fourier--Motzkin elimination. The bounds govern recognition and certificate
// construction.
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

template <class Parameters, class Visitor>
auto visit_parameter_fields(FieldsOf<SignedCardinalityProofParameters>,
                            Parameters &p, Visitor visit) {
  return visit(p.maximum_variables, p.maximum_groups, p.maximum_constraints,
               p.maximum_group_arity, p.maximum_bdd_nodes,
               p.maximum_proof_additions);
}

// SAT-only low-weight search of a projected affine image (DIMACS ids).
enum class AffineProjectionMode : uint8_t { Boundary, AllParity, Explicit };
struct AffineSearchScope {
  // AUTO_SPLIT halves the coordinates, after the caller's permutation.
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
  // Recovered nonunit equations required; zero admits root-only search.
  uint64_t minimum_equations = 1;
  uint64_t maximum_entries = 5000000;
  // Declared payload budgets, excluding allocator and kernel storage.
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

using ScopedReduceParameters = ScopedRuleParameters<ReduceParameters>;

// A Walk profile marks the budget bounded; restore the mark afterwards.
class ScopedWalkParameters {
public:
  ScopedWalkParameters(Kernel &, const WalkParameters &);
  ScopedWalkParameters(const ScopedWalkParameters &) = delete;
  ScopedWalkParameters &operator=(const ScopedWalkParameters &) = delete;
  ~ScopedWalkParameters();

private:
  // Declared first: saves the mark before install, restores it last.
  class BoundedMark {
  public:
    explicit BoundedMark(Kernel &);
    ~BoundedMark();

  private:
    Kernel &kernel_;
    bool saved_;
  };
  BoundedMark bounded_;
  ScopedRuleParameters<WalkParameters> parameters_;
};

} // namespace strategy
} // namespace tactix
