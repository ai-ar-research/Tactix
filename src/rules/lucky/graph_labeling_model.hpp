#pragma once

#include "kernel/kernel.hpp"
#include "rules/registry.hpp"

#include <memory>

namespace tactix {

enum class GraphLabelingAuthenticationStatus : uint8_t {
  NoStructure,
  Authenticated,
  Exhausted,
  Interrupted,
};

struct GraphLabelingAuthenticationResult {
  GraphLabelingAuthenticationStatus status =
      GraphLabelingAuthenticationStatus::NoStructure;
  uint64_t work = 0;
};

enum class GraphLabelingSliceStatus : uint8_t {
  NoStructure,
  Pending,
  TrajectoryComplete,
  Sat,
  Interrupted,
};

struct GraphLabelingSliceResult {
  ApplyResult result = ApplyResult::NoOp;
  GraphLabelingSliceStatus status = GraphLabelingSliceStatus::NoStructure;
  uint64_t work = 0;
  uint64_t objective_before = UINT64_MAX;
  uint64_t objective_after = UINT64_MAX;
  bool hint_published = false;
  bool useful_hint = false;
};

// A policy-owned SAT-only constructor session. Authentication atomically
// captures the indexed graph before source-changing preprocessing. Later root
// slices retain the graph, best permutation and stage cursor; their only
// nonterminal effect is an optional phase hint. No clause or proof fact is
// derived from this heuristic state.
class GraphLabelingModelSession {
public:
  struct State;
  GraphLabelingModelSession();
  ~GraphLabelingModelSession();
  GraphLabelingModelSession(const GraphLabelingModelSession &) = delete;
  GraphLabelingModelSession &operator=(const GraphLabelingModelSession &) = delete;
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

RuleEligibility graph_labeling_model_guard(const Kernel &);
GraphLabelingAuthenticationResult graph_labeling_model_authenticate(
    Kernel &, GraphLabelingModelSession &);
GraphLabelingSliceResult graph_labeling_model_slice(
    Kernel &, GraphLabelingModelSession &);
ApplyResult graph_labeling_model_apply(Kernel &);
} // namespace tactix
