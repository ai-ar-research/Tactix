#pragma once
#include "rules/semantic_extension/direct_csp_action_limits.hpp"
#include "rules/semantic_extension/direct_csp_guarded_cover_live.hpp"
#include "rules/semantic_extension/direct_csp_support_action.hpp"
#include <string>

namespace tactix {
using DirectCspGuardedCoverActionSelection = direct_csp_guarded_cover::Selection;
using DirectCspGuardedCoverActionResult = DirectCspSupportActionResult;

enum class DirectCspGuardedCoverKeyLookup : uint8_t {
  Absent, Present, Invalid, ResourceExhausted,
};

class DirectCspGuardedCoverActionSession {
public:
  bool retained() const { return owner_ != nullptr; }
  bool belongs_to(const Kernel &k) const { return owner_ == &k; }
  size_t committed_keys() const { return keys_.size(); }
  size_t retained_owners() const { return owners_.size(); }
  const DirectCspGuardedCoverOwnerSnapshot *snapshot(
      const DirectCspGuardedCoverActionSelection &) const;
  // Durable identities survive deletion of their published clause. Lookup is
  // pure and uses the same authenticated image and full tuple as publication;
  // an exhausted charge is not evidence that a key is absent.
  DirectCspGuardedCoverKeyLookup lookup_committed(
      const DirectCspGuardedCoverActionSelection &,
      const direct_csp_guarded_cover::Bounds &,
      direct_csp_guarded_cover::Charge) const;
private:
  friend struct DirectCspGuardedCoverSessionAccess;
  const Kernel *owner_ = nullptr;
  std::vector<std::string> keys_;
  std::vector<DirectCspGuardedCoverOwnerSnapshot> owners_;
};

RuleEligibility direct_csp_guarded_cover_action_guard(const Kernel &);
DirectCspGuardedCoverActionResult direct_csp_guarded_cover_action_apply(
    Kernel &, DirectCspGuardedCoverActionSession &,
    const DirectCspGuardedCoverActionLimits &,
    const DirectCspGuardedCoverActionSelection &);
} // namespace tactix
