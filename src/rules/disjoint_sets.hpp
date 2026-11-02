#pragma once

#include <cstddef>
#include <cstdint>
#include <numeric>
#include <utility>
#include <vector>

namespace tactix {

// Union-find with path halving over dense indices. Callers depend only on the
// partition and on unite's merged flag, never on which member is the root.
class DisjointSets {
  std::vector<uint32_t> parent_;

public:
  explicit DisjointSets(size_t size) : parent_(size) {
    std::iota(parent_.begin(), parent_.end(), uint32_t{0});
  }

  uint32_t find(uint32_t item) {
    while (parent_[item] != item) {
      parent_[item] = parent_[parent_[item]];
      item = parent_[item];
    }
    return item;
  }

  // False when both items were already in one set.
  bool unite(uint32_t first, uint32_t second) {
    first = find(first);
    second = find(second);
    if (first == second)
      return false;
    if (first > second)
      std::swap(first, second);
    parent_[second] = first;
    return true;
  }
};

} // namespace tactix
