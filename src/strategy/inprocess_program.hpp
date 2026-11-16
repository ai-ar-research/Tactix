#pragma once

#include "strategy/bve_invocation.hpp"
#include "strategy/theory_action_parameters.hpp"
#include "strategy/rule_parameters.hpp"
#include "strategy/sweep_parameters.hpp"
#include "rules/substitution_redundancy/substitution_redundancy.hpp"
#include "rules/semantic_extension/cardinality_search_action.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
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
                 SignedCardinalityProofParameters, ExactCoverModelParameters,
                 IndependentSetModelParameters,
                 AffineModelParameters,
                 SubstitutionRedundancyParameters, GraphLabelingParameters,
                 ParityConsequenceParameters, RamseyStartupParameters,
                 AffineConsequenceParameters, ExactCoverExtensionParameters,
                 IndependentSetCoverParameters,
                 Gf3AffineConsequenceParameters, DirectCspSupportParameters,
                 NaryCspEqualityParameters,
                 NaryCspEqualityAmoParameters,
                 DirectPhpRefutationParameters,
                 Gf3GaussianConsequenceParameters,
                 Gf3GeneralRowConsequenceParameters,
                 TseitinCycleBreakingParameters,
                 SemanticVariableAdviceParameters, DirectCspGuardedCoverParameters, NaryCspEqualityTransitivityParameters, AffineMatrixInitializeParameters,
                 CardinalitySearchActionLimits>;

struct ActionParameterMetadata {
  bool matches_rule = false;
  bool valid = true;
  const char *invalid_message = nullptr;
  bool inprocess_checked = false;
  const char *inprocess_mismatch_message = nullptr;
  const char *inprocess_invalid_message = nullptr;
  const char *inprocess_unsupported_message = nullptr;
};

template <class Parameters>
ActionParameterMetadata action_parameter_metadata(
    const Parameters &parameters, const std::string &rule,
    bool check_validity = false) {
  using P = std::decay_t<Parameters>;
#define CONTRACT(TYPE, MATCH, VALIDITY, ONLINE, CHECKED, MISMATCH, INVALID,    \
                 UNSUPPORTED)                                                  \
  if constexpr (std::is_same_v<P, TYPE>) {                                     \
    const bool valid = !check_validity || (VALIDITY);                          \
    return {MATCH, valid, valid ? nullptr : ONLINE, CHECKED, MISMATCH, INVALID,\
            UNSUPPORTED};                                                       \
  }
// Online and inprocess validation share wording; three Lucky families have
// shorter local labels. Keep those explicit through SEPARATE_LABELS.
#define SEPARATE_LABELS(TYPE, MATCH, ONLINE, LOCAL, OWNER)                     \
  CONTRACT(TYPE, MATCH, parameters.valid(), ONLINE " parameters are invalid", \
           true, LOCAL " parameters are attached to a different " OWNER,     \
           ONLINE " parameters are invalid", nullptr)
#define SEPARATE(TYPE, MATCH, LABEL, OWNER)                                   \
  SEPARATE_LABELS(TYPE, MATCH, LABEL, LABEL, OWNER)
#define COMBINED(TYPE, MATCH, LABEL)                                          \
  CONTRACT(TYPE, MATCH, parameters.valid(), LABEL " parameters are invalid", \
           true, LABEL " parameters are invalid or misplaced",               \
           LABEL " parameters are invalid or misplaced", nullptr)
#define ONLINE_ONLY(TYPE, MATCH, LABEL)                                       \
  CONTRACT(TYPE, MATCH, parameters.valid(), LABEL " parameters are invalid", \
           false, nullptr, nullptr, nullptr)
#define ONLINE_UNCHECKED(TYPE, MATCH)                                           \
  CONTRACT(TYPE, MATCH, true, nullptr, false, nullptr, nullptr, nullptr)
#define UNSUPPORTED(TYPE, MATCH, ONLINE, MESSAGE)                               \
  CONTRACT(TYPE, MATCH, parameters.valid(), ONLINE, true, nullptr, nullptr,    \
           MESSAGE)
#define UNSUPPORTED_UNCHECKED(TYPE, MATCH, MESSAGE)                             \
  CONTRACT(TYPE, MATCH, true, nullptr, true, nullptr, nullptr, MESSAGE)
  if constexpr (std::is_same_v<P, std::monostate>)
    return {true, true, nullptr, false, nullptr, nullptr, nullptr};
  SEPARATE(AnalyzeParameters, rule == "ConflictAnalysis", "Analyze", "rule");
  SEPARATE(BveParameters, rule == "BVE", "BVE", "rule");
  SEPARATE(SubsumeForwardParameters, rule == "SubsumeForward", "C1", "rule");
  SEPARATE(StrengthenParameters, rule == "Strengthen", "C2", "rule");
  SEPARATE(CongruenceParameters, rule == "Congruence", "Congruence", "rule");
  COMBINED(ParityConsequenceParameters, rule == "ParityConsequence", "ParityConsequence");
  COMBINED(AffineMatrixInitializeParameters, rule == "AffineMatrixInitialize", "Persistent affine");
  COMBINED(AffineConsequenceParameters, rule == "AffineConsequence" || rule == "AffineMatrixInitialize",
      "AffineConsequence");
  COMBINED(ExactCoverExtensionParameters, rule == "ExactCoverExtension", "ExactCoverExtension");
  COMBINED(IndependentSetCoverParameters, rule == "IndependentSetCover", "IndependentSetCover");
  COMBINED(Gf3AffineConsequenceParameters, rule == "Gf3AffineConsequence", "Gf3AffineConsequence");
  UNSUPPORTED(DirectCspGuardedCoverParameters, rule == "DirectCspSupport",
              "GuardedCoverV2 parameters are invalid",
              "GuardedCoverV2 requires online typed selections");
  COMBINED(DirectCspSupportParameters, rule == "DirectCspSupport", "DirectCspSupport");
  COMBINED(NaryCspEqualityParameters, rule == "NaryCspEquality", "NaryCspEquality");
  UNSUPPORTED_UNCHECKED(
      NaryCspEqualityTransitivityParameters,
      rule == "NaryCspEqualityTransitivity",
      "transitivity requires explicit online selections");
  COMBINED(NaryCspEqualityAmoParameters, rule == "NaryCspEqualityAmo", "NaryCspEqualityAmo");
  COMBINED(DirectPhpRefutationParameters, rule == "DirectPhpRefutation", "DirectPhpRefutation");
  COMBINED(Gf3GaussianConsequenceParameters, rule == "Gf3GaussianConsequence",
      "Gf3GaussianConsequence");
  COMBINED(Gf3GeneralRowConsequenceParameters, rule == "Gf3GeneralRowConsequence",
      "Gf3GeneralRowConsequence");
  ONLINE_UNCHECKED(CardinalitySearchActionLimits, rule == "CardinalitySearch");
  COMBINED(TseitinCycleBreakingParameters, rule == "TseitinCycleBreaking", "TseitinCycleBreaking");
  ONLINE_ONLY(SemanticVariableAdviceParameters, rule == "SemanticVariableAdvice",
      "SemanticVariableAdvice");
  SEPARATE(InstantiateParameters, rule == "Instantiate", "Instantiate", "rule");
  SEPARATE(BackboneParameters, rule == "Backbone", "Backbone", "rule");
  SEPARATE(SweepParameters, rule == "Sweep", "Sweep", "rule");
  SEPARATE(VivifyParameters, rule == "Vivify", "Vivify", "rule");
  SEPARATE(BceParameters, rule == "BCE", "BCE", "rule");
  SEPARATE(TernaryParameters, rule == "HyperTernaryRes", "ternary", "rule");
  SEPARATE(FactorParameters, rule == "Factor", "Factor", "rule");
  SEPARATE(TransitiveReduceParameters, rule == "TransitiveReduce", "transitive", "rule");
  SEPARATE(SubstituteParameters, rule == "Substitute", "Substitute", "rule");
  SEPARATE(ReduceParameters, rule == "Reduce", "Reduce", "rule");
  SEPARATE(WalkParameters, rule == "Walk", "Walk", "action");
  SEPARATE(ReorderParameters, rule == "Reorder", "Reorder", "action");
  SEPARATE_LABELS(ParityModelParameters, rule == "Lucky",
      "Lucky parity-model", "parity-model", "action");
  SEPARATE_LABELS(OnehotModelParameters, rule == "Lucky",
      "Lucky onehot-model", "onehot-model", "action");
  SEPARATE_LABELS(BalancedSignatureModelParameters, rule == "Lucky",
      "Lucky balanced-signature-model", "balanced-signature-model", "action");
  COMBINED(FermatModelParameters, rule == "Lucky", "Lucky fermat-model");
  COMBINED(LookupCircuitModelParameters, rule == "Lucky", "Lucky lookup-circuit-model");
  COMBINED(SimonModelParameters, rule == "Lucky", "Lucky simon-model");
  COMBINED(StconnProofParameters, rule == "Lucky", "Lucky stconn-proof");
  COMBINED(SignedCardinalityProofParameters, rule == "Lucky", "Lucky signed-cardinality-proof");
  ONLINE_ONLY(ExactCoverModelParameters, rule == "Lucky", "Lucky exact-cover-model");
  ONLINE_ONLY(IndependentSetModelParameters, rule == "Lucky", "Lucky independent-set-model");
  ONLINE_ONLY(RamseyStartupParameters, rule == "Lucky", "Lucky ramsey-startup");
  ONLINE_ONLY(GraphLabelingParameters, rule == "Lucky", "Lucky graph-labeling");
  COMBINED(AffineModelParameters, rule == "Lucky", "Lucky affine-model");
  SEPARATE(SubstitutionRedundancyParameters, rule == "SubstitutionRedundancy", "SR", "rule");
#undef UNSUPPORTED_UNCHECKED
#undef UNSUPPORTED
#undef ONLINE_UNCHECKED
#undef ONLINE_ONLY
#undef COMBINED
#undef SEPARATE
#undef SEPARATE_LABELS
#undef CONTRACT
  return {};
}
// Compatibility name for the inprocessing scheduler.
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
  // Absence inherits; a tagged alternative rejects cross-rule attachment.
  InprocessParameters parameters;
  // Per-occurrence choices; empty inherits, zero max_applications unlimited.
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
      // Explicit-only until its default schedule has N4 evidence. Its authored
      // position keeps future enablement adjacent to gate/parity discovery and
      // avoids the registry append fallback becoming an accidental policy.
      {19, "ParityConsequence", Probe, false},
      {20, "AffineConsequence", Probe, false},
      {21, "ExactCoverExtension", Probe, false},
      {22, "IndependentSetCover", Probe, false},
      {23, "Gf3AffineConsequence", Probe, false},
      {24, "DirectCspSupport", Probe, false},
      {25, "DirectPhpRefutation", Probe, false},
      {26, "Gf3GaussianConsequence", Probe, false},
      {27, "TseitinCycleBreaking", Probe, false},
      // StepId 28 is retired and must never be reused.
      {29, "GraphIntervalEquivalence", Probe, false},
      {30, "AffineMatrixInitialize", Probe, false},
      {31, "NaryCspEquality", Probe, false},
      {32, "Gf3GeneralRowConsequence", Probe, false},
      {33, "NaryCspEqualityAmo", Probe, false},
      {34, "NaryCspEqualityTransitivity", Probe, false},
      {35, "CardinalitySearch", Probe, false},
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
    if (step.id == 28)
      return reject("StepId 28 is retired");
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

    ActionParameterMetadata metadata = std::visit(
        [&](const auto &parameters) {
          return action_parameter_metadata(parameters, step.rule);
        },
        step.parameters);
    if (metadata.inprocess_checked) {
      if (metadata.inprocess_unsupported_message)
        return reject(metadata.inprocess_unsupported_message);
      if (!metadata.matches_rule)
        return reject(metadata.inprocess_mismatch_message);
      metadata = std::visit(
          [&](const auto &parameters) {
            return action_parameter_metadata(parameters, step.rule, true);
          },
          step.parameters);
      if (!metadata.valid)
        return reject(metadata.inprocess_invalid_message);
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
