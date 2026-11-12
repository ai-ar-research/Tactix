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
  // Charges replay-record construction and comparison.
  uint64_t maximum_replay_work = 0;

  bool valid() const;
  friend bool operator==(const DirectPhpActionLimits &,
                         const DirectPhpActionLimits &) = default;
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
  // Retained for its feedback counter; apply never reports it.
  UnsupportedProofSplit,
  Stale,
  AlreadyApplied,
  ReplayMismatch,
  Prepared,
  Applied,
};

struct DirectPhpActionEffort {
  // Admission capture of the live adapter; revalidations use the same limits.
  uint64_t snapshot_clauses = 0;
  uint64_t snapshot_literals = 0;
  uint64_t snapshot_work = 0;
  uint64_t recognition_work = 0;
  uint64_t replay_work = 0;
};

// Every input identity and proof event of the committed refutation.
struct DirectPhpActionRecord {
  uint32_t version = 1;
  uint64_t action_id = 0;
  DirectPhpActionLimits limits;
  direct_php_live::Snapshot snapshot;
  std::vector<direct_php_live::ReplayEvent> events;
  direct_php_certificate::Accounting accounting;
  int32_t first_fresh = 0;
};

class DirectPhpActionSession {
public:
  // Bound to one live Kernel and destroyed before it; non-copyable.
  explicit DirectPhpActionSession(Kernel &kernel) : owner_(&kernel) {}
  DirectPhpActionSession(const DirectPhpActionSession &) = delete;
  DirectPhpActionSession &operator=(const DirectPhpActionSession &) = delete;
  DirectPhpActionSession(DirectPhpActionSession &&) = delete;
  DirectPhpActionSession &operator=(DirectPhpActionSession &&) = delete;

  bool committed() const { return committed_; }
  bool belongs_to(const Kernel &kernel) const { return owner_ == &kernel; }
  uint64_t action_id() const { return action_id_; }
  const DirectPhpActionRecord *record() const {
    return committed() ? &record_ : nullptr;
  }

private:
  friend struct DirectPhpActionSessionAccess;
  const Kernel *const owner_;
  bool committed_ = false;
  uint64_t action_id_ = 0;
  DirectPhpActionRecord record_;
};

struct DirectPhpActionResult {
  DirectPhpActionStatus status = DirectPhpActionStatus::Invalid;
  ApplyResult result = ApplyResult::NoOp;
  uint64_t action_id = 0;
  bool replay_checked = false; // apply compares no external replay record
  DirectPhpActionEffort effort;
  // Populated as soon as a complete live transaction exists, including a
  // replay-budget decline before publication.
  direct_php_certificate::Accounting proof;
};

// Formula/proof-state guard only.  Request validity and retained session state
// remain explicit inputs to apply.
RuleEligibility direct_php_action_guard(const Kernel &);

// Recognize, certify and publish the refutation in one exclusive call; a
// committed session answers AlreadyApplied or ReplayMismatch.
DirectPhpActionResult direct_php_action_apply(
    Kernel &, DirectPhpActionSession &, const DirectPhpActionRequest &);

// Append one public invocation's exact terminal status, live effort, and proof
// accounting to the observation counters.  Call exactly once per request.
void record_direct_php_action_feedback(Kernel &, const DirectPhpActionResult &);

} // namespace tactix
