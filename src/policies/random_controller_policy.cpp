#include "policies/random_controller_policy.hpp"

#include "policies/stress_common.hpp"
#include "rules/decide/decide.hpp"
#include "rules/lucky/lucky.hpp"

#include <cstdio>
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace tactix {
using namespace strategy;
using namespace strategy::controller;

bool RandomControllerConfig::configure_actions(const OnlineStrategyProgram &program,
                                               std::string *error) {
  const auto reject = [&](const char *message) {
    if (error) *error = message;
    return false;
  };
  if (!validate_online_strategy_program(program, error))
    return false;
  if (program.sequence.empty())
    return reject("controller action collection is empty");
  auto replacement = *this;
  replacement.offered_actions.clear();
  replacement.allowances.clear();
  replacement.parameters.rules.clear();
  for (const auto &step : program.sequence) {
    const int id = tactix_rule_id(step.rule.c_str());
    if (id < 0 || !root_exclusion(tactix_rules()[id]).empty())
      return reject("controller collection contains an unknown or excluded root action");
    if (step.budget.is_unlimited())
      return reject("controller action collection requires fixed composition allowances");
    ActionAllowance allowance{step.work_unit, step.budget.amount(), step.rule_work_budget};
    if (const auto found = replacement.allowances.find(step.rule);
        found != replacement.allowances.end()) {
      const auto previous = found->second;
      if (previous.unit != allowance.unit || previous.amount != allowance.amount ||
          previous.native.mode != allowance.native.mode ||
          previous.native.amount != allowance.native.amount)
        return reject("one controller family has inconsistent effort allowances");
    } else {
      replacement.parameters.rules.push_back(step.rule);
      replacement.allowances[step.rule] = allowance;
    }
    replacement.offered_actions[step.rule].push_back(step);
  }
  *this = std::move(replacement);
  if (error) error->clear();
  return true;
}

bool RandomControllerConfig::configure_startup_prefix(
    const OnlineStrategyProgram &program, std::string *error) {
  const auto reject = [error](const char *message) {
    if (error) *error = message;
    return false;
  };
  if (!startup_prefix.sequence.empty())
    return reject("controller startup prefix is already installed");
  if (!validate_online_strategy_program(program, error, true)) return false;
  if (program.repeat.mode != RepeatMode::Once || program.repeat.limit != 1)
    return reject("controller startup prefix must be once-only");

  auto replacement = *this;
  for (const auto &step : program.sequence) {
    const int id = tactix_rule_id(step.rule.c_str());
    if (id < 0 || !root_exclusion(tactix_rules()[id]).empty())
      return reject("controller startup prefix contains an unknown or excluded root action");
    if (!replacement.parameters.rules.empty() &&
        std::find(replacement.parameters.rules.begin(),
                  replacement.parameters.rules.end(), step.rule) ==
            replacement.parameters.rules.end())
      return reject("controller startup prefix action is outside the configured collection");
    if (step.repeat.mode != RepeatMode::Once || step.repeat.limit != 1 ||
        step.post_application != PostApplication::None)
      return reject("controller startup prefix steps must be once-only with no hidden follow-up");
    if (step.budget.is_unlimited() || !step.budget.amount() ||
        step.rule_work_budget.mode != RuleWorkBudgetMode::Bounded ||
        !step.rule_work_budget.amount)
      return reject("controller startup prefix requires fixed positive allowances");

    const ActionAllowance allowance{step.work_unit, step.budget.amount(),
                                     step.rule_work_budget};
    if (const auto found = replacement.allowances.find(step.rule);
        found != replacement.allowances.end()) {
      const auto previous = found->second;
      if (previous.unit != allowance.unit ||
          previous.amount != allowance.amount ||
          previous.native.mode != allowance.native.mode ||
          previous.native.amount != allowance.native.amount)
        return reject("controller startup prefix allowance conflicts with its action collection");
    } else {
      replacement.allowances[step.rule] = allowance;
    }

    if (step.granularity != InvocationGranularity::NativeProducerOffer)
      continue;
    if (const auto offered = replacement.offered_actions.find(step.rule);
        offered != replacement.offered_actions.end() && !offered->second.empty())
      return reject("controller startup native offer conflicts with a static action offer");
    if (step.rule == "AffineConsequence") {
      const auto *parameters =
          std::get_if<AffineConsequenceParameters>(&step.parameters);
      if (replacement.affine_basis_profile_configured)
        return reject("controller startup affine offer conflicts with the GF2 affine producer profile");
      if (!replacement.affine_basis_action_producer.enabled || !parameters)
        return reject("controller startup affine offer requires its native producer and typed profile");
      replacement.parameters.affine_consequence_reference = *parameters;
    } else if (step.rule == "Gf3AffineConsequence") {
      const auto *parameters =
          std::get_if<Gf3AffineConsequenceParameters>(&step.parameters);
      if (!replacement.gf3_action_producer.enabled || !parameters)
        return reject("controller startup GF3 offer requires its native producer and typed profile");
      replacement.parameters.gf3_affine_consequence_reference = *parameters;
    } else if (step.rule == "DirectCspSupport") {
      const auto *parameters =
          std::get_if<DirectCspSupportParameters>(&step.parameters);
      if (!replacement.direct_csp_action_producer.enabled || !parameters)
        return reject("controller startup direct-CSP offer requires its native producer and typed profile");
      replacement.parameters.direct_csp_support_reference = *parameters;
    } else if (step.rule == "NaryCspPrefix") {
      const auto *parameters =
          std::get_if<NaryCspPrefixParameters>(&step.parameters);
      if (!replacement.nary_csp_action_producer.enabled || !parameters)
        return reject("controller startup n-ary offer requires its native producer and typed profile");
      replacement.parameters.nary_csp_prefix_reference = *parameters;
    } else if (step.rule == "NaryCspEquality") {
      const auto *parameters =
          std::get_if<NaryCspEqualityParameters>(&step.parameters);
      if (!replacement.nary_csp_equality_action_producer.enabled || !parameters)
        return reject("controller startup n-ary equality offer requires its native producer and typed profile");
      replacement.parameters.nary_csp_equality_reference = *parameters;
    } else if (step.rule == "NaryCspEqualityAmo") {
      const auto *parameters =
          std::get_if<NaryCspEqualityAmoParameters>(&step.parameters);
      if (!replacement.nary_csp_equality_amo_producer.enabled || !parameters)
        return reject("controller startup n-ary equality AMO offer requires its native producer and typed profile");
      replacement.parameters.nary_csp_equality_amo_reference = *parameters;
    } else if (step.rule == "Gf3GaussianConsequence") {
      const auto *parameters =
          std::get_if<Gf3GaussianConsequenceParameters>(&step.parameters);
      if (!replacement.gf3_gaussian_action_producer.enabled || !parameters)
        return reject("controller startup GF3 Gaussian offer requires its native producer and typed profile");
      replacement.parameters.gf3_gaussian_consequence_reference = *parameters;
    } else if (step.rule == "Gf3GeneralRowConsequence") {
      const auto *parameters =
          std::get_if<Gf3GeneralRowConsequenceParameters>(&step.parameters);
      if (!replacement.gf3_general_row_action_producer.enabled || !parameters)
        return reject("controller startup GF3 general-row offer requires its native producer and typed profile");
      replacement.parameters.gf3_general_row_consequence_reference =
          *parameters;
    } else if (step.rule == "TseitinCycleBreaking") {
      const auto *parameters =
          std::get_if<TseitinCycleBreakingParameters>(&step.parameters);
      if (!replacement.tseitin_cycle_action_producer.enabled || !parameters)
        return reject("controller startup Tseitin offer requires its native producer and typed profile");
      replacement.parameters.tseitin_cycle_breaking_reference = *parameters;
    } else {
      return reject("controller startup action has no native producer");
    }
  }
  replacement.startup_prefix = program;
  *this = std::move(replacement);
  if (error) error->clear();
  return true;
}

bool RandomControllerConfig::configure_affine_basis_producer_profile(
    const OnlineStrategyProgram &program, std::string *error) {
  const auto reject = [&](const char *message) {
    if (error) *error = message;
    return false;
  };
  if (std::any_of(startup_prefix.sequence.begin(),
                  startup_prefix.sequence.end(), [](const auto &step) {
                    return step.rule == "AffineConsequence" &&
                           step.granularity ==
                               InvocationGranularity::NativeProducerOffer;
                  }))
    return reject("GF2 affine producer profile conflicts with the controller startup affine offer");
  if (!validate_online_strategy_program(program, error))
    return false;
  if (program.sequence.empty() || program.sequence.size() > 2)
    return reject("GF2 affine producer profile requires one affine action and "
                  "at most one advice action");
  if ((program.repeat.mode != RepeatMode::Once &&
       program.repeat.mode != RepeatMode::Count) ||
      !program.repeat.limit ||
      (program.repeat.mode == RepeatMode::Once && program.repeat.limit != 1))
    return reject("GF2 affine producer profile requires once-only or bounded-count sequence repetition");
  const auto &step = program.sequence.front();
  const auto *profile_parameters =
      std::get_if<AffineConsequenceParameters>(&step.parameters);
  if (step.rule != "AffineConsequence" || !profile_parameters ||
      profile_parameters->semantics !=
          AffineConsequenceSemantics::CertifiedBasisV2)
    return reject("GF2 affine producer profile requires certified-basis parameters");
  if (step.granularity != InvocationGranularity::CanonicalApplication ||
      !step.instances.empty() || !step.factor_grids.empty() ||
      !step.substitution_redundancy_instances.empty() ||
      !step.condition_instances.empty() ||
      !step.independent_set_cover_instances.empty() ||
      !step.gf3_affine_instances.empty() ||
      !step.affine_basis_instances.empty() ||
      !step.gf3_gaussian_instances.empty() ||
      !step.tseitin_cycle_instances.empty() ||
      !step.direct_csp_guarded_cover_instances.empty() ||
      !step.direct_csp_support_instances.empty() ||
      !step.nary_csp_prefix_instances.empty() ||
      !step.nary_csp_equality_instances.empty() ||
      !step.nary_csp_equality_transitivity_instances.empty() ||
      !step.nary_csp_equality_amo_instances.empty())
    return reject("GF2 affine producer profile must be canonical and payload-free");
  if (step.repeat.mode != RepeatMode::Once || step.repeat.limit != 1 ||
      step.post_application != PostApplication::None)
    return reject("GF2 affine producer profile must be once-only with no follow-up");
  if (step.material.touch_filter != SettingOverride::Inherit ||
      step.material.likely_kept_redundant != SettingOverride::Inherit)
    return reject("GF2 affine producer profile requires inherited material settings");
  if (step.budget.is_unlimited() || !step.budget.amount())
    return reject("GF2 affine producer profile requires a positive fixed allowance");
  if (step.rule_work_budget.mode != RuleWorkBudgetMode::Bounded ||
      !step.rule_work_budget.amount)
    return reject("GF2 affine producer profile requires a positive fixed native allowance");
  if (!affine_basis_action_producer.enabled)
    return reject("GF2 affine producer profile requires the native producer");
  if (!parameters.rules.empty() &&
      std::find(parameters.rules.begin(), parameters.rules.end(),
                "AffineConsequence") == parameters.rules.end())
    return reject("GF2 affine producer profile is outside the configured action collection");
  if (const auto offered = offered_actions.find("AffineConsequence");
      offered != offered_actions.end() && !offered->second.empty())
    return reject("GF2 affine producer profile conflicts with a static action offer");

  const SemanticVariableAdviceParameters *advice_parameters = nullptr;
  const OnlineInvocation *advice = nullptr;
  if (program.sequence.size() == 2) {
    advice = &program.sequence[1];
    advice_parameters =
        std::get_if<SemanticVariableAdviceParameters>(&advice->parameters);
    if (advice->rule != "SemanticVariableAdvice" || !advice_parameters)
      return reject("GF2 affine producer profile companion must be semantic "
                    "variable advice");
    if (advice->granularity != InvocationGranularity::CanonicalApplication ||
        !advice->instances.empty() || !advice->factor_grids.empty() ||
        !advice->substitution_redundancy_instances.empty() ||
        !advice->condition_instances.empty() ||
        !advice->independent_set_cover_instances.empty() ||
        !advice->gf3_affine_instances.empty() ||
        !advice->affine_basis_instances.empty() ||
        !advice->gf3_gaussian_instances.empty() ||
        !advice->tseitin_cycle_instances.empty() ||
        !advice->direct_csp_guarded_cover_instances.empty() ||
        !advice->direct_csp_support_instances.empty() ||
        !advice->nary_csp_prefix_instances.empty() ||
        !advice->nary_csp_equality_instances.empty())
      return reject("GF2 semantic variable advice profile must be canonical "
                    "and payload-free");
    if (advice->repeat.mode != RepeatMode::Once || advice->repeat.limit != 1 ||
        advice->post_application != PostApplication::None)
      return reject("GF2 semantic variable advice profile must be once-only "
                    "with no follow-up");
    if (advice->material.touch_filter != SettingOverride::Inherit ||
        advice->material.likely_kept_redundant != SettingOverride::Inherit)
      return reject("GF2 semantic variable advice profile requires inherited "
                    "material settings");
    if (advice->budget.is_unlimited() || !advice->budget.amount())
      return reject("GF2 semantic variable advice profile requires a positive "
                    "fixed allowance");
    if (advice->rule_work_budget.mode != RuleWorkBudgetMode::Bounded ||
        !advice->rule_work_budget.amount)
      return reject("GF2 semantic variable advice profile requires a positive "
                    "fixed native allowance");
    if (!advice_parameters->maximum_variables ||
        !advice_parameters->maximum_priority_sum ||
        !advice_parameters->existing_theory_priority ||
        !advice_parameters->introduced_theory_priority)
      return reject("GF2 semantic variable advice profile requires positive "
                    "limits and both theory-role priorities");
    if (!parameters.rules.empty() &&
        std::find(parameters.rules.begin(), parameters.rules.end(),
                  "SemanticVariableAdvice") == parameters.rules.end())
      return reject("GF2 semantic variable advice profile is outside the "
                    "configured action collection");
    if (const auto offered = offered_actions.find("SemanticVariableAdvice");
        offered != offered_actions.end() && !offered->second.empty())
      return reject("GF2 semantic variable advice profile conflicts with a "
                    "static action offer");
  }

  if (program.repeat.mode == RepeatMode::Count) {
    if (!advice)
      return reject("GF2 affine startup batching requires explicit semantic variable advice");
    if (!parameters.rules.empty() &&
        std::find(parameters.rules.begin(), parameters.rules.end(),
                  "Propagate") == parameters.rules.end())
      return reject("GF2 affine startup batching requires Propagate in the configured action collection");
  }

  auto replacement = *this;
  replacement.affine_basis_profile_configured = true;
  replacement.affine_basis_action_producer.startup_batch_actions =
      program.repeat.mode == RepeatMode::Count ? program.repeat.limit : 0;
  replacement.parameters.affine_consequence_reference = *profile_parameters;
  replacement.allowances["AffineConsequence"] =
      {step.work_unit, step.budget.amount(), step.rule_work_budget};
  if (advice) {
    replacement.parameters.semantic_variable_advice_reference =
        *advice_parameters;
    replacement.allowances["SemanticVariableAdvice"] =
        {advice->work_unit, advice->budget.amount(), advice->rule_work_budget};
  }
  *this = std::move(replacement);
  if (error) error->clear();
  return true;
}

bool RandomControllerConfig::configure_guarded_cover_producer_profile(
    const OnlineStrategyProgram &program,
    const DirectCspGuardedCoverOfferLimits &limits, std::string *error) {
  const auto reject = [&](const char *message) {
    if (error) *error = message;
    return false;
  };
  if (!limits.valid() || program.sequence.size() != 1 ||
      program.repeat.mode != RepeatMode::Once || program.repeat.limit != 1)
    return reject("GuardedCover producer requires one bounded once-only profile");
  if (!validate_online_strategy_program(program, error, true)) return false;
  const auto &step = program.sequence.front();
  const auto *p = std::get_if<DirectCspGuardedCoverParameters>(&step.parameters);
  if (step.rule != "DirectCspSupport" || !p || !p->valid() ||
      step.granularity != InvocationGranularity::NativeProducerOffer ||
      step.repeat.mode != RepeatMode::Once || step.repeat.limit != 1 ||
      step.post_application != PostApplication::None ||
      step.budget.is_unlimited() || !step.budget.amount() ||
      step.rule_work_budget.mode != RuleWorkBudgetMode::Bounded ||
      !step.rule_work_budget.amount ||
      step.material.touch_filter != SettingOverride::Inherit ||
      step.material.likely_kept_redundant != SettingOverride::Inherit)
    return reject("GuardedCover producer requires a payload-free V2 native offer with fixed allowances");
  if (guarded_cover_producer.window.after_cover || guarded_cover_producer.window.stamp.kernel)
    return reject("GuardedCover producer starts with an unbound cover window");
  if (guarded_cover_producer.window.preferred_literals.size() > limits.maximum_work ||
      std::any_of(guarded_cover_producer.window.preferred_literals.begin(),
                  guarded_cover_producer.window.preferred_literals.end(),
                  [](int32_t x) { return !x || x == INT32_MIN; }))
    return reject("GuardedCover producer preference snapshot is invalid or exceeds its authored work envelope");
  const auto &b = limits.theory;
  if (b.maximum_source_clauses != p->maximum_source_clauses ||
      b.maximum_source_literals != p->maximum_source_literals ||
      b.maximum_domains != p->maximum_domains ||
      b.maximum_domain_values != p->maximum_domain_values ||
      b.maximum_cover_literals != p->maximum_cover_literals ||
      b.maximum_or_definitions != p->maximum_or_definitions ||
      b.maximum_or_inputs != p->maximum_or_inputs ||
      b.maximum_output_literals != p->maximum_output_literals)
    return reject("GuardedCover producer theory must match the authored public profile");
  const auto offers = offered_actions.find(step.rule);
  if ((offers != offered_actions.end() && !offers->second.empty()) ||
      direct_csp_action_producer.enabled)
    return reject("GuardedCover producer conflicts with another DirectCspSupport owner");
  if (!parameters.rules.empty() &&
      std::find(parameters.rules.begin(), parameters.rules.end(), step.rule) == parameters.rules.end())
    return reject("GuardedCover producer is outside the configured action collection");
  if (guarded_cover_producer.revision == UINT64_MAX)
    return reject("GuardedCover producer revision exhausted");
  auto replacement = guarded_cover_producer;
  replacement.enabled = true;
  replacement.limits = limits;
  replacement.profile = step;
  ++replacement.revision;
  guarded_cover_producer = std::move(replacement);
  allowances[step.rule] = {step.work_unit, step.budget.amount(), step.rule_work_budget};
  if (error) error->clear();
  return true;
}

bool RandomControllerConfig::configure_direct_csp_producer_profile(
    const OnlineStrategyProgram &program, std::string *error) {
  const auto reject = [&](const char *message) {
    if (error) *error = message;
    return false;
  };
  if (guarded_cover_producer.enabled)
    return reject("DirectCSP producer conflicts with the GuardedCover producer owner");
  if (program.sequence.size() != 3)
    return reject("DirectCSP producer profile requires support, propagation, "
                  "and semantic advice actions");
  // The shared runtime validator correctly requires a concrete typed payload
  // for an executable DirectCspSupport invocation.  This input is a producer
  // template: it must remain canonical and payload-free until discovery binds
  // an exact row. Validate all common fields using a private representative
  // payload, then apply the stricter template checks below to the untouched
  // public program.
  OnlineStrategyProgram executable_shape = program;
  auto &shape_step = executable_shape.sequence.front();
  if (shape_step.rule == "DirectCspSupport" &&
      shape_step.direct_csp_support_instances.empty()) {
    shape_step.granularity = InvocationGranularity::OneInstance;
    shape_step.direct_csp_support_instances = {{{1, 2}, {3, 4}, 0}};
  }
  if (!validate_online_strategy_program(executable_shape, error)) return false;
  if ((program.repeat.mode != RepeatMode::Once &&
       program.repeat.mode != RepeatMode::Count) ||
      !program.repeat.limit ||
      (program.repeat.mode == RepeatMode::Once && program.repeat.limit != 1))
    return reject("DirectCSP producer profile requires once-only or "
                  "bounded-count sequence repetition");

  const auto payload_free = [](const OnlineInvocation &step) {
    return step.instances.empty() && step.factor_grids.empty() &&
           step.substitution_redundancy_instances.empty() &&
           step.condition_instances.empty() &&
           step.independent_set_cover_instances.empty() &&
           step.gf3_affine_instances.empty() &&
           step.affine_basis_instances.empty() &&
           step.gf3_gaussian_instances.empty() &&
           step.tseitin_cycle_instances.empty() &&
           step.direct_csp_guarded_cover_instances.empty() &&
           step.direct_csp_support_instances.empty() &&
           step.nary_csp_prefix_instances.empty();
  };
  const auto fixed_positive = [](const OnlineInvocation &step) {
    return !step.budget.is_unlimited() && step.budget.amount() &&
           step.rule_work_budget.mode == RuleWorkBudgetMode::Bounded &&
           step.rule_work_budget.amount;
  };

  const OnlineInvocation &step = program.sequence.front();
  const auto *profile_parameters =
      std::get_if<DirectCspSupportParameters>(&step.parameters);
  if (step.rule != "DirectCspSupport" || !profile_parameters ||
      !profile_parameters->valid())
    return reject("DirectCSP producer profile requires valid typed support "
                  "parameters");
  if (step.granularity != InvocationGranularity::CanonicalApplication ||
      !payload_free(step))
    return reject("DirectCSP producer profile must be canonical and "
                  "payload-free");
  if (step.repeat.mode != RepeatMode::Once || step.repeat.limit != 1 ||
      step.post_application != PostApplication::None)
    return reject("DirectCSP producer profile requires once-only application "
                  "with no bundled follow-up");
  if (step.material.touch_filter != SettingOverride::Inherit ||
      step.material.likely_kept_redundant != SettingOverride::Inherit)
    return reject("DirectCSP producer profile requires inherited material "
                  "settings");
  if (!fixed_positive(step))
    return reject("DirectCSP producer profile requires positive fixed "
                  "composition and native allowances");
  if (!direct_csp_action_producer.enabled)
    return reject("DirectCSP producer profile requires the native producer");
  if (!parameters.rules.empty() &&
      std::find(parameters.rules.begin(), parameters.rules.end(),
                "DirectCspSupport") == parameters.rules.end())
    return reject("DirectCSP producer profile is outside the configured "
                  "action collection");
  if (const auto offered = offered_actions.find("DirectCspSupport");
      offered != offered_actions.end() && !offered->second.empty())
    return reject("DirectCSP producer profile conflicts with a static action "
                  "offer");

  const OnlineInvocation &propagation = program.sequence[1];
  if (propagation.rule != "Propagate" ||
      !std::holds_alternative<std::monostate>(propagation.parameters))
    return reject("DirectCSP producer profile second action must be Propagate");
  if (propagation.granularity !=
          InvocationGranularity::CanonicalApplication ||
      !payload_free(propagation) ||
      propagation.repeat.mode != RepeatMode::Once ||
      propagation.repeat.limit != 1 ||
      propagation.post_application != PostApplication::Propagate)
    return reject("DirectCSP propagation must be canonical, payload-free, "
                  "once-only, and explicitly fixpoint-settling");
  if (propagation.material.touch_filter != SettingOverride::Inherit ||
      propagation.material.likely_kept_redundant != SettingOverride::Inherit)
    return reject("DirectCSP propagation requires inherited material settings");
  if (!fixed_positive(propagation))
    return reject("DirectCSP propagation requires positive fixed composition "
                  "and native allowances");
  if (!parameters.rules.empty() &&
      std::find(parameters.rules.begin(), parameters.rules.end(),
                "Propagate") == parameters.rules.end())
    return reject("DirectCSP propagation is outside the configured action "
                  "collection");
  if (const auto offered = offered_actions.find("Propagate");
      offered != offered_actions.end() && !offered->second.empty())
    return reject("DirectCSP propagation conflicts with a static action offer");

  const OnlineInvocation *advice = &program.sequence[2];
  const auto *advice_parameters =
      std::get_if<SemanticVariableAdviceParameters>(&advice->parameters);
  {
    if (advice->rule != "SemanticVariableAdvice" || !advice_parameters)
      return reject("DirectCSP producer profile companion must be semantic "
                    "variable advice");
    if (advice->granularity != InvocationGranularity::CanonicalApplication ||
        !payload_free(*advice) ||
        advice->repeat.mode != RepeatMode::Once ||
        advice->repeat.limit != 1 ||
        advice->post_application != PostApplication::None)
      return reject("DirectCSP semantic variable advice must be canonical, "
                    "payload-free and once-only");
    if (advice->material.touch_filter != SettingOverride::Inherit ||
        advice->material.likely_kept_redundant != SettingOverride::Inherit)
      return reject("DirectCSP semantic variable advice requires inherited "
                    "material settings");
    if (!fixed_positive(*advice) || !advice_parameters->maximum_variables ||
        !advice_parameters->maximum_priority_sum ||
        !advice_parameters->existing_theory_priority ||
        !advice_parameters->introduced_theory_priority)
      return reject("DirectCSP semantic variable advice requires positive "
                    "limits and fixed allowances");
    if (!parameters.rules.empty() &&
        std::find(parameters.rules.begin(), parameters.rules.end(),
                  "SemanticVariableAdvice") == parameters.rules.end())
      return reject("DirectCSP semantic variable advice is outside the "
                    "configured action collection");
    if (const auto offered = offered_actions.find("SemanticVariableAdvice");
        offered != offered_actions.end() && !offered->second.empty())
      return reject("DirectCSP semantic variable advice conflicts with a "
                    "static action offer");
  }

  auto replacement = *this;
  replacement.direct_csp_action_producer.startup_batch_actions =
      program.repeat.limit;
  replacement.parameters.direct_csp_support_reference = *profile_parameters;
  replacement.allowances["DirectCspSupport"] =
      {step.work_unit, step.budget.amount(), step.rule_work_budget};
  replacement.allowances["Propagate"] =
      {propagation.work_unit, propagation.budget.amount(),
       propagation.rule_work_budget};
  replacement.parameters.semantic_variable_advice_reference =
      *advice_parameters;
  replacement.allowances["SemanticVariableAdvice"] =
      {advice->work_unit, advice->budget.amount(), advice->rule_work_budget};
  *this = std::move(replacement);
  if (error) error->clear();
  return true;
}

bool RandomControllerConfig::configure_search_configurations(
    const std::vector<SearchConfiguration> &configurations,
    std::string *error) {
  const auto reject = [&](const char *message) {
    if (error) *error = message;
    return false;
  };
  if (configurations.empty())
    return reject("controller search configuration collection is empty");
  for (size_t i = 0; i < configurations.size(); ++i) {
    if (!valid(configurations[i]))
      return reject("controller search configuration is invalid");
    if (std::find(configurations.begin(), configurations.begin() + i,
                  configurations[i]) != configurations.begin() + i)
      return reject("controller search configuration is repeated");
  }
  search_configurations = configurations;
  if (error) error->clear();
  return true;
}

std::vector<ControllerActionAvailability>
controller_action_availability(const Kernel &k, const RandomControllerConfig &config) {
  std::vector<ControllerActionAvailability> result;
  for (const auto &rule : tactix_rules()) {
    ControllerActionAvailability entry{rule.name, false, {}};
    const auto selected_guard = [&] {
      if (rule.guard(k)) return true;
      if (std::string_view(rule.name) != "DirectCspSupport") return false;
      if (config.guarded_cover_producer.enabled &&
          k.direct_csp_guarded_cover_action_session &&
          direct_csp_guarded_cover_action_guard(k)) return true;
      const auto offers = config.offered_actions.find(rule.name);
      return offers != config.offered_actions.end() &&
          k.direct_csp_guarded_cover_action_session &&
          direct_csp_guarded_cover_action_guard(k) &&
          std::any_of(offers->second.begin(), offers->second.end(),
              [](const OnlineInvocation &s) {
                return std::holds_alternative<DirectCspGuardedCoverParameters>(s.parameters) &&
                       !s.direct_csp_guarded_cover_instances.empty();
              });
    };
    if (const auto excluded = root_exclusion(rule); !excluded.empty())
      entry.reason = excluded;
    else if (!config.parameters.rules.empty() &&
             std::find(config.parameters.rules.begin(), config.parameters.rules.end(),
                       rule.name) == config.parameters.rules.end())
      entry.reason = "outside-configured-action-collection";
    else if (k.level || k.inconsistent || k.conflict_valid)
      entry.reason = "not-a-consistent-root-boundary";
    else if (std::string_view(rule.name) == "AffineMatrixInitialize" &&
             k.affine_matrix_search_session &&
             k.affine_matrix_search_session->attached_to(k))
      entry.reason = "affine-matrix-session-active";
    else if (!selected_guard())
      entry.reason = "guard-false";
    else if (std::string_view(rule.name) == "AffineMatrixInitialize" &&
             k.affine_matrix_search_session &&
             k.affine_matrix_search_session->initialization_retry_suppressed(
                 k, config.parameters.affine_consequence_reference))
      entry.reason = "unchanged-terminal-initialization";
    else if ((std::string_view(rule.name) == "Condition" ||
              std::string_view(rule.name) == "IndependentSetCover" ||
              std::string_view(rule.name) == "AffineConsequence" ||
              std::string_view(rule.name) == "Gf3AffineConsequence" ||
              std::string_view(rule.name) == "DirectCspSupport" ||
              std::string_view(rule.name) == "NaryCspPrefix" ||
              std::string_view(rule.name) == "NaryCspEquality" ||
              std::string_view(rule.name) == "NaryCspEqualityTransitivity" ||
              std::string_view(rule.name) == "NaryCspEqualityAmo" ||
              std::string_view(rule.name) == "Gf3GaussianConsequence" ||
              std::string_view(rule.name) == "Gf3GeneralRowConsequence" ||
              std::string_view(rule.name) == "TseitinCycleBreaking") &&
             (!config.offered_actions.contains(rule.name) ||
              config.offered_actions.at(rule.name).empty()))
      entry.reason = "structured-witness-not-offered";
    else if (std::string_view(rule.name) == "LocalSearchGuidance" &&
             std::none_of(k.local_search_hints.begin(),
                          k.local_search_hints.end(),
                          [](const auto &hint) { return hint.available(); }))
      entry.reason = "candidate-not-observed";
    else
      entry.eligible = true;
    result.push_back(std::move(entry));
  }
  return result;
}

static ActionParameters reference_parameters(const Kernel &k,
                                            const std::string &rule,
                                            const RandomOnlineConfig &profile) {
  if (rule == "BVE")
    return BveParameters::from_kernel(k);
#define CONTROLLER_REFERENCE(identity, field) \
  if (rule == identity) return profile.field
  CONTROLLER_REFERENCE("SubsumeForward", subsume_reference);
  CONTROLLER_REFERENCE("Strengthen", strengthen_reference);
  CONTROLLER_REFERENCE("Congruence", congruence_reference);
  CONTROLLER_REFERENCE("ParityConsequence", parity_consequence_reference);
  CONTROLLER_REFERENCE("AffineConsequence", affine_consequence_reference);
  CONTROLLER_REFERENCE("AffineMatrixInitialize", affine_consequence_reference);
  CONTROLLER_REFERENCE("ExactCoverExtension", exact_cover_extension_reference);
  CONTROLLER_REFERENCE("IndependentSetCover", independent_set_cover_reference);
  CONTROLLER_REFERENCE("Gf3AffineConsequence", gf3_affine_consequence_reference);
  CONTROLLER_REFERENCE("DirectCspSupport", direct_csp_support_reference);
  CONTROLLER_REFERENCE("NaryCspPrefix", nary_csp_prefix_reference);
  CONTROLLER_REFERENCE("NaryCspEquality", nary_csp_equality_reference);
  CONTROLLER_REFERENCE("NaryCspEqualityAmo", nary_csp_equality_amo_reference);
  CONTROLLER_REFERENCE("DirectPhpRefutation", direct_php_refutation_reference);
  CONTROLLER_REFERENCE("Gf3GaussianConsequence", gf3_gaussian_consequence_reference);
  CONTROLLER_REFERENCE("Gf3GeneralRowConsequence", gf3_general_row_consequence_reference);
  CONTROLLER_REFERENCE("TseitinCycleBreaking", tseitin_cycle_breaking_reference);
  CONTROLLER_REFERENCE("SemanticVariableAdvice", semantic_variable_advice_reference);
  CONTROLLER_REFERENCE("Instantiate", instantiate_reference);
  CONTROLLER_REFERENCE("Backbone", backbone_reference);
  CONTROLLER_REFERENCE("Sweep", sweep_reference);
  CONTROLLER_REFERENCE("Vivify", vivify_reference);
  CONTROLLER_REFERENCE("BCE", bce_reference);
  CONTROLLER_REFERENCE("HyperTernaryRes", ternary_reference);
  CONTROLLER_REFERENCE("Factor", factor_reference);
  CONTROLLER_REFERENCE("TransitiveReduce", transitive_reference);
  CONTROLLER_REFERENCE("Substitute", substitute_reference);
  CONTROLLER_REFERENCE("Reduce", reduce_reference);
  CONTROLLER_REFERENCE("Walk", walk_reference);
  CONTROLLER_REFERENCE("Reorder", reorder_reference);
#undef CONTROLLER_REFERENCE
  if (rule == "SubstitutionRedundancy") {
    auto sr = profile.sr_reference;
    if (sr.producer == SubstitutionRedundancyProducer::ExplicitOnly)
      sr.producer = SubstitutionRedundancyProducer::Orbitopal;
    sr.maximum_validation_ticks = profile.maximum_sr_validation_ticks;
    sr.maximum_detection_ticks = profile.maximum_sr_detection_ticks;
    sr.maximum_orbitopal_matrices = profile.maximum_sr_matrices;
    sr.maximum_orbitopal_instances = profile.maximum_sr_instances;
    sr.maximum_general_candidates_per_advance =
        profile.maximum_sr_general_candidates_per_advance;
    return sr;
  }
  // Monostate inherits the kernel's fixed reference settings, rather than
  // sampling effort or semantic limits on each request.
  return {};
}

std::string controller_configuration(const RandomControllerConfig &config) {
  std::ostringstream out;
  out << std::setprecision(17);
  const auto &r = config.search.restart;
  out << "search " << r.initial_interval << ' ' << r.glue_margin << ' '
      << r.fast_ema_window << ' ' << r.slow_ema_window << ' '
      << r.logarithm_offset << ' ' << r.logarithm_bias << ' '
      << r.reluctant_period << ' ' << r.reluctant_limit << ' '
      << config.search.branching.focused_phase_switch_span;
  for (const auto phase : config.search.branching.focused_phase_pattern)
    out << ',' << static_cast<int>(phase);
  out << " trail-retention "
      << (config.reuse_search_trail ? "ranked-prefix" : "full")
      << " window " << config.parameters.maximum_selected_batch;
  for (const auto search : config.search_configurations)
    out << " search-configuration " << std::quoted(name(search));
  for (const auto &rule : config.parameters.rules)
    out << " family " << std::quoted(rule);
  if (!config.startup_prefix.sequence.empty())
    out << " startup-prefix "
        << std::quoted(serialize_online_strategy_program(
               config.startup_prefix));
  Kernel reference;
  OnlineStrategyProgram profiles;
  for (const auto &rule : tactix_rules()) {
    if (!root_exclusion(rule).empty())
      continue;
    if (std::string_view(rule.name) == "Gf3AffineConsequence" ||
        std::string_view(rule.name) == "DirectCspSupport" ||
        std::string_view(rule.name) == "NaryCspPrefix" ||
        std::string_view(rule.name) == "NaryCspEquality")
      // These families have no canonical action.  Their complete typed
      // requests, including limits, are fingerprinted below as offered
      // actions; inventing an empty profile here would serialize a program
      // that the public validator correctly rejects.
      continue;
    OnlineInvocation invocation;
    invocation.id = profiles.sequence.size() + 1;
    invocation.rule = rule.name;
    invocation.parameters = reference_parameters(reference, rule.name, config.parameters);
    invocation.granularity = InvocationGranularity::CanonicalApplication;
    const auto allowance = config.allowance(rule.name);
    invocation.work_unit = allowance.unit;
    invocation.budget = WorkLimit::bounded(allowance.amount);
    invocation.rule_work_budget = allowance.native;
    profiles.sequence.push_back(std::move(invocation));
    if (std::string_view(rule.name) == "Lucky") {
      auto balanced = profiles.sequence.back();
      balanced.id = profiles.sequence.size() + 1;
      balanced.granularity = InvocationGranularity::OneInstance;
      balanced.instances = {10};
      balanced.parameters =
          config.parameters.balanced_signature_model_reference;
      profiles.sequence.push_back(std::move(balanced));
      auto fermat = profiles.sequence.back();
      fermat.id = profiles.sequence.size() + 1;
      fermat.instances = {11};
      fermat.parameters = config.parameters.fermat_model_reference;
      profiles.sequence.push_back(std::move(fermat));
      auto lookup = profiles.sequence.back();
      lookup.id = profiles.sequence.size() + 1;
      lookup.instances = {13};
      lookup.parameters = config.parameters.lookup_circuit_model_reference;
      profiles.sequence.push_back(std::move(lookup));
      auto simon = profiles.sequence.back();
      simon.id = profiles.sequence.size() + 1;
      simon.instances = {14};
      simon.parameters = config.parameters.simon_model_reference;
      profiles.sequence.push_back(std::move(simon));
      for (const auto &[instance, parameters] :
           std::initializer_list<std::pair<uint64_t, ActionParameters>>{
               {17, config.parameters.exact_cover_model_reference},
               {18, config.parameters.independent_set_model_reference},
               {LUCKY_GRAPH_LABELING,
                config.parameters.graph_labeling_reference}}) {
        auto constructor = profiles.sequence.back();
        constructor.id = profiles.sequence.size() + 1;
        constructor.instances = {instance};
        constructor.parameters = parameters;
        profiles.sequence.push_back(std::move(constructor));
      }
      auto ramsey = profiles.sequence.back();
      ramsey.id = profiles.sequence.size() + 1;
      ramsey.instances = {LUCKY_STARTUP_RAMSEY};
      ramsey.parameters = config.parameters.ramsey_startup_reference;
      profiles.sequence.push_back(std::move(ramsey));
    }
  }
  out << " profiles " << std::quoted(serialize_online_strategy_program(profiles));
  for (const auto &[family, choices] : config.offered_actions) {
    OnlineStrategyProgram offered;
    offered.sequence = choices;
    out << " offered " << std::quoted(family) << ' '
        << std::quoted(serialize_online_strategy_program(offered));
  }
  // Disabled producers are deliberately absent so introducing the opt-in
  // mechanism cannot alter an existing controller fingerprint.
  if (config.affine_basis_action_producer.enabled) {
    OnlineInvocation profile;
    profile.rule = "AffineConsequence";
    profile.parameters = config.parameters.affine_consequence_reference;
    profile.granularity = InvocationGranularity::CanonicalApplication;
    out << " producer " << std::quoted(profile.rule) << ' '
        << config.affine_basis_action_producer.maximum_offers << ' '
        << config.affine_basis_action_producer.startup_batch_actions << ' '
        << std::quoted(serialize_online_strategy_program({{profile}, {}}));
  }
  if (config.guarded_cover_producer.enabled) {
    const auto &p = config.guarded_cover_producer;
    const auto &l = p.limits;
    const auto &b = l.theory;
    const auto &q = l.proposals;
    out << " producer GuardedCoverV2 "
        << std::quoted(serialize_online_strategy_program({{p.profile}, {}}));
    for (uint64_t n : {b.maximum_source_clauses, b.maximum_source_literals,
          uint64_t(b.maximum_domains), uint64_t(b.maximum_domain_values),
          uint64_t(b.maximum_cover_literals), uint64_t(b.maximum_or_definitions),
          uint64_t(b.maximum_or_inputs), uint64_t(b.maximum_output_literals),
          q.maximum_work, q.maximum_proposals, q.maximum_single_moves,
          q.maximum_pair_moves, q.maximum_repair_steps, q.maximum_visited_keys,
          q.maximum_nominations, l.maximum_work, l.maximum_index_clauses,
          l.maximum_index_literals, l.maximum_index_bytes,
          l.maximum_cover_candidates, l.maximum_role_queries, l.maximum_owners,
          l.maximum_offers, l.maximum_binary_assignments, l.maximum_binary_edges})
      out << ' ' << n;
    out << " role " << p.window.role_blocker << ' ' << p.window.role_alternative
        << " preferences " << p.window.preferred_literals.size();
    for (int32_t x : p.window.preferred_literals) out << ' ' << x;
  }
  if (config.gf3_action_producer.enabled) {
    const auto &producer = config.gf3_action_producer;
    OnlineInvocation profile;
    profile.rule = "Gf3AffineConsequence";
    profile.parameters = config.parameters.gf3_affine_consequence_reference;
    profile.granularity = InvocationGranularity::OneInstance;
    profile.gf3_affine_instances = {{0, 1}};
    out << " producer " << std::quoted(profile.rule) << ' '
        << producer.maximum_pair_candidates << ' '
        << producer.maximum_offers << ' '
        << producer.maximum_enumeration_work << ' '
        << std::quoted(serialize_online_strategy_program({{profile}, {}}));
  }
  if (config.direct_csp_action_producer.enabled) {
    const auto &producer = config.direct_csp_action_producer;
    OnlineInvocation profile;
    profile.rule = "DirectCspSupport";
    profile.parameters = config.parameters.direct_csp_support_reference;
    profile.granularity = InvocationGranularity::OneInstance;
    profile.direct_csp_support_instances = {{{1, 2}, {3, 4}, 0}};
    out << " producer " << std::quoted(profile.rule) << ' '
        << producer.maximum_domain_candidates << ' '
        << producer.maximum_pair_candidates << ' '
        << producer.maximum_row_candidates << ' '
        << producer.maximum_offers << ' '
        << producer.maximum_enumeration_work << ' '
        << producer.startup_batch_actions << ' '
        << std::quoted(serialize_online_strategy_program({{profile}, {}}));
  }
  if (config.nary_csp_action_producer.enabled) {
    const auto &producer = config.nary_csp_action_producer;
    OnlineInvocation profile;
    profile.rule = "NaryCspPrefix";
    profile.parameters = config.parameters.nary_csp_prefix_reference;
    profile.granularity = InvocationGranularity::OneInstance;
    profile.nary_csp_prefix_instances = {
        {{{1, 2}, {3, 4}, {5, 6}}, {2, {{0, 0}, {1, 0}}}}};
    out << " producer " << std::quoted(profile.rule) << ' '
        << producer.relation_arity << ' '
        << producer.maximum_domain_candidates << ' '
        << producer.maximum_relation_candidates << ' '
        << producer.maximum_prefix_candidates << ' '
        << producer.maximum_offers << ' '
        << producer.maximum_enumeration_work << ' '
        << std::quoted(serialize_online_strategy_program({{profile}, {}}));
    if (producer.diagnostic_census_once) out << " census-once";
  }
  if (config.nary_csp_equality_action_producer.enabled) {
    const auto &producer = config.nary_csp_equality_action_producer;
    OnlineInvocation profile;
    profile.rule = "NaryCspEquality";
    profile.parameters = config.parameters.nary_csp_equality_reference;
    profile.granularity = InvocationGranularity::OneInstance;
    profile.nary_csp_equality_instances = {
        {{{1, 2}, {3, 4}, {5, 6}}, {0, 1, 2}}};
    out << " producer " << std::quoted(profile.rule) << ' '
        << producer.maximum_owner_candidates << ' '
        << producer.maximum_coordinate_candidates << ' '
        << producer.maximum_offers << ' '
        << producer.maximum_enumeration_work << ' '
        << std::quoted(serialize_online_strategy_program({{profile}, {}}));
  }
  if (config.nary_csp_equality_amo_producer.enabled) {
    const auto &producer = config.nary_csp_equality_amo_producer;
    OnlineInvocation profile;
    profile.rule = "NaryCspEqualityAmo";
    profile.parameters = config.parameters.nary_csp_equality_amo_reference;
    profile.granularity = InvocationGranularity::OneInstance;
    profile.nary_csp_equality_amo_instances = {
        {{{1, 2}, {3, 4}, {5, 6}}}};
    out << " producer " << std::quoted(profile.rule) << ' '
        << producer.maximum_owner_candidates << ' '
        << producer.maximum_offers << ' '
        << producer.maximum_enumeration_work << ' '
        << std::quoted(serialize_online_strategy_program({{profile}, {}}));
  }
  if (config.gf3_gaussian_action_producer.enabled) {
    OnlineInvocation profile;
    profile.rule = "Gf3GaussianConsequence";
    profile.parameters = config.parameters.gf3_gaussian_consequence_reference;
    profile.granularity = InvocationGranularity::OneInstance;
    profile.gf3_gaussian_instances = {"g3b1:00"};
    out << " producer " << std::quoted(profile.rule) << ' '
        << std::quoted(serialize_online_strategy_program({{profile}, {}}));
  }
  if (config.gf3_general_row_action_producer.enabled) {
    const auto &producer = config.gf3_general_row_action_producer;
    OnlineInvocation profile;
    profile.rule = "Gf3GeneralRowConsequence";
    profile.parameters = config.parameters.gf3_general_row_consequence_reference;
    profile.granularity = InvocationGranularity::OneInstance;
    profile.post_application = PostApplication::Propagate;
    profile.gf3_general_row_instances = {{0, 0, "g3r1:00"}};
    out << " producer " << std::quoted(profile.rule) << ' '
        << producer.maximum_candidates << ' ' << producer.maximum_offers << ' '
        << producer.maximum_enumeration_work << ' '
        << std::quoted(serialize_online_strategy_program({{profile}, {}}));
  }
  if (config.tseitin_cycle_action_producer.enabled) {
    OnlineInvocation profile;
    profile.rule = "TseitinCycleBreaking";
    profile.parameters = config.parameters.tseitin_cycle_breaking_reference;
    profile.granularity = InvocationGranularity::OneInstance;
    out << " producer " << std::quoted(profile.rule) << ' '
        << std::quoted(serialize_online_strategy_program({{profile}, {}}));
  }
  return out.str();
}

static std::string request_record(const ActionRequest &request) {
  std::ostringstream out;
  out << "request " << request.version << ' ' << request.sequence << ' '
      << request.expected.arena_generation << ' ' << request.expected.touch_epoch
      << ' ' << request.expected.strategy_epoch << ' ';
  if (request.type == ActionType::ResumeSearch)
    out << "ResumeSearch " << name(request.search);
  else {
    OnlineStrategyProgram program;
    program.sequence.push_back(request.invocation);
    out << "Rule " << serialize_online_strategy_program(program);
  }
  for (const auto &effort : request.producer_effort)
    out << " producer " << std::quoted(effort.family) << ' '
        << static_cast<unsigned>(effort.status) << ' ' << effort.complete << ' '
        << effort.preparation_work << ' ' << effort.enumeration_work << ' '
        << effort.total_work << ' ' << effort.candidates << ' '
        << effort.offers;
  for (const auto &effort : request.producer_effort)
    if (effort.have_detector_census)
      out << " census " << effort.candidate_domains << ' '
          << effort.exact_domains << ' ' << effort.relation_keys << ' '
          << effort.tuple_occurrences << ' '
          << static_cast<unsigned>(effort.first_decline_reason);
  return out.str();
}

static std::string result_record(const ActionResult &result) {
  std::ostringstream out;
  const auto &invocation = result.invocation;
  out << "result " << result.sequence << ' '
      << static_cast<unsigned>(invocation.status) << ' '
      << static_cast<unsigned>(invocation.rule_result) << ' '
      << static_cast<unsigned>(invocation.budget_status) << ' '
      << static_cast<unsigned>(invocation.work_unit) << ' '
      << invocation.work << ' ' << invocation.applications << ' '
      << invocation.made_rule_progress << ' ' << invocation.selection_complete << ' '
      << result.propagation_ticks << ' ' << result.root_assignments << ' '
      << result.after.arena_generation << ' ' << result.after.touch_epoch << ' '
      << result.after.strategy_epoch << ' ' << result.resumed_search << ' '
      << result.composition_overshoot;
  return out.str();
}

static std::string service_record(const Kernel &k, const char *service,
                                 uint64_t choice, ApplyResult result) {
  std::ostringstream out;
  out << "service " << service << ' ' << choice << ' '
      << static_cast<unsigned>(result) << ' ' << k.level << ' '
      << k.arena_generation << ' ' << k.touch_epoch << ' ' << k.strategy_epoch
      << ' ' << k.stats.decisions << ' ' << k.stats.conflicts << ' '
      << k.stats.learned << ' ' << k.stats.learned_glue_sum;
  return out.str();
}

std::string affine_matrix_service_trace_record(
    const Kernel &k, const AffineMatrixServiceResult &result) {
  std::ostringstream out;
  out << "service AffineMatrixSearch "
      << static_cast<unsigned>(result.status) << ' '
      << std::quoted(result.event_identity) << ' '
      << result.synchronization_work << ' ' << result.session_work << ' '
      << result.reason_width << ' ' << result.live.planner_work << ' '
      << result.live.revalidation_work << ' ' << result.live.additions << ' '
      << result.live.deletions << ' ' << result.live.helper_extent << ' '
      << result.live.journal_bytes << ' ' << result.bcp_pending << ' '
      << k.level << ' ' << k.arena_generation << ' ' << k.touch_epoch << ' '
      << k.strategy_epoch << ' ' << k.stats.decisions << ' '
      << k.stats.conflicts << ' ' << k.stats.learned << ' '
      << k.stats.learned_glue_sum;
  return out.str();
}

uint64_t RandomControllerSelector::draw(uint64_t bound) {
  assert(bound);
  // Rejection makes each eligible family/configuration equally likely without
  // relying on a registry-order-dependent modulo bias.
  const uint64_t threshold = -bound % bound;
  uint64_t value;
  do {
    value = stress_random(rng_);
  } while (value < threshold);
  return value % bound;
}

bool RandomControllerSelector::choose(
    Kernel &k, const Observation &observation, const RandomControllerConfig &config,
    ActionRequest &request, std::string &error, volatile sig_atomic_t *stop) {
  if (observation.searching || k.level || k.inconsistent || k.conflict_valid) {
    error = "controller selection requires a consistent root boundary";
    return false;
  }
  error.clear();

  const auto same_version = [](StateVersion left, StateVersion right) {
    return left.arena_generation == right.arena_generation &&
           left.touch_epoch == right.touch_epoch &&
           left.strategy_epoch == right.strategy_epoch;
  };
  const auto same_formula_version = [](StateVersion left, StateVersion right) {
    return left.arena_generation == right.arena_generation &&
           left.touch_epoch == right.touch_epoch;
  };
  const auto settled_prefix_dose = [](const InvocationResult &result) {
    // A CompleteCurrentPass remains one public invocation even when its
    // current selection takes several atomic controller doses. Completed and
    // consumed NoChange members retain the executor cursor; reissue the same
    // required step until that scoped selection reports completion.
    return result.status == InvocationStatus::Completed ||
           result.status == InvocationStatus::ScopedFixpoint ||
           result.status == InvocationStatus::NoChange ||
           result.status == InvocationStatus::Sat ||
           result.status == InvocationStatus::Unsat;
  };
  const auto settled_terminal = [](const InvocationResult &result) {
    if (result.made_rule_progress || result.status == InvocationStatus::Sat ||
        result.status == InvocationStatus::Unsat)
      return true;
    return result.selection_complete &&
           (result.status == InvocationStatus::Completed ||
            result.status == InvocationStatus::ScopedFixpoint ||
            result.status == InvocationStatus::NoChange);
  };
  if (startup_prefix_pending_sequence_) {
    if (!observation.last_action ||
        observation.last_action->sequence != startup_prefix_pending_sequence_) {
      error = "required controller startup prefix result is unavailable";
      return false;
    }
    const auto &result = observation.last_action->invocation;
    if (!settled_prefix_dose(result)) {
      error = "required controller startup prefix action did not complete";
      return false;
    }
    startup_prefix_pending_sequence_ = 0;
    if (result.selection_complete)
      ++startup_prefix_position_;
  }
  if (startup_resume_pending_sequence_) {
    // Choosing does not consume the mandatory ResumeSearch.  A stale request
    // leaves RestartController::last_action unchanged and therefore fails
    // here; a trace/replay rejection ends the policy before this selector is
    // reused.  ResumeSearch itself is atomic and has no interruptible window.
    if (!observation.last_action ||
        observation.last_action->sequence !=
            startup_resume_pending_sequence_) {
      error = "required controller startup ResumeSearch result is unavailable";
      return false;
    }
    const auto &result = *observation.last_action;
    if (result.type != ActionType::ResumeSearch || !result.resumed_search ||
        result.invocation.status != InvocationStatus::Completed) {
      error = "required controller startup ResumeSearch did not complete";
      return false;
    }
    startup_resume_pending_sequence_ = 0;
    startup_resume_forced_ = true;
  }
  enum class NativeSettlement : uint8_t {
    Pending,
    Consume,
    DiscardUntilContextChange,
    RetainUntilReconfiguration,
  };
  const auto native_settlement = [&](const InvocationResult &result) {
    switch (result.status) {
    case InvocationStatus::ResourceExhausted:
    case InvocationStatus::BudgetExhausted:
    case InvocationStatus::Interrupted:
      return NativeSettlement::RetainUntilReconfiguration;
    case InvocationStatus::Inapplicable:
    case InvocationStatus::InvalidRequest:
    case InvocationStatus::StaleRequest:
    case InvocationStatus::AccountingError:
      return NativeSettlement::DiscardUntilContextChange;
    default:
      return settled_terminal(result) ? NativeSettlement::Consume
                                      : NativeSettlement::Pending;
    }
  };
  if (guarded_cover_discovery_.pending && observation.last_action &&
      observation.last_action->sequence == guarded_cover_discovery_.pending_sequence) {
    const auto settlement = native_settlement(observation.last_action->invocation);
    if (settlement == NativeSettlement::RetainUntilReconfiguration) {
      guarded_cover_discovery_.reconfiguration_required = true;
      guarded_cover_discovery_.pending_sequence = 0;
    } else if (settlement != NativeSettlement::Pending) {
      guarded_cover_discovery_.pending.reset();
      guarded_cover_discovery_.pending_sequence = 0;
    }
  }
  bool affine_session_commit_observed = false;
  if (affine_basis_discovery_.pending && observation.last_action &&
      observation.last_action->sequence ==
          affine_basis_discovery_.pending_sequence) {
    const NativeSettlement settlement =
        native_settlement(observation.last_action->invocation);
    if (settlement == NativeSettlement::Consume) {
      affine_session_commit_observed =
          observation.last_action->invocation.made_rule_progress;
      if (affine_session_commit_observed && k.affine_basis_action_session &&
          affine_basis_discovery_.session ==
              k.affine_basis_action_session)
        affine_basis_discovery_.session_keys =
            k.affine_basis_action_session->committed_keys();
      const auto pending = *affine_basis_discovery_.pending;
      affine_basis_discovery_.offers.erase(
          std::remove(affine_basis_discovery_.offers.begin(),
                      affine_basis_discovery_.offers.end(), pending),
          affine_basis_discovery_.offers.end());
      affine_basis_discovery_.pending.reset();
      affine_basis_discovery_.pending_sequence = 0;
      affine_basis_discovery_.state =
          observation.last_action->invocation.after;
      if (affine_basis_discovery_.offers.empty() &&
          !affine_basis_discovery_.more) {
        affine_basis_discovery_.terminal = true;
        affine_basis_discovery_.durable_terminal = true;
      }
    } else if (settlement == NativeSettlement::DiscardUntilContextChange) {
      affine_basis_discovery_.pending.reset();
      affine_basis_discovery_.offers.clear();
      affine_basis_discovery_.pending_sequence = 0;
      affine_basis_discovery_.terminal = true;
      affine_basis_discovery_.durable_terminal = false;
      affine_basis_discovery_.state =
          observation.last_action->invocation.after;
    } else if (settlement ==
               NativeSettlement::RetainUntilReconfiguration) {
      affine_basis_discovery_.pending_sequence = 0;
      affine_basis_discovery_.reconfiguration_required = true;
      affine_basis_discovery_.state =
          observation.last_action->invocation.after;
    }
  }
  if (gf3_discovery_.pending && observation.last_action &&
      observation.last_action->sequence == gf3_discovery_.pending_sequence) {
    const NativeSettlement settlement =
        native_settlement(observation.last_action->invocation);
    if (settlement == NativeSettlement::Consume) {
      const auto pending = *gf3_discovery_.pending;
      gf3_discovery_.offers.erase(
          std::remove(gf3_discovery_.offers.begin(),
                      gf3_discovery_.offers.end(), pending),
          gf3_discovery_.offers.end());
      gf3_discovery_.pending.reset();
      gf3_discovery_.pending_sequence = 0;
      gf3_discovery_.state = observation.last_action->invocation.after;
      if (gf3_discovery_.offers.empty() && gf3_discovery_.complete)
        gf3_discovery_.terminal = true;
    } else if (settlement == NativeSettlement::DiscardUntilContextChange) {
      gf3_discovery_.pending.reset();
      gf3_discovery_.offers.clear();
      gf3_discovery_.pending_sequence = 0;
      gf3_discovery_.terminal = true;
      gf3_discovery_.state = observation.last_action->invocation.after;
    } else if (settlement ==
               NativeSettlement::RetainUntilReconfiguration) {
      gf3_discovery_.pending_sequence = 0;
      gf3_discovery_.reconfiguration_required = true;
      gf3_discovery_.state = observation.last_action->invocation.after;
    }
  }
  if (direct_csp_discovery_.pending && observation.last_action &&
      observation.last_action->sequence ==
          direct_csp_discovery_.pending_sequence) {
    const NativeSettlement settlement =
        native_settlement(observation.last_action->invocation);
    if (settlement == NativeSettlement::Consume) {
      if (observation.last_action->invocation.made_rule_progress &&
          k.direct_csp_support_action_session &&
          direct_csp_discovery_.session ==
              k.direct_csp_support_action_session) {
        direct_csp_discovery_.session_keys =
            k.direct_csp_support_action_session->committed_keys();
        direct_csp_discovery_.retained_owners =
            k.direct_csp_support_action_session->retained_owners();
      }
      const auto pending = *direct_csp_discovery_.pending;
      direct_csp_discovery_.offers.erase(
          std::remove(direct_csp_discovery_.offers.begin(),
                      direct_csp_discovery_.offers.end(), pending),
          direct_csp_discovery_.offers.end());
      direct_csp_discovery_.pending.reset();
      direct_csp_discovery_.pending_sequence = 0;
      direct_csp_discovery_.state = observation.last_action->invocation.after;
      if (direct_csp_discovery_.offers.empty() &&
          direct_csp_discovery_.complete)
        direct_csp_discovery_.terminal = true;
    } else if (settlement == NativeSettlement::DiscardUntilContextChange) {
      direct_csp_discovery_.pending.reset();
      direct_csp_discovery_.offers.clear();
      direct_csp_discovery_.pending_sequence = 0;
      direct_csp_discovery_.terminal = true;
      direct_csp_discovery_.state = observation.last_action->invocation.after;
    } else if (settlement ==
               NativeSettlement::RetainUntilReconfiguration) {
      direct_csp_discovery_.pending_sequence = 0;
      direct_csp_discovery_.reconfiguration_required = true;
      direct_csp_discovery_.state = observation.last_action->invocation.after;
    }
  }
  if (nary_csp_discovery_.pending && observation.last_action &&
      observation.last_action->sequence ==
          nary_csp_discovery_.pending_sequence) {
    const NativeSettlement settlement =
        native_settlement(observation.last_action->invocation);
    if (settlement == NativeSettlement::Consume) {
      const auto pending = *nary_csp_discovery_.pending;
      nary_csp_discovery_.offers.erase(
          std::remove(nary_csp_discovery_.offers.begin(),
                      nary_csp_discovery_.offers.end(), pending),
          nary_csp_discovery_.offers.end());
      nary_csp_discovery_.pending.reset();
      nary_csp_discovery_.pending_sequence = 0;
      nary_csp_discovery_.state = observation.last_action->invocation.after;
      if (nary_csp_discovery_.offers.empty() &&
          nary_csp_discovery_.complete)
        nary_csp_discovery_.terminal = true;
    } else if (settlement == NativeSettlement::DiscardUntilContextChange) {
      nary_csp_discovery_.pending.reset();
      nary_csp_discovery_.offers.clear();
      nary_csp_discovery_.pending_sequence = 0;
      nary_csp_discovery_.terminal = true;
      nary_csp_discovery_.state = observation.last_action->invocation.after;
    } else if (settlement ==
               NativeSettlement::RetainUntilReconfiguration) {
      nary_csp_discovery_.pending_sequence = 0;
      nary_csp_discovery_.reconfiguration_required = true;
      nary_csp_discovery_.state = observation.last_action->invocation.after;
    }
  }
  if (nary_csp_equality_discovery_.pending && observation.last_action &&
      observation.last_action->sequence ==
          nary_csp_equality_discovery_.pending_sequence) {
    const NativeSettlement settlement =
        native_settlement(observation.last_action->invocation);
    if (settlement == NativeSettlement::Consume) {
      const auto pending = *nary_csp_equality_discovery_.pending;
      nary_csp_equality_discovery_.offers.erase(
          std::remove(nary_csp_equality_discovery_.offers.begin(),
                      nary_csp_equality_discovery_.offers.end(), pending),
          nary_csp_equality_discovery_.offers.end());
      nary_csp_equality_discovery_.pending.reset();
      nary_csp_equality_discovery_.pending_sequence = 0;
      nary_csp_equality_discovery_.state =
          observation.last_action->invocation.after;
      if (nary_csp_equality_discovery_.offers.empty() &&
          nary_csp_equality_discovery_.complete)
        nary_csp_equality_discovery_.terminal = true;
    } else if (settlement == NativeSettlement::DiscardUntilContextChange) {
      nary_csp_equality_discovery_.pending.reset();
      nary_csp_equality_discovery_.offers.clear();
      nary_csp_equality_discovery_.pending_sequence = 0;
      nary_csp_equality_discovery_.terminal = true;
      nary_csp_equality_discovery_.state =
          observation.last_action->invocation.after;
    } else if (settlement ==
               NativeSettlement::RetainUntilReconfiguration) {
      nary_csp_equality_discovery_.pending_sequence = 0;
      nary_csp_equality_discovery_.reconfiguration_required = true;
      nary_csp_equality_discovery_.state =
          observation.last_action->invocation.after;
    }
  }
  if (nary_csp_equality_amo_discovery_.pending && observation.last_action &&
      observation.last_action->sequence ==
          nary_csp_equality_amo_discovery_.pending_sequence) {
    const NativeSettlement settlement =
        native_settlement(observation.last_action->invocation);
    if (settlement == NativeSettlement::Consume) {
      const auto pending = *nary_csp_equality_amo_discovery_.pending;
      nary_csp_equality_amo_discovery_.offers.erase(
          std::remove(nary_csp_equality_amo_discovery_.offers.begin(),
                      nary_csp_equality_amo_discovery_.offers.end(), pending),
          nary_csp_equality_amo_discovery_.offers.end());
      nary_csp_equality_amo_discovery_.pending.reset();
      nary_csp_equality_amo_discovery_.pending_sequence = 0;
      nary_csp_equality_amo_discovery_.state =
          observation.last_action->invocation.after;
      if (nary_csp_equality_amo_discovery_.offers.empty() &&
          nary_csp_equality_amo_discovery_.complete)
        nary_csp_equality_amo_discovery_.terminal = true;
    } else if (settlement == NativeSettlement::DiscardUntilContextChange) {
      nary_csp_equality_amo_discovery_.pending.reset();
      nary_csp_equality_amo_discovery_.offers.clear();
      nary_csp_equality_amo_discovery_.pending_sequence = 0;
      nary_csp_equality_amo_discovery_.terminal = true;
      nary_csp_equality_amo_discovery_.state =
          observation.last_action->invocation.after;
    } else if (settlement == NativeSettlement::RetainUntilReconfiguration) {
      nary_csp_equality_amo_discovery_.pending_sequence = 0;
      nary_csp_equality_amo_discovery_.reconfiguration_required = true;
      nary_csp_equality_amo_discovery_.state =
          observation.last_action->invocation.after;
    }
  }
  if (gf3_gaussian_discovery_.pending && observation.last_action &&
      observation.last_action->sequence ==
          gf3_gaussian_discovery_.pending_sequence) {
    const NativeSettlement settlement =
        native_settlement(observation.last_action->invocation);
    if (settlement == NativeSettlement::Consume) {
      gf3_gaussian_discovery_.pending.reset();
      gf3_gaussian_discovery_.offer.reset();
      gf3_gaussian_discovery_.pending_sequence = 0;
      gf3_gaussian_discovery_.terminal = true;
      gf3_gaussian_discovery_.state =
          observation.last_action->invocation.after;
    } else if (settlement == NativeSettlement::DiscardUntilContextChange) {
      gf3_gaussian_discovery_.pending.reset();
      gf3_gaussian_discovery_.offer.reset();
      gf3_gaussian_discovery_.pending_sequence = 0;
      gf3_gaussian_discovery_.terminal = true;
      gf3_gaussian_discovery_.state =
          observation.last_action->invocation.after;
    } else if (settlement ==
               NativeSettlement::RetainUntilReconfiguration) {
      gf3_gaussian_discovery_.pending_sequence = 0;
      gf3_gaussian_discovery_.reconfiguration_required = true;
      gf3_gaussian_discovery_.state =
          observation.last_action->invocation.after;
    }
  }
  if (gf3_general_row_discovery_.pending && observation.last_action &&
      observation.last_action->sequence ==
          gf3_general_row_discovery_.pending_sequence) {
    const NativeSettlement settlement =
        native_settlement(observation.last_action->invocation);
    if (settlement == NativeSettlement::Consume) {
      const auto pending = *gf3_general_row_discovery_.pending;
      gf3_general_row_discovery_.offers.erase(
          std::remove(gf3_general_row_discovery_.offers.begin(),
                      gf3_general_row_discovery_.offers.end(), pending),
          gf3_general_row_discovery_.offers.end());
      gf3_general_row_discovery_.pending.reset();
      gf3_general_row_discovery_.pending_sequence = 0;
      gf3_general_row_discovery_.state =
          observation.last_action->invocation.after;
      if (gf3_general_row_discovery_.offers.empty() &&
          gf3_general_row_discovery_.complete)
        gf3_general_row_discovery_.terminal = true;
    } else if (settlement == NativeSettlement::DiscardUntilContextChange) {
      gf3_general_row_discovery_.pending.reset();
      gf3_general_row_discovery_.offers.clear();
      gf3_general_row_discovery_.pending_sequence = 0;
      gf3_general_row_discovery_.terminal = true;
      gf3_general_row_discovery_.state =
          observation.last_action->invocation.after;
    } else if (settlement == NativeSettlement::RetainUntilReconfiguration) {
      gf3_general_row_discovery_.pending_sequence = 0;
      gf3_general_row_discovery_.reconfiguration_required = true;
      gf3_general_row_discovery_.state =
          observation.last_action->invocation.after;
    }
  }
  if (tseitin_cycle_discovery_.pending && observation.last_action &&
      observation.last_action->sequence ==
          tseitin_cycle_discovery_.pending_sequence) {
    const NativeSettlement settlement =
        native_settlement(observation.last_action->invocation);
    if (settlement == NativeSettlement::Consume) {
      const std::string pending = *tseitin_cycle_discovery_.pending;
      tseitin_cycle_discovery_.offers.erase(
          std::remove(tseitin_cycle_discovery_.offers.begin(),
                      tseitin_cycle_discovery_.offers.end(), pending),
          tseitin_cycle_discovery_.offers.end());
      tseitin_cycle_discovery_.pending.reset();
      tseitin_cycle_discovery_.pending_sequence = 0;
      tseitin_cycle_discovery_.state =
          observation.last_action->invocation.after;
      if (observation.last_action->invocation.made_rule_progress) {
        tseitin_cycle_discovery_.offers.clear();
        // The installed unit invalidates every old exact payload.  A later
        // separately selected propagation changes the root context and may
        // expose a newly authenticated component census.
        tseitin_cycle_discovery_.terminal = false;
      } else if (tseitin_cycle_discovery_.offers.empty()) {
        tseitin_cycle_discovery_.terminal = true;
      }
    } else if (settlement == NativeSettlement::DiscardUntilContextChange) {
      tseitin_cycle_discovery_.pending.reset();
      tseitin_cycle_discovery_.offers.clear();
      tseitin_cycle_discovery_.pending_sequence = 0;
      tseitin_cycle_discovery_.terminal = true;
      tseitin_cycle_discovery_.state =
          observation.last_action->invocation.after;
    } else if (settlement ==
               NativeSettlement::RetainUntilReconfiguration) {
      tseitin_cycle_discovery_.pending_sequence = 0;
      tseitin_cycle_discovery_.reconfiguration_required = true;
      tseitin_cycle_discovery_.state =
          observation.last_action->invocation.after;
    }
  }
  std::vector<ActionProducerEffort> producer_effort;
  const auto saturated_add = [](uint64_t left, uint64_t right) {
    return right > UINT64_MAX - left ? UINT64_MAX : left + right;
  };
  const auto has_static_offer = [&](const char *family) {
    const auto found = config.offered_actions.find(family);
    return found != config.offered_actions.end() && !found->second.empty();
  };
  const auto affine_profile = [&] {
    OnlineInvocation invocation;
    invocation.rule = "AffineConsequence";
    invocation.parameters = config.parameters.affine_consequence_reference;
    invocation.granularity = InvocationGranularity::CanonicalApplication;
    std::ostringstream out;
    out << config.affine_basis_action_producer.maximum_offers << ' '
        << serialize_online_strategy_program({{invocation}, {}});
    return out.str();
  };
  const auto affine_limits = [&] {
    const auto &p = config.parameters.affine_consequence_reference;
    AffineBasisActionLimits limits;
    limits.image = {p.maximum_clauses, p.maximum_groups, p.maximum_patterns,
                    p.maximum_elimination_work, p.maximum_arity,
                    p.maximum_intermediate_width, p.maximum_sources,
                    p.maximum_output_width};
    limits.basis = {p.basis_maximum_work,
                    p.basis_maximum_intermediate_width,
                    p.basis_maximum_sources,
                    p.basis_maximum_premises,
                    p.basis_maximum_nodes,
                    p.basis_maximum_direct_width};
    limits.maximum_snapshot_literals = p.maximum_snapshot_literals;
    limits.maximum_snapshot_work = p.maximum_snapshot_work;
    limits.maximum_proof_additions = p.maximum_proof_additions;
    limits.maximum_proof_deletions = p.maximum_proof_deletions;
    limits.maximum_journal_bytes = p.maximum_journal_bytes;
    limits.maximum_external_extent = p.maximum_external_extent;
    limits.maximum_session_keys = p.maximum_session_keys;
    limits.maximum_session_work = p.maximum_session_work;
    return limits;
  };
  const auto gf3_profile = [&] {
    OnlineInvocation invocation;
    invocation.rule = "Gf3AffineConsequence";
    invocation.parameters = config.parameters.gf3_affine_consequence_reference;
    invocation.granularity = InvocationGranularity::OneInstance;
    invocation.gf3_affine_instances = {{0, 1}};
    std::ostringstream out;
    const auto &producer = config.gf3_action_producer;
    out << producer.maximum_pair_candidates << ' ' << producer.maximum_offers
        << ' ' << producer.maximum_enumeration_work << ' '
        << serialize_online_strategy_program({{invocation}, {}});
    return out.str();
  };
  const auto direct_profile = [&] {
    OnlineInvocation invocation;
    invocation.rule = "DirectCspSupport";
    invocation.parameters = config.parameters.direct_csp_support_reference;
    invocation.granularity = InvocationGranularity::OneInstance;
    invocation.direct_csp_support_instances = {{{1, 2}, {3, 4}, 0}};
    std::ostringstream out;
    const auto &producer = config.direct_csp_action_producer;
    out << producer.maximum_domain_candidates << ' '
        << producer.maximum_pair_candidates << ' '
        << producer.maximum_row_candidates << ' ' << producer.maximum_offers
        << ' ' << producer.maximum_enumeration_work << ' '
        << serialize_online_strategy_program({{invocation}, {}});
    return out.str();
  };

  const auto nary_profile = [&] {
    OnlineInvocation invocation;
    invocation.rule = "NaryCspPrefix";
    invocation.parameters = config.parameters.nary_csp_prefix_reference;
    invocation.granularity = InvocationGranularity::OneInstance;
    invocation.nary_csp_prefix_instances = {
        {{{1, 2}, {3, 4}, {5, 6}}, {2, {{0, 0}, {1, 0}}}}};
    std::ostringstream out;
    const auto &producer = config.nary_csp_action_producer;
    out << producer.relation_arity << ' '
        << producer.maximum_domain_candidates << ' '
        << producer.maximum_relation_candidates << ' '
        << producer.maximum_prefix_candidates << ' '
        << producer.maximum_offers << ' '
        << producer.maximum_enumeration_work << ' '
        << serialize_online_strategy_program({{invocation}, {}});
    if (producer.diagnostic_census_once) out << " census-once";
    return out.str();
  };

  const auto nary_limits = [&] {
    const auto &p = config.parameters.nary_csp_prefix_reference;
    return NaryCspPrefixActionLimits{
        p.maximum_source_clauses, p.maximum_source_literals,
        p.maximum_recognition_work, p.maximum_domains,
        p.maximum_domain_values, p.maximum_forbidden_tuples,
        p.maximum_prefix_arity, p.maximum_definition_clause_literals,
        p.maximum_proof_work, p.maximum_proof_additions,
        p.maximum_proof_literal_occurrences, p.maximum_journal_bytes,
        p.maximum_extent_growth, p.maximum_external_extent,
        p.maximum_session_keys, p.maximum_session_work};
  };

  const auto nary_equality_profile = [&] {
    OnlineInvocation invocation;
    invocation.rule = "NaryCspEquality";
    invocation.parameters = config.parameters.nary_csp_equality_reference;
    invocation.granularity = InvocationGranularity::OneInstance;
    invocation.nary_csp_equality_instances = {
        {{{1, 2}, {3, 4}, {5, 6}}, {0, 1, 2}}};
    std::ostringstream out;
    const auto &producer = config.nary_csp_equality_action_producer;
    out << producer.maximum_owner_candidates << ' '
        << producer.maximum_coordinate_candidates << ' '
        << producer.maximum_offers << ' '
        << producer.maximum_enumeration_work << ' '
        << serialize_online_strategy_program({{invocation}, {}});
    return out.str();
  };

  const auto nary_equality_limits = [&] {
    const auto &p = config.parameters.nary_csp_equality_reference;
    return NaryCspEqualityLimits{
        p.maximum_source_clauses, p.maximum_source_literals,
        p.maximum_recognition_work, p.maximum_domain_values,
        p.maximum_forbidden_tuples, p.maximum_proof_work,
        p.maximum_proof_additions, p.maximum_proof_literal_occurrences,
        p.maximum_journal_bytes, p.maximum_extent_growth,
        p.maximum_external_extent, p.maximum_session_keys};
  };

  const auto nary_equality_amo_profile = [&] {
    OnlineInvocation invocation;
    invocation.rule = "NaryCspEqualityAmo";
    invocation.parameters = config.parameters.nary_csp_equality_amo_reference;
    invocation.granularity = InvocationGranularity::OneInstance;
    invocation.nary_csp_equality_amo_instances = {
        {{{1, 2}, {3, 4}, {5, 6}}}};
    std::ostringstream out;
    const auto &producer = config.nary_csp_equality_amo_producer;
    out << producer.maximum_owner_candidates << ' '
        << producer.maximum_offers << ' '
        << producer.maximum_enumeration_work << ' '
        << serialize_online_strategy_program({{invocation}, {}});
    return out.str();
  };

  const auto nary_equality_amo_limits = [&] {
    const auto &p = config.parameters.nary_csp_equality_amo_reference;
    return NaryCspEqualityAmoLimits{
        p.maximum_source_clauses, p.maximum_source_literals,
        p.maximum_recognition_work, p.maximum_domain_values,
        p.maximum_forbidden_tuples, p.maximum_proof_work,
        p.maximum_proof_additions, p.maximum_proof_literal_occurrences,
        p.maximum_journal_bytes, p.maximum_external_extent,
        p.maximum_session_keys};
  };

  const auto gaussian_profile = [&] {
    OnlineInvocation invocation;
    invocation.rule = "Gf3GaussianConsequence";
    invocation.parameters =
        config.parameters.gf3_gaussian_consequence_reference;
    invocation.granularity = InvocationGranularity::OneInstance;
    invocation.gf3_gaussian_instances = {"g3b1:00"};
    return serialize_online_strategy_program({{invocation}, {}});
  };

  const auto gaussian_limits = [&] {
    const auto &p = config.parameters.gf3_gaussian_consequence_reference;
    return Gf3GaussianActionLimits{
        {p.maximum_source_clauses, p.maximum_source_literals,
         p.maximum_domains, p.maximum_equations,
         p.maximum_equation_arity, p.maximum_tuples_per_equation},
        {{p.maximum_bridge_work, p.maximum_bridge_source_clauses,
          p.maximum_bridge_source_literals, p.maximum_bridge_domains,
          p.maximum_bridge_equations, p.maximum_bridge_terms,
          p.maximum_authentication_references,
          p.maximum_references_per_equation,
          {p.maximum_gaussian_work, p.maximum_fill_entries,
           p.maximum_intermediate_width, p.maximum_provenance_terms,
           p.maximum_derivations, p.maximum_retained_plan_bytes}},
         p.maximum_certificate_work, p.maximum_events,
         p.maximum_event_literals, p.maximum_active_clauses,
         p.maximum_retained_source_references, p.maximum_event_width,
         p.maximum_branch_domains, p.maximum_consequences},
        p.maximum_capture_work, p.maximum_revalidation_work,
        p.maximum_proof_additions, p.maximum_proof_literal_occurrences,
        p.maximum_journal_bytes, p.maximum_identity_bytes,
        p.maximum_external_extent, p.maximum_session_keys,
        p.maximum_session_work, p.maximum_session_identity_bytes};
  };

  const auto general_row_profile = [&] {
    OnlineInvocation invocation;
    invocation.rule = "Gf3GeneralRowConsequence";
    invocation.parameters =
        config.parameters.gf3_general_row_consequence_reference;
    invocation.granularity = InvocationGranularity::OneInstance;
    invocation.post_application = PostApplication::Propagate;
    invocation.gf3_general_row_instances = {{0, 0, "g3r1:00"}};
    std::ostringstream out;
    const auto &producer = config.gf3_general_row_action_producer;
    out << producer.maximum_candidates << ' ' << producer.maximum_offers
        << ' ' << producer.maximum_enumeration_work << ' '
        << serialize_online_strategy_program({{invocation}, {}});
    return out.str();
  };

  const auto general_row_limits = [&] {
    const auto &p = config.parameters.gf3_general_row_consequence_reference;
    const auto &g = p.gaussian;
    return Gf3GeneralRowActionLimits{
        {{g.maximum_source_clauses, g.maximum_source_literals,
          g.maximum_domains, g.maximum_equations,
          g.maximum_equation_arity, g.maximum_tuples_per_equation},
         {{g.maximum_bridge_work, g.maximum_bridge_source_clauses,
           g.maximum_bridge_source_literals, g.maximum_bridge_domains,
           g.maximum_bridge_equations, g.maximum_bridge_terms,
           g.maximum_authentication_references,
           g.maximum_references_per_equation,
           {g.maximum_gaussian_work, g.maximum_fill_entries,
            g.maximum_intermediate_width, g.maximum_provenance_terms,
            g.maximum_derivations, g.maximum_retained_plan_bytes}},
          g.maximum_certificate_work, g.maximum_events,
          g.maximum_event_literals, g.maximum_active_clauses,
          g.maximum_retained_source_references, p.maximum_row_tuples,
          g.maximum_event_width, g.maximum_branch_domains,
          p.maximum_row_width},
         g.maximum_capture_work, p.maximum_duplicate_scan_work,
         g.maximum_revalidation_work, g.maximum_proof_additions,
         g.maximum_proof_literal_occurrences, p.maximum_arena_words,
         g.maximum_journal_bytes, g.maximum_identity_bytes,
         g.maximum_external_extent},
        g.maximum_session_keys, g.maximum_session_work,
        g.maximum_session_identity_bytes};
  };

  const auto tseitin_profile = [&] {
    OnlineInvocation invocation;
    invocation.rule = "TseitinCycleBreaking";
    invocation.parameters = config.parameters.tseitin_cycle_breaking_reference;
    invocation.granularity = InvocationGranularity::OneInstance;
    return serialize_online_strategy_program({{invocation}, {}});
  };

  const auto tseitin_limits = [&] {
    const auto &p = config.parameters.tseitin_cycle_breaking_reference;
    TseitinCycleBreakingLimits result;
    result.snapshot = {p.maximum_snapshot_clauses,
                       p.maximum_snapshot_literals,
                       p.maximum_snapshot_work,
                       p.maximum_snapshot_variables, nullptr, nullptr};
    result.discovery = {p.maximum_discovery_work,
                        p.maximum_discovery_clauses,
                        p.maximum_discovery_groups,
                        4, nullptr, nullptr,
                        p.maximum_components,
                        p.maximum_discovery_retained_bytes};
    result.admission = {p.maximum_admission_work, nullptr, nullptr};
    result.planning = {p.maximum_planning_work, p.maximum_witness_length,
                       p.maximum_planning_additions};
    result.transaction = {p.maximum_transaction_work,
                          p.maximum_transaction_additions,
                          p.maximum_transaction_deletions,
                          p.maximum_transaction_helpers,
                          p.maximum_transaction_witness_literals,
                          nullptr, nullptr};
    result.maximum_validation_ticks = p.maximum_validation_ticks;
    result.maximum_components = p.maximum_components;
    result.maximum_identity_bytes = p.maximum_identity_bytes;
    result.maximum_total_identity_bytes = p.maximum_total_identity_bytes;
    result.maximum_identity_work = p.maximum_identity_work;
    result.maximum_journal_bytes = p.maximum_journal_bytes;
    result.maximum_session_keys = p.maximum_session_keys;
    result.maximum_session_work = p.maximum_session_work;
    result.maximum_session_identity_bytes = p.maximum_session_identity_bytes;
    return result;
  };

  const auto discover_affine = [&]() -> bool {
    if (!config.affine_basis_action_producer.enabled ||
        has_static_offer("AffineConsequence") ||
        !k.affine_basis_action_session)
      return false;
    if (!config.affine_basis_action_producer.valid()) {
      error = "GF2 affine structured action producer window is invalid";
      return false;
    }
    const auto &session = *k.affine_basis_action_session;
    const auto profile = affine_profile();
    const auto state = state_version(k);
    const bool owner_changed = affine_basis_discovery_.owner != &k ||
                               affine_basis_discovery_.session != &session;
    const bool profile_changed = affine_basis_discovery_.profile != profile;
    const bool state_changed =
        !same_version(affine_basis_discovery_.state, state);
    const bool session_changed =
        affine_basis_discovery_.session_keys != session.committed_keys();
    const bool session_commit =
        session.committed_keys() > affine_basis_discovery_.session_keys;
    const auto reset_discovery = [&]() {
      affine_basis_discovery_ = {};
      affine_basis_discovery_.owner = &k;
      affine_basis_discovery_.session = &session;
      affine_basis_discovery_.state = state;
      affine_basis_discovery_.session_keys = session.committed_keys();
      affine_basis_discovery_.profile = profile;
    };
    if (owner_changed ||
        (session_changed && !affine_session_commit_observed &&
         !session_commit) ||
        (profile_changed &&
         (!affine_basis_discovery_.reconfiguration_required ||
          !affine_basis_discovery_.pending))) {
      reset_discovery();
    } else if (session_changed) {
      // A successful exact row action only grows the durable consumed-key
      // set.  The retained owner and every other payload in the already
      // authenticated page stay exact, so preserve them instead of repeating
      // the full recognition/basis preflight after every row.
      affine_basis_discovery_.session_keys = session.committed_keys();
    } else if (affine_basis_discovery_.reconfiguration_required &&
               profile_changed) {
      affine_basis_discovery_.state = state;
      affine_basis_discovery_.session_keys = session.committed_keys();
      affine_basis_discovery_.profile = profile;
      affine_basis_discovery_.reconfiguration_required = false;
    } else if (state_changed) {
      if (session.bound() && session.belongs_to(k) && session.owner() &&
          session.owner()->arena_generation == k.arena_generation) {
        // Root propagation changes the touch epoch but does not invalidate
        // exact rows owned by the durable session snapshot.  Retain the
        // authenticated page across that boundary; live apply rederives and
        // validates the selected row and all of its sources before mutation.
        affine_basis_discovery_.state = state;
      } else if (affine_basis_discovery_.durable_terminal ||
          affine_basis_discovery_.reconfiguration_required) {
        affine_basis_discovery_.state = state;
      } else {
        reset_discovery();
      }
    }
    if (affine_basis_discovery_.reconfiguration_required) return false;
    if (affine_basis_discovery_.pending ||
        !affine_basis_discovery_.offers.empty())
      return true;
    if (affine_basis_discovery_.terminal) return false;
    const auto limits = affine_limits();
    if (!limits.valid()) {
      error = "GF2 affine structured action producer parameters are invalid";
      return false;
    }
    const auto page = affine_basis_action_offers(
        k, session, limits, affine_basis_discovery_.cursor,
        config.affine_basis_action_producer.maximum_offers);
    uint64_t preparation = page.preflight_recognition_work;
    preparation = saturated_add(preparation, page.preflight_basis_work);
    preparation = saturated_add(preparation, page.preflight_additions);
    preparation = saturated_add(preparation, page.preflight_deletions);
    producer_effort.push_back(
        {"AffineConsequence", static_cast<uint8_t>(page.status), !page.more,
         preparation, page.enumeration_work,
         saturated_add(preparation, page.enumeration_work),
         page.candidates_examined, page.offers.size()});
    if (page.status == AffineBasisOfferStatus::Interrupted ||
        page.status == AffineBasisOfferStatus::ResourceExhausted) {
      affine_basis_discovery_.reconfiguration_required = true;
      if (page.status == AffineBasisOfferStatus::Interrupted)
        error = "controller selection interrupted";
      return false;
    }
    if (page.status != AffineBasisOfferStatus::Complete ||
        page.offers.empty()) {
      affine_basis_discovery_.terminal = true;
      affine_basis_discovery_.durable_terminal =
          !page.more && (page.status == AffineBasisOfferStatus::Complete ||
                         page.status == AffineBasisOfferStatus::NoStructure);
      return false;
    }
    affine_basis_discovery_.offers = page.offers;
    affine_basis_discovery_.more = page.more;
    affine_basis_discovery_.cursor = page.continuation;
    return true;
  };

  const auto discover_gf3 = [&]() -> bool {
    if (!config.gf3_action_producer.enabled || has_static_offer("Gf3AffineConsequence") ||
        !k.gf3_affine_action_session)
      return false;
    if (!config.gf3_action_producer.valid()) {
      error = "GF3 structured action producer window is invalid";
      return false;
    }
    const auto &session = *k.gf3_affine_action_session;
    const auto profile = gf3_profile();
    const auto state = state_version(k);
    const bool owner_changed = gf3_discovery_.owner != &k ||
                               gf3_discovery_.session != &session;
    const bool profile_changed = gf3_discovery_.profile != profile;
    const bool state_or_session_changed =
        owner_changed || !same_version(gf3_discovery_.state, state) ||
        gf3_discovery_.session_keys != session.committed_keys();
    if (state_or_session_changed ||
        (profile_changed &&
         (!gf3_discovery_.reconfiguration_required ||
          !gf3_discovery_.pending))) {
      gf3_discovery_ = {};
      gf3_discovery_.owner = &k;
      gf3_discovery_.session = &session;
      gf3_discovery_.state = state;
      gf3_discovery_.session_keys = session.committed_keys();
      gf3_discovery_.profile = profile;
    } else if (gf3_discovery_.reconfiguration_required && profile_changed) {
      gf3_discovery_.state = state;
      gf3_discovery_.session_keys = session.committed_keys();
      gf3_discovery_.profile = profile;
      gf3_discovery_.reconfiguration_required = false;
    }
    if (gf3_discovery_.reconfiguration_required) return false;
    if (gf3_discovery_.pending || !gf3_discovery_.offers.empty())
      return true;
    if (gf3_discovery_.terminal)
      return false;
    const auto &producer = config.gf3_action_producer;
    Gf3AffineActionOfferWindow window{
        producer.maximum_pair_candidates, producer.maximum_offers,
        producer.maximum_enumeration_work, gf3_discovery_.have_after,
        gf3_discovery_.after};
    const Gf3AffineActionLimits limits{
        {config.parameters.gf3_affine_consequence_reference.maximum_source_clauses,
         config.parameters.gf3_affine_consequence_reference.maximum_source_literals,
         config.parameters.gf3_affine_consequence_reference.maximum_domains,
         config.parameters.gf3_affine_consequence_reference.maximum_equations,
         config.parameters.gf3_affine_consequence_reference.maximum_equation_arity,
         config.parameters.gf3_affine_consequence_reference.maximum_tuples_per_equation},
        {config.parameters.gf3_affine_consequence_reference.maximum_result_arity,
         config.parameters.gf3_affine_consequence_reference.maximum_result_tuples,
         config.parameters.gf3_affine_consequence_reference.maximum_additions,
         config.parameters.gf3_affine_consequence_reference.maximum_addition_literals},
        config.parameters.gf3_affine_consequence_reference.maximum_recognition_work,
        config.parameters.gf3_affine_consequence_reference.maximum_planning_work,
        config.parameters.gf3_affine_consequence_reference.maximum_duplicate_scan_work,
        config.parameters.gf3_affine_consequence_reference.maximum_revalidation_work,
        config.parameters.gf3_affine_consequence_reference.maximum_proof_work,
        config.parameters.gf3_affine_consequence_reference.maximum_proof_additions,
        config.parameters.gf3_affine_consequence_reference.maximum_proof_literal_occurrences,
        config.parameters.gf3_affine_consequence_reference.maximum_arena_words,
        config.parameters.gf3_affine_consequence_reference.maximum_journal_bytes,
        config.parameters.gf3_affine_consequence_reference.maximum_external_extent,
        config.parameters.gf3_affine_consequence_reference.maximum_session_keys,
        config.parameters.gf3_affine_consequence_reference.maximum_session_work};
    if (!limits.valid()) {
      error = "GF3 structured action producer parameters are invalid";
      return false;
    }
    const auto offered = gf3_affine_action_offer(k, session, limits, window);
    uint64_t preparation = offered.effort.source_work;
    preparation = saturated_add(preparation, offered.effort.recognition_work);
    preparation = saturated_add(preparation, offered.effort.revalidation_work);
    preparation = saturated_add(preparation, offered.effort.session_work);
    producer_effort.push_back(
        {"Gf3AffineConsequence", static_cast<uint8_t>(offered.status),
         offered.complete, preparation, offered.effort.enumeration_work,
         saturated_add(preparation, offered.effort.enumeration_work),
         offered.effort.pair_candidates, offered.offers.size()});
    if (offered.status == Gf3AffineActionOfferStatus::Interrupted ||
        offered.status == Gf3AffineActionOfferStatus::ResourceExhausted) {
      gf3_discovery_.reconfiguration_required = true;
      return false;
    }
    if (offered.status != Gf3AffineActionOfferStatus::Ready) {
      gf3_discovery_.terminal = true;
      return false;
    }
    const bool cursor_advanced =
        offered.have_resume_after &&
        (!gf3_discovery_.have_after ||
         !(offered.resume_after == gf3_discovery_.after));
    gf3_discovery_.offers = offered.offers;
    gf3_discovery_.complete = offered.complete;
    gf3_discovery_.have_after = offered.have_resume_after;
    if (offered.have_resume_after)
      gf3_discovery_.after = offered.resume_after;
    if (offered.complete && offered.offers.empty())
      gf3_discovery_.terminal = true;
    else if (offered.offers.empty() && !cursor_advanced)
      gf3_discovery_.terminal = true;
    return !gf3_discovery_.offers.empty();
  };

  const auto discover_direct = [&]() -> bool {
    if (!config.direct_csp_action_producer.enabled ||
        has_static_offer("DirectCspSupport") ||
        !k.direct_csp_support_action_session)
      return false;
    if (!config.direct_csp_action_producer.valid()) {
      error = "DirectCSP structured action producer window is invalid";
      return false;
    }
    const auto &session = *k.direct_csp_support_action_session;
    const auto profile = direct_profile();
    const auto state = state_version(k);
    const bool owner_changed = direct_csp_discovery_.owner != &k ||
                               direct_csp_discovery_.session != &session;
    const bool profile_changed = direct_csp_discovery_.profile != profile;
    const bool state_or_session_changed =
        owner_changed ||
        !same_formula_version(direct_csp_discovery_.state, state) ||
        direct_csp_discovery_.session_keys != session.committed_keys() ||
        direct_csp_discovery_.retained_owners != session.retained_owners();
    if (state_or_session_changed ||
        (profile_changed &&
         (!direct_csp_discovery_.reconfiguration_required ||
          !direct_csp_discovery_.pending))) {
      direct_csp_discovery_ = {};
      direct_csp_discovery_.owner = &k;
      direct_csp_discovery_.session = &session;
      direct_csp_discovery_.state = state;
      direct_csp_discovery_.session_keys = session.committed_keys();
      direct_csp_discovery_.retained_owners = session.retained_owners();
      direct_csp_discovery_.profile = profile;
    } else if (direct_csp_discovery_.reconfiguration_required &&
               profile_changed) {
      direct_csp_discovery_.state = state;
      direct_csp_discovery_.session_keys = session.committed_keys();
      direct_csp_discovery_.retained_owners = session.retained_owners();
      direct_csp_discovery_.profile = profile;
      direct_csp_discovery_.reconfiguration_required = false;
    }
    if (direct_csp_discovery_.reconfiguration_required) return false;
    if (direct_csp_discovery_.pending ||
        !direct_csp_discovery_.offers.empty())
      return true;
    if (direct_csp_discovery_.terminal)
      return false;
    const auto &producer = config.direct_csp_action_producer;
    DirectCspSupportActionOfferWindow window{
        producer.maximum_domain_candidates, producer.maximum_pair_candidates,
        producer.maximum_row_candidates, producer.maximum_offers,
        producer.maximum_enumeration_work, direct_csp_discovery_.have_after,
        direct_csp_discovery_.after};
    const auto &parameters = config.parameters.direct_csp_support_reference;
    const DirectCspSupportActionLimits limits{
        parameters.maximum_source_clauses, parameters.maximum_source_literals,
        parameters.maximum_recognition_work, parameters.maximum_domain_values,
        parameters.maximum_direct_clause_literals,
        parameters.maximum_definition_clause_literals,
        parameters.maximum_proof_work, parameters.maximum_proof_additions,
        parameters.maximum_proof_literal_occurrences,
        parameters.maximum_journal_bytes, parameters.maximum_extent_growth,
        parameters.maximum_external_extent, parameters.maximum_session_keys,
        parameters.maximum_session_work, parameters.allow_extension};
    if (!limits.valid()) {
      error = "DirectCSP structured action producer parameters are invalid";
      return false;
    }
    const auto offered =
        direct_csp_support_action_offer(k, session, limits, window);
    const uint64_t preparation = offered.effort.session_work;
    producer_effort.push_back(
        {"DirectCspSupport", static_cast<uint8_t>(offered.status),
         offered.complete, preparation, offered.effort.enumeration_work,
         saturated_add(preparation, offered.effort.enumeration_work),
         offered.effort.row_candidates, offered.offers.size()});
    if (offered.status == DirectCspSupportActionOfferStatus::Interrupted ||
        offered.status ==
            DirectCspSupportActionOfferStatus::ResourceExhausted) {
      direct_csp_discovery_.reconfiguration_required = true;
      return false;
    }
    if (offered.status != DirectCspSupportActionOfferStatus::Ready) {
      direct_csp_discovery_.terminal = true;
      return false;
    }
    const bool cursor_advanced =
        offered.have_resume_after &&
        (!direct_csp_discovery_.have_after ||
         !(offered.resume_after == direct_csp_discovery_.after));
    direct_csp_discovery_.offers = offered.offers;
    direct_csp_discovery_.complete = offered.complete;
    direct_csp_discovery_.have_after = offered.have_resume_after;
    if (offered.have_resume_after)
      direct_csp_discovery_.after = offered.resume_after;
    if (offered.complete && offered.offers.empty())
      direct_csp_discovery_.terminal = true;
    else if (offered.offers.empty() && !cursor_advanced)
      direct_csp_discovery_.terminal = true;
    return !direct_csp_discovery_.offers.empty();
  };

  const auto discover_nary = [&]() -> bool {
    if (!config.nary_csp_action_producer.enabled ||
        has_static_offer("NaryCspPrefix") ||
        !k.nary_csp_prefix_action_session)
      return false;
    if (!config.nary_csp_action_producer.valid()) {
      error = "n-ary CSP structured action producer window is invalid";
      return false;
    }
    const auto &session = *k.nary_csp_prefix_action_session;
    const auto profile = nary_profile();
    const auto state = state_version(k);
    const bool owner_changed = nary_csp_discovery_.owner != &k ||
                               nary_csp_discovery_.session != &session;
    const bool profile_changed = nary_csp_discovery_.profile != profile;
    const bool state_or_session_changed =
        owner_changed || !same_version(nary_csp_discovery_.state, state) ||
        nary_csp_discovery_.session_keys != session.committed_keys() ||
        nary_csp_discovery_.retained_owners != session.retained_owners();
    if (state_or_session_changed ||
        (profile_changed &&
         (!nary_csp_discovery_.reconfiguration_required ||
          !nary_csp_discovery_.pending))) {
      nary_csp_discovery_ = {};
      nary_csp_discovery_.owner = &k;
      nary_csp_discovery_.session = &session;
      nary_csp_discovery_.state = state;
      nary_csp_discovery_.session_keys = session.committed_keys();
      nary_csp_discovery_.retained_owners = session.retained_owners();
      nary_csp_discovery_.profile = profile;
    } else if (nary_csp_discovery_.reconfiguration_required &&
               profile_changed) {
      nary_csp_discovery_.state = state;
      nary_csp_discovery_.session_keys = session.committed_keys();
      nary_csp_discovery_.retained_owners = session.retained_owners();
      nary_csp_discovery_.profile = profile;
      nary_csp_discovery_.reconfiguration_required = false;
    }
    if (nary_csp_discovery_.reconfiguration_required) return false;
    if (nary_csp_discovery_.pending || !nary_csp_discovery_.offers.empty())
      return true;
    if (nary_csp_discovery_.terminal) return false;
    const auto &producer = config.nary_csp_action_producer;
    NaryCspPrefixActionOfferWindow window;
    window.relation_arity = producer.relation_arity;
    window.maximum_domain_candidates = producer.maximum_domain_candidates;
    window.maximum_relation_candidates = producer.maximum_relation_candidates;
    window.maximum_prefix_candidates = producer.maximum_prefix_candidates;
    window.maximum_offers = producer.maximum_offers;
    window.maximum_enumeration_work = producer.maximum_enumeration_work;
    window.have_after = nary_csp_discovery_.have_after;
    if (window.have_after) window.after = nary_csp_discovery_.after;
    const auto limits = nary_limits();
    if (!limits.valid()) {
      error = "n-ary CSP structured action producer parameters are invalid";
      return false;
    }
    const auto offered =
        nary_csp_prefix_action_offer(k, session, limits, window);
    const uint64_t preparation = offered.effort.session_work;
    ActionProducerEffort effort{
        "NaryCspPrefix", static_cast<uint8_t>(offered.status),
        offered.complete, preparation, offered.effort.enumeration_work,
        saturated_add(preparation, offered.effort.enumeration_work),
        offered.effort.prefix_candidates, offered.offers.size()};
    if (producer.diagnostic_census_once && !nary_csp_census_emitted_) {
      effort.have_detector_census = true;
      effort.candidate_domains = offered.effort.domain_candidates;
      effort.exact_domains = offered.effort.exact_domains;
      effort.relation_keys = offered.effort.indexed_relation_keys;
      effort.tuple_occurrences = offered.effort.indexed_tuple_occurrences;
      effort.first_decline_reason = static_cast<uint8_t>(offered.reason);
      nary_csp_census_emitted_ = true;
    }
    producer_effort.push_back(std::move(effort));
    if (offered.status == NaryCspPrefixActionOfferStatus::Interrupted ||
        offered.status == NaryCspPrefixActionOfferStatus::ResourceExhausted) {
      nary_csp_discovery_.reconfiguration_required = true;
      return false;
    }
    if (offered.status != NaryCspPrefixActionOfferStatus::Ready) {
      nary_csp_discovery_.terminal = true;
      return false;
    }
    const bool cursor_advanced =
        offered.have_resume_after &&
        (!nary_csp_discovery_.have_after ||
         !(offered.resume_after == nary_csp_discovery_.after));
    nary_csp_discovery_.offers = offered.offers;
    nary_csp_discovery_.complete = offered.complete;
    nary_csp_discovery_.have_after = offered.have_resume_after;
    if (offered.have_resume_after)
      nary_csp_discovery_.after = offered.resume_after;
    if (offered.complete && offered.offers.empty())
      nary_csp_discovery_.terminal = true;
    else if (offered.offers.empty() && !cursor_advanced)
      nary_csp_discovery_.terminal = true;
    return !nary_csp_discovery_.offers.empty();
  };

  const auto discover_nary_equality = [&]() -> bool {
    if (!config.nary_csp_equality_action_producer.enabled ||
        has_static_offer("NaryCspEquality") ||
        !k.nary_csp_prefix_action_session || !k.nary_csp_equality_session)
      return false;
    if (!config.nary_csp_equality_action_producer.valid()) {
      error = "n-ary CSP equality producer window is invalid";
      return false;
    }
    const auto &prefix = *k.nary_csp_prefix_action_session;
    const auto &equality = *k.nary_csp_equality_session;
    const auto profile = nary_equality_profile();
    const auto state = state_version(k);
    const bool owner_changed =
        nary_csp_equality_discovery_.owner != &k ||
        nary_csp_equality_discovery_.prefix_session != &prefix ||
        nary_csp_equality_discovery_.equality_session != &equality;
    const bool profile_changed =
        nary_csp_equality_discovery_.profile != profile;
    const bool relevant_change =
        owner_changed ||
        !same_version(nary_csp_equality_discovery_.state, state) ||
        nary_csp_equality_discovery_.prefix_owners != prefix.retained_owners() ||
        nary_csp_equality_discovery_.equality_keys != equality.keys().size();
    if (relevant_change ||
        (profile_changed &&
         (!nary_csp_equality_discovery_.reconfiguration_required ||
          !nary_csp_equality_discovery_.pending))) {
      nary_csp_equality_discovery_ = {};
      nary_csp_equality_discovery_.owner = &k;
      nary_csp_equality_discovery_.prefix_session = &prefix;
      nary_csp_equality_discovery_.equality_session = &equality;
      nary_csp_equality_discovery_.state = state;
      nary_csp_equality_discovery_.prefix_owners = prefix.retained_owners();
      nary_csp_equality_discovery_.equality_keys = equality.keys().size();
      nary_csp_equality_discovery_.profile = profile;
    } else if (nary_csp_equality_discovery_.reconfiguration_required &&
               profile_changed) {
      nary_csp_equality_discovery_.state = state;
      nary_csp_equality_discovery_.prefix_owners = prefix.retained_owners();
      nary_csp_equality_discovery_.equality_keys = equality.keys().size();
      nary_csp_equality_discovery_.profile = profile;
      nary_csp_equality_discovery_.reconfiguration_required = false;
    }
    if (nary_csp_equality_discovery_.reconfiguration_required) return false;
    if (nary_csp_equality_discovery_.pending ||
        !nary_csp_equality_discovery_.offers.empty())
      return true;
    if (nary_csp_equality_discovery_.terminal) return false;
    const auto &producer = config.nary_csp_equality_action_producer;
    NaryCspEqualityActionOfferWindow window{
        producer.maximum_owner_candidates,
        producer.maximum_coordinate_candidates,
        producer.maximum_offers,
        producer.maximum_enumeration_work,
        nary_csp_equality_discovery_.have_after,
        nary_csp_equality_discovery_.after};
    const auto limits = nary_equality_limits();
    if (!limits.valid()) {
      error = "n-ary CSP equality producer parameters are invalid";
      return false;
    }
    const auto offered = nary_csp_equality_action_offer(
        k, prefix, equality, limits, window);
    producer_effort.push_back(
        {"NaryCspEquality", static_cast<uint8_t>(offered.status),
         offered.complete, 0, offered.effort.enumeration_work,
         offered.effort.enumeration_work,
         offered.effort.coordinate_candidates, offered.offers.size()});
    if (offered.status == NaryCspEqualityActionOfferStatus::Interrupted ||
        offered.status == NaryCspEqualityActionOfferStatus::ResourceExhausted) {
      nary_csp_equality_discovery_.reconfiguration_required = true;
      return false;
    }
    if (offered.status != NaryCspEqualityActionOfferStatus::Ready) {
      nary_csp_equality_discovery_.terminal = true;
      return false;
    }
    const bool cursor_advanced =
        offered.have_resume_after &&
        (!nary_csp_equality_discovery_.have_after ||
         !(offered.resume_after == nary_csp_equality_discovery_.after));
    nary_csp_equality_discovery_.offers = offered.offers;
    nary_csp_equality_discovery_.complete = offered.complete;
    nary_csp_equality_discovery_.have_after = offered.have_resume_after;
    if (offered.have_resume_after)
      nary_csp_equality_discovery_.after = offered.resume_after;
    if ((offered.complete && offered.offers.empty()) ||
        (offered.offers.empty() && !cursor_advanced))
      nary_csp_equality_discovery_.terminal = true;
    return !nary_csp_equality_discovery_.offers.empty();
  };

  const auto discover_nary_equality_amo = [&]() -> bool {
    if (!config.nary_csp_equality_amo_producer.enabled ||
        has_static_offer("NaryCspEqualityAmo") ||
        !k.nary_csp_prefix_action_session || !k.nary_csp_equality_session ||
        !k.nary_csp_equality_amo_session)
      return false;
    if (!config.nary_csp_equality_amo_producer.valid()) {
      error = "n-ary CSP equality AMO producer window is invalid";
      return false;
    }
    const auto &prefix = *k.nary_csp_prefix_action_session;
    const auto &equality = *k.nary_csp_equality_session;
    const auto &amo = *k.nary_csp_equality_amo_session;
    const auto profile = nary_equality_amo_profile();
    const auto state = state_version(k);
    const bool relevant_change =
        nary_csp_equality_amo_discovery_.owner != &k ||
        nary_csp_equality_amo_discovery_.prefix_session != &prefix ||
        nary_csp_equality_amo_discovery_.equality_session != &equality ||
        nary_csp_equality_amo_discovery_.amo_session != &amo ||
        !same_version(nary_csp_equality_amo_discovery_.state, state) ||
        nary_csp_equality_amo_discovery_.prefix_owners !=
            prefix.retained_owners() ||
        nary_csp_equality_amo_discovery_.equality_keys !=
            equality.keys().size() ||
        nary_csp_equality_amo_discovery_.amo_keys != amo.keys().size() ||
        nary_csp_equality_amo_discovery_.profile != profile;
    if (relevant_change) {
      nary_csp_equality_amo_discovery_ = {};
      nary_csp_equality_amo_discovery_.owner = &k;
      nary_csp_equality_amo_discovery_.prefix_session = &prefix;
      nary_csp_equality_amo_discovery_.equality_session = &equality;
      nary_csp_equality_amo_discovery_.amo_session = &amo;
      nary_csp_equality_amo_discovery_.state = state;
      nary_csp_equality_amo_discovery_.prefix_owners = prefix.retained_owners();
      nary_csp_equality_amo_discovery_.equality_keys = equality.keys().size();
      nary_csp_equality_amo_discovery_.amo_keys = amo.keys().size();
      nary_csp_equality_amo_discovery_.profile = profile;
    }
    if (nary_csp_equality_amo_discovery_.pending ||
        !nary_csp_equality_amo_discovery_.offers.empty())
      return true;
    if (nary_csp_equality_amo_discovery_.terminal) return false;
    const auto &producer = config.nary_csp_equality_amo_producer;
    const NaryCspEqualityAmoOfferWindow window{
        producer.maximum_owner_candidates, producer.maximum_offers,
        producer.maximum_enumeration_work,
        nary_csp_equality_amo_discovery_.have_after,
        nary_csp_equality_amo_discovery_.after};
    const auto limits = nary_equality_amo_limits();
    if (!limits.valid()) {
      error = "n-ary CSP equality AMO producer parameters are invalid";
      return false;
    }
    const auto offered = nary_csp_equality_amo_offer(
        k, prefix, equality, amo, limits, window);
    producer_effort.push_back(
        {"NaryCspEqualityAmo", static_cast<uint8_t>(offered.status),
         offered.complete, 0, offered.enumeration_work,
         offered.enumeration_work, offered.owner_candidates,
         offered.offers.size()});
    if (offered.status != NaryCspEqualityAmoOfferStatus::Ready) {
      nary_csp_equality_amo_discovery_.terminal = true;
      return false;
    }
    const bool cursor_advanced =
        offered.have_resume_after &&
        (!nary_csp_equality_amo_discovery_.have_after ||
         !(offered.resume_after == nary_csp_equality_amo_discovery_.after));
    nary_csp_equality_amo_discovery_.offers = offered.offers;
    nary_csp_equality_amo_discovery_.complete = offered.complete;
    nary_csp_equality_amo_discovery_.have_after = offered.have_resume_after;
    if (offered.have_resume_after)
      nary_csp_equality_amo_discovery_.after = offered.resume_after;
    if ((offered.complete && offered.offers.empty()) ||
        (offered.offers.empty() && !cursor_advanced))
      nary_csp_equality_amo_discovery_.terminal = true;
    return !nary_csp_equality_amo_discovery_.offers.empty();
  };

  const auto discover_gaussian = [&]() -> bool {
    if (!config.gf3_gaussian_action_producer.enabled ||
        has_static_offer("Gf3GaussianConsequence") ||
        !k.gf3_gaussian_action_session)
      return false;
    const auto &session = *k.gf3_gaussian_action_session;
    const auto profile = gaussian_profile();
    const auto state = state_version(k);
    const bool owner_changed = gf3_gaussian_discovery_.owner != &k ||
                               gf3_gaussian_discovery_.session != &session;
    const bool profile_changed =
        gf3_gaussian_discovery_.profile != profile;
    const bool state_or_session_changed =
        owner_changed ||
        !same_version(gf3_gaussian_discovery_.state, state) ||
        gf3_gaussian_discovery_.session_keys != session.keys().size();
    if (state_or_session_changed ||
        (profile_changed &&
         (!gf3_gaussian_discovery_.reconfiguration_required ||
          !gf3_gaussian_discovery_.pending))) {
      gf3_gaussian_discovery_ = {};
      gf3_gaussian_discovery_.owner = &k;
      gf3_gaussian_discovery_.session = &session;
      gf3_gaussian_discovery_.state = state;
      gf3_gaussian_discovery_.session_keys = session.keys().size();
      gf3_gaussian_discovery_.profile = profile;
    } else if (gf3_gaussian_discovery_.reconfiguration_required &&
               profile_changed) {
      gf3_gaussian_discovery_.state = state;
      gf3_gaussian_discovery_.session_keys = session.keys().size();
      gf3_gaussian_discovery_.profile = profile;
      gf3_gaussian_discovery_.reconfiguration_required = false;
    }
    if (gf3_gaussian_discovery_.reconfiguration_required) return false;
    if (gf3_gaussian_discovery_.pending || gf3_gaussian_discovery_.offer)
      return true;
    if (gf3_gaussian_discovery_.terminal) return false;
    const auto limits = gaussian_limits();
    if (!limits.valid()) {
      error = "GF3 Gaussian action producer parameters are invalid";
      return false;
    }
    const auto offered = gf3_gaussian_action_offer(k, session, limits);
    uint64_t preparation = offered.live.effort.capture_work;
    preparation = saturated_add(preparation, offered.live.effort.bridge_work);
    preparation = saturated_add(preparation,
                                offered.live.effort.certificate_work);
    preparation = saturated_add(preparation,
                                offered.live.effort.validation_work);
    preparation = saturated_add(preparation,
                                offered.live.effort.revalidation_work);
    preparation = saturated_add(preparation, offered.live.effort.journal_work);
    preparation = saturated_add(preparation, offered.live.effort.identity_work);
    preparation = saturated_add(preparation, offered.session_work);
    producer_effort.push_back(
        {"Gf3GaussianConsequence", static_cast<uint8_t>(offered.status),
         offered.complete, preparation, 0, preparation, offered.candidates,
         offered.offers.size()});
    if (offered.status == Gf3GaussianActionStatus::Interrupted ||
        offered.status == Gf3GaussianActionStatus::ResourceExhausted) {
      gf3_gaussian_discovery_.reconfiguration_required = true;
      return false;
    }
    if (offered.status != Gf3GaussianActionStatus::Applied ||
        offered.offers.size() != 1) {
      gf3_gaussian_discovery_.terminal = true;
      return false;
    }
    gf3_gaussian_discovery_.offer = offered.offers.front();
    return true;
  };

  const auto discover_general_row = [&]() -> bool {
    if (!config.gf3_general_row_action_producer.enabled ||
        has_static_offer("Gf3GeneralRowConsequence") ||
        !k.gf3_general_row_action_session)
      return false;
    if (!config.gf3_general_row_action_producer.valid()) {
      error = "GF3 general-row action producer parameters are invalid";
      return false;
    }
    const auto &session = *k.gf3_general_row_action_session;
    const auto profile = general_row_profile();
    const auto state = state_version(k);
    const bool changed = gf3_general_row_discovery_.owner != &k ||
                         gf3_general_row_discovery_.session != &session ||
                         !same_version(gf3_general_row_discovery_.state, state) ||
                         gf3_general_row_discovery_.session_keys !=
                             session.keys().size();
    const bool profile_changed =
        gf3_general_row_discovery_.profile != profile;
    if (changed ||
        (profile_changed &&
         (!gf3_general_row_discovery_.reconfiguration_required ||
          !gf3_general_row_discovery_.pending))) {
      gf3_general_row_discovery_ = {};
      gf3_general_row_discovery_.owner = &k;
      gf3_general_row_discovery_.session = &session;
      gf3_general_row_discovery_.state = state;
      gf3_general_row_discovery_.session_keys = session.keys().size();
      gf3_general_row_discovery_.profile = profile;
    } else if (gf3_general_row_discovery_.reconfiguration_required &&
               profile_changed) {
      gf3_general_row_discovery_.state = state;
      gf3_general_row_discovery_.session_keys = session.keys().size();
      gf3_general_row_discovery_.profile = profile;
      gf3_general_row_discovery_.reconfiguration_required = false;
    }
    if (gf3_general_row_discovery_.reconfiguration_required) return false;
    if (gf3_general_row_discovery_.pending ||
        !gf3_general_row_discovery_.offers.empty())
      return true;
    if (gf3_general_row_discovery_.terminal) return false;
    const auto limits = general_row_limits();
    if (!limits.valid()) {
      error = "GF3 general-row action limits are invalid";
      return false;
    }
    const auto &producer = config.gf3_general_row_action_producer;
    Gf3GeneralRowActionOfferWindow window{
        producer.maximum_candidates, producer.maximum_offers,
        producer.maximum_enumeration_work,
        gf3_general_row_discovery_.have_after,
        gf3_general_row_discovery_.after};
    const auto offered =
        gf3_general_row_action_offer(k, session, limits, window);
    uint64_t preparation = offered.live_effort.capture_work;
    preparation = saturated_add(preparation, offered.live_effort.bridge_work);
    preparation = saturated_add(preparation, offered.live_effort.planning_work);
    preparation = saturated_add(preparation,
                                offered.live_effort.validation_work);
    preparation = saturated_add(preparation,
                                offered.live_effort.duplicate_scan_work);
    preparation = saturated_add(preparation,
                                offered.live_effort.revalidation_work);
    preparation = saturated_add(preparation, offered.enumeration_work);
    producer_effort.push_back(
        {"Gf3GeneralRowConsequence", static_cast<uint8_t>(offered.status),
         offered.complete, preparation, 0, preparation, offered.candidates,
         offered.offers.size()});
    if (offered.status == Gf3GeneralRowActionStatus::Interrupted ||
        offered.status == Gf3GeneralRowActionStatus::ResourceExhausted) {
      gf3_general_row_discovery_.reconfiguration_required = true;
      return false;
    }
    if (offered.status != Gf3GeneralRowActionStatus::Applied) {
      gf3_general_row_discovery_.terminal = true;
      return false;
    }
    const bool cursor_advanced = offered.have_resume_after &&
        (!gf3_general_row_discovery_.have_after ||
         !(offered.resume_after == gf3_general_row_discovery_.after));
    gf3_general_row_discovery_.offers = offered.offers;
    gf3_general_row_discovery_.complete = offered.complete;
    gf3_general_row_discovery_.have_after = offered.have_resume_after;
    if (offered.have_resume_after)
      gf3_general_row_discovery_.after = offered.resume_after;
    if ((offered.complete && offered.offers.empty()) ||
        (offered.offers.empty() && !cursor_advanced))
      gf3_general_row_discovery_.terminal = true;
    return !gf3_general_row_discovery_.offers.empty();
  };

  const auto discover_tseitin = [&]() -> bool {
    if (!config.tseitin_cycle_action_producer.enabled ||
        has_static_offer("TseitinCycleBreaking") ||
        !k.tseitin_cycle_breaking_session)
      return false;
    const auto &session = *k.tseitin_cycle_breaking_session;
    const auto profile = tseitin_profile();
    const auto state = state_version(k);
    const bool owner_changed = tseitin_cycle_discovery_.owner != &k ||
                               tseitin_cycle_discovery_.session != &session;
    const bool profile_changed =
        tseitin_cycle_discovery_.profile != profile;
    const bool state_or_session_changed =
        owner_changed ||
        !same_version(tseitin_cycle_discovery_.state, state) ||
        tseitin_cycle_discovery_.session_keys != session.keys().size();
    if (state_or_session_changed ||
        (profile_changed &&
         (!tseitin_cycle_discovery_.reconfiguration_required ||
          !tseitin_cycle_discovery_.pending))) {
      tseitin_cycle_discovery_ = {};
      tseitin_cycle_discovery_.owner = &k;
      tseitin_cycle_discovery_.session = &session;
      tseitin_cycle_discovery_.state = state;
      tseitin_cycle_discovery_.session_keys = session.keys().size();
      tseitin_cycle_discovery_.profile = profile;
    } else if (tseitin_cycle_discovery_.reconfiguration_required &&
               profile_changed) {
      tseitin_cycle_discovery_.state = state;
      tseitin_cycle_discovery_.session_keys = session.keys().size();
      tseitin_cycle_discovery_.profile = profile;
      tseitin_cycle_discovery_.reconfiguration_required = false;
    }
    if (tseitin_cycle_discovery_.reconfiguration_required) return false;
    if (tseitin_cycle_discovery_.pending ||
        !tseitin_cycle_discovery_.offers.empty())
      return true;
    if (tseitin_cycle_discovery_.terminal) return false;
    const auto limits = tseitin_limits();
    if (!limits.valid()) {
      error = "Tseitin cycle action producer parameters are invalid";
      return false;
    }
    const auto offered =
        tseitin_cycle_breaking_offer(k, session, limits);
    const auto &e = offered.effort;
    uint64_t preparation = e.snapshot_work;
    preparation = saturated_add(preparation, e.recapture_work);
    preparation = saturated_add(preparation, e.admission_work);
    preparation = saturated_add(preparation, e.planning_work);
    preparation = saturated_add(preparation, e.transaction_work);
    preparation = saturated_add(preparation, e.validation_ticks);
    preparation = saturated_add(preparation, e.identity_work);
    preparation = saturated_add(preparation, e.session_work);
    const bool complete =
        offered.status == TseitinCycleBreakingStatus::Applied ||
        offered.status == TseitinCycleBreakingStatus::AlreadyApplied ||
        offered.status == TseitinCycleBreakingStatus::NoStructure;
    producer_effort.push_back(
        {"TseitinCycleBreaking", static_cast<uint8_t>(offered.status),
         complete, preparation, e.discovery_work,
         saturated_add(preparation, e.discovery_work), e.candidates,
         offered.offers.size()});
    if (offered.status == TseitinCycleBreakingStatus::Interrupted) {
      tseitin_cycle_discovery_.reconfiguration_required = true;
      error = "controller selection interrupted";
      return false;
    }
    if (offered.status == TseitinCycleBreakingStatus::ResourceExhausted) {
      tseitin_cycle_discovery_.reconfiguration_required = true;
      return false;
    }
    if (offered.status != TseitinCycleBreakingStatus::Applied ||
        offered.offers.empty()) {
      tseitin_cycle_discovery_.terminal = true;
      return false;
    }
    tseitin_cycle_discovery_.offers = offered.offers;
    return true;
  };

  const auto discover_guarded_cover = [&]() -> bool {
    const auto &producer = config.guarded_cover_producer;
    auto &d = guarded_cover_discovery_;
    if (!producer.enabled || has_static_offer("DirectCspSupport") ||
        !k.direct_csp_guarded_cover_action_session) return false;
    const auto *actions = k.direct_csp_guarded_cover_action_session;
    if (d.owner != &k || d.actions != actions || d.revision != producer.revision) {
      d = {};
      d.owner = &k; d.actions = actions; d.revision = producer.revision;
      d.window = producer.window;
    }
    if (d.reconfiguration_required) return false;
    // A new formula/session image invalidates a resume cursor, never the
    // durable committed keys authenticated by the offer module.
    if (d.window.stamp.kernel &&
        (d.window.stamp.arena_generation != k.arena_generation ||
         d.window.stamp.touch_epoch != k.touch_epoch ||
         d.window.stamp.strategy_epoch != k.strategy_epoch ||
         d.window.stamp.arena_words != k.arena.size() ||
         d.window.stamp.next_clause_id != k.next_clause_id ||
         d.window.stamp.variables != k.nvars ||
         d.window.stamp.external_extent != k.external_to_internal.size() ||
         d.session_keys != actions->committed_keys())) {
      d.window.after_cover = 0;
      d.window.stamp = {};
      d.pending.reset();
    }
    if (d.pending) return true;
    const auto result = direct_csp_guarded_cover_offer(k, *actions, d.session,
                                                     producer.limits, d.window);
    producer_effort.push_back({"DirectCspSupport", static_cast<uint8_t>(result.status),
        false, 0, result.effort.work, result.effort.work,
        result.effort.cover_candidates, result.offers.size()});
    d.session_keys = actions->committed_keys();
    if (result.status == DirectCspGuardedCoverOfferStatus::Interrupted ||
        result.status == DirectCspGuardedCoverOfferStatus::ResourceExhausted) {
      d.reconfiguration_required = true; return false;
    }
    if (result.status != DirectCspGuardedCoverOfferStatus::Offered &&
        result.status != DirectCspGuardedCoverOfferStatus::NoOffer) return false;
    d.window.stamp = result.stamp;
    // NoOffer is merely a bounded local page. A complete page starts over;
    // no global terminal/exhaustion state is inferred.
    d.window.after_cover = result.cover_window_complete ? 0 : result.after_cover;
    if (result.offers.empty()) return false;
    // Consume one request at a time; revisit this page after publication so
    // durable keys filter consumed tuples without losing other offers.
    d.window.after_cover = 0;
    d.pending = result.offers.front().selection;
    return true;
  };

  enum class NativeOffer : uint8_t {
    None,
    AffineBasis,
    Gf3,
    DirectCsp,
    GuardedCover,
    NaryCsp,
    NaryCspEquality,
    NaryCspEqualityAmo,
    Gf3Gaussian,
    Gf3GeneralRow,
    TseitinCycle
  };
  struct Eligible {
    const RuleVTable *rule = nullptr;
    NativeOffer native = NativeOffer::None;
  };
  const bool startup_gate = !config.startup_prefix.sequence.empty() &&
                            !startup_resume_forced_;
  std::optional<std::string_view> startup_native_family;
  if (startup_gate &&
      startup_prefix_position_ < config.startup_prefix.sequence.size()) {
    const auto &step =
        config.startup_prefix.sequence[startup_prefix_position_];
    if (step.granularity == InvocationGranularity::NativeProducerOffer)
      startup_native_family = step.rule;
  }
  const auto native_allowed = [&](std::string_view family) {
    return !startup_gate ||
           (startup_native_family && *startup_native_family == family);
  };
  const uint64_t direct_batch =
      config.direct_csp_action_producer.startup_batch_actions;
  const bool direct_propagation_due =
      direct_batch && observation.last_action &&
      observation.last_action->invocation.rule_id ==
          static_cast<uint32_t>(tactix_rule_id("DirectCspSupport")) &&
      observation.last_action->invocation.made_rule_progress;
  const bool direct_advice_due =
      direct_batch && observation.last_action &&
      observation.last_action->invocation.rule_id ==
          static_cast<uint32_t>(tactix_rule_id("Propagate")) &&
      observation.last_action->invocation.status == InvocationStatus::NoChange &&
      !observation.last_action->invocation.made_rule_progress;
  std::vector<Eligible> eligible;
  for (const auto &entry : controller_action_availability(k, config)) {
    const bool exact_startup_offer =
        startup_gate &&
        startup_prefix_position_ < config.startup_prefix.sequence.size() &&
        config.startup_prefix.sequence[startup_prefix_position_].granularity !=
            InvocationGranularity::NativeProducerOffer &&
        config.startup_prefix.sequence[startup_prefix_position_].rule ==
            entry.rule;
    if (entry.eligible) {
      eligible.push_back(
          {&tactix_rules()[tactix_rule_id(entry.rule.c_str())], NativeOffer::None});
    } else if (entry.reason == "structured-witness-not-offered" &&
               exact_startup_offer) {
      eligible.push_back(
          {&tactix_rules()[tactix_rule_id(entry.rule.c_str())], NativeOffer::None});
    } else if (entry.reason == "guard-false" &&
               entry.rule == "Propagate" && direct_propagation_due) {
      eligible.push_back(
          {&tactix_rules()[tactix_rule_id(entry.rule.c_str())], NativeOffer::None});
    } else if (entry.reason == "structured-witness-not-offered" &&
               entry.rule == "AffineConsequence" &&
               native_allowed(entry.rule) && discover_affine()) {
      eligible.push_back({&tactix_rules()[tactix_rule_id(entry.rule.c_str())],
                          NativeOffer::AffineBasis});
    } else if (entry.reason == "structured-witness-not-offered" &&
               entry.rule == "Gf3AffineConsequence" &&
               native_allowed(entry.rule) && discover_gf3()) {
      eligible.push_back(
          {&tactix_rules()[tactix_rule_id(entry.rule.c_str())], NativeOffer::Gf3});
    } else if (entry.reason == "structured-witness-not-offered" &&
               entry.rule == "DirectCspSupport" &&
               native_allowed(entry.rule) && discover_guarded_cover()) {
      eligible.push_back({&tactix_rules()[tactix_rule_id(entry.rule.c_str())],
                          NativeOffer::GuardedCover});
    } else if (entry.reason == "structured-witness-not-offered" &&
               entry.rule == "DirectCspSupport" &&
               native_allowed(entry.rule) && discover_direct()) {
      eligible.push_back({&tactix_rules()[tactix_rule_id(entry.rule.c_str())],
                          NativeOffer::DirectCsp});
    } else if (entry.reason == "structured-witness-not-offered" &&
               entry.rule == "NaryCspPrefix" &&
               native_allowed(entry.rule) && discover_nary()) {
      eligible.push_back({&tactix_rules()[tactix_rule_id(entry.rule.c_str())],
                          NativeOffer::NaryCsp});
    } else if (entry.reason == "structured-witness-not-offered" &&
               entry.rule == "NaryCspEquality" &&
               native_allowed(entry.rule) &&
               discover_nary_equality()) {
      eligible.push_back({&tactix_rules()[tactix_rule_id(entry.rule.c_str())],
                          NativeOffer::NaryCspEquality});
    } else if (entry.reason == "structured-witness-not-offered" &&
               entry.rule == "NaryCspEqualityAmo" &&
               native_allowed(entry.rule) &&
               discover_nary_equality_amo()) {
      eligible.push_back({&tactix_rules()[tactix_rule_id(entry.rule.c_str())],
                          NativeOffer::NaryCspEqualityAmo});
    } else if (entry.reason == "structured-witness-not-offered" &&
               entry.rule == "Gf3GaussianConsequence" &&
               native_allowed(entry.rule) &&
               discover_gaussian()) {
      eligible.push_back({&tactix_rules()[tactix_rule_id(entry.rule.c_str())],
                          NativeOffer::Gf3Gaussian});
    } else if (entry.reason == "structured-witness-not-offered" &&
               entry.rule == "Gf3GeneralRowConsequence" &&
               native_allowed(entry.rule) && discover_general_row()) {
      eligible.push_back({&tactix_rules()[tactix_rule_id(entry.rule.c_str())],
                          NativeOffer::Gf3GeneralRow});
    } else if (entry.reason == "structured-witness-not-offered" &&
               entry.rule == "TseitinCycleBreaking" &&
               native_allowed(entry.rule) && discover_tseitin()) {
      eligible.push_back({&tactix_rules()[tactix_rule_id(entry.rule.c_str())],
                          NativeOffer::TseitinCycle});
    } else if (!error.empty()) {
      request = {};
      request.sequence = observation.sequence + 1;
      request.producer_effort = std::move(producer_effort);
      return false;
    }
  }
  const bool can_resume = kernel_propagation_fixpoint(k);
  request = {};
  request.sequence = observation.sequence + 1;
  std::optional<size_t> forced_choice;
  bool startup_forced = false;
  const auto find_rule = [&](std::string_view name,
                             std::optional<NativeOffer> native = std::nullopt) {
    for (size_t i = 0; i < eligible.size(); ++i)
      if (std::string_view(eligible[i].rule->name) == name &&
          (!native || eligible[i].native == *native))
        return std::optional<size_t>{i};
    return std::optional<size_t>{};
  };
  if (!config.startup_prefix.sequence.empty() && !startup_resume_forced_) {
    if (startup_prefix_position_ < config.startup_prefix.sequence.size()) {
      const auto &step = config.startup_prefix.sequence[startup_prefix_position_];
      if (step.granularity == InvocationGranularity::NativeProducerOffer) {
        for (size_t i = 0; i < eligible.size(); ++i)
          if (std::string_view(eligible[i].rule->name) == step.rule &&
              eligible[i].native != NativeOffer::None) {
            forced_choice = i;
            break;
          }
      } else {
        forced_choice = find_rule(step.rule, NativeOffer::None);
      }
      if (!forced_choice) {
        request.producer_effort = std::move(producer_effort);
        error = "required controller startup prefix action is unavailable: " +
                step.rule;
        return false;
      }
      startup_forced = true;
    } else {
      if (!can_resume) {
        request.producer_effort = std::move(producer_effort);
        error = "controller startup prefix completed away from a propagation fixpoint";
        return false;
      }
      forced_choice = eligible.size();
    }
  }
  if (!forced_choice && eligible.empty() && !can_resume) {
    request.producer_effort = std::move(producer_effort);
    error = "no eligible controller action";
    return false;
  }
  const uint64_t affine_batch =
      config.affine_basis_action_producer.startup_batch_actions;
  if (!forced_choice && affine_batch && k.affine_basis_action_session) {
    if (!can_resume) {
      forced_choice = find_rule("Propagate");
    } else if (k.affine_basis_action_session->committed_keys() < affine_batch) {
      forced_choice = find_rule("AffineConsequence", NativeOffer::AffineBasis);
    }
    if (!forced_choice)
      forced_choice = find_rule("SemanticVariableAdvice");
  }
  if (!forced_choice && direct_batch && k.direct_csp_support_action_session) {
    if (direct_propagation_due) {
      forced_choice = find_rule("Propagate");
      if (!forced_choice) {
        error = "DirectCSP startup batch requires explicit propagation";
        return false;
      }
    } else if (!can_resume) {
      forced_choice = find_rule("Propagate");
      if (!forced_choice) {
        error = "DirectCSP startup batch requires propagation";
        return false;
      }
    } else if (direct_advice_due) {
      forced_choice = find_rule("SemanticVariableAdvice");
      if (!forced_choice) {
        error = "DirectCSP startup batch requires semantic variable advice";
        return false;
      }
    } else if (k.direct_csp_support_action_session->committed_keys() <
               direct_batch) {
      forced_choice = find_rule("DirectCspSupport", NativeOffer::DirectCsp);
      if (!forced_choice) {
        error = "DirectCSP startup batch has no eligible support action";
        return false;
      }
    }
  }
  const auto choice = forced_choice ? *forced_choice
                                    : draw(eligible.size() + can_resume);
  if (choice == eligible.size()) {
    request.type = ActionType::ResumeSearch;
    const auto configurations = config.search_configurations.empty()
                                    ? controller::search_configurations()
                                    : config.search_configurations;
    if (configurations.empty() ||
        std::any_of(configurations.begin(), configurations.end(),
                    [](const auto selected) { return !valid(selected); })) {
      error = "invalid controller search configuration collection";
      return false;
    }
    request.search = configurations[draw(configurations.size())];
    if (!config.startup_prefix.sequence.empty() && !startup_resume_forced_ &&
        startup_prefix_position_ == config.startup_prefix.sequence.size())
      startup_resume_pending_sequence_ = request.sequence;
  } else {
    const auto selected_action = eligible[choice];
    const auto &rule = *selected_action.rule;
    const OnlineInvocation *startup_step =
        startup_forced
            ? &config.startup_prefix.sequence[startup_prefix_position_]
            : nullptr;
    const auto offered = config.offered_actions.find(rule.name);
    if (startup_step &&
        startup_step->granularity !=
            InvocationGranularity::NativeProducerOffer) {
      request.invocation = *startup_step;
    } else if (offered != config.offered_actions.end() &&
               !offered->second.empty()) {
      request.invocation = offered->second[draw(offered->second.size())];
      if (request.invocation.rule != rule.name) {
        error = "offered invocation identity differs from its action family";
        return false;
      }
    } else if (selected_action.native == NativeOffer::AffineBasis) {
      if (!affine_basis_discovery_.pending)
        affine_basis_discovery_.pending = affine_basis_discovery_.offers[
            startup_forced ? 0 : draw(affine_basis_discovery_.offers.size())];
      affine_basis_discovery_.pending_sequence = request.sequence;
      request.invocation.id = request.sequence;
      request.invocation.rule = rule.name;
      request.invocation.parameters =
          config.parameters.affine_consequence_reference;
      request.invocation.granularity = InvocationGranularity::OneInstance;
      request.invocation.affine_basis_instances = {
          *affine_basis_discovery_.pending};
    } else if (selected_action.native == NativeOffer::Gf3) {
      if (!gf3_discovery_.pending)
        gf3_discovery_.pending =
            gf3_discovery_.offers[startup_forced
                                      ? 0
                                      : draw(gf3_discovery_.offers.size())];
      const auto key = *gf3_discovery_.pending;
      gf3_discovery_.pending_sequence = request.sequence;
      request.invocation.rule = rule.name;
      request.invocation.parameters =
          config.parameters.gf3_affine_consequence_reference;
      request.invocation.granularity = InvocationGranularity::OneInstance;
      request.invocation.gf3_affine_instances = {
          {key.first_equation, key.second_equation}};
    } else if (selected_action.native == NativeOffer::GuardedCover) {
      request.invocation = config.guarded_cover_producer.profile;
      request.invocation.id = request.sequence;
      request.invocation.granularity = InvocationGranularity::OneInstance;
      request.invocation.direct_csp_guarded_cover_instances = {*guarded_cover_discovery_.pending};
      guarded_cover_discovery_.pending_sequence = request.sequence;
    } else if (selected_action.native == NativeOffer::DirectCsp) {
      if (!direct_csp_discovery_.pending)
        direct_csp_discovery_.pending = direct_csp_discovery_.offers[
            startup_forced ? 0 : draw(direct_csp_discovery_.offers.size())];
      const auto exact = *direct_csp_discovery_.pending;
      direct_csp_discovery_.pending_sequence = request.sequence;
      request.invocation.rule = rule.name;
      request.invocation.parameters =
          config.parameters.direct_csp_support_reference;
      request.invocation.granularity = InvocationGranularity::OneInstance;
      request.invocation.direct_csp_support_instances = {exact};
    } else if (selected_action.native == NativeOffer::NaryCsp) {
      if (!nary_csp_discovery_.pending)
        nary_csp_discovery_.pending = nary_csp_discovery_.offers[
            startup_forced ? 0 : draw(nary_csp_discovery_.offers.size())];
      const auto exact = *nary_csp_discovery_.pending;
      nary_csp_discovery_.pending_sequence = request.sequence;
      request.invocation.rule = rule.name;
      request.invocation.parameters =
          config.parameters.nary_csp_prefix_reference;
      request.invocation.granularity = InvocationGranularity::OneInstance;
      request.invocation.nary_csp_prefix_instances = {exact};
    } else if (selected_action.native == NativeOffer::NaryCspEquality) {
      if (!nary_csp_equality_discovery_.pending)
        nary_csp_equality_discovery_.pending =
            nary_csp_equality_discovery_.offers[
                startup_forced
                    ? 0
                    : draw(nary_csp_equality_discovery_.offers.size())];
      const auto exact = *nary_csp_equality_discovery_.pending;
      nary_csp_equality_discovery_.pending_sequence = request.sequence;
      request.invocation.rule = rule.name;
      request.invocation.parameters =
          config.parameters.nary_csp_equality_reference;
      request.invocation.granularity = InvocationGranularity::OneInstance;
      request.invocation.nary_csp_equality_instances = {exact};
    } else if (selected_action.native == NativeOffer::NaryCspEqualityAmo) {
      if (!nary_csp_equality_amo_discovery_.pending)
        nary_csp_equality_amo_discovery_.pending =
            nary_csp_equality_amo_discovery_.offers[
                startup_forced
                    ? 0
                    : draw(nary_csp_equality_amo_discovery_.offers.size())];
      const auto exact = *nary_csp_equality_amo_discovery_.pending;
      nary_csp_equality_amo_discovery_.pending_sequence = request.sequence;
      request.invocation.rule = rule.name;
      request.invocation.parameters =
          config.parameters.nary_csp_equality_amo_reference;
      request.invocation.granularity = InvocationGranularity::OneInstance;
      request.invocation.nary_csp_equality_amo_instances = {exact};
    } else if (selected_action.native == NativeOffer::Gf3Gaussian) {
      if (!gf3_gaussian_discovery_.pending)
        gf3_gaussian_discovery_.pending = gf3_gaussian_discovery_.offer;
      gf3_gaussian_discovery_.pending_sequence = request.sequence;
      request.invocation.rule = rule.name;
      request.invocation.parameters =
          config.parameters.gf3_gaussian_consequence_reference;
      request.invocation.granularity = InvocationGranularity::OneInstance;
      request.invocation.gf3_gaussian_instances = {
          *gf3_gaussian_discovery_.pending};
    } else if (selected_action.native == NativeOffer::Gf3GeneralRow) {
      if (!gf3_general_row_discovery_.pending)
        gf3_general_row_discovery_.pending =
            gf3_general_row_discovery_.offers[
                draw(gf3_general_row_discovery_.offers.size())];
      gf3_general_row_discovery_.pending_sequence = request.sequence;
      request.invocation.rule = rule.name;
      request.invocation.parameters =
          config.parameters.gf3_general_row_consequence_reference;
      request.invocation.granularity = InvocationGranularity::OneInstance;
      request.invocation.post_application = PostApplication::Propagate;
      request.invocation.gf3_general_row_instances = {
          *gf3_general_row_discovery_.pending};
    } else if (selected_action.native == NativeOffer::TseitinCycle) {
      if (!tseitin_cycle_discovery_.pending)
        tseitin_cycle_discovery_.pending = tseitin_cycle_discovery_.offers[
            startup_forced ? 0
                           : draw(tseitin_cycle_discovery_.offers.size())];
      tseitin_cycle_discovery_.pending_sequence = request.sequence;
      request.invocation.rule = rule.name;
      request.invocation.parameters =
          config.parameters.tseitin_cycle_breaking_reference;
      request.invocation.granularity = InvocationGranularity::OneInstance;
      request.invocation.tseitin_cycle_instances = {
          *tseitin_cycle_discovery_.pending};
    } else {
      request.invocation.rule = rule.name;
      request.invocation.parameters = reference_parameters(k, rule.name, config.parameters);
      request.invocation.granularity = InvocationGranularity::CanonicalApplication;
      // Native callback windows are caller-bounded. The public typed surface
      // still accepts any explicit legal identity or structured payload.
      const auto enumerate = rule.semantic_instances ? rule.semantic_instances : rule.instances;
      if (enumerate && config.parameters.maximum_selected_batch && draw(2)) {
        // Lucky's finite constructor catalog is not a prefix of a potentially
        // huge semantic candidate space. Keep every constructor selectable.
        const auto cap = std::string_view(rule.name) == "Lucky"
            ? LUCKY_ACTIVE_INSTANCE_COUNT
            : config.parameters.maximum_selected_batch;
        std::vector<uint64_t> instances(cap);
        const auto count = enumerate(k, instances.data(), cap);
        if (count > cap) {
          error = "native instance producer exceeded its caller window";
          return false;
        }
        if (count) {
          const auto selected = instances[draw(count)];
          // Native callbacks may advertise the canonical whole-rule choice.
          // It has its own typed form, not an explicit semantic instance.
          if (selected != INSTANCE_CANONICAL) {
            request.invocation.granularity = InvocationGranularity::OneInstance;
            request.invocation.instances = {selected};
          }
        }
      }
      if (request.invocation.rule == "Lucky" &&
          request.invocation.instances == std::vector<uint64_t>{LUCKY_STARTUP_RAMSEY})
        request.invocation.parameters = config.parameters.ramsey_startup_reference;
      if (request.invocation.rule == "Lucky" &&
          request.invocation.instances == std::vector<uint64_t>{17})
        request.invocation.parameters =
            config.parameters.exact_cover_model_reference;
      if (request.invocation.rule == "Lucky" &&
          request.invocation.instances == std::vector<uint64_t>{18})
        request.invocation.parameters =
            config.parameters.independent_set_model_reference;
      if (request.invocation.rule == "Lucky" &&
          request.invocation.instances ==
              std::vector<uint64_t>{LUCKY_GRAPH_LABELING})
        request.invocation.parameters = config.parameters.graph_labeling_reference;
      if (request.invocation.rule == "Lucky" &&
          request.invocation.instances == std::vector<uint64_t>{15})
        request.invocation.parameters = config.parameters.stconn_proof_reference;
      if (request.invocation.rule == "Lucky" &&
          request.invocation.instances == std::vector<uint64_t>{16})
        request.invocation.parameters =
            config.parameters.signed_cardinality_proof_reference;
      if (request.invocation.rule == "Lucky" &&
          request.invocation.instances == std::vector<uint64_t>{10})
        request.invocation.parameters =
            config.parameters.balanced_signature_model_reference;
      if (request.invocation.rule == "Lucky" &&
          request.invocation.instances == std::vector<uint64_t>{6})
        request.invocation.parameters = config.parameters.parity_model_reference;
      if (request.invocation.rule == "Lucky" &&
          (request.invocation.instances == std::vector<uint64_t>{7} ||
           request.invocation.instances == std::vector<uint64_t>{9}))
        request.invocation.parameters = config.parameters.onehot_model_reference;
      if (request.invocation.rule == "Lucky" &&
          request.invocation.instances == std::vector<uint64_t>{8})
        request.invocation.parameters = config.parameters.affine_model_reference;
      if (request.invocation.rule == "Lucky" &&
          request.invocation.instances == std::vector<uint64_t>{11})
        request.invocation.parameters = config.parameters.fermat_model_reference;
      if (request.invocation.rule == "Lucky" &&
          request.invocation.instances == std::vector<uint64_t>{13})
        request.invocation.parameters =
            config.parameters.lookup_circuit_model_reference;
      if (request.invocation.rule == "Lucky" &&
          request.invocation.instances == std::vector<uint64_t>{14})
        request.invocation.parameters = config.parameters.simon_model_reference;
      if (request.invocation.rule == "Lucky" &&
          (request.invocation.instances ==
               std::vector<uint64_t>{LUCKY_STARTUP_PARITY} ||
           request.invocation.instances ==
               std::vector<uint64_t>{LUCKY_PARITY_PROOF}))
        request.invocation.parameters = config.parameters.parity_model_reference;
      if (request.invocation.rule == "Lucky" &&
          request.invocation.instances ==
              std::vector<uint64_t>{LUCKY_EXACT_ONE_PROOF})
        request.invocation.parameters = config.parameters.onehot_model_reference;
      if (request.invocation.rule == "Lucky" &&
          !request.invocation.instances.empty() &&
          (request.invocation.instances.front() <= LUCKY_BACKWARD_TRUE ||
           request.invocation.instances.front() == LUCKY_STARTUP_COUNT ||
           request.invocation.instances.front() == LUCKY_NOL_HYPERGRAPH))
        request.invocation.parameters = std::monostate{};
      // Guidance is a policy choice over the pure candidate snapshot. Bind
      // the exact publication so replacement cannot change a queued action.
      // Only legacy Walk permits the additional ordinary-phase (zero) choice;
      // full-CNF admission requires an exact nonzero candidate. With no live
      // candidates, keep its configured identity for an explicit stale decline.
      if (!observation.local_search_candidates.empty()) {
        if (request.invocation.rule == "Walk") {
          auto &parameters = std::get<WalkParameters>(request.invocation.parameters);
          const bool ordinary =
              parameters.objective == WalkParameters::Objective::LegacyWalk;
          const auto selected =
              draw(observation.local_search_candidates.size() + ordinary);
          parameters.seed_candidate = selected < observation.local_search_candidates.size()
              ? observation.local_search_candidates[selected].id : 0;
        } else if (request.invocation.rule == "LocalSearchGuidance") {
          request.invocation.granularity = InvocationGranularity::OneInstance;
          request.invocation.instances = {observation.local_search_candidates[
              draw(observation.local_search_candidates.size())].id};
        }
      }
    }
    if (direct_propagation_due && request.invocation.rule == "Propagate")
      request.invocation.post_application = PostApplication::Propagate;
    if (stop && *stop) {
      error = "controller selection interrupted";
      return false;
    }
    const auto allowance = config.allowance(request.invocation.rule);
    request.invocation.repeat = {RepeatMode::Once, 1};
    request.invocation.work_unit = allowance.unit;
    request.invocation.budget = WorkLimit::bounded(allowance.amount);
    request.invocation.rule_work_budget = allowance.native;
    if (startup_forced)
      startup_prefix_pending_sequence_ = request.sequence;
  }
  // Discovery can refresh a native scope. Bind to the resulting safe boundary
  // rather than falsely report the chooser's own discovery as external ABA.
  request.expected = state_version(k);
  request.producer_effort = std::move(producer_effort);
  return true;
}

int RandomControllerPolicy::solve(Kernel &k, volatile sig_atomic_t *stop) {
  coverage_ = {};
  for (const auto &rule : tactix_rules())
    if (root_exclusion(rule).empty())
      coverage_.rules.emplace(rule.name, ControllerRuleCoverage{});
  for (const auto configuration : search_configurations())
    coverage_.search.emplace(name(configuration), ControllerSearchCoverage{});
  uint64_t root_actions = 0;
  KernelStopPublisher publish_stop(k, stop);
  RestartController controller(k, config_.search, feedback_);
  RandomControllerSelector selector(trace_ && trace_->replaying() ? trace_->seed() : seed_);
  ControllerSelection &selection = selection_ ? *selection_ : selector;
  struct FeedbackEnd {
    RestartController &controller;
    int verdict = -1;
    std::optional<StopReason> reason = std::nullopt;
    ~FeedbackEnd() { controller.finish_feedback(verdict, reason); }
  } feedback_end{controller};
  const auto emit = [&](const std::string &event) {
    if (!trace_ || trace_->event(event))
      return true;
    std::fprintf(stderr, "c ERROR: %s\n", trace_->error().c_str());
    return false;
  };
  const auto finish = [&](int verdict, std::optional<StopReason> reason = std::nullopt) {
    feedback_end.verdict = verdict;
    feedback_end.reason = reason;
    for (const auto &[rule, counts] : coverage_.rules)
      std::printf("c controller-rule %s requests=%llu applications=%llu "
                  "effective=%llu exhausted=%llu stale=%llu interrupted=%llu "
                  "work=%llu overshoot=%llu\n", rule.c_str(),
                  (unsigned long long)counts.requests,
                  (unsigned long long)counts.applications,
                  (unsigned long long)counts.effective,
                  (unsigned long long)counts.exhausted,
                  (unsigned long long)counts.stale,
                  (unsigned long long)counts.interrupted,
                  (unsigned long long)counts.work,
                  (unsigned long long)counts.overshoot);
    for (const auto &[rule, counts] : coverage_.rules)
      if (counts.producer_calls)
        std::printf("c controller-producer %s calls=%llu work=%llu "
                    "candidates=%llu offers=%llu\n", rule.c_str(),
                    (unsigned long long)counts.producer_calls,
                    (unsigned long long)counts.producer_work,
                    (unsigned long long)counts.producer_candidates,
                    (unsigned long long)counts.producer_offers);
    for (const auto &[configuration, counts] : coverage_.search)
      std::printf("c controller-search %s requests=%llu intervals=%llu "
                  "decisions=%llu conflicts=%llu continuations=%llu\n", configuration.c_str(),
                  (unsigned long long)counts.requests,
                  (unsigned long long)counts.intervals,
                  (unsigned long long)counts.decisions,
                  (unsigned long long)counts.conflicts,
                  (unsigned long long)counts.continuations);
    std::printf("c controller-loop full-restarts=%llu partial-restarts=%llu configuration-changes=%llu "
                "maximum-root-actions=%llu\n",
                (unsigned long long)coverage_.full_restarts,
                (unsigned long long)coverage_.partial_restarts,
                (unsigned long long)coverage_.configuration_changes,
                (unsigned long long)coverage_.maximum_root_actions);
    if (trace_ && !trace_->finish(verdict)) {
      feedback_end.verdict = -1;
      feedback_end.reason = StopReason::Error;
      std::fprintf(stderr, "c ERROR: %s\n", trace_->error().c_str());
      return -1;
    }
    return verdict;
  };
  const auto &analysis = tactix_rules()[tactix_rule_id("ConflictAnalysis")];
  const auto &propagation = tactix_rules()[tactix_rule_id("Propagate")];
  uint64_t actions = 0;
  if (k.level && !k.inconsistent && !(stop && *stop)) {
    const auto result = controller.full_restart();
    ++coverage_.full_restarts;
    if (trace_ && !emit(service_record(k, "Restart", 0, result)))
      return -1;
    if (result == ApplyResult::Unsat || k.inconsistent)
      return finish(20);
  }
  for (;;) {
    if (stop && *stop)
      return finish(0);
    if (k.inconsistent)
      return finish(20);
    // Root conflicts are terminal search-service obligations, never a random
    // inprocessing choice that might destroy a live antecedent.
    if (k.conflict_valid) {
      const auto before = controller.observe();
      const auto glue = k.stats.learned_glue_sum;
      if (before.searching)
        controller.capture_before_backtrack();
      // The registered ConflictAnalysis boundary fuses learning and backjump;
      // Analyze's internal primitive alone leaves a pending assertion.
      const auto result = controller.search_service("ConflictAnalysis", INSTANCE_CANONICAL, [&] {
        const auto native = apply_rule(analysis, k);
        heuristics_consume_root_assignments(k, 0);
        heuristics_consume_rule_advice(k);
        heuristics_consume_retirements(k);
        return native;
      });
      if (trace_ && !emit(service_record(k, "ConflictAnalysis", 0, result)))
        return -1;
      if (result == ApplyResult::Unsat || k.inconsistent)
        return finish(20);
      if (before.searching)
        ++coverage_.search[name(before.search)].conflicts;
      if (before.searching)
        controller.search_conflict(static_cast<uint32_t>(
            k.stats.learned_glue_sum - glue));
      continue;
    }
    if (!k.unassigned_active && kernel_propagation_fixpoint(k))
      return finish(10);
    const auto observation = controller.observe();
    if (observation.searching) {
      if (!kernel_propagation_fixpoint(k)) {
        const size_t trail = k.trail.size();
        const auto result = controller.search_service("Propagate", INSTANCE_CANONICAL, [&] {
          const auto native = apply_rule(propagation, k);
          heuristics_consume_root_assignments(k, trail);
          return native;
        });
        if (trace_ && !emit(service_record(k, "Propagate", 0, result)))
          return -1;
        continue;
      }
      if (controller.affine_matrix_search_enabled()) {
        const auto matrix = controller.affine_matrix_fixpoint_service();
        if (trace_ && matrix.status != AffineMatrixServiceStatus::NoEvent &&
            matrix.status != AffineMatrixServiceStatus::Disabled &&
            !emit(affine_matrix_service_trace_record(k, matrix)))
          return -1;
        if (matrix.status == AffineMatrixServiceStatus::Applied)
          continue;
        if (matrix.status == AffineMatrixServiceStatus::ResourceExhausted) {
          // A finite optional service allowance is local to its session. The
          // recorded resource result remains exact; ordinary CDCL continues.
          k.affine_matrix_search_session->disable(k);
        } else if (matrix.terminal_failure() || matrix.status == AffineMatrixServiceStatus::Stale)
          return finish(0, matrix.status == AffineMatrixServiceStatus::Interrupted
                               ? StopReason::Interrupted
                           : matrix.status ==
                                 AffineMatrixServiceStatus::ResourceExhausted
                               ? StopReason::Resource
                           : matrix.status == AffineMatrixServiceStatus::Stale
                               ? StopReason::Stale
                               : StopReason::Error);
      }
      if (controller.restart_due()) {
        const uint32_t keep_level =
            config_.reuse_search_trail && k.level && k.unassigned_active
                ? heuristics_restart_reuse_level_for(
                      k, var_of(controller.next_decision()))
                : 0;
        const auto result = controller.restart(keep_level);
        if (keep_level) {
          ++coverage_.partial_restarts;
          ++coverage_.search[name(observation.search)].continuations;
        } else {
          ++coverage_.full_restarts;
        }
        if (trace_ && !emit(service_record(k, "Restart", keep_level, result)))
          return -1;
        if (result == ApplyResult::Unsat)
          return finish(20);
        continue;
      }
      const auto literal = controller.next_decision();
      const auto identity = lit_instance(k, literal);
      const auto result = controller.search_service("Decide", identity, [&] {
        const auto native = execution::execute_action(k, execution::DecisionRequest{literal});
        if (native == ApplyResult::Ok) {
          heuristics_note_target_decision(
              k, literal, observation.search.phase == Phase::Target);
          ++k.strategy_epoch;
        }
        return native;
      });
      if (result != ApplyResult::Ok) {
        std::fprintf(stderr, "c ERROR: controller ranked an invalid decision\n");
        return finish(0, StopReason::Error);
      }
      ++coverage_.search[name(observation.search)].decisions;
      if (trace_ && !emit(service_record(k, "Decide", identity, result)))
        return -1;
      continue;
    }
    ActionRequest request;
    std::string error;
    const bool selected =
        selection.choose(k, observation, config_, request, error, stop);
    for (const auto &effort : request.producer_effort) {
      auto &counts = coverage_.rules[effort.family];
      ++counts.producer_calls;
      counts.producer_work = counts.producer_work > UINT64_MAX - effort.total_work
                                 ? UINT64_MAX
                                 : counts.producer_work + effort.total_work;
      counts.producer_candidates =
          counts.producer_candidates > UINT64_MAX - effort.candidates
              ? UINT64_MAX
              : counts.producer_candidates + effort.candidates;
      counts.producer_offers = counts.producer_offers > UINT64_MAX - effort.offers
                                   ? UINT64_MAX
                                   : counts.producer_offers + effort.offers;
      if (effort.have_detector_census) {
        std::printf("c nary-csp-census candidate-domains=%llu "
                    "exact-domains=%llu relation-keys=%llu tuples=%llu "
                    "first-decline=%s reason=%u status=%u complete=%u "
                    "work=%llu offers=%llu\n",
                    (unsigned long long)effort.candidate_domains,
                    (unsigned long long)effort.exact_domains,
                    (unsigned long long)effort.relation_keys,
                    (unsigned long long)effort.tuple_occurrences,
                    nary_csp_prefix_action_offer_reason_name(
                        static_cast<NaryCspPrefixActionOfferReason>(
                            effort.first_decline_reason)),
                    unsigned(effort.first_decline_reason),
                    unsigned(effort.status), unsigned(effort.complete),
                    (unsigned long long)effort.total_work,
                    (unsigned long long)effort.offers);
        std::fflush(stdout);
      }
    }
    if (!selected) {
      if (!(stop && *stop))
        std::fprintf(stderr, "c ERROR: controller selection: %s\n", error.c_str());
      return finish(0, stop && *stop ? StopReason::Interrupted : StopReason::Error);
    }
    if (trace_ && !emit(request_record(request)))
      return -1;
    if (request.type == ActionType::ResumeSearch)
      ++coverage_.search[name(request.search)].requests;
    else {
      ++coverage_.rules[request.invocation.rule].requests;
      ++root_actions;
      coverage_.maximum_root_actions =
          std::max(coverage_.maximum_root_actions, root_actions);
    }
    const auto result = request.type == ActionType::ResumeSearch
                            ? controller.resume_search(request)
                            : controller.apply_rule(
                                  request, config_.allowance(request.invocation.rule),
                                  stop);
    if (result.resumed_search) {
      ++coverage_.search[name(request.search)].intervals;
      coverage_.configuration_changes +=
          observation.have_search_configuration &&
          observation.search != request.search;
      root_actions = 0;
    } else if (request.type == ActionType::Rule) {
      auto &counts = coverage_.rules[request.invocation.rule];
      counts.applications += result.invocation.applications;
      counts.effective += result.invocation.made_rule_progress;
      counts.exhausted +=
          result.invocation.status == InvocationStatus::BudgetExhausted ||
          result.invocation.status == InvocationStatus::ResourceExhausted;
      counts.stale += result.invocation.status == InvocationStatus::StaleRequest;
      counts.interrupted += result.invocation.status == InvocationStatus::Interrupted;
      counts.work += result.invocation.work;
      counts.overshoot += result.composition_overshoot;
    }
    if (trace_ && !emit(result_record(result)))
      return -1;
    if (trace_ && !trace_->timing(result.elapsed_nanos)) {
      std::fprintf(stderr, "c ERROR: %s\n", trace_->error().c_str());
      return -1;
    }
    ++actions;
    if ((actions & 255) == 0)
      kernel_check_invariants(k);
    if (result.invocation.status == InvocationStatus::Unsat || k.inconsistent)
      return finish(20);
    if (result.invocation.status == InvocationStatus::Sat)
      return finish(10);
    if (result.invocation.status == InvocationStatus::Interrupted)
      return finish(0, StopReason::Interrupted);
    if (result.invocation.status == InvocationStatus::InvalidRequest ||
        result.invocation.status == InvocationStatus::AccountingError) {
      std::fprintf(stderr, "c ERROR: controller execution status=%u action=%s\n",
                   static_cast<unsigned>(result.invocation.status),
                   request.type == ActionType::ResumeSearch
                       ? "ResumeSearch" : request.invocation.rule.c_str());
      return finish(0, StopReason::Error);
    }
    // Completed work stays committed. Stale scopes and native interrupted
    // progress are reselected at their existing boundaries, with no fallback.
  }
}

} // namespace tactix
