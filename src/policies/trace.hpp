// RULE TRACE + REPLAY (plan §5 "Reproducibility", §8.5 offline replay harness).
//
// The plan has always said `--trace` logs the rule sequence so any run is
// replayable through a scripted policy, and §8.5 forbids trusting a reward
// proxy before an offline replay harness validates it — so RL1 cannot start
// without this. Nothing implemented it: `TACTIX_TRACE` was a human-readable
// stderr dump with no reader.
//
// WHAT A TRACE IS. Exactly the N1 stepping surface: one (rule, instance) pair
// per applied action, in order. That is the whole action space a pull-based
// policy has (§5.1: guard -> instances -> apply_instance), so a policy that
// drives the solver through it is replayable by construction, and one that does
// not is not expressible as a sequence at all.
//
// SCOPE, deliberately narrow. This records and replays the REGISTRY-DRIVEN
// policies — RandomPolicy today, LearnedPolicy tomorrow. DefaultPolicy is NOT
// traceable this way and pretending otherwise would be the worse outcome: its
// micro loop calls propagate_apply/decide/analyze directly rather than through
// the registry, and its dosed entries re-apply one rule many times under a
// policy-computed budget, so its behaviour is not a function of a (rule,
// instance) sequence. Making it one is the scheduling-as-data work (SP-* ledger
// rows), not a trace format.
//
// FORMAT: ASCII, one action per line, `c ...` comments elsewhere.
//     c tactix-trace 1
//     c policy random seed=7
//     Decide 4294967295
//     Propagate -
//     c verdict 20
// The rule is named, never numbered: registry ids are assigned by a name sort,
// so ADDING a rule renumbers them (tests/test_api_c.c pins that list as a
// golden file precisely because it moves). A trace keyed on ids would silently
// replay the wrong rules after the next rule lands. `-` is the canonical
// instance; anything else is the rule's own uint64 payload encoding.
#pragma once

#include "kernel/kernel.hpp"

#include <cstdint>
#include <cstdio>

namespace tactix {

// Recorder. Handed to a policy, which calls `record` for every action it
// applies. Zero cost when `out` is null, which is every non-tracing run.
struct RuleTrace {
  std::FILE *out = nullptr;

  bool open(const char *path, const char *policy, uint64_t seed);
  void record(int rule_id, uint64_t instance);
  // The verdict the traced run reached; replay checks it and fails on a
  // mismatch, so "the trace replayed" is a claim about the OUTCOME and not
  // merely about the lines parsing.
  void finish(int verdict);
  void close();
};

// Apply exactly the recorded sequence, through the same public surface
// `tactix_apply_instance` uses. Returns the verdict (10/20/0), or -1 if the
// trace is malformed, names an unknown rule, reaches an action whose guard does
// not hold, or ends on a different verdict than it recorded — each of which is
// a real defect rather than a reason to keep going.
int replay_solve(Kernel &k, const char *path, volatile sig_atomic_t *stop);

} // namespace tactix
