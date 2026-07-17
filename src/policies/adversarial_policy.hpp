// AdversarialPolicy (D-LEVEL-1 stress harness): a Policy instantiation that
// aggressively DECIDES without propagating to a fixpoint in between — the exact
// N1 schedule DefaultPolicy never generates. Its purpose is to reproduce and
// then guard against the "off-fixpoint decide" escaped-unit family (calculus
// D-LEVEL-1). Like RandomPolicy it drives the solver only through sound rule
// applications, so soundness + proof validity must survive its schedule.
//
// Knobs (env, so the fuzz driver can sweep without rebuilding):
//   TACTIX_ADV_BURST=N   max consecutive decides before a propagate (default 4)
//   TACTIX_ADV_PROP=P    percent chance to propagate when work remains (else
//                        decide); 0 disables mid-burst propagation (default 15)
//   TACTIX_TRACE=1       per-step rule trace + per-step invariant check to stderr
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
