// Mutation-free root normalization and RUP certification of an affine subimage.
#pragma once
#include "rules/semantic_extension/affine_live.hpp"
#include <bit>
#include <functional>

namespace tactix::affine_semantic_extension {
// Saturated work of one scope-directory lookup of `width` over `entries`.
inline uint64_t scope_index_work(size_t width, size_t entries) {
  const __uint128_t amount =
      __uint128_t(1 + width) * (1 + std::bit_width(entries));
  return amount > UINT64_MAX ? UINT64_MAX : uint64_t(amount);
}
struct RootImageLimits {
  Limits image;
  uint64_t maximum_sources = 0, maximum_literals = 0;
  uint64_t maximum_units = 0, maximum_work = 0, maximum_bytes = 0;
  uint64_t maximum_proof_additions = 0;
};
// Exact normalized population identity; physical proof identities remain in residuals.
struct RootScopeRecord {
  std::vector<External> scope;
  std::vector<std::vector<External>> normalized_bodies;
  bool admitted = false; // Set only by the caller after successful native admission.
};
using RootScopeCensus = std::vector<RootScopeRecord>;
struct RootScopeEffort {
  uint64_t recognized_groups = 0, skipped_groups = 0, recognition_work = 0;
};
struct RootResidual {
  uint64_t occurrence = 0; // New plan-local identity, not a native clause ID.
  AffineLiveSource physical_source; // Full body and exact original identity.
  std::vector<External> clause;
  std::vector<External> removed_root_facts;
};
struct RootImagePlan {
  Image image;
  std::vector<External> certified_root_facts;
  // Source view of the root/residual RUP oracle.
  std::vector<AffineLiveSource> certification_sources;
  std::vector<RootResidual> residuals;
  uint64_t work = 0, rup_checks = 0;
  uint64_t accounted_peak_bytes = 0;
  // Preparation reservation, excluding the capture and later allocations.
  uint64_t reserved_bytes = 0;
};
enum class RootImageStatus { Prepared, NoStructure, Invalid, ResourceExhausted,
                             Interrupted, UncertifiedRoot };
// Plan root facts and shortened residuals, each RUP against the whole view,
// with scope-local recognition. Premises are immutable checker premises only;
// prospective assignments are never premises. Zero IDs are non-exact binaries
// or RootUnits; the caller checks stream/physical identity and never promotes
// a sentinel ID. Live integration atomically revalidates/leases premises and
// uses fresh IDs.
// Output changes only on Prepared; the census commits on Prepared or completed
// NoStructure, excludes admitted populations and retries unresolved scopes on
// body changes; it shares maximum_bytes. Obligations: docs/dev/calculus.md,
// docs/dev/implementation-notes.md.
RootImageStatus prepare_affine_root_image(
    const std::vector<AffineLiveSource> &sources,
    const std::vector<External> &requested_root_facts,
    const RootImageLimits &, const std::function<bool(uint64_t)> &charge,
    RootImagePlan &output, const RootScopeCensus *previous = nullptr,
    RootScopeCensus *completed = nullptr, RootScopeEffort *effort = nullptr);
} // namespace tactix::affine_semantic_extension
