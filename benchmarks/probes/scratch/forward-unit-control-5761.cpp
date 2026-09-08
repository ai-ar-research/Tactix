// Fixed actual-code witness: unit inference must not hide later useful work.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/strengthen/strengthen.hpp"
#include <cassert>
#include <cstdio>
using namespace tactix;
int main() {
  for (unsigned tail_size : {3u,4u}) {
    Kernel k;
    kernel_reserve_vars(k,7);
    const Lit first[] = {internalize(1),internalize(2),internalize(3)};
    const Lit tail[] = {internalize(4),internalize(5),internalize(6),internalize(7)};
    kernel_add_large_clause(k,first,3,false,0);
    kernel_add_large_clause(k,tail,tail_size,false,0);
    kernel_add_binary_clause(k,internalize(-2),internalize(3));
    kernel_add_binary_clause(k,internalize(4),internalize(5));
    kernel_assign_unit(k,internalize(-1));
    assert(propagate_apply(k) == ApplyResult::Ok);
    assert(k.value(internalize(3)) == 0);
    assert(strengthen_apply(k) == ApplyResult::Ok);
    assert(k.value(internalize(3)) > 0);
    // This control records the current early exit, not desired parity.
    assert(k.stats.strengthen_checked == 1);
    assert(k.stats.strengthen_subsumed == 0);
    assert(!k.strengthen_complete);
    kernel_check_invariants(k);
    std::printf("CONTROL tail=%u forced3=%d checked=%llu subsumed=%llu\n",tail_size,
        k.value(internalize(3)),(unsigned long long)k.stats.strengthen_checked,
        (unsigned long long)k.stats.strengthen_subsumed);
  }
}
