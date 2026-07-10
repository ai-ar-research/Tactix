// Analyze/Backjump unit test: forced 1-UIP scenario with a known learned
// clause and backjump level.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "rules/analyze/analyze.hpp"
#include "rules/backjump/backjump.hpp"
#include "rules/propagate/propagate.hpp"

#include <cassert>
#include <cstdio>

using namespace tactix;

static void test_uip() {
  // c1 = (-1 -2 3), c2 = (-1 -2 -3); decide 1, decide 2:
  // c1 forces 3, c2 conflicts; 1-UIP learns (-2 -1) with UIP 2, jump to 1
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit c1[3] = {internalize(-1), internalize(-2), internalize(3)};
  const Lit c2[3] = {internalize(-1), internalize(-2), internalize(-3)};
  kernel_add_large_clause(k, c1, 3, false, 0);
  kernel_add_large_clause(k, c2, 3, false, 0);

  kernel_assign_decision(k, internalize(1));
  assert(propagate_apply(k) == ApplyResult::Ok);
  kernel_assign_decision(k, internalize(2));
  assert(propagate_apply(k) == ApplyResult::Conflict);

  assert(analyze_guard(k));
  assert(analyze_apply(k) == ApplyResult::Ok);
  assert(k.pending.valid);
  assert(k.pending.lits.size() == 2);
  assert(k.pending.lits[0] == internalize(-2)); // negated UIP
  assert(k.pending.lits[1] == internalize(-1));
  assert(k.pending.jump_level == 1);

  assert(backjump_guard(k));
  assert(backjump_apply(k) == ApplyResult::Ok);
  assert(k.level == 1);
  assert(k.values[internalize(-2)] > 0); // asserted
  assert(!k.pending.valid);

  // and the search must now conclude SAT (model: 1, -2, any 3)
  assert(default_solve(k) == 10);
}

static void test_learned_unit() {
  // (-1 2), (-1 -2): deciding 1 conflicts and learns unit -1
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(-2));
  kernel_assign_decision(k, internalize(1));
  assert(propagate_apply(k) == ApplyResult::Conflict);
  assert(analyze_apply(k) == ApplyResult::Ok);
  assert(k.pending.lits.size() == 1);
  assert(k.pending.lits[0] == internalize(-1));
  assert(k.pending.jump_level == 0);
  assert(backjump_apply(k) == ApplyResult::Ok);
  assert(k.level == 0);
  assert(k.values[internalize(-1)] > 0);
  assert(k.stats.learned_units == 1);
}

int main() {
  test_uip();
  test_learned_unit();
  puts("test_analyze OK");
  return 0;
}
