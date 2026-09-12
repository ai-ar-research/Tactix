#undef NDEBUG
#include "kernel/kernel.hpp"
#include "strategy/vivify_parameters.hpp"

#include <cassert>
#include <cstdio>

using namespace tactix;
using namespace tactix::strategy;

int main() {
  Kernel kernel;
  const VivifyParameters defaults = VivifyParameters::from_kernel(kernel);
  assert(defaults.scope == 0 && defaults.reuse && !defaults.flush);
  assert(defaults.irredundant_sort_ratio == 10 && defaults.valid());

  // Preserve the exact integer-division boundary of the shipped 10:1 test.
  assert(!vivify_skip_global_order(109, 10, defaults));
  assert(vivify_skip_global_order(110, 10, defaults));
  VivifyParameters ratio = defaults;
  ratio.irredundant_sort_ratio = 2;
  assert(!vivify_skip_global_order(5, 2, ratio));
  assert(vivify_skip_global_order(6, 2, ratio));

  const VivifyParameters posed{5, false, true, 3};
  {
    ScopedVivifyParameters scope(kernel, posed);
    assert(VivifyParameters::from_kernel(kernel).scope == 5);
    assert(!kernel.vivify_reuse && kernel.vivify_flush);
    assert(kernel.vivify_irredundant_sort_ratio == 3);
  }
  const VivifyParameters restored = VivifyParameters::from_kernel(kernel);
  assert(restored.scope == defaults.scope && restored.reuse == defaults.reuse);
  assert(restored.flush == defaults.flush);
  assert(restored.irredundant_sort_ratio ==
         defaults.irredundant_sort_ratio);

  VivifyParameters invalid = defaults;
  invalid.irredundant_sort_ratio = 0;
  assert(!invalid.valid());
  invalid = defaults;
  invalid.scope = 6;
  assert(!invalid.valid());
  std::puts("Vivify typed parameters, threshold boundary and scope restore PASS");
}
