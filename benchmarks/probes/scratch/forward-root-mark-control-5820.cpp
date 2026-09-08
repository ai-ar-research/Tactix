#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/strengthen/strengthen.hpp"
#include <cassert>
#include <cstdio>
using namespace tactix;
int main() {
  Kernel k;
  kernel_reserve_vars(k,3);
  const Lit clause[] = {internalize(1),internalize(2),internalize(3)};
  kernel_add_large_clause(k,clause,3,false,0);
  kernel_add_binary_clause(k,internalize(2),internalize(3));
  k.strengthen_touch_mark = k.touch_epoch;
  const uint64_t watermark = k.strengthen_touch_mark;
  kernel_touch_var(k,var_of(internalize(3)));
  kernel_assign_unit(k,internalize(-1));
  assert(!kernel_touched_since(k,0,watermark));
  k.touch_filter = true;
  assert(strengthen_apply(k) == ApplyResult::Ok);
  assert(kernel_touched_since(k,0,watermark));
  assert(k.stats.strengthen_checked == 1 && k.stats.strengthen_subsumed == 1);
  assert(k.value(internalize(-1)) > 0 && !k.inconsistent);
  kernel_check_invariants(k);
  puts("TACTIX root notification adds a second admission mark;subsumed=1");
}
