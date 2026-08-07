// AdversarialPolicy: the off-fixpoint-decide stress harness, and the worked
// example the other stress policies follow (contract in stress_registry.hpp).
// It decides without propagating to a fixpoint in between, guarding the
// escaped-unit family (calculus D-LEVEL-1). Knobs: TACTIX_ADV_BURST (max
// consecutive decides before a propagate) and TACTIX_ADV_PROP (percent chance
// to propagate when work remains; 0 disables mid-burst propagation).
#pragma once

#include "policies/policy.hpp"

namespace tactix {

// returns 10 (SAT) / 20 (UNSAT) / 0 (interrupted); deterministic per seed+env
int adversarial_solve(Kernel &, uint64_t seed,
                      volatile sig_atomic_t *stop = nullptr);

class AdversarialPolicy final : public Policy {
public:
  explicit AdversarialPolicy(uint64_t seed) : seed_(seed) {}
  int solve(Kernel &k, volatile sig_atomic_t *stop = nullptr) override {
    return adversarial_solve(k, seed_, stop);
  }

private:
  uint64_t seed_;
};

} // namespace tactix
