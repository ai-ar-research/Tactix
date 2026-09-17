#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

namespace tactix {

// Diagnostic fingerprint, never an equality test or proof check. Literals use
// external variable indices: 2*(abs(DIMACS)-1)+(DIMACS<0). Sorting ignores
// literal order; sums preserve multiplicity while ignoring clause order.
struct FormulaFingerprint {
  uint64_t clauses = 0, literals = 0, sum = 0, squared_sum = 0;
  std::vector<uint32_t> normalized;

  void add(const std::vector<uint32_t> &clause) {
    normalized = clause;
    std::sort(normalized.begin(), normalized.end());
    uint64_t hash = 0xcbf29ce484222325ull;
    for (const uint32_t literal : normalized) {
      hash ^= static_cast<uint64_t>(literal) + 1;
      hash *= 0x100000001b3ull;
    }
    hash ^= normalized.size();
    hash *= 0x100000001b3ull;
    ++clauses;
    literals += normalized.size();
    sum += hash;
    squared_sum += hash * hash;
  }
};

} // namespace tactix
