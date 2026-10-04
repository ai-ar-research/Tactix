#pragma once

#include "rules/registry.hpp"

namespace tactix {

// Internal first increment for the SGEN theory-to-Boolean replacement.  The
// caller chooses a recognized block; policy registration and scheduling are a
// separate integration step.
struct ExactCoverDefinitionRequest {
  uint64_t maximum_clauses = 0;
  uint64_t maximum_literals = 0;
  uint64_t recognition_work = 0;
  uint64_t maximum_items = 0;
  uint32_t block_arity = 0;
  uint32_t block = 0;
  uint32_t maximum_external_extent = 0;
  uint64_t maximum_additions = 0;
  uint64_t action_id = 0;
  uint64_t semantic_id = 0;
  FILE *records = nullptr;
};

struct ExactCoverDefinitionResult {
  ApplyResult result = ApplyResult::NoOp;
  uint64_t recognition_work = 0;
  uint32_t blocks = 0;
  uint32_t covers = 0;
  uint64_t items = 0;
  int32_t output = 0;
  uint64_t additions = 0;
};

// Define a fresh live literal exactly equivalent to occupancy of one certified
// exact-cover block.  Failure, interruption, and exhausted caller limits leave
// the kernel and record stream untouched.  The fresh output is omitted when an
// original model is projected.
ExactCoverDefinitionResult define_exact_cover_block_occupancy(
    Kernel &, const ExactCoverDefinitionRequest &);

} // namespace tactix
