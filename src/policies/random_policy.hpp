// RandomPolicy (plan §8.2 rule-order fuzzing): generates broad typed programs
// and drives them through the same OnlineStrategyExecutor exposed to callers.
// Soundness and proof validity must survive every schedule it generates.
#pragma once

#include "policies/policy.hpp"
#include "policies/trace.hpp"
#include "strategy/random_online.hpp"

namespace tactix {

// returns 10 (SAT) / 20 (UNSAT) / 0 (interrupted); deterministic per seed.
// `trace`, when non-null, records every complete typed program for replay_solve.
int random_solve(Kernel &, uint64_t seed,
                 volatile sig_atomic_t *stop = nullptr,
                 RuleTrace *trace = nullptr,
                 const strategy::RandomOnlineConfig *config = nullptr);

// Generic targeted scheduler. Configuration is supplied by the validated fuzz
// scenario launcher through TACTIX_STRESS_RULES and TACTIX_STRESS_NORMAL_EVERY.
int stress_solve(Kernel &, uint64_t seed,
                 volatile sig_atomic_t *stop = nullptr);

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
