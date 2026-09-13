#include "strategy/rule_parameters.hpp"

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
          k.reduce_compact_collection};
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

} // namespace tactix::strategy
