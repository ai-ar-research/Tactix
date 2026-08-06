#include "policies/random_policy.hpp"

#include <cstdio>
#include <cstdlib>

#include "rules/decide/decide.hpp"
#include "rules/registry.hpp"
#include "rules/restart/restart.hpp"

namespace tactix {

// xorshift64* — tiny, deterministic, no libc rand state
static inline uint64_t next_random(uint64_t &s) {
  s ^= s >> 12;
  s ^= s << 25;
  s ^= s >> 27;
  return s * 0x2545F4914F6CDD1Dull;
}

static Lit random_unassigned_literal(Kernel &k, uint64_t &rng) {
  // uniform over ALL unassigned variables (theoretical instance space)
  assert(k.unassigned > 0);
  for (;;) {
    const Var v = static_cast<Var>(next_random(rng) % k.nvars);
    if (k.values[make_lit(v, false)] == 0)
      return make_lit(v, next_random(rng) & 1);
  }
}

int random_solve(Kernel &k, uint64_t seed, volatile sig_atomic_t *stop,
                 RuleTrace *trace) {
  // A verdict must be recorded on EVERY exit, or a trace that stops early
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
    // periodic invariant oracle (plan §8.2 oracle 4): most random walks on
    // nontrivial instances never reach a verdict, so without this a logic
    // bug that corrupts state (but not memory) is invisible to the fuzz
    // leg. Every 256 steps: per-step would dominate the walk (the check
    // is O(vars + arena + watches)). No-op in NDEBUG builds.
    if ((steps & 255) == 0)
      kernel_check_invariants(k);
    if (k.inconsistent)
      return fin.verdict = 20;
    // SAT only when the assignment is total, fully propagated, and no
    // conflict is in flight: an unpropagated suffix or a recorded conflict can
    // both mean the current assignment still falsifies a clause. (No
    // `pending` check since rev 37: ConflictAnalysis is atomic, so a learned
    // clause never persists across this boundary.)
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
        applicable[next_random(rng) % applicable.size()];
    ApplyResult r;
    // The payload this step applied, in the rule's own encoding — recorded so
    // the step is replayable through apply_instance (trace.hpp). Decide and
    // Restart reach the same effect through a direct call below, so their
    // payload has to be spelled out rather than read back.
    uint64_t applied = INSTANCE_CANONICAL;
    if (id == decide_id) {
      const Lit l = random_unassigned_literal(k, rng);
      applied = lit_instance(l);
      r = decide_apply(k, l, true);
    } else if (id == restart_id) {
      // random keep-level (plan §8.2: random INSTANCE choices, not just
      // random order) — every level in [0, current] is a legal instance
      applied = next_random(rng) % (k.level + 1);
      r = restart_apply_to(k, static_cast<uint32_t>(applied));
    } else if (rules[id].instances && rules[id].apply_instance) {
      // generic random instance draw over the rule's own enumeration —
      // any rule registering an instance surface (Rephase, S/C/V families)
      // gets rule-order AND instance fuzzing for free. INSTANCE_CANONICAL
      // is drawn ~1/3 of the time (always when the enumeration is empty) so
      // the canonical BATCH/ROUND path is exercised end-to-end, not only one
      // instance at a time: a canonical round emits proof deltas the single
      // instances never emit together (Probe's multi-unit batch, Strengthen's
      // flush pre-pass + cascade, Vivify's full round, Lucky's cascade). The
      // proof strategy must be sound under EVERY legal schedule, so every
      // schedulable action — including the batches — is a fuzz target.
      uint64_t insts[16];
      const uint32_t m = rules[id].instances(k, insts, 16);
      const uint64_t inst = (m == 0 || next_random(rng) % 3 == 0)
                                ? INSTANCE_CANONICAL
                                : insts[next_random(rng) % m];
      applied = inst;
      r = apply_rule_instance(rules[id], k, inst);
    } else {
      r = apply_rule(rules[id], k);
    }
    if (trace)
      trace->record(id, applied);
    // per-step rule trace, env-gated (zero cost when off): the diagnostic that
    // pins an N1-schedule bug — it names the rule + result + state at every
    // step, where the every-256 invariant check only says that SOMETHING
    // corrupted state. See the `stress-policy` skill.
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
