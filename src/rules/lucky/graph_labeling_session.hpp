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
  // DefaultPolicy uses the retained profile's construction dose as the
  // per-invocation Window allowance. The Window stage keeps its independent
  // total cap; zero means the current stage needs no policy-imposed slice.
  uint64_t recommended_continuation_work() const;
  void clear();
  // The typed result of the last public Lucky 26/27 application: its public
  // observation (null before one).
  void record_public_result(const GraphLabelingAuthenticationResult &,
                            const GraphLabelingSliceResult &);
  const GraphLabelingAuthenticationResult *last_authentication() const {
    return last_authentication_.get();
  }
  const GraphLabelingSliceResult *last_slice() const { return last_slice_.get(); }

private:
  std::unique_ptr<State> state_;
  std::unique_ptr<GraphLabelingAuthenticationResult> last_authentication_;
  std::unique_ptr<GraphLabelingSliceResult> last_slice_;
  friend GraphLabelingAuthenticationResult graph_labeling_model_authenticate(
      Kernel &, GraphLabelingModelSession &);
  friend GraphLabelingSliceResult graph_labeling_model_slice(
      Kernel &, GraphLabelingModelSession &);
};

} // namespace tactix
