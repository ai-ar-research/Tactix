#undef NDEBUG
#include <cstdint>
#include <cstdio>

namespace capi {
extern "C" {
#include "tactix.h"
}
} // namespace capi

#include "dimacs_text.hpp"
#include "policies/heuristics.hpp"
#include "rules/semantic_extension/affine_image.hpp"
#include "rules/semantic_extension/direct_csp_support_action.hpp"
#include "rules/semantic_extension/gf3_affine_action.hpp"
#include "strategy/controller_observation.hpp"
#include "strategy/online_inprocess.hpp"
#include "strategy/semantic_variable_advice.hpp"

#include <algorithm>
#include <cassert>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

using namespace tactix;
using namespace tactix::strategy;

namespace {

std::string gf3_dimacs() {
  std::vector<std::vector<int32_t>> clauses;
  std::vector<std::vector<int32_t>> values;
  for (int32_t domain = 0; domain < 7; ++domain) {
    const int32_t first = 3 * domain + 1;
    values.push_back({first, first + 1, first + 2});
    clauses.push_back(values.back());
    for (uint8_t left = 0; left < 3; ++left)
      for (uint8_t right = left + 1; right < 3; ++right)
        clauses.push_back({-values.back()[left], -values.back()[right]});
  }
  const auto equation = [&](const std::vector<uint32_t> &domains) {
    for (uint64_t tuple = 0; tuple < 27; ++tuple) {
      uint64_t rest = tuple;
      uint8_t sum = 0;
      std::vector<int32_t> clause;
      for (uint32_t domain : domains) {
        const uint8_t value = static_cast<uint8_t>(rest % 3);
        rest /= 3;
        sum = static_cast<uint8_t>((sum + value) % 3);
        clause.push_back(-values[domain][value]);
      }
      if (sum) clauses.push_back(std::move(clause));
    }
  };
  equation({0, 1, 2});
  equation({2, 3, 4});
  equation({4, 5, 6});
  return dimacs_text(21, clauses);
}

std::string direct_csp_dimacs() {
  const std::vector<std::vector<int32_t>> clauses = {
      {1, 2, 3},  {-1, -2}, {-1, -3}, {-2, -3},
      {4, 5, 6, 7}, {-4, -5}, {-4, -6}, {-4, -7}, {-5, -6},
      {-5, -7}, {-6, -7}, {-1, -7}, {-2, -4}, {-2, -6},
      {-2, -7}, {-3, -5}};
  return dimacs_text(7, clauses);
}

std::string affine_dimacs() {
  return dimacs_text(
      4, affine_semantic_extension::clausal_form({1, 2, 3, 4}, false));
}

Gf3AffineConsequenceParameters gf3_parameters() {
  return {2000, 20000, 100, 100, 6, 1000, 8, 10000, 1000, 10000,
          1000000, 1000000, 1000000, 1000000, 100000, 1000, 10000,
          10000, 1000000, 100, 100, 10000};
}

DirectCspSupportParameters direct_parameters(bool extension = true) {
  return {100, 1000, 100000, 16, extension ? 2u : 8u, 16,
          1000, 20, 100, 100000, extension ? 1u : 0u, 100,
          16, 10000, extension};
}

OnlineInvocation gf3_step() {
  OnlineInvocation step;
  step.id = 1;
  step.rule = "Gf3AffineConsequence";
  step.parameters = gf3_parameters();
  step.granularity = InvocationGranularity::SelectedBatch;
  step.gf3_affine_instances = {{0, 1}, {1, 2}};
  step.repeat = {RepeatMode::Once, 1};
  step.post_application = PostApplication::Propagate;
  step.budget = WorkLimit::bounded(2);
  return step;
}

OnlineInvocation direct_step() {
  OnlineInvocation step;
  step.id = 2;
  step.rule = "DirectCspSupport";
  step.parameters = direct_parameters();
  step.granularity = InvocationGranularity::SelectedBatch;
  step.direct_csp_support_instances = {
      {{1, 2, 3}, {4, 5, 6, 7}, 1},
      {{1, 2, 3}, {4, 5, 6, 7}, 0}};
  step.repeat = {RepeatMode::Once, 1};
  step.post_application = PostApplication::Propagate;
  step.budget = WorkLimit::bounded(2);
  return step;
}

AffineConsequenceParameters reference_affine_parameters() {
  AffineConsequenceParameters result;
  result.maximum_clauses = 10000;
  result.maximum_snapshot_literals = 100000;
  result.maximum_snapshot_work = 1000000;
  result.maximum_groups = 1000;
  result.maximum_patterns = 10000;
  result.maximum_elimination_work = 10000000;
  result.maximum_arity = 8;
  result.maximum_intermediate_width = 32;
  result.maximum_sources = 32;
  result.maximum_output_width = 4;
  result.maximum_proof_additions = 1000000;
  result.maximum_proof_deletions = 1000000;
  result.maximum_journal_bytes = 10000000;
  result.semantics = AffineConsequenceSemantics::CertifiedBasisV2;
  result.basis_maximum_work = 10000000;
  result.basis_maximum_intermediate_width = 32;
  result.basis_maximum_sources = 32;
  result.basis_maximum_premises = 10000;
  result.basis_maximum_nodes = 10000;
  result.basis_maximum_direct_width = 3;
  result.maximum_external_extent = 100000;
  result.maximum_session_keys = 10000;
  result.maximum_session_work = 1000000;
  return result;
}

OnlineInvocation affine_step() {
  OnlineInvocation step;
  step.id = 3;
  step.rule = "AffineConsequence";
  step.parameters = reference_affine_parameters();
  step.granularity = InvocationGranularity::CanonicalApplication;
  step.repeat = {RepeatMode::Once, 1};
  step.post_application = PostApplication::Propagate;
  step.budget = WorkLimit::bounded(1);
  return step;
}

// A parsed kernel with a provenance stream and an action session bound.
template <class Session> struct Bound {
  Kernel kernel;
  std::optional<ScopedHeuristics> heuristics;
  Session session;
  FILE *records = std::tmpfile();
  Bound(const std::string &dimacs, Session *Kernel::*slot, bool attach) {
    parse_dimacs_text(dimacs, kernel, ParsedRoot::EmptyTrail);
    if (attach) heuristics.emplace(kernel);
    assert(records);
    kernel.structural_provenance = records;
    kernel.*slot = &session;
  }
  ~Bound() { std::fclose(records); }
};

void consume_candidate_and_verify_guidance(
    Kernel &kernel, OnlineStrategyExecutor &executor,
    SemanticVariableProducer producer, uint64_t revision) {
  const auto candidates = semantic_variable_candidates(kernel);
  const auto info = std::find_if(
      candidates.begin(), candidates.end(), [producer](const auto &candidate) {
        return candidate.producer == producer;
      });
  assert(info != candidates.end());
  const auto *candidate = semantic_variable_candidate(kernel, info->id);
  assert(candidate && !candidate->variables.empty());
  const auto variables = candidate->variables;
  const auto scores_before = kernel.h->scores.score;

  const auto priority = [](SemanticVariableRole role) {
    return role == SemanticVariableRole::IntroducedTheory ? 5u : 2u;
  };
  const auto expected_last = std::max_element(
      variables.begin(), variables.end(), [&](const auto &left,
                                              const auto &right) {
        const uint32_t left_priority = priority(left.role);
        const uint32_t right_priority = priority(right.role);
        if (left_priority != right_priority)
          return left_priority < right_priority;
        return left.external > right.external;
      });
  assert(expected_last != variables.end());

  OnlineInvocation advice;
  advice.id = 100 + revision;
  advice.rule = "SemanticVariableAdvice";
  advice.parameters = SemanticVariableAdviceParameters{64, 4096, 2, 5};
  advice.granularity = InvocationGranularity::OneInstance;
  advice.instances = {info->id};
  advice.repeat = {RepeatMode::Once, 1};
  advice.budget = WorkLimit::bounded(1);
  OnlineStrategyProgram program;
  program.sequence = {advice};
  assert(executor.configure(program, revision, ReconfigureMode::ResetRuntime));
  const InvocationResult applied = executor.advance(kernel);
  assert(applied.status == InvocationStatus::Completed &&
         applied.made_rule_progress && applied.applications == 1 &&
         applied.selection_complete);
  assert(kernel.h->queue_last ==
         kernel.external_to_internal[var_of(
             internalize(static_cast<int>(expected_last->external)))]);
  for (const SemanticVariable &variable : variables) {
    const Var internal = kernel.external_to_internal[
        var_of(internalize(static_cast<int>(variable.external)))];
    assert(kernel.h->scores.score[internal] > scores_before[internal]);
  }
  assert(kernel.stats.semantic_variable_advice_applied == 1 &&
         kernel.stats.semantic_variable_advice_variables == variables.size());
  assert(semantic_variable_candidates(kernel).empty());
  assert(executor.advance(kernel).status == InvocationStatus::Completed &&
         executor.complete());
}

// A two-instance selected batch commits both instances, publishes one current
// candidate of `producer`, and its advice then guides the heuristics.
template <class Session, class Check>
void productive_batch(const std::string &dimacs, Session *Kernel::*slot,
                      const OnlineInvocation &step,
                      SemanticVariableProducer producer, Check check) {
  Bound<Session> bound(dimacs, slot, true);
  Kernel &kernel = bound.kernel;
  OnlineStrategyProgram program;
  program.sequence = {step};
  OnlineStrategyExecutor executor;
  assert(executor.configure(program, step.id));
  const InvocationResult first = executor.advance(kernel);
  assert(first.status == InvocationStatus::Completed &&
         first.instance == 0 && first.made_rule_progress &&
         !first.selection_complete);
  const uint64_t first_generation = kernel.arena_generation;
  const InvocationResult second = executor.advance(kernel);
  assert(second.status == InvocationStatus::Completed &&
         second.instance == 1 && second.made_rule_progress &&
         second.selection_complete &&
         kernel.arena_generation >= first_generation);
  check(bound.session, kernel.stats);
  const auto candidates = semantic_variable_candidates(kernel);
  assert(candidates.size() == 1 && candidates.front().producer == producer &&
         (candidates.front().id & 255) == static_cast<uint8_t>(producer) &&
         candidates.front().arena_generation == second.after.arena_generation &&
         candidates.front().touch_epoch == second.after.touch_epoch &&
         candidates.front().strategy_epoch == second.after.strategy_epoch);
  // The composed propagation boundary did not invalidate the candidate.
  assert(has_current_semantic_variable_candidate(kernel));
  assert(executor.advance(kernel).status == InvocationStatus::Completed &&
         executor.complete());
  consume_candidate_and_verify_guidance(kernel, executor, producer,
                                        100 + step.id);
}

void productive_batches_publish_distinct_candidates_and_refresh_generation() {
  productive_batch(gf3_dimacs(), &Kernel::gf3_affine_action_session, gf3_step(),
                   SemanticVariableProducer::Gf3Affine,
                   [](const Gf3AffineActionSession &session, const auto &stats) {
                     assert(session.committed_keys() == 2 &&
                            stats.gf3_affine_applied == 2 &&
                            stats.gf3_affine_session_commits == 2);
                   });
  productive_batch(
      direct_csp_dimacs(), &Kernel::direct_csp_support_action_session,
      direct_step(), SemanticVariableProducer::DirectCsp,
      [](const DirectCspSupportActionSession &session, const auto &stats) {
        assert(session.committed_keys() == 2 && session.retained_owners() == 1 &&
               stats.direct_csp_support_applied == 2 &&
               stats.direct_csp_support_session_commits == 2 &&
               stats.direct_csp_support_owner_commits == 1);
      });
}

void affine_followup_publishes_one_current_typed_candidate() {
  for (const bool stable : {false, true}) {
    Bound<AffineBasisActionSession> bound(
        affine_dimacs(), &Kernel::affine_basis_action_session, true);
    Kernel &kernel = bound.kernel;
    kernel.h->stable = stable;
    if (stable) heuristics_update_scores(kernel);

    OnlineStrategyProgram program;
    program.sequence = {affine_step()};
    OnlineStrategyExecutor executor;
    assert(executor.configure(program, stable ? 12 : 11));
    const InvocationResult applied = executor.advance(kernel);
    assert(applied.status == InvocationStatus::Completed &&
           applied.made_rule_progress && applied.selection_complete &&
           kernel_propagation_fixpoint(kernel) &&
           kernel.stats.affine_consequence_applied == 1 &&
           kernel.stats.semantic_variable_candidates_published == 1);

    const auto candidates = semantic_variable_candidates(kernel);
    assert(candidates.size() == 1 &&
           candidates.front().producer == SemanticVariableProducer::Affine &&
           candidates.front().existing_variables == 4 &&
           candidates.front().introduced_variables > 0 &&
           kernel.stats.semantic_variable_candidate_existing == 4 &&
           kernel.stats.semantic_variable_candidate_introduced ==
               candidates.front().introduced_variables &&
           candidates.front().arena_generation == applied.after.arena_generation &&
           candidates.front().touch_epoch == applied.after.touch_epoch &&
           candidates.front().strategy_epoch == applied.after.strategy_epoch);
    const auto *candidate =
        semantic_variable_candidate(kernel, candidates.front().id);
    assert(candidate &&
           candidate->variables.size() ==
               candidates.front().existing_variables +
                   candidates.front().introduced_variables);
    for (const auto &variable : candidate->variables) {
      const Var external =
          var_of(internalize(static_cast<int>(variable.external)));
      assert(external < kernel.external_to_internal.size());
      const Var internal = kernel.external_to_internal[external];
      assert(internal != INVALID_VAR && heuristics_var_active(kernel, internal) &&
             !kernel_root_value(kernel, make_lit(internal, false)));
    }

    SemanticVariableAdvice stale;
    assert(make_semantic_variable_advice(kernel, candidates.front().id, 2, 5,
                                         stale));
    assert(executor.advance(kernel).status == InvocationStatus::Completed &&
           executor.complete());
    consume_candidate_and_verify_guidance(
        kernel, executor, SemanticVariableProducer::Affine,
        stable ? 112 : 111);
    assert(commit_semantic_variable_advice(
               kernel, stale, SemanticVariableAdviceLimits{64, 4096})
               .status == SemanticVariableAdviceStatus::Stale);
    kernel_check_invariants(kernel);
  }
}

void invalid_is_consumed_and_resource_exhaustion_is_retryable() {
  {
    Bound<Gf3AffineActionSession> bound(
        gf3_dimacs(), &Kernel::gf3_affine_action_session, false);
    Kernel &kernel = bound.kernel;
    OnlineInvocation step = gf3_step();
    step.granularity = InvocationGranularity::OneInstance;
    step.gf3_affine_instances = {{100, 101}};
    step.budget = WorkLimit::bounded(1);
    OnlineStrategyProgram program;
    program.sequence = {step};
    OnlineStrategyExecutor executor;
    assert(executor.configure(program, 3));
    const InvocationResult invalid = executor.advance(kernel);
    assert(invalid.status == InvocationStatus::InvalidRequest &&
           invalid.applications == 1 && invalid.selection_complete &&
           executor.step_consumed(step.id) == 1 &&
           kernel.stats.gf3_affine_invalid == 1);
    assert(executor.advance(kernel).status == InvocationStatus::Completed &&
           executor.complete());
  }
  {
    Bound<DirectCspSupportActionSession> bound(
        direct_csp_dimacs(), &Kernel::direct_csp_support_action_session, false);
    Kernel &kernel = bound.kernel;
    OnlineInvocation step = direct_step();
    step.granularity = InvocationGranularity::OneInstance;
    step.direct_csp_support_instances.resize(1);
    step.budget = WorkLimit::bounded(2);
    auto limited = std::get<DirectCspSupportParameters>(step.parameters);
    limited.maximum_recognition_work = 1;
    step.parameters = limited;
    OnlineStrategyProgram program;
    program.sequence = {step};
    OnlineStrategyExecutor executor;
    assert(executor.configure(program, 4));
    const InvocationResult exhausted = executor.advance(kernel);
    assert(exhausted.status == InvocationStatus::ResourceExhausted &&
           exhausted.applications == 1 && !exhausted.selection_complete &&
           !bound.session.committed_keys() &&
           kernel.stats.direct_csp_support_resource_exhausted == 1);
    step.parameters = direct_parameters();
    program.sequence = {step};
    assert(executor.configure(program, 5, ReconfigureMode::ResetRuntime));
    const InvocationResult retried = executor.advance(kernel);
    assert(retried.status == InvocationStatus::Completed &&
           retried.made_rule_progress && retried.selection_complete &&
           bound.session.committed_keys() == 1 &&
           kernel.stats.direct_csp_support_applied == 1);
  }
}

int c_api_rule_id(const char *name) {
  for (int id = 0; id < capi::tactix_rule_count(); ++id)
    if (std::string(capi::tactix_rule_name(id)) == name) return id;
  return -1;
}

void load_c_api_dimacs(capi::tactix *solver, const std::string &dimacs) {
  std::istringstream input(dimacs);
  std::string marker;
  uint32_t variables = 0, clauses = 0;
  assert(input >> marker && marker == "p");
  assert(input >> marker && marker == "cnf");
  assert(input >> variables >> clauses);
  (void)variables;
  uint32_t ended = 0;
  int literal = 0;
  while (input >> literal) {
    capi::tactix_add(solver, literal);
    ended += literal == 0;
  }
  assert(ended == clauses);
}

// The v5 step's one-instance online base for `rule`.
capi::tactix_online_step &c_api_base(capi::tactix_online_step_v5 &step,
                                     uint64_t id, const char *rule) {
  capi::tactix_online_step &base = step.base.base.base.base;
  base.step_id = id;
  base.rule_id = c_api_rule_id(rule);
  assert(base.rule_id >= 0);
  base.granularity = TACTIX_ONLINE_GRANULARITY_ONE;
  base.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  base.repeat_limit = 1;
  base.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  base.budget = 1;
  base.post_application = TACTIX_ONLINE_POST_PROPAGATE;
  return base;
}

// One productive application of `step` through the public C API.
void c_api_productive(const std::string &dimacs,
                      const capi::tactix_online_step_v5 &step) {
  capi::tactix *solver = capi::tactix_init();
  FILE *records = std::tmpfile();
  assert(solver && records);
  load_c_api_dimacs(solver, dimacs);
  assert(capi::tactix_bind_structural_provenance(solver, records) == 0);
  const capi::tactix_online_program_v5 program{
      &step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(capi::tactix_online_configure_v5(
             solver, &program, step.base.base.base.base.step_id,
             TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  capi::tactix_online_result result{};
  assert(capi::tactix_online_advance(solver, &result) == 0 &&
         result.status == TACTIX_ONLINE_COMPLETED &&
         result.made_rule_progress && result.applications == 1 &&
         result.selection_complete);
  assert(capi::tactix_online_advance(solver, &result) == 0 &&
         capi::tactix_online_complete(solver));
  capi::tactix_release(solver);
  std::fclose(records);
}

void public_c_api_executes_productive_v5_actions() {
  {
    capi::tactix_online_step_v5 step{};
    capi::tactix_online_step &base = c_api_base(step, 201, "Gf3AffineConsequence");
    base.parameters.kind = TACTIX_ONLINE_PARAMETERS_GF3_AFFINE_CONSEQUENCE;
    base.parameters.value.gf3_affine_consequence = {
        2000, 20000, 100, 100, 6, 1000, 8, 10000, 1000, 10000,
        1000000, 1000000, 1000000, 1000000, 100000, 1000,
        10000, 10000, 1000000, 100, 100, 10000};
    const capi::tactix_online_gf3_affine_instance selected{0, 1};
    step.gf3_affine_instances = &selected;
    step.gf3_affine_instance_count = 1;
    c_api_productive(gf3_dimacs(), step);
  }
  {
    capi::tactix_online_step_v5 step{};
    capi::tactix_online_step &base = c_api_base(step, 202, "DirectCspSupport");
    base.parameters.kind = TACTIX_ONLINE_PARAMETERS_DIRECT_CSP_SUPPORT;
    base.parameters.value.direct_csp_support = {
        100, 1000, 100000, 16, 2, 16, 1000, 20, 100, 100000, 1, 100, 16, 10000, 1};
    const int left[] = {1, 2, 3};
    const int right[] = {4, 5, 6, 7};
    const capi::tactix_online_direct_csp_support_instance selected{
        left, 3, right, 4, 1};
    step.direct_csp_support_instances = &selected;
    step.direct_csp_support_instance_count = 1;
    c_api_productive(direct_csp_dimacs(), step);
  }
}
} // namespace

int main() {
  productive_batches_publish_distinct_candidates_and_refresh_generation();
  affine_followup_publishes_one_current_typed_candidate();
  invalid_is_consumed_and_resource_exhaustion_is_retryable();
  public_c_api_executes_productive_v5_actions();
  std::puts("v5 theory action runtime PASS");
}
