#pragma once

// Support shared by the substitution-redundancy detectors (orbitopal.cpp,
// symmetry.cpp) and the SR application entry points.

#include "kernel/kernel.hpp"
#include "rules/substitution_redundancy/substitution_redundancy.hpp"
#include "symmetry_backend/dejavu_backend.hpp"

#include <csignal>
#include <cstdint>
#include <utility>
#include <vector>

namespace tactix::sr_detail {

// Query assumptions constrain the projection even while unassigned; only the
// selected pointwise stabilizer colors them.
inline void stabilize_query_coordinates(const Kernel &kernel, bool enabled,
                                          std::vector<uint32_t> &colors,
                                          uint32_t literals) {
  if (!enabled || !kernel.query_projection) return;
  for (const int32_t external : kernel.query_projection->constraints()) {
    const Lit literal = kernel_internalize(kernel, external);
    if (literal == INVALID_LIT || literal >= literals) continue;
    colors[literal] = 3+literal;
    colors[negate(literal)] = 3+negate(literal);
  }
}

// A backend input graph whose first vertices are the literals.
struct LiteralGraph {
  uint32_t vertex_count = 0;
  uint64_t edge_count = 0;
  std::vector<uint32_t> colors;
  std::vector<uint32_t> degrees;
  std::vector<uint32_t> edges;

  // Sizes an edgeless graph. Literals are color 0 unless individualized (a
  // retired variable, or a root-assigned literal or query coordinate of the
  // pointwise stabilizer); every other vertex is color 1.
  void reset(const Kernel &kernel, bool pointwise_root_stabilizer,
             uint32_t vertices, uint64_t edge_total, uint32_t literals) {
    vertex_count = vertices;
    edge_count = edge_total;
    colors.assign(vertices, 1);
    degrees.assign(vertices, 0);
    edges.clear();
    edges.reserve(static_cast<size_t>(2 * edge_total));
    for (Lit literal = 0; literal < literals; ++literal)
      colors[literal] = (kernel_var_retired(kernel, var_of(literal)) ||
                         (pointwise_root_stabilizer && kernel.value(literal)))
                            ? 3 + literal
                            : 0;
    stabilize_query_coordinates(kernel, pointwise_root_stabilizer, colors,
                                literals);
  }

  void add_edge(uint32_t first, uint32_t second) {
    if (first > second) std::swap(first, second);
    edges.push_back(first);
    edges.push_back(second);
    ++degrees[first];
    ++degrees[second];
  }

  // A color-2 `vertex` joining `variable`'s two literals.
  void add_negation_gadget(Var variable, uint32_t vertex) {
    colors[vertex] = 2;
    add_edge(make_lit(variable, false), vertex);
    add_edge(make_lit(variable, true), vertex);
  }

  symmetry_backend::Graph backend() const {
    return {vertex_count, edge_count, colors.data(), degrees.data(),
            edges.data()};
  }
};

// A pure read of the caller's and the kernel's stop flags. It must not poll
// kernel_terminating, whose stop-fuzz draws belong to the schedule.
inline bool stop_requested(const Kernel &kernel, volatile sig_atomic_t *stop) {
  return (stop && *stop) || (kernel.stop && *kernel.stop);
}

struct StopContext {
  const Kernel *kernel;
  volatile sig_atomic_t *stop;
};

// Backend stop callback over a StopContext.
inline bool backend_stop(void *opaque) noexcept {
  const auto &context = *static_cast<const StopContext *>(opaque);
  return stop_requested(*context.kernel, context.stop);
}

// Record `status` in a pass result and return that result.
template <class Result, class Status>
Result finish(Result &result, Status status) {
  result.status = status;
  return result;
}

// Record `status` in a pass result and fail the current step.
template <class Result, class Status> bool fail(Result &result, Status status) {
  result.status = status;
  return false;
}

inline uint64_t saturating_add(uint64_t first, uint64_t second) {
  return second > UINT64_MAX - first ? UINT64_MAX : first + second;
}

// Backend detection over the configured graph-symmetry controls, returning
// the first `prefix` vertices of each sparse generator. Initial and progress
// generator caps keep their backend defaults.
inline symmetry_backend::DetectionSettings
detection_settings(const GraphSymmetryParameters &configured, uint32_t prefix) {
  symmetry_backend::DetectionSettings settings;
  settings.seed = configured.seed;
  settings.error_bound = configured.error_bound;
  settings.search_budget = configured.backend_search_budget;
  settings.support_budget = configured.backend_support_budget;
  settings.component_size_limit = configured.backend_component_size_limit;
  settings.maximum_generators = configured.maximum_generators;
  settings.preserve_generator_order = configured.preserve_detector_order;
  settings.returned_prefix_size = prefix;
  settings.sparse_output = true;
  settings.decompose = configured.decompose;
  settings.prefer_depth_first = configured.prefer_depth_first;
  settings.strong_certification = configured.strong_certification;
  return settings;
}

} // namespace tactix::sr_detail
