#pragma once

#include "kernel/types.hpp"
#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace tactix {

// Exact signed literal permutation, represented by sorted nonidentity pairs.
// Missing sources are fixed. validate() precedes every untrusted use.
struct SparseLiteralPermutation {
  uint32_t domain = 0;
  std::vector<std::pair<Lit, Lit>> moved;

  size_t size() const { return domain; }
  Lit operator[](Lit source) const {
    const auto at = std::lower_bound(moved.begin(), moved.end(), source,
        [](const auto &entry, Lit literal) { return entry.first < literal; });
    return at != moved.end() && at->first == source ? at->second : source;
  }

  template <class Visitor>
  void positive_moves(Visitor &&visit) const {
    for (const auto &[source, target] : moved)
      if (!negated(source)) visit(source, target);
  }

  template <class Retired, class Charge>
  bool validate(Retired &&retired, Charge &&charge) const {
    if ((domain & 1u) || !charge(moved.size())) return false;
    for (size_t i = 0; i < moved.size(); ++i) {
      const auto [source, target] = moved[i];
      if (source >= domain || target >= domain || source == target ||
          (i && moved[i - 1].first >= source)) return false;
    }
    std::vector<uint8_t> targets(moved.size(), 0);
    for (const auto &[source, target] : moved) {
      if (!charge(1) || retired(var_of(source))) return false;
      const auto at = std::lower_bound(moved.begin(), moved.end(), target,
          [](const auto &entry, Lit literal) { return entry.first < literal; });
      // A target outside the support would collide with its fixed identity.
      if (at == moved.end() || at->first != target || targets[at - moved.begin()])
        return false;
      targets[at - moved.begin()] = 1;
      if ((*this)[negate(source)] != negate(target)) return false;
    }
    return true;
  }
};

} // namespace tactix
