#pragma once

#include "strategy/policy_completion.hpp"
#include "strategy/lucky_execution.hpp"

#include <string_view>

namespace tactix::execution {

inline strategy::controller::FeedbackOutcome native_rule_outcome(
    const Kernel &k, const char *rule, ApplyResult result) {
  auto out = strategy::controller::feedback_outcome(result);
  if (result == ApplyResult::Sat || result == ApplyResult::Unsat ||
      result == ApplyResult::Conflict) return out;
  const std::string_view name(rule);
  const bool incomplete = (name == "Factor" && !k.factor_completed) ||
      (name == "Strengthen" && !k.strengthen_complete) ||
      (name == "SubsumeForward" && !k.subsume_complete) ||
      (name == "Sweep" && k.sweep_incomplete);
  out.invocation.selection_complete = !incomplete;
  if (kernel_terminating(k)) out.invocation.status = strategy::InvocationStatus::Interrupted;
  else if (incomplete) {
    out.invocation.status = strategy::InvocationStatus::BudgetExhausted;
    out.invocation.budget_status = strategy::BudgetStatus::Exhausted;
  }
  return out;
}

// Describe the live profile without installing it or enumerating instances.
// Native-only composed payloads (victim lists, warm-up, rotation) are supplied
// by the caller; this helper describes registered atomic rule requests.
inline NativeFeedbackSpec native_rule_feedback(
    const Kernel &k, const char *rule, uint64_t instance,
    std::string description,
    uint64_t (*meter)(const Kernel &) = nullptr,
    std::string currency = "applications",
    strategy::WorkLimit allowance = strategy::WorkLimit::unlimited()) {
  NativeFeedbackSpec spec;
  spec.meter = meter;
  spec.description.description = std::move(description);
  spec.description.effort_currency = std::move(currency);
  spec.description.effort_allowance = allowance;
  auto &inv = spec.description.invocation.emplace();
  inv.rule = rule;
  inv.granularity = instance == INSTANCE_CANONICAL
      ? strategy::InvocationGranularity::CanonicalApplication
      : strategy::InvocationGranularity::OneInstance;
  if (instance != INSTANCE_CANONICAL) inv.instances.push_back(instance);
  using namespace strategy;
  const std::string_view name(rule);
  if (name == "ConflictAnalysis") inv.parameters = AnalyzeParameters::from_kernel(k);
  else if (name == "BVE") inv.parameters = BveParameters::from_kernel(k);
  else if (name == "SubsumeForward") inv.parameters = k.subsume_forward_parameters;
  else if (name == "Strengthen") inv.parameters = k.strengthen_parameters;
  else if (name == "Congruence") inv.parameters = k.congruence_parameters;
  else if (name == "Instantiate") inv.parameters = k.instantiate_parameters;
  else if (name == "Backbone") inv.parameters = k.backbone_parameters;
  else if (name == "Vivify") inv.parameters = VivifyParameters::from_kernel(k);
  else if (name == "BCE") inv.parameters = BceParameters::from_kernel(k);
  else if (name == "HyperTernaryRes") inv.parameters = TernaryParameters::from_kernel(k);
  else if (name == "Factor") inv.parameters = FactorParameters::from_kernel(k);
  else if (name == "TransitiveReduce") inv.parameters = TransitiveReduceParameters::from_kernel(k);
  else if (name == "Substitute") inv.parameters = SubstituteParameters::from_kernel(k);
  else if (name == "Reduce") inv.parameters = ReduceParameters::from_kernel(k);
  else if (name == "Walk") inv.parameters = WalkParameters::from_kernel(k);
  else if (name == "Reorder") inv.parameters = ReorderParameters::from_kernel(k);
  else if (name == "Lucky") {
    if (instance == 6 || instance == LUCKY_STARTUP_PARITY)
      inv.parameters = ParityModelParameters::from_kernel(k);
    else if (instance == 7 || instance == 9)
      inv.parameters = OnehotModelParameters::from_kernel(k);
    else if (instance == 8) inv.parameters = AffineModelParameters::from_kernel(k);
    else if (instance == 10) inv.parameters = BalancedSignatureModelParameters::from_kernel(k);
    else if (instance == 11) inv.parameters = FermatModelParameters::from_kernel(k);
    else if (instance == 13) inv.parameters = LookupCircuitModelParameters::from_kernel(k);
    else if (instance == 14) inv.parameters = SimonModelParameters::from_kernel(k);
    else if (instance == 15) inv.parameters = StconnProofParameters::from_kernel(k);
    else if (instance == 16) inv.parameters = SignedCardinalityProofParameters::from_kernel(k);
    else if (instance == 17) inv.parameters = ExactCoverModelParameters::from_kernel(k);
    else if (instance == 18) inv.parameters = IndependentSetModelParameters::from_kernel(k);
  }
  return spec;
}

template <LuckyOpportunity Opportunity>
constexpr uint64_t lucky_feedback_instance() {
  if constexpr (Opportunity == LuckyOpportunity::Cascade) return INSTANCE_CANONICAL;
  else if constexpr (Opportunity == LuckyOpportunity::StartupParity) return LUCKY_STARTUP_PARITY;
  else if constexpr (Opportunity == LuckyOpportunity::StartupCount) return LUCKY_STARTUP_COUNT;
  else return 5 + static_cast<uint64_t>(Opportunity);
}

} // namespace tactix::execution
