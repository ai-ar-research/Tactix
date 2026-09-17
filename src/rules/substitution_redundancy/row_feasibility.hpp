#pragma once

#include "kernel/types.hpp"
#include "symmetry_backend/dejavu_backend.hpp"
#include <cassert>
#include <vector>

namespace tactix {

// A completed row can only remove possible new columns. A source with a
// missing image, a variable already used by that row's seed, or a repeated
// variable in earlier rows can never become a legal additional column.
// This SAT-specific predicate belongs to the matrix consumer, not the graph
// backend. It observes images without changing their discovery/column order.
class RowColumnFeasibility {
public:
  explicit RowColumnFeasibility(const std::vector<std::vector<Lit>> &seed)
      : seed_(seed) {}

  static bool observe(const symmetry_backend::RefinementRowPrefix &prefix,
                      symmetry_backend::RowWorkCallback charge,
                      void *work_context, void *context) {
    return static_cast<RowColumnFeasibility *>(context)->advance(
        prefix, charge, work_context);
  }

  const std::vector<uint32_t> &remaining_sources() const { return sources_; }

private:
  bool advance(const symmetry_backend::RefinementRowPrefix &prefix,
               symmetry_backend::RowWorkCallback charge, void *work_context) {
    assert(prefix.completed_rows && prefix.completed_rows <= seed_.size());
    assert(prefix.anchor_count == seed_.size());
    const uint32_t row = prefix.completed_rows - 1;
    if (!row) {
      sources_.clear();
      for (uint32_t source = 0; source < prefix.domain_size; ++source) {
        if (!charge(work_context)) return false;
        if (prefix.images[source] < prefix.domain_size)
          sources_.push_back(source);
      }
    }
    std::size_t kept = 0;
    for (const uint32_t source : sources_) {
      if (!charge(work_context)) return false;
      const uint32_t target = prefix.images[
          static_cast<uint64_t>(row) * prefix.domain_size + source];
      bool possible = target < prefix.domain_size;
      for (const Lit literal : seed_[row]) {
        if (!possible) break;
        if (!charge(work_context)) return false;
        possible = var_of(target) != var_of(literal) &&
                   (row || var_of(source) != var_of(literal));
      }
      for (uint32_t previous = 0; previous < row && possible; ++previous) {
        if (!charge(work_context)) return false;
        const uint32_t prior = prefix.images[
            static_cast<uint64_t>(previous) * prefix.domain_size + source];
        possible = var_of(prior) != var_of(target);
      }
      if (possible) sources_[kept++] = source;
    }
    sources_.resize(kept);
    return !sources_.empty();
  }

  const std::vector<std::vector<Lit>> &seed_;
  std::vector<uint32_t> sources_;
};

} // namespace tactix
