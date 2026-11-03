#pragma once

// Support shared by the substitution-redundancy detectors (orbitopal.cpp,
// symmetry.cpp) and the SR application entry points.

#include "kernel/kernel.hpp"
#include "rules/substitution_redundancy/substitution_redundancy.hpp"
#include "symmetry_backend/dejavu_backend.hpp"

#include <csignal>
#include <cstdint>

namespace tactix::sr_detail {

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
