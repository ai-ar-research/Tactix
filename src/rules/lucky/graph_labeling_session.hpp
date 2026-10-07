#pragma once

#include <cstdint>
#include <memory>

namespace tactix {
struct Kernel;
struct GraphLabelingAuthenticationResult;
struct GraphLabelingSliceResult;

// A Kernel-owned SAT-only constructor session. Authentication atomically
// captures the indexed graph before source-changing preprocessing. Later root
// slices retain the graph, best permutation and stage cursor; their only
// nonterminal effect is an optional phase hint. No clause or proof fact is
// derived from this heuristic state.
class GraphLabelingModelSession {
public:
  struct State;
  GraphLabelingModelSession();
  ~GraphLabelingModelSession();
  GraphLabelingModelSession(const GraphLabelingModelSession &);
  GraphLabelingModelSession &operator=(const GraphLabelingModelSession &);
  GraphLabelingModelSession(GraphLabelingModelSession &&) noexcept;
  GraphLabelingModelSession &operator=(GraphLabelingModelSession &&) noexcept;
  bool authenticated() const;
  bool complete() const;
  uint64_t candidate_fingerprint() const;
  void clear();

private:
  std::unique_ptr<State> state_;
  friend GraphLabelingAuthenticationResult graph_labeling_model_authenticate(
      Kernel &, GraphLabelingModelSession &);
  friend GraphLabelingSliceResult graph_labeling_model_slice(
      Kernel &, GraphLabelingModelSession &);
};

} // namespace tactix
