// Feature-local controller boundary for the certified direct-PHP refutation.
// This layer deliberately does not register or schedule the action.
#pragma once

#include "proof/direct_php_live.hpp"
#include "rules/registry.hpp"

#include <cstdint>
#include <vector>

namespace tactix {

struct DirectPhpActionLimits {
  direct_php_live::SnapshotLimits snapshot;
  direct_php_recognition::Limits recognition;
  direct_php_certificate::Limits certificate;
  // Charges construction and exact comparison of replay records.  This is
  // separate from recognition and certificate planning because a controller
  // may persist or validate a record without changing either producer limit.
  uint64_t maximum_replay_work = 0;

  bool valid() const;
  friend bool operator==(const DirectPhpActionLimits &,
                         const DirectPhpActionLimits &);
};

struct DirectPhpActionRequest {
  uint32_t version = 1;
  uint64_t action_id = 0;
  DirectPhpActionLimits limits;

  friend bool operator==(const DirectPhpActionRequest &,
                         const DirectPhpActionRequest &) = default;
};

enum class DirectPhpActionStatus : uint8_t {
  Invalid,
  Inapplicable,
  NotRecognized,
  Malformed,
  Ambiguous,
  ResourceExhausted,
  Interrupted,
  UnsupportedProof,
  Stale,
  AlreadyApplied,
  ReplayMismatch,
  Prepared,
  Applied,
};

struct DirectPhpActionEffort {
  // Snapshot fields are the live adapter's metered admission capture.  Every
  // subsequent revalidation is independently constrained by the same
  // SnapshotLimits; replay_work covers this layer's record scan/comparison.
  uint64_t snapshot_clauses = 0;
  uint64_t snapshot_literals = 0;
  uint64_t snapshot_work = 0;
  uint64_t recognition_work = 0;
  uint64_t replay_work = 0;
};

// The record contains every input identity and every proof event needed to
// distinguish an exact replay from a merely equivalent refutation.
struct DirectPhpActionRecord {
  uint32_t version = 1;
  uint64_t action_id = 0;
  DirectPhpActionLimits limits;
  direct_php_live::Snapshot snapshot;
  std::vector<direct_php_live::ReplayEvent> events;
  direct_php_certificate::Accounting accounting;
  int32_t first_fresh = 0;
};

struct DirectPhpActionResult;

class DirectPhpActionSession {
public:
  bool committed() const { return owner_ != nullptr; }
  bool belongs_to(const Kernel &kernel) const { return owner_ == &kernel; }
  uint64_t action_id() const { return action_id_; }
  const DirectPhpActionRecord *record() const {
    return committed() ? &record_ : nullptr;
  }

private:
  friend struct DirectPhpActionSessionAccess;
  const Kernel *owner_ = nullptr;
  uint64_t action_id_ = 0;
  DirectPhpActionRecord record_;
};

class DirectPhpPreparedAction {
public:
  bool ready() const { return ready_; }
  const DirectPhpActionRecord &record() const { return record_; }
  const direct_php_certificate::Accounting &accounting() const {
    return record_.accounting;
  }

private:
  friend struct DirectPhpPreparedActionAccess;
  friend DirectPhpActionResult direct_php_action_prepare(
      Kernel &, DirectPhpActionSession &, const DirectPhpActionRequest &,
      DirectPhpPreparedAction &);
  friend DirectPhpActionResult direct_php_action_prepare_replay(
      Kernel &, DirectPhpActionSession &, const DirectPhpActionRequest &,
      const DirectPhpActionRecord &, DirectPhpPreparedAction &);
  friend DirectPhpActionResult direct_php_action_publish(
      Kernel &, DirectPhpActionSession &, DirectPhpPreparedAction &);
  bool ready_ = false;
  Kernel *owner_ = nullptr;
  DirectPhpActionSession *session_ = nullptr;
  direct_php_live::Admission admission_;
  direct_php_live::Transaction transaction_;
  DirectPhpActionRecord record_;
  DirectPhpActionEffort effort_;
};

struct DirectPhpActionResult {
  DirectPhpActionStatus status = DirectPhpActionStatus::Invalid;
  ApplyResult result = ApplyResult::NoOp;
  uint64_t action_id = 0;
  bool replay_checked = false;
  DirectPhpActionEffort effort;
  // Populated as soon as a complete live transaction exists, including an
  // exact-replay mismatch or replay-budget decline before publication.
  direct_php_certificate::Accounting proof;
};

// Formula/proof-state guard only.  Request validity and retained session state
// remain explicit inputs to preparation.
RuleEligibility direct_php_action_guard(const Kernel &);

DirectPhpActionResult direct_php_action_prepare(
    Kernel &, DirectPhpActionSession &, const DirectPhpActionRequest &,
    DirectPhpPreparedAction &);

DirectPhpActionResult direct_php_action_prepare_replay(
    Kernel &, DirectPhpActionSession &, const DirectPhpActionRequest &,
    const DirectPhpActionRecord &, DirectPhpPreparedAction &);

DirectPhpActionResult direct_php_action_publish(
    Kernel &, DirectPhpActionSession &, DirectPhpPreparedAction &);

DirectPhpActionResult direct_php_action_apply(
    Kernel &, DirectPhpActionSession &, const DirectPhpActionRequest &);

DirectPhpActionResult direct_php_action_replay(
    Kernel &, DirectPhpActionSession &, const DirectPhpActionRequest &,
    const DirectPhpActionRecord &);

} // namespace tactix
