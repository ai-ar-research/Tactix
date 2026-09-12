#pragma once
#include "kernel/kernel.hpp"

// Test-only materialization: assertions name the logical sequence rather than
// requiring a second owning payload in Eliminate. Production uses visitors.
template <class Index>
inline std::vector<tactix::Ref> large_occurrences(const Index &occ,
                                                tactix::Lit lit) {
  std::vector<tactix::Ref> refs;
  if constexpr (requires { occ.copy_large(lit, refs); }) {
    occ.copy_large(lit, refs);
    assert(refs.size() == occ.large_size(lit));
    if (occ.track_order)
      assert(occ.large.empty() && occ.large.arena_capacity_bytes() == 0 &&
             "mixed mode must not retain the removed payload");
  } else {
    // The same fixture links against actual immutable predecessor objects.
    const auto span = occ.large[lit];
    refs.assign(span.begin(), span.end());
  }
  if (occ.track_order) {
    std::vector<tactix::Ref> projected;
    for (const auto entry : occ.ordered[lit])
      if (!entry.binary) projected.push_back(entry.payload);
    assert(refs == projected && "exact order, not merely multiset equality");
  }
  return refs;
}
