#include "policies/random_policy.hpp"

#include "rules/backjump/backjump.hpp"
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

int random_solve(Kernel &k, uint64_t seed, volatile sig_atomic_t *stop) {
  uint64_t rng = seed ? seed : 1;
  const auto &rules = tactix_rules();
  const int decide_id = tactix_rule_id("Decide");
  const int restart_id = tactix_rule_id("Restart");
  const int backjump_id = tactix_rule_id("Backjump");
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
      return 20;
    // SAT only when the assignment is total, fully propagated, and no
    // conflict is in flight (recorded OR analyzed-but-not-backjumped): an
    // unpropagated suffix or a pending learned clause can both mean the
    // current assignment still falsifies a clause
    if (k.unassigned == 0 && !k.conflict_valid && !k.pending.valid &&
        k.propagate_head == k.trail.size())
      return 10;
    if (stop && *stop && (steps & 1023) == 0)
      return 0;

    applicable.clear();
    for (size_t i = 0; i < rules.size(); i++)
      if (rules[i].guard(k))
        applicable.push_back(static_cast<int>(i));
    assert(!applicable.empty()); // some rule is always applicable pre-verdict

    const int id =
        applicable[next_random(rng) % applicable.size()];
    ApplyResult r;
    if (id == decide_id) {
      r = decide_apply(k, random_unassigned_literal(k, rng), true);
    } else if (id == restart_id) {
      // random keep-level (plan §8.2: random INSTANCE choices, not just
      // random order) — every level in [0, current] is a legal instance
      r = restart_apply_to(k, next_random(rng) % (k.level + 1));
    } else if (id == backjump_id) {
      // random target; apply_to clamps into [jump_level, current]
      r = backjump_apply_to(k, next_random(rng) % (k.level + 1));
    } else {
      r = rules[id].apply(k);
    }
    if (r == ApplyResult::Unsat)
      return 20;
    if (r == ApplyResult::Sat)
      return 10;
  }
}

} // namespace tactix
