#pragma once

#include <cstdint>

namespace tactix {

// Bounded deterministic work accounting for pure Lucky helpers whose caller
// supplies an interruption callback. Status values remain owned by each
// helper's public result type.
template <class Limits, class Status, Status Running, Status Interrupted,
          Status Exhausted>
class CallbackWorkMeter {
  const Limits &limits_;
  uint64_t used_ = 0;
  Status stopped_ = Running;

public:
  explicit CallbackWorkMeter(const Limits &limits) : limits_(limits) {}

  bool spend(uint64_t amount = 1) {
    if (limits_.interrupted && limits_.interrupted(limits_.interrupt_state)) {
      stopped_ = Interrupted;
      return false;
    }
    if (amount > limits_.work - used_) {
      stopped_ = Exhausted;
      return false;
    }
    used_ += amount;
    return true;
  }

  uint64_t used() const { return used_; }
  Status stopped() const { return stopped_; }
};

} // namespace tactix
