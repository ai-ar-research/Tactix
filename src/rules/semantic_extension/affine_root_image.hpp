// Mutation-free root normalization and RUP certification of an affine subimage.
#pragma once
#include "rules/semantic_extension/affine_live.hpp"
#include <functional>

namespace tactix::affine_semantic_extension {
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
  // Complete exact source view used by the root/residual RUP oracle.
  // Kept separately from residual occurrence identities for revalidation.
  std::vector<AffineLiveSource> certification_sources;
  std::vector<RootResidual> residuals;
  uint64_t work = 0, rup_checks = 0;
  uint64_t accounted_peak_bytes = 0;
  // Conservative helper-owned simultaneous preparation reservation; excludes
  // the caller-owned capture and any future matrix/session allocations.
  uint64_t reserved_bytes = 0;
};
enum class RootImageStatus { Prepared, NoStructure, Invalid, ResourceExhausted,
                             Interrupted, UncertifiedRoot };
// Immutable checker premises only; prospective assignments are never premises.
// Zero IDs represent authenticated non-exact binaries or RootUnits only; the
// caller checks stream/physical identity and never promotes a sentinel ID.
// Each root fact and shortened residual must be RUP against the complete view.
// Recognition is scope-local; this helper never publishes or mutates a Kernel.
// Live integration atomically revalidates/leases premises and uses fresh IDs.
// Non-Prepared outcomes leave output unchanged; completed census commits only
// on Prepared or completed NoStructure. Census is nomination, not proof.
// Admitted populations are excluded; unresolved scopes retry on exact body
// changes. Input/output census memory shares maximum_bytes. Lifecycle and
// certification obligations: docs/calculus.md, docs/implementation-notes.md.
RootImageStatus prepare_affine_root_image(
    const std::vector<AffineLiveSource> &sources,
    const std::vector<External> &requested_root_facts,
    const RootImageLimits &, const std::function<bool(uint64_t)> &charge,
    RootImagePlan &output, const RootScopeCensus *previous = nullptr,
    RootScopeCensus *completed = nullptr, RootScopeEffort *effort = nullptr);
} // namespace tactix::affine_semantic_extension
