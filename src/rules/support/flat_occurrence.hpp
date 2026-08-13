#pragma once

#include "kernel/kernel.hpp"

#include <span>
#include <vector>

namespace tactix {

// Deterministic CSR-style literal-to-large-clause index. The caller chooses
// the clause set; entries retain that set's order within every literal.
struct FlatOccurrenceIndex {
  std::vector<uint32_t> offsets;
  std::vector<Ref> refs;

  void build(const Kernel &, const std::vector<Ref> &clauses);
  std::span<const Ref> operator[](Lit lit) const {
    return std::span<const Ref>(refs).subspan(
        offsets[lit], offsets[lit + 1] - offsets[lit]);
  }
  size_t size(Lit lit) const { return offsets[lit + 1] - offsets[lit]; }
};

} // namespace tactix
