#include "policies/cce_stress_policy.hpp"

#include "rules/analyze/analyze.hpp"
#include "rules/backjump/backjump.hpp"
#include "rules/decide/decide.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/registry.hpp"

#include <cstdio>
#include <cstdlib>

namespace tactix {

static inline uint64_t cc_random(uint64_t &s) {
  s ^= s >> 12;
  s ^= s << 25;
  s ^= s >> 27;
  return s * 0x2545F4914F6CDD1Dull;
}

static Lit cc_random_unassigned_literal(Kernel &k, uint64_t &rng) {
  assert(k.unassigned > 0);
  for (;;) {
    const Var v = static_cast<Var>(cc_random(rng) % k.nvars);
    if (k.values[make_lit(v, false)] == 0)
      return make_lit(v, cc_random(rng) & 1);
  }
}

static uint32_t cc_env_u32(const char *name, uint32_t dflt) {
  const char *s = getenv(name);
  if (!s || !*s)
    return dflt;
  const long v = strtol(s, nullptr, 10);
  return v < 0 ? dflt : static_cast<uint32_t>(v);
}

int cce_stress_solve(Kernel &k, uint64_t seed, volatile sig_atomic_t *stop) {
  KernelStopPublisher stop_published(k, stop);
  uint64_t rng = seed ? seed : 1;
  const bool trace = getenv("TACTIX_TRACE") != nullptr;
  const bool solo = getenv("TACTIX_CCE_STRESS_SOLO") != nullptr;
  const uint32_t max_rounds = cc_env_u32("TACTIX_CCE_STRESS_ROUNDS", 4);
  const uint32_t max_instances = cc_env_u32("TACTIX_CCE_STRESS_INSTANCES", 3);

  const RuleVTable &cce =
      tactix_rules()[static_cast<size_t>(tactix_rule_id("CCE"))];
  const RuleVTable &bce =
      tactix_rules()[static_cast<size_t>(tactix_rule_id("BCE"))];
  const RuleVTable &bve =
      tactix_rules()[static_cast<size_t>(tactix_rule_id("BVE"))];

  auto step = [&](const char *what) {
    if (trace)
      fprintf(stderr,
              "step %-16s level=%u trail=%zu head=%u unassigned=%u rho=%zu "
              "cce_tried=%llu del=%llu blk=%llu ata=%llu ala=%llu cla=%llu "
              "bce_del=%llu conflict=%d incons=%d\n",
              what, k.level, k.trail.size(), k.propagate_head, k.unassigned,
              k.reconstruction.size(), (unsigned long long)k.stats.cce_tried,
              (unsigned long long)k.stats.cce_deleted,
              (unsigned long long)k.stats.cce_blocked,
              (unsigned long long)k.stats.cce_ata,
              (unsigned long long)k.stats.cce_ala,
              (unsigned long long)k.stats.cce_cla,
              (unsigned long long)k.stats.bce_deleted, (int)k.conflict_valid,
              (int)k.inconsistent);
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
      if (backjump_guard(k)) // a reused conflict leaves nothing pending
        backjump_apply(k);
      step("analyze+backjump");
      continue;
    }

    // --- the stressed window: cover every root, to a fixpoint ---------------
    // Deliberately BEFORE the propagate branch, so CCE is entered off a
    // propagation fixpoint (legal — `cce_guard` is `!inconsistent && level ==
    // 0`) and immediately again on the formula its own deletions just changed.
    // `k.work_budget` stays 0, so each apply runs the WHOLE round rather than
    // the 4-per-mille dose: more candidates extended is more witness segments
    // pushed, and a witness is what a wrong SAT answer comes from here.
    if (k.level == 0) {
      bool progressed = false;
      for (uint32_t round = 0; round < max_rounds; round++) {
        if (!cce.guard(k))
          break;
        const ApplyResult r = apply_rule(cce, k);
        if (r == ApplyResult::Unsat)
          return 20;
        if (r != ApplyResult::Ok)
          break; // NoOp: nothing more to eliminate at this root
        step("cce");
        progressed = true;
      }
      // The N1 instance surface, which the round above never touches: the
      // per-literal payload path enters through `inprocess_reach_root`, so it
      // is a different entry as well as a different candidate set.
      for (uint32_t i = 0; i < max_instances && k.level == 0 && k.nvars; i++) {
        if (!cce.guard(k))
          break;
        const Lit l = static_cast<Lit>(cc_random(rng) % (2 * k.nvars));
        const ApplyResult r = apply_rule_instance(cce, k, lit_instance(l));
        if (r == ApplyResult::Unsat)
          return 20;
        if (r == ApplyResult::Ok) {
          step("cce-instance");
          progressed = true;
        }
      }
      // The other two witness-pushing rules, so `ρ` interleaves segment
      // sequences from three sources. CCE is the only one that pushes more
      // than one segment per deletion.
      if (!solo) {
        if (bce.guard(k)) {
          const ApplyResult r = apply_rule(bce, k);
          if (r == ApplyResult::Unsat)
            return 20;
          if (r == ApplyResult::Ok) {
            step("bce");
            progressed = true;
          }
        }
        if (bve.guard(k)) {
          const ApplyResult r = apply_rule(bve, k);
          if (r == ApplyResult::Unsat)
            return 20;
          if (r == ApplyResult::Ok) {
            step("bve");
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

    decide_apply(k, cc_random_unassigned_literal(k, rng), true);
    step("decide");
  }
}

} // namespace tactix
