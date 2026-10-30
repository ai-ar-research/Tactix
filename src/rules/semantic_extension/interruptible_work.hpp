#pragma once

#include "kernel/kernel.hpp"

#include <cstdint>

namespace tactix {

// Shared bounded-work accounting for semantic-extension recognition and
// validation.  Charging observes termination before the allowance, while
// absorb deliberately accounts already-performed child work without another
// termination check.
struct InterruptibleWork {
  const Kernel &kernel;
  uint64_t maximum = 0;
  uint64_t used = 0;
  bool honor_termination = true;
  bool interrupted = false;
  bool exhausted = false;

  InterruptibleWork(const Kernel &kernel, uint64_t maximum,
                    uint64_t used = 0, bool honor_termination = true)
      : kernel(kernel), maximum(maximum), used(used),
        honor_termination(honor_termination) {}

  bool charge(uint64_t amount) {
    if (honor_termination && kernel_terminating(kernel)) {
      interrupted = true;
      return false;
    }
    return absorb(amount);
  }

  bool absorb(uint64_t amount) {
    if (used > maximum || amount > maximum - used) {
      exhausted = true;
      return false;
    }
    used += amount;
    return true;
  }

  uint64_t remaining() const { return maximum - used; }

  static bool callback(void *context, uint64_t amount) {
    return static_cast<InterruptibleWork *>(context)->charge(amount);
  }
};

} // namespace tactix
