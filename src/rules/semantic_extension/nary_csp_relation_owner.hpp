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

// Neutral authenticated ownership for downstream n-ary actions. The session
// itself publishes nothing and is deliberately absent from the rule registry.
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

// Capture and stage a complete replacement session. Applied is the only status
// that changes output. A downstream publishing action can therefore install
// output only after its own atomic commit. Duplicate leaves output untouched.
NaryCspRelationCaptureResult nary_csp_relation_prepare_owner(
    Kernel &, const NaryCspRelationOwnerSession &,
    const NaryCspRelationCaptureRequest &,
    const NaryCspRelationSessionLimits &, NaryCspRelationOwnerSession &output);

} // namespace tactix
