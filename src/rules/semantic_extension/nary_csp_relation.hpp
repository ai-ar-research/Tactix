#pragma once

#include "rules/semantic_extension/csp_source.hpp"

#include <cstdint>
#include <vector>

namespace tactix::nary_csp_relation {

// Identity within one authenticated source image. native_proof_id is zero
// when the Kernel does not retain physical proof identifiers.
TACTIX_CSP_PREMISE_RECORD(bool operator==(const Premise &) const = default)

// An exactly-one domain; at_most_one is a symmetric zero-diagonal n-by-n
// matrix naming the binaries (~values[i] v ~values[j]).
TACTIX_CSP_DOMAIN_RECORD(bool operator==(const Domain &) const = default)

// One exact clause containing the complement of one value from every domain.
// Relations may be sparse: absence is never interpreted as an allowed tuple.
struct ForbiddenTuple {
  std::vector<uint32_t> value_indices;
  uint64_t occurrence = 0;

  bool operator==(const ForbiddenTuple &) const = default;
};

struct Relation {
  std::vector<Domain> domains;
  std::vector<ForbiddenTuple> forbidden;

  bool operator==(const Relation &) const = default;
};

struct DomainValue {
  uint32_t domain = 0;
  uint32_t value = 0;

  bool operator==(const DomainValue &) const = default;
};

} // namespace tactix::nary_csp_relation
