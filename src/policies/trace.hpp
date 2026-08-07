// Rule trace and replay (plan §5 "Reproducibility", §8.5 offline replay
// harness, which must validate a reward proxy before RL1 may trust it).
//
// A trace is exactly the N1 stepping surface: one (rule, instance) pair per
// applied action, in order — the whole action space of a pull-based policy
// (§5.1: guard -> instances -> apply_instance). Scope is therefore the
// registry-driven policies only. DefaultPolicy is not traceable this way: its
// micro loop calls propagate/decide/analyze directly rather than through the
// registry, and its dosed entries re-apply one rule many times under a
// policy-computed budget, so its behaviour is not a function of such a
// sequence, and making it one is scheduling-as-data work.
#pragma once

#include "kernel/kernel.hpp"

#include <cstdint>
#include <cstdio>

namespace tactix {

// Format: ASCII, one action per line; `c ...` is a comment line.
//     c tactix-trace 1
//     c policy random seed=7
//     Decide 4294967295
//     Propagate -
//     c verdict 20
// The rule is named, never numbered: registry ids come from a name sort, so
// adding a rule renumbers them and an id-keyed trace would silently replay the
// wrong rules (tests/test_api_c.c pins the list as a golden file). `-` is the
// canonical instance, anything else the rule's own uint64 payload encoding.
// The recorder is handed to a policy, which calls `record` for every action;
// zero cost when `out` is null, which is every non-tracing run.
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
