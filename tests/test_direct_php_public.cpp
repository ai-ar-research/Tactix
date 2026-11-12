#undef NDEBUG
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix

#include "direct_php_test_support.hpp"
#include "temporary_file.hpp"
#include "frontend/parse.hpp"
#include "rules/direct_php/direct_php_action.hpp"
#include "rules/registry.hpp"
#include "strategy/online_inprocess.hpp"
#include "strategy/controller_observation.hpp"

#include <cassert>
#include <algorithm>
#include <string>
#include <vector>

using namespace tactix;
using namespace tactix::strategy;

namespace {

Kernel fixture() { return php_kernel(); }

DirectPhpActionLimits default_limits() {
  const DirectPhpRefutationParameters p;
  return {{p.maximum_snapshot_clauses, p.maximum_snapshot_literals,
           p.maximum_snapshot_work},
          {p.maximum_recognition_clauses, p.maximum_recognition_literals,
           p.maximum_recognition_work, p.maximum_row_candidates,
           p.maximum_row_combinations, p.maximum_mapping_trials,
           p.maximum_holes},
          {p.maximum_proof_additions, p.maximum_proof_deletions,
           p.maximum_proof_literals, p.maximum_fresh_variables,
           p.maximum_proof_events},
          p.maximum_replay_work};
}

OnlineInvocation invocation(uint64_t id = 25) {
  OnlineInvocation step;
  step.id = id;
  step.rule = "DirectPhpRefutation";
  step.parameters = DirectPhpRefutationParameters{};
  step.granularity = InvocationGranularity::CanonicalApplication;
  step.repeat = {RepeatMode::Once, 1};
  step.work_unit = WorkUnit::Applications;
  step.budget = WorkLimit::bounded(1);
  return step;
}

void identity_guard_and_disabled_default() {
  const int id = tactix_rule_id("DirectPhpRefutation");
  assert(id == 36);
  const auto &rule = tactix_rules().at(id);
  assert(rule.tier == RuleTier::Inprocess);
  Kernel kernel = fixture();
  assert(!rule.guard(kernel));
  DirectPhpActionSession session(kernel);
  kernel.direct_php_action_session = &session;
  assert(rule.guard(kernel));
  const auto program = reference_inprocess_program();
  const auto found = std::find_if(program.begin(), program.end(), [](const auto &s) {
    return s.rule == "DirectPhpRefutation";
  });
  assert(found != program.end() && found->id == 25 && !found->enabled);
}

void explicit_program_reaches_the_same_action() {
  OnlineStrategyProgram program{{invocation()}, {RepeatMode::Once, 1}};
  std::string error;
  assert(validate_online_strategy_program(program, &error));
  const std::string encoded = serialize_online_strategy_program(program);
  assert(encoded.find("direct-php-refutation1(") != std::string::npos);

  Kernel kernel = fixture();
  DirectPhpActionSession session(kernel);
  kernel.direct_php_action_session = &session;
  OnlineStrategyExecutor executor;
  assert(executor.configure(program, 7, ReconfigureMode::ResetRuntime, &error));
  const InvocationResult result = executor.advance(kernel);
  assert(result.status == InvocationStatus::Unsat);
  assert(result.rule_result == ApplyResult::Unsat);
  assert(result.rule_id == 36 && result.step_id == 25);
  assert(result.applications == 1 && result.work == 1);
  assert(session.committed() && kernel.inconsistent);
  assert(kernel.stats.direct_php_attempts == 1);
  assert(kernel.stats.direct_php_applied == 1);
  assert(kernel.stats.direct_php_snapshot_clauses == 22);
  assert(kernel.stats.direct_php_recognition_work);
  assert(kernel.stats.direct_php_proof_events);
  assert(kernel.stats.direct_php_proof_additions);
  const auto observed = controller::observe_kernel(kernel);
  assert(observed.cumulative.direct_php_snapshot_work ==
         kernel.stats.direct_php_snapshot_work);
  assert(observed.cumulative.direct_php_proof_events ==
         kernel.stats.direct_php_proof_events);
}

void allowance_declines_before_action() {
  OnlineInvocation step = invocation(26);
  step.budget = WorkLimit::bounded(0);
  OnlineStrategyProgram program{{step}, {RepeatMode::Once, 1}};
  Kernel kernel = fixture();
  DirectPhpActionSession session(kernel);
  kernel.direct_php_action_session = &session;
  OnlineStrategyExecutor executor;
  std::string error;
  assert(executor.configure(program, 8, ReconfigureMode::ResetRuntime, &error));
  const InvocationResult result = executor.advance(kernel);
  assert(result.status == InvocationStatus::BudgetExhausted);
  assert(!session.committed() && kernel.stats.direct_php_attempts == 0);
}

void duplicate_binary_snapshot_is_not_invalid() {
  Kernel kernel;
  kernel_reserve_vars(kernel, 3);
  kernel_add_binary_clause(kernel, internalize(1), internalize(2),
                           ++kernel.next_clause_id);
  kernel_add_binary_clause(kernel, internalize(1), internalize(2),
                           ++kernel.next_clause_id);
  DirectPhpActionSession session(kernel);
  const auto result =
      direct_php_action_apply(kernel, session, {1, 1, default_limits()});
  assert(result.status == DirectPhpActionStatus::NotRecognized);
  assert(result.effort.snapshot_clauses == 1);
}

void add_c_clause(tactix_c_handle *solver, const std::vector<int32_t> &clause) {
  for (int literal : clause) tactix_add(solver, literal);
  tactix_add(solver, 0);
}

void parser_surface_has_stable_source_ids() {
  Kernel kernel;
  for (const auto &clause : php_clauses(3)) {
    std::vector<Lit> internal;
    for (int literal : clause) internal.push_back(internalize(literal));
    ingest_clause(kernel, internal);
  }
  finish_input(kernel);
  DirectPhpActionSession session(kernel);
  const auto result =
      direct_php_action_apply(kernel, session, {1, 1, default_limits()});
  assert(result.status == DirectPhpActionStatus::Applied);
  assert(result.effort.snapshot_clauses == 22);
  const auto *record = session.record();
  assert(record);
  assert(std::any_of(record->snapshot.sources.begin(),
                     record->snapshot.sources.end(), [](const auto &source) {
    return source.kind == direct_php_live::SourceKind::Binary &&
           source.identity_kind ==
               direct_php_live::SourceIdentityKind::CaptureLocal;
  }));
  assert(std::any_of(record->events.begin(), record->events.end(),
                     [](const auto &event) {
    return event.source_occurrence && event.deleted_id_is_capture_local;
  }));
}

void c_api_executes_terminal_action() {
  tactix_c_handle *solver = tactix_init();
  TemporaryFile file;
  assert(tactix_proof(solver, file.path.c_str(), TACTIX_PROOF_DSR_ASCII) == 0);
  for (const auto &clause : php_clauses(3)) add_c_clause(solver, clause);
  tactix_online_step step{};
  step.step_id = 25;
  step.rule_id = 36;
  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_DIRECT_PHP_REFUTATION;
  auto &p = step.parameters.value.direct_php_refutation;
  p = {100000, 1000000, 10000000, 100000, 1000000, 100000000,
       10000, 1000000, 1000000, 20, 1000000, 1000000, 100000000,
       1000000, 2000000, 100000000};
  step.granularity = TACTIX_ONLINE_GRANULARITY_CANONICAL;
  step.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.repeat_limit = 1;
  step.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  step.budget = 1;
  tactix_online_program program{&step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_configure(solver, &program, 9,
                                 TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  tactix_online_result result{};
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.status == TACTIX_ONLINE_UNSAT);
  assert(result.rule_id == 36 && result.applications == 1);
  assert(tactix_status(solver) == 20);
  tactix_release(solver);
}

} // namespace

int main() {
  identity_guard_and_disabled_default();
  explicit_program_reaches_the_same_action();
  allowance_declines_before_action();
  duplicate_binary_snapshot_is_not_invalid();
  parser_surface_has_stable_source_ids();
  c_api_executes_terminal_action();
}
