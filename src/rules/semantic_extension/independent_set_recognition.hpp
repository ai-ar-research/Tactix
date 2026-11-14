#pragma once

#include <cstdint>
#include <vector>

namespace tactix::independent_set_extension {

// A stable occurrence in a caller-supplied (Kernel-independent) source view.
struct Premise {
  uint64_t occurrence = 0;
  std::vector<int32_t> literals;
};

struct Limits {
  uint64_t maximum_clauses = 0;
  uint64_t maximum_literals = 0;
  uint64_t maximum_work = 0;
  uint32_t maximum_vertices = 0;
  uint32_t maximum_target = 0;
};

// A monotone charge; false stops without a partial result.
using Charge = bool (*)(void *context, uint64_t amount);

struct Transition {
  uint32_t layer = 0;       // vertex read by this transition, 1 based
  uint32_t from = 0;
  uint32_t on_false = 0;
  uint32_t on_true = 0;
  uint64_t false_premise = 0;
  uint64_t true_premise = 0;
};

struct LayerExclusion {
  uint32_t layer = 0;
  uint32_t first = 0;
  uint32_t second = 0;
  uint64_t premise = 0;
};

struct TerminalImplication {
  uint32_t state = 0;
  uint64_t premise = 0;
};

struct Edge {
  uint32_t first = 0;
  uint32_t second = 0;
  // Every duplicate occurrence, for lossless accounting.
  std::vector<uint64_t> premises;
};

// Owner-family image: vertex variables 1..vertices, then contiguous state
// layers; valid only once every clause is classified with provenance.
struct Image {
  uint32_t external_variables = 0;
  uint32_t vertices = 0;
  uint32_t target = 0;
  std::vector<int32_t> vertex_literals;
  std::vector<std::vector<int32_t>> layers;

  uint64_t base_false_premise = 0;
  uint64_t base_true_premise = 0;
  std::vector<Transition> transitions;
  std::vector<LayerExclusion> exclusions;
  std::vector<TerminalImplication> terminal_implications;
  uint64_t terminal_cover_premise = 0;
  std::vector<Edge> edges;

  // Every occurrence, sorted: exactly the counter and edge clauses.
  std::vector<uint64_t> classified_occurrences;
  uint64_t inspected_work = 0;
};

// Recognize the layered exact-count encoding (grammar and soundness:
// docs/dev/calculus.md); any unclassified clause leaves `out` empty.
bool recognize(const std::vector<Premise> &source,
               uint32_t external_variables, const Limits &limits,
               Charge charge, void *charge_context, Image &out);

enum class CertificateStepKind : uint8_t { ForbiddenAssignment, Resolution };

struct CertificateStep {
  CertificateStepKind kind = CertificateStepKind::ForbiddenAssignment;
  std::vector<int32_t> clause;

  // A leaf is RUP by base, transitions, terminal implication and AMO; a
  // resolution step by the additions left/right.
  std::vector<uint64_t> premise_occurrences;
  uint32_t left = UINT32_MAX;
  uint32_t right = UINT32_MAX;
  int32_t pivot = 0;
};

struct CoverCertificate {
  // A positive clause over N-K+1 vertices.
  std::vector<uint32_t> cover_vertices;
  std::vector<CertificateStep> steps;
  std::vector<int32_t> conclusion;
  uint64_t literal_occurrences = 0;
  uint64_t work = 0;
};

struct CertificateLimits {
  uint64_t maximum_additions = 0;
  uint64_t maximum_literal_occurrences = 0;
  uint64_t maximum_work = 0;
};

// A RUP plan for one cover clause: leaves exclude assignments with
// `cover_vertices` false, then binary resolution eliminates the remaining
// K-1 vertices. Failure has no output effect.
bool derive_cover_certificate(const Image &image,
                              const std::vector<uint32_t> &cover_vertices,
                              const CertificateLimits &limits, Charge charge,
                              void *charge_context, CoverCertificate &out);

} // namespace tactix::independent_set_extension
