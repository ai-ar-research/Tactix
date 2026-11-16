#pragma once

#include "parameter_schema.hpp"

#include "rules/semantic_extension/direct_csp_action_limits.hpp"
#include "rules/semantic_extension/nary_csp_action_limits.hpp"
#include "strategy/scoped_rule_parameters.hpp"

#include <cstdint>

namespace tactix {
struct Kernel;
struct AffineBasisActionLimits;
struct DirectPhpActionLimits;
struct Gf3AffineActionLimits;
struct Gf3GaussianActionLimits;
struct Gf3GeneralRowActionLimits;
struct TseitinCycleBreakingLimits;
namespace strategy {

enum class AffineConsequenceSemantics : uint8_t {
  SelectedConsequenceV1 = 1,
  CertifiedBasisV2 = 2,
};

// Typed, serializable limits; runtime identities stay session-owned.
struct AffineConsequenceParameters {
  TACTIX_DECLARE_PARAMETER_FIELDS(AffineImageFields)
  uint64_t consequence = 0;
  // Appended fields preserve every existing 14-value aggregate as V1.
  AffineConsequenceSemantics semantics =
      AffineConsequenceSemantics::SelectedConsequenceV1;
  TACTIX_DECLARE_PARAMETER_FIELDS(AffineBasisFields)
  bool operator==(const AffineConsequenceParameters &) const = default;
  bool valid() const {
    if (maximum_arity > 8 || maximum_output_width > 4) return false;
    if (semantics == AffineConsequenceSemantics::SelectedConsequenceV1)
      return maximum_intermediate_width <= 16;
    if (semantics != AffineConsequenceSemantics::CertifiedBasisV2)
      return false;
    return maximum_clauses && maximum_snapshot_literals &&
           maximum_snapshot_work && maximum_groups && maximum_patterns &&
           maximum_elimination_work && maximum_arity &&
           maximum_intermediate_width && maximum_sources &&
           maximum_proof_additions && maximum_journal_bytes &&
           basis_maximum_work && basis_maximum_intermediate_width &&
           basis_maximum_sources && basis_maximum_premises &&
           basis_maximum_nodes && basis_maximum_direct_width >= 2 &&
           basis_maximum_direct_width <= 8 && maximum_external_extent &&
           maximum_session_keys && maximum_session_work;
  }
  static AffineConsequenceParameters from_kernel(const Kernel &);
};

// Image limits, then CertifiedBasisV2 limits.
struct AffineImageFields {};
struct AffineBasisFields {};

TACTIX_VISIT_PARAMETER_FIELDS(AffineImageFields)

TACTIX_VISIT_PARAMETER_FIELDS(AffineBasisFields)

// Positive-persistence profile; deferred limits bound deletion occupancy.
struct AffineMatrixInitializeParameters {
  // CompleteSnapshot: the root closure's non-persistent session.
  enum class Admission : uint8_t { AuthenticatedSubimage = 1, CompleteSnapshot = 2 };
  enum class Operation : uint8_t {
    Initialize = 1, Refresh = 2, RootMaintenance = 3,
    // Append-only compatibility tag, root-only like RootMaintenance.
    SearchStep = 4,
    // Opt-in lazy reason boundary: DeferredSearchStep may install one
    // preflighted implication, which MaterializeReasons must discharge before
    // any reason-consuming rule.
    DeferredSearchStep = 5, MaterializeReasons = 6,
    // Eager exact-core, implication-graph-only large reason.
    DetachedMinimalSearchStep = 7,
    // Eager source-support weakening with an unwatched trail lifetime.
    DetachedSourceSearchStep = 8,
    // Compact trail handles, planned only for conflict ancestors.
    LazySourceSearchStep = 9,
    LazyMinimalSearchStep = 10
  };
  AffineConsequenceParameters basis;
  Admission admission = Admission::AuthenticatedSubimage;
  Operation operation = Operation::Initialize;
  uint64_t maximum_retained_bytes = 0;
  uint64_t maximum_deferred_deletions = 0;
  uint64_t maximum_deferred_literal_work = 0;
  bool operator==(const AffineMatrixInitializeParameters &) const = default;
  bool valid() const {
    if (!basis.valid() || basis.semantics != AffineConsequenceSemantics::CertifiedBasisV2 ||
        basis.consequence || operation < Operation::Initialize ||
        operation > Operation::LazyMinimalSearchStep)
      return false;
    if (admission == Admission::CompleteSnapshot)
      return (operation == Operation::Initialize || operation == Operation::SearchStep) &&
             !maximum_retained_bytes && !maximum_deferred_deletions &&
             !maximum_deferred_literal_work;
    return admission == Admission::AuthenticatedSubimage && maximum_retained_bytes &&
           maximum_deferred_deletions && maximum_deferred_literal_work;
  }
  static AffineMatrixInitializeParameters from_kernel(const Kernel &);
};

struct ExactCoverExtensionParameters {
  TACTIX_DECLARE_PARAMETER_FIELDS(ExactCoverExtensionParameters)
  bool valid() const { return arity == 0 || arity >= 3; }
  static ExactCoverExtensionParameters from_kernel(const Kernel &);
};

TACTIX_VISIT_PARAMETER_FIELDS(ExactCoverExtensionParameters)

struct IndependentSetCoverParameters {
  TACTIX_DECLARE_PARAMETER_FIELDS(IndependentSetCoverParameters)
  bool valid() const { return true; }
  static IndependentSetCoverParameters from_kernel(const Kernel &);
};

TACTIX_VISIT_PARAMETER_FIELDS(IndependentSetCoverParameters)

struct Gf3AffineConsequenceParameters {
  TACTIX_DECLARE_PARAMETER_FIELDS(Gf3AffineConsequenceParameters)

  bool valid() const;
  static Gf3AffineConsequenceParameters from_kernel(const Kernel &);
};

TACTIX_VISIT_PARAMETER_FIELDS(Gf3AffineConsequenceParameters)

// Explicit GuardedCoverV2 profile; zero limits decline rather than inherit V1.
using DirectCspGuardedCoverParameters =
    DirectCspGuardedCoverActionLimits;

TACTIX_VISIT_PARAMETER_FIELDS(DirectCspGuardedCoverParameters)

struct DirectCspSupportParameters : DirectCspSupportActionLimits {
  static DirectCspSupportParameters from_kernel(const Kernel &);
};

TACTIX_VISIT_PARAMETER_FIELDS(DirectCspSupportParameters)

struct NaryCspEqualityParameters : NaryCspEqualityLimits {
  static NaryCspEqualityParameters from_kernel(const Kernel &);
  bool operator==(const NaryCspEqualityParameters &) const = default;
};

TACTIX_VISIT_PARAMETER_FIELDS(NaryCspEqualityParameters)

struct NaryCspEqualityAmoParameters : NaryCspEqualityAmoLimits {
  static NaryCspEqualityAmoParameters from_kernel(const Kernel &);
  bool operator==(const NaryCspEqualityAmoParameters &) const = default;
};

TACTIX_VISIT_PARAMETER_FIELDS(NaryCspEqualityAmoParameters)

struct NaryCspEqualityTransitivityParameters
    : NaryCspEqualityTransitivityLimits {
  static NaryCspEqualityTransitivityParameters from_kernel(const Kernel &);
  bool operator==(
      const NaryCspEqualityTransitivityParameters &) const = default;
};

TACTIX_VISIT_PARAMETER_FIELDS(NaryCspEqualityTransitivityParameters)

// Direct pigeonhole limits; budgets stay distinct for attribution.
struct DirectPhpRefutationParameters {
  TACTIX_DECLARE_PARAMETER_FIELDS(DirectPhpRefutationParameters)

  bool valid() const;
  static DirectPhpRefutationParameters from_kernel(const Kernel &);
};

TACTIX_VISIT_PARAMETER_FIELDS(DirectPhpRefutationParameters)

// Bounded profile for authenticated Gaussian closure over recognized GF(3)
// one-hot equations; the controller names the batch, these fields define
// recognition, certificate and resource semantics.
struct Gf3GaussianConsequenceParameters {
  TACTIX_DECLARE_PARAMETER_FIELDS(Gf3GaussianConsequenceParameters)

  bool valid() const;
  static Gf3GaussianConsequenceParameters from_kernel(const Kernel &);
};

TACTIX_VISIT_PARAMETER_FIELDS(Gf3GaussianConsequenceParameters)

// One forbidden tuple of a non-unit GF(3) Gaussian row.
struct Gf3GeneralRowConsequenceParameters {
  Gf3GaussianConsequenceParameters gaussian;
  uint64_t maximum_row_tuples = 100000;
  uint32_t maximum_row_width = 32;
  uint64_t maximum_duplicate_scan_work = 1000000;
  uint64_t maximum_arena_words = 1000000;

  bool valid() const;
  static Gf3GeneralRowConsequenceParameters from_kernel(const Kernel &);
  bool operator==(const Gf3GeneralRowConsequenceParameters &) const = default;
};

// The Gaussian list, then the appended single-row limits.
template <class Parameters, class Visitor>
auto visit_parameter_fields(FieldsOf<Gf3GeneralRowConsequenceParameters>,
                            Parameters &p, Visitor visit) {
  return visit_parameter_fields(
      FieldsOf<Gf3GaussianConsequenceParameters>{}, p.gaussian,
      [&](auto &...gaussian) {
        return visit(gaussian..., p.maximum_row_tuples, p.maximum_row_width,
                     p.maximum_duplicate_scan_work, p.maximum_arena_words);
      });
}

struct TseitinCycleBreakingParameters {
  TACTIX_DECLARE_PARAMETER_FIELDS(TseitinCycleBreakingParameters)

  bool valid() const;
  static TseitinCycleBreakingParameters from_kernel(const Kernel &);
};

TACTIX_VISIT_PARAMETER_FIELDS(TseitinCycleBreakingParameters)

struct SemanticVariableAdviceParameters {
  TACTIX_DECLARE_PARAMETER_FIELDS(SemanticVariableAdviceParameters)
  bool valid() const { return true; }
  static SemanticVariableAdviceParameters from_kernel(const Kernel &);
};

TACTIX_VISIT_PARAMETER_FIELDS(SemanticVariableAdviceParameters)

// Pure public-parameter to native-limit conversions, no validation.
AffineBasisActionLimits action_limits(const AffineConsequenceParameters &);
Gf3AffineActionLimits action_limits(const Gf3AffineConsequenceParameters &);
// These profiles are their native limits.
inline DirectCspGuardedCoverActionLimits
action_limits(const DirectCspGuardedCoverParameters &p) { return p; }
inline DirectCspSupportActionLimits
action_limits(const DirectCspSupportParameters &p) { return p; }
inline NaryCspEqualityLimits
action_limits(const NaryCspEqualityParameters &p) { return p; }
inline NaryCspEqualityAmoLimits
action_limits(const NaryCspEqualityAmoParameters &p) { return p; }
inline NaryCspEqualityTransitivityLimits
action_limits(const NaryCspEqualityTransitivityParameters &p) { return p; }
DirectPhpActionLimits action_limits(const DirectPhpRefutationParameters &);
Gf3GaussianActionLimits
action_limits(const Gf3GaussianConsequenceParameters &);
Gf3GeneralRowActionLimits
action_limits(const Gf3GeneralRowConsequenceParameters &);
TseitinCycleBreakingLimits
action_limits(const TseitinCycleBreakingParameters &);

void install_rule_parameters(Kernel &, const AffineConsequenceParameters &);
void install_rule_parameters(Kernel &, const AffineMatrixInitializeParameters &);
void install_rule_parameters(Kernel &, const ExactCoverExtensionParameters &);
void install_rule_parameters(Kernel &, const IndependentSetCoverParameters &);
void install_rule_parameters(Kernel &, const Gf3AffineConsequenceParameters &);
void install_rule_parameters(Kernel &, const DirectCspSupportParameters &);
void install_rule_parameters(Kernel &, const NaryCspEqualityParameters &);
void install_rule_parameters(Kernel &, const NaryCspEqualityTransitivityParameters &);
void install_rule_parameters(Kernel &, const NaryCspEqualityAmoParameters &);
void install_rule_parameters(Kernel &, const DirectPhpRefutationParameters &);
void install_rule_parameters(Kernel &, const Gf3GaussianConsequenceParameters &);
void install_rule_parameters(Kernel &,
                             const Gf3GeneralRowConsequenceParameters &);
void install_rule_parameters(Kernel &, const TseitinCycleBreakingParameters &);
void install_rule_parameters(Kernel &, const SemanticVariableAdviceParameters &);

} // namespace strategy
} // namespace tactix
