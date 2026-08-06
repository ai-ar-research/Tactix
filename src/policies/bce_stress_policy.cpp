#include "policies/bce_stress_policy.hpp"

#include "policies/stress_common.hpp"
#include "rules/analyze/analyze.hpp"
#include "rules/backjump/backjump.hpp"
#include "rules/decide/decide.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/registry.hpp"

#include <cstdio>

namespace tactix {

int bce_stress_solve(Kernel &k, uint64_t seed, volatile sig_atomic_t *stop) {
  KernelStopPublisher stop_published(k, stop);
  uint64_t rng = seed ? seed : 1;
  const bool trace = stress_env_flag("TACTIX_TRACE");
  const bool solo = stress_env_flag("TACTIX_BCE_STRESS_SOLO");
  const uint32_t max_rounds = stress_env_u32("TACTIX_BCE_STRESS_ROUNDS", 4);
  const uint32_t max_instances =
      stress_env_u32("TACTIX_BCE_STRESS_INSTANCES", 4);

  const RuleVTable &bce =
      tactix_rules()[static_cast<size_t>(tactix_rule_id("BCE"))];
  const RuleVTable &bve =
      tactix_rules()[static_cast<size_t>(tactix_rule_id("BVE"))];
  const RuleVTable &cce =
      tactix_rules()[static_cast<size_t>(tactix_rule_id("CCE"))];
  const RuleVTable &subst =
      tactix_rules()[static_cast<size_t>(tactix_rule_id("Substitute"))];

  auto step = [&](const char *what) {
    if (trace)
      fprintf(stderr,
              "step %-16s level=%u trail=%zu head=%u unassigned=%u rho=%zu "
              "bce_rnd=%llu bce_del=%llu cce_del=%llu bve_elim=%llu "
              "subst=%llu conflict=%d incons=%d\n",
              what, k.level, k.trail.size(), k.propagate_head, k.unassigned,
              k.reconstruction.size(),
              (unsigned long long)k.stats.bce_rounds,
              (unsigned long long)k.stats.bce_deleted,
              (unsigned long long)k.stats.cce_deleted,
              (unsigned long long)k.stats.bve_eliminated,
              (unsigned long long)k.stats.substitute_vars,
              (int)k.conflict_valid,
              (int)k.inconsistent);
    kernel_check_invariants(k); // PER-STEP (not every-256): pin the culprit
  };

  // One rule, canonically, reporting whether it changed anything. Only ever
  // called with the guard tested first — that is the N1 contract.
  auto fire = [&](const RuleVTable &vt, const char *what, bool &progressed,
                  bool &unsat) {
    if (!vt.guard(k))
      return;
    const ApplyResult r = apply_rule(vt, k);
    if (r == ApplyResult::Unsat) {
      unsat = true;
      return;
    }
    if (r == ApplyResult::Ok) {
      step(what);
      progressed = true;
    }
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

    if (k.level == 0) {
      bool progressed = false, unsat = false;
      for (uint32_t round = 0; round < max_rounds; round++) {
        if (!bce.guard(k))
          break;
        const ApplyResult r = apply_rule(bce, k);
        if (r == ApplyResult::Unsat)
          return 20;
        if (r != ApplyResult::Ok)
          break; // NoOp: nothing left blocked
        step("bce");
        progressed = true;
      }
      // The instance surface: one blocking literal at a time. Drawn from the
      // whole literal space (apply_instance is total over payloads, and a
      // literal that blocks nothing is a legal NoOp), so the draw does not
      // depend on the rule's own candidate ranking.
      for (uint32_t i = 0; i < max_instances && k.level == 0 && k.nvars; i++) {
        if (!bce.guard(k))
          break;
        const Lit l = static_cast<Lit>(stress_random(rng) % (2 * k.nvars));
        const ApplyResult r = apply_rule_instance(bce, k, lit_instance(l));
        if (r == ApplyResult::Unsat)
          return 20;
        if (r == ApplyResult::Ok) {
          step("bce-instance");
          progressed = true;
        }
      }
      if (!solo) {
        // ρ interleaving: three more witness-pushing rules, in an order that
        // varies with the RNG so BCE's segments land both above and below
        // theirs.
        const uint64_t order = stress_random(rng) % 3;
        const RuleVTable *others[3] = {&bve, &cce, &subst};
        const char *names[3] = {"bve", "cce", "substitute"};
        for (uint32_t i = 0; i < 3; i++) {
          const uint32_t j = static_cast<uint32_t>((order + i) % 3);
          fire(*others[j], names[j], progressed, unsat);
          if (unsat)
            return 20;
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
