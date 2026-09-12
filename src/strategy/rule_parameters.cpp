#include "strategy/rule_parameters.hpp"

#include "kernel/kernel.hpp"

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
  return {k.tier1, k.tier2};
}
WalkParameters WalkParameters::from_kernel(const Kernel &k) {
  return {k.work_budget, k.walk_random_seed};
}
ReorderParameters ReorderParameters::from_kernel(const Kernel &k) {
  return {k.reorder_maximum_clause_size};
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
}

void install_rule_parameters(Kernel &k, const WalkParameters &p) {
  k.work_budget = p.step_budget;
  k.work_budget_bounded = true;
  k.walk_random_seed = p.random_seed;
}

ScopedWalkParameters::ScopedWalkParameters(
    Kernel &kernel, const WalkParameters &parameters)
    : kernel_(kernel), saved_budget_(kernel.work_budget),
      saved_seed_(kernel.walk_random_seed),
      saved_bounded_(kernel.work_budget_bounded) {
  install_rule_parameters(kernel_, parameters);
}

ScopedWalkParameters::~ScopedWalkParameters() {
  kernel_.work_budget = saved_budget_;
  kernel_.walk_random_seed = saved_seed_;
  kernel_.work_budget_bounded = saved_bounded_;
}

void install_rule_parameters(Kernel &k, const ReorderParameters &p) {
  k.reorder_maximum_clause_size = p.maximum_clause_size;
}

} // namespace tactix::strategy
