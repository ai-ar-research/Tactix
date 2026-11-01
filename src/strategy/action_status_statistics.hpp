#pragma once

#include <cstdint>

namespace tactix::strategy {

struct StandardActionStatusCounters {
  uint64_t &applied;
  uint64_t &invalid;
  uint64_t &inapplicable;
  uint64_t &resource_exhausted;
  uint64_t &interrupted;
  uint64_t &unsupported_proof;
  uint64_t &stale;
};

template <class Status>
bool record_standard_action_status(Status status,
                                   StandardActionStatusCounters counters) {
  uint64_t *counter = nullptr;
  switch (status) {
  case Status::Applied: counter = &counters.applied; break;
  case Status::Invalid: counter = &counters.invalid; break;
  case Status::Inapplicable: counter = &counters.inapplicable; break;
  case Status::ResourceExhausted: counter = &counters.resource_exhausted; break;
  case Status::Interrupted: counter = &counters.interrupted; break;
  case Status::UnsupportedProof: counter = &counters.unsupported_proof; break;
  case Status::Stale: counter = &counters.stale; break;
  default: return false;
  }
  ++*counter;
  return true;
}

} // namespace tactix::strategy

#define TACTIX_STANDARD_ACTION_STATUS_COUNTERS(stats, prefix)                 \
  ::tactix::strategy::StandardActionStatusCounters {                         \
    (stats).prefix##_applied, (stats).prefix##_invalid,                       \
        (stats).prefix##_inapplicable, (stats).prefix##_resource_exhausted,   \
        (stats).prefix##_interrupted, (stats).prefix##_unsupported_proof,     \
        (stats).prefix##_stale                                                \
  }
