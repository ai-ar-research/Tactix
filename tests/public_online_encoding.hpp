#pragma once

// A typed online step as a tactix_online_step_v2, the inverse of the C API's
// step conversion (api.cpp online_program / online_program_v2). Include after
// public_parameter_encoding.hpp. A step with a field or payload that has no
// v2 form returns false; callers verify the round trip by configuring the
// encoding and comparing the converted request with the original.

#include "public_parameter_encoding.hpp"

#include "kernel/kernel.hpp"

#include <memory>
#include <vector>

namespace public_encoding {

struct EncodedStep {
  tactix_online_step_v2 step{};
  Encoded parameters;
  std::vector<uint64_t> instances;
  std::vector<std::vector<int>> sr_literals; // clause, witness per instance
  std::vector<std::vector<tactix_online_sr_mapping>> sr_mappings;
  std::vector<tactix_online_sr_instance> sr_instances;
  EncodedStep() = default;
  EncodedStep(const EncodedStep &) = delete;
  EncodedStep &operator=(const EncodedStep &) = delete;
};

inline bool encode_step(const Kernel &k, const strategy::OnlineInvocation &in,
                        int rule_id, EncodedStep &out) {
  using namespace strategy;
  if (!in.factor_grids.empty() || !in.condition_instances.empty() ||
      !in.independent_set_cover_instances.empty() || !in.gf3_affine_instances.empty() ||
      !in.affine_basis_instances.empty() || !in.gf3_gaussian_instances.empty() ||
      !in.tseitin_cycle_instances.empty() || !in.direct_csp_support_instances.empty() ||
      !in.nary_csp_equality_instances.empty() ||
      !in.nary_csp_equality_transitivity_instances.empty() ||
      !in.nary_csp_equality_amo_instances.empty() ||
      !in.gf3_general_row_instances.empty() ||
      !in.direct_csp_guarded_cover_instances.empty())
    return false; // payloads of later step versions
  tactix_online_step &c = out.step.base;
  c.step_id = in.id;
  c.rule_id = rule_id;
  if (std::holds_alternative<std::monostate>(in.parameters))
    c.parameters.kind = TACTIX_ONLINE_PARAMETERS_INHERIT;
  else if (!encode(in.parameters, out.parameters))
    return false;
  else
    c.parameters = out.parameters.parameters;
  switch (in.granularity) {
  case InvocationGranularity::OneInstance: c.granularity = TACTIX_ONLINE_GRANULARITY_ONE; break;
  case InvocationGranularity::SelectedBatch: c.granularity = TACTIX_ONLINE_GRANULARITY_BATCH; break;
  case InvocationGranularity::CompleteCurrentPass:
    c.granularity = TACTIX_ONLINE_GRANULARITY_COMPLETE_PASS; break;
  case InvocationGranularity::CanonicalApplication:
    c.granularity = TACTIX_ONLINE_GRANULARITY_CANONICAL; break;
  default: return false;
  }
  out.instances = in.instances;
  c.instances = out.instances.data();
  c.instance_count = static_cast<int>(out.instances.size());
  switch (in.repeat.mode) {
  case RepeatMode::Once: c.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE; break;
  case RepeatMode::Count: c.repeat_mode = TACTIX_ONLINE_REPEAT_COUNT; break;
  case RepeatMode::UntilScopedFixpoint: c.repeat_mode = TACTIX_ONLINE_REPEAT_FIXPOINT; break;
  case RepeatMode::UntilSizeScaledFixpoint:
    c.repeat_mode = TACTIX_ONLINE_REPEAT_SIZE_SCALED_FIXPOINT; break;
  default: return false;
  }
  c.repeat_limit = in.repeat.limit;
  c.rule_budget_mode = in.rule_work_budget.mode == RuleWorkBudgetMode::Inherit
                           ? TACTIX_ONLINE_RULE_BUDGET_INHERIT
                       : in.rule_work_budget.mode == RuleWorkBudgetMode::Unlimited
                           ? TACTIX_ONLINE_RULE_BUDGET_UNLIMITED
                           : TACTIX_ONLINE_RULE_BUDGET_BOUNDED;
  c.rule_budget = in.rule_work_budget.amount;
  const auto setting_mode = [](SettingOverride value) {
    return value == SettingOverride::Inherit   ? TACTIX_ONLINE_SETTING_INHERIT
           : value == SettingOverride::Disable ? TACTIX_ONLINE_SETTING_DISABLE
                                               : TACTIX_ONLINE_SETTING_ENABLE;
  };
  c.touch_filter_mode = setting_mode(in.material.touch_filter);
  c.likely_kept_redundant_mode = setting_mode(in.material.likely_kept_redundant);
  switch (in.work_unit) {
  case WorkUnit::Applications: c.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS; break;
  case WorkUnit::BveTicks: c.work_unit = TACTIX_ONLINE_WORK_BVE_TICKS; break;
  case WorkUnit::Propagations: c.work_unit = TACTIX_ONLINE_WORK_PROPAGATIONS; break;
  default: return false; // SR discovery ticks have no C step unit
  }
  c.budget_unlimited = in.budget.is_unlimited();
  c.budget = in.budget.is_unlimited() ? 0 : in.budget.amount();
  c.post_application = in.post_application == PostApplication::Propagate
                           ? TACTIX_ONLINE_POST_PROPAGATE
                           : TACTIX_ONLINE_POST_NONE;
  // SR payloads keep internal literals; C names them by external identity.
  for (const auto &instance : in.substitution_redundancy_instances) {
    std::vector<int> clause, witness;
    for (Lit l : instance.clause) clause.push_back(kernel_externalize(k, l));
    for (Lit l : instance.witness) witness.push_back(kernel_externalize(k, l));
    std::vector<tactix_online_sr_mapping> mapping;
    for (const auto &[from, to] : instance.substitution)
      mapping.push_back({kernel_externalize(k, from), kernel_externalize(k, to)});
    out.sr_literals.push_back(std::move(clause));
    out.sr_literals.push_back(std::move(witness));
    out.sr_mappings.push_back(std::move(mapping));
  }
  for (size_t i = 0; i < in.substitution_redundancy_instances.size(); ++i) {
    const auto &clause = out.sr_literals[2 * i], &witness = out.sr_literals[2 * i + 1];
    const auto &mapping = out.sr_mappings[i];
    out.sr_instances.push_back({clause.data(), static_cast<int>(clause.size()),
                                witness.data(), static_cast<int>(witness.size()),
                                mapping.data(), static_cast<int>(mapping.size())});
  }
  out.step.sr_instances = out.sr_instances.data();
  out.step.sr_instance_count = static_cast<int>(out.sr_instances.size());
  return true;
}

} // namespace public_encoding
