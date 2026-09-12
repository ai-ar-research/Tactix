// Rule trace and replay for the registry stepping surface. One named rule,
// instance kind and payload is recorded per applied action. See
// DEVELOPMENT_PLAN.md §5 and §8.5 for the reproducibility contract.
#pragma once

#include "kernel/kernel.hpp"

#include <cstdint>
#include <cstdio>

namespace tactix {

// ASCII format, one action per line; `c ...` is a comment line. Example:
// `Decide v:4294967295`, `Congruence s:4294967298`, or `Propagate -`.
// The rule is named, never numbered: registry ids come from a name sort, so
// adding a rule renumbers them and an id-keyed trace would silently replay the
// wrong rules (tests/test_api_c.c pins the list as a golden file). `-` is the
// canonical instance, anything else the rule's own uint64 payload encoding.
// The recorder is handed to a policy, which calls `record` for every action;
// zero cost when `out` is null, which is every non-tracing run.
struct RuleTrace {
  std::FILE *out = nullptr;

  bool open(const char *path, const char *policy, uint64_t seed);
  void record(int rule_id, uint64_t instance, bool semantic = false);
  // The verdict the traced run reached; replay checks it and fails on a
  // mismatch, so "the trace replayed" is a claim about the OUTCOME and not
  // merely about the lines parsing.
  void finish(int verdict);
  void close();
};

// Apply exactly the recorded sequence, through the same public surface
// scalar or semantic application callbacks use. Returns the verdict (10/20/0), or -1 if the
// trace is malformed, names an unknown rule, reaches an action whose guard does
// not hold, or ends on a different verdict than it recorded — each of which is
// a real defect rather than a reason to keep going.
int replay_solve(Kernel &k, const char *path, volatile sig_atomic_t *stop);

} // namespace tactix
