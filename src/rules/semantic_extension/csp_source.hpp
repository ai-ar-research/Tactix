#pragma once

#include <cstdint>
#include <vector>

namespace tactix::csp_source {

// Identity within an authenticated source image. A zero native proof id names
// the clause by literals instead.
struct Premise {
  uint64_t occurrence = 0;
  std::vector<int32_t> literals;
  uint64_t native_proof_id = 0;
  bool operator==(const Premise &) const = default;
};

// An exactly-one domain, ordered by (variable, literal). at_most_one is a
// symmetric zero-diagonal matrix naming (~values[i] v ~values[j]).
struct Domain {
  std::vector<int32_t> values;
  uint64_t at_least_one = 0;
  std::vector<uint64_t> at_most_one;
  bool operator==(const Domain &) const = default;
};

} // namespace tactix::csp_source
