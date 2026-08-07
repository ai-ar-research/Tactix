#include "policies/stress_registry.hpp"

#include "policies/adversarial_policy.hpp"
#include "policies/bce_stress_policy.hpp"
#include "policies/bve_stress_policy.hpp"
#include "policies/cce_stress_policy.hpp"
#include "policies/congruence_stress_policy.hpp"
#include "policies/factor_stress_policy.hpp"
#include "policies/probing_stress_policy.hpp"
#include "policies/reuse_stress_policy.hpp"
#include "policies/substitute_stress_policy.hpp"
#include "policies/sweep_stress_policy.hpp"
#include "policies/walk_stress_policy.hpp"

#include <cstdlib>
#include <cstring>

namespace tactix {
namespace {

struct StressRow {
  const char *flag; // without the trailing `=SEED`
  StressSolveFn solve;
};

// One row per stress policy; see each policy's header for the pattern it
// maximizes. A new policy is one row and nothing else — adding it to `main`
// instead costs four edits that must agree (a bool, a parse branch, two
// validation lists, a dispatch arm), and a policy missing from the
// `--no-heuristics` list runs with `k.h == nullptr`.
const StressRow ROWS[] = {
    {"--adversarial-policy", adversarial_solve},
    {"--bce-stress-policy", bce_stress_solve},
    {"--bve-stress-policy", bve_stress_solve},
    {"--cce-stress-policy", cce_stress_solve},
    {"--congruence-stress-policy", congruence_stress_solve},
    {"--factor-stress-policy", factor_stress_solve},
    {"--probing-stress-policy", probing_stress_solve},
    {"--reuse-stress-policy", reuse_stress_solve},
    {"--substitute-stress-policy", substitute_stress_solve},
    {"--sweep-stress-policy", sweep_stress_solve},
    {"--walk-stress-policy", walk_stress_solve},
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
    // A longer flag sharing this prefix is not a match: accepting it would
    // silently swallow an unknown option (`--random-policy-typo`).
  }
  return nullptr;
}

const char *stress_policy_flags() {
  return "[--adversarial-policy[=SEED]] [--{bce,bve,cce,congruence,factor,"
         "probing,reuse,substitute,sweep,walk}-stress-policy[=SEED]]";
}

} // namespace tactix
