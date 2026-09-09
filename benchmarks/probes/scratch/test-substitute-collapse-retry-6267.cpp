// Fixed actual-implementation differential: an unchanged endpoint of a
// large-to-binary collapse still participates in native removal advice.
#ifdef NATIVE
#define NDEBUG 1
extern "C" {
#include "internal.h"
#include "substitute.h"
}
#undef NDEBUG
#else
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/substitute/substitute.hpp"
#endif
#include <cassert>
#include <cstdio>
#ifndef NATIVE
using namespace tactix;
#endif

int main() {
  for (unsigned copies : {1u, 2u, 5u}) {
#ifdef NATIVE
    kissat *s = kissat_init();
    kissat_set_option(s, "quiet", 1);
    kissat_set_option(s, "substituterounds", 1);
    for (int x : {-1, 2, 0, 1, -2, 0}) kissat_add(s, x);
    for (unsigned i = 0; i < copies; ++i)
      for (int x : {1, 2, 3, 0}) kissat_add(s, x);
    for (unsigned i = 0; i < 3; ++i) s->flags[i].eliminate = false;
    s->probing = true; kissat_substitute(s, false); s->probing = false;
    assert(!s->inconsistent && s->flags[2].eliminate);
    std::printf("native copies=%u retained_endpoint_retry=%u\n", copies, s->flags[2].eliminate);
    kissat_release(s);
#else
    Kernel k; kernel_reserve_vars(k, 3);
    kernel_add_binary_clause(k, internalize(-1), internalize(2));
    kernel_add_binary_clause(k, internalize(1), internalize(-2));
    const Lit source[] = {internalize(1), internalize(2), internalize(3)};
    for (unsigned i = 0; i < copies; ++i) kernel_add_large_clause(k, source, 3, false, 0);
    std::fill(k.bve_retry.begin(), k.bve_retry.end(), 0);
    assert(substitute_apply(k) == ApplyResult::Ok);
    assert(kernel_binary_live(k, internalize(1), internalize(3)));
    assert(!k.bve_retry[2]); // Expected existing discrepancy, not an abort.
    std::printf("tactix copies=%u retained_endpoint_retry=%u\n", copies, k.bve_retry[2]);
    kernel_check_invariants(k);
#endif
  }
}
