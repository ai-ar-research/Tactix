#pragma once

#include "kernel/types.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace tactix {

struct Kernel;

// Indexed by the smaller signed operand. The watched binaries authenticate a
// substitution. Compaction forwards surviving internal identities.
struct ConflictEr {
  struct Definition {
    Lit right, output;
  };
  struct Candidate {
    Lit left, right, output;
    uint32_t left_position, right_position;
    double score;
  };
  std::unordered_map<Lit, std::vector<Definition>> definitions;
  std::vector<Candidate> candidates;
  std::vector<Lit> previous, older_heads;
  std::vector<uint32_t> positions;
  std::vector<Lit> rewritten;
  // Native automatic BVE can defer active definitions. Released outputs
  // remain mapped until their binaries disappear, so they cannot duplicate.
  std::vector<uint8_t> retained;
  uint64_t last_learned = 0, last_restart = 0, last_reduction = 0;
  uint64_t last_retirement = 0;
};

// Plan a single GlucosER-style pass over the nonasserting literals. The plan
// is empty when no supported substitution preserves the asserting clause.
const std::vector<Lit> &conflict_er_rewrite(Kernel &, const std::vector<Lit> &);
void conflict_er_observe(Kernel &, const std::vector<Lit> &);
void conflict_er_compact(ConflictEr &, const std::vector<Var> &);
void conflict_er_release_cold_outputs(Kernel &);
inline bool conflict_er_retained(const ConflictEr &er, Var output) {
  return output < er.retained.size() && er.retained[output];
}

} // namespace tactix
