#pragma once

#include "kernel/types.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace tactix {

// Index conditional equivalences by condition while preserving their original
// positions.  A lookup therefore yields exactly the successful j > i sequence
// of the former nested scan, without visiting unrelated conditions.
class ConditionalIndex {
  using Entry = std::pair<Lit, size_t>;
  std::vector<Entry> ordered_;

public:
  using const_iterator = std::vector<Entry>::const_iterator;

  void build(const std::vector<std::pair<Lit, Lit>> &conditions) {
    ordered_.clear();
    ordered_.reserve(conditions.size());
    for (size_t i = 0; i < conditions.size(); ++i)
      ordered_.emplace_back(conditions[i].first, i);
    std::sort(ordered_.begin(), ordered_.end());
  }

  const_iterator after(Lit condition, size_t position) const {
    return std::upper_bound(ordered_.begin(), ordered_.end(),
                            Entry{condition, position});
  }

  const_iterator end() const { return ordered_.end(); }
};

} // namespace tactix
