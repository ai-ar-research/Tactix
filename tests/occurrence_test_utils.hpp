#pragma once
#include "kernel/kernel.hpp"
#include <algorithm>

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

// The seven ordered rewrite images shared by Sweep and replacement cursors.
// Mutate the caller's vector so its construction and capacity stay unchanged.
inline void apply_cursor_image_shape(std::vector<tactix::Lit> &image,
                                     unsigned size, unsigned shape) {
  using namespace tactix;
  if (shape == 1 && size > 3) image.erase(image.begin() + size / 2);
  if (shape == 2) image.back() = make_lit(2 * size, true);
  if (shape == 3)
    for (unsigned i = 0; i < size; ++i)
      image[i] = make_lit(size + i, !(i & 1));
  if (shape == 4) std::rotate(image.begin(), image.begin() + size / 2, image.end());
  if (shape == 5) std::reverse(image.begin(), image.end());
  if (shape == 6) {
    for (unsigned i = 1; i < size; i += 2)
      image[i] = make_lit(size + i, true);
    std::reverse(image.begin(), image.end());
  }
}
