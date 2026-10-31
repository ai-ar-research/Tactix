// Bounded autonomous discovery for the cautious one-step Tseitin-cycle X7
// adapter.  Discovery is read-only: it freezes a complete live snapshot and
// an exact collection of authenticated XOR premise groups, but never mutates
// the Kernel, proof stream, or caller-owned output on failure.
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
  friend Result discover(const tseitin_cycle_live::Snapshot &, const Limits &,
                         Plan &);
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

// Derive one exact plan from a previously captured complete snapshot.  Every
// clause is classified; unsupported clauses remain residual and therefore
// exclude any candidate edge they touch.
Result discover(const tseitin_cycle_live::Snapshot &, const Limits &, Plan &);

// Enumerate every independently legal connected component in deterministic
// group order.  Each returned plan freezes the complete formula snapshot, so
// an apply can exact-match the selected component after a fresh recapture.
Result enumerate(const tseitin_cycle_live::Snapshot &, const Limits &,
                 std::vector<Plan> &);

// Visit legal components one at a time and retain only the first accepted
// component.  The complete live snapshot and equation census are still built
// before visitation; unrelated components are never copied into the result.
Result select(const tseitin_cycle_live::Snapshot &, const Limits &,
              ComponentSelector, void *, Plan &);

// Capture the complete current population and then discover.  Both phases are
// bounded and interruptible.  The output is replaced only on success.
Result capture(Kernel &, const tseitin_cycle_live::SnapshotLimits &,
               const Limits &, Plan &);

// Hand the frozen plan to the existing live adapter.  That adapter re-captures
// and independently authenticates the exact groups before producing an
// immutable transaction.
tseitin_cycle_live::Result
prepare(Kernel &, const Plan &, const tseitin_cycle_live::Request &,
        tseitin_cycle_live::Prepared &);

} // namespace tactix::tseitin_cycle_discovery
