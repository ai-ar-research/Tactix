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
    const AffineConsequenceParameters &parameters, uint64_t allowance = 3) {
  OnlineInvocation step;
  step.id = 91;
  step.rule = "AffineConsequence";
  step.parameters = parameters;
  step.granularity = InvocationGranularity::CanonicalApplication;
  step.repeat = {RepeatMode::Once, 1};
  step.rule_work_budget = {RuleWorkBudgetMode::Bounded, 123456};
  step.work_unit = WorkUnit::Applications;
  step.budget = WorkLimit::bounded(allowance);
  return {{step}, {RepeatMode::Once, 1}};
}

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

  RandomControllerConfig raised;
  raised.parameters.rules = {"AffineConsequence"};
  raised.affine_basis_action_producer.enabled = true;
  assert(raised.configure_affine_basis_producer_profile(
      affine_producer_profile(
          raised.parameters.affine_consequence_reference),
      &error));
  RandomControllerSelector raised_selector(7);
  bool offered = false;
  for (unsigned attempt = 0; attempt < 32 && !offered; ++attempt) {
    ActionRequest request;
    assert(raised_selector.choose(kernel, controller.observe(), raised, request,
                                  error));
    offered = request.type == ActionType::Rule &&
              request.invocation.rule == "AffineConsequence" &&
              request.invocation.affine_basis_instances.size() == 1;
  }
  assert(offered);
  std::fclose(records);
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
    applied = true;
  }
  assert(applied);
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

int main() {
  native_affine_profile_is_exact_transactional_and_effective();
  native_affine_basis_offer_is_exact_and_effective();
  finite_lucky_catalog_is_reachable();
  guidance_requires_an_observed_candidate();
  random_guidance_uses_observed_identities();
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
