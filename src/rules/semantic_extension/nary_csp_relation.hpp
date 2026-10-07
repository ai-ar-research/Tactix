#pragma once

#include <cstdint>
#include <vector>

namespace tactix::nary_csp_relation {

// Identity within one authenticated source image. native_proof_id is zero
// when the Kernel does not retain physical proof identifiers.
struct Premise {
  uint64_t occurrence = 0;
  std::vector<int32_t> literals;
  uint64_t native_proof_id = 0;

  bool operator==(const Premise &) const = default;
};

// One directly authenticated exactly-one domain. at_most_one is an n-by-n
// row-major matrix. Its diagonal is zero; symmetric off-diagonal entries name
// the exact binary clauses (~values[i] v ~values[j]).
struct Domain {
  std::vector<int32_t> values;
  uint64_t at_least_one = 0;
  std::vector<uint64_t> at_most_one;

  bool operator==(const Domain &) const = default;
};

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
