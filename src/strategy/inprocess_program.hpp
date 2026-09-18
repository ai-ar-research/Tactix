#pragma once

#include "strategy/analyze_parameters.hpp"
#include "strategy/bve_invocation.hpp"
#include "strategy/congruence_parameters.hpp"
#include "strategy/forward_parameters.hpp"
#include "strategy/failed_literal_parameters.hpp"
#include "strategy/instantiate_parameters.hpp"
#include "strategy/rule_parameters.hpp"
#include "strategy/sweep_parameters.hpp"
#include "strategy/vivify_parameters.hpp"
#include "rules/substitution_redundancy/substitution_redundancy.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace tactix::strategy {

using StepId = uint64_t;

enum class InprocessPhase : uint8_t { Probe, Eliminate };

enum class Followup : uint8_t {
  None,
  AfterCongruence,
  AfterSweepOrFinal,
};

enum class CandidateOrder : uint8_t {
  Native,
  Reverse,
  Random,
  OccAsc,
  OccDesc,
  NegOccDesc,
  Heuristic
};

constexpr bool valid_candidate_order(CandidateOrder order) {
  return order >= CandidateOrder::Native && order <= CandidateOrder::Heuristic;
}

using ActionParameters =
    std::variant<std::monostate, AnalyzeParameters, BveParameters,
                 SubsumeForwardParameters, StrengthenParameters,
                 CongruenceParameters, InstantiateParameters,
                 BackboneParameters, SweepParameters, VivifyParameters,
                 BceParameters, TernaryParameters, FactorParameters,
                 TransitiveReduceParameters, SubstituteParameters,
                 ReduceParameters, WalkParameters, ReorderParameters,
                 ParityModelParameters, OnehotModelParameters,
                 BalancedSignatureModelParameters, FermatModelParameters,
                 LookupCircuitModelParameters, SimonModelParameters,
                 StconnProofParameters,
                 AffineModelParameters,
                 SubstitutionRedundancyParameters>;

// Compatibility name for the inprocessing scheduler. Online composition uses
// the same typed parameter vocabulary for every registered action.
using InprocessParameters = ActionParameters;

// One occurrence in an inprocessing strategy.  Rule names are resolved and
// validated when a strategy attaches to a registry, while StepId distinguishes
// repeated occurrences of the same rule and therefore their runtime state.
struct InprocessStep {
  InprocessStep(StepId step_id, std::string rule_name,
                InprocessPhase step_phase, bool step_enabled = true,
                bool step_preprocessing = false,
                Followup step_followup = Followup::None,
                InprocessParameters step_parameters = {})
      : id(step_id), rule(std::move(rule_name)), phase(step_phase),
        enabled(step_enabled), preprocessing(step_preprocessing),
        followup(step_followup), parameters(std::move(step_parameters)) {}

  StepId id;
  std::string rule;
  InprocessPhase phase;
  bool enabled = true;
  bool preprocessing = false;
  Followup followup = Followup::None;
  // Absence means inherit the owning strategy profile. The tagged alternative
  // makes cross-rule parameter attachment invalid rather than silently inert.
  InprocessParameters parameters;
  // Per-occurrence scheduler choices. Empty values inherit the rule's typed
  // profile and native traversal. Zero max_applications means unlimited.
  std::optional<uint32_t> effort_permille;
  uint32_t max_applications = 0;
  std::optional<bool> cursor_enabled;
  std::optional<CandidateOrder> candidate_order;
};

inline std::vector<InprocessStep> reference_inprocess_program() {
  using enum Followup;
  using enum InprocessPhase;
  std::vector<InprocessStep> result = {
      {1, "Congruence", Probe, true, true, None},
      {2, "Substitute", Probe, true, true, AfterCongruence},
      {3, "HyperBinaryRes", Probe, false},
      {4, "Backbone", Probe, true, true, None},
      {5, "Probe", Probe, false},
      {6, "Vivify", Probe, true},
      {7, "Sweep", Probe, true, true, None},
      {8, "Substitute", Probe, true, true, AfterSweepOrFinal},
      {9, "TransitiveReduce", Probe, true},
      {10, "HyperTernaryRes", Probe, false},
      {11, "Backbone", Probe, true},
      {12, "Factor", Probe, true, true, None},
      {13, "SubsumeForward", Eliminate, false},
      {14, "Strengthen", Eliminate, true},
      {15, "BVE", Eliminate, true},
      {16, "Instantiate", Eliminate, false},
      {17, "BCE", Eliminate, false},
      {18, "CCE", Eliminate, false},
  };
  return result;
}

inline bool validate_inprocess_program(std::span<const InprocessStep> program,
                                       std::string *error = nullptr) {
  const auto reject = [error](const char *message) {
    if (error)
      *error = message;
    return false;
  };
  for (size_t i = 0; i < program.size(); ++i) {
    const InprocessStep &step = program[i];
    if (!step.id)
      return reject("StepId zero is reserved");
    if (step.rule.empty())
      return reject("rule name is empty");
    if (step.phase != InprocessPhase::Probe &&
        step.phase != InprocessPhase::Eliminate)
      return reject("phase is invalid");
    if (step.followup != Followup::None && !step.preprocessing)
      return reject("a preprocessing follow-up must itself be preprocessing");
    if (step.followup != Followup::None &&
        step.followup != Followup::AfterCongruence &&
        step.followup != Followup::AfterSweepOrFinal)
      return reject("follow-up kind is invalid");
    if (step.candidate_order && !valid_candidate_order(*step.candidate_order))
      return reject("candidate order is invalid");
    if (const auto *p = std::get_if<AnalyzeParameters>(&step.parameters)) {
      if (step.rule != "ConflictAnalysis")
        return reject("Analyze parameters are attached to a different rule");
      if (!p->valid())
        return reject("Analyze parameters are invalid");
    } else if (const auto *p = std::get_if<BveParameters>(&step.parameters)) {
      if (step.rule != "BVE")
        return reject("BVE parameters are attached to a different rule");
      if (!p->valid())
        return reject("BVE parameters are invalid");
    } else if (const auto *p =
                   std::get_if<SubsumeForwardParameters>(&step.parameters)) {
      if (step.rule != "SubsumeForward")
        return reject("C1 parameters are attached to a different rule");
      if (!p->valid())
        return reject("C1 parameters are invalid");
    } else if (const auto *p =
                   std::get_if<StrengthenParameters>(&step.parameters)) {
      if (step.rule != "Strengthen")
        return reject("C2 parameters are attached to a different rule");
      if (!p->valid())
        return reject("C2 parameters are invalid");
    } else if (const auto *p =
                   std::get_if<CongruenceParameters>(&step.parameters)) {
      if (step.rule != "Congruence")
        return reject("Congruence parameters are attached to a different rule");
      if (!p->valid())
        return reject("Congruence parameters are invalid");
    } else if (const auto *p =
                   std::get_if<InstantiateParameters>(&step.parameters)) {
      if (step.rule != "Instantiate")
        return reject("Instantiate parameters are attached to a different rule");
      if (!p->valid())
        return reject("Instantiate parameters are invalid");
    } else if (const auto *p =
                   std::get_if<BackboneParameters>(&step.parameters)) {
      if (step.rule != "Backbone")
        return reject("Backbone parameters are attached to a different rule");
      if (!p->valid())
        return reject("Backbone parameters are invalid");
    } else if (const auto *p =
                   std::get_if<SweepParameters>(&step.parameters)) {
      if (step.rule != "Sweep")
        return reject("Sweep parameters are attached to a different rule");
      if (!p->valid())
        return reject("Sweep parameters are invalid");
    } else if (const auto *p =
                   std::get_if<VivifyParameters>(&step.parameters)) {
      if (step.rule != "Vivify")
        return reject("Vivify parameters are attached to a different rule");
      if (!p->valid())
        return reject("Vivify parameters are invalid");
    } else if (const auto *p = std::get_if<BceParameters>(&step.parameters)) {
      if (step.rule != "BCE")
        return reject("BCE parameters are attached to a different rule");
      if (!p->valid())
        return reject("BCE parameters are invalid");
    } else if (const auto *p =
                   std::get_if<TernaryParameters>(&step.parameters)) {
      if (step.rule != "HyperTernaryRes")
        return reject("ternary parameters are attached to a different rule");
      if (!p->valid())
        return reject("ternary parameters are invalid");
    } else if (const auto *p =
                   std::get_if<FactorParameters>(&step.parameters)) {
      if (step.rule != "Factor")
        return reject("Factor parameters are attached to a different rule");
      if (!p->valid())
        return reject("Factor parameters are invalid");
    } else if (const auto *p =
                   std::get_if<TransitiveReduceParameters>(&step.parameters)) {
      if (step.rule != "TransitiveReduce")
        return reject("transitive parameters are attached to a different rule");
      if (!p->valid())
        return reject("transitive parameters are invalid");
    } else if (const auto *p =
                   std::get_if<SubstituteParameters>(&step.parameters)) {
      if (step.rule != "Substitute")
        return reject("Substitute parameters are attached to a different rule");
      if (!p->valid())
        return reject("Substitute parameters are invalid");
    } else if (const auto *p =
                   std::get_if<ReduceParameters>(&step.parameters)) {
      if (step.rule != "Reduce")
        return reject("Reduce parameters are attached to a different rule");
      if (!p->valid())
        return reject("Reduce parameters are invalid");
    } else if (const auto *p =
                   std::get_if<WalkParameters>(&step.parameters)) {
      if (step.rule != "Walk")
        return reject("Walk parameters are attached to a different action");
      if (!p->valid())
        return reject("Walk parameters are invalid");
    } else if (const auto *p =
                   std::get_if<ReorderParameters>(&step.parameters)) {
      if (step.rule != "Reorder")
        return reject("Reorder parameters are attached to a different action");
      if (!p->valid())
        return reject("Reorder parameters are invalid");
    } else if (const auto *p =
                   std::get_if<ParityModelParameters>(&step.parameters)) {
      if (step.rule != "Lucky")
        return reject("parity-model parameters are attached to a different action");
      if (!p->valid())
        return reject("Lucky parity-model parameters are invalid");
    } else if (const auto *p =
                   std::get_if<OnehotModelParameters>(&step.parameters)) {
      if (step.rule != "Lucky")
        return reject("onehot-model parameters are attached to a different action");
      if (!p->valid())
        return reject("Lucky onehot-model parameters are invalid");
    } else if (const auto *p = std::get_if<
                   BalancedSignatureModelParameters>(&step.parameters)) {
      if (step.rule != "Lucky")
        return reject(
            "balanced-signature-model parameters are attached to a different action");
      if (!p->valid())
        return reject("Lucky balanced-signature-model parameters are invalid");
    } else if (const auto *p =
                   std::get_if<FermatModelParameters>(&step.parameters)) {
      if (step.rule != "Lucky" || !p->valid())
        return reject("Lucky fermat-model parameters are invalid or misplaced");
    } else if (const auto *p =
                   std::get_if<LookupCircuitModelParameters>(&step.parameters)) {
      if (step.rule != "Lucky" || !p->valid())
        return reject(
            "Lucky lookup-circuit-model parameters are invalid or misplaced");
    } else if (const auto *p =
                   std::get_if<SimonModelParameters>(&step.parameters)) {
      if (step.rule != "Lucky" || !p->valid())
        return reject("Lucky simon-model parameters are invalid or misplaced");
    } else if (const auto *p =
                   std::get_if<StconnProofParameters>(&step.parameters)) {
      if (step.rule != "Lucky" || !p->valid())
        return reject("Lucky stconn-proof parameters are invalid or misplaced");
    } else if (const auto *p = std::get_if<AffineModelParameters>(&step.parameters)) {
      if (step.rule != "Lucky" || !p->valid())
        return reject("Lucky affine-model parameters are invalid or misplaced");
} else if (const auto *p = std::get_if<
                   SubstitutionRedundancyParameters>(&step.parameters)) {
      if (step.rule != "SubstitutionRedundancy")
        return reject("SR parameters are attached to a different rule");
      if (!p->valid())
        return reject("SR parameters are invalid");
    }
    for (size_t j = 0; j < i; ++j)
      if (program[j].id == step.id)
        return reject("StepId is duplicated");
  }
  if (error)
    error->clear();
  return true;
}

} // namespace tactix::strategy
