#pragma once

#include "parameter_schema.hpp"

#include <cstdint>

namespace tactix::strategy {

// Reference-backed Sweep defaults shared by all invocations.
struct SweepReferenceDefaults {
  static constexpr uint32_t DEPTH = 2;
  static constexpr uint32_t MAX_DEPTH = 3;
  static constexpr uint32_t VARS = 256;
  static constexpr uint32_t MAX_VARS = 8192;
  static constexpr uint32_t CLAUSES = 1024;
  static constexpr uint32_t MAX_CLAUSES = 32768;
  static constexpr uint32_t CANDIDATE_PAIRS = 0;
  static constexpr uint32_t FLIP_ROUNDS = 1;
  static constexpr uint32_t SEED_CAP = 0;
  static constexpr uint64_t ENVIRONMENT_TICKS = 0;
  // Unbounded, as in Kissat; derivations are bounded by env ticks.
  static constexpr uint32_t PROOF_STEPS = 0;
};

struct SweepParameters {
  // force_general_proof selects the captured embedded-solver proof path.
  TACTIX_SWEEP_FIELDS(TACTIX_DECLARE_PROFILE_FIELD)

  constexpr bool valid() const { return true; }
};

} // namespace tactix::strategy
