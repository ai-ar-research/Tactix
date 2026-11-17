#pragma once

#include "parameter_schema.hpp"
#include "rules/factor/fx_options.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
#include "strategy/local_search.hpp"
#include "strategy/scoped_rule_parameters.hpp"

namespace tactix {
struct Kernel;

namespace strategy {

struct AnalyzeParameters {
  // Kissat minimizedepth; zero disables recursive minimization.
  static constexpr uint32_t MINIMIZE_DEPTH_DEFAULT = 1000;
  // The eager register has four slots; larger values are unrepresentable.
  static constexpr uint32_t EAGER_SUBSUME_CAPACITY = 4;
  TACTIX_ANALYZE_FIELDS(TACTIX_DECLARE_PROFILE_FIELD)

  constexpr bool valid() const {
    return eager_subsume <= EAGER_SUBSUME_CAPACITY;
  }
  static AnalyzeParameters from_kernel(const Kernel &);
};

struct BackboneParameters {
  static constexpr uint32_t INITIAL_ROUNDS_DEFAULT = 100;
  static constexpr uint32_t MAXIMUM_ROUNDS_DEFAULT = 1000;

  uint32_t initial_rounds = INITIAL_ROUNDS_DEFAULT;
  uint32_t maximum_rounds = MAXIMUM_ROUNDS_DEFAULT;

  static BackboneParameters from_kernel(const Kernel &);
  constexpr bool valid() const { return true; }
};

// Canonical-round discovery costs (strategy only); UINT32_MAX uncaps.
struct SubsumeForwardParameters {
  static constexpr uint32_t CLAUSE_SIZE_DEFAULT = 1000;
  static constexpr uint32_t LARGE_OCCURRENCE_DEFAULT = 100;
  static constexpr uint32_t BINARY_OCCURRENCE_DEFAULT = 10000;

  uint32_t clause_size_limit = CLAUSE_SIZE_DEFAULT;
  uint32_t large_occurrence_limit = LARGE_OCCURRENCE_DEFAULT;
  uint32_t binary_occurrence_limit = BINARY_OCCURRENCE_DEFAULT;

  static SubsumeForwardParameters from_kernel(const Kernel &);
  constexpr bool valid() const { return true; }
};

// Kissat's fused-forward discovery costs; explicit C2 bypasses them.
struct StrengthenParameters {
  static constexpr uint32_t CLAUSE_SIZE_DEFAULT = 1000;
  static constexpr uint32_t OCCURRENCE_DEFAULT = 1000;

  uint32_t clause_size_limit = CLAUSE_SIZE_DEFAULT;
  uint32_t occurrence_limit = OCCURRENCE_DEFAULT;

  static StrengthenParameters from_kernel(const Kernel &);
  constexpr bool valid() const { return true; }
};

// Canonical X4 discovery policy; explicit instances bypass it.
struct InstantiateParameters {
  static constexpr uint32_t CLAUSE_SIZE_DEFAULT = 3;
  static constexpr uint32_t OCCURRENCE_DEFAULT = 1;
  static constexpr uint32_t UNASSIGNED_DEFAULT = 3;
  static constexpr uint32_t NEW_MATERIAL_DEFAULT = 1;

  // Zero removes the size/unassigned lower bound. UINT32_MAX removes the
  // occurrence upper bound. Learned clauses stay outside the reference
  // canonical round unless the caller opts in.
  TACTIX_INSTANTIATE_FIELDS(TACTIX_DECLARE_PROFILE_FIELD)

  static InstantiateParameters from_kernel(const Kernel &);
  constexpr bool valid() const { return true; }
};

enum class CongruenceBinaryWatchOrder : uint8_t {
  Canonical = 0,
  PreserveExisting = 1,
};

struct CongruenceParameters {
  static constexpr uint32_t AND_ARITY_DEFAULT = 1000000;
  static constexpr size_t GATE_COUNT_DEFAULT = 20000000;
  static constexpr uint32_t XOR_ARITY_DEFAULT = 4;
  static constexpr uint32_t XOR_ARITY_CAPACITY = 4;

  // Zero removes a discovery limit except xor_arity_limit, where zero disables
  // XOR extraction. The fixed-key implementation currently validates against
  // XOR_ARITY_CAPACITY rather than silently truncating a caller request.
  uint32_t and_arity_limit = AND_ARITY_DEFAULT;
  size_t gate_count_limit = GATE_COUNT_DEFAULT;
  uint32_t xor_arity_limit = XOR_ARITY_DEFAULT;
  // The ordinary pass keeps the reference dense-resume order.
  CongruenceBinaryWatchOrder binary_watch_order =
      CongruenceBinaryWatchOrder::Canonical;

  static CongruenceParameters from_kernel(const Kernel &);
  constexpr bool valid() const {
    return xor_arity_limit <= XOR_ARITY_CAPACITY &&
           (binary_watch_order == CongruenceBinaryWatchOrder::Canonical ||
            binary_watch_order == CongruenceBinaryWatchOrder::PreserveExisting);
  }
};

// A bounded discovery-and-proof request; zero limits disable work.
struct ParityConsequenceParameters {
  uint64_t maximum_clauses = 0, maximum_groups = 0, selection_work = 0;
  uint32_t intermediate_width = 0, source_count = 0, output_width = 0;
  bool descending = false;
  uint64_t maximum_proof_additions = 0;
  uint32_t maximum_tree_variables = 0;
  uint64_t consequence = 0;
  bool valid() const { return output_width <= 4; }
  static ParityConsequenceParameters from_kernel(const Kernel &);
};

// Strategy-owned controls for one Vivify invocation.  They select the
// candidate band, reuse/flush policy, and the reference population threshold;
// none is a soundness precondition on an explicit clause action.
struct VivifyParameters {
  static constexpr uint32_t IRREDUNDANT_SORT_RATIO_DEFAULT = 10;

  uint8_t scope = 0;
  bool reuse = true;
  bool flush = false;
  uint32_t irredundant_sort_ratio = IRREDUNDANT_SORT_RATIO_DEFAULT;
  // One band of a cumulative dose ledger (vivify.c: each tier's share joins
  // one probing-tick ceiling). With `band`, the application is bounded by
  // `band_budget` measured from a dose start `band_offset` ticks earlier, so
  // unused work of an earlier band carries into later ones and an overrun is
  // precharged. Per application only: installs and from_kernel leave it off.
  bool band = false;
  uint64_t band_budget = 0;
  uint64_t band_offset = 0;

  constexpr bool valid() const {
    return scope <= 5 && irredundant_sort_ratio != 0 &&
           (band || (!band_budget && !band_offset));
  }

  static VivifyParameters from_kernel(const Kernel &);
};

inline bool vivify_skip_global_order(uint64_t irredundant,
                                     uint64_t redundant,
                                     const VivifyParameters &parameters) {
  return irredundant / parameters.irredundant_sort_ratio > redundant;
}

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
  TACTIX_FACTOR_FIELDS(TACTIX_DECLARE_PROFILE_FIELD)
  bool fx = false;
  FxOptions fx_options{};
  constexpr bool valid() const { return !fx || fx_options.valid(); }
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

  TACTIX_ONEHOT_MODEL_FIELDS(TACTIX_DECLARE_PROFILE_FIELD)
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

  TACTIX_DECLARE_PARAMETER_FIELDS(BalancedSignatureModelParameters)
  constexpr bool valid() const {
    return maximum_balls >= 3 && maximum_width && maximum_width <= 12 &&
           restarts && steps_per_restart && samples_per_step &&
           completion_rounds;
  }
  static BalancedSignatureModelParameters from_kernel(const Kernel &);
};

TACTIX_VISIT_PARAMETER_FIELDS(BalancedSignatureModelParameters)

// SAT-only reconstruction for circuits encoding (a-b)(a+b)=N: recover N from
// the final comparator, factor with deterministic 64-bit Miller--Rabin/Pollard
// Rho, validate against the live CNF. The owner's limits, exposed for dosing.
struct FermatModelParameters {
  static constexpr uint32_t MAXIMUM_BITS_DEFAULT = 63;
  static constexpr uint32_t COMPARATOR_TAIL_CLAUSES_DEFAULT = 80;
  static constexpr uint64_t RHO_CONSTANTS_DEFAULT = 199;
  static constexpr uint64_t RHO_ITERATIONS_DEFAULT = 200000;

  TACTIX_DECLARE_PARAMETER_FIELDS(FermatModelParameters)
  constexpr bool valid() const {
    return maximum_bits && maximum_bits <= 63 &&
           comparator_tail_clauses >= 2 &&
           comparator_tail_clauses <= COMPARATOR_TAIL_CLAUSES_DEFAULT &&
           rho_constants && rho_iterations;
  }
  static FermatModelParameters from_kernel(const Kernel &);
};

TACTIX_VISIT_PARAMETER_FIELDS(FermatModelParameters)

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

  TACTIX_DECLARE_PARAMETER_FIELDS(LookupCircuitModelParameters)
  constexpr bool valid() const {
    return maximum_fanin && maximum_fanin <= 8 && maximum_fanout &&
           maximum_fanout <= 8 && maximum_primary_inputs &&
           maximum_primary_inputs <= 64 && maximum_support &&
           maximum_support <= 63 && maximum_vector_gate_evaluations;
  }
  static LookupCircuitModelParameters from_kernel(const Kernel &);
};

TACTIX_VISIT_PARAMETER_FIELDS(LookupCircuitModelParameters)

// SAT-only key search for Simon-family round circuits; recognition recovers key
// permutation and per-round selection, the controls bound depth and 32-bit key
// work.
struct SimonModelParameters {
  static constexpr uint32_t MAXIMUM_ROUNDS_DEFAULT = 64;
  static constexpr uint64_t MAXIMUM_CANDIDATES_DEFAULT = uint64_t{1} << 32;

  TACTIX_DECLARE_PARAMETER_FIELDS(SimonModelParameters)
  constexpr bool valid() const {
    return maximum_rounds && maximum_rounds <= 64 && maximum_candidates;
  }
  static SimonModelParameters from_kernel(const Kernel &);
};

TACTIX_VISIT_PARAMETER_FIELDS(SimonModelParameters)

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

  TACTIX_DECLARE_PARAMETER_FIELDS(ExactCoverModelParameters)
  constexpr bool valid() const {
    return block_arity >= 3 && block_arity <= 16 && maximum_nodes &&
           restart_nodes;
  }
  static ExactCoverModelParameters from_kernel(const Kernel &);
};

TACTIX_VISIT_PARAMETER_FIELDS(ExactCoverModelParameters)

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

  TACTIX_DECLARE_PARAMETER_FIELDS(IndependentSetModelParameters)
  constexpr bool valid() const { return maximum_vertices >= 2; }
  static IndependentSetModelParameters from_kernel(const Kernel &);
};

TACTIX_VISIT_PARAMETER_FIELDS(IndependentSetModelParameters)

// Native RUP refutation of exact bounded-width StConn grids; width sets the
// crossing-sequence state space, the other bounds formula extent and proof
// work.
struct StconnProofParameters {
  static constexpr uint32_t MAXIMUM_WIDTH_DEFAULT = 9;
  static constexpr uint32_t MAXIMUM_COLUMNS_DEFAULT = UINT32_MAX;
  static constexpr uint64_t MAXIMUM_RUP_CLAUSES_DEFAULT = UINT64_MAX;

  TACTIX_DECLARE_PARAMETER_FIELDS(StconnProofParameters)
  constexpr bool valid() const {
    return maximum_width >= 2 && maximum_width <= 9 &&
           maximum_columns >= 3 && maximum_rup_clauses;
  }
  static StconnProofParameters from_kernel(const Kernel &);
};

TACTIX_VISIT_PARAMETER_FIELDS(StconnProofParameters)

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

  TACTIX_DECLARE_PARAMETER_FIELDS(SignedCardinalityProofParameters)
  constexpr bool valid() const {
    return maximum_variables && maximum_groups && maximum_constraints &&
           maximum_group_arity >= 3 && maximum_group_arity <= 16 &&
           maximum_bdd_nodes && maximum_proof_additions;
  }
  static SignedCardinalityProofParameters from_kernel(const Kernel &);
};

TACTIX_VISIT_PARAMETER_FIELDS(SignedCardinalityProofParameters)

enum class GraphLabelingStage : uint8_t {
  Recognition, Construction, Window, Repair, Anneal, FinalRepair,
  Completion, Publication, Count
};
constexpr size_t GRAPH_LABELING_STAGES = static_cast<size_t>(GraphLabelingStage::Count);

// A single constructor action has separately dosed algorithm stages. These
// allowances sum to its effort ceiling; zero disables a stage. No wall-clock
// calibration or solver-specific performance default is invented here. The
// owning policy must install its reviewed nonzero effort profile explicitly.
struct GraphLabelingParameters {
  uint32_t maximum_vertices = UINT32_MAX;
  uint64_t workspace_bytes = UINT64_MAX;
  std::array<uint64_t, GRAPH_LABELING_STAGES> work{};
  // LymphoSAT's bounded window-search profile.
  uint32_t maximum_dfs_vertices = 140;
  uint64_t maximum_dfs_nodes = 50000000;
  uint32_t maximum_dfs_starts = 12;
  uint32_t early_dfs_choices = 28;
  uint64_t repair_rounds = UINT64_MAX;
  uint64_t anneal_iterations = UINT64_MAX;
  uint64_t stall_anneal_iterations = 0;
  uint64_t seed = 0;

  bool operator==(const GraphLabelingParameters &) const = default;
  constexpr bool valid() const {
    if (maximum_vertices < 3) return false;
    uint64_t remaining = UINT64_MAX;
    for (uint64_t allowance : work) {
      if (allowance > remaining) return false;
      remaining -= allowance;
    }
    return true;
  }
  static GraphLabelingParameters from_kernel(const Kernel &);
};

// Caller-owned bounds. Zero disables work; this is not a production profile.
struct RamseyStartupParameters {
  uint64_t recognition_clauses = 0, recognition_variables = 0;
  uint64_t recognition_pair_cells = 0, recognition_work = 0;
  uint64_t certificate_work = 0, maximum_nodes = 0, maximum_additions = 0;
  uint32_t maximum_external_extent = 0;
  bool valid() const {
    return maximum_external_extent <= INT32_MAX &&
           recognition_work <= UINT64_MAX - certificate_work;
  }
  static RamseyStartupParameters from_kernel(const Kernel &);
};

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

void install_rule_parameters(Kernel &, const AnalyzeParameters &);
void install_rule_parameters(Kernel &, const BackboneParameters &);
void install_rule_parameters(Kernel &, const SubsumeForwardParameters &);
void install_rule_parameters(Kernel &, const StrengthenParameters &);
void install_rule_parameters(Kernel &, const InstantiateParameters &);
void install_rule_parameters(Kernel &, const CongruenceParameters &);
void install_rule_parameters(Kernel &, const ParityConsequenceParameters &);
void install_rule_parameters(Kernel &, const VivifyParameters &);
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
void install_rule_parameters(Kernel &, const RamseyStartupParameters &);

using ScopedAnalyzeParameters = ScopedRuleParameters<AnalyzeParameters>;
using ScopedBackboneParameters = ScopedRuleParameters<BackboneParameters>;
using ScopedSubsumeForwardParameters = ScopedRuleParameters<SubsumeForwardParameters>;
using ScopedStrengthenParameters = ScopedRuleParameters<StrengthenParameters>;
using ScopedCongruenceParameters = ScopedRuleParameters<CongruenceParameters>;
using ScopedVivifyParameters = ScopedRuleParameters<VivifyParameters>;
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
