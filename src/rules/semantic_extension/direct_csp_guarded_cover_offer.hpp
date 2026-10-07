#pragma once
#include "rules/semantic_extension/direct_csp_guarded_cover_action.hpp"
#include "rules/semantic_extension/direct_csp_guarded_cover_proposals.hpp"

namespace tactix {
struct DirectCspGuardedCoverOfferLimits {
  direct_csp_guarded_cover::Bounds theory;
  direct_csp_guarded_cover::proposals::Limits proposals;
  uint64_t maximum_work = 0;
  uint64_t maximum_index_clauses = 0;
  uint64_t maximum_index_literals = 0;
  uint64_t maximum_index_bytes = 0;
  uint64_t maximum_cover_candidates = 0;
  uint64_t maximum_role_queries = 0;
  uint64_t maximum_owners = 0;
  uint64_t maximum_offers = 0;
  uint64_t maximum_binary_assignments = 0;
  uint64_t maximum_binary_edges = 0;
  bool valid() const;
};
struct DirectCspGuardedCoverOfferStamp {
  const Kernel *kernel = nullptr;
  uint64_t arena_generation = 0, touch_epoch = 0, strategy_epoch = 0;
  uint64_t arena_words = 0, next_clause_id = 0;
  uint32_t variables = 0;
  size_t external_extent = 0;
  bool operator==(const DirectCspGuardedCoverOfferStamp &) const = default;
};
struct DirectCspGuardedCoverOfferWindow {
  // Signed preferences are nominations only, never assignments or SAT evidence.
  std::vector<int32_t> preferred_literals;
  // Resume only against exactly the returned stamp; zero starts a fresh window.
  DirectCspGuardedCoverOfferStamp stamp;
  uint64_t after_cover = 0;
  // A separate bounded role window can revisit a cover and choose the Nth
  // authenticated domain/OR role at one blocker; UINT32_MAX means baseline.
  // Other blockers retain their first role. Combined role products are unseen.
  uint32_t role_blocker = UINT32_MAX;
  uint64_t role_alternative = 0;
};
struct DirectCspGuardedCoverOfferSession {
  // No retained premise bodies: every call rebuilds the bounded arena index.
  // This deliberately pays for in-place Vivify and un-stamped native changes.
  uint64_t calls = 0;
};
enum class DirectCspGuardedCoverOfferStatus : uint8_t {
  Invalid, Inapplicable, Stale, ResourceExhausted, Interrupted, NoOffer, Offered,
};
struct DirectCspGuardedCoverOfferEffort {
  uint64_t work = 0, index_clauses = 0, index_literals = 0;
  uint64_t cover_candidates = 0, role_queries = 0, owners = 0;
  uint64_t binary_assignments = 0, binary_edges = 0;
  uint64_t existing_rejections = 0, committed_rejections = 0;
  uint64_t binary_implied_rejections = 0, ambiguous_source_rejections = 0;
};
struct DirectCspGuardedCoverOfferResult {
  DirectCspGuardedCoverOfferStatus status = DirectCspGuardedCoverOfferStatus::Invalid;
  DirectCspGuardedCoverOfferEffort effort;
  DirectCspGuardedCoverOfferStamp stamp;
  uint64_t after_cover = 0;
  bool arena_index_complete = false, cover_window_complete = false;
  // Arena covers, deterministic first roles, and local tuple neighborhoods only.
  bool all_schema_owners_complete = false;
  bool nomination_limit_reached = false, offer_limit_reached = false;
  std::vector<direct_csp_guarded_cover::proposals::Nomination> offers;
};
// Read-only formula/proof/session operation. Output is a fresh nomination for the
// existing public action, which must revalidate and atomically certify it.
DirectCspGuardedCoverOfferResult direct_csp_guarded_cover_offer(
    Kernel &, const DirectCspGuardedCoverActionSession &,
    DirectCspGuardedCoverOfferSession &, const DirectCspGuardedCoverOfferLimits &,
    const DirectCspGuardedCoverOfferWindow &);
} // namespace tactix
