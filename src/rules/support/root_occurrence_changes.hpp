#pragma once

#include "kernel/kernel.hpp"

namespace tactix {

// BCE/CCE omit root-satisfied opposite occurrences. Record those removals
// during their existing arena pass; binary occurrences are already watched.
// Shared removed marks re-offer only affected neighborhoods to both rules.
class RootOccurrenceChanges {
  Kernel &k_;
  const uint32_t from_, fixed_;

public:
  explicit RootOccurrenceChanges(Kernel &k)
      : k_(k), from_(k.root_occurrence_head), fixed_(kernel_root_fixed(k)) {
    assert(!k.level && from_ <= fixed_);
  }

  void note(const Clause *clause) {
    if (from_ == fixed_ || clause->garbage()) return;
    for (Lit literal : *clause) {
      if (kernel_root_value(k_, literal) <= 0 ||
          k_.assigned[var_of(literal)].trail < from_) continue;
      for (Lit member : *clause) kernel_removed_var(k_, var_of(member));
      break;
    }
  }

  void finish() {
    for (uint32_t i = from_; i < fixed_; ++i) {
      const Lit literal = k_.trail[i];
      for (WatchEntry watch : k_.watches.entries(literal))
        if (watch.binary()) kernel_removed_var(k_, var_of(watch.other()));
    }
    k_.root_occurrence_head = fixed_;
  }
};

} // namespace tactix
