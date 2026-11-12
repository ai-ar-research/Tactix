// Bounded read-only discovery for the one-step Tseitin-cycle X7 adapter: a
// frozen live snapshot plus authenticated XOR premise groups.
#pragma once

#include "rules/lucky/tseitin_cycle_live.hpp"

#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

namespace tactix::tseitin_cycle_discovery {

struct Limits {
  uint64_t work = 0;
  uint64_t maximum_clauses = 0;
  uint64_t maximum_groups = 0;
  uint32_t maximum_width = 0;
  tseitin_cycle::InterruptPoll interrupted = nullptr;
  void *interrupt_state = nullptr;
  uint64_t maximum_components = std::numeric_limits<uint64_t>::max();
  // Bounds canonical retained payload bytes (vectors and their elements),
  // excluding allocator and shared_ptr control-block overhead.
  uint64_t maximum_retained_bytes = std::numeric_limits<uint64_t>::max();
};

enum class Status : uint8_t {
  Discovered,
  Inapplicable,
  Declined,
  ResourceExhausted,
  Interrupted,
  InvalidInput,
};

enum class Reason : uint8_t {
  None,
  InvalidLimits,
  InvalidSnapshot,
  ClauseLimit,
  GroupLimit,
  WorkLimit,
  Interrupted,
  NoCompleteEquations,
  AmbiguousEquation,
  Disconnected,
  EdgeNotInExactlyTwoEquations,
  EvenCharge,
  ResidualEdgeTouch,
};

struct Result;
class Plan;
using ComponentSelector = bool (*)(const Plan &, void *);

class Plan {
public:
  bool ready() const { return ready_ && snapshot_; }
  const tseitin_cycle_live::Snapshot &snapshot() const { return *snapshot_; }
  const std::vector<tseitin_cycle::CertifiedGroup> &groups() const {
    return groups_;
  }

private:
  friend Result enumerate(const tseitin_cycle_live::Snapshot &, const Limits &,
                          std::vector<Plan> &);
  friend Result select(const tseitin_cycle_live::Snapshot &, const Limits &,
                       ComponentSelector, void *, Plan &);
  friend Result enumerate_impl(const tseitin_cycle_live::Snapshot &,
                               const Limits &, std::vector<Plan> *,
                               ComponentSelector, void *, Plan *);
  bool ready_ = false;
  std::shared_ptr<const tseitin_cycle_live::Snapshot> snapshot_;
  std::vector<tseitin_cycle::CertifiedGroup> groups_;
};

struct Result {
  Status status = Status::InvalidInput;
  Reason reason = Reason::InvalidLimits;
  uint64_t work = 0;
  uint64_t clauses = 0;
  uint64_t complete_groups = 0;
  uint64_t selected_groups = 0;
  uint64_t components = 0;
  uint64_t retained_bytes = 0;
  bool discovered() const { return status == Status::Discovered; }
};

// Enumerate every legal connected component in group order; unsupported
// clauses stay residual and exclude the edges they touch. Each plan freezes
// the snapshot so apply can exact-match it after recapture.
Result enumerate(const tseitin_cycle_live::Snapshot &, const Limits &,
                 std::vector<Plan> &);

// Visit legal components in order and keep only the first accepted one.
Result select(const tseitin_cycle_live::Snapshot &, const Limits &,
              ComponentSelector, void *, Plan &);

// The live adapter re-captures and re-authenticates the plan's groups.
tseitin_cycle_live::Result
prepare(Kernel &, const Plan &, const tseitin_cycle_live::Request &,
        tseitin_cycle_live::Prepared &);

} // namespace tactix::tseitin_cycle_discovery
