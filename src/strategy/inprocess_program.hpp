#pragma once

#include "strategy/analyze_parameters.hpp"
#include "strategy/bve_invocation.hpp"
#include "strategy/congruence_parameters.hpp"
#include "strategy/parity_consequence_parameters.hpp"
#include "strategy/theory_action_parameters.hpp"
#include "strategy/forward_parameters.hpp"
#include "strategy/failed_literal_parameters.hpp"
#include "strategy/instantiate_parameters.hpp"
#include "strategy/rule_parameters.hpp"
#include "strategy/sweep_parameters.hpp"
#include "strategy/vivify_parameters.hpp"
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
#define SEPARATE(TYPE, MATCH, ONLINE, MISMATCH)                                 \
  CONTRACT(TYPE, MATCH, parameters.valid(), ONLINE, true, MISMATCH, ONLINE,    \
           nullptr)
#define COMBINED(TYPE, MATCH, ONLINE, MESSAGE)                                  \
  CONTRACT(TYPE, MATCH, parameters.valid(), ONLINE, true, MESSAGE, MESSAGE,    \
           nullptr)
#define ONLINE_ONLY(TYPE, MATCH, ONLINE)                                        \
  CONTRACT(TYPE, MATCH, parameters.valid(), ONLINE, false, nullptr, nullptr,   \
           nullptr)
#define ONLINE_UNCHECKED(TYPE, MATCH)                                           \
  CONTRACT(TYPE, MATCH, true, nullptr, false, nullptr, nullptr, nullptr)
#define UNSUPPORTED(TYPE, MATCH, ONLINE, MESSAGE)                               \
  CONTRACT(TYPE, MATCH, parameters.valid(), ONLINE, true, nullptr, nullptr,    \
           MESSAGE)
#define UNSUPPORTED_UNCHECKED(TYPE, MATCH, MESSAGE)                             \
  CONTRACT(TYPE, MATCH, true, nullptr, true, nullptr, nullptr, MESSAGE)
  if constexpr (std::is_same_v<P, std::monostate>)
    return {true, true, nullptr, false, nullptr, nullptr, nullptr};
  SEPARATE(AnalyzeParameters, rule == "ConflictAnalysis",
           "Analyze parameters are invalid",
           "Analyze parameters are attached to a different rule");
  SEPARATE(BveParameters, rule == "BVE", "BVE parameters are invalid",
           "BVE parameters are attached to a different rule");
  SEPARATE(SubsumeForwardParameters, rule == "SubsumeForward",
           "C1 parameters are invalid",
           "C1 parameters are attached to a different rule");
  SEPARATE(StrengthenParameters, rule == "Strengthen",
           "C2 parameters are invalid",
           "C2 parameters are attached to a different rule");
  SEPARATE(CongruenceParameters, rule == "Congruence",
           "Congruence parameters are invalid",
           "Congruence parameters are attached to a different rule");
  COMBINED(ParityConsequenceParameters, rule == "ParityConsequence",
           "ParityConsequence parameters are invalid",
           "ParityConsequence parameters are invalid or misplaced");
  COMBINED(AffineMatrixInitializeParameters,
           rule == "AffineMatrixInitialize",
           "Persistent affine parameters are invalid",
           "Persistent affine parameters are invalid or misplaced");
  COMBINED(AffineConsequenceParameters,
           rule == "AffineConsequence" || rule == "AffineMatrixInitialize",
           "AffineConsequence parameters are invalid",
           "AffineConsequence parameters are invalid or misplaced");
  COMBINED(ExactCoverExtensionParameters, rule == "ExactCoverExtension",
           "ExactCoverExtension parameters are invalid",
           "ExactCoverExtension parameters are invalid or misplaced");
  COMBINED(IndependentSetCoverParameters, rule == "IndependentSetCover",
           "IndependentSetCover parameters are invalid",
           "IndependentSetCover parameters are invalid or misplaced");
  COMBINED(Gf3AffineConsequenceParameters, rule == "Gf3AffineConsequence",
           "Gf3AffineConsequence parameters are invalid",
           "Gf3AffineConsequence parameters are invalid or misplaced");
  UNSUPPORTED(DirectCspGuardedCoverParameters, rule == "DirectCspSupport",
              "GuardedCoverV2 parameters are invalid",
              "GuardedCoverV2 requires online typed selections");
  COMBINED(DirectCspSupportParameters, rule == "DirectCspSupport",
           "DirectCspSupport parameters are invalid",
           "DirectCspSupport parameters are invalid or misplaced");
  COMBINED(NaryCspEqualityParameters, rule == "NaryCspEquality",
           "NaryCspEquality parameters are invalid",
           "NaryCspEquality parameters are invalid or misplaced");
  UNSUPPORTED_UNCHECKED(
      NaryCspEqualityTransitivityParameters,
      rule == "NaryCspEqualityTransitivity",
      "transitivity requires explicit online selections");
  COMBINED(NaryCspEqualityAmoParameters, rule == "NaryCspEqualityAmo",
           "NaryCspEqualityAmo parameters are invalid",
           "NaryCspEqualityAmo parameters are invalid or misplaced");
  COMBINED(DirectPhpRefutationParameters, rule == "DirectPhpRefutation",
           "DirectPhpRefutation parameters are invalid",
           "DirectPhpRefutation parameters are invalid or misplaced");
  COMBINED(Gf3GaussianConsequenceParameters,
           rule == "Gf3GaussianConsequence",
           "Gf3GaussianConsequence parameters are invalid",
           "Gf3GaussianConsequence parameters are invalid or misplaced");
  COMBINED(Gf3GeneralRowConsequenceParameters,
           rule == "Gf3GeneralRowConsequence",
           "Gf3GeneralRowConsequence parameters are invalid",
           "Gf3GeneralRowConsequence parameters are invalid or misplaced");
  ONLINE_UNCHECKED(CardinalitySearchActionLimits, rule == "CardinalitySearch");
  COMBINED(TseitinCycleBreakingParameters, rule == "TseitinCycleBreaking",
           "TseitinCycleBreaking parameters are invalid",
           "TseitinCycleBreaking parameters are invalid or misplaced");
  ONLINE_ONLY(SemanticVariableAdviceParameters,
              rule == "SemanticVariableAdvice",
              "SemanticVariableAdvice parameters are invalid");
  SEPARATE(InstantiateParameters, rule == "Instantiate",
           "Instantiate parameters are invalid",
           "Instantiate parameters are attached to a different rule");
  SEPARATE(BackboneParameters, rule == "Backbone",
           "Backbone parameters are invalid",
           "Backbone parameters are attached to a different rule");
  SEPARATE(SweepParameters, rule == "Sweep", "Sweep parameters are invalid",
           "Sweep parameters are attached to a different rule");
  SEPARATE(VivifyParameters, rule == "Vivify",
           "Vivify parameters are invalid",
           "Vivify parameters are attached to a different rule");
  SEPARATE(BceParameters, rule == "BCE", "BCE parameters are invalid",
           "BCE parameters are attached to a different rule");
  SEPARATE(TernaryParameters, rule == "HyperTernaryRes",
           "ternary parameters are invalid",
           "ternary parameters are attached to a different rule");
  SEPARATE(FactorParameters, rule == "Factor", "Factor parameters are invalid",
           "Factor parameters are attached to a different rule");
  SEPARATE(TransitiveReduceParameters, rule == "TransitiveReduce",
           "transitive parameters are invalid",
           "transitive parameters are attached to a different rule");
  SEPARATE(SubstituteParameters, rule == "Substitute",
           "Substitute parameters are invalid",
           "Substitute parameters are attached to a different rule");
  SEPARATE(ReduceParameters, rule == "Reduce", "Reduce parameters are invalid",
           "Reduce parameters are attached to a different rule");
  SEPARATE(WalkParameters, rule == "Walk", "Walk parameters are invalid",
           "Walk parameters are attached to a different action");
  SEPARATE(ReorderParameters, rule == "Reorder",
           "Reorder parameters are invalid",
           "Reorder parameters are attached to a different action");
  SEPARATE(ParityModelParameters, rule == "Lucky",
           "Lucky parity-model parameters are invalid",
           "parity-model parameters are attached to a different action");
  SEPARATE(OnehotModelParameters, rule == "Lucky",
           "Lucky onehot-model parameters are invalid",
           "onehot-model parameters are attached to a different action");
  SEPARATE(
      BalancedSignatureModelParameters, rule == "Lucky",
      "Lucky balanced-signature-model parameters are invalid",
      "balanced-signature-model parameters are attached to a different action");
  COMBINED(FermatModelParameters, rule == "Lucky",
           "Lucky fermat-model parameters are invalid",
           "Lucky fermat-model parameters are invalid or misplaced");
  COMBINED(LookupCircuitModelParameters, rule == "Lucky",
           "Lucky lookup-circuit-model parameters are invalid",
           "Lucky lookup-circuit-model parameters are invalid or misplaced");
  COMBINED(SimonModelParameters, rule == "Lucky",
           "Lucky simon-model parameters are invalid",
           "Lucky simon-model parameters are invalid or misplaced");
  COMBINED(StconnProofParameters, rule == "Lucky",
           "Lucky stconn-proof parameters are invalid",
           "Lucky stconn-proof parameters are invalid or misplaced");
  COMBINED(SignedCardinalityProofParameters, rule == "Lucky",
           "Lucky signed-cardinality-proof parameters are invalid",
           "Lucky signed-cardinality-proof parameters are invalid or misplaced");
  ONLINE_ONLY(ExactCoverModelParameters, rule == "Lucky",
              "Lucky exact-cover-model parameters are invalid");
  ONLINE_ONLY(IndependentSetModelParameters, rule == "Lucky",
              "Lucky independent-set-model parameters are invalid");
  ONLINE_ONLY(RamseyStartupParameters, rule == "Lucky",
              "Lucky ramsey-startup parameters are invalid");
  ONLINE_ONLY(GraphLabelingParameters, rule == "Lucky",
              "Lucky graph-labeling parameters are invalid");
  COMBINED(AffineModelParameters, rule == "Lucky",
           "Lucky affine-model parameters are invalid",
           "Lucky affine-model parameters are invalid or misplaced");
  SEPARATE(SubstitutionRedundancyParameters,
           rule == "SubstitutionRedundancy", "SR parameters are invalid",
           "SR parameters are attached to a different rule");
#undef UNSUPPORTED_UNCHECKED
#undef UNSUPPORTED
#undef ONLINE_UNCHECKED
#undef ONLINE_ONLY
#undef COMBINED
#undef SEPARATE
#undef CONTRACT
  return {};
}
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
