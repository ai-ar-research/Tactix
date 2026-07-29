#include "policies/congruence_stress_policy.hpp"

#include "rules/analyze/analyze.hpp"
#include "rules/backjump/backjump.hpp"
#include "rules/decide/decide.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/registry.hpp"

#include <cstdio>
#include <cstdlib>

namespace tactix {

static inline uint64_t cs_random(uint64_t &s) {
  s ^= s >> 12;
  s ^= s << 25;
  s ^= s >> 27;
  return s * 0x2545F4914F6CDD1Dull;
}

static Lit cs_random_unassigned_literal(Kernel &k, uint64_t &rng) {
  assert(k.unassigned > 0);
  for (;;) {
    const Var v = static_cast<Var>(cs_random(rng) % k.nvars);
    if (k.values[make_lit(v, false)] == 0)
      return make_lit(v, cs_random(rng) & 1);
  }
}

static uint32_t cs_env_u32(const char *name, uint32_t dflt) {
  const char *s = getenv(name);
  if (!s || !*s)
    return dflt;
  const long v = strtol(s, nullptr, 10);
  return v < 0 ? dflt : static_cast<uint32_t>(v);
}

int congruence_stress_solve(Kernel &k, uint64_t seed,
                            volatile sig_atomic_t *stop) {
  uint64_t rng = seed ? seed : 1;
  const bool trace = getenv("TACTIX_TRACE") != nullptr;
  const bool no_subst = getenv("TACTIX_CONG_STRESS_NOSUBST") != nullptr;
  const uint32_t max_rounds = cs_env_u32("TACTIX_CONG_STRESS_ROUNDS", 8);

  const RuleVTable &cong =
      tactix_rules()[static_cast<size_t>(tactix_rule_id("Congruence"))];
  const RuleVTable &subst =
      tactix_rules()[static_cast<size_t>(tactix_rule_id("Substitute"))];

  auto step = [&](const char *what) {
    if (trace)
      fprintf(stderr,
              "step %-16s level=%u trail=%zu head=%u unassigned=%u "
              "cong_rounds=%llu cong_units=%llu conflict=%d incons=%d\n",
              what, k.level, k.trail.size(), k.propagate_head, k.unassigned,
              (unsigned long long)k.stats.congruence_rounds,
              (unsigned long long)k.stats.congruence_units,
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
      backjump_apply(k);
      step("analyze+backjump");
      continue;
    }

    // --- the stressed window: everything legal at the root ------------------
    // Deliberately BEFORE the propagate branch, so congruence is entered off a
    // propagation fixpoint (legal since the D-LEVEL purification — it drains via
    // inprocess_reach_root) and immediately again on the formula its own units
    // and equivalences just changed.
    if (k.level == 0) {
      bool progressed = false;
      for (uint32_t round = 0; round < max_rounds; round++) {
        if (!cong.guard(k))
          break;
        const ApplyResult r = apply_rule(cong, k);
        if (r == ApplyResult::Unsat)
          return 20; // V6 refuted — the path that did not exist before C3
        if (r != ApplyResult::Ok)
          break; // NoOp: fixpoint for this root visit
        step("congruence");
        progressed = true;
        // Collapse the equivalences congruence just added: that is what makes
        // further gates congruent and further XOR keys cancel, so the next round
        // sees genuinely new degeneracies rather than the same ones.
        if (!no_subst && subst.guard(k)) {
          const ApplyResult s = apply_rule(subst, k);
          if (s == ApplyResult::Unsat)
            return 20;
          if (s == ApplyResult::Ok)
            step("substitute");
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

    decide_apply(k, cs_random_unassigned_literal(k, rng), true);
    step("decide");
  }
}

} // namespace tactix
