#pragma once
#include <cstdint>
#include <utility>
namespace tactix::strategy::affine {
// A single invocation-owned work ledger, borrowed by all nested algorithms and
// candidate callbacks. Zero is zero work. Failure is sticky until a new call.
template<class Stop> class Work {
public:
  Work(uint64_t limit, Stop stop) : limit_(limit), stop_(std::move(stop)) {}
  bool charge(uint64_t amount) {
    if (exhausted_ || interrupted_) return false;
    if (stop_()) { interrupted_ = true; return false; }
    if (amount > limit_ - used_) { exhausted_ = true; return false; }
    used_ += amount;
    return true;
  }
  void exhaust() { exhausted_ = true; }
  uint64_t used() const { return used_; }
  bool interrupted() const { return interrupted_; }
  bool exhausted() const { return exhausted_; }
private:
  uint64_t limit_, used_ = 0;
  Stop stop_;
  bool exhausted_ = false, interrupted_ = false;
};
}
