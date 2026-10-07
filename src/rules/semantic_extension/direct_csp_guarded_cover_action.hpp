#pragma once
#include "rules/semantic_extension/direct_csp_guarded_cover_live.hpp"
#include "rules/semantic_extension/direct_csp_support_action.hpp"
#include <string>

namespace tactix {
using DirectCspGuardedCoverActionSelection = direct_csp_guarded_cover::Selection;
using DirectCspGuardedCoverActionResult = DirectCspSupportActionResult;

struct DirectCspGuardedCoverActionLimits {
  uint64_t maximum_source_clauses = 0;
  uint64_t maximum_source_literals = 0;
  uint64_t maximum_recognition_work = 0;
  uint32_t maximum_domains = 0;
  uint32_t maximum_domain_values = 0;
  uint32_t maximum_cover_literals = 0;
  uint32_t maximum_or_definitions = 0;
  uint32_t maximum_or_inputs = 0;
  uint32_t maximum_output_literals = 0;
  uint64_t maximum_proof_work = 0;
  uint64_t maximum_proof_additions = 0;
  uint64_t maximum_proof_literal_occurrences = 0;
  uint64_t maximum_journal_bytes = 0;
  uint32_t maximum_external_extent = 0;
  uint64_t maximum_session_keys = 0;
  uint64_t maximum_session_work = 0;
  bool valid() const;
};

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
