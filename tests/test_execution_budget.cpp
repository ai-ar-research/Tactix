#undef NDEBUG
#include "strategy/execution_budget.hpp"

#include <cassert>
#include <cstdio>
#include <limits>
#include <type_traits>

using namespace tactix::strategy;
constexpr WorkUnit unit = WorkUnit::BveTicks;

int main() {
  static_assert(!std::is_copy_constructible_v<Allowance>);
  static_assert(!std::is_move_constructible_v<Allowance>);
  static_assert(!std::is_copy_constructible_v<InvocationBudget>);
  static_assert(!WorkLimit::bounded(0).admits_work());
  static_assert(WorkLimit::unlimited().admits_work());

  uint64_t fixtures = 0;
  // Finite allowance/cap and actual atomic-unit costs, including overshoot.
  for (uint64_t cap = 0; cap <= 8; ++cap)
    for (uint64_t pool_cap = 0; pool_cap <= 8; ++pool_cap)
      for (uint64_t amount = 0; amount <= 10; ++amount) {
        Allowance pool(unit, WorkLimit::bounded(pool_cap));
        Allowance *pools[] = {&pool};
        InvocationBudget call(unit, WorkLimit::bounded(cap), pools);
        const bool ready = cap && pool_cap;
        assert((call.status() == BudgetStatus::Ready) == ready);
        assert(call.remaining().amount() == (cap < pool_cap ? cap : pool_cap));
        const auto result = call.charge(unit, amount);
        const uint64_t expected = ready ? amount : 0;
        assert(call.consumed() == expected && pool.consumed() == expected);
        assert((result == BudgetStatus::Ready) ==
               (ready && amount < cap && amount < pool_cap));
        ++fixtures;
      }

  Allowance phase(unit, WorkLimit::bounded(10));
  Allowance outer(unit, WorkLimit::bounded(20));
  Allowance *pools[] = {&phase, &outer};
  InvocationBudget first(unit, WorkLimit::bounded(4), pools);
  assert(first.charge(unit, 3) == BudgetStatus::Ready);
  // Pause/resume uses the same invocation object and retains its cap charge.
  assert(first.remaining().amount() == 1);
  assert(first.charge(unit, 2) == BudgetStatus::Exhausted);
  assert(first.consumed() == 5 && phase.consumed() == 5 && outer.consumed() == 5);
  InvocationBudget repeated(unit, WorkLimit::bounded(100), pools);
  assert(repeated.remaining().amount() == 5);
  assert(repeated.charge(unit, 6) == BudgetStatus::Exhausted);
  assert(phase.consumed() == 11 && outer.consumed() == 11);
  InvocationBudget third(unit, WorkLimit::unlimited(), pools);
  assert(third.status() == BudgetStatus::Exhausted);
  phase.set_limit(WorkLimit::bounded(12));
  assert(third.remaining().amount() == 1 && phase.consumed() == 11);
  phase.set_limit(WorkLimit::bounded(5));
  assert(third.status() == BudgetStatus::Exhausted && phase.consumed() == 11);

  Allowance foreign(WorkUnit::Propagations, WorkLimit::unlimited());
  Allowance *wrong[] = {&foreign};
  InvocationBudget mismatch(unit, WorkLimit::unlimited(), wrong);
  assert(mismatch.status() == BudgetStatus::CurrencyMismatch);
  assert(mismatch.charge(unit, 5) == BudgetStatus::CurrencyMismatch);
  assert(foreign.consumed() == 0);
  Allowance *duplicate[] = {&outer, &outer};
  InvocationBudget duplicate_call(unit, WorkLimit::unlimited(), duplicate);
  assert(duplicate_call.status() == BudgetStatus::DuplicatePool);
  assert(duplicate_call.charge(unit, 5) == BudgetStatus::DuplicatePool);
  assert(outer.consumed() == 11);
  Allowance *null_pool[] = {nullptr};
  InvocationBudget invalid(unit, WorkLimit::unlimited(), null_pool);
  assert(invalid.status() == BudgetStatus::InvalidPool);

  InvocationBudget unlimited(unit, WorkLimit::unlimited());
  assert(unlimited.remaining().is_unlimited());
  assert(unlimited.charge(WorkUnit::Applications, 1) == BudgetStatus::CurrencyMismatch);
  assert(unlimited.consumed() == 0 && unlimited.status() == BudgetStatus::Ready);
  constexpr uint64_t maximum = std::numeric_limits<uint64_t>::max();
  assert(unlimited.charge(unit, maximum) == BudgetStatus::Ready);
  assert(unlimited.charge(unit, 1) == BudgetStatus::AccountingOverflow);
  assert(unlimited.consumed() == maximum);
  assert(unlimited.remaining().amount() == 0);

  Allowance huge(unit, WorkLimit::unlimited());
  Allowance *huge_pools[] = {&huge};
  InvocationBudget before(unit, WorkLimit::unlimited(), huge_pools);
  assert(before.charge(unit, maximum) == BudgetStatus::Ready);
  InvocationBudget after(unit, WorkLimit::unlimited(), huge_pools);
  assert(after.charge(unit, 2) == BudgetStatus::AccountingOverflow);
  assert(after.consumed() == 2 && huge.consumed() == maximum);
  huge.set_limit(WorkLimit::unlimited());
  assert(after.status() == BudgetStatus::AccountingOverflow);
  std::printf("execution budgets: %llu exhaustive finite fixtures plus scope/"
              "resume/configuration/currency/overflow controls PASS\n",
              static_cast<unsigned long long>(fixtures));
}
