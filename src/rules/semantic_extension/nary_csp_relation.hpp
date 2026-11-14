#pragma once

#include "rules/semantic_extension/csp_source.hpp"

#include <cstdint>
#include <vector>

namespace tactix::nary_csp_relation {

using Premise = csp_source::Premise;

using Domain = csp_source::Domain;

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
