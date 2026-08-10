#pragma once

#include <cstdint>

namespace tactix {

// Reference-backed standalone defaults shared by the public Sweep rule and
// DefaultPolicy's typed strategy profile.  Keeping the literals here prevents
// the rule fallback and shipped policy from silently drifting apart.
struct SweepReferenceDefaults {
  static constexpr uint32_t DEPTH = 2;
  static constexpr uint32_t MAX_DEPTH = 3;
  static constexpr uint32_t VARS = 256;
  static constexpr uint32_t MAX_VARS = 8192;
  static constexpr uint32_t CLAUSES = 1024;
  static constexpr uint32_t MAX_CLAUSES = 32768;
  static constexpr uint32_t CANDIDATE_PAIRS = 32;
  static constexpr uint32_t FLIP_ROUNDS = 1;
  static constexpr uint32_t SEED_CAP = 0;
  static constexpr uint64_t ENVIRONMENT_TICKS = 200000;
};

} // namespace tactix
