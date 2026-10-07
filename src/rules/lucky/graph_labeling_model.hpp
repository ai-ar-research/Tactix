#pragma once

#include "kernel/kernel.hpp"
#include "rules/registry.hpp"

#include "rules/lucky/graph_labeling_session.hpp"

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


RuleEligibility graph_labeling_model_guard(const Kernel &);
GraphLabelingAuthenticationResult graph_labeling_model_authenticate(
    Kernel &, GraphLabelingModelSession &);
GraphLabelingSliceResult graph_labeling_model_slice(
    Kernel &, GraphLabelingModelSession &);
ApplyResult graph_labeling_model_apply(Kernel &);
} // namespace tactix
