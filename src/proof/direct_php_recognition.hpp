// Bounded structural recognition for a direct pigeonhole core.  This is a
// read-only frontend for direct_php_certificate.hpp, not a solver action.
#pragma once

#include "proof/direct_php_certificate.hpp"

#include <cstdint>
#include <vector>

namespace tactix::direct_php_recognition {

struct Premise {
  uint64_t occurrence = 0;
  std::vector<int32_t> literals;
  uint64_t native_proof_id = 0;
};

struct Identity {
  uint64_t occurrence = 0;
  uint64_t native_proof_id = 0;
};

struct AloPremise {
  uint32_t pigeon = 0;
  Identity identity;
};

struct AmoPremise {
  uint32_t hole = 0;
  uint32_t first_pigeon = 0;
  uint32_t second_pigeon = 0;
  int32_t first_variable = 0;
  int32_t second_variable = 0;
  Identity identity;
};

struct Census {
  uint64_t alo_clauses = 0;
  uint64_t amo_clauses = 0;
  uint64_t literals = 0;
  // Lossless source records, sorted by stable occurrence identity.
  std::vector<Premise> premises;
};

struct Limits {
  uint64_t maximum_clauses = 0;
  uint64_t maximum_literals = 0;
  uint64_t maximum_work = 0;
  uint64_t maximum_row_candidates = 0;
  uint64_t maximum_row_combinations = 0;
  uint64_t maximum_mapping_trials = 0;
  uint32_t maximum_holes = 0;
};

using Charge = bool (*)(void *context, uint64_t amount);

enum class Status : uint8_t {
  Recognized,
  NotRecognized,
  Malformed,
  Ambiguous,
  LimitExceeded,
  Interrupted,
};

struct Image {
  direct_php_certificate::Matrix variables;
  std::vector<AloPremise> alos;
  std::vector<AmoPremise> amos;
  Census census;
  uint64_t inspected_work = 0;
  uint64_t row_combinations = 0;
  uint64_t mapping_trials = 0;
};

// `source` is a full immutable clause snapshot.  Every clause must have unique
// nonzero occurrence and native proof identities and a canonicalizable literal
// set.  Unrelated clauses are allowed.  Recognition finds exactly one
// H+1-by-H core consisting of positive row ALOs and selected pairwise negative
// column AMOs.  Additional clauses and unused AMO edges do not enter the
// census.  Multiple cores or multiple column mappings are rejected.
//
// The callback is an interruption poll charged in deterministic work units.
// `out` changes only when Recognized is returned.
Status recognize(const std::vector<Premise> &source, const Limits &limits,
                 Charge charge, void *charge_context, Image &out);

} // namespace tactix::direct_php_recognition
