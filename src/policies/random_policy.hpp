// RandomPolicy (plan §8.2 rule-order fuzzing): drives the solver purely through
// the rule registry, choosing uniformly among the rules whose soundness guard
// holds with no scheduling heuristics, and for Decide branching on a uniformly
// random unassigned literal with a random phase — the full theoretical instance
// space, not a heuristic ranking. Soundness and proof validity must survive
// every schedule it generates; that is N1.
#pragma once

#include "policies/policy.hpp"
#include "policies/trace.hpp"

namespace tactix {

// returns 10 (SAT) / 20 (UNSAT) / 0 (interrupted); deterministic per seed.
// `trace`, when non-null, records the (rule, instance) sequence for
// replay_solve (trace.hpp) — this policy is traceable BECAUSE every action it
// takes is one registry application with one payload.
int random_solve(Kernel &, uint64_t seed,
                 volatile sig_atomic_t *stop = nullptr,
                 RuleTrace *trace = nullptr);

class RandomPolicy final : public Policy {
public:
  explicit RandomPolicy(uint64_t seed) : seed_(seed) {}
  int solve(Kernel &k, volatile sig_atomic_t *stop = nullptr) override {
    return random_solve(k, seed_, stop);
  }

private:
  uint64_t seed_;
};

} // namespace tactix
