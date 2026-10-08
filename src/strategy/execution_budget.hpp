#pragma once

// Policy-neutral invocation accounting. These are resource requests, never
// rule soundness conditions. No solver clock or default is defined here.
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace tactix::strategy {

enum class WorkUnit : uint8_t {
  Applications,
  BveTicks,
  Propagations,
  SrDiscoveryTicks,
};

class WorkLimit {
public:
  static constexpr WorkLimit bounded(uint64_t amount) { return {false, amount}; }
  static constexpr WorkLimit unlimited() { return {true, 0}; }
  constexpr bool is_unlimited() const { return unlimited_; }
  // Meaningful only for a bounded limit; unlimited is not UINT64_MAX.
  constexpr uint64_t amount() const { return amount_; }
  constexpr bool admits_work() const { return unlimited_ || amount_ != 0; }

private:
  constexpr WorkLimit(bool unlimited, uint64_t amount)
      : unlimited_(unlimited), amount_(amount) {}
  bool unlimited_;
  uint64_t amount_;
};

class InvocationBudget;

// Scope-owned shared pool. A repeated/duplicated request borrows this pool;
// it cannot copy it to replenish an enclosing allowance. Non-thread-safe,
// like the solver; owners and borrowed spans outlive each invocation.
class Allowance {
public:
  Allowance(WorkUnit unit, WorkLimit limit) : unit_(unit), limit_(limit) {}
  Allowance(const Allowance &) = delete;
  Allowance &operator=(const Allowance &) = delete;
  WorkUnit unit() const { return unit_; }
  uint64_t consumed() const { return consumed_; }
  WorkLimit limit() const { return limit_; }
  bool accounting_overflow() const { return overflow_; }
  WorkLimit remaining() const {
    if (overflow_)
      return WorkLimit::bounded(0);
    if (limit_.is_unlimited())
      return limit_;
    return WorkLimit::bounded(consumed_ < limit_.amount()
                                  ? limit_.amount() - consumed_ : 0);
  }
  // Explicit online limit change never resets consumed work. Increases are
  // deliberate configuration changes, not a side effect of resuming a step.
  void set_limit(WorkLimit limit) { limit_ = limit; }

private:
  friend class InvocationBudget;
  void charge(uint64_t amount) {
    constexpr uint64_t maximum = std::numeric_limits<uint64_t>::max();
    if (amount > maximum - consumed_) {
      consumed_ = maximum;
      overflow_ = true;
    } else {
      consumed_ += amount;
    }
  }
  WorkUnit unit_;
  WorkLimit limit_;
  uint64_t consumed_ = 0;
  bool overflow_ = false;
};

enum class BudgetStatus : uint8_t {
  Ready, Exhausted, InvalidPool, CurrencyMismatch, DuplicatePool,
  AccountingOverflow
};

class InvocationBudget {
public:
  InvocationBudget(WorkUnit unit, WorkLimit cap,
                   std::span<Allowance *const> pools = {})
      : local_(unit, cap), pools_(pools) {
    for (std::size_t i = 0; i < pools_.size(); ++i) {
      if (!pools_[i]) {
        validation_ = BudgetStatus::InvalidPool;
        return;
      }
      if (pools_[i]->unit() != unit) {
        validation_ = BudgetStatus::CurrencyMismatch;
        return;
      }
      for (std::size_t j = 0; j < i; ++j)
        if (pools_[j] == pools_[i]) {
          validation_ = BudgetStatus::DuplicatePool;
          return;
        }
    }
  }
  InvocationBudget(const InvocationBudget &) = delete;
  InvocationBudget &operator=(const InvocationBudget &) = delete;
  uint64_t consumed() const { return local_.consumed(); }
  WorkUnit unit() const { return local_.unit(); }
  BudgetStatus status() const {
    if (validation_ != BudgetStatus::Ready)
      return validation_;
    if (local_.accounting_overflow())
      return BudgetStatus::AccountingOverflow;
    for (const Allowance *pool : pools_)
      if (pool->accounting_overflow())
        return BudgetStatus::AccountingOverflow;
    return remaining_unchecked().admits_work() ? BudgetStatus::Ready
                                              : BudgetStatus::Exhausted;
  }
  WorkLimit remaining() const {
    return status() == BudgetStatus::Ready ? remaining_unchecked()
                                         : WorkLimit::bounded(0);
  }
  // Charge a completed atomic semantic unit at its actual cost. Overshoot
  // is recorded, not clamped to a fictitious cap. Caller checks Ready before
  // beginning work; no interleaving/configure occurs inside that atomic unit.
  BudgetStatus charge(WorkUnit currency, uint64_t amount) {
    if (currency != unit())
      return BudgetStatus::CurrencyMismatch;
    const BudgetStatus before = status();
    if (before != BudgetStatus::Ready)
      return before;
    local_.charge(amount);
    for (Allowance *pool : pools_)
      pool->charge(amount);
    return status();
  }

private:
  WorkLimit remaining_unchecked() const {
    WorkLimit result = local_.remaining();
    for (const Allowance *pool : pools_) {
      const WorkLimit candidate = pool->remaining();
      if (!candidate.is_unlimited() &&
          (result.is_unlimited() || candidate.amount() < result.amount()))
        result = candidate;
    }
    return result;
  }
  Allowance local_;
  std::span<Allowance *const> pools_;
  BudgetStatus validation_ = BudgetStatus::Ready;
};

} // namespace tactix::strategy
