#include "strategy/rule_parameters.hpp"
#include <algorithm>
#include <climits>

#include "kernel/kernel.hpp"

#include <utility>

namespace tactix::strategy {

BceParameters BceParameters::from_kernel(const Kernel &k) {
  return {k.bce_occ_limit};
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
  return true;
}

WalkParameters WalkParameters::from_kernel(const Kernel &k) {
  WalkParameters result;
  result.step_budget = k.work_budget;
  result.random_seed = k.walk_random_seed;
  result.even_break_base_milli = k.walk_even_break_base_milli;
  result.score_curve = k.walk_score_curve;
  return result;
}
ReorderParameters ReorderParameters::from_kernel(const Kernel &k) {
  return {k.reorder_maximum_clause_size, k.reorder_clause_decay_milli,
          k.reorder_binary_weight_milli,
          k.reorder_balanced_polarity_milli};
}
ParityModelParameters ParityModelParameters::from_kernel(const Kernel &k) {
  return {k.parity_model_maximum_variables,
          k.parity_model_maximum_groups,
          k.parity_model_maximum_constraints,
          k.parity_model_minimum_group_clauses,
          k.parity_model_minimum_equations,
          k.parity_model_maximum_free_variables,
          k.parity_model_candidate_budget,
          k.parity_refutation_maximum_variables,
          k.parity_refutation_maximum_groups,
          k.parity_refutation_maximum_constraints,
          k.parity_refutation_maximum_arity,
          k.parity_refutation_probe_constraints,
          k.parity_refutation_minimum_probe_repeats,
          k.parity_refutation_maximum_proof_additions};
}
OnehotModelParameters OnehotModelParameters::from_kernel(const Kernel &k) {
  return {k.onehot_model_maximum_variables,
          k.onehot_model_maximum_domains,
          k.onehot_model_maximum_constraints,
          k.onehot_model_maximum_equation_arity,
          k.onehot_model_minimum_domains,
          k.onehot_model_minimum_equations,
          k.onehot_model_maximum_free_domains,
          k.onehot_model_candidate_budget,
          k.exact_one_refutation_maximum_variables,
          k.exact_one_refutation_maximum_groups,
          k.exact_one_refutation_maximum_constraints,
          k.exact_one_refutation_maximum_arity,
          k.exact_one_refutation_maximum_proof_additions};
}
BalancedSignatureModelParameters
BalancedSignatureModelParameters::from_kernel(const Kernel &k) {
  return k.balanced_signature_model_parameters;
}
FermatModelParameters FermatModelParameters::from_kernel(const Kernel &k) {
  return k.fermat_model_parameters;
}
LookupCircuitModelParameters
LookupCircuitModelParameters::from_kernel(const Kernel &k) {
  return k.lookup_circuit_model_parameters;
}
SimonModelParameters SimonModelParameters::from_kernel(const Kernel &k) {
  return k.simon_model_parameters;
}
ExactCoverModelParameters
ExactCoverModelParameters::from_kernel(const Kernel &k) {
  return k.exact_cover_model_parameters;
}
IndependentSetModelParameters
IndependentSetModelParameters::from_kernel(const Kernel &k) {
  return k.independent_set_model_parameters;
}
StconnProofParameters StconnProofParameters::from_kernel(const Kernel &k) {
  return k.stconn_proof_parameters;
}

SignedCardinalityProofParameters
SignedCardinalityProofParameters::from_kernel(const Kernel &k) {
  return k.signed_cardinality_proof_parameters;
}

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
    variables.push_back(static_cast<uint32_t>(literal < 0 ? -literal : literal));
  }
  std::sort(variables.begin(), variables.end());
  return std::adjacent_find(variables.begin(), variables.end()) == variables.end();
}
AffineModelParameters AffineModelParameters::from_kernel(const Kernel &k) {
  return k.affine_model_parameters;
}
void install_rule_parameters(Kernel &k, const AffineModelParameters &p) {
  k.affine_model_parameters = p;
}

void install_rule_parameters(Kernel &k, const BceParameters &p) {
  k.bce_occ_limit = p.occurrence_limit;
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
  k.walk_even_break_base_milli = p.even_break_base_milli;
  k.walk_score_curve = p.score_curve;
}

ScopedWalkParameters::ScopedWalkParameters(
    Kernel &kernel, const WalkParameters &parameters)
    : kernel_(kernel), saved_budget_(kernel.work_budget),
      saved_seed_(kernel.walk_random_seed),
      saved_even_break_base_milli_(kernel.walk_even_break_base_milli),
      saved_score_curve_(kernel.walk_score_curve),
      saved_bounded_(kernel.work_budget_bounded) {
  install_rule_parameters(kernel_, parameters);
}

ScopedWalkParameters::~ScopedWalkParameters() {
  kernel_.work_budget = saved_budget_;
  kernel_.walk_random_seed = saved_seed_;
  kernel_.walk_even_break_base_milli = saved_even_break_base_milli_;
  kernel_.walk_score_curve = std::move(saved_score_curve_);
  kernel_.work_budget_bounded = saved_bounded_;
}

void install_rule_parameters(Kernel &k, const ReorderParameters &p) {
  k.reorder_maximum_clause_size = p.maximum_clause_size;
  k.reorder_clause_decay_milli = p.clause_decay_milli;
  k.reorder_binary_weight_milli = p.binary_weight_milli;
  k.reorder_balanced_polarity_milli = p.balanced_polarity_milli;
}

void install_rule_parameters(Kernel &k, const ParityModelParameters &p) {
  k.parity_model_maximum_variables = p.maximum_variables;
  k.parity_model_maximum_groups = p.maximum_groups;
  k.parity_model_maximum_constraints = p.maximum_constraints;
  k.parity_model_minimum_group_clauses = p.minimum_group_clauses;
  k.parity_model_minimum_equations = p.minimum_equations;
  k.parity_model_maximum_free_variables = p.maximum_free_variables;
  k.parity_model_candidate_budget = p.candidate_budget;
  k.parity_refutation_maximum_variables = p.refutation_maximum_variables;
  k.parity_refutation_maximum_groups = p.refutation_maximum_groups;
  k.parity_refutation_maximum_constraints = p.refutation_maximum_constraints;
  k.parity_refutation_maximum_arity = p.refutation_maximum_arity;
  k.parity_refutation_probe_constraints = p.refutation_probe_constraints;
  k.parity_refutation_minimum_probe_repeats =
      p.refutation_minimum_probe_repeats;
  k.parity_refutation_maximum_proof_additions =
      p.refutation_maximum_proof_additions;
}

void install_rule_parameters(Kernel &k, const OnehotModelParameters &p) {
  k.onehot_model_maximum_variables = p.maximum_variables;
  k.onehot_model_maximum_domains = p.maximum_domains;
  k.onehot_model_maximum_constraints = p.maximum_constraints;
  k.onehot_model_maximum_equation_arity = p.maximum_equation_arity;
  k.onehot_model_minimum_domains = p.minimum_domains;
  k.onehot_model_minimum_equations = p.minimum_equations;
  k.onehot_model_maximum_free_domains = p.maximum_free_domains;
  k.onehot_model_candidate_budget = p.candidate_budget;
  k.exact_one_refutation_maximum_variables = p.refutation_maximum_variables;
  k.exact_one_refutation_maximum_groups = p.refutation_maximum_groups;
  k.exact_one_refutation_maximum_constraints =
      p.refutation_maximum_constraints;
  k.exact_one_refutation_maximum_arity = p.refutation_maximum_arity;
  k.exact_one_refutation_maximum_proof_additions =
      p.refutation_maximum_proof_additions;
}
void install_rule_parameters(Kernel &k,
                             const BalancedSignatureModelParameters &p) {
  k.balanced_signature_model_parameters = p;
}
void install_rule_parameters(Kernel &k, const FermatModelParameters &p) {
  k.fermat_model_parameters = p;
}
void install_rule_parameters(Kernel &k,
                             const LookupCircuitModelParameters &p) {
  k.lookup_circuit_model_parameters = p;
}
void install_rule_parameters(Kernel &k, const SimonModelParameters &p) {
  k.simon_model_parameters = p;
}
void install_rule_parameters(Kernel &k, const ExactCoverModelParameters &p) {
  k.exact_cover_model_parameters = p;
}
void install_rule_parameters(Kernel &k,
                             const IndependentSetModelParameters &p) {
  k.independent_set_model_parameters = p;
}
void install_rule_parameters(Kernel &k, const StconnProofParameters &p) {
  k.stconn_proof_parameters = p;
}

void install_rule_parameters(Kernel &k,
                             const SignedCardinalityProofParameters &p) {
  k.signed_cardinality_proof_parameters = p;
}

} // namespace tactix::strategy
