// A configurable caller budget, not a hidden candidate cap, controls CCE.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/cce/cce.hpp"

#include <cassert>
#include <cstdio>

using namespace tactix;

static void check(uint64_t budget, bool expected) {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit clause[] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, clause, 3, false, 0);
  // Duplicated occurrences make this covered-literal inference exceed the
  // former hidden per-candidate ceiling.
  for (unsigned i = 0; i < 3000; ++i)
    kernel_add_binary_clause(k, internalize(-1), internalize(4));
  kernel_add_binary_clause(k, internalize(-4), internalize(-2));
  kernel_add_binary_clause(k, internalize(-3), internalize(4));
  const auto &rule = tactix_rules()[tactix_rule_id("CCE")];
  uint64_t choices[8];
  const uint32_t before = rule.instances(k, choices, 8);
  assert(before == 8);
  k.work_budget = budget;
  uint64_t posed[8];
  assert(rule.instances(k, posed, 8) == before);
  for (uint32_t i = 0; i < before; ++i)
    assert(choices[i] == posed[i]);
  const ApplyResult result =
      rule.apply_instance(k, lit_instance(internalize(1)));
  assert(result == (expected ? ApplyResult::Ok : ApplyResult::NoOp));
  assert(k.stats.cce_deleted == unsigned(expected));
  if (expected)
    assert(k.stats.cce_ticks > 4096);
  kernel_check_invariants(k);
}

int main() {
  check(1, false);
  check(0, true);
  check(UINT64_MAX, true);
  std::puts("CCE caller-budget reachability, unbounded and overflow controls PASS");
}
