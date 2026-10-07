#pragma once
#include "rules/semantic_extension/direct_csp_guarded_cover.hpp"
#include "rules/semantic_extension/direct_csp_support_live.hpp"

namespace tactix {

struct DirectCspGuardedCoverOwnerSnapshot {
  direct_csp_guarded_cover::Selection selection;
  std::vector<direct_csp_guarded_cover::Premise> source;
  uint64_t source_literals = 0;
};

// A structural owner is independent of the selected tuple. Fresh requests
// with another selected value reuse the source only after exact recapture.
bool direct_csp_guarded_cover_owner_shape_equal(
    const direct_csp_guarded_cover::Selection &,
    const direct_csp_guarded_cover::Selection &);

struct DirectCspGuardedCoverLiveRequest {
  direct_csp_guarded_cover::Selection selection;
  direct_csp_guarded_cover::Bounds bounds;
  const DirectCspGuardedCoverOwnerSnapshot *retained_owner = nullptr;
  uint64_t maximum_recognition_work = 0;
  uint64_t maximum_proof_work = 0;
  uint64_t maximum_proof_additions = 0;
  uint64_t maximum_proof_literal_occurrences = 0;
  uint64_t maximum_journal_bytes = 0;
  uint32_t maximum_external_extent = 0;
  uint64_t action_id = 0;
  uint64_t semantic_id = 0;
  FILE *records = nullptr;
};

DirectCspSupportOwnerCaptureResult direct_csp_guarded_cover_live_capture_owner(
    Kernel &, const DirectCspGuardedCoverLiveRequest &,
    DirectCspGuardedCoverOwnerSnapshot &);
DirectCspSupportLiveResult direct_csp_guarded_cover_live_preflight(
    Kernel &, const DirectCspGuardedCoverLiveRequest &);
DirectCspSupportLiveResult direct_csp_guarded_cover_live_apply(
    Kernel &, const DirectCspGuardedCoverLiveRequest &);
} // namespace tactix
