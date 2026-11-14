#pragma once

#include <cstdint>
#include <vector>

// Shared record spelling; each recognizer retains its original nominal type
// and equality signature. Keep flat aggregate initialization and field order.
#define TACTIX_CSP_PREMISE_RECORD(Equality)                                    \
  struct Premise {                                                           \
    uint64_t occurrence = 0;                                                 \
    std::vector<int32_t> literals;                                           \
    uint64_t native_proof_id = 0;                                            \
    Equality;                                                               \
  };
#define TACTIX_CSP_DOMAIN_RECORD(Equality)                                     \
  struct Domain {                                                            \
    std::vector<int32_t> values;                                             \
    uint64_t at_least_one = 0;                                                \
    std::vector<uint64_t> at_most_one;                                        \
    Equality;                                                               \
  };

namespace tactix::csp_source {

// Identity in an authenticated source image; zero proof id names the literals.
TACTIX_CSP_PREMISE_RECORD(bool operator==(const Premise &) const = default)
// Exactly one ordered value; a symmetric zero-diagonal matrix names exclusions.
TACTIX_CSP_DOMAIN_RECORD(bool operator==(const Domain &) const = default)

} // namespace tactix::csp_source
