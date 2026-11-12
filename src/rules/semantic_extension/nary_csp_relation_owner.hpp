#pragma once

#include "rules/semantic_extension/nary_csp_relation_live.hpp"

#include <cstdint>
#include <vector>

namespace tactix {

struct NaryCspRelationSessionLimits {
  uint64_t maximum_owners = 0;
  // Deterministic storage/copy units over the complete staged session.
  uint64_t maximum_work = 0;

  bool valid() const { return maximum_owners && maximum_work; }
};

// Authenticated ownership for downstream n-ary actions; publishes nothing and
// is not a registered rule.
class NaryCspRelationOwnerSession {
public:
  NaryCspRelationOwnerSession() = default;
  NaryCspRelationOwnerSession(const NaryCspRelationOwnerSession &) = delete;
  NaryCspRelationOwnerSession &
  operator=(const NaryCspRelationOwnerSession &) = delete;
  NaryCspRelationOwnerSession(NaryCspRelationOwnerSession &&) = default;
  NaryCspRelationOwnerSession &
  operator=(NaryCspRelationOwnerSession &&) = default;

  bool retained() const { return owner_ != nullptr; }
  bool belongs_to(const Kernel &kernel) const { return owner_ == &kernel; }
  size_t retained_owners() const { return snapshots_.size(); }
  uint64_t version() const { return version_; }
  uint64_t session_work() const { return session_work_; }
  const std::vector<NaryCspRelationOwnerSnapshot> &snapshots() const {
    return snapshots_;
  }
  const NaryCspRelationOwnerSnapshot *
  snapshot(const std::vector<std::vector<int32_t>> &domains) const;

private:
  friend NaryCspRelationCaptureResult nary_csp_relation_prepare_owner(
      Kernel &, const NaryCspRelationOwnerSession &,
      const NaryCspRelationCaptureRequest &,
      const NaryCspRelationSessionLimits &, NaryCspRelationOwnerSession &);

  const Kernel *owner_ = nullptr;
  uint64_t version_ = 0;
  uint64_t session_work_ = 0;
  std::vector<NaryCspRelationOwnerSnapshot> snapshots_;
};

// Whether two captures authenticate the same image (extent and work aside).
inline bool same_authenticated_image(
    const NaryCspRelationOwnerSnapshot &first,
    const NaryCspRelationOwnerSnapshot &second) {
  return first.source_literals == second.source_literals &&
         first.source == second.source && first.relation == second.relation;
}

// A downstream action's status for a declined recapture of a retained owner.
template <class Status>
Status nary_csp_owner_recapture_status(NaryCspRelationCaptureStatus status) {
  return status == NaryCspRelationCaptureStatus::Interrupted
             ? Status::Interrupted
         : status == NaryCspRelationCaptureStatus::ResourceExhausted
             ? Status::ResourceExhausted
             : Status::Stale;
}

// Capture a replacement session; only Applied changes `output`.
NaryCspRelationCaptureResult nary_csp_relation_prepare_owner(
    Kernel &, const NaryCspRelationOwnerSession &,
    const NaryCspRelationCaptureRequest &,
    const NaryCspRelationSessionLimits &, NaryCspRelationOwnerSession &output);

} // namespace tactix
