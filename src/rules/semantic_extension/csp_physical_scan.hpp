#pragma once

#include "kernel/kernel.hpp"
#include "rules/semantic_extension/interruptible_work.hpp"

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace tactix::csp_physical_scan {

enum class Status : uint8_t { Complete, Stopped, Declined };

// A metered CSP source capture's outcome.
enum class CaptureStatus : uint8_t {
  Complete, NoStructure, ResourceExhausted, Interrupted, Invalid
};

inline CaptureStatus stopped_capture(const InterruptibleWork &meter) {
  return stopped_status(meter, CaptureStatus::Invalid);
}

// Canonical premise order: by literals, then by native proof ID.
template <class Clause, class Id>
void sort_premises(std::vector<Clause> &clauses, Id Clause::*native_id) {
  std::sort(clauses.begin(), clauses.end(),
            [native_id](const Clause &first, const Clause &second) {
              if (first.literals != second.literals)
                return first.literals < second.literals;
              return first.*native_id < second.*native_id;
            });
}

// The public image of a capture outcome: Complete is Applied, and every other
// outcome keeps its name.
template <class Public> Public public_capture_status(CaptureStatus status) {
  switch (status) {
  case CaptureStatus::Complete: return Public::Applied;
  case CaptureStatus::NoStructure: return Public::NoStructure;
  case CaptureStatus::ResourceExhausted: return Public::ResourceExhausted;
  case CaptureStatus::Interrupted: return Public::Interrupted;
  case CaptureStatus::Invalid: return Public::Invalid;
  }
  std::abort();
}

struct FormulaMark {
  KernelChangeMark change;
  uint64_t semantic_namespace = 0;
  uint64_t arena_generation = 0;
  uint64_t next_clause_id = 0;
  uint32_t variables = 0;
  size_t external_extent = 0;

  bool operator==(const FormulaMark &) const = default;
};

inline FormulaMark formula_mark(const Kernel &kernel) {
  return {kernel_change_mark(kernel), kernel.affine_semantic_namespace_epoch,
          kernel.arena_generation, kernel.next_clause_id, kernel.nvars,
          kernel.external_to_internal.size()};
}

// An assignment-free, propagated, sparse root with a nonempty external extent
// that fits signed external literals. Pure: it polls no termination.
inline bool root_ready(const Kernel &kernel) {
  return !kernel.inconsistent && !kernel.level && !kernel.conflict_valid &&
         !kernel.pending.valid && kernel.trail.empty() && !kernel.dense &&
         !kernel.probing.recording && kernel.large_watches_connected &&
         kernel_propagation_fixpoint(kernel) &&
         !kernel.external_to_internal.empty() &&
         kernel.external_to_internal.size() <= INT32_MAX;
}

// Exact physical premises in stable arena-then-binary order. `RawBinaryIds`
// reports kernel_binary_id even when the table does not track proof IDs.
template <bool RawBinaryIds = false, class Meter, class Observe>
Status run(const Kernel &kernel, Meter &meter, Observe &observe) {
  for (const auto [reference, clause] : arena_records(kernel.arena)) {
    if (!meter.charge(1)) return Status::Stopped;
    if (!clause->garbage() &&
        !observe(clause->lits, clause->size, clause->id()))
      return Status::Declined;
  }
  for (Lit first = 0; first < 2 * kernel.nvars; ++first)
    for (const WatchEntry watch : kernel.watches.entries(first)) {
      if (!meter.charge(1)) return Status::Stopped;
      if (!watch.binary() || first >= watch.other()) continue;
      const Lit pair[] = {first, watch.other()};
      const uint64_t native_id =
          RawBinaryIds || kernel.binary_ids.tracks_values()
              ? kernel_binary_id(kernel, first, watch.other())
              : 0;
      if (!observe(pair, 2, native_id)) return Status::Declined;
    }
  return Status::Complete;
}

} // namespace tactix::csp_physical_scan
