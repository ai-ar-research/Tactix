#pragma once

#include <cstdint>

namespace tactix::strategy {

// Reference-backed standalone defaults shared by public and default-policy
// Sweep invocations. They are strategy choices, not rule preconditions.
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
  // Unbounded, as Kissat commits whatever its Kitten core proves; a captured
  // derivation is already bounded by the environment's tick limit.
  static constexpr uint32_t PROOF_STEPS = 0;
};

struct SweepParameters {
  uint32_t max_depth = SweepReferenceDefaults::DEPTH;
  uint32_t max_depth_ceiling = SweepReferenceDefaults::MAX_DEPTH;
  uint32_t max_env_vars = SweepReferenceDefaults::VARS;
  uint32_t max_env_vars_ceiling = SweepReferenceDefaults::MAX_VARS;
  uint32_t max_env_clauses = SweepReferenceDefaults::CLAUSES;
  uint32_t max_env_clauses_ceiling = SweepReferenceDefaults::MAX_CLAUSES;
  uint32_t max_candidates = SweepReferenceDefaults::CANDIDATE_PAIRS;
  uint64_t env_ticks = SweepReferenceDefaults::ENVIRONMENT_TICKS;
  uint32_t max_proof_steps = SweepReferenceDefaults::PROOF_STEPS;
  uint32_t max_seeds = SweepReferenceDefaults::SEED_CAP;
  uint64_t round_ticks = 0;
  bool strip_false = true;
  bool measure_closure = false;
  bool full_partition = true;
  bool skip_embedded_fixed = false;
  uint32_t flip_rounds = SweepReferenceDefaults::FLIP_ROUNDS;
  bool resume = true;
  // Select the captured embedded-solver proof path even when a direct or
  // main-formula RUP emission is available.
  bool force_general_proof = false;

  constexpr bool valid() const { return true; }
};

} // namespace tactix::strategy
