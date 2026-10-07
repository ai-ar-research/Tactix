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
// Sources are an immutable authenticated checker-premise view, including any
// existing unit premises. A zero native ID is accepted only for an authenticated
// non-exact binary presence or a one-literal RootUnit presence; RootUnit stream
// binding and actual physical-source liveness are the live caller's obligation.
// No sentinel is promoted to an exact proof ID. Prospective units or assignment values must never
// be inserted into this view as if already authenticated. Each requested
// root fact must independently be RUP
// against this full view; native assignment values alone are never evidence.
// Every shortened residual is separately RUP checked before recognition.
// Recognition is scope-local: image contains authenticated equations; global
// elimination/consequences belong to the consuming AffineMatrix service.
// This prepares a certificate only: it does not publish, lease or mutate a
// Kernel. Live integration must revalidate physical identities and commit
// certified units/residuals with fresh proof IDs in one shared transaction.
// On every outcome other than Prepared, output remains unchanged. Optional
// completed census commits on Prepared or fully completed NoStructure only.
// Admitted exact populations are excluded; unresolved scopes retry only when
// their exact normalized body multiset changes. Census is nomination metadata,
// never proof authority. Both input and output census memory share maximum_bytes.
RootImageStatus prepare_affine_root_image(
    const std::vector<AffineLiveSource> &sources,
    const std::vector<External> &requested_root_facts,
    const RootImageLimits &, const std::function<bool(uint64_t)> &charge,
    RootImagePlan &output, const RootScopeCensus *previous = nullptr,
    RootScopeCensus *completed = nullptr, RootScopeEffort *effort = nullptr);
} // namespace tactix::affine_semantic_extension
