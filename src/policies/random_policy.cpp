#include "policies/random_policy.hpp"
#include "policies/heuristics.hpp"
#include "policies/stress_common.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "rules/registry.hpp"
#include "strategy/online_inprocess.hpp"
#include "strategy/random_online.hpp"

namespace tactix {

static strategy::OnlineStrategyProgram
targeted_program(Kernel &k, const std::vector<int> &preferred,
                 uint64_t ordering_seed) {
  strategy::OnlineStrategyProgram program;
  if (preferred.empty())
    return program;
  const size_t start = ordering_seed % preferred.size();
  for (size_t offset = 0; offset < preferred.size(); ++offset) {
    const int id = preferred[(start + offset) % preferred.size()];
    const RuleVTable &rule = tactix_rules()[id];
    if (!rule.guard(k))
      continue;
    strategy::OnlineInvocation step;
    step.id = static_cast<strategy::StepId>(program.sequence.size()) + 1;
    step.rule = rule.name;
    step.granularity = strategy::InvocationGranularity::CanonicalApplication;
    step.repeat = {strategy::RepeatMode::Once, 1};
    step.work_unit = step.rule == "BVE"
                         ? strategy::WorkUnit::BveTicks
                         : step.rule == "Propagate"
                               ? strategy::WorkUnit::Propagations
                               : strategy::WorkUnit::Applications;
    step.budget = strategy::WorkLimit::unlimited();
    program.sequence.push_back(std::move(step));
  }
  program.repeat = {strategy::RepeatMode::Once, 1};
  return program;
}

static strategy::OnlineStrategyProgram service_program(Kernel &k) {
  // A targeted stress leg still has to reach a verdict so proof/model oracles
  // run. Use the ordinary proof-rule service order through the same executor;
  // parameter-randomized broad programs remain the unconfigured policy path.
  for (const char *name : {"Propagate", "ConflictAnalysis", "Decide",
                           "Restart"}) {
    const int id = tactix_rule_id(name);
    if (id < 0 || !tactix_rules()[id].guard(k))
      continue;
    return targeted_program(k, {id}, 0);
  }
  return {};
}

int random_solve(Kernel &k, uint64_t seed, volatile sig_atomic_t *stop,
                 RuleTrace *trace,
                 const strategy::RandomOnlineConfig *configured) {
  const char *stress_spec = getenv("TACTIX_STRESS_RULES");
  const bool targeted = stress_spec && *stress_spec;
  const uint32_t normal_every = stress_env_u32("TACTIX_STRESS_NORMAL_EVERY", 8);
  std::vector<int> preferred;
  if (targeted) {
    const std::string spec(stress_spec);
    for (size_t begin = 0; begin <= spec.size();) {
      const size_t end = spec.find(',', begin);
      const int id = tactix_rule_id(spec.substr(begin, end - begin).c_str());
      if (id >= 0)
        preferred.push_back(id);
      if (end == std::string::npos)
        break;
      begin = end + 1;
    }
  }
  const bool trace_steps = getenv("TACTIX_TRACE") != nullptr;
  struct Finish {
    RuleTrace *trace;
    int verdict = 0;
    bool targeted = false;
    uint64_t target_fires = 0;
    ~Finish() {
      if (trace)
        trace->finish(verdict);
      if (targeted)
        std::printf("c stress-target-fires %llu\n",
                    (unsigned long long)target_fires);
    }
  } finish{trace, 0, targeted, 0};
  KernelStopPublisher stop_published(k, stop);

  OccIndex occurrence_index;
  struct DenseScope {
    Kernel &kernel;
    bool active = false;
    ~DenseScope() {
      if (active)
        kernel_dense_disconnect(kernel);
    }
  } dense{k};
  if ((seed & 1) != 0 && k.level == 0) {
    kernel_dense_connect(k, occurrence_index, true);
    dense.active = true;
  }
  std::printf("c stress-dense %d\n", dense.active ? 1 : 0);

  uint64_t rng = seed ? seed : 1;
  uint64_t revision = 0;
  uint64_t applications = 0;
  bool force_service_program = false;
  const strategy::RandomOnlineConfig default_config;
  const strategy::RandomOnlineConfig &config =
      configured ? *configured : default_config;
  for (;;) {
    if ((applications & 255) == 0)
      kernel_check_invariants(k);
    if (k.inconsistent)
      return finish.verdict = 20;
    if (k.unassigned == 0 && !k.conflict_valid &&
        kernel_propagation_fixpoint(k))
      return finish.verdict = 10;
    if (stop && *stop)
      return finish.verdict = 0;

    strategy::OnlineStrategyProgram program;
    std::string error;
    const uint64_t program_seed = stress_random(rng);
    bool targeted_program_selected =
        targeted && !force_service_program && normal_every &&
        ((revision + 1) % normal_every) != 0;
    if (targeted_program_selected)
      program = targeted_program(k, preferred, program_seed);
    if (program.sequence.empty()) {
      targeted_program_selected = false;
      if (targeted)
        program = service_program(k);
      if (program.sequence.empty() &&
          !strategy::random_online_strategy_program(k, program_seed, config,
                                                     program, &error, stop)) {
        if (stop && *stop)
          return finish.verdict = 0;
        std::fprintf(stderr, "c ERROR: cannot generate online strategy: %s\n",
                     error.c_str());
        return finish.verdict = 0;
      }
    }
    force_service_program = false;
    ++revision;
    strategy::OnlineStrategyExecutor executor;
    if (!executor.configure(program, revision,
                            strategy::ReconfigureMode::ResetRuntime, &error)) {
      std::fprintf(stderr, "c ERROR: cannot configure online strategy: %s\n",
                   error.c_str());
      return finish.verdict = 0;
    }
    if (trace)
      trace->record_program(
          revision, strategy::serialize_online_strategy_program(program));

    bool program_progress = false;
    while (!executor.complete()) {
      const strategy::InvocationResult result = executor.advance(k, stop);
      applications += result.applications;
      program_progress |= result.made_rule_progress;
      if (targeted && result.applications)
        for (const int id : preferred)
          if (result.rule_id == static_cast<uint32_t>(id)) {
            finish.target_fires += result.applications;
            break;
          }
      if (trace_steps && result.applications) {
        const char *name = result.rule_id < tactix_rules().size()
                               ? tactix_rules()[result.rule_id].name
                               : "?";
        std::fprintf(stderr,
                     "online %-16s status=%u level=%u applications=%llu "
                     "revision=%llu\n",
                     name, static_cast<unsigned>(result.status), k.level,
                     (unsigned long long)applications,
                     (unsigned long long)revision);
      }
      if (result.applications && (applications & 255) == 0)
        kernel_check_invariants(k);
      if (targeted && result.applications)
        kernel_check_invariants(k);
      if (result.status == strategy::InvocationStatus::Unsat ||
          k.inconsistent)
        return finish.verdict = 20;
      if (result.status == strategy::InvocationStatus::Sat ||
          (k.unassigned == 0 && !k.conflict_valid &&
           kernel_propagation_fixpoint(k)))
        return finish.verdict = 10;
      if (result.status == strategy::InvocationStatus::Interrupted)
        return finish.verdict = 0;
      // A changed guard or invalidated explicit snapshot is a normal online
      // boundary: discard only executor runtime and generate from the current
      // kernel. The formula, proof, heuristic state and search trail continue.
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
                     "c ERROR: generated online strategy failed validation\n");
        return finish.verdict = 0;
      }
    }
    force_service_program = targeted_program_selected && !program_progress;
  }
}

int stress_solve(Kernel &k, uint64_t seed, volatile sig_atomic_t *stop) {
  return random_solve(k, seed, stop, nullptr);
}

} // namespace tactix
