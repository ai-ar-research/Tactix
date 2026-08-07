#include "policies/factor_stress_policy.hpp"
#include "policies/stress_common.hpp"

#include "policies/subpolicy.hpp"
#include "rules/analyze/analyze.hpp"
#include "rules/backjump/backjump.hpp"
#include "rules/decide/decide.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/registry.hpp"

#include <cstdio>
#include <cstdlib>

namespace tactix {

namespace {

inline uint64_t fs_hash(uint64_t x) {
  x ^= x >> 33;
  x *= 0xff51afd7ed558ccdull;
  x ^= x >> 33;
  x *= 0xc4ceb9fe1a85ec53ull;
  x ^= x >> 33;
  return x;
}

// The adversarial arm. Scores are deterministic per (salt, literal) — stable
// within a sweep, since the stale-heap-key check re-reads them, and fresh
// across sweeps via begin_sweep — but uncorrelated with the watch sizes the
// built-in path keys on. Three modes drawn from the seed: 0 scrambles scores
// by hash; 1 ties them all, so the tie-break never separates and first-max
// selection is degenerate everywhere; 2 reverses the default (0xffffff minus
// watch_size). `accept` is the N1 reachability floor, delta >= 1 always.
class AdversarialFactorMatch final : public FactorMatchPolicy {
public:
  explicit AdversarialFactorMatch(uint64_t seed)
      : salt_(fs_hash(seed | 1)), mode_(seed % 3) {}
  const char *name() const override { return "adversarial-stress"; }
  void begin_sweep(const Kernel &) override { salt_ = fs_hash(salt_ + 1); }
  double seed_score(const Kernel &k, Lit s) override { return score(k, s); }
  double next_factor_score(const Kernel &k, Lit c, uint32_t, Lit) override {
    return score(k, c);
  }
  bool accept(uint32_t, uint32_t, int delta) override { return delta >= 1; }

private:
  double score(const Kernel &k, Lit l) const {
    switch (mode_) {
    case 0:
      return static_cast<double>(fs_hash(salt_ ^ l) & 0xffffff);
    case 1:
      return 1.0;
    default:
      return static_cast<double>(0xffffffu - k.watches.heads[l].size);
    }
  }
  uint64_t salt_;
  uint64_t mode_;
};

} // namespace

int factor_stress_solve(Kernel &k, uint64_t seed, volatile sig_atomic_t *stop) {
  KernelStopPublisher stop_published(k, stop);
  uint64_t rng = seed ? seed : 1;
  const bool trace = getenv("TACTIX_TRACE") != nullptr;
  const int factor_id = tactix_rule_id("Factor");
  const RuleVTable &factor = tactix_rules()[static_cast<size_t>(factor_id)];

  AdversarialFactorMatch fm(seed);
  k.factor_match = &fm; // cleared on every return below (never dangles)

  // Budget-boundary stress: a seed-drawn mode drives Factor's two mid-round
  // quit points (`Kernel::work_budget`). Mode 0 leaves the budget unbounded;
  // modes 1 and 2 redraw a fresh tiny/medium budget before every apply, so
  // successive root fixpoints cut the round at different boundary positions,
  // before the first seed pop and mid-grow. A budget-cut NoOp falls through to
  // Decide and the next fixpoint retries with a new budget.
  const uint64_t budget_mode = fs_hash(seed ^ 0xb0d6e7u) % 3;

  auto step = [&](const char *what) {
    if (trace)
      fprintf(stderr,
              "step %-16s level=%u trail=%zu head=%u unassigned=%u "
              "factorings=%llu conflict=%d incons=%d\n",
              what, k.level, k.trail.size(), k.propagate_head, k.unassigned,
              (unsigned long long)k.stats.factorings, (int)k.conflict_valid,
              (int)k.inconsistent);
    kernel_check_invariants(k); // per step, not every-256: pin the culprit
  };
  auto done = [&](int res) {
    k.factor_match = nullptr;
    k.work_budget = 0;
    return res;
  };

  for (uint64_t steps = 0;; steps++) {
    if (stop && *stop && (steps & 1023) == 0)
      return done(0);
    if (k.inconsistent)
      return done(20);

    if (k.conflict_valid) {
      if (analyze_apply(k) == ApplyResult::Unsat)
        return done(20);
      // A reused conflict leaves nothing pending, so Backjump's guard is false.
      if (backjump_guard(k))
        backjump_apply(k);
      step("analyze+backjump");
      continue;
    }

    if (k.propagate_head < k.trail.size()) {
      propagate_apply(k);
      step("propagate");
      continue;
    }

    // fixpoint, no conflict.
    if (k.unassigned == 0)
      return done(10); // total + fixpoint = SAT

    // Maximize Factor at the root under the adversarial arm; the fuzz driver's
    // TACTIX_FACTOR_FORCE bypasses the density NoOp so it fires on small CNFs.
    // Each Ok apply re-enters off the formula the previous sweep rewrote:
    // grid-on-grid chains DefaultFactorMatch never steers into.
    if (factor.guard(k)) {
      if (budget_mode)
        k.work_budget =
            1 + stress_random(rng) % (budget_mode == 1 ? 4096u : 65536u);
      const ApplyResult r = apply_rule(factor, k);
      k.work_budget = 0;
      if (r == ApplyResult::Unsat)
        return done(20);
      if (r == ApplyResult::Ok) {
        step("factor");
        continue;
      }
      // NoOp: no positive-gain grid left this round.
    }

    decide_apply(k, stress_random_unassigned_literal(k, rng), true);
    step("decide");
  }
}

} // namespace tactix
