#pragma once
#include <array>
#include <cstdint>
#include <vector>

namespace tactix::equality_transitivity {
using Clause = std::vector<int32_t>;
using Domains = std::array<Clause, 3>;
struct Plan {
  // AB, AD, BD; each group has one bridge per anchor value then a triangle.
  std::array<std::vector<Clause>, 3> bridges;
  std::array<Clause, 3> triangles;
  uint64_t additions = 0, deletions = 0, literal_occurrences = 0;
  uint64_t publication_work = 0, planning_work = 0;
};
bool domains_valid(const Domains &);
// Pure generic schema. All equality meanings/complete exactly-one sources
// must be authenticated by the live adapter before publication.
bool plan(const Domains &, const std::array<uint32_t, 3> &equalities,
          uint64_t maximum_work, Plan &);
} // namespace tactix::equality_transitivity
