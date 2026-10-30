#pragma once

#include "kernel/types.hpp"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace tactix::csp_exact_theory {

inline uint32_t variable(int32_t literal) {
  return external_variable_of(literal);
}

inline bool valid_values(const std::vector<int32_t> &values) {
  if (values.empty()) return false;
  for (int32_t literal : values)
    if (!literal || literal == INT32_MIN) return false;
  for (size_t i = 1; i < values.size(); ++i) {
    const auto previous = std::pair(variable(values[i - 1]), values[i - 1]);
    const auto current = std::pair(variable(values[i]), values[i]);
    if (!(previous < current) || previous.first == current.first) return false;
  }
  return true;
}

inline bool safe_product(size_t first, size_t second, size_t &result) {
  if (first && second > std::numeric_limits<size_t>::max() / first)
    return false;
  result = first * second;
  return true;
}

inline bool add_count(uint64_t amount, uint64_t &total) {
  if (amount > UINT64_MAX - total) return false;
  total += amount;
  return true;
}

enum class BuildStatus : uint8_t {
  Valid,
  IncompleteTheory,
  BoundExceeded,
  EffortExhausted,
};

template <class Premise> struct Snapshot {
  std::map<uint64_t, const Premise *> by_occurrence;
  std::set<uint64_t> used;
  uint64_t literals = 0;
  uint32_t maximum_variable = 0;
  bool effort_exhausted = false;

  template <class Spend>
  BuildStatus build(const std::vector<Premise> &source,
                    uint64_t maximum_literals, Spend &spend,
                    uint64_t &consumed_clauses,
                    uint64_t &consumed_literals) {
    for (const Premise &premise : source) {
      if (!spend(uint64_t{1} + premise.literals.size()))
        return BuildStatus::EffortExhausted;
      if (!premise.occurrence || premise.literals.empty() ||
          !by_occurrence.emplace(premise.occurrence, &premise).second)
        return BuildStatus::IncompleteTheory;
      ++consumed_clauses;
      if (!add_count(premise.literals.size(), literals) ||
          literals > maximum_literals)
        return BuildStatus::BoundExceeded;
      consumed_literals = literals;
      std::set<uint32_t> clause_variables;
      for (int32_t literal : premise.literals) {
        if (!literal || literal == INT32_MIN ||
            !clause_variables.insert(variable(literal)).second)
          return BuildStatus::IncompleteTheory;
        maximum_variable = std::max(maximum_variable, variable(literal));
      }
    }
    return BuildStatus::Valid;
  }

  template <class Spend>
  bool exact(uint64_t occurrence, std::vector<int32_t> clause, Spend &spend) {
    if (!occurrence) return false;
    if (!spend(uint64_t{1} + clause.size())) {
      effort_exhausted = true;
      return false;
    }
    const auto found = by_occurrence.find(occurrence);
    if (found == by_occurrence.end()) return false;
    std::vector<int32_t> premise = found->second->literals;
    std::sort(premise.begin(), premise.end());
    std::sort(clause.begin(), clause.end());
    if (premise != clause) return false;
    used.insert(occurrence);
    return true;
  }

  template <class Domain, class Spend>
  bool validate_domain(const Domain &domain, Spend &spend) {
    const size_t values = domain.values.size();
    size_t square = 0;
    if (!safe_product(values, values, square) ||
        domain.at_most_one.size() != square ||
        !exact(domain.at_least_one, domain.values, spend))
      return false;
    for (size_t first = 0; first < values; ++first) {
      if (domain.at_most_one[first * values + first]) return false;
      for (size_t second = first + 1; second < values; ++second) {
        const uint64_t forward = domain.at_most_one[first * values + second];
        const uint64_t reverse = domain.at_most_one[second * values + first];
        if (!forward || forward != reverse ||
            !exact(forward,
                   {-domain.values[first], -domain.values[second]}, spend))
          return false;
      }
    }
    return true;
  }

  bool complete() const { return used.size() == by_occurrence.size(); }
};

} // namespace tactix::csp_exact_theory
