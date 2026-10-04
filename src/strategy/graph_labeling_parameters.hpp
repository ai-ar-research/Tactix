#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace tactix {
struct Kernel;
namespace strategy {

enum class GraphLabelingStage : uint8_t {
  Recognition, Construction, Window, Repair, Anneal, FinalRepair,
  Completion, Publication, Count
};
constexpr size_t GRAPH_LABELING_STAGES = static_cast<size_t>(GraphLabelingStage::Count);

// A single constructor action has separately dosed algorithm stages. These
// allowances sum to its effort ceiling; zero disables a stage. No wall-clock
// calibration or solver-specific performance default is invented here. The
// owning policy must install its reviewed nonzero effort profile explicitly.
struct GraphLabelingParameters {
  uint32_t maximum_vertices = UINT32_MAX;
  uint64_t workspace_bytes = UINT64_MAX;
  std::array<uint64_t, GRAPH_LABELING_STAGES> work{};
  // LymphoSAT's bounded window-search profile.
  uint32_t maximum_dfs_vertices = 140;
  uint64_t maximum_dfs_nodes = 50000000;
  uint32_t maximum_dfs_starts = 12;
  uint32_t early_dfs_choices = 28;
  uint64_t repair_rounds = UINT64_MAX;
  uint64_t anneal_iterations = UINT64_MAX;
  uint64_t stall_anneal_iterations = 0;
  uint64_t seed = 0;

  constexpr bool valid() const {
    if (maximum_vertices < 3) return false;
    uint64_t remaining = UINT64_MAX;
    for (uint64_t allowance : work) {
      if (allowance > remaining) return false;
      remaining -= allowance;
    }
    return true;
  }
  static GraphLabelingParameters from_kernel(const Kernel &);
};

} // namespace strategy
} // namespace tactix
