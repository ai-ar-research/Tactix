#pragma once

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

class Gf3GaussianActionSession {
public:
  bool retained() const { return owner_ != nullptr; }
  bool belongs_to(const Kernel &kernel) const { return owner_ == &kernel; }
  const std::vector<std::string> &keys() const { return keys_; }

private:
  friend struct Gf3GaussianActionSessionAccess;
  const Kernel *owner_ = nullptr;
  std::vector<std::string> keys_;
};

enum class Gf3GaussianActionStatus : uint8_t {
  Invalid,
  Inapplicable,
  NoStructure,
  NoSupportedOutcome,
  ResourceExhausted,
  Interrupted,
  UnsupportedProof,
  Stale,
  AlreadyPublished,
  Applied,
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

struct Gf3GaussianActionOfferResult {
  Gf3GaussianActionStatus status = Gf3GaussianActionStatus::Invalid;
  std::vector<std::string> offers;
  uint64_t session_work = 0;
  uint64_t session_identity_bytes = 0;
  gf3_gaussian_live::Result live;
};

RuleEligibility gf3_gaussian_action_guard(const Kernel &);

// Rebuild and authenticate the complete Gaussian plan on every call, then
// publish exactly the caller-selected identity.  A missing or changed identity
// is stale and cannot mutate solver, proof, journal, or session state.
Gf3GaussianActionResult gf3_gaussian_action_apply(
    Kernel &, Gf3GaussianActionSession &, const Gf3GaussianActionLimits &,
    const std::string &identity);

// Mutation-free enumeration of the singular complete-batch identity.  It
// commits every consequence row, full Source/Scale/Add provenance, every live
// authentication reference and its exact source clause.
Gf3GaussianActionOfferResult gf3_gaussian_action_offer(
    Kernel &, const Gf3GaussianActionSession &,
    const Gf3GaussianActionLimits &);

void record_gf3_gaussian_action_feedback(Kernel &,
                                         const Gf3GaussianActionResult &);

} // namespace tactix
