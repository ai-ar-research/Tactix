// See trace.hpp for the format and the scope argument.
#include "policies/trace.hpp"

#include "rules/registry.hpp"

#include <cstdlib>
#include <cstring>

namespace tactix {

static constexpr int TRACE_VERSION = 1;

bool RuleTrace::open(const char *path, const char *policy, uint64_t seed) {
  out = std::fopen(path, "w");
  if (!out)
    return false;
  std::fprintf(out, "c tactix-trace %d\n", TRACE_VERSION);
  std::fprintf(out, "c policy %s seed=%llu\n", policy,
               (unsigned long long)seed);
  return true;
}

void RuleTrace::record(int rule_id, uint64_t instance) {
  if (!out)
    return;
  const char *name = tactix_rules()[static_cast<size_t>(rule_id)].name;
  if (instance == INSTANCE_CANONICAL)
    std::fprintf(out, "%s -\n", name);
  else
    std::fprintf(out, "%s %llu\n", name, (unsigned long long)instance);
}

void RuleTrace::finish(int verdict) {
  if (out)
    std::fprintf(out, "c verdict %d\n", verdict);
}

void RuleTrace::close() {
  if (out) {
    std::fclose(out);
    out = nullptr;
  }
}

// The same SAT predicate the registry-driven policies use: total assignment,
// fully propagated, no conflict in flight. Deliberately duplicated rather than
// shared with random_policy.cpp, so replay cannot silently inherit a change to
// it and a divergence must surface as the verdict mismatch replay checks.
static bool concluded_sat(const Kernel &k) {
  return k.unassigned == 0 && !k.conflict_valid &&
         kernel_propagation_fixpoint(k);
}

int replay_solve(Kernel &k, const char *path, volatile sig_atomic_t *stop) {
  std::FILE *in = std::fopen(path, "r");
  if (!in) {
    std::fprintf(stderr, "c ERROR: cannot read trace '%s'\n", path);
    return -1;
  }
  KernelStopPublisher stop_published(k, stop);
  const std::vector<RuleVTable> &rules = tactix_rules();
  char line[512];
  int version = 0, recorded = -1, verdict = 0;
  uint64_t steps = 0;
  bool done = false;
  bool policy_seen = false;
  // Mirror the recorded run's dense-connection state (random_policy.cpp: odd
  // seeds walk with the dense index attached). Replaying without it would
  // silently skip every dense-gated tail the recording exercised.
  OccIndex replay_occ;
  struct DenseScope {
    Kernel &k;
    bool active = false;
    ~DenseScope() {
      if (active)
        kernel_dense_disconnect(k);
    }
  } dense_scope{k};

  while (std::fgets(line, sizeof line, in)) {
    if (line[0] == 'c') {
      if (!version && std::sscanf(line, "c tactix-trace %d", &version) == 1) {
        if (version != TRACE_VERSION) {
          std::fprintf(stderr, "c ERROR: trace version %d, expected %d\n",
                       version, TRACE_VERSION);
          std::fclose(in);
          return -1;
        }
      }
      unsigned long long hseed = 0;
      if (!policy_seen &&
          std::sscanf(line, "c policy %*s seed=%llu", &hseed) == 1) {
        policy_seen = true;
        if ((hseed & 1) != 0 && k.level == 0) {
          kernel_dense_connect(k, replay_occ, true);
          dense_scope.active = true;
        }
        // byte-for-byte with the recorded run's stdout (trace_replay.sh
        // compares counter lines)
        std::printf("c stress-dense %d\n", dense_scope.active ? 1 : 0);
      }
      (void)std::sscanf(line, "c verdict %d", &recorded);
      continue;
    }
    if (!version) {
      std::fprintf(stderr, "c ERROR: '%s' is not a tactix trace\n", path);
      std::fclose(in);
      return -1;
    }
    if (done)
      continue; // actions after the verdict: the trace is longer than the run
    char name[128];
    char inst[64];
    if (std::sscanf(line, "%127s %63s", name, inst) != 2) {
      std::fprintf(stderr, "c ERROR: malformed trace line: %s", line);
      std::fclose(in);
      return -1;
    }
    const int id = tactix_rule_id(name);
    if (id < 0) {
      std::fprintf(stderr, "c ERROR: trace names unknown rule '%s'\n", name);
      std::fclose(in);
      return -1;
    }
    const RuleVTable &r = rules[static_cast<size_t>(id)];
    // A step whose guard no longer holds is a replay failure, not something to
    // skip: the recorded run and this one have already diverged, and skipping
    // would hide the divergence a replay harness exists to detect.
    if (!r.guard(k)) {
      std::fprintf(stderr,
                   "c ERROR: replay diverged at step %llu: %s's guard does not "
                   "hold\n",
                   (unsigned long long)steps, name);
      std::fclose(in);
      return -1;
    }
    const uint64_t payload = inst[0] == '-' && inst[1] == '\0'
                                 ? INSTANCE_CANONICAL
                                 : std::strtoull(inst, nullptr, 10);
    const ApplyResult res =
        (payload == INSTANCE_CANONICAL || !r.apply_instance)
            ? apply_rule(r, k)
            : apply_rule_instance(r, k, payload);
    steps++;
    if ((steps & 255) == 0)
      kernel_check_invariants(k); // no-op in NDEBUG, as in random_solve
    if (res == ApplyResult::Unsat || k.inconsistent) {
      verdict = 20;
      done = true;
    } else if (res == ApplyResult::Sat || concluded_sat(k)) {
      verdict = 10;
      done = true;
    }
    if (stop && *stop) {
      std::fclose(in);
      return 0; // interrupted: no verdict to compare
    }
  }
  std::fclose(in);
  if (recorded >= 0 && recorded != verdict) {
    std::fprintf(stderr,
                 "c ERROR: replay reached %d, the trace recorded %d — the run "
                 "is not reproducible from its own trace\n",
                 verdict, recorded);
    return -1;
  }
  return verdict;
}

} // namespace tactix
