#include "strategy/rule_parameters.hpp"

#include "kernel/types.hpp"

#include <algorithm>
#include <climits>

#include "kernel/kernel.hpp"
#include "strategy/congruence_parameters.hpp"
#include "strategy/failed_literal_parameters.hpp"
#include "strategy/forward_parameters.hpp"
#include "strategy/instantiate_parameters.hpp"
#include "strategy/parity_consequence_parameters.hpp"

#include <utility>

namespace tactix::strategy {

BceParameters BceParameters::from_kernel(const Kernel &k) {
  return {k.bce_occ_limit, k.bce_preserve_query};
}
TernaryParameters TernaryParameters::from_kernel(const Kernel &k) {
  return {k.ternary_occ_limit, k.ternary_max_add_percent};
}
FactorParameters FactorParameters::from_kernel(const Kernel &k) {
  return {k.factor_size, k.factor_bound, k.factor_cand_rounds,
          k.factor_composite, k.factor_demote_fresh};
}
TransitiveReduceParameters
TransitiveReduceParameters::from_kernel(const Kernel &k) {
  return {k.transitive_early_exit, k.transitive_live_meter,
          k.transitive_budget_checked};
}
SubstituteParameters SubstituteParameters::from_kernel(const Kernel &k) {
  return {k.subst_demote};
}
ReduceParameters ReduceParameters::from_kernel(const Kernel &k) {
  return {k.tier1, k.tier2, k.reduce_victim_numerator,
          k.reduce_victim_denominator, k.reduce_protect_tiers,
          k.reduce_compact_collection, k.reduce_bookkeeping};
}
std::vector<WalkParameters::ScorePoint>
WalkParameters::reference_score_curve() {
  return {{0, 2000}, {3000, 2500}, {4000, 2850},
          {5000, 3700}, {6000, 5100}, {7000, 7400}};
}

bool WalkParameters::valid() const {
  if (objective == Objective::LegacyWalk) {
    if (admission != Admission::Legacy || preparation_budget) return false;
  } else if (objective != Objective::FullCurrentCnf ||
             admission != Admission::StrictlyBetter || !seed_candidate) {
    return false;
  }
  if (seed_candidate && !valid_local_search_candidate(seed_candidate))
    return false;
  if (!even_break_base_milli || score_curve.size() < 2 ||
      score_curve.front().average_clause_size_milli != 0)
    return false;
  for (size_t i = 0; i < score_curve.size(); ++i) {
    if (!score_curve[i].break_base_milli)
      return false;
    if (i && score_curve[i - 1].average_clause_size_milli >=
                 score_curve[i].average_clause_size_milli)
      return false;
    if (i && score_curve[i - 1].break_base_milli >
                 score_curve[i].break_base_milli)
      return false;
  }
  if (algorithm != Algorithm::ProbSat &&
      algorithm != Algorithm::ConfigurationChecking)
    return false;
  if (initialization != Initialization::BestPhases &&
      initialization != Initialization::Negative &&
      initialization != Initialization::Random)
    return false;
  if (algorithm == Algorithm::ConfigurationChecking &&
      (seed_candidate || objective != Objective::LegacyWalk ||
       admission != Admission::Legacy || preparation_budget))
    return false;
  if (algorithm == Algorithm::ProbSat &&
      initialization != Initialization::BestPhases)
    return false;
  if (!maximum_tries || !flips_per_try || !weight_smoothing_threshold ||
      !smoothing_denominator ||
      smoothing_old_numerator > smoothing_denominator)
    return false;
  return true;
}

WalkParameters WalkParameters::from_kernel(const Kernel &k) {
  WalkParameters result;
  result.step_budget = k.work_budget;
  result.random_seed = k.walk_random_seed;
  result.even_break_base_milli = k.walk_even_break_base_milli;
  result.score_curve = k.walk_score_curve;
  result.seed_candidate = k.walk_seed_candidate;
  result.objective = static_cast<WalkParameters::Objective>(k.walk_objective);
  result.admission = static_cast<WalkParameters::Admission>(k.walk_admission);
  result.preparation_budget = k.walk_preparation_budget;
  result.algorithm = static_cast<WalkParameters::Algorithm>(k.walk_algorithm);
  result.initialization =
      static_cast<WalkParameters::Initialization>(k.walk_initialization);
  result.maximum_tries = k.walk_maximum_tries;
  result.flips_per_try = k.walk_flips_per_try;
  result.weight_smoothing_threshold = k.walk_weight_smoothing_threshold;
  result.smoothing_old_numerator = k.walk_smoothing_old_numerator;
  result.smoothing_denominator = k.walk_smoothing_denominator;
  result.aspiration = k.walk_aspiration;
  return result;
}
#define STORED_PARAMETERS(Type, member)                                       \
  Type Type::from_kernel(const Kernel &kernel) { return kernel.member; }       \
  void install_rule_parameters(Kernel &kernel, const Type &parameters) {      \
    kernel.member = parameters;                                                \
  }

STORED_PARAMETERS(BalancedSignatureModelParameters,
                  balanced_signature_model_parameters)
STORED_PARAMETERS(FermatModelParameters, fermat_model_parameters)
STORED_PARAMETERS(LookupCircuitModelParameters,
                  lookup_circuit_model_parameters)
STORED_PARAMETERS(SimonModelParameters, simon_model_parameters)
STORED_PARAMETERS(ExactCoverModelParameters, exact_cover_model_parameters)
STORED_PARAMETERS(GraphLabelingParameters, graph_labeling_parameters)
STORED_PARAMETERS(IndependentSetModelParameters,
                  independent_set_model_parameters)
STORED_PARAMETERS(StconnProofParameters, stconn_proof_parameters)
STORED_PARAMETERS(SignedCardinalityProofParameters,
                  signed_cardinality_proof_parameters)
STORED_PARAMETERS(AffineModelParameters, affine_model_parameters)
STORED_PARAMETERS(RamseyStartupParameters, ramsey_startup_parameters)
STORED_PARAMETERS(ParityConsequenceParameters, parity_consequence_parameters)
STORED_PARAMETERS(InstantiateParameters, instantiate_parameters)
STORED_PARAMETERS(BackboneParameters, backbone_parameters)
STORED_PARAMETERS(CongruenceParameters, congruence_parameters)
STORED_PARAMETERS(SubsumeForwardParameters, subsume_forward_parameters)
STORED_PARAMETERS(StrengthenParameters, strengthen_parameters)
STORED_PARAMETERS(ReorderParameters, reorder_parameters)
STORED_PARAMETERS(ParityModelParameters, parity_model_parameters)
STORED_PARAMETERS(OnehotModelParameters, onehot_model_parameters)

#undef STORED_PARAMETERS

std::vector<AffineSearchScope> AffineModelParameters::reference_scopes() {
  std::vector<AffineSearchScope> result;
  for (uint64_t weight = 0; weight <= DEFAULT_MAXIMUM_WEIGHT; ++weight) {
    const uint64_t low = weight / 2;
    const uint64_t high = std::min(weight, low + DEFAULT_IMBALANCE);
    result.push_back({AffineSearchScope::AUTO_SPLIT, low, high, weight});
    if (low != high)
      result.push_back({AffineSearchScope::AUTO_SPLIT, high, low, weight});
  }
  return result;
}
bool AffineModelParameters::valid() const {
  if (projection_mode < AffineProjectionMode::Boundary ||
      projection_mode > AffineProjectionMode::Explicit)
    return false;
  if (projection_mode != AffineProjectionMode::Explicit && !projection.empty())
    return false;
  std::vector<uint32_t> variables;
  variables.reserve(projection.size());
  for (int literal : projection) {
    if (!literal || literal == INT_MIN) return false;
    variables.push_back(external_variable_of(literal));
  }
  std::sort(variables.begin(), variables.end());
  return std::adjacent_find(variables.begin(), variables.end()) == variables.end();
}

void install_rule_parameters(Kernel &k, const BceParameters &p) {
  k.bce_occ_limit = p.occurrence_limit;
  k.bce_preserve_query = p.preserve_query;
}

void install_rule_parameters(Kernel &k, const TernaryParameters &p) {
  k.ternary_occ_limit = p.occurrence_limit;
  k.ternary_max_add_percent = p.maximum_add_percent;
}

void install_rule_parameters(Kernel &k, const FactorParameters &p) {
  k.factor_size = p.maximum_clause_size;
  k.factor_bound = p.growth_bound;
  k.factor_cand_rounds = p.candidate_rounds;
  k.factor_composite = p.composite;
  k.factor_demote_fresh = p.demote_fresh;
}

void install_rule_parameters(Kernel &k,
                             const TransitiveReduceParameters &p) {
  k.transitive_early_exit = p.early_exit;
  k.transitive_live_meter = p.live_meter;
  k.transitive_budget_checked = p.checked_budget_meter;
}

void install_rule_parameters(Kernel &k, const SubstituteParameters &p) {
  k.subst_demote = p.demote_substituted;
}

void install_rule_parameters(Kernel &k, const ReduceParameters &p) {
  k.tier1 = p.tier1;
  k.tier2 = p.tier2;
  k.reduce_victim_numerator = p.victim_numerator;
  k.reduce_victim_denominator = p.victim_denominator;
  k.reduce_protect_tiers = p.protect_tiers;
  k.reduce_compact_collection = p.compact_collection;
  k.reduce_bookkeeping = p.reduction_bookkeeping;
}

void install_rule_parameters(Kernel &k, const WalkParameters &p) {
  k.work_budget = p.step_budget;
  k.work_budget_bounded = true;
  k.walk_random_seed = p.random_seed;
  k.walk_seed_candidate = p.seed_candidate;
  k.walk_objective = static_cast<uint32_t>(p.objective);
  k.walk_admission = static_cast<uint32_t>(p.admission);
  k.walk_preparation_budget = p.preparation_budget;
  k.walk_algorithm = static_cast<uint32_t>(p.algorithm);
  k.walk_initialization = static_cast<uint32_t>(p.initialization);
  k.walk_maximum_tries = p.maximum_tries;
  k.walk_flips_per_try = p.flips_per_try;
  k.walk_weight_smoothing_threshold = p.weight_smoothing_threshold;
  k.walk_smoothing_old_numerator = p.smoothing_old_numerator;
  k.walk_smoothing_denominator = p.smoothing_denominator;
  k.walk_aspiration = p.aspiration;
  install_walk_guidance(k, p.even_break_base_milli, p.score_curve);
}

void install_walk_guidance(
    Kernel &k, uint32_t even_break_base_milli,
    const std::vector<WalkParameters::ScorePoint> &score_curve) {
  k.walk_even_break_base_milli = even_break_base_milli;
  k.walk_score_curve = score_curve;
}

ScopedWalkParameters::BoundedMark::BoundedMark(Kernel &kernel)
    : kernel_(kernel), saved_(kernel.work_budget_bounded) {}

ScopedWalkParameters::BoundedMark::~BoundedMark() {
  kernel_.work_budget_bounded = saved_;
}

ScopedWalkParameters::ScopedWalkParameters(
    Kernel &kernel, const WalkParameters &parameters)
    : bounded_(kernel), parameters_(kernel, parameters) {}

ScopedWalkParameters::~ScopedWalkParameters() = default;
} // namespace tactix::strategy
