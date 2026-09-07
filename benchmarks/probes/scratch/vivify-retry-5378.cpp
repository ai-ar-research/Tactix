// Fixed public Vivify application with the old/new actual solver objects.
#include "kernel/kernel.hpp"
#include "rules/vivify/vivify.hpp"
#include <algorithm>
#include <cstdio>
using namespace tactix;
int main() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit lits[4] = {internalize(1), internalize(2), internalize(3), internalize(4)};
  const Ref r = kernel_add_large_clause(k, lits, 4, false, 0);
  kernel_add_binary_clause(k, internalize(1), internalize(-2));
  std::fill(k.bve_retry.begin(), k.bve_retry.end(), 0);
  const auto outcome = vivify_try(k, r);
  unsigned armed = 0;
  for (const auto b : k.bve_retry) armed += !!b;
  const Clause *c = clause_at(k.arena, r);
  std::printf("tactix size=%u armed=%u\n", c->size, armed);
  return outcome == ApplyResult::Ok && !c->garbage() && c->size == 3 && armed == 4 ? 0 : 1;
}
