#include "strategy/online_inprocess.hpp"

#include "policies/heuristics.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/sweep/sweep.hpp"
#include "strategy/bve_invocation.hpp"
#include "strategy/rule_execution.hpp"
#include "strategy/work_execution.hpp"

#include <cmath>
#include <algorithm>
#include <cstring>
#include <limits>
#include <optional>
#include <unordered_map>

namespace tactix {
// Private cross-translation-unit admission used only for freshly enumerated
// producer certificates. It is intentionally absent from the rule API.
SubstitutionRedundancyApplicationResult
substitution_redundancy_apply_orbitopal_prefix(
    Kernel &, const SubstitutionRedundancyInstance &,
    const SubstitutionRedundancyParameters &, volatile sig_atomic_t *);
}

namespace tactix::strategy {

namespace {

} // namespace

uint64_t repeat_round_limit(const Kernel &k, const RepeatSpec &repeat) {
  if (repeat.mode != RepeatMode::UntilSizeScaledFixpoint)
    return repeat.limit;
  // satsuma.h: size is the clause database's literal count plus its variables.
  uint64_t size = 2 * k.watches.binary_clauses() + k.nvars;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    if (!clause->garbage())
      size += clause->size;
    ref += clause_actual_words(clause);
  }
  constexpr double scale = static_cast<double>(uint64_t{1} << 23);
  const double raw =
      std::pow(scale / static_cast<double>(std::max<uint64_t>(size, 1)), 1.5);
  // The owner iterates while `iteration < iteration_max`, after one initial
  // pass: its pass count is the clamped scale plus one.
  if (repeat.limit && !(raw < static_cast<double>(repeat.limit)))
    return repeat.limit + 1;
  if (!(raw < 0x1p62))
    return 0;
  return std::max<uint64_t>(1, static_cast<uint64_t>(std::ceil(raw))) + 1;
}

namespace {

bool valid_repeat(const RepeatSpec &repeat) {
  switch (repeat.mode) {
  case RepeatMode::Once:
  case RepeatMode::UntilScopedFixpoint:
  case RepeatMode::UntilSizeScaledFixpoint:
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

bool valid_post_application(PostApplication value) {
  switch (value) {
  case PostApplication::None:
  case PostApplication::Propagate:
    return true;
  }
  return false;
}

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
  if (std::holds_alternative<BalancedSignatureModelParameters>(step.parameters))
    return step.rule == "Lucky";
  if (std::holds_alternative<FermatModelParameters>(step.parameters))
    return step.rule == "Lucky";
  if (std::holds_alternative<LookupCircuitModelParameters>(step.parameters))
    return step.rule == "Lucky";
  if (std::holds_alternative<SimonModelParameters>(step.parameters))
    return step.rule == "Lucky";
  if (std::holds_alternative<StconnProofParameters>(step.parameters))
    return step.rule == "Lucky";
  if (std::holds_alternative<SignedCardinalityProofParameters>(step.parameters))
    return step.rule == "Lucky";
  if (std::holds_alternative<ExactCoverModelParameters>(step.parameters))
    return step.rule == "Lucky";
  if (std::holds_alternative<IndependentSetModelParameters>(step.parameters))
    return step.rule == "Lucky";
  if (std::holds_alternative<AffineModelParameters>(step.parameters))
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
    if (!valid_post_application(step.post_application))
      return reject("post-application action is invalid");
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
    if (const auto *p =
            std::get_if<BalancedSignatureModelParameters>(&step.parameters);
        p && !p->valid())
      return reject("Lucky balanced-signature-model parameters are invalid");
    if (const auto *p = std::get_if<FermatModelParameters>(&step.parameters);
        p && !p->valid())
      return reject("Lucky fermat-model parameters are invalid");
    if (const auto *p =
            std::get_if<LookupCircuitModelParameters>(&step.parameters);
        p && !p->valid())
      return reject("Lucky lookup-circuit-model parameters are invalid");
    if (const auto *p = std::get_if<SimonModelParameters>(&step.parameters);
        p && !p->valid())
      return reject("Lucky simon-model parameters are invalid");
    if (const auto *p = std::get_if<StconnProofParameters>(&step.parameters);
        p && !p->valid())
      return reject("Lucky stconn-proof parameters are invalid");
    if (const auto *p =
            std::get_if<SignedCardinalityProofParameters>(&step.parameters);
        p && !p->valid())
      return reject("Lucky signed-cardinality-proof parameters are invalid");
    if (const auto *p = std::get_if<ExactCoverModelParameters>(&step.parameters);
        p && !p->valid())
      return reject("Lucky exact-cover-model parameters are invalid");
    if (const auto *p =
            std::get_if<IndependentSetModelParameters>(&step.parameters);
        p && !p->valid())
      return reject("Lucky independent-set-model parameters are invalid");
    if (const auto *p = std::get_if<AffineModelParameters>(&step.parameters);
        p && !p->valid())
      return reject("Lucky affine-model parameters are invalid");
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
    if (!step.condition_instances.empty() && step.rule != "Condition")
      return reject("Condition instances are attached to a different rule");
    // These are external Lit encodings. UINT32_MAX has no DIMACS spelling:
    // externalize() would overflow its signed result before a live-state
    // check, even if the malformed tuple came only from replay text.
    const auto representable = [](const std::vector<Lit> &literals) {
      return std::all_of(literals.begin(), literals.end(), [](Lit literal) {
        return var_of(literal) <
               static_cast<Var>(std::numeric_limits<int>::max());
      });
    };
    for (const ConditionInstance &instance : step.condition_instances) {
      if (!representable(instance.clause) ||
          !representable(instance.witness))
        return reject("Condition instance has an invalid external literal");
    }
    const unsigned structured_payload_kinds =
        !step.factor_grids.empty() +
        !step.substitution_redundancy_instances.empty() +
        !step.condition_instances.empty();
    if (structured_payload_kinds && !step.instances.empty())
      return reject("scalar and structured selections cannot be mixed");
    if (structured_payload_kinds > 1)
      return reject("different structured selection kinds cannot be mixed");
    const size_t structured_size = !step.factor_grids.empty()
                                       ? step.factor_grids.size()
                                       : !step.substitution_redundancy_instances.empty()
                                             ? step.substitution_redundancy_instances.size()
                                             : step.condition_instances.size();
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

bool OnlineStrategyExecutor::pending_atomic_scope() const {
  if (!configured_ || complete_ || step_index_ >= runtime_.size())
    return false;
  const auto &runtime = runtime_[step_index_];
  return runtime.cursor < runtime.selection.size() ||
         (runtime.sr_general_mode && !runtime.sr_general_cursor.complete);
}

bool OnlineStrategyExecutor::renew_atomic_dose(WorkLimit allowance,
                                              uint64_t revision) {
  if (!pending_atomic_scope() || program_.sequence.size() != 1 ||
      program_.repeat.mode != RepeatMode::Once ||
      program_.sequence[0].repeat.mode != RepeatMode::Once)
    return false;
  program_.sequence[0].budget = allowance;
  runtime_[0].consumed = 0;
  revision_ = revision;
  return true;
}

bool OnlineStrategyExecutor::prepare_selection(Kernel &k,
                                               const OnlineInvocation &step,
                                               Runtime &runtime,
                                               InvocationResult &out,
                                               volatile sig_atomic_t *stop,
                                               Allowance *sr_discovery_allowance,
                                               DiscoveryFeedbackPolicy discovery_feedback) {
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
      sr_parameters &&
      sr_parameters->producer ==
          SubstitutionRedundancyProducer::GeneralComplete;
  const bool continuing_general_sr = runtime.sr_general_mode && general_sr;
  runtime.bve_occurrences.reset();
  runtime.selection.clear();
  runtime.structured_selection.clear();
  runtime.sr_selection.clear();
  runtime.cursor = 0;
  if (!continuing_general_sr)
    runtime.pass_progress = false;
  runtime.selection_truncated = false;
  runtime.selection_discovery_exhausted = false;
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
        step.substitution_redundancy_instances.empty() &&
        step.condition_instances.empty()) {
      runtime.selection = step.instances;
    } else {
      const size_t size = !step.factor_grids.empty()
                              ? step.factor_grids.size()
                              : !step.substitution_redundancy_instances.empty()
                                    ? step.substitution_redundancy_instances.size()
                                    : step.condition_instances.size();
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
    SubstitutionRedundancyParameters parameters =
        configured ? *configured : SubstitutionRedundancyParameters{};
    const bool shared_discovery =
        sr_discovery_allowance &&
        parameters.producer != SubstitutionRedundancyProducer::GeneralComplete;
    Allowance unlimited_discovery(WorkUnit::SrDiscoveryTicks,
                                  WorkLimit::unlimited());
    Allowance *discovery_pool = shared_discovery
                                    ? sr_discovery_allowance
                                    : &unlimited_discovery;
    std::span<Allowance *const> discovery_pools(&discovery_pool, 1);
    InvocationBudget discovery_budget(WorkUnit::SrDiscoveryTicks,
                                      WorkLimit::unlimited(),
                                      discovery_pools);
    uint64_t discovery_limit_at_entry = 0;
    uint64_t hard_detection_ticks = 0;
    const bool inline_generator_feedback =
        shared_discovery && discovery_feedback.ticks_per_generator &&
        discovery_feedback.hard_limit > sr_discovery_allowance->consumed();
    const auto graph_entry_reaches_detector = [&] {
      const uint64_t binaries = k.watches.binary_clauses();
      const uint64_t binary_visits =
          binaries > UINT64_MAX / 2 ? UINT64_MAX : 2 * binaries;
      const uint64_t snapshot_lower_bound =
          k.nvars > UINT64_MAX - binary_visits
              ? UINT64_MAX
              : binary_visits + static_cast<uint64_t>(k.nvars);
      // snapshot_formula must visit both watches of every live binary and all
      // variables before graph construction can call the detector.  If a
      // fresh owner-scale window cannot cover even this strict lower bound,
      // granting it can only repeat a partial snapshot and yield no feedback.
      return snapshot_lower_bound <=
             discovery_feedback.ticks_per_generator;
    };
    if (shared_discovery) {
      const BudgetStatus status = discovery_budget.status();
      if (status != BudgetStatus::Ready &&
          status != BudgetStatus::Exhausted) {
        out.status = InvocationStatus::AccountingError;
        out.budget_status = status;
        return false;
      }
      const WorkLimit remaining = discovery_budget.remaining();
      if (!remaining.admits_work()) {
        out.status = InvocationStatus::BudgetExhausted;
        out.budget_status = discovery_budget.status();
        if (parameters.producer ==
                SubstitutionRedundancyProducer::GraphSymmetry &&
            discovery_feedback.ticks_per_generator &&
            discovery_feedback.hard_limit >
                sr_discovery_allowance->consumed() &&
            !runtime.graph_entry_requested &&
            graph_entry_reaches_detector()) {
          runtime.graph_entry_requested = true;
          out.discovery_entry_required = true;
        }
        return false;
      }
      if (!remaining.is_unlimited())
        parameters.maximum_detection_ticks =
            std::min(parameters.maximum_detection_ticks, remaining.amount());
      if (!remaining.is_unlimited() &&
          remaining.amount() <= UINT64_MAX - sr_discovery_allowance->consumed())
        discovery_limit_at_entry =
            sr_discovery_allowance->consumed() + remaining.amount();
      if (inline_generator_feedback)
        hard_detection_ticks =
            discovery_feedback.hard_limit - sr_discovery_allowance->consumed();
    }
    if (parameters.producer ==
        SubstitutionRedundancyProducer::GeneralComplete) {
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
    } else if (parameters.producer ==
               SubstitutionRedundancyProducer::Orbitopal) {
      const OrbitopalEnumerationResult enumeration =
          substitution_redundancy_enumerate_orbitopal(
              k, parameters,
              [&](const SubstitutionRedundancyInstance &instance) {
                runtime.sr_selection.push_back(instance);
                return true;
              },
              stop);
      out.budget_status = discovery_budget.charge(
          WorkUnit::SrDiscoveryTicks, enumeration.ticks);
      if (out.budget_status == BudgetStatus::AccountingOverflow) {
        out.status = InvocationStatus::AccountingError;
        return false;
      }
      if (enumeration.status == OrbitopalEnumerationStatus::Interrupted) {
        out.status = InvocationStatus::Interrupted;
        return false;
      }
      runtime.selection_truncated =
          enumeration.status == OrbitopalEnumerationStatus::LimitReached;
      // Orbitopal sub-strategies share one structured detector family.  A
      // bounded empty prefix is not evidence that paying another complete
      // snapshot for the next orientation is useful.  Formula progress still
      // renews the shared allowance.  The producer-boundary opportunity below
      // is reserved for the distinct generic graph detector, matching the
      // owning structured-then-generic schedule.
      runtime.selection_discovery_exhausted = false;
    } else if (parameters.producer ==
               SubstitutionRedundancyProducer::GraphSymmetry) {
      const SymmetryEnumerationResult enumeration =
          substitution_redundancy_enumerate_symmetry(
              k, parameters,
              [&](const SubstitutionRedundancyInstance &instance) {
                runtime.sr_selection.push_back(instance);
                return true;
              },
              stop, nullptr,
              SymmetryEnumerationScheduling{
                  shared_discovery,
                  inline_generator_feedback
                      ? discovery_feedback.ticks_per_generator
                      : 0,
                  hard_detection_ticks});
      if (inline_generator_feedback &&
          enumeration.discovery_progress_units) {
        const uint64_t units = enumeration.discovery_progress_units;
        const uint64_t credit =
            units > UINT64_MAX / discovery_feedback.ticks_per_generator
                ? UINT64_MAX
                : units * discovery_feedback.ticks_per_generator;
        uint64_t renewed =
            credit > UINT64_MAX - discovery_limit_at_entry
                ? UINT64_MAX
                : discovery_limit_at_entry + credit;
        renewed = std::min(renewed, discovery_feedback.hard_limit);
        sr_discovery_allowance->set_limit(WorkLimit::bounded(renewed));
      }
      out.budget_status = discovery_budget.charge(
          WorkUnit::SrDiscoveryTicks, enumeration.ticks);
      if (enumeration.nonforcing_orbit_cuts_declined) {
        ++k.stats.sr_nonforcing_cut_batches_declined;
        k.stats.sr_nonforcing_cuts_declined +=
            enumeration.nonforcing_orbit_cuts_declined;
      }
      if (enumeration.singleton_forcing_rows_declined) {
        ++k.stats.sr_singleton_forcing_batches_declined;
        k.stats.sr_singleton_forcing_rows_declined +=
            enumeration.singleton_forcing_rows_declined;
      }
      if (out.budget_status == BudgetStatus::AccountingOverflow) {
        out.status = InvocationStatus::AccountingError;
        return false;
      }
      if (enumeration.discovery_progress_units &&
          !inline_generator_feedback) {
        // The generic callback prefix is exact but intentionally unpublished:
        // its callback-order subgroup is incomplete. Leave this step pending
        // so the root scheduler can reward the measured producer progress and
        // retry with a larger shared allowance.
        runtime.sr_selection.clear();
        out.status = InvocationStatus::BudgetExhausted;
        out.budget_status = BudgetStatus::Exhausted;
        out.discovery_progress_units = enumeration.discovery_progress_units;
        out.step_id = step.id;
        return false;
      }
      if (enumeration.status == SymmetryEnumerationStatus::Interrupted) {
        out.status = InvocationStatus::Interrupted;
        return false;
      }
      if (enumeration.status == SymmetryEnumerationStatus::Invalid) {
        out.status = InvocationStatus::InvalidRequest;
        return false;
      }
      if (enumeration.status ==
              SymmetryEnumerationStatus::ResourceExhausted ||
          enumeration.status == SymmetryEnumerationStatus::Failed) {
        out.status = InvocationStatus::ResourceExhausted;
        return false;
      }
      runtime.selection_truncated =
          enumeration.status == SymmetryEnumerationStatus::LimitReached;
      runtime.selection_discovery_exhausted =
          shared_discovery && runtime.selection_truncated;
      const uint64_t discovery_consumed_before =
          shared_discovery ? sr_discovery_allowance->consumed() : 0;
      const uint64_t discovery_consumed_after =
          enumeration.ticks >
                  UINT64_MAX - discovery_consumed_before
              ? UINT64_MAX
              : discovery_consumed_before + enumeration.ticks;
      if (inline_generator_feedback && runtime.selection_truncated &&
          runtime.sr_selection.empty() &&
          !enumeration.discovery_progress_units &&
          !runtime.graph_entry_requested &&
          graph_entry_reaches_detector() &&
          discovery_feedback.hard_limit >
              discovery_consumed_after) {
        runtime.graph_entry_requested = true;
        out.status = InvocationStatus::BudgetExhausted;
        out.budget_status = BudgetStatus::Exhausted;
        out.discovery_entry_required = true;
        out.step_id = step.id;
        return false;
      }
    }
    runtime.selection.resize(runtime.sr_selection.size());
    for (size_t i = 0; i < runtime.selection.size(); ++i)
      runtime.selection[i] = i;
  } else if (step.rule == "BVE") {
    const auto *configured = std::get_if<BveParameters>(&step.parameters);
    const BveParameters parameters =
        configured ? *configured : BveParameters::from_kernel(k);
    if ((stop && *stop) || (k.stop && *k.stop)) {
      out.status = InvocationStatus::Interrupted;
      return false;
    }
    bve_complete_instances(k, rule, parameters, runtime.selection);
    if ((stop && *stop) || (k.stop && *k.stop)) {
      runtime.selection.clear();
      out.status = InvocationStatus::Interrupted;
      return false;
    }
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

bool OnlineStrategyExecutor::finish_step_pass(const Kernel &k,
                                              const OnlineInvocation &step,
                                              Runtime &runtime,
                                              InvocationResult &out) {
  runtime.selection_ready = false;
  runtime.bve_occurrences.reset();
  runtime.selection.clear();
  runtime.structured_selection.clear();
  runtime.sr_selection.clear();
  runtime.sr_general_cursor = {};
  runtime.sr_general_mode = false;
  runtime.selection_truncated = false;
  runtime.selection_discovery_exhausted = false;
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
  case RepeatMode::UntilSizeScaledFixpoint:
    if (!progress) {
      out.status = InvocationStatus::ScopedFixpoint;
      out.selection_complete = true;
    } else if (const uint64_t limit = repeat_round_limit(k, step.repeat);
               !limit || runtime.passes < limit) {
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

bool OnlineStrategyExecutor::finish_sequence(const Kernel &k,
                                             InvocationResult &out) {
  ++sequence_passes_;
  bool repeat = false;
  switch (program_.repeat.mode) {
  case RepeatMode::Once:
    break;
  case RepeatMode::Count:
    repeat = sequence_passes_ < program_.repeat.limit;
    break;
  case RepeatMode::UntilScopedFixpoint:
  case RepeatMode::UntilSizeScaledFixpoint:
    if (!sequence_progress_) {
      out.status = InvocationStatus::ScopedFixpoint;
      out.selection_complete = true;
    } else if (const uint64_t limit = repeat_round_limit(k, program_.repeat);
               !limit || sequence_passes_ < limit) {
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
      runtime.selection_discovery_exhausted = false;
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
    Kernel &k, volatile sig_atomic_t *stop,
    Allowance *sr_discovery_allowance,
    DiscoveryFeedbackPolicy discovery_feedback) {
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
      if (finish_sequence(k, out))
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
      if (!prepare_selection(k, step, runtime, out, stop,
                             sr_discovery_allowance, discovery_feedback)) {
        out.after = state_version(k);
        return out;
      }
      runtime.pass_progress |= progress;
    }
    if (!runtime.selection_ready) {
      if (!prepare_selection(k, step, runtime, out, stop,
                             sr_discovery_allowance, discovery_feedback)) {
        // An inapplicable scope is not a fixpoint and is not consumed. The
        // caller may change solver state or replace the program and retry.
        out.after = state_version(k);
        return out;
      }
      if (runtime.selection.empty()) {
        if (runtime.selection_truncated) {
          out.status = InvocationStatus::BudgetExhausted;
          out.budget_status = BudgetStatus::Exhausted;
          out.discovery_exhausted = runtime.selection_discovery_exhausted;
          out.selection_complete = false;
          out.step_id = step.id;
          out.after = state_version(k);
          return out;
        }
        finish_step_pass(k, step, runtime, out);
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
        if (!prepare_selection(k, step, runtime, out, stop,
                               sr_discovery_allowance, discovery_feedback)) {
          out.after = state_version(k);
          return out;
        }
        continue;
      }
      if (runtime.selection_truncated) {
        out.status = InvocationStatus::BudgetExhausted;
        out.budget_status = BudgetStatus::Exhausted;
        out.discovery_exhausted = runtime.selection_discovery_exhausted;
        out.selection_complete = false;
        out.step_id = step.id;
        out.after = state_version(k);
        return out;
      }
      finish_step_pass(k, step, runtime, out);
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
    const size_t trail_before = k.trail.size();
    InvocationBudget budget(step.work_unit, allowance);
    bool consume_generated_sr_noop = false;
    bool discard_generated_sr_batch = false;
    bool preserve_affine_selection = false;
    const uint64_t affine_budget_before = k.stats.affine_model_budget;
    const uint64_t affine_interrupt_before = k.stats.affine_model_interrupted;
    const uint64_t affine_unavailable_before = k.stats.affine_model_unavailable;
    ScopedRuleMaterialSettings material(k, step.material);
    // Attach only for a generated pure-BVE pass and its atomic propagation
    // follow-up. No index survives at the public boundary or changes another
    // rule's eligibility. Borrow an existing caller-owned index unchanged.
    std::optional<BveOccurrenceCache::Scope> bve_occurrences;
    if (step.rule == "BVE" &&
        step.granularity == InvocationGranularity::CompleteCurrentPass) {
      const auto *parameters = std::get_if<BveParameters>(&step.parameters);
      if (parameters && pure_bve_candidate_scope(parameters->candidate_scope))
        bve_occurrences.emplace(runtime.bve_occurrences, k);
    }
    const bool generated_sr_pass =
        step.rule == "SubstitutionRedundancy" &&
        step.granularity == InvocationGranularity::CompleteCurrentPass;
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
      std::optional<ScopedBalancedSignatureModelParameters>
          balanced_signature_model;
      std::optional<ScopedFermatModelParameters> fermat_model;
      std::optional<ScopedLookupCircuitModelParameters> lookup_circuit_model;
      std::optional<ScopedSimonModelParameters> simon_model;
      std::optional<ScopedStconnProofParameters> stconn_proof;
      std::optional<ScopedSignedCardinalityProofParameters>
          signed_cardinality_proof;
      std::optional<ScopedExactCoverModelParameters> exact_cover_model;
      std::optional<ScopedIndependentSetModelParameters> independent_set_model;
      std::optional<ScopedAffineModelParameters> affine_model;
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
      else if (const auto *p = std::get_if<
                   BalancedSignatureModelParameters>(&step.parameters))
        balanced_signature_model.emplace(k, *p);
      else if (const auto *p =
                   std::get_if<FermatModelParameters>(&step.parameters))
        fermat_model.emplace(k, *p);
      else if (const auto *p =
                   std::get_if<LookupCircuitModelParameters>(&step.parameters))
        lookup_circuit_model.emplace(k, *p);
      else if (const auto *p =
                   std::get_if<SimonModelParameters>(&step.parameters))
        simon_model.emplace(k, *p);
      else if (const auto *p =
                   std::get_if<StconnProofParameters>(&step.parameters))
        stconn_proof.emplace(k, *p);
      else if (const auto *p =
                   std::get_if<SignedCardinalityProofParameters>(
                       &step.parameters))
        signed_cardinality_proof.emplace(k, *p);
      else if (const auto *p =
                   std::get_if<ExactCoverModelParameters>(&step.parameters))
        exact_cover_model.emplace(k, *p);
      else if (const auto *p =
                   std::get_if<IndependentSetModelParameters>(&step.parameters))
        independent_set_model.emplace(k, *p);
      else if (const auto *p = std::get_if<AffineModelParameters>(&step.parameters))
        affine_model.emplace(k, *p);
      // Invocation-level native work limits override a rule parameter which
      // happens to share the legacy Kernel::work_budget handoff (Walk).  Both
      // scopes still unwind to the caller profile after this atomic apply.
      ScopedRuleWorkBudget rule_work_budget(k, step.rule_work_budget);
      const bool enumerated_factor_grid =
          step.rule == "Factor" &&
          step.granularity == InvocationGranularity::CompleteCurrentPass;
      const bool enumerated_sr =
          generated_sr_pass;
      std::optional<SubstitutionRedundancyApplicationResult> sr_application;
      bool condition_declined = false;
      bool condition_interrupted = false;
      const ApplyResult result =
          !step.condition_instances.empty()
              ? [&] {
                  const ConditionInstance &request =
                      step.condition_instances[payload];
                  ConditionInstance selected = request;
                  for (Lit &literal : selected.clause) {
                    const Var external = var_of(literal);
                    if (external >= k.external_to_internal.size() ||
                        k.external_to_internal[external] == INVALID_VAR) {
                      condition_declined = true;
                      return ApplyResult::NoOp;
                    }
                    literal = make_lit(k.external_to_internal[external],
                                       negated(literal));
                  }
                  ConditionPrepared prepared;
                  if (!condition_prepare(k, selected, prepared)) {
                    condition_declined = true;
                    condition_interrupted =
                        (stop && *stop) || (k.stop && *k.stop);
                    return ApplyResult::NoOp;
                  }
                  return apply_rule_structured(rule, k, "exact-condition", [&] {
                    return condition_commit(k, selected, prepared);
                  });
                }()
          : (!step.substitution_redundancy_instances.empty() || enumerated_sr)
              ? apply_rule_structured(rule, k, "exact-sr", [&] {
                  const auto *configured =
                      std::get_if<SubstitutionRedundancyParameters>(
                          &step.parameters);
                  const SubstitutionRedundancyParameters parameters =
                      configured ? *configured
                                 : SubstitutionRedundancyParameters{};
                  const SubstitutionRedundancyInstance &selected =
                      !step.substitution_redundancy_instances.empty()
                          ? step.substitution_redundancy_instances[payload]
                          : runtime.sr_selection[payload];
                  SubstitutionRedundancyInstance reoriented;
                  const SubstitutionRedundancyInstance *instance = &selected;
                  if (enumerated_sr &&
                      selected.unique_literal_amo_certificate &&
                      selected.clause.size() == 2 &&
                      k.value(selected.clause[0]) < 0 &&
                      k.value(selected.clause[1]) >= 0) {
                    // ULC discovery certifies either member as the SR pivot.
                    // Satsuma publishes the whole AMO before propagation, but
                    // the online executor observes root consequences between
                    // atomic members. Keep caller-owned ordering observable;
                    // only the private generated batch turns a now-false
                    // pivot around so the still-open endpoint can realize the
                    // same add-then-propagate closure.
                    reoriented = selected;
                    std::swap(reoriented.clause[0], reoriented.clause[1]);
                    instance = &reoriented;
                  }
                  sr_application =
                      enumerated_sr &&
                              (instance->orbitopal_prefix_certificate ||
                               instance->symmetry_chain_certificate ||
                               instance->unique_literal_amo_certificate)
                          ? substitution_redundancy_apply_orbitopal_prefix(
                                k, *instance, parameters, stop)
                          : substitution_redundancy_apply_detailed(
                                k, *instance, parameters, stop);
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
          : execution::execute_action(
                k, execution::RegisteredRuleRequest{rule, payload});
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
          if (enumerated_sr) {
            out.status = InvocationStatus::NoChange;
            consume_generated_sr_noop = true;
          } else
            out.status = runtime.pass_progress
                             ? InvocationStatus::StaleRequest
                             : InvocationStatus::InvalidRequest;
          break;
        case SubstitutionRedundancyApplicationStatus::Inapplicable:
          if (enumerated_sr &&
              runtime.sr_selection[payload].symmetry_chain_certificate) {
            // A later Schreier member is certified only for the contiguous
            // prefix selected from the same snapshot. If that obligation ever
            // fails, discard the rest of this snapshot.  A repeated step may
            // enumerate again from the strengthened live state; it must never
            // skip the failed premise and continue through the old batch.
            out.status = InvocationStatus::NoChange;
            discard_generated_sr_batch = true;
          } else if (enumerated_sr) {
            out.status = InvocationStatus::NoChange;
            consume_generated_sr_noop = true;
          } else
            out.status = InvocationStatus::Inapplicable;
          break;
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
        if (consume_generated_sr_noop) {
          out.work = 0;
          out.applications = 0;
          out.budget_status = BudgetStatus::Ready;
        }
      }
      if (step.rule == "Lucky" && payload == 8 && result == ApplyResult::NoOp) {
        // An unfinished private model search is not a scoped fixpoint. Keep
        // its explicit selection pending for reconfiguration; attempted work
        // stays charged even though its private candidate state was discarded.
        if (k.stats.affine_model_interrupted != affine_interrupt_before)
          out.status = InvocationStatus::Interrupted;
        else if (k.stats.affine_model_budget != affine_budget_before) {
          out.status = InvocationStatus::BudgetExhausted;
          out.budget_status = BudgetStatus::Exhausted;
        } else if (k.stats.affine_model_unavailable != affine_unavailable_before)
          out.status = InvocationStatus::Inapplicable;
        preserve_affine_selection = out.status == InvocationStatus::Interrupted ||
            out.status == InvocationStatus::BudgetExhausted ||
            out.status == InvocationStatus::Inapplicable;
        if (preserve_affine_selection) out.selection_complete = false;
      }
      out.after = state_version(k);
      if (condition_declined) {
        // A selected cube is checked against the live formula at application
        // time. A later batch member may lose its premise after earlier
        // progress; never count or advance that stale selection.
        out.status = condition_interrupted
                         ? InvocationStatus::Interrupted
                         : runtime.pass_progress
                               ? InvocationStatus::StaleRequest
                               : InvocationStatus::Inapplicable;
        out.work = out.applications = 0;
        out.selection_complete = false;
        out.budget_status = BudgetStatus::Ready;
        return out;
      }
    }
    // Keep the selected follow-up inside the same executor transaction.  A
    // caller-side propagation here would change touch/strategy epochs after
    // expected_state was recorded and correctly make the retained batch stale.
    // The follow-up is scheduling scaffolding: only the primary rule decides
    // whether this pass made progress for repeat-to-fixpoint.
    if (step.post_application == PostApplication::Propagate &&
        out.status != InvocationStatus::Unsat &&
        out.status != InvocationStatus::Sat &&
        out.status != InvocationStatus::Conflict &&
        out.status != InvocationStatus::Interrupted &&
        out.status != InvocationStatus::ResourceExhausted &&
        propagate_guard(k)) {
      const int propagation_id = tactix_rule_id("Propagate");
      assert(propagation_id >= 0);
      const ApplyResult propagated =
          execution::execute_action(
              k, execution::RegisteredRuleRequest{
                     tactix_rules()[static_cast<size_t>(propagation_id)]});
      if (propagated == ApplyResult::Unsat) {
        out.status = InvocationStatus::Unsat;
        out.rule_result = propagated;
      } else if (propagated == ApplyResult::Conflict) {
        out.status = InvocationStatus::Conflict;
        out.rule_result = propagated;
      }
    }
    // Online execution can run with DefaultPolicy's H already attached.  Root
    // units produced directly or by the configured follow-up must leave its
    // queue and heap before the next composed action observes the state.
    heuristics_consume_root_assignments(k, trail_before);
    out.after = state_version(k);
    runtime.expected_state = state_version(k);
    // Generated SR payloads contain only stable literal identities. A prior
    // member may collect/rebuild clause storage while reaching root. Ordinary
    // members are checked against that strengthened formula; a certified
    // orbitopal member already carries a base automorphism that preserves its
    // complete preceding unit prefix. The expected-state check above rejects
    // external transitions. Preserve the batch instead of repeating expensive
    // structured discovery after each successful unit.
    if (generated_sr_pass)
      runtime.selection_generation = k.arena_generation;
    runtime.consumed = saturated_add(runtime.consumed, out.work);
    runtime.pass_progress |= out.made_rule_progress;
    sequence_progress_ |= out.made_rule_progress;
    if (discard_generated_sr_batch)
      runtime.cursor = runtime.selection.size();
    else if (!preserve_affine_selection && (out.applications || consume_generated_sr_noop))
      ++runtime.cursor;
    out.configuration_revision = revision_;
    out.step_id = step.id;
    out.after = state_version(k);
    return out;
  }
}

} // namespace tactix::strategy
