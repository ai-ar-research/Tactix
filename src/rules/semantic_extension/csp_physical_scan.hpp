#pragma once

#include "kernel/kernel.hpp"

namespace tactix::csp_physical_scan {

enum class Status : uint8_t { Complete, Stopped, Declined };

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

// Exact physical premises in stable arena-then-binary order.
template <class Meter, class Observe>
Status run(const Kernel &kernel, Meter &meter, Observe &observe) {
  for (Ref reference = 0; reference < kernel.arena.size();) {
    const Clause *clause = clause_at(kernel.arena, reference);
    reference += clause_actual_words(clause);
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
      const uint64_t native_id = kernel.binary_ids.tracks_values()
                                     ? kernel_binary_id(kernel, first,
                                                        watch.other())
                                     : 0;
      if (!observe(pair, 2, native_id)) return Status::Declined;
    }
  return Status::Complete;
}

} // namespace tactix::csp_physical_scan
