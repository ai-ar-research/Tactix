#pragma once

#include "rules/semantic_extension/direct_csp_action_limits.hpp"

#include <cstdint>

namespace tactix {
struct Kernel;
struct AffineBasisActionLimits;
struct DirectPhpActionLimits;
struct Gf3AffineActionLimits;
struct Gf3GaussianActionLimits;
struct Gf3GeneralRowActionLimits;
struct NaryCspEqualityAmoLimits;
struct NaryCspEqualityLimits;
struct NaryCspEqualityTransitivityLimits;
struct TseitinCycleBreakingLimits;
namespace strategy {

enum class AffineConsequenceSemantics : uint8_t {
  SelectedConsequenceV1 = 1,
  CertifiedBasisV2 = 2,
};

// Typed, serializable limits only. Runtime action/semantic IDs, source caches,
// provenance streams and fresh-variable identities remain session-owned.
struct AffineConsequenceParameters {
  uint64_t maximum_clauses = 0;
  uint64_t maximum_snapshot_literals = 0;
  uint64_t maximum_snapshot_work = 0;
  uint64_t maximum_groups = 0;
  uint64_t maximum_patterns = 0;
  uint64_t maximum_elimination_work = 0;
  uint32_t maximum_arity = 0;
  uint32_t maximum_intermediate_width = 0;
  uint32_t maximum_sources = 0;
  uint32_t maximum_output_width = 0;
  uint64_t maximum_proof_additions = 0;
  uint64_t maximum_proof_deletions = 0;
  uint64_t maximum_journal_bytes = 0;
  uint64_t consequence = 0;
  // Appended fields preserve every existing 14-value aggregate as V1.
  AffineConsequenceSemantics semantics =
      AffineConsequenceSemantics::SelectedConsequenceV1;
  uint64_t basis_maximum_work = 0;
  uint32_t basis_maximum_intermediate_width = 0;
  uint32_t basis_maximum_sources = 0;
  uint32_t basis_maximum_premises = 0;
  uint32_t basis_maximum_nodes = 0;
  uint32_t basis_maximum_direct_width = 0;
  uint32_t maximum_external_extent = 0;
  uint64_t maximum_session_keys = 0;
  uint64_t maximum_session_work = 0;
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

template <class Parameters, class Visitor>
bool visit_affine_image_parameter_fields(Parameters &p, Visitor visit) {
  return visit(p.maximum_clauses, p.maximum_snapshot_literals,
               p.maximum_snapshot_work, p.maximum_groups, p.maximum_patterns,
               p.maximum_elimination_work, p.maximum_arity,
               p.maximum_intermediate_width, p.maximum_sources,
               p.maximum_output_width, p.maximum_proof_additions,
               p.maximum_proof_deletions, p.maximum_journal_bytes);
}

template <class Parameters, class Visitor>
bool visit_affine_basis_parameter_fields(Parameters &p, Visitor visit) {
  return visit(p.basis_maximum_work, p.basis_maximum_intermediate_width,
               p.basis_maximum_sources, p.basis_maximum_premises,
               p.basis_maximum_nodes, p.basis_maximum_direct_width,
               p.maximum_external_extent, p.maximum_session_keys,
               p.maximum_session_work);
}

template <class Target, class Source>
void copy_affine_parameter_fields(Target &target, const Source &source) {
  visit_affine_image_parameter_fields(target, [&](auto &...targets) {
    return visit_affine_image_parameter_fields(
        source, [&](const auto &...sources) {
          ((targets = sources), ...);
          return true;
        });
  });
  visit_affine_basis_parameter_fields(target, [&](auto &...targets) {
    return visit_affine_basis_parameter_fields(
        source, [&](const auto &...sources) {
          ((targets = sources), ...);
          return true;
        });
  });
}

// Separate positive-persistence profile: old affine encodings retain zero
// persistence. Deferred limits bound outstanding physical deletion occupancy,
// not all proof-maintenance scans or total process memory.
struct AffineMatrixInitializeParameters {
  // CompleteSnapshot: the startup root closure's non-persistent session over
  // the complete live formula (zero persistence limits).
  enum class Admission : uint8_t { AuthenticatedSubimage = 1, CompleteSnapshot = 2 };
  enum class Operation : uint8_t {
    Initialize = 1, Refresh = 2, RootMaintenance = 3,
    // Append-only compatibility tag. It is root-only like RootMaintenance;
    // the name no longer grants execution during a search interval.
    SearchStep = 4,
    // Opt-in lazy clausal reason boundary.  DeferredSearchStep may install one
    // preflighted implication; MaterializeReasons must discharge it before a
    // rule that consumes implication reasons becomes legal.
    DeferredSearchStep = 5, MaterializeReasons = 6,
    // Eager proof publication with an exact-core, implication-graph-only
    // large reason. It isolates watched-clause lifetime before the full lazy
    // matrix-row handle is admitted.
    DetachedMinimalSearchStep = 7,
    // Eagerly certified source-support weakening with the same unwatched
    // trail lifetime. Kept distinct so reason shape is a public choice.
    DetachedSourceSearchStep = 8,
    // Compact native trail handles; proof planning happens only for a conflict
    // ancestor. Reason shape remains an explicit public strategy choice.
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
  uint32_t arity = 0;
  uint64_t maximum_items = 0;
  uint64_t maximum_source_clauses = 0;
  uint64_t maximum_source_literals = 0;
  uint64_t maximum_recognition_work = 0;
  uint64_t maximum_premise_references = 0;
  uint32_t maximum_definitions = 0;
  uint64_t maximum_additions = 0;
  uint64_t maximum_literal_occurrences = 0;
  uint32_t maximum_extent_growth = 0;
  uint32_t maximum_external_extent = 0;
  uint64_t maximum_journal_bytes = 0;
  bool valid() const { return arity == 0 || arity >= 3; }
  static ExactCoverExtensionParameters from_kernel(const Kernel &);
};

struct IndependentSetCoverParameters {
  uint64_t maximum_clauses = 0;
  uint64_t maximum_literals = 0;
  uint64_t maximum_recognition_work = 0;
  uint64_t maximum_proof_work = 0;
  uint64_t maximum_proof_additions = 0;
  uint64_t maximum_proof_deletions = 0;
  uint64_t maximum_proof_literal_occurrences = 0;
  uint64_t maximum_journal_bytes = 0;
  uint32_t maximum_vertices = 0;
  uint32_t maximum_target = 0;
  bool valid() const { return true; }
  static IndependentSetCoverParameters from_kernel(const Kernel &);
};

struct Gf3AffineConsequenceParameters {
  uint64_t maximum_source_clauses = 0;
  uint64_t maximum_source_literals = 0;
  uint32_t maximum_domains = 0;
  uint32_t maximum_equations = 0;
  uint32_t maximum_equation_arity = 0;
  uint32_t maximum_tuples_per_equation = 0;
  uint32_t maximum_result_arity = 0;
  uint32_t maximum_result_tuples = 0;
  uint64_t maximum_additions = 0;
  uint64_t maximum_addition_literals = 0;
  uint64_t maximum_recognition_work = 0;
  uint64_t maximum_planning_work = 0;
  uint64_t maximum_duplicate_scan_work = 0;
  uint64_t maximum_revalidation_work = 0;
  uint64_t maximum_proof_work = 0;
  uint64_t maximum_proof_additions = 0;
  uint64_t maximum_proof_literal_occurrences = 0;
  uint64_t maximum_arena_words = 0;
  uint64_t maximum_journal_bytes = 0;
  uint32_t maximum_external_extent = 0;
  uint64_t maximum_session_keys = 0;
  uint64_t maximum_session_work = 0;

  bool valid() const;
  static Gf3AffineConsequenceParameters from_kernel(const Kernel &);
};

// Explicit GuardedCoverV2 profile; zero limits decline rather than inherit V1.
using DirectCspGuardedCoverParameters =
    DirectCspGuardedCoverActionLimits;

struct DirectCspSupportParameters : DirectCspSupportActionLimits {
  static DirectCspSupportParameters from_kernel(const Kernel &);
};


struct NaryCspEqualityParameters {
  uint64_t maximum_source_clauses = 0;
  uint64_t maximum_source_literals = 0;
  uint64_t maximum_recognition_work = 0;
  uint32_t maximum_domain_values = 0;
  uint64_t maximum_forbidden_tuples = 0;
  uint64_t maximum_proof_work = 0;
  uint64_t maximum_proof_additions = 0;
  uint64_t maximum_proof_literal_occurrences = 0;
  uint64_t maximum_journal_bytes = 0;
  uint32_t maximum_extent_growth = 0;
  uint32_t maximum_external_extent = 0;
  uint64_t maximum_session_keys = 0;

  bool valid() const;
  static NaryCspEqualityParameters from_kernel(const Kernel &);
  bool operator==(const NaryCspEqualityParameters &) const = default;
};

struct NaryCspEqualityAmoParameters {
  uint64_t maximum_source_clauses = 0;
  uint64_t maximum_source_literals = 0;
  uint64_t maximum_recognition_work = 0;
  uint32_t maximum_domain_values = 0;
  uint64_t maximum_forbidden_tuples = 0;
  uint64_t maximum_proof_work = 0;
  uint64_t maximum_proof_additions = 0;
  uint64_t maximum_proof_literal_occurrences = 0;
  uint64_t maximum_journal_bytes = 0;
  uint32_t maximum_external_extent = 0;
  uint64_t maximum_session_keys = 0;

  bool valid() const;
  static NaryCspEqualityAmoParameters from_kernel(const Kernel &);
  bool operator==(const NaryCspEqualityAmoParameters &) const = default;
};

struct NaryCspEqualityTransitivityParameters {
  uint64_t maximum_source_clauses = 0;
  uint64_t maximum_source_literals = 0;
  uint64_t maximum_recognition_work = 0;
  uint64_t maximum_planning_work = 0;
  uint32_t maximum_domain_values = 0;
  uint64_t maximum_forbidden_tuples = 0;
  uint64_t maximum_proof_work = 0;
  uint64_t maximum_proof_additions = 0;
  uint64_t maximum_proof_literal_occurrences = 0;
  uint64_t maximum_journal_bytes = 0;
  uint32_t maximum_external_extent = 0;
  uint64_t maximum_session_keys = 0;
  uint64_t maximum_session_bytes = 0;
  uint64_t maximum_arena_words = 0;
  uint64_t maximum_watch_words = 0;
  uint64_t maximum_preparation_bytes = 0;
  bool valid() const;
  static NaryCspEqualityTransitivityParameters from_kernel(const Kernel &);
  bool operator==(const NaryCspEqualityTransitivityParameters &) const = default;
};

// Exact controller-visible limits for the terminal direct pigeonhole
// refutation.  The three producer budgets and replay budget remain distinct
// so feedback can attribute work to the phase that consumed it.
struct DirectPhpRefutationParameters {
  uint64_t maximum_snapshot_clauses = 100000;
  uint64_t maximum_snapshot_literals = 1000000;
  uint64_t maximum_snapshot_work = 10000000;
  uint64_t maximum_recognition_clauses = 100000;
  uint64_t maximum_recognition_literals = 1000000;
  uint64_t maximum_recognition_work = 100000000;
  uint64_t maximum_row_candidates = 10000;
  uint64_t maximum_row_combinations = 1000000;
  uint64_t maximum_mapping_trials = 1000000;
  uint32_t maximum_holes = 20;
  uint64_t maximum_proof_additions = 1000000;
  uint64_t maximum_proof_deletions = 1000000;
  uint64_t maximum_proof_literals = 100000000;
  uint64_t maximum_fresh_variables = 1000000;
  uint64_t maximum_proof_events = 2000000;
  uint64_t maximum_replay_work = 100000000;

  bool valid() const;
  static DirectPhpRefutationParameters from_kernel(const Kernel &);
};

// Exact bounded profile for authenticated Gaussian closure over recognized
// GF(3) one-hot equations.  The controller supplies one stable identity for
// the complete authenticated Gaussian batch; these fields only define
// recognition/certificate/resource semantics.
struct Gf3GaussianConsequenceParameters {
  uint64_t maximum_source_clauses = 1000;
  uint64_t maximum_source_literals = 10000;
  uint32_t maximum_domains = 100;
  uint32_t maximum_equations = 100;
  uint32_t maximum_equation_arity = 8;
  uint32_t maximum_tuples_per_equation = 10000;
  uint64_t maximum_bridge_work = 1000000;
  uint64_t maximum_bridge_source_clauses = 10000;
  uint64_t maximum_bridge_source_literals = 100000;
  uint32_t maximum_bridge_domains = 100;
  uint32_t maximum_bridge_equations = 100;
  uint64_t maximum_bridge_terms = 1000;
  uint64_t maximum_authentication_references = 10000;
  uint64_t maximum_references_per_equation = 1000;
  uint64_t maximum_gaussian_work = 100000;
  uint64_t maximum_fill_entries = 100000;
  uint32_t maximum_intermediate_width = 32;
  uint32_t maximum_provenance_terms = 100;
  uint32_t maximum_derivations = 1000;
  uint64_t maximum_retained_plan_bytes = 1000000;
  uint64_t maximum_certificate_work = 10000000;
  uint64_t maximum_events = 100000;
  uint64_t maximum_event_literals = 1000000;
  uint64_t maximum_active_clauses = 100000;
  uint64_t maximum_retained_source_references = 10000;
  uint32_t maximum_event_width = 32;
  uint32_t maximum_branch_domains = 32;
  uint32_t maximum_consequences = 32;
  uint64_t maximum_capture_work = 1000000;
  uint64_t maximum_revalidation_work = 1000000;
  uint64_t maximum_proof_additions = 100000;
  uint64_t maximum_proof_literal_occurrences = 1000000;
  uint64_t maximum_journal_bytes = 1000000;
  uint64_t maximum_identity_bytes = 4000000;
  uint32_t maximum_external_extent = 100000;
  uint64_t maximum_session_keys = 10000;
  uint64_t maximum_session_work = 1000000;
  uint64_t maximum_session_identity_bytes = 4000000;

  bool valid() const;
  static Gf3GaussianConsequenceParameters from_kernel(const Kernel &);
};

template <class Parameters, class Visitor>
bool visit_gf3_gaussian_parameter_fields(Parameters &p, Visitor visit) {
  return visit(
      p.maximum_source_clauses, p.maximum_source_literals, p.maximum_domains,
      p.maximum_equations, p.maximum_equation_arity,
      p.maximum_tuples_per_equation, p.maximum_bridge_work,
      p.maximum_bridge_source_clauses, p.maximum_bridge_source_literals,
      p.maximum_bridge_domains, p.maximum_bridge_equations,
      p.maximum_bridge_terms, p.maximum_authentication_references,
      p.maximum_references_per_equation, p.maximum_gaussian_work,
      p.maximum_fill_entries, p.maximum_intermediate_width,
      p.maximum_provenance_terms, p.maximum_derivations,
      p.maximum_retained_plan_bytes, p.maximum_certificate_work,
      p.maximum_events, p.maximum_event_literals, p.maximum_active_clauses,
      p.maximum_retained_source_references, p.maximum_event_width,
      p.maximum_branch_domains, p.maximum_consequences, p.maximum_capture_work,
      p.maximum_revalidation_work, p.maximum_proof_additions,
      p.maximum_proof_literal_occurrences, p.maximum_journal_bytes,
      p.maximum_identity_bytes, p.maximum_external_extent,
      p.maximum_session_keys, p.maximum_session_work,
      p.maximum_session_identity_bytes);
}

template <class Target, class Source>
void copy_gf3_gaussian_parameter_fields(Target &target,
                                        const Source &source) {
  visit_gf3_gaussian_parameter_fields(target, [&](auto &...targets) {
    return visit_gf3_gaussian_parameter_fields(
        source, [&](const auto &...sources) {
          ((targets = sources), ...);
          return true;
        });
  });
}

// One authenticated forbidden tuple of one non-unit GF(3) Gaussian row.
// Gaussian supplies source/bridge limits; the appended fields bound the
// single-row Boolean certificate and retained exact replay identity.
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

struct TseitinCycleBreakingParameters {
  uint64_t maximum_snapshot_clauses = 100000;
  uint64_t maximum_snapshot_literals = 1000000;
  uint64_t maximum_snapshot_work = 10000000;
  uint32_t maximum_snapshot_variables = 1000000;
  uint64_t maximum_discovery_work = 10000000;
  uint64_t maximum_discovery_clauses = 100000;
  uint64_t maximum_discovery_groups = 100000;
  // Canonical retained payload bytes only; excludes allocator/control-block
  // overhead and vector capacity beyond live elements.
  uint64_t maximum_discovery_retained_bytes = 16000000;
  uint64_t maximum_admission_work = 10000000;
  uint64_t maximum_planning_work = 10000000;
  uint32_t maximum_witness_length = 100000;
  uint32_t maximum_planning_additions = 1;
  uint64_t maximum_transaction_work = 10000000;
  uint64_t maximum_transaction_additions = 3;
  uint64_t maximum_transaction_deletions = 2;
  uint64_t maximum_transaction_helpers = 2;
  uint64_t maximum_transaction_witness_literals = 1000000;
  uint64_t maximum_validation_ticks = 10000000;
  uint64_t maximum_components = 10000;
  uint64_t maximum_identity_bytes = 4000000;
  uint64_t maximum_total_identity_bytes = 16000000;
  uint64_t maximum_identity_work = 32000000;
  uint64_t maximum_journal_bytes = 8000000;
  uint64_t maximum_session_keys = 10000;
  uint64_t maximum_session_work = 1000000;
  uint64_t maximum_session_identity_bytes = 16000000;

  bool valid() const;
  static TseitinCycleBreakingParameters from_kernel(const Kernel &);
};

struct SemanticVariableAdviceParameters {
  uint32_t maximum_variables = 0;
  uint64_t maximum_priority_sum = 0;
  uint32_t existing_theory_priority = 0;
  uint32_t introduced_theory_priority = 0;
  bool valid() const { return true; }
  static SemanticVariableAdviceParameters from_kernel(const Kernel &);
};

// One ownership point for the typed public-parameter to native-action mapping.
// These conversions are pure aggregate construction and perform no validation.
AffineBasisActionLimits action_limits(const AffineConsequenceParameters &);
Gf3AffineActionLimits action_limits(const Gf3AffineConsequenceParameters &);
DirectCspGuardedCoverActionLimits
action_limits(const DirectCspGuardedCoverParameters &);
DirectCspSupportActionLimits action_limits(const DirectCspSupportParameters &);
NaryCspEqualityLimits action_limits(const NaryCspEqualityParameters &);
NaryCspEqualityAmoLimits action_limits(const NaryCspEqualityAmoParameters &);
NaryCspEqualityTransitivityLimits
action_limits(const NaryCspEqualityTransitivityParameters &);
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
