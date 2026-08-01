#include "policies/probing_stress_policy.hpp"

#include "rules/analyze/analyze.hpp"
#include "rules/backjump/backjump.hpp"
#include "rules/decide/decide.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/registry.hpp"

#include <cstdio>
#include <cstdlib>

namespace tactix {

namespace {

inline uint64_t ps_random(uint64_t &s) {
  s ^= s >> 12;
  s ^= s << 25;
  s ^= s >> 27;
  return s * 0x2545F4914F6CDD1Dull;
}

Lit ps_random_unassigned_literal(Kernel &k, uint64_t &rng) {
  assert(k.unassigned > 0);
  for (;;) {
    const Var v = static_cast<Var>(ps_random(rng) % k.nvars);
    if (k.values[make_lit(v, false)] == 0)
      return make_lit(v, ps_random(rng) & 1);
  }
}

} // namespace

int probing_stress_solve(Kernel &k, uint64_t seed, volatile sig_atomic_t *stop) {
  KernelStopPublisher stop_published(k, stop);
  uint64_t rng = seed ? seed : 1;
  const bool trace = getenv("TACTIX_TRACE") != nullptr;
  // The kernel_probing scope consumers (P2's stress surface).
  static const char *const NAMES[] = {"Probe", "Backbone", "Vivify",
                                      "TransitiveReduce", "Congruence"};
  constexpr int NRULES = 5;
  const RuleVTable *probers[NRULES];
  for (int i = 0; i < NRULES; i++)
    probers[i] = &tactix_rules()[static_cast<size_t>(tactix_rule_id(NAMES[i]))];

  auto step = [&](const char *what) {
    if (trace)
      fprintf(stderr,
              "step %-16s level=%u trail=%zu head=%u unassigned=%u "
              "conflict=%d incons=%d\n",
              what, k.level, k.trail.size(), k.propagate_head, k.unassigned,
              (int)k.conflict_valid, (int)k.inconsistent);
    kernel_check_invariants(k); // PER-STEP (not every-256): pin the culprit
  };

  for (uint64_t steps = 0;; steps++) {
    if (stop && *stop && (steps & 1023) == 0)
      return 0;
    if (k.inconsistent)
      return 20;

    if (k.conflict_valid) {
      if (analyze_apply(k) == ApplyResult::Unsat)
        return 20;
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

    if (k.unassigned == 0)
      return 10; // total + fixpoint = SAT

    // At every root fixpoint, run the probing family in a fresh random order
    // — dense open/close cycles of the P2 scope, immediately followed by
    // search that reuses the restored watch lists. Each rule only when its
    // guard holds (the N1 contract).
    if (k.level == 0) {
      int order[NRULES] = {0, 1, 2, 3, 4};
      for (int i = NRULES - 1; i > 0; i--) {
        const int j = static_cast<int>(ps_random(rng) % (i + 1));
        const int t = order[i];
        order[i] = order[j];
        order[j] = t;
      }
      bool progressed = false;
      for (int i = 0; i < NRULES && !k.inconsistent; i++) {
        const RuleVTable &r = *probers[order[i]];
        if (!r.guard(k))
          continue;
        const ApplyResult ar = apply_rule(r, k);
        if (ar == ApplyResult::Unsat)
          return 20;
        step(r.name);
        if (ar == ApplyResult::Ok)
          progressed = true;
        if (k.conflict_valid || k.propagate_head < k.trail.size())
          break; // let the search loop drain what the rule asserted
      }
      if (progressed || k.conflict_valid ||
          k.propagate_head < k.trail.size())
        continue; // re-enter off the just-changed state (the L3/P2 pattern)
    }

    decide_apply(k, ps_random_unassigned_literal(k, rng), true);
    step("decide");
  }
}

} // namespace tactix
