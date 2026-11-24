#pragma once

#include "kernel/types.hpp"

#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tactix {

struct Kernel;

// Indexed by the smaller signed operand. The watched binaries authenticate a
// substitution. Compaction forwards surviving internal identities.
struct ConflictEr {
  struct Definition {
    Lit right, output;
  };
  struct LearnedOrigin {
    Ref ref = INVALID_REF;
    uint64_t id = 0;
    Lit head = INVALID_LIT;
  };
  struct LearnedPair {
    LearnedOrigin first, second;
  };
  struct Candidate {
    Lit left, right, output;
    uint32_t left_position, right_position;
    double score;
  };
  std::unordered_map<Lit, std::vector<Definition>> definitions;
  // Retains only the applied substitutions after a rewrite.
  std::vector<Candidate> candidates;
  std::vector<Lit> previous, older_heads;
  std::vector<uint32_t> positions;
  std::vector<Lit> rewritten;
  // Native automatic BVE can defer active definitions. Released outputs
  // remain mapped until their binaries disappear, so they cannot duplicate.
  std::vector<uint8_t> retained;
  std::vector<uint64_t> use_epoch;
  // Stable names, in publication order, for the focused random picker.
  std::vector<Var> automatic_outputs_external;
  // Signed external literals survive arena collection and variable compaction.
  std::vector<std::pair<int, int>> learned_pair_heads;
  uint64_t previous_generation = 0;
  bool previous_has_reference = false;
  bool factor_learned = false;
  bool observed_lbd_gain = false;
  uint64_t last_learned = 0, last_restart = 0, last_reduction = 0;
  uint64_t last_retirement = 0;
};

// Consume the current phase's discovery, including on an early exit.
class DeferredConflictErBatch {
public:
  explicit DeferredConflictErBatch(Kernel &);
  ~DeferredConflictErBatch();
  DeferredConflictErBatch(const DeferredConflictErBatch &) = delete;
  DeferredConflictErBatch &operator=(const DeferredConflictErBatch &) = delete;
  explicit operator bool() const { return er_ && !er_->learned_pair_heads.empty(); }

private:
  ConflictEr *er_ = nullptr;
};

// Plan a single GlucosER-style pass over the nonasserting literals. The plan
// is empty when no supported substitution preserves the asserting clause.
const std::vector<Lit> &conflict_er_rewrite(Kernel &, const std::vector<Lit> &);
void conflict_er_refresh_applied_outputs(Kernel &);
void conflict_er_observe(Kernel &, const std::vector<Lit> &, bool factor_learned = false);
// Report whether the root batch passed its entry checks.
bool conflict_er_factor_learned_pairs(Kernel &, bool consume_seeds = true);
// Explicit root action, independent of the automatic policy's storage test.
ConflictEr::LearnedPair conflict_er_find_learned_pair(
    const Kernel &, Lit first_head, Lit second_head, std::vector<Lit> tail);
Lit conflict_er_factor_learned_pair(Kernel &, const ConflictEr::LearnedPair &);
void conflict_er_compact(ConflictEr &, const std::vector<Var> &);
void conflict_er_release_cold_outputs(Kernel &);
inline bool conflict_er_retained(const ConflictEr &er, Var output) {
  return output < er.retained.size() && er.retained[output];
}

} // namespace tactix
