// Trace and replay for typed online programs. Historical action traces remain
// readable so saved v1/v2 fuzz failures can still be reproduced. See
// DEVELOPMENT_PLAN.md §5 and §8.5 for the reproducibility contract.
#pragma once

#include "kernel/kernel.hpp"

#include <cstdint>
#include <cstdio>
#include <string>

namespace tactix {

// ASCII format, one action or typed online program per line; `c ...` is a
// comment line. Example:
// `Decide v:4294967295`, `Congruence s:4294967298`, or `Propagate -`.
// The rule is named, never numbered: registry ids come from a name sort, so
// adding a rule renumbers them and an id-keyed trace would silently replay the
// wrong rules (tests/test_api_c.c pins the list as a golden file). `-` is the
// canonical instance, anything else the rule's own uint64 payload encoding.
// New traces write `p REVISION SERIALIZED_PROGRAM`, preserving the full
// executor request. Replay continues to read historical action lines. The
// recorder has zero cost when `out` is null, which is every non-tracing run.
struct RuleTrace {
  std::FILE *out = nullptr;

  bool open(const char *path, const char *policy, uint64_t seed);
  // RandomPolicy records the complete typed program before executing it. A
  // replay therefore restores parameters, granularity, budgets and repetition
  // as well as the atomic rule payloads selected by the executor.
  void record_program(uint64_t revision, const std::string &program);
  // The verdict the traced run reached; replay checks it and fails on a
  // mismatch, so "the trace replayed" is a claim about the OUTCOME and not
  // merely about the lines parsing.
  void finish(int verdict);
  void close();
};

// Apply exactly the recorded programs through OnlineStrategyExecutor, or a
// historical sequence through the scalar/semantic callbacks. Returns the
// verdict (10/20/0), or -1 if the trace is malformed, diverges, or ends on a
// different verdict than it recorded.
int replay_solve(Kernel &k, const char *path, volatile sig_atomic_t *stop);

} // namespace tactix
