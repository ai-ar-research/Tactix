#include "policies/substitute_stress_policy.hpp"

#include "policies/stress_common.hpp"
#include "rules/analyze/analyze.hpp"
#include "rules/backjump/backjump.hpp"
#include "rules/decide/decide.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/registry.hpp"

#include <cstdio>

namespace tactix {

int substitute_stress_solve(Kernel &k, uint64_t seed,
                            volatile sig_atomic_t *stop) {
  KernelStopPublisher stop_published(k, stop);
  uint64_t rng = seed ? seed : 1;
  const bool trace = stress_env_flag("TACTIX_TRACE");
  const bool solo = stress_env_flag("TACTIX_SUBSTITUTE_STRESS_SOLO");
  const uint32_t max_rounds =
      stress_env_u32("TACTIX_SUBSTITUTE_STRESS_ROUNDS", 4);
  const uint32_t max_instances =
      stress_env_u32("TACTIX_SUBSTITUTE_STRESS_INSTANCES", 4);

  const RuleVTable &subst =
      tactix_rules()[static_cast<size_t>(tactix_rule_id("Substitute"))];
  // The rules that rewrite the binary implication graph Substitute reads
  // (Congruence and Sweep add equivalence binaries, TransitiveReduce deletes
  // binaries), plus the other two witness pushers so ρ interleaves.
  const char *partner_names[5] = {"Congruence", "Sweep", "TransitiveReduce",
                                  "BCE", "BVE"};
  const RuleVTable *partners[5];
  for (uint32_t i = 0; i < 5; i++)
    partners[i] =
        &tactix_rules()[static_cast<size_t>(tactix_rule_id(partner_names[i]))];

  auto step = [&](const char *what) {
    if (trace)
      fprintf(stderr,
              "step %-16s level=%u trail=%zu head=%u unassigned=%u rho=%zu "
              "subst_rnd=%llu sccs=%llu vars=%llu cong=%llu sweep_eq=%llu "
              "conflict=%d incons=%d\n",
              what, k.level, k.trail.size(), k.propagate_head, k.unassigned,
              k.reconstruction.size(),
              (unsigned long long)k.stats.substitute_rounds,
              (unsigned long long)k.stats.substitute_sccs,
              (unsigned long long)k.stats.substitute_vars,
              (unsigned long long)k.stats.congruence_merges,
              (unsigned long long)k.stats.sweep_equivalences,
              (int)k.conflict_valid, (int)k.inconsistent);
    kernel_check_invariants(k); // per step, not every-256: pin the culprit
  };

  for (uint64_t steps = 0;; steps++) {
    if (stop && *stop && (steps & 1023) == 0)
      return 0;
    if (k.inconsistent)
      return 20;

    if (k.conflict_valid) {
      if (analyze_apply(k) == ApplyResult::Unsat)
        return 20;
      if (backjump_guard(k)) // a reused conflict leaves nothing pending
        backjump_apply(k);
      step("analyze+backjump");
      continue;
    }

    // The stressed window sits before the propagate branch on purpose, so
    // Substitute is entered off a propagation fixpoint and again on the graph
    // its own rewrite just changed.
    if (k.level == 0) {
      bool progressed = false;

      // (a) the per-SCC instance surface, from the rule's own enumeration and
      //     in random order. Applying one representative can dissolve another,
      //     so later payloads in the batch are deliberately stale.
      uint64_t insts[16];
      const uint32_t n =
          (subst.instances && subst.guard(k))
              ? subst.instances(k, insts, max_instances < 16 ? max_instances
                                                             : 16)
              : 0;
      for (uint32_t i = 0; i < n; i++) {
        const uint32_t j = i + static_cast<uint32_t>(stress_random(rng) %
                                                     (n - i));
        const uint64_t pick = insts[j];
        insts[j] = insts[i];
        insts[i] = pick;
        if (!subst.guard(k))
          break;
        const ApplyResult r = apply_rule_instance(subst, k, pick);
        if (r == ApplyResult::Unsat)
          return 20;
        if (r == ApplyResult::Ok) {
          step("substitute-instance");
          progressed = true;
        }
      }

      // (b) canonical rounds to a fixpoint: every remaining non-trivial SCC.
      for (uint32_t round = 0; round < max_rounds; round++) {
        if (!subst.guard(k))
          break;
        const ApplyResult r = apply_rule(subst, k);
        if (r == ApplyResult::Unsat)
          return 20;
        if (r != ApplyResult::Ok)
          break; // NoOp: no non-trivial SCC left
        step("substitute");
        progressed = true;
      }

      // (c) the partners, rotated, so the graph Substitute reads next round has
      //     been both grown and shrunk, and ρ carries their segments between
      //     Substitute's witness pairs.
      if (!solo) {
        const uint64_t start = stress_random(rng) % 5;
        for (uint32_t i = 0; i < 5; i++) {
          const uint32_t j = static_cast<uint32_t>((start + i) % 5);
          if (!partners[j]->guard(k))
            continue;
          const ApplyResult r = apply_rule(*partners[j], k);
          if (r == ApplyResult::Unsat)
            return 20;
          if (r == ApplyResult::Ok) {
            step(partner_names[j]);
            progressed = true;
          }
        }
      }
      if (progressed)
        continue;
    }

    if (k.propagate_head < k.trail.size()) {
      propagate_apply(k);
      step("propagate");
      continue;
    }

    if (k.unassigned == 0)
      return 10; // total + fixpoint = SAT

    decide_apply(k, stress_random_unassigned_literal(k, rng), true);
    step("decide");
  }
}

} // namespace tactix
