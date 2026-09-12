#include "policies/random_policy.hpp"
#include "policies/heuristics.hpp"
#include "policies/stress_common.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "rules/decide/decide.hpp"
#include "rules/registry.hpp"
#include "rules/restart/restart.hpp"
#include "strategy/online_inprocess.hpp"

namespace tactix {

int random_solve(Kernel &k, uint64_t seed, volatile sig_atomic_t *stop,
                 RuleTrace *trace) {
  const char *stress_spec = getenv("TACTIX_STRESS_RULES");
  const bool targeted = stress_spec && *stress_spec;
  const uint32_t normal_every = stress_env_u32("TACTIX_STRESS_NORMAL_EVERY", 8);
  const bool trace_steps = getenv("TACTIX_TRACE") != nullptr;
  // A verdict must be recorded on every exit, or a trace that stops early
  // replays as "reached 0" against a recorded 20 and reports a divergence that
  // is really a missing line. One scope-exit writer covers all four returns.
  struct Finish {
    RuleTrace *t;
    int verdict = 0;
    bool targeted = false;
    uint64_t target_fires = 0;
    ~Finish() {
      if (t)
        t->finish(verdict);
      if (targeted)
        std::printf("c stress-target-fires %llu\n",
                    (unsigned long long)target_fires);
    }
  } fin{trace, 0, targeted, 0};
  KernelStopPublisher stop_published(k, stop);
  // Half of all seeds walk with the dense occurrence index attached:
  // dense-gated tails (bve_backward) are unreachable without it, and a leg
  // that cannot reach a subsystem is vacuous evidence for it
  // (benchmarks/reports/2026-08-08-n2-frozen-pivot.md). Seed-derived, never
  // an env knob, so a recorded trace replays with the same connection state
  // (replay_solve reads the seed back from the trace header).
  OccIndex stress_occ;
  struct DenseScope {
    Kernel &k;
    bool active = false;
    ~DenseScope() {
      if (active)
        kernel_dense_disconnect(k);
    }
  } dense_scope{k};
  if ((seed & 1) != 0 && k.level == 0) {
    // A dense random walk can select BVE at any later step.
    kernel_dense_connect(k, stress_occ, true);
    dense_scope.active = true;
  }
  std::printf("c stress-dense %d\n", dense_scope.active ? 1 : 0);
  uint64_t rng = seed ? seed : 1;
  const auto &rules = tactix_rules();
  const int decide_id = tactix_rule_id("Decide");
  const int restart_id = tactix_rule_id("Restart");
  std::vector<int> applicable;
  applicable.reserve(rules.size());
  std::vector<int> preferred;
  if (targeted) {
    std::string spec(stress_spec);
    for (size_t begin = 0; begin <= spec.size();) {
      const size_t end = spec.find(',', begin);
      const std::string name = spec.substr(begin, end - begin);
      const int id = tactix_rule_id(name.c_str());
      if (id >= 0)
        preferred.push_back(id);
      if (end == std::string::npos)
        break;
      begin = end + 1;
    }
  }

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
        kernel_propagation_fixpoint(k))
      return fin.verdict = 10;
    if (stop && *stop && (steps & 1023) == 0)
      return fin.verdict = 0;

    applicable.clear();
    for (size_t i = 0; i < rules.size(); i++)
      if (rules[i].guard(k))
        applicable.push_back(static_cast<int>(i));
    assert(!applicable.empty()); // some rule is always applicable pre-verdict

    int id = -1;
    if (targeted && normal_every && (steps % normal_every) != 0) {
      for (int candidate : preferred)
        if (rules[candidate].guard(k)) {
          id = candidate;
          fin.target_fires++;
          break;
        }
    }
    if (id < 0)
      id = applicable[stress_random(rng) % applicable.size()];
    // Named BEFORE the apply: a rule that aborts inside its own body (an
    // invariant or proof assert) is otherwise the one step the trace never
    // attributes, since the result line below is never reached.
    if (trace_steps)
      fprintf(stderr, "step %-16s -> ...\n", rules[id].name);
    ApplyResult r;
    // The payload this step applied, in the rule's own encoding, recorded so
    // the step replays through apply_instance (trace.hpp). Decide and Restart
    // reach the same effect via a direct call below, so their payload has to be
    // spelled out rather than read back.
    uint64_t applied = INSTANCE_CANONICAL;
    bool applied_semantic = false;
    if (id == decide_id) {
      const Lit l = stress_random_unassigned_literal(k, rng);
      applied = lit_instance(k, l);
      r = decide_apply(k, l, true);
    } else if (id == restart_id) {
      // Random keep-level: every level in [0, current] is a legal instance
      // (plan §8.2 fuzzes instance choices, not only rule order).
      applied = stress_random(rng) % (k.level + 1);
      if (applied < k.level)
        heuristics_update_target_phases(k);
      r = restart_apply_to(k, static_cast<uint32_t>(applied));
    } else if ((rules[id].semantic_instances &&
                rules[id].apply_semantic_instance) ||
               (rules[id].instances && rules[id].apply_instance)) {
      // Enumerate the complete current semantic frontier. The former fixed
      // array made every candidate after ordinal 15 unreachable regardless of
      // seed. Decide remains the uniform O(1) special case above; other rules
      // draw from the full formula-dependent space. Canonical closure remains
      // a one-in-three choice because its grouped proof/state transition is
      // independently legal and must stay fuzzed.
      std::vector<uint64_t> instances;
      const strategy::InvocationStatus enumerated =
          strategy::enumerate_complete_instances(k, rules[id], instances);
      if (enumerated != strategy::InvocationStatus::Completed)
        return fin.verdict = 0;
      const bool canonical =
          instances.empty() || stress_random(rng) % 3 == 0;
      if (canonical) {
        r = apply_rule(rules[id], k);
      } else {
        applied = instances[stress_random(rng) % instances.size()];
        applied_semantic = rules[id].semantic_instances != nullptr;
        r = applied_semantic
                ? apply_rule_semantic_instance(rules[id], k, applied)
                : apply_rule_instance(rules[id], k, applied);
      }
    } else {
      r = apply_rule(rules[id], k);
    }
    heuristics_consume_rule_advice(k);
    heuristics_consume_retirements(k);
    if (trace)
      trace->record(id, applied, applied_semantic);
    // Per-step rule trace, env-gated: it names rule, result and state at every
    // step, where the every-256 invariant check only reports that state was
    // corrupted. Printed before the invariant oracle so an abort inside the
    // oracle still has this step's result line. See the stress-policy skill.
    if (trace_steps) {
      static const char *rn[] = {"Ok", "NoOp", "Conflict", "Sat", "Unsat"};
      fprintf(stderr,
              "step %-16s -> %-8s level=%u trail=%zu head=%u unassigned=%u "
              "conflict=%d incons=%d\n",
              rules[id].name, rn[(int)r], k.level, k.trail.size(),
              k.propagate_head, k.unassigned, (int)k.conflict_valid,
              (int)k.inconsistent);
    }
    if (targeted)
      kernel_check_invariants(k);
    if (r == ApplyResult::Unsat)
      return fin.verdict = 20;
    if (r == ApplyResult::Sat)
      return fin.verdict = 10;
  }
}

int stress_solve(Kernel &k, uint64_t seed, volatile sig_atomic_t *stop) {
  return random_solve(k, seed, stop, nullptr);
}

} // namespace tactix
