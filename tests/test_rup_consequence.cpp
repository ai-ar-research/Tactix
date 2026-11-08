#undef NDEBUG
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix
#include "api/api_internal.hpp"
#include "rules/rup_consequence/rup_consequence.hpp"
#include "rules/propagate/propagate.hpp"
#include <cassert>
#include <climits>
using namespace tactix;

static void public_inference() {
  auto *s = tactix_init();
  // Resolving on 2 gives (1 or 3), a consequence without root propagation.
  for (int x : {1,2,0,-2,3,0}) tactix_add(s,x);
  assert(tactix_status(s) == 0);
  auto &k = api_detail::kernel_for_fixed_testing(s);
  const auto id = k.next_clause_id, epoch = k.strategy_epoch;
  int a[] = {1,2}, b[] = {-2,3}, target[] = {1,3}, false_clause[] = {-1,-3};
  tactix_rup_premise premises[] = {{a,2},{b,2}};
  assert(tactix_add_rup(s,target,2,premises,2,1) == -1);
  assert(tactix_add_rup(s,target,2,premises,1,10000) == -1);
  assert(tactix_add_rup(s,false_clause,2,premises,2,10000) == -1);
  int forged[] = {1,-3};
  tactix_rup_premise fake[] = {{forged,2}};
  assert(tactix_add_rup(s,forged,2,fake,1,10000) == -1);
  assert(k.next_clause_id == id && k.strategy_epoch == epoch);
  assert(tactix_add_rup(s,target,2,premises,2,10000) == 0);
  assert(k.next_clause_id == id+1);
  int tautology[] = {1,-1};
  assert(tactix_add_rup(s,tautology,2,nullptr,0,10000) == 1);
  int invalid[] = {INT_MIN};
  assert(tactix_add_rup(s,invalid,1,premises,2,10000) == -1);
  assert(tactix_solve(s) == 10);
  tactix_release(s);
}

static void selected_units_and_empty() {
  Kernel k;
  kernel_reserve_vars(k,2);
  const Lit clause[] = {internalize(-1),internalize(2)};
  kernel_add_binary_clause(k,clause[0],clause[1],++k.next_clause_id);
  kernel_assign_unit(k,internalize(1));
  assert(propagate_apply(k) != ApplyResult::Unsat);
  RupConsequencePlan plan;
  // Root assignments are admissible explicit unit premises.
  assert(rup_consequence_prepare(k,{1},{{1}},10000,plan) && plan.already_true);
  assert(!rup_consequence_prepare(k,{},{{1},{2}},10000,plan));
}

static void moved_large_watch() {
  Kernel k;
  kernel_reserve_vars(k,4);
  const Lit lits[] = {internalize(1),internalize(2),internalize(3),internalize(4)};
  kernel_add_large_clause(k,lits,4,false,0,++k.next_clause_id);
  kernel_assign_unit(k,internalize(-1));
  assert(propagate_apply(k) != ApplyResult::Unsat);
  RupConsequencePlan plan;
  // Clause identity is independent of literal order and current watched pair.
  assert(rup_consequence_prepare(k,{2,3,4},{{4,3,2,1},{-1}},10000,plan));
  assert(!plan.already_true && plan.clause.size() == 3);
  assert(!rup_consequence_prepare(k,{2,3},{{4,3,2,1},{-1}},10000,plan));
}

int main() {
  public_inference();
  selected_units_and_empty();
  moved_large_watch();
}
