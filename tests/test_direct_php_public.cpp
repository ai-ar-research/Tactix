#undef NDEBUG
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix

#include "direct_php_test_support.hpp"
#include "dimacs_text.hpp"
#include "temporary_file.hpp"
#include "test_io.hpp"
#include "frontend/parse.hpp"
#include "proof/proof.hpp"
#include "rules/direct_php/direct_php_action.hpp"
#include "rules/registry.hpp"
#include "strategy/online_inprocess.hpp"
#include "strategy/controller_observation.hpp"

#include <cassert>
#include <algorithm>
#include <cstdio>
#include <set>
#include <string>
#include <type_traits>
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

static_assert(!std::is_copy_constructible_v<DirectPhpActionSession>);
static_assert(!std::is_move_constructible_v<DirectPhpActionSession>);

DirectPhpActionRequest request(uint64_t id = 37) {
  DirectPhpActionRequest request;
  request.action_id = id;
  ample_php_limits(request.limits);
  request.limits.maximum_replay_work = 100000000;
  assert(request.limits.valid());
  return request;
}

// The committed record recounts to the reported proof accounting, and the
// session answers every later request without touching the kernel.
void apply_accounts_and_commits_once() {
  Kernel kernel = fixture();
  const auto internal_to_external = kernel.internal_to_external;
  const auto reconstruction = kernel.reconstruction;
  const auto arena = kernel.arena;
  const uint32_t nvars = kernel.nvars;
  const size_t input_extent = kernel.external_to_internal.size();
  const uint64_t input_ids = kernel.next_clause_id;
  DirectPhpActionSession session(kernel);
  const auto applied = direct_php_action_apply(kernel, session, request());
  assert(applied.status == DirectPhpActionStatus::Applied);
  assert(applied.result == ApplyResult::Unsat && kernel.inconsistent);
  assert(applied.action_id == 37 && session.committed());
  assert(applied.effort.snapshot_clauses == 22 &&
         applied.effort.snapshot_literals == 48);
  assert(applied.effort.snapshot_work && applied.effort.recognition_work &&
         applied.effort.replay_work);
  const DirectPhpActionRecord &record = *session.record();
  assert(record.version == 1 && record.action_id == 37);
  assert(record.snapshot.sources.size() == 22 && record.first_fresh == 13);
  assert(record.events.size() == record.accounting.events);
  assert(record.accounting == applied.proof);
  // Sources carry distinct input proof ids; additions take fresh consecutive
  // ids and every retired original AMO names its own source's id.
  std::set<uint64_t> source_ids;
  for (const auto &source : record.snapshot.sources)
    assert(source.native_proof_id >= 1 && source.native_proof_id <= input_ids &&
           source_ids.insert(source.native_proof_id).second);
  uint64_t additions = 0, rup = 0, deletions = 0, retired = 0;
  uint64_t addition_literals = 0, deletion_literals = 0, maximum_width = 0;
  for (const auto &event : record.events) {
    maximum_width = std::max<uint64_t>(maximum_width, event.clause.size());
    if (event.kind == direct_php_certificate::EventKind::Delete) {
      ++deletions;
      deletion_literals += event.clause.size();
      assert(event.deleted_id && !event.id);
      if (event.role == direct_php_certificate::EventRole::RetireOriginalAmo) {
        ++retired;
        assert(record.snapshot.sources.at(event.source_occurrence - 1)
                   .native_proof_id == event.deleted_id);
      }
    } else {
      ++additions;
      addition_literals += event.clause.size();
      assert(event.id == input_ids + additions && !event.deleted_id &&
             !event.source_occurrence);
      rup += event.kind == direct_php_certificate::EventKind::AddRup;
    }
  }
  assert(retired == 18 && record.events.back().clause.empty());
  // A proofless commit only reserves the ids and fresh names it used.
  assert(kernel.nvars == nvars && kernel.arena == arena &&
         kernel.internal_to_external == internal_to_external &&
         kernel.reconstruction == reconstruction);
  assert(kernel.next_clause_id == input_ids + additions);
  assert(kernel.external_to_internal.size() ==
         input_extent + applied.proof.fresh_variables);
  for (size_t i = input_extent; i < kernel.external_to_internal.size(); ++i)
    assert(kernel.external_to_internal[i] == INVALID_VAR);
  assert(additions == applied.proof.additions &&
         rup == applied.proof.rup_additions &&
         additions - rup == applied.proof.rat_additions &&
         deletions == applied.proof.deletions &&
         addition_literals == applied.proof.addition_literals &&
         deletion_literals == applied.proof.deletion_literals &&
         maximum_width == applied.proof.maximum_event_width);

  const uint64_t next_id = kernel.next_clause_id;
  const size_t extent = kernel.external_to_internal.size();
  const auto repeated = direct_php_action_apply(kernel, session, request());
  assert(repeated.status == DirectPhpActionStatus::AlreadyApplied);
  assert(repeated.result == ApplyResult::NoOp &&
         repeated.proof == applied.proof);
  assert(direct_php_action_apply(kernel, session, request(38)).status ==
         DirectPhpActionStatus::ReplayMismatch);
  auto different_limits = request();
  --different_limits.limits.maximum_replay_work;
  assert(direct_php_action_apply(kernel, session, different_limits).status ==
         DirectPhpActionStatus::ReplayMismatch);
  assert(kernel.next_clause_id == next_id &&
         kernel.external_to_internal.size() == extent);

  Kernel other = fixture();
  assert(direct_php_action_apply(other, session, request()).status ==
         DirectPhpActionStatus::Stale);
  assert(!other.inconsistent);
}

// Every decline before publication leaves the session and kernel untouched.
void declines_are_atomic() {
  uint64_t replay_work = 0;
  {
    Kernel kernel = fixture();
    DirectPhpActionSession session(kernel);
    replay_work = direct_php_action_apply(kernel, session, request())
                      .effort.replay_work;
  }
  const auto declines = [](Kernel &kernel, const DirectPhpActionRequest &r,
                           DirectPhpActionStatus expected) {
    const uint64_t next_id = kernel.next_clause_id;
    DirectPhpActionSession session(kernel);
    const auto result = direct_php_action_apply(kernel, session, r);
    assert(result.status == expected && result.result == ApplyResult::NoOp);
    assert(!session.committed() && !kernel.inconsistent &&
           kernel.next_clause_id == next_id);
    return result;
  };
  {
    // Building the record needs its whole replay budget.
    Kernel kernel = fixture();
    auto short_budget = request();
    short_budget.limits.maximum_replay_work = replay_work - 1;
    const auto exhausted = declines(kernel, short_budget,
                                    DirectPhpActionStatus::ResourceExhausted);
    assert(exhausted.effort.replay_work <= replay_work - 1 &&
           exhausted.proof.events);
  }
  {
    Kernel kernel = fixture();
    auto small_snapshot = request();
    small_snapshot.limits.snapshot.maximum_clauses = 1;
    declines(kernel, small_snapshot, DirectPhpActionStatus::ResourceExhausted);
  }
  {
    Kernel kernel = fixture();
    auto invalid = request();
    invalid.action_id = 0;
    declines(kernel, invalid, DirectPhpActionStatus::Invalid);
  }
  {
    Kernel kernel = fixture();
    Proof proof;
    TemporaryFile file;
    assert(proof.open(file.path.c_str(), ProofFormat::PbpStreaming));
    proof.finish_input_transaction();
    assert(proof.begin_pb());
    kernel.proof = &proof;
    declines(kernel, request(), DirectPhpActionStatus::UnsupportedProof);
    kernel.proof = nullptr;
  }
  {
    Kernel unrelated;
    kernel_reserve_vars(unrelated, 3);
    const Lit clause[] = {internalize(1), internalize(2), internalize(3)};
    kernel_add_large_clause(unrelated, clause, 3, false, 0,
                            ++unrelated.next_clause_id);
    declines(unrelated, request(), DirectPhpActionStatus::NotRecognized);
  }
  Kernel kernel = fixture();
  assert(direct_php_action_guard(kernel));
  OccIndex dense;
  kernel.dense = &dense;
  assert(!direct_php_action_guard(kernel));
  declines(kernel, request(), DirectPhpActionStatus::Inapplicable);
}

// A stop at any single poll, up to publish's final gate and recapture,
// declines without committing. Poll i alone fires: the fuzz state is rewound
// i generator steps from the firing state 0.
void every_interruption_point_declines() {
  constexpr uint64_t a = 6364136223846793005ull, c = 1442695040888963407ull;
  uint64_t inverse = a; // Newton iteration: a * inverse == 1 (mod 2^64)
  for (int i = 0; i < 5; ++i) inverse *= 2 - a * inverse;
  assert(a * inverse == 1);
  const auto polls_from = [&](uint64_t seed, uint64_t state) {
    uint64_t polls = 0;
    for (; seed != state; ++polls) {
      seed = seed * a + c;
      assert(polls < 100000000);
    }
    return polls;
  };
  const uint64_t seed = 0x5851f42d4c957f2dull;
  uint64_t total = 0, per_capture = 0, replay_work = 0;
  {
    Kernel kernel = fixture();
    kernel.stop_fuzz = UINT32_MAX;
    kernel.stop_fuzz_state = seed;
    direct_php_live::Snapshot snapshot;
    assert(direct_php_live::detail::capture(kernel, request().limits.snapshot,
                                            snapshot) ==
           direct_php_live::detail::CaptureStatus::Complete);
    per_capture = polls_from(seed, kernel.stop_fuzz_state);
    kernel.stop_fuzz_state = seed;
    DirectPhpActionSession session(kernel);
    const auto applied = direct_php_action_apply(kernel, session, request());
    assert(applied.status == DirectPhpActionStatus::Applied &&
           kernel.stats.stop_fuzz_fires == 0);
    total = polls_from(seed, kernel.stop_fuzz_state);
    replay_work = applied.effort.replay_work;
  }
  assert(per_capture && total > per_capture);
  uint64_t after_replay = 0;
  for (uint64_t poll = 1; poll <= total; ++poll) {
    Kernel kernel = fixture();
    const uint64_t next_id = kernel.next_clause_id;
    const size_t extent = kernel.external_to_internal.size();
    uint64_t state = 0;
    for (uint64_t i = 0; i < poll; ++i) state = (state - c) * inverse;
    kernel.stop_fuzz = UINT32_MAX;
    kernel.stop_fuzz_state = state;
    DirectPhpActionSession session(kernel);
    const auto result = direct_php_action_apply(kernel, session, request());
    assert(result.status == DirectPhpActionStatus::Interrupted &&
           result.result == ApplyResult::NoOp);
    assert(kernel.stats.stop_fuzz_fires == 1);
    assert(!session.committed() && !kernel.inconsistent &&
           kernel.next_clause_id == next_id &&
           kernel.external_to_internal.size() == extent);
    after_replay += result.effort.replay_work == replay_work;
  }
  // Publish polls its gate once and then recaptures the formula.
  assert(after_replay == per_capture + 1);
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

// With `directory`, php.cnf and the run's php.dsr are left there for the
// external DSR checker.
void c_api_executes_terminal_action(const char *directory) {
  tactix_c_handle *solver = tactix_init();
  TemporaryFile file;
  std::string proof_path = file.path;
  if (directory) {
    write_file((std::string(directory) + "/php.cnf").c_str(),
               dimacs_text(12, php_clauses(3)));
    proof_path = std::string(directory) + "/php.dsr";
  }
  assert(tactix_proof(solver, proof_path.c_str(), TACTIX_PROOF_DSR_ASCII) == 0);
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

int main(int argc, char **argv) {
  assert(argc <= 2);
  apply_accounts_and_commits_once();
  declines_are_atomic();
  every_interruption_point_declines();
  identity_guard_and_disabled_default();
  explicit_program_reaches_the_same_action();
  allowance_declines_before_action();
  duplicate_binary_snapshot_is_not_invalid();
  parser_surface_has_stable_source_ids();
  c_api_executes_terminal_action(argc == 2 ? argv[1] : nullptr);
  std::puts("test_direct_php_public: passed");
}
