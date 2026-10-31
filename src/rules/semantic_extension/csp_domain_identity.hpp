#pragma once

#include "kernel/types.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace tactix::csp_domain_identity {

enum class PairStatus : uint8_t { Valid, InvalidDomain, RepeatedVariable };

template <class DomainAt>
PairStatus identity_status(size_t count, DomainAt domain_at,
                           size_t minimum_width, size_t maximum_width,
                           bool equal_width, bool ordered_domains) {
  if (!count) return PairStatus::InvalidDomain;
  const size_t width = domain_at(0).size();
  std::vector<uint32_t> variables;
  for (size_t domain = 0; domain < count; ++domain) {
    const auto &literals = domain_at(domain);
    if (literals.size() < minimum_width ||
        literals.size() > maximum_width ||
        (equal_width && literals.size() != width) ||
        (ordered_domains && domain && !(domain_at(domain - 1) < literals)))
      return PairStatus::InvalidDomain;
    for (size_t index = 0; index < literals.size(); ++index) {
      const int32_t literal = literals[index];
      if (!literal || literal == std::numeric_limits<int32_t>::min() ||
          (index && !external_literal_less(literals[index - 1], literal)))
        return PairStatus::InvalidDomain;
      variables.push_back(external_variable_of(literal));
    }
  }
  std::sort(variables.begin(), variables.end());
  return std::adjacent_find(variables.begin(), variables.end()) ==
                 variables.end()
             ? PairStatus::Valid
             : PairStatus::RepeatedVariable;
}

inline PairStatus pair_status(const std::vector<int32_t> &left,
                              const std::vector<int32_t> &right) {
  return identity_status(
      2, [&](size_t index) -> const auto & { return index ? right : left; },
      2, std::numeric_limits<size_t>::max(), false, false);
}

inline bool pair_valid(const std::vector<int32_t> &left,
                       const std::vector<int32_t> &right) {
  return pair_status(left, right) == PairStatus::Valid;
}

template <class Domains>
bool ordered_three_valid(
    const Domains &domains, size_t minimum_width,
    size_t maximum_width = std::numeric_limits<size_t>::max()) {
  if (domains.size() != 3) return false;
  return identity_status(3, [&](size_t index) -> const auto & {
    return domains[index];
  }, minimum_width, maximum_width, true, true) == PairStatus::Valid;
}

} // namespace tactix::csp_domain_identity
