// Private ConflictAnalysis helper, shared with its fixed ordering fixture.
#pragma once

#include "kernel/kernel.hpp"
#include <algorithm>

namespace tactix {

// Kissat's sort_deduced_clause stably groups descending decision levels.
// Complemented-level LSD sorting preserves that order. OTFS scratch is dead
// here and its next OTFS consumer clears it before assembling a resolvent.
static void radix_sort_learned_by_level(Kernel &k, std::vector<Lit> &learned) {
  assert(!learned.empty()); // first entry is the UIP, never part of the sort
  const size_t n = learned.size() - 1;
  if (n < 2)
    return;
  std::vector<Lit> &scratch = k.otfs_scratch;
  scratch.resize(n);
  Lit *const original = learned.data() + 1;
  Lit *src = original, *dst = scratch.data();
  for (uint32_t shift = 0; shift < 32; shift += 8) {
    uint32_t count[257] = {0};
    const uint32_t first = (~k.var_level(var_of(src[0])) >> shift) & 0xff;
    bool constant = true;
    for (size_t i = 0; i < n; i++) {
      const uint32_t rank = ~k.var_level(var_of(src[i]));
      const uint32_t byte = (rank >> shift) & 0xff;
      constant &= byte == first;
      count[byte + 1]++;
    }
    // A stable pass on one bucket is the identity. As with the neighboring
    // bump-stamp radix sort, omit prefix sums and scattering for that byte.
    if (constant)
      continue;
    for (uint32_t i = 1; i < 257; i++)
      count[i] += count[i - 1];
    for (size_t i = 0; i < n; i++) {
      const uint32_t rank = ~k.var_level(var_of(src[i]));
      dst[count[(rank >> shift) & 0xff]++] = src[i];
    }
    std::swap(src, dst);
  }
  if (src != original)
    std::copy(src, src + n, original);
}

} // namespace tactix
