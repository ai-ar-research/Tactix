#undef NDEBUG
#include <cstdint>
#include <cstdio>

namespace capi {
extern "C" {
#include "tactix.h"
}
} // namespace capi

#include "policies/heuristics.hpp"
#include "frontend/parse.hpp"
#include "policies/random_controller_policy.hpp"
#include "rules/semantic_extension/direct_csp_support_action.hpp"
#include "rules/semantic_extension/gf3_affine_action.hpp"
#include "strategy/controller_observation.hpp"
#include "strategy/online_inprocess.hpp"
#include "strategy/semantic_variable_advice.hpp"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>

using namespace tactix;
using namespace tactix::strategy;

namespace {

Kernel parse_text(const std::string &text) {
  FILE *input = std::tmpfile();
  assert(input);
  assert(std::fwrite(text.data(), 1, text.size(), input) == text.size());
  std::rewind(input);
  Kernel kernel;
  assert(parse_dimacs(input, kernel) == nullptr);
  std::fclose(input);
  assert(kernel.trail.empty() && kernel_propagation_fixpoint(kernel));
  return kernel;
}

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
  std::ostringstream output;
  output << "p cnf 21 " << clauses.size() << '\n';
  for (const auto &clause : clauses) {
    for (int32_t literal : clause) output << literal << ' ';
    output << "0\n";
  }
  return output.str();
}

std::string direct_csp_dimacs() {
  const std::vector<std::vector<int32_t>> clauses = {
      {1, 2, 3},  {-1, -2}, {-1, -3}, {-2, -3},
      {4, 5, 6, 7}, {-4, -5}, {-4, -6}, {-4, -7}, {-5, -6},
      {-5, -7}, {-6, -7}, {-1, -7}, {-2, -4}, {-2, -6},
      {-2, -7}, {-3, -5}};
  std::ostringstream output;
  output << "p cnf 7 " << clauses.size() << '\n';
  for (const auto &clause : clauses) {
    for (int32_t literal : clause) output << literal << ' ';
    output << "0\n";
  }
  return output.str();
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

void productive_batches_publish_distinct_candidates_and_refresh_generation() {
  {
    Kernel kernel = parse_text(gf3_dimacs());
    ScopedHeuristics heuristics(kernel);
    FILE *records = std::tmpfile();
    assert(records);
    Gf3AffineActionSession session;
    kernel.structural_provenance = records;
    kernel.gf3_affine_action_session = &session;
    OnlineStrategyProgram program;
    program.sequence = {gf3_step()};
    OnlineStrategyExecutor executor;
    assert(executor.configure(program, 1));
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
    assert(session.committed_keys() == 2 &&
           kernel.stats.gf3_affine_applied == 2 &&
           kernel.stats.gf3_affine_session_commits == 2);
    const auto candidates = semantic_variable_candidates(kernel);
    assert(candidates.size() == 1 &&
           candidates.front().producer == SemanticVariableProducer::Gf3Affine &&
           (candidates.front().id & 255) ==
               static_cast<uint8_t>(SemanticVariableProducer::Gf3Affine) &&
           candidates.front().arena_generation ==
               second.after.arena_generation &&
           candidates.front().touch_epoch == second.after.touch_epoch &&
           candidates.front().strategy_epoch == second.after.strategy_epoch);
    assert(executor.advance(kernel).status == InvocationStatus::Completed &&
           executor.complete());
    consume_candidate_and_verify_guidance(
        kernel, executor, SemanticVariableProducer::Gf3Affine, 101);
    std::fclose(records);
  }
  {
    Kernel kernel = parse_text(direct_csp_dimacs());
    ScopedHeuristics heuristics(kernel);
    FILE *records = std::tmpfile();
    assert(records);
    DirectCspSupportActionSession session;
    kernel.structural_provenance = records;
    kernel.direct_csp_support_action_session = &session;
    OnlineStrategyProgram program;
    program.sequence = {direct_step()};
    OnlineStrategyExecutor executor;
    assert(executor.configure(program, 2));
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
    assert(session.committed_keys() == 2 && session.retained_owners() == 1 &&
           kernel.stats.direct_csp_support_applied == 2 &&
           kernel.stats.direct_csp_support_session_commits == 2 &&
           kernel.stats.direct_csp_support_owner_commits == 1);
    const auto candidates = semantic_variable_candidates(kernel);
    assert(candidates.size() == 1 &&
           candidates.front().producer == SemanticVariableProducer::DirectCsp &&
           (candidates.front().id & 255) ==
               static_cast<uint8_t>(SemanticVariableProducer::DirectCsp) &&
           candidates.front().arena_generation ==
               second.after.arena_generation &&
           candidates.front().touch_epoch == second.after.touch_epoch &&
           candidates.front().strategy_epoch == second.after.strategy_epoch);
    // The composed propagation boundary did not invalidate the candidate.
    assert(has_current_semantic_variable_candidate(kernel));
    assert(executor.advance(kernel).status == InvocationStatus::Completed &&
           executor.complete());
    consume_candidate_and_verify_guidance(
        kernel, executor, SemanticVariableProducer::DirectCsp, 102);
    std::fclose(records);
  }
}

void invalid_is_consumed_and_resource_exhaustion_is_retryable() {
  {
    Kernel kernel = parse_text(gf3_dimacs());
    FILE *records = std::tmpfile();
    assert(records);
    Gf3AffineActionSession session;
    kernel.structural_provenance = records;
    kernel.gf3_affine_action_session = &session;
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
    std::fclose(records);
  }
  {
    Kernel kernel = parse_text(direct_csp_dimacs());
    FILE *records = std::tmpfile();
    assert(records);
    DirectCspSupportActionSession session;
    kernel.structural_provenance = records;
    kernel.direct_csp_support_action_session = &session;
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
           !session.committed_keys() &&
           kernel.stats.direct_csp_support_resource_exhausted == 1);
    step.parameters = direct_parameters();
    program.sequence = {step};
    assert(executor.configure(program, 5, ReconfigureMode::ResetRuntime));
    const InvocationResult retried = executor.advance(kernel);
    assert(retried.status == InvocationStatus::Completed &&
           retried.made_rule_progress && retried.selection_complete &&
           session.committed_keys() == 1 &&
           kernel.stats.direct_csp_support_applied == 1);
    std::fclose(records);
  }
}

void offered_random_action_is_exact_and_deterministic() {
  Kernel kernel = parse_text(direct_csp_dimacs());
  FILE *records = std::tmpfile();
  assert(records);
  DirectCspSupportActionSession session;
  kernel.structural_provenance = records;
  kernel.direct_csp_support_action_session = &session;
  OnlineInvocation offered = direct_step();
  offered.granularity = InvocationGranularity::OneInstance;
  offered.direct_csp_support_instances.resize(1);
  offered.budget = WorkLimit::bounded(1);
  OnlineStrategyProgram collection;
  collection.sequence = {offered};
  RandomControllerConfig config;
  std::string error;
  assert(config.configure_actions(collection, &error));
  const auto observation = controller::observe_kernel(kernel);
  bool selected = false;
  for (uint64_t seed = 1; seed < 64 && !selected; ++seed) {
    RandomControllerSelector first(seed), second(seed);
    controller::ActionRequest a, b;
    assert(first.choose(kernel, observation, config, a, error));
    assert(second.choose(kernel, observation, config, b, error));
    assert(a.type == b.type);
    if (a.type != controller::ActionType::Rule) continue;
    selected = true;
    assert(a.invocation.rule == "DirectCspSupport" &&
           b.invocation.rule == "DirectCspSupport" &&
           a.invocation.direct_csp_support_instances.size() == 1 &&
           b.invocation.direct_csp_support_instances.size() == 1 &&
           a.invocation.direct_csp_support_instances[0].left_values ==
               offered.direct_csp_support_instances[0].left_values &&
           a.invocation.direct_csp_support_instances[0].selected_left_value ==
               offered.direct_csp_support_instances[0].selected_left_value &&
           serialize_online_strategy_program({{a.invocation}, {}}) ==
               serialize_online_strategy_program({{b.invocation}, {}}));
  }
  assert(selected);
  std::fclose(records);
}

RandomControllerConfig dynamic_gf3_config() {
  RandomControllerConfig config;
  config.parameters.rules = {"Gf3AffineConsequence"};
  config.parameters.gf3_affine_consequence_reference = gf3_parameters();
  config.gf3_action_producer.enabled = true;
  config.gf3_action_producer.maximum_pair_candidates = 1;
  config.gf3_action_producer.maximum_offers = 1;
  config.gf3_action_producer.maximum_enumeration_work = 100000;
  return config;
}

RandomControllerConfig dynamic_direct_config() {
  RandomControllerConfig config;
  config.parameters.rules = {"DirectCspSupport"};
  config.parameters.direct_csp_support_reference = direct_parameters();
  config.direct_csp_action_producer.enabled = true;
  config.direct_csp_action_producer.maximum_domain_candidates = 16;
  config.direct_csp_action_producer.maximum_pair_candidates = 1;
  config.direct_csp_action_producer.maximum_row_candidates = 1;
  config.direct_csp_action_producer.maximum_offers = 1;
  config.direct_csp_action_producer.maximum_enumeration_work = 100000;
  return config;
}

class ResourceThenSearchSelection final : public ControllerSelection {
public:
  explicit ResourceThenSearchSelection(OnlineInvocation invocation)
      : invocation_(std::move(invocation)) {}

  bool choose(Kernel &kernel, const controller::Observation &observation,
              const RandomControllerConfig &, controller::ActionRequest &request,
              std::string &, volatile sig_atomic_t *) override {
    request = {};
    request.sequence = observation.sequence + 1;
    request.expected = state_version(kernel);
    if (!issued_) {
      issued_ = true;
      request.type = controller::ActionType::Rule;
      request.invocation = invocation_;
    } else {
      request.type = controller::ActionType::ResumeSearch;
      request.search = controller::search_configurations().front();
    }
    return true;
  }

private:
  OnlineInvocation invocation_;
  bool issued_ = false;
};

void disabled_dynamic_producers_preserve_configuration_and_selection() {
  RandomControllerConfig baseline;
  baseline.parameters.rules = {"Rephase"};
  auto disabled = baseline;
  disabled.gf3_action_producer.maximum_pair_candidates = 1;
  disabled.gf3_action_producer.maximum_offers = 1;
  disabled.direct_csp_action_producer.maximum_domain_candidates = 1;
  disabled.direct_csp_action_producer.maximum_row_candidates = 1;
  assert(controller_configuration(baseline) ==
         controller_configuration(disabled));

  Kernel kernel = parse_text("p cnf 2 1\n1 2 0\n");
  const auto observation = controller::observe_kernel(kernel);
  for (uint64_t seed = 1; seed != 32; ++seed) {
    RandomControllerSelector first(seed), second(seed);
    controller::ActionRequest left, right;
    std::string error;
    assert(first.choose(kernel, observation, baseline, left, error));
    assert(second.choose(kernel, observation, disabled, right, error));
    assert(left.type == right.type && left.search == right.search);
    if (left.type == controller::ActionType::Rule)
      assert(serialize_online_strategy_program({{left.invocation}, {}}) ==
             serialize_online_strategy_program({{right.invocation}, {}}));
  }
}

void static_structured_offer_precedes_native_discovery() {
  Kernel kernel = parse_text(direct_csp_dimacs());
  FILE *records = std::tmpfile();
  assert(records);
  DirectCspSupportActionSession session;
  kernel.structural_provenance = records;
  kernel.direct_csp_support_action_session = &session;

  auto config = dynamic_direct_config();
  OnlineInvocation exact = direct_step();
  exact.granularity = InvocationGranularity::OneInstance;
  exact.direct_csp_support_instances = {
      exact.direct_csp_support_instances.back()};
  exact.budget = WorkLimit::bounded(1);
  config.offered_actions[exact.rule] = {exact};
  // Even a malformed unused producer window cannot replace or reject the
  // caller's explicit exact payload.
  config.direct_csp_action_producer.maximum_offers = 0;

  bool selected = false;
  for (uint64_t seed = 1; seed != 128 && !selected; ++seed) {
    RandomControllerSelector selector(seed);
    controller::ActionRequest request;
    std::string error;
    assert(selector.choose(kernel, controller::observe_kernel(kernel), config,
                           request, error));
    if (request.type != controller::ActionType::Rule)
      continue;
    selected = true;
    assert(request.invocation.direct_csp_support_instances ==
           exact.direct_csp_support_instances);
  }
  assert(selected && !session.retained());
  std::fclose(records);
}

void dynamic_theory_offers_are_exact_deterministic_and_applicable() {
  {
    Kernel kernel = parse_text(gf3_dimacs());
    FILE *records = std::tmpfile();
    assert(records);
    Gf3AffineActionSession session;
    kernel.structural_provenance = records;
    kernel.gf3_affine_action_session = &session;
    const auto config = dynamic_gf3_config();
    const auto observation = controller::observe_kernel(kernel);
    bool selected = false;
    for (uint64_t seed = 1; seed != 128 && !selected; ++seed) {
      RandomControllerSelector first(seed), second(seed);
      controller::ActionRequest left, right;
      std::string error;
      assert(first.choose(kernel, observation, config, left, error));
      assert(second.choose(kernel, observation, config, right, error));
      assert(left.type == right.type);
      if (left.type != controller::ActionType::Rule)
        continue;
      selected = true;
      assert(left.invocation.rule == "Gf3AffineConsequence" &&
             left.invocation.granularity == InvocationGranularity::OneInstance &&
             left.invocation.gf3_affine_instances ==
                 std::vector<Gf3AffineInstance>({{0, 1}}) &&
             serialize_online_strategy_program({{left.invocation}, {}}) ==
                 serialize_online_strategy_program({{right.invocation}, {}}));
      controller::RestartController controller(kernel);
      left.sequence = controller.observe().sequence + 1;
      left.expected = state_version(kernel);
      const auto applied =
          controller.apply_rule(left, config.allowance(left.invocation.rule));
      assert(applied.invocation.status == InvocationStatus::Completed &&
             applied.invocation.made_rule_progress &&
             session.committed_keys() == 1);
    }
    assert(selected);
    std::fclose(records);
  }
  {
    Kernel kernel = parse_text(direct_csp_dimacs());
    FILE *records = std::tmpfile();
    assert(records);
    DirectCspSupportActionSession session;
    kernel.structural_provenance = records;
    kernel.direct_csp_support_action_session = &session;
    const auto config = dynamic_direct_config();
    const auto observation = controller::observe_kernel(kernel);
    bool selected = false;
    for (uint64_t seed = 1; seed != 128 && !selected; ++seed) {
      RandomControllerSelector first(seed), second(seed);
      controller::ActionRequest left, right;
      std::string error;
      assert(first.choose(kernel, observation, config, left, error));
      assert(second.choose(kernel, observation, config, right, error));
      assert(left.type == right.type);
      if (left.type != controller::ActionType::Rule)
        continue;
      selected = true;
      assert(left.invocation.rule == "DirectCspSupport" &&
             left.invocation.granularity == InvocationGranularity::OneInstance &&
             left.invocation.direct_csp_support_instances ==
                 std::vector<DirectCspSupportActionSelection>(
                     {{{1, 2, 3}, {4, 5, 6, 7}, 0}}) &&
             serialize_online_strategy_program({{left.invocation}, {}}) ==
                 serialize_online_strategy_program({{right.invocation}, {}}));
      controller::RestartController controller(kernel);
      left.sequence = controller.observe().sequence + 1;
      left.expected = state_version(kernel);
      const auto applied =
          controller.apply_rule(left, config.allowance(left.invocation.rule));
      assert(applied.invocation.status == InvocationStatus::Completed &&
             applied.invocation.made_rule_progress &&
             session.committed_keys() == 1);
    }
    assert(selected);
    std::fclose(records);
  }
}

void dynamic_gf3_pagination_reaches_terminal_without_reoffering() {
  Kernel kernel = parse_text(gf3_dimacs());
  FILE *records = std::tmpfile();
  assert(records);
  Gf3AffineActionSession session;
  kernel.structural_provenance = records;
  kernel.gf3_affine_action_session = &session;
  const auto config = dynamic_gf3_config();

  RandomControllerSelector selector(1);
  auto observation = controller::observe_kernel(kernel);
  std::vector<Gf3AffineInstance> selected;
  std::string error;
  for (uint32_t attempt = 0; attempt != 128 && selected.size() != 2;
       ++attempt) {
    controller::ActionRequest request;
    assert(selector.choose(kernel, observation, config, request, error));
    if (request.type == controller::ActionType::Rule) {
      selected.push_back(request.invocation.gf3_affine_instances.front());
      controller::ActionResult settled;
      settled.sequence = request.sequence;
      settled.type = request.type;
      settled.invocation.status = InvocationStatus::Completed;
      settled.invocation.selection_complete = true;
      settled.invocation.after = state_version(kernel);
      observation.last_action = settled;
    } else {
      observation.last_action.reset();
    }
    ++observation.sequence; // model a settled same-state no-op request
  }
  assert(selected == std::vector<Gf3AffineInstance>({{0, 1}, {1, 2}}));
  for (uint32_t repeat = 0; repeat != 16; ++repeat) {
    controller::ActionRequest terminal;
    assert(selector.choose(kernel, observation, config, terminal, error) &&
           terminal.type == controller::ActionType::ResumeSearch);
    ++observation.sequence;
  }
  assert(!session.retained());
  std::fclose(records);
}

void native_execution_outcomes_obey_pending_lifecycle() {
  Kernel kernel = parse_text(gf3_dimacs());
  FILE *records = std::tmpfile();
  assert(records);
  Gf3AffineActionSession session;
  kernel.structural_provenance = records;
  kernel.gf3_affine_action_session = &session;
  const auto config = dynamic_gf3_config();
  controller::RestartController restart_controller(kernel);
  RandomControllerSelector selector(1);
  auto observation = restart_controller.observe();
  controller::ActionRequest original;
  std::string error;
  bool selected = false;
  for (uint32_t attempt = 0; attempt != 128 && !selected; ++attempt) {
    assert(selector.choose(kernel, observation, config, original, error));
    selected = original.type == controller::ActionType::Rule;
  }
  assert(selected);
  const auto exact = original.invocation.gf3_affine_instances;

  // Exercise the real executor/dispatch boundary.  The declined rule still
  // advances strategy_epoch; the selector must absorb that expected transition
  // instead of immediately rediscovering and reoffering the retained payload.
  auto constrained =
      std::get<Gf3AffineConsequenceParameters>(original.invocation.parameters);
  constrained.maximum_planning_work = 1;
  original.invocation.parameters = constrained;
  const auto exhausted =
      restart_controller.apply_rule(
          original, config.allowance("Gf3AffineConsequence"));
  assert(exhausted.invocation.status == InvocationStatus::ResourceExhausted);
  observation = restart_controller.observe();
  for (uint32_t attempt = 0; attempt != 64; ++attempt) {
    controller::ActionRequest suspended;
    assert(selector.choose(kernel, observation, config, suspended, error));
    assert(suspended.type != controller::ActionType::Rule);
  }

  // A profile-only reconfiguration preserves and retries the exact pending
  // identity without paying discovery work again.
  auto reconfigured = config;
  ++reconfigured.parameters.gf3_affine_consequence_reference.maximum_planning_work;
  bool retried = false;
  for (uint32_t attempt = 0; attempt != 128 && !retried; ++attempt) {
    controller::ActionRequest request;
    assert(selector.choose(kernel, observation, reconfigured, request, error));
    if (request.type != controller::ActionType::Rule) continue;
    assert(request.invocation.gf3_affine_instances == exact &&
           request.producer_effort.empty());
    retried = true;
  }
  assert(retried);

  // Policy coverage distinguishes resource exhaustion from an ordinary
  // no-change result just as it already does allowance exhaustion.
  {
    Kernel coverage_kernel = parse_text(gf3_dimacs());
    Gf3AffineActionSession coverage_session;
    coverage_kernel.structural_provenance = records;
    coverage_kernel.gf3_affine_action_session = &coverage_session;
    OnlineInvocation invocation = original.invocation;
    invocation.gf3_affine_instances = exact;
    invocation.parameters = constrained;
    ResourceThenSearchSelection scripted(std::move(invocation));
    RandomControllerPolicy policy(1, config, nullptr, &scripted);
    assert(policy.solve(coverage_kernel) == 10);
    const auto found =
        policy.coverage().rules.find("Gf3AffineConsequence");
    assert(found != policy.coverage().rules.end() &&
           found->second.exhausted == 1);
  }

  // Invalid/stale exact requests are discarded.  They remain terminal at the
  // same state and become discoverable only after a real context change.
  for (const InvocationStatus terminal :
       {InvocationStatus::StaleRequest, InvocationStatus::InvalidRequest}) {
    Kernel fresh = parse_text(gf3_dimacs());
    Gf3AffineActionSession fresh_session;
    fresh.structural_provenance = records;
    fresh.gf3_affine_action_session = &fresh_session;
    RandomControllerSelector fresh_selector(1);
    auto current = controller::observe_kernel(fresh);
    controller::ActionRequest pending;
    bool have_pending = false;
    for (uint32_t attempt = 0; attempt != 128 && !have_pending; ++attempt) {
      assert(fresh_selector.choose(fresh, current, config, pending, error));
      have_pending = pending.type == controller::ActionType::Rule;
    }
    assert(have_pending);
    controller::ActionResult outcome;
    outcome.sequence = pending.sequence;
    outcome.type = pending.type;
    outcome.invocation.status = terminal;
    outcome.invocation.after = state_version(fresh);
    current.last_action = outcome;
    current.sequence = pending.sequence;
    for (uint32_t attempt = 0; attempt != 64; ++attempt) {
      controller::ActionRequest suppressed;
      assert(fresh_selector.choose(fresh, current, config, suppressed, error));
      assert(suppressed.type != controller::ActionType::Rule);
    }
    ++fresh.strategy_epoch;
    current = controller::observe_kernel(fresh);
    bool refreshed = false;
    for (uint32_t attempt = 0; attempt != 128 && !refreshed; ++attempt) {
      controller::ActionRequest request;
      assert(fresh_selector.choose(fresh, current, config, request, error));
      refreshed = request.type == controller::ActionType::Rule;
    }
    assert(refreshed && !fresh_session.retained());
  }
  assert(!session.retained());
  std::fclose(records);
}

void dynamic_direct_policy_trace_replays_exact_payload() {
  auto config = dynamic_direct_config();
  config.parameters.rules.push_back("Propagate");
  const auto signature = controller_configuration(config);
  char path[] = "/tmp/tactix-dynamic-direct-trace-XXXXXX";
  const int fd = mkstemp(path);
  assert(fd >= 0);
  close(fd);

  uint64_t seed = 0;
  for (uint64_t candidate = 1; candidate != 128 && !seed; ++candidate) {
    Kernel probe = parse_text(direct_csp_dimacs());
    FILE *records = std::tmpfile();
    assert(records);
    DirectCspSupportActionSession session;
    probe.structural_provenance = records;
    probe.direct_csp_support_action_session = &session;
    RandomControllerSelector selector(candidate);
    controller::ActionRequest request;
    std::string error;
    assert(selector.choose(probe, controller::observe_kernel(probe), config,
                           request, error));
    if (request.type == controller::ActionType::Rule &&
        request.invocation.rule == "DirectCspSupport")
      seed = candidate;
    std::fclose(records);
  }
  assert(seed);

  {
    Kernel kernel = parse_text(direct_csp_dimacs());
    FILE *records = std::tmpfile();
    assert(records);
    DirectCspSupportActionSession session;
    kernel.structural_provenance = records;
    kernel.direct_csp_support_action_session = &session;
    ControllerTrace trace;
    assert(trace.write(path, seed, signature));
    RandomControllerPolicy policy(seed, config, &trace);
    assert(policy.solve(kernel) == 10 && session.committed_keys() >= 1);
    const auto &coverage = policy.coverage().rules.at("DirectCspSupport");
    assert(coverage.producer_calls >= 1 && coverage.producer_work &&
           coverage.producer_candidates >= 1 && coverage.producer_offers >= 1);
    std::fclose(records);
  }
  {
    Kernel kernel = parse_text(direct_csp_dimacs());
    FILE *records = std::tmpfile();
    assert(records);
    DirectCspSupportActionSession session;
    kernel.structural_provenance = records;
    kernel.direct_csp_support_action_session = &session;
    ControllerTrace trace;
    assert(trace.replay(path, signature));
    RandomControllerPolicy policy(seed + 99, config, &trace);
    assert(policy.solve(kernel) == 10 && session.committed_keys() >= 1);
    std::fclose(records);
  }
  std::ifstream input(path);
  std::ostringstream bytes;
  bytes << input.rdbuf();
  const std::string transcript = bytes.str();
  assert(transcript.find(":direct-csp-support[") != std::string::npos &&
         transcript.find("producer \"DirectCspSupport\"") !=
             std::string::npos);
  input.close();
  unlink(path);
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

void configure_c_api_base(capi::tactix_online_step &base, uint64_t id,
                          const char *rule) {
  base.step_id = id;
  base.rule_id = c_api_rule_id(rule);
  assert(base.rule_id >= 0);
  base.granularity = TACTIX_ONLINE_GRANULARITY_ONE;
  base.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  base.repeat_limit = 1;
  base.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  base.budget = 1;
  base.post_application = TACTIX_ONLINE_POST_PROPAGATE;
}

void public_c_api_executes_productive_v5_actions() {
  {
    capi::tactix *solver = capi::tactix_init();
    FILE *records = std::tmpfile();
    assert(solver && records);
    load_c_api_dimacs(solver, gf3_dimacs());
    assert(capi::tactix_bind_structural_provenance(solver, records) == 0);
    capi::tactix_online_step_v5 step{};
    configure_c_api_base(step.base.base.base.base, 201,
                         "Gf3AffineConsequence");
    auto &parameters =
        step.base.base.base.base.parameters.value.gf3_affine_consequence;
    step.base.base.base.base.parameters.kind =
        TACTIX_ONLINE_PARAMETERS_GF3_AFFINE_CONSEQUENCE;
    parameters = {2000, 20000, 100, 100, 6, 1000, 8, 10000, 1000, 10000,
                  1000000, 1000000, 1000000, 1000000, 100000, 1000,
                  10000, 10000, 1000000, 100, 100, 10000};
    const capi::tactix_online_gf3_affine_instance selected{0, 1};
    step.gf3_affine_instances = &selected;
    step.gf3_affine_instance_count = 1;
    const capi::tactix_online_program_v5 program{
        &step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
    assert(capi::tactix_online_configure_v5(
               solver, &program, 201, TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
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
  {
    capi::tactix *solver = capi::tactix_init();
    FILE *records = std::tmpfile();
    assert(solver && records);
    load_c_api_dimacs(solver, direct_csp_dimacs());
    assert(capi::tactix_bind_structural_provenance(solver, records) == 0);
    capi::tactix_online_step_v5 step{};
    configure_c_api_base(step.base.base.base.base, 202, "DirectCspSupport");
    auto &parameters =
        step.base.base.base.base.parameters.value.direct_csp_support;
    step.base.base.base.base.parameters.kind =
        TACTIX_ONLINE_PARAMETERS_DIRECT_CSP_SUPPORT;
    parameters = {100, 1000, 100000, 16, 2, 16, 1000, 20, 100, 100000,
                  1, 100, 16, 10000, 1};
    const int left[] = {1, 2, 3};
    const int right[] = {4, 5, 6, 7};
    const capi::tactix_online_direct_csp_support_instance selected{
        left, 3, right, 4, 1};
    step.direct_csp_support_instances = &selected;
    step.direct_csp_support_instance_count = 1;
    const capi::tactix_online_program_v5 program{
        &step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
    assert(capi::tactix_online_configure_v5(
               solver, &program, 202, TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
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
}

void random_policy_solves_after_executing_offered_v5_action() {
  OnlineInvocation offered = direct_step();
  offered.granularity = InvocationGranularity::OneInstance;
  offered.direct_csp_support_instances.resize(1);
  offered.budget = WorkLimit::bounded(1);
  OnlineStrategyProgram collection;
  collection.sequence = {offered};
  RandomControllerConfig config;
  config.parameters.rules = {"DirectCspSupport"};
  std::string error;
  assert(config.configure_actions(collection, &error));

  uint64_t selected_seed = 0;
  {
    Kernel probe = parse_text(direct_csp_dimacs());
    FILE *records = std::tmpfile();
    assert(records);
    DirectCspSupportActionSession session;
    probe.structural_provenance = records;
    probe.direct_csp_support_action_session = &session;
    controller::RestartController controller(probe);
    for (uint64_t seed = 1; seed < 128 && !selected_seed; ++seed) {
      RandomControllerSelector selector(seed);
      controller::ActionRequest request;
      assert(selector.choose(probe, controller.observe(), config, request,
                             error));
      if (request.type == controller::ActionType::Rule &&
          request.invocation.rule == "DirectCspSupport")
        selected_seed = seed;
    }
    std::fclose(records);
  }
  assert(selected_seed);

  Kernel kernel = parse_text(direct_csp_dimacs());
  FILE *records = std::tmpfile();
  assert(records);
  DirectCspSupportActionSession session;
  kernel.structural_provenance = records;
  kernel.direct_csp_support_action_session = &session;
  RandomControllerPolicy policy(selected_seed, config);
  assert(policy.solve(kernel) == 10);
  const auto &coverage = policy.coverage().rules.at("DirectCspSupport");
  assert(coverage.requests >= 1 && coverage.applications >= 1 &&
         coverage.effective >= 1 && session.committed_keys() >= 1 &&
         kernel.stats.direct_csp_support_applied >= 1);
  std::fclose(records);
}

} // namespace

int main() {
  productive_batches_publish_distinct_candidates_and_refresh_generation();
  invalid_is_consumed_and_resource_exhaustion_is_retryable();
  offered_random_action_is_exact_and_deterministic();
  disabled_dynamic_producers_preserve_configuration_and_selection();
  static_structured_offer_precedes_native_discovery();
  dynamic_theory_offers_are_exact_deterministic_and_applicable();
  dynamic_gf3_pagination_reaches_terminal_without_reoffering();
  native_execution_outcomes_obey_pending_lifecycle();
  public_c_api_executes_productive_v5_actions();
  random_policy_solves_after_executing_offered_v5_action();
  dynamic_direct_policy_trace_replays_exact_payload();
  std::puts("v5 theory action runtime PASS");
}
