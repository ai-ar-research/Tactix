#include "strategy/online_inprocess.hpp"

#include "policies/heuristics.hpp"
#include "rules/sweep/sweep.hpp"
#include "strategy/bve_invocation.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <optional>
#include <unordered_map>

namespace tactix::strategy {

namespace {

bool valid_repeat(const RepeatSpec &repeat) {
  switch (repeat.mode) {
  case RepeatMode::Once:
  case RepeatMode::UntilScopedFixpoint:
    return true;
  case RepeatMode::Count:
    return repeat.limit != 0;
  }
  return false;
}

bool valid_rule_work_budget(const RuleWorkBudget &budget) {
  switch (budget.mode) {
  case RuleWorkBudgetMode::Inherit:
  case RuleWorkBudgetMode::Unlimited:
    return budget.amount == 0;
  case RuleWorkBudgetMode::Bounded:
    return true;
  }
  return false;
}

bool valid_setting_override(SettingOverride value) {
  switch (value) {
  case SettingOverride::Inherit:
  case SettingOverride::Disable:
  case SettingOverride::Enable:
    return true;
  }
  return false;
}

class ScopedRuleMaterialSettings {
public:
  ScopedRuleMaterialSettings(Kernel &kernel,
                             const RuleMaterialSettings &settings)
      : kernel_(kernel), saved_touch_filter_(kernel.touch_filter),
        saved_redundant_(kernel.touch_redundant) {
    install(settings.touch_filter, kernel_.touch_filter);
    install(settings.likely_kept_redundant, kernel_.touch_redundant);
  }
  ScopedRuleMaterialSettings(const ScopedRuleMaterialSettings &) = delete;
  ScopedRuleMaterialSettings &
  operator=(const ScopedRuleMaterialSettings &) = delete;
  ~ScopedRuleMaterialSettings() {
    kernel_.touch_filter = saved_touch_filter_;
    kernel_.touch_redundant = saved_redundant_;
  }

private:
  static void install(SettingOverride value, bool &target) {
    if (value == SettingOverride::Disable)
      target = false;
    else if (value == SettingOverride::Enable)
      target = true;
  }
  Kernel &kernel_;
  bool saved_touch_filter_;
  bool saved_redundant_;
};

class ScopedRuleWorkBudget {
public:
  ScopedRuleWorkBudget(Kernel &kernel, const RuleWorkBudget &budget)
      : kernel_(kernel), saved_budget_(kernel.work_budget),
        saved_offset_(kernel.work_budget_offset),
        saved_bounded_(kernel.work_budget_bounded),
        saved_midround_(kernel.midround_budget), posed_(
            budget.mode != RuleWorkBudgetMode::Inherit) {
    if (!posed_)
      return;
    kernel_.work_budget = budget.mode == RuleWorkBudgetMode::Bounded
                              ? budget.amount
                              : 0;
    kernel_.work_budget_offset = 0;
    kernel_.work_budget_bounded =
        budget.mode == RuleWorkBudgetMode::Bounded;
    kernel_.midround_budget = budget.mode == RuleWorkBudgetMode::Bounded;
  }
  ScopedRuleWorkBudget(const ScopedRuleWorkBudget &) = delete;
  ScopedRuleWorkBudget &operator=(const ScopedRuleWorkBudget &) = delete;
  ~ScopedRuleWorkBudget() {
    if (!posed_)
      return;
    kernel_.work_budget = saved_budget_;
    kernel_.work_budget_offset = saved_offset_;
    kernel_.work_budget_bounded = saved_bounded_;
    kernel_.midround_budget = saved_midround_;
  }

private:
  Kernel &kernel_;
  uint64_t saved_budget_;
  uint64_t saved_offset_;
  bool saved_bounded_;
  bool saved_midround_;
  bool posed_;
};

bool parameter_matches(const OnlineInvocation &step) {
  if (std::holds_alternative<std::monostate>(step.parameters))
    return true;
  if (std::holds_alternative<AnalyzeParameters>(step.parameters))
    return step.rule == "ConflictAnalysis";
  if (std::holds_alternative<BveParameters>(step.parameters))
    return step.rule == "BVE";
  if (std::holds_alternative<SubsumeForwardParameters>(step.parameters))
    return step.rule == "SubsumeForward";
  if (std::holds_alternative<StrengthenParameters>(step.parameters))
    return step.rule == "Strengthen";
  if (std::holds_alternative<CongruenceParameters>(step.parameters))
    return step.rule == "Congruence";
  if (std::holds_alternative<InstantiateParameters>(step.parameters))
    return step.rule == "Instantiate";
  if (std::holds_alternative<BackboneParameters>(step.parameters))
    return step.rule == "Backbone";
  if (std::holds_alternative<SweepParameters>(step.parameters))
    return step.rule == "Sweep";
  if (std::holds_alternative<VivifyParameters>(step.parameters))
    return step.rule == "Vivify";
  if (std::holds_alternative<BceParameters>(step.parameters))
    return step.rule == "BCE";
  if (std::holds_alternative<TernaryParameters>(step.parameters))
    return step.rule == "HyperTernaryRes";
  if (std::holds_alternative<FactorParameters>(step.parameters))
    return step.rule == "Factor";
  if (std::holds_alternative<TransitiveReduceParameters>(step.parameters))
    return step.rule == "TransitiveReduce";
  if (std::holds_alternative<SubstituteParameters>(step.parameters))
    return step.rule == "Substitute";
  if (std::holds_alternative<ReduceParameters>(step.parameters))
    return step.rule == "Reduce";
  if (std::holds_alternative<WalkParameters>(step.parameters))
    return step.rule == "Walk";
  if (std::holds_alternative<ReorderParameters>(step.parameters))
    return step.rule == "Reorder";
  if (std::holds_alternative<ParityModelParameters>(step.parameters))
    return step.rule == "Lucky";
  if (std::holds_alternative<OnehotModelParameters>(step.parameters))
    return step.rule == "Lucky";
  if (std::holds_alternative<SubstitutionRedundancyParameters>(step.parameters))
    return step.rule == "SubstitutionRedundancy";
  return false;
}

uint64_t saturated_add(uint64_t a, uint64_t b) {
  const uint64_t maximum = std::numeric_limits<uint64_t>::max();
  return b > maximum - a ? maximum : a + b;
}

WorkLimit remaining(const OnlineInvocation &step, uint64_t consumed) {
  if (step.budget.is_unlimited())
    return step.budget;
  return WorkLimit::bounded(consumed < step.budget.amount()
                                ? step.budget.amount() - consumed
                                : 0);
}

WorkUnit action_work_unit(const std::string &rule) {
  if (rule == "BVE")
    return WorkUnit::BveTicks;
  if (rule == "Propagate")
    return WorkUnit::Propagations;
  return WorkUnit::Applications;
}

} // namespace

bool validate_online_strategy_program(const OnlineStrategyProgram &program,
                                      std::string *error) {
  const auto reject = [error](const char *message) {
    if (error)
      *error = message;
    return false;
  };
  if (!valid_repeat(program.repeat))
    return reject("sequence repeat specification is invalid");
  for (size_t i = 0; i < program.sequence.size(); ++i) {
    const OnlineInvocation &step = program.sequence[i];
    if (!step.id)
      return reject("StepId zero is reserved");
    if (step.rule.empty())
      return reject("rule name is empty");
    for (size_t j = 0; j < i; ++j)
      if (program.sequence[j].id == step.id)
        return reject("StepId is duplicated");
    if (!valid_repeat(step.repeat))
      return reject("step repeat specification is invalid");
    if (!valid_rule_work_budget(step.rule_work_budget))
      return reject("rule work budget is invalid");
    if (!valid_setting_override(step.material.touch_filter) ||
        !valid_setting_override(step.material.likely_kept_redundant))
      return reject("rule material setting is invalid");
    if (!parameter_matches(step))
      return reject("parameters do not match rule");
    if (const auto *p = std::get_if<AnalyzeParameters>(&step.parameters);
        p && !p->valid())
      return reject("Analyze parameters are invalid");
    if (const auto *p = std::get_if<BveParameters>(&step.parameters);
        p && !p->valid())
      return reject("BVE parameters are invalid");
    if (const auto *p =
            std::get_if<SubsumeForwardParameters>(&step.parameters);
        p && !p->valid())
      return reject("C1 parameters are invalid");
    if (const auto *p = std::get_if<StrengthenParameters>(&step.parameters);
        p && !p->valid())
      return reject("C2 parameters are invalid");
    if (const auto *p = std::get_if<CongruenceParameters>(&step.parameters);
        p && !p->valid())
      return reject("Congruence parameters are invalid");
    if (const auto *p = std::get_if<InstantiateParameters>(&step.parameters);
        p && !p->valid())
      return reject("Instantiate parameters are invalid");
    if (const auto *p = std::get_if<BackboneParameters>(&step.parameters);
        p && !p->valid())
      return reject("Backbone parameters are invalid");
    if (const auto *p = std::get_if<SweepParameters>(&step.parameters);
        p && !p->valid())
      return reject("Sweep parameters are invalid");
    if (const auto *p = std::get_if<VivifyParameters>(&step.parameters);
        p && !p->valid())
      return reject("Vivify parameters are invalid");
    if (const auto *p = std::get_if<BceParameters>(&step.parameters);
        p && !p->valid())
      return reject("BCE parameters are invalid");
    if (const auto *p = std::get_if<TernaryParameters>(&step.parameters);
        p && !p->valid())
      return reject("ternary parameters are invalid");
    if (const auto *p = std::get_if<FactorParameters>(&step.parameters);
        p && !p->valid())
      return reject("Factor parameters are invalid");
    if (const auto *p =
            std::get_if<TransitiveReduceParameters>(&step.parameters);
        p && !p->valid())
      return reject("transitive parameters are invalid");
    if (const auto *p = std::get_if<SubstituteParameters>(&step.parameters);
        p && !p->valid())
      return reject("Substitute parameters are invalid");
    if (const auto *p = std::get_if<ReduceParameters>(&step.parameters);
        p && !p->valid())
      return reject("Reduce parameters are invalid");
    if (const auto *p = std::get_if<WalkParameters>(&step.parameters);
        p && !p->valid())
      return reject("Walk parameters are invalid");
    if (const auto *p = std::get_if<ReorderParameters>(&step.parameters);
        p && !p->valid())
      return reject("Reorder parameters are invalid");
    if (const auto *p =
            std::get_if<ParityModelParameters>(&step.parameters);
        p && !p->valid())
      return reject("Lucky parity-model parameters are invalid");
    if (const auto *p =
            std::get_if<OnehotModelParameters>(&step.parameters);
        p && !p->valid())
      return reject("Lucky onehot-model parameters are invalid");
    if (const auto *p = std::get_if<SubstitutionRedundancyParameters>(
            &step.parameters);
        p && !p->valid())
      return reject("SR parameters are invalid");
    if (!step.factor_grids.empty() && step.rule != "Factor")
      return reject("exact Factor grids are attached to a different rule");
    if (!step.factor_grids.empty() && !step.instances.empty())
      return reject("scalar and exact-grid selections cannot be mixed");
    if (!step.substitution_redundancy_instances.empty() &&
        step.rule != "SubstitutionRedundancy")
      return reject("SR instances are attached to a different rule");
    const unsigned structured_payload_kinds =
        !step.factor_grids.empty() +
        !step.substitution_redundancy_instances.empty();
    if (structured_payload_kinds && !step.instances.empty())
      return reject("scalar and structured selections cannot be mixed");
    if (structured_payload_kinds > 1)
      return reject("different structured selection kinds cannot be mixed");
    const size_t structured_size = !step.factor_grids.empty()
                                       ? step.factor_grids.size()
                                       : step.substitution_redundancy_instances.size();
    switch (step.granularity) {
    case InvocationGranularity::OneInstance:
      if ((structured_payload_kinds && structured_size != 1) ||
          (!structured_payload_kinds && step.instances.size() != 1))
        return reject("one-instance selection does not contain one payload");
      if (!structured_payload_kinds &&
          step.instances.front() == INSTANCE_CANONICAL)
        return reject("explicit selection contains the canonical whole-rule payload");
      break;
    case InvocationGranularity::SelectedBatch:
      if (step.instances.empty() && !structured_payload_kinds)
        return reject("selected batch is empty");
      if (std::find(step.instances.begin(), step.instances.end(),
                    INSTANCE_CANONICAL) != step.instances.end())
        return reject("explicit selection contains the canonical whole-rule payload");
      break;
    case InvocationGranularity::CompleteCurrentPass:
      if (!step.instances.empty() || structured_payload_kinds)
        return reject("complete pass carries explicit payloads");
      break;
    case InvocationGranularity::CanonicalApplication:
      if (!step.instances.empty() || structured_payload_kinds)
        return reject("canonical application carries explicit payloads");
      break;
    default:
      return reject("granularity is invalid");
    }
    if (step.work_unit != action_work_unit(step.rule))
      return reject("action budget has the wrong currency");
  }
  if (error)
    error->clear();
  return true;
}

InvocationStatus enumerate_complete_instances(Kernel &k,
                                              const RuleVTable &rule,
                                              std::vector<uint64_t> &out,
                                              volatile sig_atomic_t *stop) {
  out.clear();
  if ((stop && *stop) || (k.stop && *k.stop))
    return InvocationStatus::Interrupted;
  const auto enumerate =
      rule.semantic_instances ? rule.semantic_instances : rule.instances;
  if (!enumerate) {
    out.push_back(INSTANCE_CANONICAL);
    return InvocationStatus::Completed;
  }
  uint32_t cap = 64;
  for (;;) {
    if ((stop && *stop) || (k.stop && *k.stop)) {
      out.clear();
      return InvocationStatus::Interrupted;
    }
    out.resize(cap);
    const uint32_t count = enumerate(k, out.data(), cap);
    if ((stop && *stop) || (k.stop && *k.stop)) {
      out.clear();
      return InvocationStatus::Interrupted;
    }
    if (count < cap) {
      out.resize(count);
      // Some frozen v1 surfaces rank the whole-rule sentinel beside finer
      // instances. A complete semantic pass must not run that whole closure
      // and then repeat each constituent action. Preserve it only when it is
      // the rule's sole available granularity.
      if (out.size() > 1)
        out.erase(std::remove(out.begin(), out.end(), INSTANCE_CANONICAL),
                  out.end());
      return InvocationStatus::Completed;
    }
    if (cap > std::numeric_limits<uint32_t>::max() / 2) {
      out.clear();
      return InvocationStatus::ResourceExhausted;
    }
    cap *= 2;
  }
}

bool OnlineStrategyExecutor::configure(const OnlineStrategyProgram &program,
                                       uint64_t revision,
                                       ReconfigureMode mode,
                                       std::string *error) {
  if (mode != ReconfigureMode::ResetRuntime &&
      mode != ReconfigureMode::RetainMatchingStepBudgets) {
    if (error)
      *error = "reconfiguration mode is invalid";
    return false;
  }
  if (!validate_online_strategy_program(program, error))
    return false;
  for (const OnlineInvocation &step : program.sequence) {
    const int id = tactix_rule_id(step.rule.c_str());
    if (id < 0) {
      if (error)
        *error = "program names an unknown action";
      return false;
    }
  }

  struct RetainedBudget {
    std::string rule;
    WorkUnit unit;
    uint64_t consumed;
  };
  std::unordered_map<StepId, RetainedBudget> retained;
  if (configured_ && mode == ReconfigureMode::RetainMatchingStepBudgets)
    for (size_t i = 0; i < program_.sequence.size(); ++i)
      retained.emplace(
          program_.sequence[i].id,
          RetainedBudget{program_.sequence[i].rule,
                         program_.sequence[i].work_unit,
                         runtime_[i].consumed});

  OnlineStrategyProgram replacement = program;
  std::vector<Runtime> replacement_runtime(replacement.sequence.size());
  for (size_t i = 0; i < replacement.sequence.size(); ++i) {
    const auto found = retained.find(replacement.sequence[i].id);
    if (found != retained.end() &&
        found->second.rule == replacement.sequence[i].rule &&
        found->second.unit == replacement.sequence[i].work_unit)
      replacement_runtime[i].consumed = found->second.consumed;
  }
  program_ = std::move(replacement);
  runtime_ = std::move(replacement_runtime);
  revision_ = revision;
  step_index_ = 0;
  sequence_passes_ = 0;
  sequence_progress_ = false;
  complete_ = program_.sequence.empty();
  configured_ = true;
  if (error)
    error->clear();
  return true;
}

void OnlineStrategyExecutor::reset_runtime() {
  runtime_.assign(program_.sequence.size(), Runtime{});
  step_index_ = 0;
  sequence_passes_ = 0;
  sequence_progress_ = false;
  complete_ = program_.sequence.empty();
}

uint64_t OnlineStrategyExecutor::step_consumed(StepId id) const {
  for (size_t i = 0; i < program_.sequence.size(); ++i)
    if (program_.sequence[i].id == id)
      return runtime_[i].consumed;
  return 0;
}

bool OnlineStrategyExecutor::prepare_selection(Kernel &k,
                                               const OnlineInvocation &step,
                                               Runtime &runtime,
                                               InvocationResult &out,
                                               volatile sig_atomic_t *stop) {
  // An explicit scope remains bound across repeated passes/sequences. Only
  // CompleteCurrentPass asks for fresh enumeration after a representation
  // change; repeating a pass is not permission to reinterpret old payloads.
  if (step.granularity != InvocationGranularity::CompleteCurrentPass &&
      step.granularity != InvocationGranularity::CanonicalApplication &&
      runtime.selection_owner &&
      (runtime.selection_owner != &k ||
       runtime.selection_generation != k.arena_generation)) {
    out.status = InvocationStatus::StaleRequest;
    out.step_id = step.id;
    return false;
  }
  const auto *sr_parameters =
      step.rule == "SubstitutionRedundancy"
          ? std::get_if<SubstitutionRedundancyParameters>(&step.parameters)
          : nullptr;
  const bool general_sr =
      step.granularity == InvocationGranularity::CompleteCurrentPass &&
      sr_parameters && sr_parameters->general_producer;
  const bool continuing_general_sr = runtime.sr_general_mode && general_sr;
  runtime.selection.clear();
  runtime.structured_selection.clear();
  runtime.sr_selection.clear();
  runtime.cursor = 0;
  if (!continuing_general_sr)
    runtime.pass_progress = false;
  runtime.selection_truncated = false;
  runtime.sr_general_mode = general_sr;
  if (!general_sr)
    runtime.sr_general_cursor = {};
  const RuleVTable &rule = tactix_rules()[tactix_rule_id(step.rule.c_str())];
  if (!rule.guard(k)) {
    out.status = InvocationStatus::Inapplicable;
    return false;
  }
  if (step.granularity == InvocationGranularity::CanonicalApplication) {
    runtime.selection = {INSTANCE_CANONICAL};
  } else if (step.granularity != InvocationGranularity::CompleteCurrentPass) {
    if (step.factor_grids.empty() &&
        step.substitution_redundancy_instances.empty()) {
      runtime.selection = step.instances;
    } else {
      const size_t size = !step.factor_grids.empty()
                              ? step.factor_grids.size()
                              : step.substitution_redundancy_instances.size();
      runtime.selection.resize(size);
      for (size_t i = 0; i < runtime.selection.size(); ++i)
        runtime.selection[i] = i;
    }
  } else if (step.rule == "Factor") {
    uint64_t maximum = std::numeric_limits<uint64_t>::max();
    if (!step.budget.is_unlimited())
      maximum = remaining(step, runtime.consumed).amount();
    const FactorEnumerationResult enumeration = factor_enumerate_grids(
        k,
        [&](const FactorGrid &grid) {
          runtime.structured_selection.push_back(grid);
          return true;
        },
        FactorEnumerationOptions{maximum, stop});
    if (enumeration.status == FactorEnumerationStatus::Stopped) {
      out.status = InvocationStatus::Interrupted;
      return false;
    }
    runtime.selection_truncated =
        enumeration.status == FactorEnumerationStatus::LimitReached;
    runtime.selection.resize(runtime.structured_selection.size());
    for (size_t i = 0; i < runtime.selection.size(); ++i)
      runtime.selection[i] = i;
  } else if (step.rule == "SubstitutionRedundancy") {
    const auto *configured =
        std::get_if<SubstitutionRedundancyParameters>(&step.parameters);
    const SubstitutionRedundancyParameters parameters =
        configured ? *configured : SubstitutionRedundancyParameters{};
    if (parameters.general_producer) {
      SubstitutionRedundancyInstance instance;
      const SubstitutionRedundancyGeneralResult enumeration =
          substitution_redundancy_next_general(
              k, parameters, runtime.sr_general_cursor, instance, stop);
      if (enumeration.status ==
          SubstitutionRedundancyGeneralStatus::Interrupted) {
        out.status = InvocationStatus::Interrupted;
        return false;
      }
      if (enumeration.status ==
              SubstitutionRedundancyGeneralStatus::ResourceExhausted ||
          enumeration.status ==
              SubstitutionRedundancyGeneralStatus::LimitReached) {
        out.status = InvocationStatus::ResourceExhausted;
        return false;
      }
      if (enumeration.status == SubstitutionRedundancyGeneralStatus::Found)
        runtime.sr_selection.push_back(std::move(instance));
    } else {
      const OrbitopalEnumerationResult enumeration =
          substitution_redundancy_enumerate_orbitopal(
              k, parameters,
              [&](const SubstitutionRedundancyInstance &instance) {
                runtime.sr_selection.push_back(instance);
                return true;
              },
              stop);
      if (enumeration.status == OrbitopalEnumerationStatus::Interrupted) {
        out.status = InvocationStatus::Interrupted;
        return false;
      }
      runtime.selection_truncated =
          enumeration.status == OrbitopalEnumerationStatus::LimitReached;
    }
    runtime.selection.resize(runtime.sr_selection.size());
    for (size_t i = 0; i < runtime.selection.size(); ++i)
      runtime.selection[i] = i;
  } else {
    out.status = enumerate_complete_instances(k, rule, runtime.selection, stop);
    if (out.status != InvocationStatus::Completed)
      return false;
  }
  runtime.selection_owner = &k;
  runtime.selection_generation = k.arena_generation;
  runtime.expected_state = state_version(k);
  runtime.selection_ready = true;
  return true;
}

bool OnlineStrategyExecutor::finish_step_pass(const OnlineInvocation &step,
                                              Runtime &runtime,
                                              InvocationResult &out) {
  runtime.selection_ready = false;
  runtime.selection.clear();
  runtime.structured_selection.clear();
  runtime.sr_selection.clear();
  runtime.sr_general_cursor = {};
  runtime.sr_general_mode = false;
  runtime.selection_truncated = false;
  runtime.cursor = 0;
  ++runtime.passes;
  const bool progress = runtime.pass_progress;
  sequence_progress_ |= progress;

  bool repeat = false;
  switch (step.repeat.mode) {
  case RepeatMode::Once:
    break;
  case RepeatMode::Count:
    repeat = runtime.passes < step.repeat.limit;
    break;
  case RepeatMode::UntilScopedFixpoint:
    if (!progress) {
      out.status = InvocationStatus::ScopedFixpoint;
      out.selection_complete = true;
    } else if (!step.repeat.limit || runtime.passes < step.repeat.limit) {
      repeat = true;
    } else {
      out.status = InvocationStatus::BudgetExhausted;
      out.selection_complete = true;
    }
    break;
  }
  runtime.pass_progress = false;
  if (repeat)
    return false;
  ++step_index_;
  return true;
}

bool OnlineStrategyExecutor::finish_sequence(InvocationResult &out) {
  ++sequence_passes_;
  bool repeat = false;
  switch (program_.repeat.mode) {
  case RepeatMode::Once:
    break;
  case RepeatMode::Count:
    repeat = sequence_passes_ < program_.repeat.limit;
    break;
  case RepeatMode::UntilScopedFixpoint:
    if (!sequence_progress_) {
      out.status = InvocationStatus::ScopedFixpoint;
      out.selection_complete = true;
    } else if (!program_.repeat.limit ||
               sequence_passes_ < program_.repeat.limit) {
      repeat = true;
    } else {
      out.status = InvocationStatus::BudgetExhausted;
      out.selection_complete = true;
    }
    break;
  }
  if (repeat) {
    step_index_ = 0;
    for (Runtime &runtime : runtime_) {
      runtime.passes = 0;
      runtime.cursor = 0;
      runtime.pass_progress = false;
      runtime.selection_ready = false;
      runtime.selection.clear();
      runtime.structured_selection.clear();
      runtime.sr_selection.clear();
      runtime.sr_general_cursor = {};
      runtime.sr_general_mode = false;
      runtime.selection_truncated = false;
    }
    sequence_progress_ = false;
    return false;
  }
  complete_ = true;
  if (out.status != InvocationStatus::ScopedFixpoint &&
      out.status != InvocationStatus::BudgetExhausted)
    out.status = InvocationStatus::Completed;
  out.selection_complete = true;
  return true;
}

InvocationResult OnlineStrategyExecutor::advance(
    Kernel &k, volatile sig_atomic_t *stop) {
  InvocationResult out;
  out.before = state_version(k);
  out.after = out.before;
  out.configuration_revision = revision_;
  if (!configured_) {
    out.status = InvocationStatus::InvalidRequest;
    return out;
  }
  if (complete_) {
    out.status = InvocationStatus::Completed;
    out.selection_complete = true;
    return out;
  }
  if ((stop && *stop) || (k.stop && *k.stop)) {
    out.status = InvocationStatus::Interrupted;
    return out;
  }

  for (;;) {
    // Empty scopes may repeat without reaching an atomic rule application.
    // Poll at composition boundaries too, so those loops remain interruptible.
    if ((stop && *stop) || (k.stop && *k.stop)) {
      out.status = InvocationStatus::Interrupted;
      out.selection_complete = false;
      return out;
    }
    if (step_index_ == program_.sequence.size()) {
      if (finish_sequence(out))
        return out;
      continue;
    }
    const OnlineInvocation &step = program_.sequence[step_index_];
    Runtime &runtime = runtime_[step_index_];
    out.work_unit = step.work_unit;
    if (runtime.selection_ready &&
        (runtime.selection_owner != &k ||
         runtime.expected_state.arena_generation != k.arena_generation ||
         runtime.expected_state.touch_epoch != k.touch_epoch ||
         runtime.expected_state.strategy_epoch != k.strategy_epoch)) {
      out.status = InvocationStatus::StaleRequest;
      out.step_id = step.id;
      out.after = state_version(k);
      return out;
    }
    if (runtime.selection_ready && runtime.cursor < runtime.selection.size() &&
        runtime.selection_generation != k.arena_generation) {
      if (step.granularity != InvocationGranularity::CompleteCurrentPass) {
        out.status = InvocationStatus::StaleRequest;
        out.step_id = step.id;
        out.after = state_version(k);
        return out;
      }
      // A preceding semantic action collected or rebuilt the arena. A
      // complete pass has no caller-owned payload identity, so restart its
      // enumeration at the new epoch while retaining progress already made in
      // this pass. An external transition was rejected by the state check
      // above and can never be silently absorbed here.
      const bool progress = runtime.pass_progress;
      runtime.selection_ready = false;
      if (!prepare_selection(k, step, runtime, out, stop)) {
        out.after = state_version(k);
        return out;
      }
      runtime.pass_progress |= progress;
    }
    if (!runtime.selection_ready) {
      if (!prepare_selection(k, step, runtime, out, stop)) {
        // An inapplicable scope is not a fixpoint and is not consumed. The
        // caller may change solver state or replace the program and retry.
        out.after = state_version(k);
        return out;
      }
      if (runtime.selection.empty()) {
        if (runtime.selection_truncated) {
          out.status = InvocationStatus::BudgetExhausted;
          out.budget_status = BudgetStatus::Exhausted;
          out.selection_complete = false;
          out.step_id = step.id;
          out.after = state_version(k);
          return out;
        }
        finish_step_pass(step, runtime, out);
        if (out.status == InvocationStatus::ScopedFixpoint ||
            out.status == InvocationStatus::BudgetExhausted) {
          out.after = state_version(k);
          return out;
        }
        continue;
      }
    }
    if (runtime.cursor == runtime.selection.size()) {
      if (runtime.sr_general_mode &&
          !runtime.sr_general_cursor.complete) {
        runtime.selection_ready = false;
        if (!prepare_selection(k, step, runtime, out, stop)) {
          out.after = state_version(k);
          return out;
        }
        continue;
      }
      if (runtime.selection_truncated) {
        out.status = InvocationStatus::BudgetExhausted;
        out.budget_status = BudgetStatus::Exhausted;
        out.selection_complete = false;
        out.step_id = step.id;
        out.after = state_version(k);
        return out;
      }
      finish_step_pass(step, runtime, out);
      if (out.status == InvocationStatus::ScopedFixpoint ||
          out.status == InvocationStatus::BudgetExhausted) {
        out.after = state_version(k);
        return out;
      }
      continue;
    }

    const WorkLimit allowance = remaining(step, runtime.consumed);
    if (!allowance.admits_work()) {
      out.status = InvocationStatus::BudgetExhausted;
      out.budget_status = BudgetStatus::Exhausted;
      return out;
    }

    const RuleVTable &rule = tactix_rules()[tactix_rule_id(step.rule.c_str())];
    // Eligibility can change between advances even when arena offsets remain
    // valid. A declined guard does not consume an application or certify scope.
    if (!rule.guard(k)) {
      out.status = InvocationStatus::Inapplicable;
      out.step_id = step.id;
      out.rule_id = rule.id;
      return out;
    }
    const uint64_t payload = runtime.selection[runtime.cursor];
    InvocationBudget budget(step.work_unit, allowance);
    ScopedRuleMaterialSettings material(k, step.material);
    if (step.rule == "BVE") {
      ScopedRuleWorkBudget rule_work_budget(k, step.rule_work_budget);
      const auto *configured = std::get_if<BveParameters>(&step.parameters);
      const BveParameters parameters =
          configured ? *configured : BveParameters::from_kernel(k);
      BveInvocationSession session(rule, k, parameters, budget, revision_);
      out = payload == INSTANCE_CANONICAL ? session.apply_canonical()
                                          : session.apply_one(payload);
      heuristics_consume_rule_advice(k);
      heuristics_consume_retirements(k);
    } else if (step.rule == "Sweep" &&
               std::holds_alternative<SweepParameters>(step.parameters)) {
      ScopedRuleWorkBudget rule_work_budget(k, step.rule_work_budget);
      const SweepParameters &limits = std::get<SweepParameters>(step.parameters);
      const ApplyResult result =
          payload == INSTANCE_CANONICAL
              ? sweep_apply_exact_limits(k, limits)
              : sweep_try_with_limits(k, static_cast<Var>(payload), limits);
      heuristics_consume_rule_advice(k);
      heuristics_consume_retirements(k);
      out.before = out.after;
      out.configuration_revision = revision_;
      out.work_unit = WorkUnit::Applications;
      out.rule_id = rule.id;
      out.instance = payload;
      out.work = 1;
      out.applications = 1;
      out.rule_result = result;
      out.made_rule_progress = result == ApplyResult::Ok;
      out.selection_complete = runtime.cursor + 1 == runtime.selection.size();
      out.budget_status = budget.charge(WorkUnit::Applications, 1);
      out.status = result == ApplyResult::NoOp
                       ? InvocationStatus::NoChange
                       : result == ApplyResult::Ok
                             ? InvocationStatus::Completed
                             : detail::terminal_status(result);
      out.after = state_version(k);
    } else {
      const uint64_t propagations_before = k.stats.propagations;
      std::optional<ScopedAnalyzeParameters> analyze;
      std::optional<ScopedSubsumeForwardParameters> c1;
      std::optional<ScopedStrengthenParameters> c2;
      std::optional<ScopedCongruenceParameters> congruence;
      std::optional<ScopedInstantiateParameters> instantiate;
      std::optional<ScopedBackboneParameters> backbone;
      std::optional<ScopedVivifyParameters> vivify;
      std::optional<ScopedBceParameters> bce;
      std::optional<ScopedTernaryParameters> ternary;
      std::optional<ScopedFactorParameters> factor;
      std::optional<ScopedTransitiveReduceParameters> transitive;
      std::optional<ScopedSubstituteParameters> substitute;
      std::optional<ScopedReduceParameters> reduce;
      std::optional<ScopedWalkParameters> walk;
      std::optional<ScopedReorderParameters> reorder;
      std::optional<ScopedParityModelParameters> parity_model;
      std::optional<ScopedOnehotModelParameters> onehot_model;
      if (const auto *p = std::get_if<AnalyzeParameters>(&step.parameters))
        analyze.emplace(k, *p);
      else if (const auto *p =
              std::get_if<SubsumeForwardParameters>(&step.parameters))
        c1.emplace(k, *p);
      else if (const auto *p =
                   std::get_if<StrengthenParameters>(&step.parameters))
        c2.emplace(k, *p);
      else if (const auto *p =
                   std::get_if<CongruenceParameters>(&step.parameters))
        congruence.emplace(k, *p);
      else if (const auto *p =
                   std::get_if<InstantiateParameters>(&step.parameters))
        instantiate.emplace(k, *p);
      else if (const auto *p =
                   std::get_if<BackboneParameters>(&step.parameters))
        backbone.emplace(k, *p);
      else if (const auto *p =
                   std::get_if<VivifyParameters>(&step.parameters))
        vivify.emplace(k, *p);
      else if (const auto *p = std::get_if<BceParameters>(&step.parameters))
        bce.emplace(k, *p);
      else if (const auto *p =
                   std::get_if<TernaryParameters>(&step.parameters))
        ternary.emplace(k, *p);
      else if (const auto *p =
                   std::get_if<FactorParameters>(&step.parameters))
        factor.emplace(k, *p);
      else if (const auto *p =
                   std::get_if<TransitiveReduceParameters>(&step.parameters))
        transitive.emplace(k, *p);
      else if (const auto *p =
                   std::get_if<SubstituteParameters>(&step.parameters))
        substitute.emplace(k, *p);
      else if (const auto *p =
                   std::get_if<ReduceParameters>(&step.parameters))
        reduce.emplace(k, *p);
      else if (const auto *p =
                   std::get_if<WalkParameters>(&step.parameters))
        walk.emplace(k, *p);
      else if (const auto *p =
                   std::get_if<ReorderParameters>(&step.parameters))
        reorder.emplace(k, *p);
      else if (const auto *p =
                   std::get_if<ParityModelParameters>(&step.parameters))
        parity_model.emplace(k, *p);
      else if (const auto *p =
                   std::get_if<OnehotModelParameters>(&step.parameters))
        onehot_model.emplace(k, *p);
      // Invocation-level native work limits override a rule parameter which
      // happens to share the legacy Kernel::work_budget handoff (Walk).  Both
      // scopes still unwind to the caller profile after this atomic apply.
      ScopedRuleWorkBudget rule_work_budget(k, step.rule_work_budget);
      const bool enumerated_factor_grid =
          step.rule == "Factor" &&
          step.granularity == InvocationGranularity::CompleteCurrentPass;
      const bool enumerated_sr =
          step.rule == "SubstitutionRedundancy" &&
          step.granularity == InvocationGranularity::CompleteCurrentPass;
      std::optional<SubstitutionRedundancyApplicationResult> sr_application;
      const ApplyResult result =
          (!step.substitution_redundancy_instances.empty() || enumerated_sr)
              ? apply_rule_structured(rule, k, "exact-sr", [&] {
                  const auto *configured =
                      std::get_if<SubstitutionRedundancyParameters>(
                          &step.parameters);
                  const SubstitutionRedundancyParameters parameters =
                      configured ? *configured
                                 : SubstitutionRedundancyParameters{};
                  const SubstitutionRedundancyInstance &instance =
                      !step.substitution_redundancy_instances.empty()
                          ? step.substitution_redundancy_instances[payload]
                          : runtime.sr_selection[payload];
                  sr_application = substitution_redundancy_apply_detailed(
                      k, instance, parameters, stop);
                  return sr_application->rule_result;
                })
          : (!step.factor_grids.empty() || enumerated_factor_grid)
              ? apply_rule_structured(rule, k, "exact-grid", [&] {
                  const FactorGrid &grid = !step.factor_grids.empty()
                                               ? step.factor_grids[payload]
                                               : runtime.structured_selection
                                                     [payload];
                  return factor_apply_grid(k, grid);
                })
          : payload == INSTANCE_CANONICAL ||
                    (!rule.apply_semantic_instance && !rule.apply_instance)
              ? apply_rule(rule, k)
          : rule.apply_semantic_instance
              ? apply_rule_semantic_instance(rule, k, payload)
              : apply_rule_instance(rule, k, payload);
      // Factor/Substitute parameter scopes control how their newly published
      // ranking advice is consumed. Drain it before those scopes restore the
      // caller's profile; retirement itself is representation state and is
      // always reflected in H.
      heuristics_consume_rule_advice(k);
      heuristics_consume_retirements(k);
      out.before = out.after;
      out.configuration_revision = revision_;
      out.work_unit = step.work_unit;
      out.rule_id = rule.id;
      out.instance = payload;
      out.work = step.work_unit == WorkUnit::Propagations
                     ? k.stats.propagations - propagations_before
                     : 1;
      out.applications = 1;
      out.rule_result = result;
      out.made_rule_progress = result == ApplyResult::Ok;
      out.selection_complete = runtime.cursor + 1 == runtime.selection.size();
      out.budget_status = budget.charge(step.work_unit, out.work);
      out.status = result == ApplyResult::NoOp
                       ? InvocationStatus::NoChange
                       : result == ApplyResult::Ok
                             ? InvocationStatus::Completed
                             : detail::terminal_status(result);
      if (sr_application) {
        switch (sr_application->status) {
        case SubstitutionRedundancyApplicationStatus::Invalid:
          // Once an earlier batch member changes the formula, a later SR
          // precondition is judged against a different state. Treat that as
          // the same stale-snapshot boundary used by other exact batches;
          // malformed storage was already rejected before any mutation.
          out.status = runtime.pass_progress
                           ? InvocationStatus::StaleRequest
                           : InvocationStatus::InvalidRequest;
          break;
        case SubstitutionRedundancyApplicationStatus::Inapplicable:
        case SubstitutionRedundancyApplicationStatus::UnsupportedProofFormat:
          out.status = InvocationStatus::Inapplicable;
          break;
        case SubstitutionRedundancyApplicationStatus::Interrupted:
          out.status = InvocationStatus::Interrupted;
          break;
        case SubstitutionRedundancyApplicationStatus::ResourceExhausted:
          out.status = InvocationStatus::ResourceExhausted;
          break;
        default:
          break;
        }
        if (out.status == InvocationStatus::InvalidRequest ||
            out.status == InvocationStatus::StaleRequest ||
            out.status == InvocationStatus::Inapplicable ||
            out.status == InvocationStatus::Interrupted ||
            out.status == InvocationStatus::ResourceExhausted) {
          out.work = 0;
          out.applications = 0;
          out.selection_complete = false;
          out.budget_status = BudgetStatus::Ready;
        }
      }
      out.after = state_version(k);
    }
    runtime.expected_state = state_version(k);
    runtime.consumed = saturated_add(runtime.consumed, out.work);
    runtime.pass_progress |= out.made_rule_progress;
    sequence_progress_ |= out.made_rule_progress;
    if (out.applications)
      ++runtime.cursor;
    out.configuration_revision = revision_;
    out.step_id = step.id;
    out.after = state_version(k);
    return out;
  }
}

} // namespace tactix::strategy
