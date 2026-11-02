#pragma once

#include "kernel/kernel.hpp"

#include <cstdint>

namespace tactix {

// Admits `amount` into `work` within `maximum`. A refused admission, including
// one whose `work` already exceeds `maximum`, leaves `work` unchanged.
inline bool add_bounded(uint64_t amount, uint64_t maximum, uint64_t &work) {
  if (work > maximum || amount > maximum - work) return false;
  work += amount;
  return true;
}

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

// The status a stopped meter reports: Interrupted takes precedence over
// ResourceExhausted; a meter that did neither reports `otherwise`.
template <class Status>
Status stopped_status(const InterruptibleWork &meter, Status otherwise) {
  if (meter.interrupted) return Status::Interrupted;
  if (meter.exhausted) return Status::ResourceExhausted;
  return otherwise;
}

// The status of a stop observed without a meter: Interrupted under
// termination, otherwise `otherwise`. Polls termination exactly once.
template <class Status>
Status terminating_status(const Kernel &kernel, Status otherwise) {
  return kernel_terminating(kernel) ? Status::Interrupted : otherwise;
}

// A `ready` kernel admits the caller; otherwise the stop is Inapplicable, or
// Interrupted under termination.
template <class Status, class Ready>
bool root_admitted(const Kernel &kernel, Ready &&ready, Status &status) {
  if (ready(kernel)) return true;
  status = terminating_status(kernel, Status::Inapplicable);
  return false;
}

} // namespace tactix
