#pragma once

#include <cstdint>
#include <vector>

namespace tactix::independent_set_extension {

// A stable occurrence in the source view supplied by the eventual action.
// The recognizer is deliberately independent of Kernel so that a caller can
// use either a revalidated startup view or an exact live-clause snapshot.
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

// Return false to interrupt or exhaust a caller-owned allowance.  Every call
// is monotone; recognition and certificate construction publish no partial
// result when the callback declines a charge.
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
  // All exact duplicate occurrences are retained even though one would
  // suffice logically.  This keeps the source accounting lossless.
  std::vector<uint64_t> premises;
};

// Exact owner-family image.  Vertex variables are the prefix 1..vertices;
// state variables are the following contiguous layers.  This indexed layout
// is only a proposal: recognition succeeds only after every defining clause
// and every source clause has been classified with exact provenance.
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

  // Every supplied occurrence, sorted.  Strict recognition requires the
  // source to consist exactly of the counter clauses and graph-edge clauses.
  std::vector<uint64_t> classified_occurrences;
  uint64_t inspected_work = 0;
};

// Recognize the complete layered exact-count encoding used by the historical
// LymphoSAT independent-set constructor.  Required clauses are:
//
//   (x1 v S[1,0]) and (-x1 v S[1,1]);
//   (-S[i-1,s] v xi v S[i,s]) and
//   (-S[i-1,s] v -xi v S[i,min(s+1,K)]) for i >= 2;
//   pairwise AMO for every state layer i >= 2;
//   (-S[N,s] v S[N,K]) for s < K; and
//   (S[N,0] v ... v S[N,K]).
//
// Negative vertex binaries are the graph edges.  The exact clauses imply that
// every model selects at least K vertices; the edge clauses make them an
// independent set.  The source must contain at least one edge.  Failure,
// ambiguity, interruption, malformed input, or any missing/extra clause leaves
// `out` empty.
bool recognize(const std::vector<Premise> &source,
               uint32_t external_variables, const Limits &limits,
               Charge charge, void *charge_context, Image &out);

enum class CertificateStepKind : uint8_t {
  ForbiddenAssignment,
  Resolution,
};

struct CertificateStep {
  CertificateStepKind kind = CertificateStepKind::ForbiddenAssignment;
  std::vector<int32_t> clause;

  // A leaf is RUP from this exact unit-propagation path: base, one transition
  // per later vertex, terminal implication, terminal AMO.  A resolution step
  // is RUP from the two earlier additions named by left/right.
  std::vector<uint64_t> premise_occurrences;
  uint32_t left = UINT32_MAX;
  uint32_t right = UINT32_MAX;
  int32_t pivot = 0;
};

struct CoverCertificate {
  // A positive clause over N-K+1 vertices.  At least K selected vertices imply
  // that every such cover contains a selected vertex.
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

// Construct a checker-independent RUP plan for one compact cardinality clause.
// Leaves exclude all complete assignments in which `cover_vertices` are false;
// binary resolution then eliminates the remaining K-1 vertices.  A kernel
// adapter can emit the steps, publish the conclusion, and delete proof-only
// intermediates.  This function neither mutates a solver nor searches the
// graph.  Failure has zero output effect.
bool derive_cover_certificate(const Image &image,
                              const std::vector<uint32_t> &cover_vertices,
                              const CertificateLimits &limits, Charge charge,
                              void *charge_context, CoverCertificate &out);

} // namespace tactix::independent_set_extension
