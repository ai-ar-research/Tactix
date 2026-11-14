#pragma once

#include "rules/semantic_extension/csp_source.hpp"
#include <cstddef>
#include <utility>

// Authenticated exactly-one domains, with occurrence and native proof IDs.
template <class Premise, class Domain> struct CspSourceFixture {
  std::vector<Premise> source;
  uint64_t next = 1;

  uint64_t add(std::vector<int32_t> clause) {
    const uint64_t occurrence = next++;
    source.push_back({occurrence, std::move(clause), 1000 + occurrence});
    return occurrence;
  }

  Domain domain(std::vector<int32_t> values) {
    Domain result;
    result.values = std::move(values);
    result.at_least_one = add(result.values);
    const std::size_t n = result.values.size();
    result.at_most_one.assign(n * n, 0);
    for (std::size_t i = 0; i < n; ++i)
      for (std::size_t j = i + 1; j < n; ++j) {
        const uint64_t occurrence = add({-result.values[i], -result.values[j]});
        result.at_most_one[i * n + j] = occurrence;
        result.at_most_one[j * n + i] = occurrence;
      }
    return result;
  }
};
