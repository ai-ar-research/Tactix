#pragma once

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace tactix::symmetry_backend::detail {

template <class Pairs>
bool implicit_dense_less(const Pairs &left, const Pairs &right) {
  size_t a = 0, b = 0;
  while (a < left.size() || b < right.size()) {
    if (b == right.size() ||
        (a < left.size() && left[a].first < right[b].first))
      return left[a].second < left[a].first;
    if (a == left.size() || right[b].first < left[a].first)
      return right[b].first < right[b].second;
    if (left[a].second != right[b].second)
      return left[a].second < right[b].second;
    ++a;
    ++b;
  }
  return false;
}

// A graph permutation restricted to a prefix. Targets may lie outside that
// prefix; the semantic consumer decides whether it is an invariant domain.
struct SparsePermutation {
  std::vector<std::pair<uint32_t, uint32_t>> moved;

  bool operator==(const SparsePermutation &) const = default;
  bool operator<(const SparsePermutation &other) const {
    // Compare implicit dense images, not the lexicographic order of pairs.
    return implicit_dense_less(moved, other.moved);
  }
};

// DejaVu supplies a superset of the support, possibly unordered, duplicated or
// containing fixed points. Its adapter convention
// also permits -1/null support for a dense callback. Missing support metadata
// therefore requests a dense scan, never an implicit identity assumption.
inline bool capture_permutation_prefix(int domain, const int *permutation,
                                      int support_size, const int *support,
                                      uint32_t prefix,
                                      SparsePermutation &result) {
  result.moved.clear();
  if (domain < 0 || prefix > static_cast<uint32_t>(domain) || !permutation ||
      support_size < -1)
    return false;
  const auto append = [&](uint32_t source) {
    if (source >= prefix) return true;
    const int target = permutation[source];
    if (target < 0 || target >= domain) return false;
    if (source != static_cast<uint32_t>(target))
      result.moved.emplace_back(source, static_cast<uint32_t>(target));
    return true;
  };
  if (support_size < 0 || (!support && support_size)) {
    for (uint32_t source = 0; source < prefix; ++source)
      if (!append(source)) return false;
  } else {
    for (int i = 0; i < support_size; ++i) {
      if (support[i] < 0 || support[i] >= domain ||
          !append(static_cast<uint32_t>(support[i])))
        return false;
    }
    std::sort(result.moved.begin(), result.moved.end());
    result.moved.erase(std::unique(result.moved.begin(), result.moved.end()),
                       result.moved.end());
  }
  return true;
}

} // namespace tactix::symmetry_backend::detail
