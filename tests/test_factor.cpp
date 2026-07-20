// E2 Factor (BVA). S0: extension-variable allocation + model projection.
// Extension variables (added mid-solve by Factor) are assigned by search but
// projected out of the reported model — the v-line ranges over original vars only.
#undef NDEBUG
#include "kernel/kernel.hpp"

#include <cassert>
#include <cstdio>

using namespace tactix;

// kernel_new_extension_vars grows all per-var state additively, records the
// extension boundary, and keeps the model-output count at the original variables.
static void test_extension_var_boundary() {
  Kernel k;
  kernel_reserve_vars(k, 5); // 5 original vars: 0..4
  assert(k.nvars == 5);
  assert(k.first_extension_var == UINT32_MAX);
  assert(kernel_num_output_vars(k) == 5); // all original

  const Var first = kernel_new_extension_vars(k, 2); // add 2 extension vars
  assert(first == 5);
  assert(k.nvars == 7);
  assert(k.first_extension_var == 5);          // boundary set to first extension
  assert(kernel_num_output_vars(k) == 5);      // extension vars projected out
  assert(k.values.size() == 2 * 7);            // per-var state grew (by lit)
  assert(k.saved.size() == 7 && k.scores.score.size() == 7); // and others

  const Var f2 = kernel_new_extension_vars(k, 1); // add one more
  assert(f2 == 7);
  assert(k.nvars == 8);
  assert(k.first_extension_var == 5);          // boundary unchanged
  assert(kernel_num_output_vars(k) == 5);
}

int main() {
  test_extension_var_boundary();
  std::printf("test_factor: all passed\n");
  return 0;
}
