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
void install_rule_parameters(Kernel &, const SemanticVariableAdviceParameters &);

} // namespace strategy
} // namespace tactix
