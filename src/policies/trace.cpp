// See trace.hpp for the format and the scope argument.
#include "policies/trace.hpp"

#include "policies/heuristics.hpp"
#include "rules/registry.hpp"
#include "strategy/online_inprocess.hpp"
#include "strategy/random_online.hpp"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

namespace tactix {

static constexpr int TRACE_VERSION = 3;

bool RuleTrace::open(const char *path, const char *policy, uint64_t seed) {
  out = std::fopen(path, "w");
  if (!out)
    return false;
  std::fprintf(out, "c tactix-trace %d\n", TRACE_VERSION);
  std::fprintf(out, "c policy %s seed=%llu\n", policy,
               (unsigned long long)seed);
  return true;
}

void RuleTrace::record_program(uint64_t revision, const std::string &program) {
  if (out) {
    std::fprintf(out, "p %llu %s\n", (unsigned long long)revision,
                 program.c_str());
    // A crash or hard timeout inside the following atomic application must
    // still identify the configuration that triggered it.
    std::fflush(out);
  }
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
  std::ifstream in(path);
  if (!in) {
    std::fprintf(stderr, "c ERROR: cannot read trace '%s'\n", path);
    return -1;
  }
  KernelStopPublisher stop_published(k, stop);
  const std::vector<RuleVTable> &rules = tactix_rules();
  std::string line;
  int version = 0, recorded = -1, verdict = 0;
  uint64_t steps = 0;
  bool done = false;
  bool policy_seen = false;
  enum class TraceBody { None, Actions, Programs } body = TraceBody::None;
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

  while (std::getline(in, line)) {
    if (!line.empty() && line[0] == 'c') {
      if (!version &&
          std::sscanf(line.c_str(), "c tactix-trace %d", &version) == 1) {
        if (version < 1 || version > TRACE_VERSION) {
          std::fprintf(stderr,
                       "c ERROR: trace version %d, expected 1 through %d\n",
                       version, TRACE_VERSION);
          return -1;
        }
      }
      unsigned long long hseed = 0;
      if (!policy_seen &&
          std::sscanf(line.c_str(), "c policy %*s seed=%llu", &hseed) == 1) {
        policy_seen = true;
        if ((hseed & 1) != 0 && k.level == 0) {
          kernel_dense_connect(k, replay_occ, true);
          dense_scope.active = true;
        }
        // byte-for-byte with the recorded run's stdout (trace_replay.sh
        // compares counter lines)
        std::printf("c stress-dense %d\n", dense_scope.active ? 1 : 0);
      }
      (void)std::sscanf(line.c_str(), "c verdict %d", &recorded);
      continue;
    }
    if (!version) {
      std::fprintf(stderr, "c ERROR: '%s' is not a tactix trace\n", path);
      return -1;
    }
    if (done)
      continue; // actions after the verdict: the trace is longer than the run
    if (version >= 3 && line.rfind("p ", 0) == 0) {
      if (body == TraceBody::Actions) {
        std::fprintf(stderr, "c ERROR: trace mixes actions and programs\n");
        return -1;
      }
      body = TraceBody::Programs;
      const size_t revision_end = line.find(' ', 2);
      if (revision_end == std::string::npos) {
        std::fprintf(stderr, "c ERROR: malformed program trace line\n");
        return -1;
      }
      char *revision_tail = nullptr;
      const uint64_t revision =
          std::strtoull(line.c_str() + 2, &revision_tail, 10);
      if (revision_tail != line.c_str() + revision_end || !revision) {
        std::fprintf(stderr, "c ERROR: malformed program revision\n");
        return -1;
      }
      strategy::OnlineStrategyProgram program;
      std::string error;
      if (!strategy::deserialize_online_strategy_program(
              line.substr(revision_end + 1), program, &error)) {
        std::fprintf(stderr, "c ERROR: malformed online program: %s\n",
                     error.c_str());
        return -1;
      }
      strategy::OnlineStrategyExecutor executor;
      if (!executor.configure(program, revision,
                              strategy::ReconfigureMode::ResetRuntime,
                              &error)) {
        std::fprintf(stderr, "c ERROR: invalid online program: %s\n",
                     error.c_str());
        return -1;
      }
      while (!executor.complete()) {
        const strategy::InvocationResult result = executor.advance(k, stop);
        steps += result.applications;
#ifndef NDEBUG
        if (result.applications) {
          if (std::getenv("TACTIX_TRACE")) {
            const char *name = result.rule_id < rules.size()
                                   ? rules[result.rule_id].name
                                   : "?";
            std::fprintf(stderr,
                         "replay %-16s applications=%llu revision=%llu\n",
                         name, (unsigned long long)steps,
                         (unsigned long long)revision);
          }
          kernel_check_invariants(k);
        }
#else
        if (result.applications && (steps & 255) == 0)
          kernel_check_invariants(k);
#endif
        if (result.status == strategy::InvocationStatus::Unsat ||
            k.inconsistent) {
          verdict = 20;
          done = true;
          break;
        }
        if (result.status == strategy::InvocationStatus::Sat ||
            concluded_sat(k)) {
          verdict = 10;
          done = true;
          break;
        }
        if (result.status == strategy::InvocationStatus::Interrupted)
          return 0;
        // These statuses are the online controller's safe reconfiguration
        // boundary. The next recorded program resumes from the current kernel.
        if (result.status == strategy::InvocationStatus::Conflict ||
            result.status == strategy::InvocationStatus::Inapplicable ||
            result.status == strategy::InvocationStatus::StaleRequest ||
            result.status == strategy::InvocationStatus::ResourceExhausted ||
            (result.status == strategy::InvocationStatus::BudgetExhausted &&
             !result.selection_complete))
          break;
        if (result.status == strategy::InvocationStatus::InvalidRequest ||
            result.status == strategy::InvocationStatus::AccountingError) {
          std::fprintf(stderr,
                       "c ERROR: online program failed during replay\n");
          return -1;
        }
      }
      continue;
    }
    if (body == TraceBody::Programs) {
      std::fprintf(stderr, "c ERROR: trace mixes programs and actions\n");
      return -1;
    }
    body = TraceBody::Actions;
    char name[128];
    char inst[64];
    if (std::sscanf(line.c_str(), "%127s %63s", name, inst) != 2) {
      std::fprintf(stderr, "c ERROR: malformed trace line: %s\n", line.c_str());
      return -1;
    }
    const int id = tactix_rule_id(name);
    if (id < 0) {
      std::fprintf(stderr, "c ERROR: trace names unknown rule '%s'\n", name);
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
      return -1;
    }
    bool semantic = false;
    const char *number = inst;
    if (version >= 2 && inst[0] != '-') {
      if ((inst[0] != 's' && inst[0] != 'v') || inst[1] != ':') {
        std::fprintf(stderr, "c ERROR: malformed v2 payload '%s'\n", inst);
        return -1;
      }
      semantic = inst[0] == 's';
      number += 2;
    }
    const uint64_t payload = inst[0] == '-' && inst[1] == '\0'
                                 ? INSTANCE_CANONICAL
                                 : std::strtoull(number, nullptr, 10);
    if (semantic && !r.apply_semantic_instance) {
      std::fprintf(stderr,
                   "c ERROR: trace requests a semantic payload from %s\n",
                   name);
      return -1;
    }
    // RandomPolicy snapshots the current target phases before abandoning a
    // non-root suffix. This is policy state around Restart rather than part of
    // the proof rule, so reproduce it at the trace boundary.
    if (!std::strcmp(name, "Restart") && payload != INSTANCE_CANONICAL &&
        payload < k.level)
      heuristics_update_target_phases(k);
    const ApplyResult res =
        payload == INSTANCE_CANONICAL
            ? apply_rule(r, k)
            : semantic ? apply_rule_semantic_instance(r, k, payload)
                       : r.apply_instance
                             ? apply_rule_instance(r, k, payload)
                             : apply_rule(r, k);
    // RandomPolicy consumes rule-produced search advice and retirements after
    // every action. Replay must do the same: these H transitions can change a
    // later branch choice even when the proof-producing rule delta is exact.
    heuristics_consume_rule_advice(k);
    heuristics_consume_retirements(k);
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
      return 0; // interrupted: no verdict to compare
    }
  }
  // The recorded driver checks for a verdict before generating its first
  // program. Empty formulas and formulas completed by parsing therefore have
  // a valid trace body containing only metadata and the verdict.
  if (body == TraceBody::None) {
    if (k.inconsistent)
      verdict = 20;
    else if (concluded_sat(k))
      verdict = 10;
  }
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
