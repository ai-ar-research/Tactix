#undef NDEBUG
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix

#include "kernel/kernel.hpp"
#include "frontend/parse.hpp"
#include "rules/direct_php/direct_php_action.hpp"
#include "rules/registry.hpp"
#include "strategy/online_inprocess.hpp"
#include "strategy/controller_observation.hpp"
#include "strategy/random_online.hpp"

#include <cassert>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>

using namespace tactix;
using namespace tactix::strategy;

namespace {

Kernel fixture(uint32_t holes = 3) {
  const uint32_t pigeons = holes + 1;
  Kernel kernel;
  kernel_reserve_vars(kernel, pigeons * holes);
  const auto variable = [holes](uint32_t pigeon, uint32_t hole) {
    return int32_t(pigeon * holes + hole + 1);
  };
  for (uint32_t pigeon = 0; pigeon < pigeons; ++pigeon) {
    std::vector<Lit> alo;
    for (uint32_t hole = 0; hole < holes; ++hole)
      alo.push_back(internalize(variable(pigeon, hole)));
    kernel_add_large_clause(kernel, alo.data(), alo.size(), false, 0,
                            ++kernel.next_clause_id);
  }
  for (uint32_t hole = 0; hole < holes; ++hole)
    for (uint32_t first = 0; first < pigeons; ++first)
      for (uint32_t second = first + 1; second < pigeons; ++second)
        kernel_add_binary_clause(kernel,
            internalize(-variable(first, hole)),
            internalize(-variable(second, hole)), ++kernel.next_clause_id);
  return kernel;
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

void serialized_replay_and_terminal_feedback() {
  OnlineStrategyProgram program{{invocation()}, {RepeatMode::Once, 1}};
  std::string error;
  assert(validate_online_strategy_program(program, &error));
  const std::string encoded = serialize_online_strategy_program(program);
  assert(encoded.find("direct-php-refutation1(") != std::string::npos);
  OnlineStrategyProgram replay;
  assert(deserialize_online_strategy_program(encoded, replay, &error));
  assert(serialize_online_strategy_program(replay) == encoded);

  Kernel kernel = fixture();
  DirectPhpActionSession session(kernel);
  kernel.direct_php_action_session = &session;
  OnlineStrategyExecutor executor;
  assert(executor.configure(replay, 7, ReconfigureMode::ResetRuntime, &error));
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

  std::string corrupted = encoded;
  const size_t tag = corrupted.find("direct-php-refutation1(");
  assert(tag != std::string::npos);
  corrupted.replace(tag, std::strlen("direct-php-refutation1"),
                    "direct-php-refutation2");
  assert(!deserialize_online_strategy_program(corrupted, replay, &error));
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
  const auto p = DirectPhpRefutationParameters{};
  const DirectPhpActionLimits limits{
      {p.maximum_snapshot_clauses, p.maximum_snapshot_literals,
       p.maximum_snapshot_work},
      {p.maximum_recognition_clauses, p.maximum_recognition_literals,
       p.maximum_recognition_work, p.maximum_row_candidates,
       p.maximum_row_combinations, p.maximum_mapping_trials,
       p.maximum_holes},
      {p.maximum_proof_additions, p.maximum_proof_deletions,
       p.maximum_proof_literals, p.maximum_fresh_variables,
       p.maximum_proof_events},
      p.maximum_replay_work};
  const auto result = direct_php_action_apply(kernel, session, {1, 1, limits});
  assert(result.status == DirectPhpActionStatus::NotRecognized);
  assert(result.effort.snapshot_clauses == 1);
}

void random_generator_reaches_the_same_action() {
  Kernel kernel = fixture();
  DirectPhpActionSession session(kernel);
  kernel.direct_php_action_session = &session;
  RandomOnlineConfig config;
  config.rules = {"DirectPhpRefutation"};
  config.maximum_sequence_length = 1;
  OnlineStrategyProgram program;
  std::string error;
  for (uint64_t seed = 1; seed <= 256; ++seed) {
    assert(random_online_strategy_program(kernel, seed, config, program,
                                          &error));
    assert(program.sequence.size() == 1);
    assert(program.sequence[0].rule == "DirectPhpRefutation");
    assert(program.sequence[0].post_application == PostApplication::None);
    assert(validate_online_strategy_program(program, &error));
  }
  assert(random_online_strategy_program(kernel, 17, config, program, &error));
  assert(program.sequence.size() == 1);
  assert(program.sequence[0].rule == "DirectPhpRefutation");
  assert(std::holds_alternative<DirectPhpRefutationParameters>(
      program.sequence[0].parameters));
  OnlineStrategyExecutor executor;
  assert(executor.configure(program, 18, ReconfigureMode::ResetRuntime,
                            &error));
  const InvocationResult result = executor.advance(kernel);
  assert(result.status == InvocationStatus::Unsat);
}

void add_c_clause(tactix_c_handle *solver, const std::vector<int> &clause) {
  for (int literal : clause) tactix_add(solver, literal);
  tactix_add(solver, 0);
}

void parser_surface_has_stable_source_ids() {
  Kernel kernel;
  const int holes = 3, pigeons = 4;
  const auto add = [&](std::vector<int> clause) {
    std::vector<Lit> internal;
    for (int literal : clause) internal.push_back(internalize(literal));
    ingest_clause(kernel, internal);
  };
  for (int pigeon = 0; pigeon < pigeons; ++pigeon) {
    std::vector<int> clause;
    for (int hole = 0; hole < holes; ++hole)
      clause.push_back(pigeon * holes + hole + 1);
    add(clause);
  }
  for (int hole = 0; hole < holes; ++hole)
    for (int first = 0; first < pigeons; ++first)
      for (int second = first + 1; second < pigeons; ++second)
        add({-(first * holes + hole + 1), -(second * holes + hole + 1)});
  finish_input(kernel);
  DirectPhpActionSession session(kernel);
  const auto p = DirectPhpRefutationParameters{};
  const DirectPhpActionLimits limits{
      {p.maximum_snapshot_clauses,p.maximum_snapshot_literals,p.maximum_snapshot_work},
      {p.maximum_recognition_clauses,p.maximum_recognition_literals,p.maximum_recognition_work,
       p.maximum_row_candidates,p.maximum_row_combinations,p.maximum_mapping_trials,p.maximum_holes},
      {p.maximum_proof_additions,p.maximum_proof_deletions,p.maximum_proof_literals,
       p.maximum_fresh_variables,p.maximum_proof_events},p.maximum_replay_work};
  const auto result = direct_php_action_apply(kernel, session, {1,1,limits});
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
  char path[] = "/tmp/tactix-direct-php-public-XXXXXX";
  const int fd = mkstemp(path);
  assert(fd >= 0);
  close(fd);
  assert(tactix_proof(solver, path, TACTIX_PROOF_DSR_ASCII) == 0);
  const int holes = 3, pigeons = 4;
  for (int pigeon = 0; pigeon < pigeons; ++pigeon) {
    std::vector<int> clause;
    for (int hole = 0; hole < holes; ++hole)
      clause.push_back(pigeon * holes + hole + 1);
    add_c_clause(solver, clause);
  }
  for (int hole = 0; hole < holes; ++hole)
    for (int first = 0; first < pigeons; ++first)
      for (int second = first + 1; second < pigeons; ++second)
        add_c_clause(solver, {-(first * holes + hole + 1),
                              -(second * holes + hole + 1)});

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
  unlink(path);
}

} // namespace

int main() {
  identity_guard_and_disabled_default();
  serialized_replay_and_terminal_feedback();
  allowance_declines_before_action();
  duplicate_binary_snapshot_is_not_invalid();
  random_generator_reaches_the_same_action();
  parser_surface_has_stable_source_ids();
  c_api_executes_terminal_action();
}
