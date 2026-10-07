#pragma once

#include <cstdint>

namespace tactix {
struct Kernel;
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
struct DirectCspGuardedCoverParameters {
  uint64_t maximum_source_clauses = 0;
  uint64_t maximum_source_literals = 0;
  uint64_t maximum_recognition_work = 0;
  uint32_t maximum_domains = 0;
  uint32_t maximum_domain_values = 0;
  uint32_t maximum_cover_literals = 0;
  uint32_t maximum_or_definitions = 0;
  uint32_t maximum_or_inputs = 0;
  uint32_t maximum_output_literals = 0;
  uint64_t maximum_proof_work = 0;
  uint64_t maximum_proof_additions = 0;
  uint64_t maximum_proof_literal_occurrences = 0;
  uint64_t maximum_journal_bytes = 0;
  uint32_t maximum_external_extent = 0;
  uint64_t maximum_session_keys = 0;
  uint64_t maximum_session_work = 0;
  bool valid() const;
  bool operator==(const DirectCspGuardedCoverParameters &) const = default;
};

struct DirectCspSupportParameters {
  uint64_t maximum_source_clauses = 0;
  uint64_t maximum_source_literals = 0;
  uint64_t maximum_recognition_work = 0;
  uint32_t maximum_domain_values = 0;
  uint32_t maximum_direct_clause_literals = 0;
  uint32_t maximum_definition_clause_literals = 0;
  uint64_t maximum_proof_work = 0;
  uint64_t maximum_proof_additions = 0;
  uint64_t maximum_proof_literal_occurrences = 0;
  uint64_t maximum_journal_bytes = 0;
  uint32_t maximum_extent_growth = 0;
  uint32_t maximum_external_extent = 0;
  uint64_t maximum_session_keys = 0;
  uint64_t maximum_session_work = 0;
  bool allow_extension = false;

  bool valid() const;
  static DirectCspSupportParameters from_kernel(const Kernel &);
};

struct NaryCspPrefixParameters {
  uint64_t maximum_source_clauses = 0;
  uint64_t maximum_source_literals = 0;
  uint64_t maximum_recognition_work = 0;
  uint32_t maximum_domains = 0;
  uint32_t maximum_domain_values = 0;
  uint64_t maximum_forbidden_tuples = 0;
  uint32_t maximum_prefix_arity = 0;
  uint32_t maximum_definition_clause_literals = 0;
  uint64_t maximum_proof_work = 0;
  uint64_t maximum_proof_additions = 0;
  uint64_t maximum_proof_literal_occurrences = 0;
  uint64_t maximum_journal_bytes = 0;
  uint32_t maximum_extent_growth = 0;
  uint32_t maximum_external_extent = 0;
  uint64_t maximum_session_keys = 0;
  uint64_t maximum_session_work = 0;

  bool valid() const;
  static NaryCspPrefixParameters from_kernel(const Kernel &);
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

void install_rule_parameters(Kernel &, const AffineConsequenceParameters &);
void install_rule_parameters(Kernel &, const ExactCoverExtensionParameters &);
void install_rule_parameters(Kernel &, const IndependentSetCoverParameters &);
void install_rule_parameters(Kernel &, const Gf3AffineConsequenceParameters &);
void install_rule_parameters(Kernel &, const DirectCspSupportParameters &);
void install_rule_parameters(Kernel &, const NaryCspPrefixParameters &);
void install_rule_parameters(Kernel &, const NaryCspEqualityParameters &);
void install_rule_parameters(Kernel &, const NaryCspEqualityAmoParameters &);
void install_rule_parameters(Kernel &, const DirectPhpRefutationParameters &);
void install_rule_parameters(Kernel &, const Gf3GaussianConsequenceParameters &);
void install_rule_parameters(Kernel &,
                             const Gf3GeneralRowConsequenceParameters &);
void install_rule_parameters(Kernel &, const TseitinCycleBreakingParameters &);
void install_rule_parameters(Kernel &, const SemanticVariableAdviceParameters &);

} // namespace strategy
} // namespace tactix
