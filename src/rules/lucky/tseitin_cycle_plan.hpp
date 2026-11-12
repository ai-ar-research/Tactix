// Pure bounded discovery of one Tseitin cycle-space X7 candidate from a
// complete occurrence classification; the caller rebuilds the abstraction
// after each applied unit.
#pragma once

#include <cstdint>
#include <vector>

namespace tactix::tseitin_cycle {

using Edge = int32_t;
using Vertex = uint32_t;

struct VertexEquation {
  Vertex vertex = 0;
  bool charge = false;
  std::vector<Edge> edges;
};

enum class OccurrenceKind : uint8_t { VertexParity, Residual };

struct ClassifiedOccurrence {
  Edge edge = 0;
  OccurrenceKind kind = OccurrenceKind::Residual;
  // Meaningful exactly for VertexParity occurrences.
  Vertex vertex = 0;
};

struct Input {
  std::vector<VertexEquation> equations;
  std::vector<ClassifiedOccurrence> occurrences;
  // Edge variables fixed at the current root boundary.  They remain part of
  // the authenticated graph, but a cycle witness may not substitute them.
  std::vector<Edge> unavailable_edges;
  // False means the producer stopped before classifying the whole live formula.
  bool occurrence_classification_complete = false;
};

struct Limits {
  uint64_t work = 0;
  uint32_t witness_length = 0;
  uint32_t additions = 0;
  bool (*interrupted)(void *) = nullptr;
  void *interrupt_state = nullptr;
};

enum class DeclineReason : uint8_t {
  None,
  NoAdditionAllowance,
  WorkLimit,
  Interrupted,
  IncompleteOccurrenceClassification,
  EmptySystem,
  InvalidVertex,
  DuplicateVertex,
  InvalidEdge,
  DuplicateEdgeInEquation,
  ResidualOccurrence,
  OccurrenceMismatch,
  EdgeNotInExactlyTwoVertices,
  Disconnected,
  EvenCharge,
  Acyclic,
  WitnessLengthLimit,
  InvalidCandidate,
};

enum class ImageKind : uint8_t { Constant, NegatedSelf };

struct SubstitutionImage {
  Edge source = 0;
  ImageKind kind = ImageKind::Constant;
  bool constant = false;
};

struct Candidate {
  // The proposed clause is the negative unit (-pivot), so false is its
  // satisfying value and true is the candidate's falsifying assignment.
  Edge pivot = 0;
  bool satisfying_value = false;
  // The pivot is first; the remaining entries are the deterministic tree path.
  std::vector<Edge> cycle_edges;
  // pivot -> false; every other cycle edge -> its negation.
  std::vector<SubstitutionImage> substitution;
};

struct Result {
  Candidate candidate;
  DeclineReason decline = DeclineReason::None;
  uint64_t work = 0;
  // Always true on success.  Sequential root propagation is intentionally not
  // simulated by this pure planner.
  bool requires_live_revalidation = false;
  bool planned() const { return decline == DeclineReason::None; }
};

// At most one candidate: a connected odd-charge graph whose edges each occur
// in exactly two vertex equations with no residual touch. Kruskal runs over
// the ascending available-edge subgraph.
Result plan_one(const Input &, Limits);

// Recheck the structural X7 witness shape against the abstraction.
bool validate_candidate(const Input &, const Candidate &);

// Bounded form: `work` counts every scan, `exhausted` marks truncation.
bool validate_candidate(const Input &, const Candidate &, uint64_t maximum_work,
                        bool (*interrupted)(void *), void *interrupt_state,
                        uint64_t &work, bool &exhausted,
                        bool &was_interrupted);

// Semantic oracle for tests: assignment[i] is edge i+1, pivot is true, and
// every vertex parity keeps its truth value under the substitution.
bool preserves_relations_on_falsifying_assignment(
    const Input &, const Candidate &, const std::vector<bool> &assignment);

} // namespace tactix::tseitin_cycle
