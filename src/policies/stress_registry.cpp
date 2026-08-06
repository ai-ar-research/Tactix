#include "policies/stress_registry.hpp"

#include "policies/bce_stress_policy.hpp"
#include "policies/substitute_stress_policy.hpp"

#include <cstdlib>
#include <cstring>

namespace tactix {
namespace {

struct StressRow {
  const char *flag; // without the trailing `=SEED`
  StressSolveFn solve;
};

// One row per stress policy. Every one of these is a WITNESS-EMITTING rule's
// driver or a schedule the uniform random policy under-samples; see each
// policy's header for the pattern it maximizes.
const StressRow ROWS[] = {
    {"--bce-stress-policy", bce_stress_solve},
    {"--substitute-stress-policy", substitute_stress_solve},
};

} // namespace

StressSolveFn stress_policy_flag(const char *arg, unsigned long long *seed) {
  for (const StressRow &row : ROWS) {
    const size_t n = strlen(row.flag);
    if (strncmp(arg, row.flag, n) != 0)
      continue;
    if (arg[n] == '\0') // bare flag: keep the caller's default seed
      return row.solve;
    if (arg[n] == '=') {
      if (seed)
        *seed = strtoull(arg + n + 1, nullptr, 10);
      return row.solve;
    }
    // a longer flag that merely shares this prefix is NOT a match: returning
    // the policy here would silently swallow an unknown option (and prefix
    // matching is how `--random-policy-typo` became a valid flag).
  }
  return nullptr;
}

const char *stress_policy_flags() {
  return "[--bce-stress-policy[=SEED]] [--substitute-stress-policy[=SEED]]";
}

} // namespace tactix
