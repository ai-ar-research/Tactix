#include "policies/factor_stress_policy.hpp"

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

inline uint64_t fs_random(uint64_t &s) {
  s ^= s >> 12;
  s ^= s << 25;
  s ^= s >> 27;
  return s * 0x2545F4914F6CDD1Dull;
}

inline uint64_t fs_hash(uint64_t x) {
  x ^= x >> 33;
  x *= 0xff51afd7ed558ccdull;
  x ^= x >> 33;
  x *= 0xc4ceb9fe1a85ec53ull;
  x ^= x >> 33;
  return x;
}

Lit fs_random_unassigned_literal(Kernel &k, uint64_t &rng) {
  assert(k.unassigned > 0);
  for (;;) {
    const Var v = static_cast<Var>(fs_random(rng) % k.nvars);
    if (k.values[make_lit(v, false)] == 0)
      return make_lit(v, fs_random(rng) & 1);
  }
}

// The adversarial arm. Scores are DETERMINISTIC per (salt, literal) — stable
// within a sweep (the mechanism's stale-heap-key check re-reads them), fresh
// across sweeps (begin_sweep advances the salt) — but UNCORRELATED with the
// watch sizes the built-in path keys on. Three modes drawn from the seed:
//   0 hash   — uniformly scrambled scores (effective random seed order);
//   1 tied   — all scores equal (mass ties: the tie-break never separates,
//              degenerate first-max selection everywhere);
//   2 reverse— UINT32_MAX − watch_size (the exact inverse of the default).
// accept is the N1 reachability floor (delta≥1, always) — the maximizer.
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
  uint64_t rng = seed ? seed : 1;
  const bool trace = getenv("TACTIX_TRACE") != nullptr;
  const int factor_id = tactix_rule_id("Factor");
  const RuleVTable &factor = tactix_rules()[static_cast<size_t>(factor_id)];

  AdversarialFactorMatch fm(seed);
  k.factor_match = &fm; // cleared on every return below (never dangles)

  // S4 Inc-1 budget-boundary stress: a seed-drawn WORK-BUDGET mode drives
  // Factor's two mid-round quit points (kernel.hpp work_budget; design §4.1).
  // Mode 0 leaves the budget unbounded (the pre-budget schedule, byte-
  // identical); modes 1/2 redraw a fresh tiny/medium budget before EVERY
  // apply, so successive root fixpoints cut the round at ever-different
  // boundary positions — before the first seed pop (a few hundred ticks
  // seeds a ≤100-var fuzz CNF) and mid-grow. A budget-cut NoOp just falls
  // through to Decide; the next fixpoint retries with a new budget.
  const uint64_t budget_mode = fs_hash(seed ^ 0xb0d6e7u) % 3;

  auto step = [&](const char *what) {
    if (trace)
      fprintf(stderr,
              "step %-16s level=%u trail=%zu head=%u unassigned=%u "
              "factorings=%llu conflict=%d incons=%d\n",
              what, k.level, k.trail.size(), k.propagate_head, k.unassigned,
              (unsigned long long)k.stats.factorings, (int)k.conflict_valid,
              (int)k.inconsistent);
    kernel_check_invariants(k); // PER-STEP (not every-256): pin the culprit
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
      // TB4b(A): see bve_stress_policy — a reused conflict leaves nothing
      // pending, so Backjump's guard is false.
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

    // Maximize Factor at the root under the adversarial arm (guard = root;
    // TACTIX_FACTOR_FORCE in the fuzz driver bypasses the density NoOp so it
    // fires on small fuzz CNFs). Each Ok apply re-enters off the formula the
    // previous sweep just rewrote — grid-on-grid chains over adversarial
    // seed/tie orders, the schedule DefaultFactorMatch never steers into.
    if (factor.guard(k)) {
      if (budget_mode)
        k.work_budget =
            1 + fs_random(rng) % (budget_mode == 1 ? 4096u : 65536u);
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

    decide_apply(k, fs_random_unassigned_literal(k, rng), true);
    step("decide");
  }
}

} // namespace tactix
