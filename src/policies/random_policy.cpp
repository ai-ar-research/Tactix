#include "policies/random_policy.hpp"
#include "policies/stress_common.hpp"

#include <cstdio>
#include <cstdlib>

#include "rules/decide/decide.hpp"
#include "rules/registry.hpp"
#include "rules/restart/restart.hpp"

namespace tactix {

int random_solve(Kernel &k, uint64_t seed, volatile sig_atomic_t *stop,
                 RuleTrace *trace) {
  // A verdict must be recorded on every exit, or a trace that stops early
  // replays as "reached 0" against a recorded 20 and reports a divergence that
  // is really a missing line. One scope-exit writer covers all four returns.
  struct Finish {
    RuleTrace *t;
    int verdict = 0;
    ~Finish() {
      if (t)
        t->finish(verdict);
    }
  } fin{trace};
  KernelStopPublisher stop_published(k, stop);
  uint64_t rng = seed ? seed : 1;
  const auto &rules = tactix_rules();
  const int decide_id = tactix_rule_id("Decide");
  const int restart_id = tactix_rule_id("Restart");
  std::vector<int> applicable;
  applicable.reserve(rules.size());

  for (uint64_t steps = 0;; steps++) {
    // Periodic invariant oracle (plan §8.2 oracle 4): most random walks never
    // reach a verdict, so without it a state-corrupting logic bug is invisible.
    // Every 256 steps, not per step: the check is O(vars + arena + watches) and
    // would dominate the walk. No-op in NDEBUG builds.
    if ((steps & 255) == 0)
      kernel_check_invariants(k);
    if (k.inconsistent)
      return fin.verdict = 20;
    // SAT only when the assignment is total, fully propagated, and no conflict
    // is in flight: an unpropagated suffix or a recorded conflict can each mean
    // the assignment still falsifies a clause. No `pending` check is needed —
    // ConflictAnalysis is atomic, so no learned clause crosses this boundary.
    if (k.unassigned == 0 && !k.conflict_valid &&
        k.propagate_head == k.trail.size())
      return fin.verdict = 10;
    if (stop && *stop && (steps & 1023) == 0)
      return fin.verdict = 0;

    applicable.clear();
    for (size_t i = 0; i < rules.size(); i++)
      if (rules[i].guard(k))
        applicable.push_back(static_cast<int>(i));
    assert(!applicable.empty()); // some rule is always applicable pre-verdict

    const int id =
        applicable[stress_random(rng) % applicable.size()];
    ApplyResult r;
    // The payload this step applied, in the rule's own encoding, recorded so
    // the step replays through apply_instance (trace.hpp). Decide and Restart
    // reach the same effect via a direct call below, so their payload has to be
    // spelled out rather than read back.
    uint64_t applied = INSTANCE_CANONICAL;
    if (id == decide_id) {
      const Lit l = stress_random_unassigned_literal(k, rng);
      applied = lit_instance(l);
      r = decide_apply(k, l, true);
    } else if (id == restart_id) {
      // Random keep-level: every level in [0, current] is a legal instance
      // (plan §8.2 fuzzes instance choices, not only rule order).
      applied = stress_random(rng) % (k.level + 1);
      r = restart_apply_to(k, static_cast<uint32_t>(applied));
    } else if (rules[id].instances && rules[id].apply_instance) {
      // Generic instance draw over the rule's own enumeration, so any rule
      // registering an instance surface gets instance fuzzing for free.
      // INSTANCE_CANONICAL is drawn about a third of the time (always when the
      // enumeration is empty) because a canonical round emits proof deltas the
      // single instances never emit together — Probe's multi-unit batch,
      // Strengthen's flush pre-pass plus cascade, Vivify's full round, Lucky's
      // cascade — and the proof strategy must hold under every legal schedule.
      uint64_t insts[16];
      const uint32_t m = rules[id].instances(k, insts, 16);
      const uint64_t inst = (m == 0 || stress_random(rng) % 3 == 0)
                                ? INSTANCE_CANONICAL
                                : insts[stress_random(rng) % m];
      applied = inst;
      r = apply_rule_instance(rules[id], k, inst);
    } else {
      r = apply_rule(rules[id], k);
    }
    if (trace)
      trace->record(id, applied);
    // Per-step rule trace, env-gated: it names rule, result and state at every
    // step, where the every-256 invariant check only reports that state was
    // corrupted. See the stress-policy skill.
    if (getenv("TACTIX_TRACE")) {
      static const char *rn[] = {"Ok", "NoOp", "Conflict", "Sat", "Unsat"};
      fprintf(stderr,
              "step %-16s -> %-8s level=%u trail=%zu head=%u unassigned=%u "
              "conflict=%d incons=%d\n",
              rules[id].name, rn[(int)r], k.level, k.trail.size(),
              k.propagate_head, k.unassigned, (int)k.conflict_valid,
              (int)k.inconsistent);
    }
    if (r == ApplyResult::Unsat)
      return fin.verdict = 20;
    if (r == ApplyResult::Sat)
      return fin.verdict = 10;
  }
}

} // namespace tactix
