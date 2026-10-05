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
  // Reading completion metadata must never poll kernel_terminating(): the
  // interruption-fuzz mode deliberately advances state at each native poll.
  if (k.stop && *k.stop) out.invocation.status = strategy::InvocationStatus::Interrupted;
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
  else if (name == "ParityConsequence") inv.parameters = k.parity_consequence_parameters;
  else if (name == "AffineConsequence") inv.parameters = k.affine_consequence_parameters;
  else if (name == "ExactCoverExtension") inv.parameters = k.exact_cover_extension_parameters;
  else if (name == "IndependentSetCover") inv.parameters = k.independent_set_cover_parameters;
  else if (name == "Gf3AffineConsequence") inv.parameters = k.gf3_affine_consequence_parameters;
  else if (name == "DirectCspSupport") inv.parameters = k.direct_csp_support_parameters;
  else if (name == "DirectPhpRefutation") inv.parameters = k.direct_php_refutation_parameters;
  else if (name == "Gf3GaussianConsequence") inv.parameters = k.gf3_gaussian_consequence_parameters;
  else if (name == "TseitinCycleBreaking") inv.parameters = k.tseitin_cycle_breaking_parameters;
  else if (name == "SemanticVariableAdvice") inv.parameters = k.semantic_variable_advice_parameters;
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
    if (instance == 6 || instance == LUCKY_PARITY_PROOF ||
        instance == LUCKY_STARTUP_PARITY)
      inv.parameters = ParityModelParameters::from_kernel(k);
    else if (instance == 7 || instance == 9 ||
             instance == LUCKY_EXACT_ONE_PROOF)
      inv.parameters = OnehotModelParameters::from_kernel(k);
    else if (instance == 8)
      inv.parameters = AffineModelParameters::from_kernel(k);
    else if (instance == 10)
      inv.parameters = BalancedSignatureModelParameters::from_kernel(k);
    else if (instance == 11)
      inv.parameters = FermatModelParameters::from_kernel(k);
    else if (instance == 13)
      inv.parameters = LookupCircuitModelParameters::from_kernel(k);
    else if (instance == 14)
      inv.parameters = SimonModelParameters::from_kernel(k);
    else if (instance == 15) inv.parameters = StconnProofParameters::from_kernel(k);
    else if (instance == 16) inv.parameters = SignedCardinalityProofParameters::from_kernel(k);
    else if (instance == 17) inv.parameters = ExactCoverModelParameters::from_kernel(k);
    else if (instance == 18) inv.parameters = IndependentSetModelParameters::from_kernel(k);
    else if (instance == LUCKY_GRAPH_LABELING) {
      const auto p = GraphLabelingParameters::from_kernel(k);
      inv.parameters = p;
      spec.meter = [](const Kernel &state) { return state.stats.graph_labeling_work; };
      spec.description.effort_currency = "graph-work";
      uint64_t bound = 0;
      for (uint64_t allowance : p.work) bound += allowance;
      if (kernel_has_work_budget(k)) bound = std::min(bound, k.work_budget);
      spec.description.effort_allowance = WorkLimit::bounded(bound);
    }
    else if (instance == LUCKY_NOL_HYPERGRAPH) {
      spec.meter = [](const Kernel &state) {
        return state.stats.nol_hypergraph_model_work;
      };
      spec.description.effort_currency = "hypergraph-work";
      if (kernel_has_work_budget(k))
        spec.description.effort_allowance = WorkLimit::bounded(k.work_budget);
    }
    else if (instance == LUCKY_STARTUP_RAMSEY) {
      const auto p = RamseyStartupParameters::from_kernel(k);
      inv.parameters = p;
      spec.meter = [](const Kernel &state) {
        return state.stats.ramsey_startup_recognition_work +
               state.stats.ramsey_startup_certificate_work;
      };
      spec.description.effort_currency = "ramsey-work";
      uint64_t bound=p.valid() ? p.recognition_work + p.certificate_work : 0;
      if(kernel_has_work_budget(k)) bound=std::min(bound,k.work_budget);
      spec.description.effort_allowance = WorkLimit::bounded(bound);
    }
  }
  return spec;
}

template <LuckyOpportunity Opportunity>
constexpr uint64_t lucky_feedback_instance() {
  if constexpr (Opportunity == LuckyOpportunity::Cascade) return INSTANCE_CANONICAL;
  else if constexpr (Opportunity == LuckyOpportunity::StartupParity) return LUCKY_STARTUP_PARITY;
  else if constexpr (Opportunity == LuckyOpportunity::StartupCount) return LUCKY_STARTUP_COUNT;
  else if constexpr (Opportunity == LuckyOpportunity::StartupRamsey) return LUCKY_STARTUP_RAMSEY;
  else return 5 + static_cast<uint64_t>(Opportunity);
}

} // namespace tactix::execution
