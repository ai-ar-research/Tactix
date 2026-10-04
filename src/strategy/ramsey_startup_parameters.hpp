#pragma once
#include <cstdint>
namespace tactix {
struct Kernel;
namespace strategy {
// Caller-owned bounds. Zero disables work; this is not a production profile.
struct RamseyStartupParameters {
  uint64_t recognition_clauses = 0, recognition_variables = 0;
  uint64_t recognition_pair_cells = 0, recognition_work = 0;
  uint64_t certificate_work = 0, maximum_nodes = 0, maximum_additions = 0;
  uint32_t maximum_external_extent = 0;
  bool valid() const {
    return maximum_external_extent <= INT32_MAX &&
           recognition_work <= UINT64_MAX - certificate_work;
  }
  bool enabled() const {
    return valid() && recognition_clauses && recognition_variables &&
           recognition_pair_cells && recognition_work && certificate_work &&
           maximum_nodes && maximum_additions && maximum_external_extent;
  }
  static RamseyStartupParameters from_kernel(const Kernel &);
};
void install_rule_parameters(Kernel &, const RamseyStartupParameters &);
} // namespace strategy
} // namespace tactix
