#undef NDEBUG
#include "policies/random_controller_policy.hpp"
#include "rules/decide/decide.hpp"
#include "rules/lucky/lucky.hpp"
#include <algorithm>
#include <cstdio>
#include <set>
#include <vector>

#include <cassert>
#include <fstream>
#include <iterator>
#include <sstream>
#include <unistd.h>

using namespace tactix;
using namespace tactix::strategy;
using namespace tactix::strategy::controller;

static Kernel fixture(bool unsat) {
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  if (unsat) {
    kernel_add_binary_clause(k, internalize(1), internalize(-2));
    kernel_add_binary_clause(k, internalize(-1), internalize(-2));
  }
  return k;
}

static void add_external_clause(Kernel &kernel,
                                const std::vector<int32_t> &external) {
  std::vector<Lit> literals;
  for (const int32_t literal : external)
    literals.push_back(kernel_internalize(kernel, literal));
  const uint64_t id = ++kernel.next_clause_id;
  if (literals.size() == 2)
    kernel_add_binary_clause(kernel, literals[0], literals[1], id);
  else
    kernel_add_large_clause(kernel, literals.data(), literals.size(), false, 0,
                            id);
}

static OnlineStrategyProgram affine_producer_profile(
    const AffineConsequenceParameters &parameters, uint64_t allowance = 3,
    std::optional<SemanticVariableAdviceParameters> advice = std::nullopt,
    uint64_t startup_batch_actions = 0) {
  OnlineInvocation step;
  step.id = 91;
  step.rule = "AffineConsequence";
  step.parameters = parameters;
  step.granularity = InvocationGranularity::CanonicalApplication;
  step.repeat = {RepeatMode::Once, 1};
  step.rule_work_budget = {RuleWorkBudgetMode::Bounded, 123456};
  step.work_unit = WorkUnit::Applications;
  step.budget = WorkLimit::bounded(allowance);
  OnlineStrategyProgram result{
      {step}, {startup_batch_actions ? RepeatMode::Count : RepeatMode::Once,
               startup_batch_actions ? startup_batch_actions : 1}};
  if (advice) {
    OnlineInvocation companion;
    companion.id = 92;
    companion.rule = "SemanticVariableAdvice";
    companion.parameters = *advice;
    companion.granularity = InvocationGranularity::CanonicalApplication;
    companion.repeat = {RepeatMode::Once, 1};
    companion.rule_work_budget = {RuleWorkBudgetMode::Bounded, 1};
    companion.work_unit = WorkUnit::Applications;
    companion.budget = WorkLimit::bounded(1);
    result.sequence.push_back(std::move(companion));
  }
  return result;
}

static Kernel direct_csp_fixture() {
  Kernel kernel;
  kernel_reserve_vars(kernel, 7);
  for (const auto &clause : std::vector<std::vector<int32_t>>{
           {1, 2, 3},  {-1, -2}, {-1, -3}, {-2, -3},
           {4, 5, 6, 7}, {-4, -5}, {-4, -6}, {-4, -7}, {-5, -6},
           {-5, -7}, {-6, -7}, {-1, -7}, {-2, -4}, {-2, -6},
           {-2, -7}, {-3, -5}})
    add_external_clause(kernel, clause);
  assert(kernel.trail.empty() && kernel_propagation_fixpoint(kernel));
  return kernel;
}

static OnlineStrategyProgram direct_csp_producer_profile(
    const DirectCspSupportParameters &parameters, uint64_t allowance = 3,
    std::optional<SemanticVariableAdviceParameters> advice = std::nullopt,
    uint64_t startup_batch_actions = 0) {
  OnlineInvocation step;
  step.id = 101;
  step.rule = "DirectCspSupport";
  step.parameters = parameters;
  step.granularity = InvocationGranularity::CanonicalApplication;
  step.repeat = {RepeatMode::Once, 1};
  step.rule_work_budget = {RuleWorkBudgetMode::Bounded, 10000000};
  step.work_unit = WorkUnit::Applications;
  step.budget = WorkLimit::bounded(allowance);
  OnlineStrategyProgram result{
      {step}, {startup_batch_actions ? RepeatMode::Count : RepeatMode::Once,
               startup_batch_actions ? startup_batch_actions : 1}};
  OnlineInvocation propagation;
  propagation.id = 102;
  propagation.rule = "Propagate";
  propagation.granularity = InvocationGranularity::CanonicalApplication;
  propagation.repeat = {RepeatMode::Once, 1};
  // Marks a separately observable propagation service even when the support
  // row adds no root unit and BCP is already settled.
  propagation.post_application = PostApplication::Propagate;
  propagation.rule_work_budget = {RuleWorkBudgetMode::Bounded, 1};
  propagation.work_unit = WorkUnit::Propagations;
  propagation.budget = WorkLimit::bounded(1);
  result.sequence.push_back(std::move(propagation));
  if (advice) {
    OnlineInvocation companion;
    companion.id = 103;
    companion.rule = "SemanticVariableAdvice";
    companion.parameters = *advice;
    companion.granularity = InvocationGranularity::CanonicalApplication;
    companion.repeat = {RepeatMode::Once, 1};
    companion.rule_work_budget = {RuleWorkBudgetMode::Bounded, 1};
    companion.work_unit = WorkUnit::Applications;
    companion.budget = WorkLimit::bounded(1);
    result.sequence.push_back(std::move(companion));
  }
  return result;
}

struct DirectCspFeedback final : FeedbackReceiver {
  std::vector<ActionFeedback> actions;
  void after_action(const ActionFeedback &record) override {
    actions.push_back(record);
  }
  void search_interval(const SearchIntervalFeedback &) override {}
};

static void native_affine_profile_is_exact_transactional_and_effective() {
  RandomControllerConfig config;
  config.parameters.rules = {"AffineConsequence"};
  config.affine_basis_action_producer.enabled = true;
  OnlineInvocation unrelated_offer;
  unrelated_offer.id = 90;
  unrelated_offer.rule = "Lucky";
  unrelated_offer.granularity = InvocationGranularity::CanonicalApplication;
  unrelated_offer.budget = WorkLimit::bounded(1);
  config.offered_actions["Lucky"] = {unrelated_offer};
  auto parameters = config.parameters.affine_consequence_reference;
  parameters.maximum_clauses = 4321;
  parameters.maximum_snapshot_literals = 8765;
  parameters.basis_maximum_nodes = 321;
  parameters.maximum_session_work = 6543;
  const auto profile = affine_producer_profile(parameters, 5);
  std::string error;
  const std::string original = controller_configuration(config);
  assert(config.configure_affine_basis_producer_profile(profile, &error));
  assert(error.empty() && config.offered_actions.size() == 1 &&
         config.offered_actions.at("Lucky").size() == 1 &&
         config.offered_actions.at("Lucky").front().id == 90);
  const auto &installed = config.parameters.affine_consequence_reference;
  assert(installed.maximum_clauses == 4321 &&
         installed.maximum_snapshot_literals == 8765 &&
         installed.basis_maximum_nodes == 321 &&
         installed.maximum_session_work == 6543);
  const auto allowance = config.allowance("AffineConsequence");
  assert(allowance.unit == WorkUnit::Applications && allowance.amount == 5 &&
         allowance.native.mode == RuleWorkBudgetMode::Bounded &&
         allowance.native.amount == 123456);
  const std::string accepted = controller_configuration(config);
  assert(accepted != original);

  RandomControllerConfig advised;
  advised.parameters.rules = {"AffineConsequence", "Propagate",
                              "SemanticVariableAdvice"};
  advised.affine_basis_action_producer.enabled = true;
  const SemanticVariableAdviceParameters advice{64, 4096, 2, 7};
  assert(advised.configure_affine_basis_producer_profile(
      affine_producer_profile(parameters, 5, advice), &error));
  assert(!advised.offered_actions.contains("AffineConsequence") &&
         !advised.offered_actions.contains("SemanticVariableAdvice"));
  assert(advised.parameters.semantic_variable_advice_reference.maximum_variables ==
             64 &&
         advised.parameters.semantic_variable_advice_reference
                 .maximum_priority_sum == 4096 &&
         advised.parameters.semantic_variable_advice_reference
                 .existing_theory_priority == 2 &&
         advised.parameters.semantic_variable_advice_reference
                 .introduced_theory_priority == 7);
  const auto advice_allowance = advised.allowance("SemanticVariableAdvice");
  assert(advice_allowance.unit == WorkUnit::Applications &&
         advice_allowance.amount == 1 &&
         advice_allowance.native.mode == RuleWorkBudgetMode::Bounded &&
         advice_allowance.native.amount == 1);
  assert(controller_configuration(advised).find(
             "semantic-variable-advice1(64,4096,2,7)") != std::string::npos);

  auto zero_advice = affine_producer_profile(parameters, 5, advice);
  std::get<SemanticVariableAdviceParameters>(zero_advice.sequence[1].parameters)
      .existing_theory_priority = 0;
  const auto advised_signature = controller_configuration(advised);
  assert(!advised.configure_affine_basis_producer_profile(zero_advice, &error));
  assert(controller_configuration(advised) == advised_signature);

  auto two_steps = profile;
  two_steps.sequence.push_back(two_steps.sequence.front());
  two_steps.sequence.back().id = 92;
  assert(!config.configure_affine_basis_producer_profile(two_steps, &error));
  assert(controller_configuration(config) == accepted);
  auto payload = profile;
  payload.sequence.front().granularity = InvocationGranularity::OneInstance;
  payload.sequence.front().instances = {1};
  assert(!config.configure_affine_basis_producer_profile(payload, &error));
  assert(controller_configuration(config) == accepted);
  auto v1 = profile;
  auto &v1_parameters =
      std::get<AffineConsequenceParameters>(v1.sequence.front().parameters);
  v1_parameters.semantics = AffineConsequenceSemantics::SelectedConsequenceV1;
  v1_parameters.maximum_intermediate_width = 16;
  assert(!config.configure_affine_basis_producer_profile(v1, &error));
  assert(controller_configuration(config) == accepted);
  auto unlimited_native = profile;
  unlimited_native.sequence.front().rule_work_budget = {
      RuleWorkBudgetMode::Unlimited, 0};
  assert(!config.configure_affine_basis_producer_profile(unlimited_native,
                                                         &error));
  assert(controller_configuration(config) == accepted);
  auto zero_native = profile;
  zero_native.sequence.front().rule_work_budget = {
      RuleWorkBudgetMode::Bounded, 0};
  assert(!config.configure_affine_basis_producer_profile(zero_native, &error));
  assert(controller_configuration(config) == accepted);
  auto unlimited_composition = profile;
  unlimited_composition.sequence.front().budget = WorkLimit::unlimited();
  assert(!config.configure_affine_basis_producer_profile(
      unlimited_composition, &error));
  assert(controller_configuration(config) == accepted);
  auto zero_composition = profile;
  zero_composition.sequence.front().budget = WorkLimit::bounded(0);
  assert(!config.configure_affine_basis_producer_profile(zero_composition,
                                                         &error));
  assert(controller_configuration(config) == accepted);

  RandomControllerConfig disabled;
  assert(!disabled.configure_affine_basis_producer_profile(profile, &error));
  RandomControllerConfig excluded;
  excluded.affine_basis_action_producer.enabled = true;
  excluded.parameters.rules = {"Lucky"};
  assert(!excluded.configure_affine_basis_producer_profile(profile, &error));
  RandomControllerConfig static_offer;
  static_offer.affine_basis_action_producer.enabled = true;
  static_offer.parameters.rules = {"AffineConsequence"};
  static_offer.offered_actions["AffineConsequence"] = profile.sequence;
  assert(!static_offer.configure_affine_basis_producer_profile(profile, &error));

  auto native_affine = profile;
  native_affine.sequence.front().granularity =
      InvocationGranularity::NativeProducerOffer;
  RandomControllerConfig profile_first;
  profile_first.parameters.rules = {"AffineConsequence"};
  profile_first.affine_basis_action_producer.enabled = true;
  assert(profile_first.configure_affine_basis_producer_profile(profile,
                                                               &error));
  const auto profile_first_signature = controller_configuration(profile_first);
  assert(!profile_first.configure_startup_prefix(native_affine, &error));
  assert(error == "controller startup affine offer conflicts with the GF2 "
                  "affine producer profile" &&
         controller_configuration(profile_first) == profile_first_signature);

  RandomControllerConfig prefix_first;
  prefix_first.parameters.rules = {"AffineConsequence"};
  prefix_first.affine_basis_action_producer.enabled = true;
  assert(prefix_first.configure_startup_prefix(native_affine, &error));
  const auto prefix_first_signature = controller_configuration(prefix_first);
  assert(!prefix_first.configure_affine_basis_producer_profile(profile,
                                                               &error));
  assert(error == "GF2 affine producer profile conflicts with the controller "
                  "startup affine offer" &&
         controller_configuration(prefix_first) == prefix_first_signature);

  RandomControllerConfig nary_prefix;
  nary_prefix.parameters.rules = {"NaryCspPrefix", "AffineConsequence"};
  nary_prefix.nary_csp_action_producer.enabled = true;
  nary_prefix.affine_basis_action_producer.enabled = true;
  OnlineInvocation native_nary;
  native_nary.id = 1;
  native_nary.rule = "NaryCspPrefix";
  native_nary.parameters = nary_prefix.parameters.nary_csp_prefix_reference;
  native_nary.granularity = InvocationGranularity::NativeProducerOffer;
  native_nary.work_unit = WorkUnit::Applications;
  native_nary.budget = WorkLimit::bounded(1);
  native_nary.rule_work_budget = {RuleWorkBudgetMode::Bounded, 1};
  assert(nary_prefix.configure_startup_prefix(
      OnlineStrategyProgram{{native_nary}, {RepeatMode::Once, 1}}, &error));
  assert(nary_prefix.configure_affine_basis_producer_profile(profile, &error));
  assert(nary_prefix.affine_basis_profile_configured);

  RandomControllerConfig gf3_row_prefix;
  gf3_row_prefix.parameters.rules = {"Gf3GeneralRowConsequence"};
  gf3_row_prefix.gf3_general_row_action_producer.enabled = true;
  OnlineInvocation native_gf3_row;
  native_gf3_row.id = 1;
  native_gf3_row.rule = "Gf3GeneralRowConsequence";
  native_gf3_row.parameters =
      gf3_row_prefix.parameters.gf3_general_row_consequence_reference;
  native_gf3_row.granularity = InvocationGranularity::NativeProducerOffer;
  native_gf3_row.work_unit = WorkUnit::Applications;
  native_gf3_row.budget = WorkLimit::bounded(1);
  native_gf3_row.rule_work_budget = {RuleWorkBudgetMode::Bounded, 1};
  assert(gf3_row_prefix.configure_startup_prefix(
      OnlineStrategyProgram{{native_gf3_row}, {RepeatMode::Once, 1}},
      &error));

  Kernel kernel;
  kernel_reserve_vars(kernel, 4);
  for (const auto &clause :
       affine_semantic_extension::clausal_form({1, 2, 3, 4}, false))
    add_external_clause(kernel, clause);
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  AffineBasisActionSession session;
  kernel.affine_basis_action_session = &session;
  RestartController controller(kernel);

  RandomControllerConfig low;
  low.parameters.rules = {"AffineConsequence"};
  low.affine_basis_action_producer.enabled = true;
  auto low_parameters = low.parameters.affine_consequence_reference;
  low_parameters.maximum_clauses = 7;
  assert(low.configure_affine_basis_producer_profile(
      affine_producer_profile(low_parameters), &error));
  RandomControllerSelector low_selector(7);
  ActionRequest low_request;
  assert(low_selector.choose(kernel, controller.observe(), low, low_request,
                             error));
  assert(low_request.type == ActionType::ResumeSearch);
  assert(std::any_of(low_request.producer_effort.begin(),
                     low_request.producer_effort.end(), [](const auto &effort) {
                       return effort.family == "AffineConsequence" &&
                              effort.status == static_cast<uint8_t>(
                                  AffineBasisOfferStatus::ResourceExhausted) &&
                              !effort.offers;
                     }));
  ++kernel.strategy_epoch;
  ActionRequest held_request;
  assert(low_selector.choose(kernel, controller.observe(), low, held_request,
                             error));
  assert(held_request.type == ActionType::ResumeSearch &&
         held_request.producer_effort.empty());

  RandomControllerConfig raised;
  raised.parameters.rules = {"AffineConsequence"};
  raised.affine_basis_action_producer.enabled = true;
  assert(raised.configure_affine_basis_producer_profile(
      affine_producer_profile(
          raised.parameters.affine_consequence_reference),
      &error));
  bool offered = false;
  for (unsigned attempt = 0; attempt < 32 && !offered; ++attempt) {
    ActionRequest request;
    assert(low_selector.choose(kernel, controller.observe(), raised, request,
                               error));
    offered = request.type == ActionType::Rule &&
              request.invocation.rule == "AffineConsequence" &&
              request.invocation.affine_basis_instances.size() == 1;
  }
  assert(offered);
  std::fclose(records);
}

static void exact_search_configuration_collection_is_learner_owned() {
  RandomControllerConfig config;
  config.parameters.rules = {"Walk"};
  const SearchConfiguration selected{Branching::Vmtf, Restart::AdaptiveGlue,
                                     Phase::FocusedRotation};
  std::string error;
  assert(config.configure_search_configurations({selected}, &error));
  assert(error.empty());
  const auto accepted = controller_configuration(config);
  assert(accepted.find("search-configuration \"vmtf/adaptive-glue/"
                       "focused-rotation\"") != std::string::npos);

  assert(!config.configure_search_configurations({selected, selected}, &error));
  assert(controller_configuration(config) == accepted);
  auto invalid = selected;
  invalid.phase = static_cast<Phase>(255);
  assert(!config.configure_search_configurations({invalid}, &error));
  assert(controller_configuration(config) == accepted);

  Kernel kernel = fixture(false);
  RestartController controller(kernel);
  RandomControllerSelector selector(17);
  bool saw_rule = false, saw_resume = false;
  for (unsigned attempt = 0; attempt < 256; ++attempt) {
    ActionRequest request;
    assert(selector.choose(kernel, controller.observe(), config, request, error));
    if (request.type == ActionType::ResumeSearch) {
      saw_resume = true;
      assert(request.search == selected);
    } else {
      saw_rule = true;
      assert(request.invocation.rule == "Walk");
    }
  }
  assert(saw_rule && saw_resume);
}

static void native_affine_basis_offer_is_exact_and_effective() {
  Kernel kernel;
  kernel_reserve_vars(kernel, 4);
  auto clauses = affine_semantic_extension::clausal_form({1, 2, 3, 4},
                                                          false);
  for (const auto &clause : clauses) add_external_clause(kernel, clause);
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  AffineBasisActionSession session;
  kernel.affine_basis_action_session = &session;
  RestartController controller(kernel);
  RandomControllerConfig config;
  config.parameters.rules = {"AffineConsequence"};
  config.affine_basis_action_producer.enabled = true;
  config.affine_basis_action_producer.maximum_offers = 1;
  RandomControllerSelector selector(7);
  bool applied = false;
  for (unsigned attempt = 0; attempt < 32 && !applied; ++attempt) {
    ActionRequest request;
    std::string error;
    assert(selector.choose(kernel, controller.observe(), config, request, error));
    if (request.type == ActionType::ResumeSearch) continue;
    assert(request.invocation.rule == "AffineConsequence" &&
           request.invocation.affine_basis_instances.size() == 1 &&
           request.invocation.instances.empty());
    assert(request.invocation.affine_basis_instances.front().kind ==
               affine_semantic_extension::BasisRowKind::Extended &&
           request.invocation.affine_basis_instances.front()
                   .source_equations.size() == 1);
    assert(std::any_of(request.producer_effort.begin(),
                       request.producer_effort.end(), [](const auto &effort) {
                         return effort.family == "AffineConsequence" &&
                                effort.candidates && effort.offers;
                       }));
    const std::string wire = serialize_online_strategy_program(
        {{request.invocation}, {}});
    OnlineStrategyProgram replay;
    const bool replayed = deserialize_online_strategy_program(wire, replay, &error);
    assert(replayed &&
           replay.sequence.front().affine_basis_instances ==
               request.invocation.affine_basis_instances);
    const auto result = controller.apply_rule(
        request, config.allowance("AffineConsequence"));
    assert(result.invocation.status == InvocationStatus::Completed &&
           result.invocation.made_rule_progress &&
           kernel.stats.affine_consequence_applied == 1 &&
           session.committed_keys() == 1);
    // A selected wide row installs its certified root-parity unit but leaves
    // propagation as an explicit controller action.  An Affine-only
    // collection therefore has a real action-space dead end: neither another
    // Affine row nor ResumeSearch is legal while propagation is pending.
    assert(!kernel_propagation_fixpoint(kernel));
    ActionRequest blocked;
    error.clear();
    assert(!selector.choose(kernel, controller.observe(), config, blocked,
                            error));
    assert(error == "no eligible controller action");

    // Admitting Propagate resolves exactly that debt.  It is selected because
    // it is the sole eligible configured action, not as a hidden fallback.
    RandomControllerConfig explicit_propagation = config;
    explicit_propagation.parameters.rules = {"AffineConsequence",
                                             "Propagate"};
    ActionRequest propagate;
    error.clear();
    assert(selector.choose(kernel, controller.observe(), explicit_propagation,
                           propagate, error));
    assert(propagate.type == ActionType::Rule &&
           propagate.invocation.rule == "Propagate");
    const auto propagated = controller.apply_rule(
        propagate, explicit_propagation.allowance("Propagate"));
    assert(propagated.invocation.status == InvocationStatus::Completed &&
           kernel_propagation_fixpoint(kernel));
    applied = true;
  }
  assert(applied);
  std::fclose(records);
}

static void native_affine_empty_census_is_durable_across_restarts() {
  Kernel kernel;
  kernel_reserve_vars(kernel, 4);
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  AffineBasisActionSession session;
  kernel.affine_basis_action_session = &session;

  RandomControllerConfig config;
  config.parameters.rules = {"AffineConsequence"};
  config.affine_basis_action_producer.enabled = true;
  RandomControllerSelector selector(7);
  Observation observation;
  uint64_t producer_calls = 0, producer_feedback_records = 0;
  uint64_t producer_work = 0;
  for (unsigned restart = 0; restart < 512; ++restart) {
    ActionRequest request;
    std::string error;
    assert(selector.choose(kernel, observation, config, request, error));
    assert(request.type == ActionType::ResumeSearch);
    producer_calls += !request.producer_effort.empty();
    producer_feedback_records += request.producer_effort.size();
    for (const auto &effort : request.producer_effort) {
      assert(effort.family == "AffineConsequence");
      producer_work += effort.total_work;
      if (!restart)
        assert(effort.status ==
                   static_cast<uint8_t>(AffineBasisOfferStatus::NoStructure) &&
               effort.complete && !effort.offers);
    }

    // A resume/search/restart cycle advances public strategy state, and
    // learned/root work may advance the formula touch stamp.  Neither expands
    // the immutable source population whose empty census just completed.
    ++observation.sequence;
    ++kernel.strategy_epoch;
    kernel_touch_var(kernel, restart % kernel.nvars);
  }
  assert(producer_calls == 1 && producer_feedback_records == 1);
  assert(producer_work <=
         config.parameters.affine_consequence_reference.maximum_elimination_work);
  assert(!session.bound());
  std::fclose(records);
}

static void native_affine_completed_census_keeps_additional_rows() {
  Kernel kernel;
  kernel_reserve_vars(kernel, 8);
  for (const auto &equation :
       std::vector<std::pair<std::vector<int32_t>, bool>>{
           {{1, 2, 3}, false}, {{1, 2, 4}, true},
           {{5, 6, 7}, false}, {{5, 6, 8}, true}})
    for (const auto &clause :
         affine_semantic_extension::clausal_form(equation.first,
                                                  equation.second))
      add_external_clause(kernel, clause);
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  AffineBasisActionSession session;
  kernel.affine_basis_action_session = &session;
  RestartController controller(kernel);

  RandomControllerConfig config;
  config.parameters.rules = {"AffineConsequence", "Propagate"};
  config.affine_basis_action_producer.enabled = true;
  config.affine_basis_action_producer.maximum_offers = 1;
  RandomControllerSelector selector(7);
  std::vector<std::vector<int32_t>> applied;
  for (unsigned attempt = 0; attempt < 128 && applied.size() < 2; ++attempt) {
    ActionRequest request;
    std::string error;
    assert(selector.choose(kernel, controller.observe(), config, request,
                           error));
    if (request.type == ActionType::ResumeSearch) {
      ++kernel.strategy_epoch;
      continue;
    }
    const auto result = controller.apply_rule(
        request, config.allowance(request.invocation.rule));
    assert(result.invocation.status == InvocationStatus::Completed);
    if (request.invocation.rule == "AffineConsequence") {
      assert(request.invocation.affine_basis_instances.size() == 1);
      applied.push_back(
          request.invocation.affine_basis_instances.front().variables);
    }
  }
  std::sort(applied.begin(), applied.end());
  assert(applied ==
         std::vector<std::vector<int32_t>>({{3, 4}, {7, 8}}));
  assert(session.committed_keys() == 2);

  // The retained owner is now exhausted.  Subsequent restart boundaries do
  // not enumerate it again or manufacture a duplicate offer.
  uint64_t later_calls = 0;
  for (unsigned restart = 0; restart < 64; ++restart) {
    ActionRequest request;
    std::string error;
    assert(selector.choose(kernel, controller.observe(), config, request,
                           error));
    later_calls += request.producer_effort.size();
    assert(request.type == ActionType::ResumeSearch);
    ++kernel.strategy_epoch;
  }
  // The second preserved page already carried an exact end-of-census marker;
  // no duplicate-only confirmation pass is needed after its row commits.
  assert(later_calls == 0);
  std::fclose(records);
}

static void native_affine_bounded_startup_batch_preserves_exact_pages() {
  Kernel kernel;
  kernel_reserve_vars(kernel, 8);
  for (const auto &equation :
       std::vector<std::pair<std::vector<int32_t>, bool>>{
           {{1, 2, 3}, false}, {{1, 2, 4}, true},
           {{5, 6, 7}, false}, {{5, 6, 8}, true}})
    for (const auto &clause :
         affine_semantic_extension::clausal_form(equation.first,
                                                  equation.second))
      add_external_clause(kernel, clause);
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  AffineBasisActionSession session;
  kernel.affine_basis_action_session = &session;
  RestartController controller(kernel);

  RandomControllerConfig config;
  config.parameters.rules = {"AffineConsequence", "Propagate",
                             "SemanticVariableAdvice"};
  config.affine_basis_action_producer.enabled = true;
  config.affine_basis_action_producer.maximum_offers = 2;
  const SemanticVariableAdviceParameters advice{64, 4096, 2, 7};
  std::string error;
  assert(config.configure_affine_basis_producer_profile(
      affine_producer_profile(config.parameters.affine_consequence_reference,
                              3, advice, 2),
      &error));
  assert(config.affine_basis_action_producer.startup_batch_actions == 2);
  assert(controller_configuration(config).find(
             "producer \"AffineConsequence\" 2 2") != std::string::npos);

  RandomControllerSelector selector(1);
  uint64_t producer_calls = 0;
  std::vector<std::vector<int32_t>> applied;
  for (unsigned step = 0; step < 8 && applied.size() < 2; ++step) {
    ActionRequest request;
    assert(selector.choose(kernel, controller.observe(), config, request,
                           error));
    assert(request.type == ActionType::Rule);
    producer_calls += request.producer_effort.size();
    const auto result = controller.apply_rule(
        request, config.allowance(request.invocation.rule));
    assert(result.invocation.status == InvocationStatus::Completed);
    if (request.invocation.rule == "AffineConsequence") {
      applied.push_back(
          request.invocation.affine_basis_instances.front().variables);
    }
  }
  std::sort(applied.begin(), applied.end());
  assert(applied ==
         std::vector<std::vector<int32_t>>({{3, 4}, {7, 8}}));
  assert(session.committed_keys() == 2);
  // Both exact rows came from one authenticated page.  Committing the first
  // key must not discard and rebuild the remaining offer.
  assert(producer_calls == 1);

  ActionRequest after_batch;
  assert(selector.choose(kernel, controller.observe(), config, after_batch,
                         error));
  assert(after_batch.type == ActionType::Rule &&
         after_batch.invocation.rule == "SemanticVariableAdvice");
  std::fclose(records);
}

static void native_affine_bounded_startup_batch_crosses_propagation() {
  Kernel kernel;
  kernel_reserve_vars(kernel, 16);
  for (const auto &equation :
       std::vector<std::pair<std::vector<int32_t>, bool>>{
           {{1, 2, 3, 4, 5}, false}, {{1, 2, 6, 7, 8}, true},
           {{9, 10, 11, 12, 13}, false}, {{9, 10, 14, 15, 16}, true}})
    for (const auto &clause :
         affine_semantic_extension::clausal_form(equation.first,
                                                  equation.second))
      add_external_clause(kernel, clause);
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  AffineBasisActionSession session;
  kernel.affine_basis_action_session = &session;
  RestartController controller(kernel);

  RandomControllerConfig config;
  config.parameters.rules = {"AffineConsequence", "Propagate",
                             "SemanticVariableAdvice"};
  config.affine_basis_action_producer.enabled = true;
  config.affine_basis_action_producer.maximum_offers = 8;
  auto parameters = config.parameters.affine_consequence_reference;
  parameters.basis_maximum_direct_width = 3;
  const SemanticVariableAdviceParameters advice{64, 4096, 2, 7};
  std::string error;
  assert(config.configure_affine_basis_producer_profile(
      affine_producer_profile(parameters, 3, advice, 2), &error));
  RandomControllerSelector selector(1);

  const uint64_t proof_id_before = kernel.next_clause_id;
  const size_t extent_before = kernel.external_to_internal.size();
  ActionRequest first;
  assert(selector.choose(kernel, controller.observe(), config, first, error));
  assert(first.type == ActionType::Rule &&
         first.invocation.rule == "AffineConsequence");
  auto result = controller.apply_rule(first, config.allowance("AffineConsequence"));
  assert(result.invocation.status == InvocationStatus::Completed &&
         result.invocation.rule_result == ApplyResult::Ok &&
         result.invocation.made_rule_progress &&
         session.committed_keys() == 1 &&
         kernel.next_clause_id > proof_id_before &&
         kernel.external_to_internal.size() > extent_before &&
         !kernel_propagation_fixpoint(kernel));

  ActionRequest propagation;
  assert(selector.choose(kernel, controller.observe(), config, propagation,
                         error));
  assert(propagation.type == ActionType::Rule &&
         propagation.invocation.rule == "Propagate");
  result = controller.apply_rule(propagation, config.allowance("Propagate"));
  assert(result.invocation.status == InvocationStatus::Completed &&
         kernel_propagation_fixpoint(kernel));

  ActionRequest second;
  assert(selector.choose(kernel, controller.observe(), config, second, error));
  assert(second.type == ActionType::Rule &&
         second.invocation.rule == "AffineConsequence");
  std::fclose(records);
}

static void native_direct_csp_profile_is_exact_and_transactional() {
  RandomControllerConfig config;
  assert(config.parameters.direct_csp_support_reference.valid());
  config.parameters.rules = {"DirectCspSupport", "Propagate",
                             "SemanticVariableAdvice"};
  config.direct_csp_action_producer.enabled = true;
  const SemanticVariableAdviceParameters advice{64, 4096, 2, 7};
  std::string error;
  const auto profile = direct_csp_producer_profile(
      config.parameters.direct_csp_support_reference, 3, advice, 2);
  const std::string encoded_profile =
      serialize_online_strategy_program(profile);
  OnlineStrategyProgram parsed_profile;
  assert(!deserialize_online_strategy_program(encoded_profile, parsed_profile,
                                               &error));
  assert(deserialize_online_strategy_program(encoded_profile, parsed_profile,
                                              &error, true));
  assert(serialize_online_strategy_program(parsed_profile) == encoded_profile &&
         parsed_profile.sequence.front().direct_csp_support_instances.empty());
  assert(config.configure_direct_csp_producer_profile(profile, &error));
  assert(error.empty());
  assert(profile.sequence.front().granularity ==
             InvocationGranularity::CanonicalApplication &&
         profile.sequence.front().direct_csp_support_instances.empty() &&
         !config.offered_actions.contains("DirectCspSupport"));
  assert(config.direct_csp_action_producer.startup_batch_actions == 2);
  assert(config.direct_csp_action_producer.maximum_enumeration_work ==
         DefaultStrategyConfig::INPROCESS_MINIMUM_EFFORT_DEFAULT);
  const auto support = config.allowance("DirectCspSupport");
  assert(support.unit == WorkUnit::Applications && support.amount == 3 &&
         support.native.mode == RuleWorkBudgetMode::Bounded &&
         support.native.amount == 10000000);
  const auto semantic = config.allowance("SemanticVariableAdvice");
  assert(semantic.unit == WorkUnit::Applications && semantic.amount == 1 &&
         semantic.native.mode == RuleWorkBudgetMode::Bounded &&
         semantic.native.amount == 1);
  const auto propagation = config.allowance("Propagate");
  assert(propagation.unit == WorkUnit::Propagations &&
         propagation.amount == 1 &&
         propagation.native.mode == RuleWorkBudgetMode::Bounded &&
         propagation.native.amount == 1);
  assert(controller_configuration(config).find(
             "producer \"DirectCspSupport\" 64 64 64 8 10000000 2") !=
         std::string::npos);

  const std::string accepted = controller_configuration(config);
  auto invalid = profile;
  invalid.sequence.front().post_application = PostApplication::Propagate;
  assert(!config.configure_direct_csp_producer_profile(invalid, &error));
  assert(controller_configuration(config) == accepted);
  invalid = profile;
  invalid.sequence.front().rule_work_budget = {};
  assert(!config.configure_direct_csp_producer_profile(invalid, &error));
  assert(controller_configuration(config) == accepted);
  invalid = profile;
  invalid.sequence.erase(invalid.sequence.begin() + 1);
  assert(!config.configure_direct_csp_producer_profile(invalid, &error));
  assert(controller_configuration(config) == accepted);

  RandomControllerConfig disabled;
  assert(!disabled.configure_direct_csp_producer_profile(
      direct_csp_producer_profile(
          disabled.parameters.direct_csp_support_reference, 1, advice),
      &error));
  assert(error.find("requires the native producer") != std::string::npos);
}

static void native_direct_csp_batch_is_replayable_metered_and_observable() {
  Kernel kernel = direct_csp_fixture();
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  DirectCspSupportActionSession session;
  kernel.direct_csp_support_action_session = &session;
  DirectCspFeedback feedback;
  RestartController controller(kernel, {}, &feedback);

  RandomControllerConfig config;
  config.parameters.rules = {"DirectCspSupport", "Propagate",
                             "SemanticVariableAdvice"};
  config.direct_csp_action_producer.enabled = true;
  config.direct_csp_action_producer.maximum_offers = 8;
  const SemanticVariableAdviceParameters advice{64, 4096, 2, 7};
  std::string error;
  assert(config.configure_direct_csp_producer_profile(
      direct_csp_producer_profile(
          config.parameters.direct_csp_support_reference, 3, advice, 2),
      &error));
  RandomControllerSelector selector(5);

  uint64_t producer_calls = 0;
  std::vector<DirectCspSupportActionSelection> applied;
  std::vector<std::string> identities;
  for (unsigned step = 0; step < 4; ++step) {
    ActionRequest request;
    assert(selector.choose(kernel, controller.observe(), config, request,
                           error));
    assert(request.type == ActionType::Rule);
    if (step == 0 || step == 3) {
      assert(request.invocation.rule == "DirectCspSupport");
      assert(request.invocation.post_application ==
             PostApplication::None);
      assert(request.invocation.direct_csp_support_instances.size() == 1);
      assert(kernel_propagation_fixpoint(kernel));
      producer_calls += request.producer_effort.size();
      applied.push_back(
          request.invocation.direct_csp_support_instances.front());

      auto replay_invocation = request.invocation;
      replay_invocation.id = request.sequence;
      const OnlineStrategyProgram encoded{{replay_invocation}, {}};
      const std::string identity = serialize_online_strategy_program(encoded);
      OnlineStrategyProgram decoded;
      std::string replay_error;
      assert(deserialize_online_strategy_program(identity, decoded,
                                                 &replay_error));
      assert(serialize_online_strategy_program(decoded) == identity);
      identities.push_back(identity);
    } else if (step == 1) {
      assert(request.invocation.rule == "Propagate");
      assert(request.invocation.post_application ==
             PostApplication::Propagate);
    } else {
      assert(request.invocation.rule == "SemanticVariableAdvice");
    }
    const auto result = controller.apply_rule(
        request, config.allowance(request.invocation.rule));
    assert(result.invocation.status ==
           (step == 1 ? InvocationStatus::NoChange
                      : InvocationStatus::Completed));
    assert(kernel_propagation_fixpoint(kernel));
  }
  assert(session.committed_keys() == 2 && applied.size() == 2);
  assert(applied[0] != applied[1] && identities[0] != identities[1]);
  // The first bounded census retained the remaining exact rows across the
  // advice-only H mutation. No duplicate discovery or fallback is needed.
  assert(producer_calls == 1);
  assert(feedback.actions.size() == 4);
  assert(feedback.actions.front().request &&
         feedback.actions.front().request->producer_effort.size() == 1 &&
         feedback.actions.front().identity == "DirectCspSupport" &&
         feedback.actions.front().before.sequence == 0 &&
         feedback.actions.front().after.sequence == 1);
  const auto &raw = feedback.actions.front().request->producer_effort.front();
  assert(raw.family == "DirectCspSupport" && raw.offers > 1 &&
         raw.total_work > 0);
  assert(feedback.actions.front().outcome.invocation.made_rule_progress);
  assert(feedback.actions[1].identity == "Propagate" &&
         feedback.actions[1].outcome.invocation.status ==
             InvocationStatus::NoChange &&
         feedback.actions[1].outcome.invocation.work == 0 &&
         feedback.actions[1].outcome.invocation.applications == 1 &&
         !feedback.actions[1].outcome.invocation.made_rule_progress);
  assert(feedback.actions[1].before.state.arena_generation ==
             feedback.actions[1].after.state.arena_generation &&
         feedback.actions[1].before.state.touch_epoch ==
             feedback.actions[1].after.state.touch_epoch &&
         feedback.actions[1].before.state.strategy_epoch ==
             feedback.actions[1].after.state.strategy_epoch);
  assert(feedback.actions[2].identity == "SemanticVariableAdvice" &&
         feedback.actions[3].identity == "DirectCspSupport");
  std::fclose(records);
}

static void native_direct_csp_batch_has_no_fallback() {
  Kernel kernel;
  kernel_reserve_vars(kernel, 2);
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  DirectCspSupportActionSession session;
  kernel.direct_csp_support_action_session = &session;
  RestartController controller(kernel);
  RandomControllerConfig config;
  config.parameters.rules = {"DirectCspSupport", "Propagate",
                             "SemanticVariableAdvice"};
  config.direct_csp_action_producer.enabled = true;
  const SemanticVariableAdviceParameters advice{64, 4096, 2, 7};
  std::string error;
  assert(config.configure_direct_csp_producer_profile(
      direct_csp_producer_profile(
          config.parameters.direct_csp_support_reference, 1, advice, 1),
      &error));
  RandomControllerSelector selector(1);
  ActionRequest request;
  assert(!selector.choose(kernel, controller.observe(), config, request,
                          error));
  assert(error == "DirectCSP startup batch has no eligible support action");
  assert(controller.observe().sequence == 0 && session.committed_keys() == 0);
  std::fclose(records);
}

static void random_guidance_uses_observed_identities() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  RestartController controller(k);
  kernel_offer_local_search_hint(k, LocalSearchKind::DirectCsp, {1, 0, 0});
  kernel_offer_local_search_hint(k, LocalSearchKind::Affine, {0, 1, 0});
  const auto observation = controller.observe();
  assert(observation.local_search_candidates.size() == 2);
  const auto first = observation.local_search_candidates[0].id;
  const auto second = observation.local_search_candidates[1].id;
  RandomControllerConfig config;
  config.parameters.rules = {"Walk", "LocalSearchGuidance"};
  RandomControllerSelector selector(17);
  bool plain = false, seeded_first = false, seeded_second = false, target = false;
  for (unsigned i = 0; i < 200; ++i) {
    ActionRequest request;
    std::string error;
    assert(selector.choose(k, controller.observe(), config, request, error));
    if (request.type != ActionType::Rule) continue;
    if (request.invocation.rule == "Walk") {
      const auto candidate = std::get<WalkParameters>(request.invocation.parameters).seed_candidate;
      assert(candidate == 0 || candidate == first || candidate == second);
      plain |= !candidate;
      seeded_first |= candidate == first;
      seeded_second |= candidate == second;
    } else {
      assert(request.invocation.rule == "LocalSearchGuidance");
      assert(request.invocation.instances.size() == 1);
      const auto candidate = request.invocation.instances[0];
      assert(candidate == first || candidate == second);
      target = true;
    }
  }
  assert(plain && seeded_first && seeded_second && target);
}

static void random_split_walk_selects_a_legal_observed_candidate() {
  // Seed 2 selects Walk, skips semantic-instance enumeration, then chooses
  // legacy's extra zero branch. Exercise that same public policy schedule
  // under walk3, where zero would invalidate the action and terminate UNKNOWN.
  constexpr uint64_t seed = 2;
  const auto initialize = [](Kernel &k) {
    kernel_reserve_vars(k, 3);
    kernel_add_binary_clause(k, internalize(-1), internalize(-2));
    kernel_add_binary_clause(k, internalize(-1), internalize(-3));
    kernel_add_binary_clause(k, internalize(-2), internalize(-3));
    const Lit cover[] = {internalize(1), internalize(2), internalize(3)};
    kernel_add_large_clause(k, cover, 3, false, 0);
    k.h->saved = {1, 1, 1};
    kernel_offer_local_search_partial_hint(k, LocalSearchKind::DirectCsp,
                                          {1, -1, -1});
  };
  char path[] = "/tmp/tactix-split-walk-controller-XXXXXX";
  const int fd = mkstemp(path);
  assert(fd >= 0);
  close(fd);
  for (unsigned mode = 0; mode != 3; ++mode) {
    const bool split = mode != 0;
    const bool stale = mode == 2;
    Kernel k;
    ScopedHeuristics embedding(k);
    initialize(k);
    const uint64_t candidate = k.local_search_hints.front().id;
    if (stale) k.local_search_hints = {};
    RandomControllerConfig config;
    config.parameters.rules = {"Walk"};
    auto &reference = config.parameters.walk_reference;
    reference.seed_candidate = candidate;
    reference.step_budget = 0;
    if (split) {
      reference.objective = WalkParameters::Objective::FullCurrentCnf;
      reference.admission = WalkParameters::Admission::StrictlyBetter;
      reference.preparation_budget = 10000;
    }
    assert(reference.valid());
    std::string error;
    assert(config.configure_search_configurations(
        {{Branching::Vmtf, Restart::Reluctant, Phase::Saved}}, &error));
    DirectCspFeedback feedback;
    ControllerTrace trace;
    if (split && !stale)
      assert(trace.write(path, seed, controller_configuration(config)));
    RandomControllerPolicy policy(seed, config,
                                  split && !stale ? &trace : nullptr,
                                  nullptr, &feedback);
    assert(policy.solve(k) == 10); // Neither invalid-action nor UNKNOWN exit.
    assert(!feedback.actions.empty() && feedback.actions.front().request);
    const auto &first = feedback.actions.front();
    assert(first.request->type == ActionType::Rule &&
           first.request->invocation.rule == "Walk");
    const auto &chosen =
        std::get<WalkParameters>(first.request->invocation.parameters);
    assert(chosen.valid() && chosen.seed_candidate == (split ? candidate : 0));
    assert(chosen.objective == reference.objective &&
           chosen.admission == reference.admission &&
           chosen.preparation_budget == reference.preparation_budget);
    assert(first.outcome.invocation.status != InvocationStatus::InvalidRequest);
    OnlineInvocation exact = first.request->invocation;
    exact.id = 1; // Standalone serialization requires a public positive step ID.
    const OnlineStrategyProgram program{{exact}, {}};
    const auto encoded = serialize_online_strategy_program(program);
    OnlineStrategyProgram decoded;
    assert(deserialize_online_strategy_program(encoded, decoded, &error));
    assert(serialize_online_strategy_program(decoded) == encoded);
    if (split) {
      assert(encoded.find(":walk3(") != std::string::npos);
      if (stale) {
        // No observed candidate preserves the configured identity, so the
        // Walk implementation explicitly declines it without an ordinary run.
        assert(first.before.local_search_candidates.empty() &&
               k.stats.local_search_hints_stale == 1 &&
               !k.stats.walk_seed_admitted && !k.stats.walk_seed_exported &&
               !k.stats.walk_preparation_steps && !k.stats.walk_search_steps &&
               !k.stats.rephased && !first.outcome.invocation.made_rule_progress);
      } else {
        assert(k.stats.walk_seed_admitted == 1 &&
               k.stats.walk_seed_exported == 1 && !k.stats.walk_search_steps);
        assert(first.outcome.invocation.made_rule_progress);
        ControllerTrace replay;
        assert(replay.replay(path, controller_configuration(config)));
        Kernel repeated;
        ScopedHeuristics repeated_embedding(repeated);
        initialize(repeated);
        assert(repeated.local_search_hints.front().id == candidate);
        DirectCspFeedback replay_feedback;
        // The exact public transcript owns the seed, overriding this caller.
        RandomControllerPolicy reader(99, config, &replay, nullptr,
                                      &replay_feedback);
        assert(reader.solve(repeated) == 10 && replay.error().empty());
        assert(!replay_feedback.actions.empty() &&
               replay_feedback.actions.front().request);
        auto replay_exact = replay_feedback.actions.front().request->invocation;
        replay_exact.id = 1;
        assert(serialize_online_strategy_program({{replay_exact}, {}}) == encoded);
        assert(repeated.stats.walk_seed_admitted == 1 &&
               repeated.stats.walk_seed_exported == 1);
        kernel_check_invariants(repeated);
      }
    }
    kernel_extend_model(k);
    // Check all original clauses independently of the Walk proposal.
    const bool a = k.value(internalize(1)) > 0;
    const bool b = k.value(internalize(2)) > 0;
    const bool c = k.value(internalize(3)) > 0;
    assert((a || b || c) && (!a || !b) && (!a || !c) && (!b || !c));
    kernel_check_invariants(k);
  }
  unlink(path);
}

static void guidance_requires_an_observed_candidate() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  RestartController controller(k);
  RandomControllerConfig config;
  config.parameters.rules = {"LocalSearchGuidance"};
  const auto availability = controller_action_availability(k, config);
  const auto found = std::find_if(
      availability.begin(), availability.end(), [](const auto &entry) {
        return entry.rule == "LocalSearchGuidance";
      });
  assert(found != availability.end());
  assert(!found->eligible && found->reason == "candidate-not-observed");

  RandomControllerSelector selector(19);
  for (unsigned i = 0; i < 32; ++i) {
    ActionRequest request;
    std::string error;
    assert(selector.choose(k, controller.observe(), config, request, error));
    assert(request.type == ActionType::ResumeSearch);
  }
}

static void finite_lucky_catalog_is_reachable() {
  Kernel k = fixture(false);
  RestartController controller(k);
  RandomControllerConfig config;
  config.parameters.rules = {"Lucky"};
  const auto original_config = controller_configuration(config);
  RandomControllerSelector selector(23);
  std::set<uint64_t> seen;
  for (unsigned i = 0; i < 4096; ++i) {
    ActionRequest request;
    std::string error;
    assert(selector.choose(k, controller.observe(), config, request, error));
    if (request.type != ActionType::Rule || request.invocation.instances.empty()) continue;
    const uint64_t instance = request.invocation.instances.front();
    seen.insert(instance);
    assert(!lucky_retired_constructor_instance(instance));
    if (instance <= LUCKY_BACKWARD_TRUE || instance == 12 ||
        instance == LUCKY_STARTUP_COUNT || instance == LUCKY_NOL_HYPERGRAPH)
      assert(std::holds_alternative<std::monostate>(
          request.invocation.parameters));
    else if (instance == 6 || instance == LUCKY_STARTUP_PARITY ||
             instance == LUCKY_PARITY_PROOF)
      assert(std::holds_alternative<ParityModelParameters>(
          request.invocation.parameters));
    else if (instance == 7 || instance == 9 ||
             instance == LUCKY_EXACT_ONE_PROOF)
      assert(std::holds_alternative<OnehotModelParameters>(
          request.invocation.parameters));
    else if (instance == 8)
      assert(std::holds_alternative<AffineModelParameters>(
          request.invocation.parameters));
    else if (instance == 10)
      assert(std::holds_alternative<BalancedSignatureModelParameters>(
          request.invocation.parameters));
    else if (instance == 11)
      assert(std::holds_alternative<FermatModelParameters>(
          request.invocation.parameters));
    else if (instance == 13)
      assert(std::holds_alternative<LookupCircuitModelParameters>(
          request.invocation.parameters));
    else if (instance == 14)
      assert(std::holds_alternative<SimonModelParameters>(
          request.invocation.parameters));
    else if (instance == 15)
      assert(std::holds_alternative<StconnProofParameters>(
          request.invocation.parameters));
    else if (instance == 16)
      assert(std::holds_alternative<SignedCardinalityProofParameters>(
          request.invocation.parameters));
    else if (instance == 17)
      assert(std::holds_alternative<ExactCoverModelParameters>(
          request.invocation.parameters));
    else if (instance == 18)
      assert(std::holds_alternative<IndependentSetModelParameters>(
          request.invocation.parameters));
    else if (instance == LUCKY_GRAPH_LABELING)
      assert(std::holds_alternative<GraphLabelingParameters>(
          request.invocation.parameters));
    else if (instance == LUCKY_STARTUP_RAMSEY)
      assert(std::holds_alternative<RamseyStartupParameters>(
          request.invocation.parameters));
    else
      assert(false && "unclassified Lucky instance");
  }
  assert(seen.size() == LUCKY_ACTIVE_INSTANCE_COUNT);
  assert(seen.count(17) && seen.count(18) &&
         seen.count(LUCKY_GRAPH_LABELING));
  config.parameters.graph_labeling_reference.work[0]++;
  assert(controller_configuration(config) != original_config);
  config.parameters.fermat_model_reference.rho_iterations++;
  assert(controller_configuration(config) != original_config);
}

static void complete_pass_prefix_repeats_atomic_doses_until_settled() {
  Kernel kernel;
  kernel_reserve_vars(kernel, 3);
  add_external_clause(kernel, {1, 2, 3});

  RandomControllerConfig config;
  config.parameters.rules = {"SubstitutionRedundancy"};
  OnlineInvocation ulc;
  ulc.id = 1;
  ulc.rule = "SubstitutionRedundancy";
  SubstitutionRedundancyParameters sr;
  sr.producer = SubstitutionRedundancyProducer::Orbitopal;
  sr.maximum_orbitopal_matrices = 0;
  sr.complete_unique_literal_amos = true;
  ulc.parameters = sr;
  ulc.granularity = InvocationGranularity::CompleteCurrentPass;
  ulc.work_unit = WorkUnit::Applications;
  ulc.budget = WorkLimit::bounded(8);
  ulc.rule_work_budget = {RuleWorkBudgetMode::Bounded, 1'000'000};
  std::string error;
  assert(config.configure_startup_prefix(
      OnlineStrategyProgram{{ulc}, {RepeatMode::Once, 1}}, &error));

  RestartController controller(kernel);
  RandomControllerSelector selector(1);
  ActionRequest request;
  for (unsigned dose = 0; dose < 3; ++dose) {
    assert(selector.choose(kernel, controller.observe(), config, request, error));
    assert(request.type == ActionType::Rule &&
           request.invocation.rule == "SubstitutionRedundancy");
    const auto result = controller.apply_rule(
        request, config.allowance(request.invocation.rule));
    assert(result.invocation.status == InvocationStatus::Completed &&
           result.invocation.applications == 1 &&
           result.invocation.made_rule_progress &&
           result.invocation.selection_complete == (dose == 2));
  }
  assert(kernel.stats.sr_unique_literal_amo_binaries == 3);
  assert(selector.choose(kernel, controller.observe(), config, request, error));
  assert(request.type == ActionType::ResumeSearch);
}

static void ordered_startup_prefix_reaches_native_nary_after_ulc() {
  RandomControllerConfig empty;
  const std::string empty_signature = controller_configuration(empty);
  std::string error;
  assert(empty.configure_startup_prefix({}, &error));
  assert(error.empty() && controller_configuration(empty) == empty_signature &&
         empty_signature.find("startup-prefix") == std::string::npos);

  Kernel kernel;
  kernel_reserve_vars(kernel, 12);
  for (int first : {1, 4, 7, 10})
    add_external_clause(kernel, {first, first + 1, first + 2});
  add_external_clause(kernel, {-1, -4, -7});
  add_external_clause(kernel, {-2, -5, -8});
  add_external_clause(kernel, {-3, -6, -9});
  kernel_assign_unit(kernel, internalize(10));
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  NaryCspPrefixActionSession session;
  kernel.nary_csp_prefix_action_session = &session;

  RandomControllerConfig config;
  config.parameters.rules = {"SubstitutionRedundancy", "Propagate",
                             "NaryCspPrefix"};
  config.nary_csp_action_producer.enabled = true;
  config.nary_csp_action_producer.diagnostic_census_once = true;
  config.nary_csp_action_producer.maximum_domain_candidates = 16;
  config.nary_csp_action_producer.maximum_relation_candidates = 4;
  config.nary_csp_action_producer.maximum_prefix_candidates = 32;
  config.nary_csp_action_producer.maximum_offers = 4;
  config.nary_csp_action_producer.maximum_enumeration_work = 1'000'000;

  OnlineInvocation ulc;
  ulc.id = 1;
  ulc.rule = "SubstitutionRedundancy";
  SubstitutionRedundancyParameters sr;
  sr.producer = SubstitutionRedundancyProducer::Orbitopal;
  sr.maximum_orbitopal_matrices = 0;
  sr.complete_unique_literal_amos = true;
  ulc.parameters = sr;
  ulc.granularity = InvocationGranularity::CompleteCurrentPass;
  ulc.work_unit = WorkUnit::Applications;
  ulc.budget = WorkLimit::bounded(32);
  ulc.rule_work_budget = {RuleWorkBudgetMode::Bounded, 10'000'000};

  OnlineInvocation propagate;
  propagate.id = 2;
  propagate.rule = "Propagate";
  propagate.granularity = InvocationGranularity::CanonicalApplication;
  propagate.work_unit = WorkUnit::Propagations;
  propagate.budget = WorkLimit::bounded(1'000'000);
  propagate.rule_work_budget = {RuleWorkBudgetMode::Bounded, 1'000'000};

  OnlineInvocation nary;
  nary.id = 3;
  nary.rule = "NaryCspPrefix";
  nary.parameters = config.parameters.nary_csp_prefix_reference;
  nary.granularity = InvocationGranularity::NativeProducerOffer;
  nary.work_unit = WorkUnit::Applications;
  nary.budget = WorkLimit::bounded(1);
  nary.rule_work_budget = {RuleWorkBudgetMode::Bounded, 10'000'000};

  OnlineStrategyProgram prefix{{ulc, propagate, nary}, {RepeatMode::Once, 1}};
  const std::string encoded_prefix =
      serialize_online_strategy_program(prefix);
  OnlineStrategyProgram decoded_prefix;
  assert(!deserialize_online_strategy_program(encoded_prefix, decoded_prefix,
                                               &error));
  assert(error == "native producer offer is controller-prefix-only");
  assert(deserialize_online_strategy_program(encoded_prefix, decoded_prefix,
                                              &error, false, true));
  assert(serialize_online_strategy_program(decoded_prefix) == encoded_prefix);
  assert(config.configure_startup_prefix(prefix, &error));
  assert(error.empty());
  const std::string signature = controller_configuration(config);
  assert(signature.find("startup-prefix") != std::string::npos);
  assert(!config.configure_startup_prefix({}, &error));
  assert(error == "controller startup prefix is already installed" &&
         controller_configuration(config) == signature);

  RestartController controller(kernel);
  RandomControllerSelector selector(1);
  ActionRequest request;
  uint64_t ulc_applications = 0;
  uint64_t ulc_effective_doses = 0;
  ActionResult result;
  for (unsigned doses = 0; doses < 32; ++doses) {
    assert(selector.choose(kernel, controller.observe(), config, request, error));
    if (request.invocation.rule != "SubstitutionRedundancy")
      break;
    assert(request.type == ActionType::Rule && request.producer_effort.empty());
    result = controller.apply_rule(
        request, config.allowance(request.invocation.rule));
    assert(result.invocation.status == InvocationStatus::Completed ||
           result.invocation.status == InvocationStatus::NoChange);
    ulc_applications += result.invocation.applications;
    ulc_effective_doses += result.invocation.made_rule_progress;
  }
  assert(request.type == ActionType::Rule &&
         request.invocation.rule == "Propagate" &&
         ulc_applications == 11 && ulc_effective_doses == 11 &&
         kernel.stats.sr_unique_literal_amo_binaries == 9);

  assert(request.producer_effort.empty());
  result = controller.apply_rule(request,
                                 config.allowance(request.invocation.rule));
  assert(result.invocation.status == InvocationStatus::Completed &&
         result.invocation.selection_complete &&
         kernel_propagation_fixpoint(kernel) &&
         kernel.value(internalize(-11)) > 0 &&
         kernel.value(internalize(-12)) > 0);

  assert(selector.choose(kernel, controller.observe(), config, request, error));
  assert(request.type == ActionType::Rule &&
         request.invocation.rule == "NaryCspPrefix" &&
         request.invocation.granularity == InvocationGranularity::OneInstance &&
         request.invocation.nary_csp_prefix_instances.size() == 1 &&
         request.producer_effort.size() == 1);
  const auto &effort = request.producer_effort.front();
  assert(effort.family == "NaryCspPrefix" &&
         effort.have_detector_census && effort.exact_domains == 3 &&
         effort.relation_keys == 1 && effort.tuple_occurrences == 3 &&
         effort.offers);
  result = controller.apply_rule(request,
                                 config.allowance(request.invocation.rule));
  assert(result.invocation.status == InvocationStatus::Completed &&
         result.invocation.selection_complete &&
         result.invocation.made_rule_progress && session.committed_keys() == 1);

  assert(selector.choose(kernel, controller.observe(), config, request, error));
  assert(request.type == ActionType::ResumeSearch);
  ++kernel.strategy_epoch;
  result = controller.resume_search(request);
  assert(!result.resumed_search &&
         result.invocation.status == InvocationStatus::StaleRequest);
  assert(!selector.choose(kernel, controller.observe(), config, request,
                          error));
  assert(error ==
         "required controller startup ResumeSearch result is unavailable");
  std::fclose(records);
}

static void exact_structured_startup_payload_needs_no_static_offer() {
  Kernel kernel = fixture(false);
  RestartController controller(kernel);
  RandomControllerConfig config;
  config.parameters.rules = {"Condition"};
  OnlineInvocation condition;
  condition.id = 1;
  condition.rule = "Condition";
  condition.granularity = InvocationGranularity::OneInstance;
  condition.condition_instances.push_back(
      {{internalize(1), internalize(2)}, {internalize(2)}});
  condition.work_unit = WorkUnit::Applications;
  condition.budget = WorkLimit::bounded(1);
  condition.rule_work_budget = config.default_allowance.native;
  std::string error;
  assert(config.configure_startup_prefix(
      OnlineStrategyProgram{{condition}, {RepeatMode::Once, 1}}, &error));
  assert(!config.offered_actions.contains("Condition"));

  RandomControllerSelector selector(1);
  ActionRequest request;
  assert(selector.choose(kernel, controller.observe(), config, request,
                         error));
  assert(request.type == ActionType::Rule &&
         request.invocation.rule == "Condition" &&
         request.invocation.condition_instances.size() == 1 &&
         request.invocation.condition_instances.front().clause ==
             condition.condition_instances.front().clause &&
         request.invocation.condition_instances.front().witness ==
             condition.condition_instances.front().witness);
  const auto result = controller.apply_rule(request, config.allowance("Condition"));
  assert(result.invocation.status == InvocationStatus::Completed &&
         result.invocation.selection_complete &&
         result.invocation.made_rule_progress);
  assert(selector.choose(kernel, controller.observe(), config, request,
                         error));
  assert(request.type == ActionType::ResumeSearch);
}

static void required_startup_native_offer_fails_closed_with_effort() {
  Kernel kernel;
  kernel_reserve_vars(kernel, 9);
  for (int first : {1, 4, 7}) {
    add_external_clause(kernel, {first, first + 1, first + 2});
    for (int a = first; a != first + 3; ++a)
      for (int b = a + 1; b != first + 3; ++b)
        add_external_clause(kernel, {-a, -b});
  }
  NaryCspPrefixActionSession session;
  kernel.nary_csp_prefix_action_session = &session;
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;

  RandomControllerConfig config;
  config.parameters.rules = {"NaryCspPrefix"};
  config.nary_csp_action_producer.enabled = true;
  config.nary_csp_action_producer.diagnostic_census_once = true;
  OnlineInvocation nary;
  nary.id = 1;
  nary.rule = "NaryCspPrefix";
  nary.parameters = config.parameters.nary_csp_prefix_reference;
  nary.granularity = InvocationGranularity::NativeProducerOffer;
  nary.work_unit = WorkUnit::Applications;
  nary.budget = WorkLimit::bounded(1);
  nary.rule_work_budget = {RuleWorkBudgetMode::Bounded, 1'000'000};
  std::string error;
  assert(config.configure_startup_prefix(
      OnlineStrategyProgram{{nary}, {RepeatMode::Once, 1}}, &error));

  RestartController controller(kernel);
  RandomControllerSelector selector(1);
  ActionRequest request;
  assert(!selector.choose(kernel, controller.observe(), config, request, error));
  assert(error ==
         "required controller startup prefix action is unavailable: NaryCspPrefix");
  assert(request.sequence == 1 && request.producer_effort.size() == 1);
  const auto &effort = request.producer_effort.front();
  assert(effort.family == "NaryCspPrefix" && effort.have_detector_census &&
         effort.candidate_domains == 12 && effort.exact_domains == 3 &&
         effort.offers == 0);
  std::fclose(records);
}

int main() {
  native_affine_profile_is_exact_transactional_and_effective();
  exact_search_configuration_collection_is_learner_owned();
  native_affine_basis_offer_is_exact_and_effective();
  native_affine_empty_census_is_durable_across_restarts();
  native_affine_completed_census_keeps_additional_rows();
  native_affine_bounded_startup_batch_preserves_exact_pages();
  native_affine_bounded_startup_batch_crosses_propagation();
  native_direct_csp_profile_is_exact_and_transactional();
  native_direct_csp_batch_is_replayable_metered_and_observable();
  native_direct_csp_batch_has_no_fallback();
  finite_lucky_catalog_is_reachable();
  complete_pass_prefix_repeats_atomic_doses_until_settled();
  ordered_startup_prefix_reaches_native_nary_after_ulc();
  exact_structured_startup_payload_needs_no_static_offer();
  required_startup_native_offer_fails_closed_with_effort();
  guidance_requires_an_observed_candidate();
  random_guidance_uses_observed_identities();
  random_split_walk_selects_a_legal_observed_candidate();
  Kernel a = fixture(false), b = fixture(false);
  RestartController ca(a), cb(b);
  RandomControllerSelector sa(17), sb(17);
  RandomControllerConfig config;
  bool saw_rule = false, saw_resume = false;
  for (unsigned i = 0; i < 100; ++i) {
    // Extra observations never alter subsequent action requests.
    ca.observe();
    ca.observe();
    ActionRequest ra, rb;
    std::string error;
    assert(sa.choose(a, ca.observe(), config, ra, error));
    assert(sb.choose(b, cb.observe(), config, rb, error));
    assert(ra.type == rb.type && ra.sequence == rb.sequence);
    assert(ra.search == rb.search);
    if (ra.type == ActionType::ResumeSearch) {
      saw_resume = true;
      assert(valid(ra.search));
    } else {
      saw_rule = true;
      assert(root_exclusion(tactix_rules()[tactix_rule_id(
          ra.invocation.rule.c_str())]).empty());
      OnlineStrategyProgram pa, pb;
      pa.sequence.push_back(ra.invocation);
      pb.sequence.push_back(rb.invocation);
      assert(serialize_online_strategy_program(pa) ==
             serialize_online_strategy_program(pb));
      const auto allowance = config.allowance(ra.invocation.rule);
      assert(ra.invocation.work_unit == allowance.unit);
      assert(ra.invocation.budget.amount() == allowance.amount);
      assert(ra.invocation.rule_work_budget.amount ==
             config.default_allowance.native.amount);
      // The controller supplies the executor's internal step identity and
      // once-only repetition; generated semantic payloads must then preflight.
      pa.sequence[0].id = 1;
      pa.sequence[0].repeat = {RepeatMode::Once, 1};
      std::string validation_error;
      assert(validate_online_strategy_program(pa, &validation_error));
    }
  }
  assert(saw_rule && saw_resume);
  {
    Kernel k = fixture(false);
    RestartController controller(k);
    RandomControllerConfig walk_only;
    walk_only.parameters.rules = {"Walk"};
    RandomControllerSelector chooser(17);
    unsigned walks = 0;
    for (unsigned i = 0; i < 100; ++i) {
      ActionRequest request;
      std::string error;
      assert(chooser.choose(k, controller.observe(), walk_only, request, error));
      if (request.type == ActionType::ResumeSearch)
        continue;
      ++walks;
      assert(request.invocation.rule == "Walk");
      assert(request.invocation.granularity == InvocationGranularity::CanonicalApplication);
      assert(request.invocation.instances.empty());
    }
    assert(walks);
  }
  {
    Kernel k = fixture(false);
    RestartController controller(k);
    RandomControllerConfig offered;
    offered.parameters.rules = {"Condition"};
    const auto unavailable = controller_action_availability(k, offered);
    bool accounted = false;
    for (const auto &entry : unavailable)
      if (entry.rule == "Condition") {
        assert(!entry.eligible && entry.reason == "structured-witness-not-offered");
        accounted = true;
      }
    assert(accounted);
    OnlineInvocation condition;
    condition.rule = "Condition";
    condition.granularity = InvocationGranularity::OneInstance;
    condition.condition_instances.push_back(
        {{internalize(1), internalize(2)}, {internalize(2)}});
    offered.offered_actions["Condition"] = {condition};
    OnlineStrategyProgram collection;
    condition.id = 1;
    condition.budget = WorkLimit::bounded(1);
    condition.rule_work_budget = offered.default_allowance.native;
    collection.sequence = {condition};
    assert(offered.configure_actions(collection));
    const auto accepted = controller_configuration(offered);
    auto inconsistent = collection;
    inconsistent.sequence.push_back(condition);
    inconsistent.sequence.back().id = 2;
    inconsistent.sequence.back().budget = WorkLimit::bounded(2);
    assert(!offered.configure_actions(inconsistent));
    assert(controller_configuration(offered) == accepted);
    RandomControllerSelector chooser(1);
    bool executed = false;
    for (unsigned i = 0; i < 20 && !executed; ++i) {
      ActionRequest request;
      std::string error;
      assert(chooser.choose(k, controller.observe(), offered, request, error));
      if (request.type == ActionType::Rule) {
        const auto result = controller.apply_rule(request, offered.allowance("Condition"));
        assert(result.invocation.applications == 1);
        assert(result.invocation.made_rule_progress);
        assert(!kernel_binary_live(k, internalize(1), internalize(2)));
        executed = true;
      }
    }
    assert(executed);
  }
  for (uint64_t seed : {1, 7, 17}) {
    Kernel sat = fixture(false), unsat = fixture(true);
    RandomControllerPolicy ps(seed), pu(seed);
    assert(ps.solve(sat) == 10);
    kernel_extend_model(sat);
    // Both original clauses require x2; check the reconstructed assignment.
    assert(sat.value(internalize(2)) > 0);
    assert(pu.solve(unsat) == 20);
    assert(unsat.inconsistent);
    assert(!sat.pending.valid && !unsat.pending.valid);
  }
  Kernel stopped = fixture(true);
  volatile sig_atomic_t stop = 1;
  RandomControllerPolicy policy(1);
  assert(policy.solve(stopped, &stop) == 0);
  assert(!stopped.h && !stopped.saved_phases && !stopped.stop);
  for (const bool conflict_boundary : {false, true}) {
    Kernel k = fixture(false);
    if (conflict_boundary)
      kernel_add_binary_clause(k, internalize(-1), internalize(-2));
    ScopedHeuristics embedding(k);
    kernel_reserve_vars(k, 3);
    kernel_assign_unit(k, internalize(3));
    apply_rule(tactix_rules()[tactix_rule_id("Propagate")], k);
    assert(decide_apply(k, internalize(1), false) == ApplyResult::Ok);
    if (conflict_boundary) {
      apply_rule(tactix_rules()[tactix_rule_id("Propagate")], k);
      assert(k.conflict_valid);
    }
    const auto level = k.level;
    const auto binaries = k.watches.binary_clauses();
    const auto restarts = k.stats.restarts;
    RandomControllerConfig config;
    config.parameters.rules = {"Rephase"};
    RandomControllerPolicy interrupted(17, config);
    volatile sig_atomic_t stop = 1;
    assert(interrupted.solve(k, &stop) == 0);
    assert(k.level == level && k.stats.restarts == restarts);
    stop = 0;
    assert(interrupted.solve(k, &stop) == 10);
    assert(k.stats.restarts > restarts);
    assert(interrupted.coverage().full_restarts == 1);
    assert(interrupted.coverage().configuration_changes == 0);
    assert(k.watches.binary_clauses() >= binaries);
    assert(k.value(internalize(3)) > 0 &&
           k.assigned[var_of(internalize(3))].level == 0);
    kernel_extend_model(k);
    assert(k.value(internalize(2)) > 0);
    if (conflict_boundary)
      assert(k.value(internalize(1)) < 0);
  }
  {
    char path[] = "/tmp/tactix-controller-transcript-XXXXXX";
    const int fd = mkstemp(path);
    assert(fd >= 0);
    close(fd);
    RandomControllerConfig replay_config;
    replay_config.parameters.rules = {"Rephase"};
    const auto signature = controller_configuration(replay_config);
    {
      ControllerTrace record;
      assert(record.write(path, 17, signature));
      Kernel original = fixture(true);
      RandomControllerPolicy writer(17, replay_config, &record);
      assert(writer.solve(original) == 20);
    }
    {
      ControllerTrace replay;
      assert(replay.replay(path, signature));
      Kernel repeated = fixture(true);
      // The transcript supplies its seed; the caller's different seed cannot
      // silently select a different run.
      RandomControllerPolicy reader(99, replay_config, &replay);
      assert(reader.solve(repeated) == 20);
      assert(repeated.inconsistent);
    }
    {
      ControllerTrace mismatch;
      auto changed = replay_config;
      ++changed.default_allowance.native.amount;
      assert(!mismatch.replay(path, controller_configuration(changed)));
    }
    std::ifstream input(path);
    std::string transcript((std::istreambuf_iterator<char>(input)), {});
    input.close();
    assert(transcript.find("service Decide") != std::string::npos);
    assert(transcript.find("service ConflictAnalysis") != std::string::npos);
    const auto terminal = transcript.rfind("verdict 20");
    assert(terminal != std::string::npos);
    transcript.replace(terminal, 10, "verdict 10");
    {
      std::ofstream corrupt(path);
      corrupt << transcript;
    }
    {
      ControllerTrace replay;
      assert(replay.replay(path, signature));
      Kernel repeated = fixture(true);
      RandomControllerPolicy reader(17, replay_config, &replay);
      assert(reader.solve(repeated) == -1);
      assert(!replay.error().empty());
    }
    unlink(path);
  }
}
