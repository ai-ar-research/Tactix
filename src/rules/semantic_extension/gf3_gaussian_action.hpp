#pragma once

#include "rules/semantic_extension/action_epilogue.hpp"
#include "rules/semantic_extension/gf3_gaussian_live.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace tactix {

struct Gf3GaussianActionLimits {
  gf3_affine::RecognitionBounds recognition;
  gf3_gaussian_certificate::Limits certificate;
  uint64_t maximum_capture_work = 0;
  uint64_t maximum_revalidation_work = 0;
  uint64_t maximum_proof_additions = 0;
  uint64_t maximum_proof_literal_occurrences = 0;
  uint64_t maximum_journal_bytes = 0;
  uint64_t maximum_identity_bytes = 0;
  uint32_t maximum_external_extent = 0;
  uint64_t maximum_session_keys = 0;
  uint64_t maximum_session_work = 0;
  uint64_t maximum_session_identity_bytes = 0;

  bool valid() const;
};

struct Gf3GaussianActionSessionAccess;
class Gf3GaussianActionSession
    : public KernelKeyedSession<std::string, Gf3GaussianActionSessionAccess> {
};

enum class Gf3GaussianActionStatus : uint8_t {
  Invalid, Inapplicable, NoStructure, NoSupportedOutcome, ResourceExhausted,
  Interrupted, UnsupportedProof, Stale, AlreadyPublished, Applied
};

struct Gf3GaussianActionResult {
  Gf3GaussianActionStatus status = Gf3GaussianActionStatus::Invalid;
  ApplyResult result = ApplyResult::NoOp;
  uint64_t proposed_action_id = 0;
  uint64_t committed_action_id = 0;
  uint64_t semantic_id = 0;
  std::string identity;
  uint64_t session_work = 0;
  uint64_t session_identity_bytes = 0;
  gf3_gaussian_live::Result live;
};

RuleEligibility gf3_gaussian_action_guard(const Kernel &);

// Rebuild the Gaussian plan and publish the selected identity; a changed
// identity is Stale and mutates nothing.
Gf3GaussianActionResult gf3_gaussian_action_apply(
    Kernel &, Gf3GaussianActionSession &, const Gf3GaussianActionLimits &,
    const std::string &identity);

void record_gf3_gaussian_action_feedback(Kernel &,
                                         const Gf3GaussianActionResult &);

} // namespace tactix
