#undef NDEBUG
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix

#include "frontend/parse.hpp"
#include "policies/random_controller_policy.hpp"
#include "rules/semantic_extension/nary_csp_prefix_action.hpp"
#include "strategy/online_inprocess.hpp"
#include "strategy/random_online.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

using namespace tactix;
using namespace tactix::strategy;
using namespace tactix::strategy::controller;

namespace {

const std::vector<std::vector<int32_t>> formula{
    {1, 2, 3}, {-1, -2}, {-1, -3}, {-2, -3},
    {4, 5, 6}, {-4, -5}, {-4, -6}, {-5, -6},
    {7, 8, 9}, {-7, -8}, {-7, -9}, {-8, -9},
    {-1, -4, -7}, {-2, -5, -8}, {-3, -6, -9}};

Kernel parsed_formula() {
  std::ostringstream dimacs;
  dimacs << "p cnf 9 " << formula.size() << '\n';
  for (const auto &clause : formula) {
    for (const int32_t literal : clause) dimacs << literal << ' ';
    dimacs << "0\n";
  }
  const std::string text = dimacs.str();
  FILE *input = std::tmpfile();
  assert(input);
  assert(std::fwrite(text.data(), 1, text.size(), input) == text.size());
  std::rewind(input);
  Kernel kernel;
  assert(parse_dimacs(input, kernel) == nullptr);
  std::fclose(input);
  return kernel;
}

NaryCspPrefixParameters parameters() {
  NaryCspPrefixParameters result{
      100, 1000, 100000, 8, 16, 100, 7, 16,
      1000, 32, 200, 100000, 1, 100, 32, 100000};
  assert(result.valid());
  return result;
}

NaryCspPrefixActionSelection selected() {
  return {{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}},
          {2, {{0, 0}, {1, 0}}}};
}

OnlineInvocation invocation() {
  OnlineInvocation result;
  result.id = 28;
  result.rule = "NaryCspPrefix";
  result.parameters = parameters();
  result.granularity = InvocationGranularity::OneInstance;
  result.nary_csp_prefix_instances = {selected()};
  result.repeat = {RepeatMode::Once, 1};
  result.budget = WorkLimit::bounded(1);
  return result;
}

void typed_replay_round_trips_and_rejects_noncanonical_payloads() {
  OnlineStrategyProgram program;
  program.sequence = {invocation()};
  std::string error;
  assert(validate_online_strategy_program(program, &error));
  const std::string wire = serialize_online_strategy_program(program);
  assert(wire.find(":nary-csp-prefix1(") != std::string::npos);
  assert(wire.find(":nary-csp-prefix[3,3,1,2,3,3,4,5,6,3,7,8,9,2,2,0,0,1,0]") !=
         std::string::npos);
  OnlineStrategyProgram replay;
  assert(deserialize_online_strategy_program(wire, replay, &error));
  assert(serialize_online_strategy_program(replay) == wire);
  assert(replay.sequence.size() == 1 &&
         replay.sequence.front().nary_csp_prefix_instances ==
             program.sequence.front().nary_csp_prefix_instances);
  auto malformed = program;
  std::swap(malformed.sequence.front()
                .nary_csp_prefix_instances.front()
                .domains[0],
            malformed.sequence.front()
                .nary_csp_prefix_instances.front()
                .domains[1]);
  assert(!validate_online_strategy_program(malformed, &error));
}

void native_random_offer_is_exact_and_applies_with_raw_feedback() {
  Kernel kernel = parsed_formula();
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  NaryCspPrefixActionSession session;
  kernel.nary_csp_prefix_action_session = &session;
  RandomControllerConfig config;
  config.parameters.rules = {"NaryCspPrefix"};
  config.parameters.nary_csp_prefix_reference = parameters();
  config.nary_csp_action_producer.enabled = true;
  config.nary_csp_action_producer.maximum_offers = 9;
  config.nary_csp_action_producer.maximum_enumeration_work = 10000000;
  RestartController controller(kernel);
  bool applied = false;
  for (uint64_t seed = 1; seed != 128 && !applied; ++seed) {
    RandomControllerSelector selector(seed);
    ActionRequest request;
    std::string error;
    assert(selector.choose(kernel, controller.observe(), config, request,
                           error));
    if (request.type != ActionType::Rule) continue;
    assert(request.invocation.rule == "NaryCspPrefix" &&
           request.invocation.nary_csp_prefix_instances.size() == 1 &&
           nary_csp_prefix_action_selection_valid(
               request.invocation.nary_csp_prefix_instances.front()) &&
           request.producer_effort.size() == 1 &&
           request.producer_effort.front().family == "NaryCspPrefix" &&
           request.producer_effort.front().offers == 9);
    request.invocation.post_application = PostApplication::Propagate;
    const auto result =
        controller.apply_rule(request, config.allowance("NaryCspPrefix"));
    assert(result.invocation.status == InvocationStatus::Completed &&
           result.invocation.made_rule_progress &&
           kernel.stats.nary_csp_prefix_attempts == 1 &&
           kernel.stats.nary_csp_prefix_applied == 1 &&
           session.committed_keys() == 1 && session.retained_owners() == 1);
    assert(kernel.stats.nary_csp_prefix_source_clauses == formula.size() &&
           kernel.stats.nary_csp_prefix_source_literals == 36 &&
           kernel.stats.nary_csp_prefix_owner_capture_work > 0 &&
           kernel.stats.nary_csp_prefix_source_scan_work > 0 &&
           kernel.stats.nary_csp_prefix_planning_work > 0 &&
           kernel.stats.nary_csp_prefix_revalidation_work > 0 &&
           kernel.stats.nary_csp_prefix_proof_work > 0 &&
           kernel.stats.nary_csp_prefix_planned_additions == 4 &&
           kernel.stats.nary_csp_prefix_emitted_additions == 4 &&
           kernel.stats.nary_csp_prefix_proof_literal_occurrences > 0 &&
           kernel.stats.nary_csp_prefix_journal_work > 0 &&
           kernel.stats.nary_csp_prefix_journal_bytes > 0 &&
           kernel.stats.nary_csp_prefix_extent_growth == 1 &&
           kernel.stats.nary_csp_prefix_session_work > 0 &&
           kernel.stats.nary_csp_prefix_session_commits == 1 &&
           kernel.stats.nary_csp_prefix_owner_commits == 1 &&
           kernel.stats.nary_csp_prefix_candidate_variables == 4 &&
           kernel.stats.nary_csp_prefix_introduced_variables == 1);
    const auto observation = controller.observe();
    const auto candidate = std::find_if(
        observation.semantic_variable_candidates.begin(),
        observation.semantic_variable_candidates.end(), [](const auto &value) {
          return value.producer == SemanticVariableProducer::NaryCsp;
        });
    assert(candidate != observation.semantic_variable_candidates.end() &&
           candidate->action_id == 1 && candidate->existing_variables == 3 &&
           candidate->introduced_variables == 1 &&
           candidate->arena_generation == observation.state.arena_generation &&
           candidate->touch_epoch == observation.state.touch_epoch &&
           candidate->strategy_epoch == observation.state.strategy_epoch);
    const auto *published = semantic_variable_candidate(kernel, candidate->id);
    assert(published && published->variables.size() == 4 &&
           published->variables.back().external == 10 &&
           published->variables.back().role ==
               SemanticVariableRole::IntroducedTheory);
    applied = true;
  }
  assert(applied);
  kernel.structural_provenance = nullptr;
  std::fclose(records);
}

void native_resource_exhaustion_retains_exact_offer_for_reconfiguration() {
  Kernel kernel = parsed_formula();
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  NaryCspPrefixActionSession session;
  kernel.nary_csp_prefix_action_session = &session;
  RandomControllerConfig config;
  config.parameters.rules = {"NaryCspPrefix"};
  config.parameters.nary_csp_prefix_reference = parameters();
  config.nary_csp_action_producer.enabled = true;
  config.nary_csp_action_producer.maximum_offers = 1;
  config.nary_csp_action_producer.maximum_enumeration_work = 10000000;
  RestartController controller(kernel);
  RandomControllerSelector selector(1);
  ActionRequest original;
  std::string error;
  bool selected_offer = false;
  for (uint32_t attempt = 0; attempt != 128 && !selected_offer; ++attempt) {
    assert(selector.choose(kernel, controller.observe(), config, original,
                           error));
    selected_offer = original.type == ActionType::Rule;
  }
  assert(selected_offer &&
         original.invocation.nary_csp_prefix_instances.size() == 1);
  const auto exact = original.invocation.nary_csp_prefix_instances;
  auto constrained =
      std::get<NaryCspPrefixParameters>(original.invocation.parameters);
  constrained.maximum_proof_additions = 1;
  original.invocation.parameters = constrained;
  const auto exhausted =
      controller.apply_rule(original, config.allowance("NaryCspPrefix"));
  assert(exhausted.invocation.status == InvocationStatus::ResourceExhausted &&
         exhausted.invocation.applications == 1 &&
         !exhausted.invocation.selection_complete &&
         kernel.stats.nary_csp_prefix_resource_exhausted == 1 &&
         session.committed_keys() == 0 && session.retained_owners() == 0);

  const auto observation = controller.observe();
  for (uint32_t attempt = 0; attempt != 32; ++attempt) {
    ActionRequest suspended;
    assert(selector.choose(kernel, observation, config, suspended, error));
    assert(suspended.type == ActionType::ResumeSearch);
  }

  auto reconfigured = config;
  ++reconfigured.parameters.nary_csp_prefix_reference.maximum_proof_additions;
  ActionRequest retry;
  bool retried = false;
  for (uint32_t attempt = 0; attempt != 128 && !retried; ++attempt) {
    assert(selector.choose(kernel, observation, reconfigured, retry, error));
    if (retry.type != ActionType::Rule) continue;
    assert(retry.invocation.nary_csp_prefix_instances == exact &&
           retry.producer_effort.empty());
    retried = true;
  }
  assert(retried);
  const auto applied =
      controller.apply_rule(retry, reconfigured.allowance("NaryCspPrefix"));
  assert(applied.invocation.status == InvocationStatus::Completed &&
         applied.invocation.made_rule_progress &&
         kernel.stats.nary_csp_prefix_applied == 1 &&
         session.committed_keys() == 1 && session.retained_owners() == 1);
  kernel.structural_provenance = nullptr;
  std::fclose(records);
}

void add_formula(tactix_c_handle *solver) {
  for (const auto &clause : formula) {
    for (const int32_t literal : clause) tactix_add(solver, literal);
    tactix_add(solver, 0);
  }
}

void assign_parameters(tactix_online_parameters &output) {
  const auto p = parameters();
  output.kind = TACTIX_ONLINE_PARAMETERS_NARY_CSP_PREFIX;
  output.value.nary_csp_prefix = {
      p.maximum_source_clauses,
      p.maximum_source_literals,
      p.maximum_recognition_work,
      p.maximum_domains,
      p.maximum_domain_values,
      p.maximum_forbidden_tuples,
      p.maximum_prefix_arity,
      p.maximum_definition_clause_literals,
      p.maximum_proof_work,
      p.maximum_proof_additions,
      p.maximum_proof_literal_occurrences,
      p.maximum_journal_bytes,
      p.maximum_extent_growth,
      p.maximum_external_extent,
      p.maximum_session_keys,
      p.maximum_session_work};
}

void c_v8_deep_copies_and_validates_the_exact_selection() {
  assert(tactix_rule_count() == 40);
  assert(std::strcmp(tactix_rule_name(39), "NaryCspPrefix") == 0);
  const auto reference = reference_inprocess_program();
  const auto reference_step = std::find_if(
      reference.begin(), reference.end(),
      [](const InprocessStep &candidate) { return candidate.id == 28; });
  assert(reference_step != reference.end() &&
         reference_step->rule == "NaryCspPrefix" &&
         !reference_step->enabled);

  tactix_c_handle *solver = tactix_init();
  FILE *records = std::tmpfile();
  assert(records && tactix_bind_structural_provenance(solver, records) == 0);
  add_formula(solver);
  int d0[] = {1, 2, 3}, d1[] = {4, 5, 6}, d2[] = {7, 8, 9};
  tactix_online_nary_csp_domain domains[] = {
      {d0, 3}, {d1, 3}, {d2, 3}};
  tactix_online_nary_csp_choice prefix[] = {{0, 0}, {1, 0}};
  tactix_online_nary_csp_prefix_instance exact{
      domains, 3, 2, prefix, 2};
  tactix_online_step_v8 step{};
  auto &base = step.base.base.base.base.base.base.base;
  base.step_id = 28;
  base.rule_id = 39;
  assign_parameters(base.parameters);
  base.granularity = TACTIX_ONLINE_GRANULARITY_ONE;
  base.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  base.repeat_limit = 1;
  base.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  base.budget = 1;
  step.nary_csp_prefix_instances = &exact;
  step.nary_csp_prefix_instance_count = 1;
  tactix_online_program_v8 program{
      &step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_configure_v8(
             solver, &program, 100, TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  d0[0] = 99;
  prefix[0] = {2, 99};
  tactix_online_result result{};
  assert(tactix_online_advance(solver, &result) == 0 &&
         result.rule_id == 39 && result.status == TACTIX_ONLINE_COMPLETED &&
         result.made_rule_progress);
  tactix_release(solver);
  std::fclose(records);

  tactix_c_handle *invalid = tactix_init();
  FILE *invalid_records = std::tmpfile();
  assert(invalid_records &&
         tactix_bind_structural_provenance(invalid, invalid_records) == 0);
  add_formula(invalid);
  d0[0] = 1;
  prefix[0] = {0, 0};
  exact.prefix_count = 1;
  assert(tactix_online_configure_v8(
             invalid, &program, 101, TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  exact.prefix_count = 2;
  std::swap(domains[0], domains[1]);
  assert(tactix_online_configure_v8(
             invalid, &program, 102, TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  tactix_release(invalid);
  std::fclose(invalid_records);
}

} // namespace

int main() {
  typed_replay_round_trips_and_rejects_noncanonical_payloads();
  native_random_offer_is_exact_and_applies_with_raw_feedback();
  native_resource_exhaustion_retains_exact_offer_for_reconfiguration();
  c_v8_deep_copies_and_validates_the_exact_selection();
  std::puts("test_nary_csp_prefix_public: passed");
}
