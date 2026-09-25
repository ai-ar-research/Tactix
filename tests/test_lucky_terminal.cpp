#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"

#include <cassert>

using namespace tactix;

int main() {
  // PHP(3,2): Lucky's failed-literal probes refute this at its first call.
  // In particular, a terminal refutation must not fall through to materialize
  // deferred state or rely on a later Propagate call to notice inconsistency.
  Kernel k;
  kernel_defer_cold_state(k);
  kernel_reserve_vars(k, 6);
  const auto var = [](int pigeon, int hole) {
    return pigeon * 2 + hole + 1;
  };
  for (int p = 0; p < 3; ++p)
    kernel_add_binary_clause(k, internalize(var(p, 0)),
                             internalize(var(p, 1)));
  for (int h = 0; h < 2; ++h)
    for (int a = 0; a < 3; ++a)
      for (int b = a + 1; b < 3; ++b)
        kernel_add_binary_clause(k, internalize(-var(a, h)),
                                 internalize(-var(b, h)));
  DefaultStrategyConfig config;
  config.inprocess.structured_root_before_lucky = false;
  config.onehot_model.enabled = false;
  assert(DefaultPolicy(config).solve(k) == 20);
  assert(k.inconsistent && k.level == 0);
  assert(k.stats.lucky_early_calls == 1);
  assert(k.stats.lucky_late_calls == 0);
  assert(k.cold_state_deferred && !k.h);
}
