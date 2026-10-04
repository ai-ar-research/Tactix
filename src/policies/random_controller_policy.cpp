#include "policies/random_controller_policy.hpp"

#include "policies/stress_common.hpp"
#include "rules/decide/decide.hpp"
#include "rules/lucky/lucky.hpp"

#include <cstdio>
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace tactix {
using namespace strategy;
using namespace strategy::controller;

bool RandomControllerConfig::configure_actions(const OnlineStrategyProgram &program,
                                               std::string *error) {
  const auto reject = [&](const char *message) {
    if (error) *error = message;
    return false;
  };
  if (!validate_online_strategy_program(program, error))
    return false;
  if (program.sequence.empty())
    return reject("controller action collection is empty");
  auto replacement = *this;
  replacement.offered_actions.clear();
  replacement.allowances.clear();
  replacement.parameters.rules.clear();
  for (const auto &step : program.sequence) {
    const int id = tactix_rule_id(step.rule.c_str());
    if (id < 0 || !root_exclusion(tactix_rules()[id]).empty())
      return reject("controller collection contains an unknown or excluded root action");
    if (step.budget.is_unlimited())
      return reject("controller action collection requires fixed composition allowances");
    ActionAllowance allowance{step.work_unit, step.budget.amount(), step.rule_work_budget};
    if (const auto found = replacement.allowances.find(step.rule);
        found != replacement.allowances.end()) {
      const auto previous = found->second;
      if (previous.unit != allowance.unit || previous.amount != allowance.amount ||
          previous.native.mode != allowance.native.mode ||
          previous.native.amount != allowance.native.amount)
        return reject("one controller family has inconsistent effort allowances");
    } else {
      replacement.parameters.rules.push_back(step.rule);
      replacement.allowances[step.rule] = allowance;
    }
    replacement.offered_actions[step.rule].push_back(step);
  }
  *this = std::move(replacement);
  if (error) error->clear();
  return true;
}

std::vector<ControllerActionAvailability>
controller_action_availability(const Kernel &k, const RandomControllerConfig &config) {
  std::vector<ControllerActionAvailability> result;
  for (const auto &rule : tactix_rules()) {
    ControllerActionAvailability entry{rule.name, false, {}};
    if (const auto excluded = root_exclusion(rule); !excluded.empty())
      entry.reason = excluded;
    else if (!config.parameters.rules.empty() &&
             std::find(config.parameters.rules.begin(), config.parameters.rules.end(),
                       rule.name) == config.parameters.rules.end())
      entry.reason = "outside-configured-action-collection";
    else if (k.level || k.inconsistent || k.conflict_valid)
      entry.reason = "not-a-consistent-root-boundary";
    else if (!rule.guard(k))
      entry.reason = "guard-false";
    else if ((std::string_view(rule.name) == "Condition" ||
              std::string_view(rule.name) == "IndependentSetCover" ||
              std::string_view(rule.name) == "Gf3AffineConsequence" ||
              std::string_view(rule.name) == "DirectCspSupport") &&
             (!config.offered_actions.contains(rule.name) ||
              config.offered_actions.at(rule.name).empty()))
      entry.reason = "structured-witness-not-offered";
    else if (std::string_view(rule.name) == "LocalSearchGuidance" &&
             std::none_of(k.local_search_hints.begin(),
                          k.local_search_hints.end(),
                          [](const auto &hint) { return hint.available(); }))
      entry.reason = "candidate-not-observed";
    else
      entry.eligible = true;
    result.push_back(std::move(entry));
  }
  return result;
}

static ActionParameters reference_parameters(const Kernel &k,
                                            const std::string &rule,
                                            const RandomOnlineConfig &profile) {
  if (rule == "BVE")
    return BveParameters::from_kernel(k);
#define CONTROLLER_REFERENCE(identity, field) \
  if (rule == identity) return profile.field
  CONTROLLER_REFERENCE("SubsumeForward", subsume_reference);
  CONTROLLER_REFERENCE("Strengthen", strengthen_reference);
  CONTROLLER_REFERENCE("Congruence", congruence_reference);
  CONTROLLER_REFERENCE("ParityConsequence", parity_consequence_reference);
  CONTROLLER_REFERENCE("AffineConsequence", affine_consequence_reference);
  CONTROLLER_REFERENCE("ExactCoverExtension", exact_cover_extension_reference);
  CONTROLLER_REFERENCE("IndependentSetCover", independent_set_cover_reference);
  CONTROLLER_REFERENCE("Gf3AffineConsequence", gf3_affine_consequence_reference);
  CONTROLLER_REFERENCE("DirectCspSupport", direct_csp_support_reference);
  CONTROLLER_REFERENCE("SemanticVariableAdvice", semantic_variable_advice_reference);
  CONTROLLER_REFERENCE("Instantiate", instantiate_reference);
  CONTROLLER_REFERENCE("Backbone", backbone_reference);
  CONTROLLER_REFERENCE("Sweep", sweep_reference);
  CONTROLLER_REFERENCE("Vivify", vivify_reference);
  CONTROLLER_REFERENCE("BCE", bce_reference);
  CONTROLLER_REFERENCE("HyperTernaryRes", ternary_reference);
  CONTROLLER_REFERENCE("Factor", factor_reference);
  CONTROLLER_REFERENCE("TransitiveReduce", transitive_reference);
  CONTROLLER_REFERENCE("Substitute", substitute_reference);
  CONTROLLER_REFERENCE("Reduce", reduce_reference);
  CONTROLLER_REFERENCE("Walk", walk_reference);
  CONTROLLER_REFERENCE("Reorder", reorder_reference);
#undef CONTROLLER_REFERENCE
  if (rule == "SubstitutionRedundancy") {
    auto sr = profile.sr_reference;
    if (sr.producer == SubstitutionRedundancyProducer::ExplicitOnly)
      sr.producer = SubstitutionRedundancyProducer::Orbitopal;
    sr.maximum_validation_ticks = profile.maximum_sr_validation_ticks;
    sr.maximum_detection_ticks = profile.maximum_sr_detection_ticks;
    sr.maximum_orbitopal_matrices = profile.maximum_sr_matrices;
    sr.maximum_orbitopal_instances = profile.maximum_sr_instances;
    sr.maximum_general_candidates_per_advance =
        profile.maximum_sr_general_candidates_per_advance;
    return sr;
  }
  // Monostate inherits the kernel's fixed reference settings, rather than
  // sampling effort or semantic limits on each request.
  return {};
}

std::string controller_configuration(const RandomControllerConfig &config) {
  std::ostringstream out;
  out << std::setprecision(17);
  const auto &r = config.search.restart;
  out << "search " << r.initial_interval << ' ' << r.glue_margin << ' '
      << r.fast_ema_window << ' ' << r.slow_ema_window << ' '
      << r.logarithm_offset << ' ' << r.logarithm_bias << ' '
      << r.reluctant_period << ' ' << r.reluctant_limit << ' '
      << config.search.branching.focused_phase_switch_span;
  for (const auto phase : config.search.branching.focused_phase_pattern)
    out << ',' << static_cast<int>(phase);
  out << " trail-retention "
      << (config.reuse_search_trail ? "ranked-prefix" : "full")
      << " window " << config.parameters.maximum_selected_batch;
  for (const auto &rule : config.parameters.rules)
    out << " family " << std::quoted(rule);
  Kernel reference;
  OnlineStrategyProgram profiles;
  for (const auto &rule : tactix_rules()) {
    if (!root_exclusion(rule).empty())
      continue;
    if (std::string_view(rule.name) == "Gf3AffineConsequence" ||
        std::string_view(rule.name) == "DirectCspSupport")
      // These families have no canonical action.  Their complete typed
      // requests, including limits, are fingerprinted below as offered
      // actions; inventing an empty profile here would serialize a program
      // that the public validator correctly rejects.
      continue;
    OnlineInvocation invocation;
    invocation.id = profiles.sequence.size() + 1;
    invocation.rule = rule.name;
    invocation.parameters = reference_parameters(reference, rule.name, config.parameters);
    invocation.granularity = InvocationGranularity::CanonicalApplication;
    const auto allowance = config.allowance(rule.name);
    invocation.work_unit = allowance.unit;
    invocation.budget = WorkLimit::bounded(allowance.amount);
    invocation.rule_work_budget = allowance.native;
    profiles.sequence.push_back(std::move(invocation));
    if (std::string_view(rule.name) == "Lucky") {
      auto balanced = profiles.sequence.back();
      balanced.id = profiles.sequence.size() + 1;
      balanced.granularity = InvocationGranularity::OneInstance;
      balanced.instances = {10};
      balanced.parameters =
          config.parameters.balanced_signature_model_reference;
      profiles.sequence.push_back(std::move(balanced));
      auto fermat = profiles.sequence.back();
      fermat.id = profiles.sequence.size() + 1;
      fermat.instances = {11};
      fermat.parameters = config.parameters.fermat_model_reference;
      profiles.sequence.push_back(std::move(fermat));
      auto lookup = profiles.sequence.back();
      lookup.id = profiles.sequence.size() + 1;
      lookup.instances = {13};
      lookup.parameters = config.parameters.lookup_circuit_model_reference;
      profiles.sequence.push_back(std::move(lookup));
      auto simon = profiles.sequence.back();
      simon.id = profiles.sequence.size() + 1;
      simon.instances = {14};
      simon.parameters = config.parameters.simon_model_reference;
      profiles.sequence.push_back(std::move(simon));
      for (const auto &[instance, parameters] :
           std::initializer_list<std::pair<uint64_t, ActionParameters>>{
               {17, config.parameters.exact_cover_model_reference},
               {18, config.parameters.independent_set_model_reference},
               {LUCKY_GRAPH_LABELING,
                config.parameters.graph_labeling_reference}}) {
        auto constructor = profiles.sequence.back();
        constructor.id = profiles.sequence.size() + 1;
        constructor.instances = {instance};
        constructor.parameters = parameters;
        profiles.sequence.push_back(std::move(constructor));
      }
      auto ramsey = profiles.sequence.back();
      ramsey.id = profiles.sequence.size() + 1;
      ramsey.instances = {LUCKY_STARTUP_RAMSEY};
      ramsey.parameters = config.parameters.ramsey_startup_reference;
      profiles.sequence.push_back(std::move(ramsey));
    }
  }
  out << " profiles " << std::quoted(serialize_online_strategy_program(profiles));
  for (const auto &[family, choices] : config.offered_actions) {
    OnlineStrategyProgram offered;
    offered.sequence = choices;
    out << " offered " << std::quoted(family) << ' '
        << std::quoted(serialize_online_strategy_program(offered));
  }
  // Disabled producers are deliberately absent so introducing the opt-in
  // mechanism cannot alter an existing controller fingerprint.
  if (config.gf3_action_producer.enabled) {
    const auto &producer = config.gf3_action_producer;
    OnlineInvocation profile;
    profile.rule = "Gf3AffineConsequence";
    profile.parameters = config.parameters.gf3_affine_consequence_reference;
    profile.granularity = InvocationGranularity::OneInstance;
    profile.gf3_affine_instances = {{0, 1}};
    out << " producer " << std::quoted(profile.rule) << ' '
        << producer.maximum_pair_candidates << ' '
        << producer.maximum_offers << ' '
        << producer.maximum_enumeration_work << ' '
        << std::quoted(serialize_online_strategy_program({{profile}, {}}));
  }
  if (config.direct_csp_action_producer.enabled) {
    const auto &producer = config.direct_csp_action_producer;
    OnlineInvocation profile;
    profile.rule = "DirectCspSupport";
    profile.parameters = config.parameters.direct_csp_support_reference;
    profile.granularity = InvocationGranularity::OneInstance;
    profile.direct_csp_support_instances = {{{1, 2}, {3, 4}, 0}};
    out << " producer " << std::quoted(profile.rule) << ' '
        << producer.maximum_domain_candidates << ' '
        << producer.maximum_pair_candidates << ' '
        << producer.maximum_row_candidates << ' '
        << producer.maximum_offers << ' '
        << producer.maximum_enumeration_work << ' '
        << std::quoted(serialize_online_strategy_program({{profile}, {}}));
  }
  return out.str();
}

static std::string request_record(const ActionRequest &request) {
  std::ostringstream out;
  out << "request " << request.version << ' ' << request.sequence << ' '
      << request.expected.arena_generation << ' ' << request.expected.touch_epoch
      << ' ' << request.expected.strategy_epoch << ' ';
  if (request.type == ActionType::ResumeSearch)
    out << "ResumeSearch " << name(request.search);
  else {
    OnlineStrategyProgram program;
    program.sequence.push_back(request.invocation);
    out << "Rule " << serialize_online_strategy_program(program);
  }
  for (const auto &effort : request.producer_effort)
    out << " producer " << std::quoted(effort.family) << ' '
        << static_cast<unsigned>(effort.status) << ' ' << effort.complete << ' '
        << effort.preparation_work << ' ' << effort.enumeration_work << ' '
        << effort.total_work << ' ' << effort.candidates << ' '
        << effort.offers;
  return out.str();
}

static std::string result_record(const ActionResult &result) {
  std::ostringstream out;
  const auto &invocation = result.invocation;
  out << "result " << result.sequence << ' '
      << static_cast<unsigned>(invocation.status) << ' '
      << static_cast<unsigned>(invocation.rule_result) << ' '
      << static_cast<unsigned>(invocation.budget_status) << ' '
      << static_cast<unsigned>(invocation.work_unit) << ' '
      << invocation.work << ' ' << invocation.applications << ' '
      << invocation.made_rule_progress << ' ' << invocation.selection_complete << ' '
      << result.propagation_ticks << ' ' << result.root_assignments << ' '
      << result.after.arena_generation << ' ' << result.after.touch_epoch << ' '
      << result.after.strategy_epoch << ' ' << result.resumed_search << ' '
      << result.composition_overshoot;
  return out.str();
}

static std::string service_record(const Kernel &k, const char *service,
                                 uint64_t choice, ApplyResult result) {
  std::ostringstream out;
  out << "service " << service << ' ' << choice << ' '
      << static_cast<unsigned>(result) << ' ' << k.level << ' '
      << k.arena_generation << ' ' << k.touch_epoch << ' ' << k.strategy_epoch
      << ' ' << k.stats.decisions << ' ' << k.stats.conflicts << ' '
      << k.stats.learned << ' ' << k.stats.learned_glue_sum;
  return out.str();
}

uint64_t RandomControllerSelector::draw(uint64_t bound) {
  assert(bound);
  // Rejection makes each eligible family/configuration equally likely without
  // relying on a registry-order-dependent modulo bias.
  const uint64_t threshold = -bound % bound;
  uint64_t value;
  do {
    value = stress_random(rng_);
  } while (value < threshold);
  return value % bound;
}

bool RandomControllerSelector::choose(
    Kernel &k, const Observation &observation, const RandomControllerConfig &config,
    ActionRequest &request, std::string &error, volatile sig_atomic_t *stop) {
  if (observation.searching || k.level || k.inconsistent || k.conflict_valid) {
    error = "controller selection requires a consistent root boundary";
    return false;
  }
  error.clear();

  const auto same_version = [](StateVersion left, StateVersion right) {
    return left.arena_generation == right.arena_generation &&
           left.touch_epoch == right.touch_epoch &&
           left.strategy_epoch == right.strategy_epoch;
  };
  const auto settled_terminal = [](const InvocationResult &result) {
    if (result.made_rule_progress || result.status == InvocationStatus::Sat ||
        result.status == InvocationStatus::Unsat)
      return true;
    return result.selection_complete &&
           (result.status == InvocationStatus::Completed ||
            result.status == InvocationStatus::ScopedFixpoint ||
            result.status == InvocationStatus::NoChange);
  };
  if (gf3_discovery_.pending && observation.last_action &&
      observation.last_action->sequence == gf3_discovery_.pending_sequence &&
      settled_terminal(observation.last_action->invocation)) {
    const auto pending = *gf3_discovery_.pending;
    gf3_discovery_.offers.erase(
        std::remove(gf3_discovery_.offers.begin(),
                    gf3_discovery_.offers.end(), pending),
        gf3_discovery_.offers.end());
    gf3_discovery_.pending.reset();
    gf3_discovery_.pending_sequence = 0;
    if (gf3_discovery_.offers.empty() && gf3_discovery_.complete)
      gf3_discovery_.terminal = true;
  }
  if (direct_csp_discovery_.pending && observation.last_action &&
      observation.last_action->sequence ==
          direct_csp_discovery_.pending_sequence &&
      settled_terminal(observation.last_action->invocation)) {
    const auto pending = *direct_csp_discovery_.pending;
    direct_csp_discovery_.offers.erase(
        std::remove(direct_csp_discovery_.offers.begin(),
                    direct_csp_discovery_.offers.end(), pending),
        direct_csp_discovery_.offers.end());
    direct_csp_discovery_.pending.reset();
    direct_csp_discovery_.pending_sequence = 0;
    if (direct_csp_discovery_.offers.empty() &&
        direct_csp_discovery_.complete)
      direct_csp_discovery_.terminal = true;
  }
  std::vector<ActionProducerEffort> producer_effort;
  const auto saturated_add = [](uint64_t left, uint64_t right) {
    return right > UINT64_MAX - left ? UINT64_MAX : left + right;
  };
  const auto has_static_offer = [&](const char *family) {
    const auto found = config.offered_actions.find(family);
    return found != config.offered_actions.end() && !found->second.empty();
  };
  const auto gf3_profile = [&] {
    OnlineInvocation invocation;
    invocation.rule = "Gf3AffineConsequence";
    invocation.parameters = config.parameters.gf3_affine_consequence_reference;
    invocation.granularity = InvocationGranularity::OneInstance;
    invocation.gf3_affine_instances = {{0, 1}};
    std::ostringstream out;
    const auto &producer = config.gf3_action_producer;
    out << producer.maximum_pair_candidates << ' ' << producer.maximum_offers
        << ' ' << producer.maximum_enumeration_work << ' '
        << serialize_online_strategy_program({{invocation}, {}});
    return out.str();
  };
  const auto direct_profile = [&] {
    OnlineInvocation invocation;
    invocation.rule = "DirectCspSupport";
    invocation.parameters = config.parameters.direct_csp_support_reference;
    invocation.granularity = InvocationGranularity::OneInstance;
    invocation.direct_csp_support_instances = {{{1, 2}, {3, 4}, 0}};
    std::ostringstream out;
    const auto &producer = config.direct_csp_action_producer;
    out << producer.maximum_domain_candidates << ' '
        << producer.maximum_pair_candidates << ' '
        << producer.maximum_row_candidates << ' ' << producer.maximum_offers
        << ' ' << producer.maximum_enumeration_work << ' '
        << serialize_online_strategy_program({{invocation}, {}});
    return out.str();
  };

  const auto discover_gf3 = [&]() -> bool {
    if (!config.gf3_action_producer.enabled || has_static_offer("Gf3AffineConsequence") ||
        !k.gf3_affine_action_session)
      return false;
    if (!config.gf3_action_producer.valid()) {
      error = "GF3 structured action producer window is invalid";
      return false;
    }
    const auto &session = *k.gf3_affine_action_session;
    const auto profile = gf3_profile();
    const auto state = state_version(k);
    if (gf3_discovery_.owner != &k ||
        gf3_discovery_.session != &session ||
        !same_version(gf3_discovery_.state, state) ||
        gf3_discovery_.session_keys != session.committed_keys() ||
        gf3_discovery_.profile != profile) {
      gf3_discovery_ = {};
      gf3_discovery_.owner = &k;
      gf3_discovery_.session = &session;
      gf3_discovery_.state = state;
      gf3_discovery_.session_keys = session.committed_keys();
      gf3_discovery_.profile = profile;
    }
    if (gf3_discovery_.pending || !gf3_discovery_.offers.empty())
      return true;
    if (gf3_discovery_.terminal)
      return false;
    const auto &producer = config.gf3_action_producer;
    Gf3AffineActionOfferWindow window{
        producer.maximum_pair_candidates, producer.maximum_offers,
        producer.maximum_enumeration_work, gf3_discovery_.have_after,
        gf3_discovery_.after};
    const Gf3AffineActionLimits limits{
        {config.parameters.gf3_affine_consequence_reference.maximum_source_clauses,
         config.parameters.gf3_affine_consequence_reference.maximum_source_literals,
         config.parameters.gf3_affine_consequence_reference.maximum_domains,
         config.parameters.gf3_affine_consequence_reference.maximum_equations,
         config.parameters.gf3_affine_consequence_reference.maximum_equation_arity,
         config.parameters.gf3_affine_consequence_reference.maximum_tuples_per_equation},
        {config.parameters.gf3_affine_consequence_reference.maximum_result_arity,
         config.parameters.gf3_affine_consequence_reference.maximum_result_tuples,
         config.parameters.gf3_affine_consequence_reference.maximum_additions,
         config.parameters.gf3_affine_consequence_reference.maximum_addition_literals},
        config.parameters.gf3_affine_consequence_reference.maximum_recognition_work,
        config.parameters.gf3_affine_consequence_reference.maximum_planning_work,
        config.parameters.gf3_affine_consequence_reference.maximum_duplicate_scan_work,
        config.parameters.gf3_affine_consequence_reference.maximum_revalidation_work,
        config.parameters.gf3_affine_consequence_reference.maximum_proof_work,
        config.parameters.gf3_affine_consequence_reference.maximum_proof_additions,
        config.parameters.gf3_affine_consequence_reference.maximum_proof_literal_occurrences,
        config.parameters.gf3_affine_consequence_reference.maximum_arena_words,
        config.parameters.gf3_affine_consequence_reference.maximum_journal_bytes,
        config.parameters.gf3_affine_consequence_reference.maximum_external_extent,
        config.parameters.gf3_affine_consequence_reference.maximum_session_keys,
        config.parameters.gf3_affine_consequence_reference.maximum_session_work};
    if (!limits.valid()) {
      error = "GF3 structured action producer parameters are invalid";
      return false;
    }
    const auto offered = gf3_affine_action_offer(k, session, limits, window);
    uint64_t preparation = offered.effort.source_work;
    preparation = saturated_add(preparation, offered.effort.recognition_work);
    preparation = saturated_add(preparation, offered.effort.revalidation_work);
    preparation = saturated_add(preparation, offered.effort.session_work);
    producer_effort.push_back(
        {"Gf3AffineConsequence", static_cast<uint8_t>(offered.status),
         offered.complete, preparation, offered.effort.enumeration_work,
         saturated_add(preparation, offered.effort.enumeration_work),
         offered.effort.pair_candidates, offered.offers.size()});
    if (offered.status == Gf3AffineActionOfferStatus::Interrupted) {
      error = "controller selection interrupted";
      gf3_discovery_.terminal = true;
      return false;
    }
    if (offered.status != Gf3AffineActionOfferStatus::Ready) {
      gf3_discovery_.terminal = true;
      return false;
    }
    const bool cursor_advanced =
        offered.have_resume_after &&
        (!gf3_discovery_.have_after ||
         !(offered.resume_after == gf3_discovery_.after));
    gf3_discovery_.offers = offered.offers;
    gf3_discovery_.complete = offered.complete;
    gf3_discovery_.have_after = offered.have_resume_after;
    if (offered.have_resume_after)
      gf3_discovery_.after = offered.resume_after;
    if (offered.complete && offered.offers.empty())
      gf3_discovery_.terminal = true;
    else if (offered.offers.empty() && !cursor_advanced)
      gf3_discovery_.terminal = true;
    return !gf3_discovery_.offers.empty();
  };

  const auto discover_direct = [&]() -> bool {
    if (!config.direct_csp_action_producer.enabled ||
        has_static_offer("DirectCspSupport") ||
        !k.direct_csp_support_action_session)
      return false;
    if (!config.direct_csp_action_producer.valid()) {
      error = "DirectCSP structured action producer window is invalid";
      return false;
    }
    const auto &session = *k.direct_csp_support_action_session;
    const auto profile = direct_profile();
    const auto state = state_version(k);
    if (direct_csp_discovery_.owner != &k ||
        direct_csp_discovery_.session != &session ||
        !same_version(direct_csp_discovery_.state, state) ||
        direct_csp_discovery_.session_keys != session.committed_keys() ||
        direct_csp_discovery_.retained_owners != session.retained_owners() ||
        direct_csp_discovery_.profile != profile) {
      direct_csp_discovery_ = {};
      direct_csp_discovery_.owner = &k;
      direct_csp_discovery_.session = &session;
      direct_csp_discovery_.state = state;
      direct_csp_discovery_.session_keys = session.committed_keys();
      direct_csp_discovery_.retained_owners = session.retained_owners();
      direct_csp_discovery_.profile = profile;
    }
    if (direct_csp_discovery_.pending ||
        !direct_csp_discovery_.offers.empty())
      return true;
    if (direct_csp_discovery_.terminal)
      return false;
    const auto &producer = config.direct_csp_action_producer;
    DirectCspSupportActionOfferWindow window{
        producer.maximum_domain_candidates, producer.maximum_pair_candidates,
        producer.maximum_row_candidates, producer.maximum_offers,
        producer.maximum_enumeration_work, direct_csp_discovery_.have_after,
        direct_csp_discovery_.after};
    const auto &parameters = config.parameters.direct_csp_support_reference;
    const DirectCspSupportActionLimits limits{
        parameters.maximum_source_clauses, parameters.maximum_source_literals,
        parameters.maximum_recognition_work, parameters.maximum_domain_values,
        parameters.maximum_direct_clause_literals,
        parameters.maximum_definition_clause_literals,
        parameters.maximum_proof_work, parameters.maximum_proof_additions,
        parameters.maximum_proof_literal_occurrences,
        parameters.maximum_journal_bytes, parameters.maximum_extent_growth,
        parameters.maximum_external_extent, parameters.maximum_session_keys,
        parameters.maximum_session_work, parameters.allow_extension};
    if (!limits.valid()) {
      error = "DirectCSP structured action producer parameters are invalid";
      return false;
    }
    const auto offered =
        direct_csp_support_action_offer(k, session, limits, window);
    const uint64_t preparation = offered.effort.session_work;
    producer_effort.push_back(
        {"DirectCspSupport", static_cast<uint8_t>(offered.status),
         offered.complete, preparation, offered.effort.enumeration_work,
         saturated_add(preparation, offered.effort.enumeration_work),
         offered.effort.row_candidates, offered.offers.size()});
    if (offered.status == DirectCspSupportActionOfferStatus::Interrupted) {
      error = "controller selection interrupted";
      direct_csp_discovery_.terminal = true;
      return false;
    }
    if (offered.status != DirectCspSupportActionOfferStatus::Ready) {
      direct_csp_discovery_.terminal = true;
      return false;
    }
    const bool cursor_advanced =
        offered.have_resume_after &&
        (!direct_csp_discovery_.have_after ||
         !(offered.resume_after == direct_csp_discovery_.after));
    direct_csp_discovery_.offers = offered.offers;
    direct_csp_discovery_.complete = offered.complete;
    direct_csp_discovery_.have_after = offered.have_resume_after;
    if (offered.have_resume_after)
      direct_csp_discovery_.after = offered.resume_after;
    if (offered.complete && offered.offers.empty())
      direct_csp_discovery_.terminal = true;
    else if (offered.offers.empty() && !cursor_advanced)
      direct_csp_discovery_.terminal = true;
    return !direct_csp_discovery_.offers.empty();
  };

  enum class NativeOffer : uint8_t { None, Gf3, DirectCsp };
  struct Eligible {
    const RuleVTable *rule = nullptr;
    NativeOffer native = NativeOffer::None;
  };
  std::vector<Eligible> eligible;
  for (const auto &entry : controller_action_availability(k, config)) {
    if (entry.eligible) {
      eligible.push_back(
          {&tactix_rules()[tactix_rule_id(entry.rule.c_str())], NativeOffer::None});
    } else if (entry.reason == "structured-witness-not-offered" &&
               entry.rule == "Gf3AffineConsequence" && discover_gf3()) {
      eligible.push_back(
          {&tactix_rules()[tactix_rule_id(entry.rule.c_str())], NativeOffer::Gf3});
    } else if (entry.reason == "structured-witness-not-offered" &&
               entry.rule == "DirectCspSupport" && discover_direct()) {
      eligible.push_back({&tactix_rules()[tactix_rule_id(entry.rule.c_str())],
                          NativeOffer::DirectCsp});
    } else if (!error.empty()) {
      return false;
    }
  }
  const bool can_resume = kernel_propagation_fixpoint(k);
  if (eligible.empty() && !can_resume) {
    error = "no eligible controller action";
    return false;
  }
  request = {};
  request.sequence = observation.sequence + 1;
  const auto choice = draw(eligible.size() + can_resume);
  if (choice == eligible.size()) {
    request.type = ActionType::ResumeSearch;
    const auto configurations = search_configurations();
    request.search = configurations[draw(configurations.size())];
  } else {
    const auto selected_action = eligible[choice];
    const auto &rule = *selected_action.rule;
    const auto offered = config.offered_actions.find(rule.name);
    if (offered != config.offered_actions.end() && !offered->second.empty()) {
      request.invocation = offered->second[draw(offered->second.size())];
      if (request.invocation.rule != rule.name) {
        error = "offered invocation identity differs from its action family";
        return false;
      }
    } else if (selected_action.native == NativeOffer::Gf3) {
      if (!gf3_discovery_.pending)
        gf3_discovery_.pending =
            gf3_discovery_.offers[draw(gf3_discovery_.offers.size())];
      const auto key = *gf3_discovery_.pending;
      gf3_discovery_.pending_sequence = request.sequence;
      request.invocation.rule = rule.name;
      request.invocation.parameters =
          config.parameters.gf3_affine_consequence_reference;
      request.invocation.granularity = InvocationGranularity::OneInstance;
      request.invocation.gf3_affine_instances = {
          {key.first_equation, key.second_equation}};
    } else if (selected_action.native == NativeOffer::DirectCsp) {
      if (!direct_csp_discovery_.pending)
        direct_csp_discovery_.pending = direct_csp_discovery_.offers[
            draw(direct_csp_discovery_.offers.size())];
      const auto exact = *direct_csp_discovery_.pending;
      direct_csp_discovery_.pending_sequence = request.sequence;
      request.invocation.rule = rule.name;
      request.invocation.parameters =
          config.parameters.direct_csp_support_reference;
      request.invocation.granularity = InvocationGranularity::OneInstance;
      request.invocation.direct_csp_support_instances = {exact};
    } else {
      request.invocation.rule = rule.name;
      request.invocation.parameters = reference_parameters(k, rule.name, config.parameters);
      request.invocation.granularity = InvocationGranularity::CanonicalApplication;
      // Native callback windows are caller-bounded. The public typed surface
      // still accepts any explicit legal identity or structured payload.
      const auto enumerate = rule.semantic_instances ? rule.semantic_instances : rule.instances;
      if (enumerate && config.parameters.maximum_selected_batch && draw(2)) {
        // Lucky's finite constructor catalog is not a prefix of a potentially
        // huge semantic candidate space. Keep every constructor selectable.
        const auto cap = std::string_view(rule.name) == "Lucky"
            ? LUCKY_ACTIVE_INSTANCE_COUNT
            : config.parameters.maximum_selected_batch;
        std::vector<uint64_t> instances(cap);
        const auto count = enumerate(k, instances.data(), cap);
        if (count > cap) {
          error = "native instance producer exceeded its caller window";
          return false;
        }
        if (count) {
          const auto selected = instances[draw(count)];
          // Native callbacks may advertise the canonical whole-rule choice.
          // It has its own typed form, not an explicit semantic instance.
          if (selected != INSTANCE_CANONICAL) {
            request.invocation.granularity = InvocationGranularity::OneInstance;
            request.invocation.instances = {selected};
          }
        }
      }
      if (request.invocation.rule == "Lucky" &&
          request.invocation.instances == std::vector<uint64_t>{LUCKY_STARTUP_RAMSEY})
        request.invocation.parameters = config.parameters.ramsey_startup_reference;
      if (request.invocation.rule == "Lucky" &&
          request.invocation.instances == std::vector<uint64_t>{17})
        request.invocation.parameters =
            config.parameters.exact_cover_model_reference;
      if (request.invocation.rule == "Lucky" &&
          request.invocation.instances == std::vector<uint64_t>{18})
        request.invocation.parameters =
            config.parameters.independent_set_model_reference;
      if (request.invocation.rule == "Lucky" &&
          request.invocation.instances ==
              std::vector<uint64_t>{LUCKY_GRAPH_LABELING})
        request.invocation.parameters = config.parameters.graph_labeling_reference;
      if (request.invocation.rule == "Lucky" &&
          request.invocation.instances == std::vector<uint64_t>{15})
        request.invocation.parameters = config.parameters.stconn_proof_reference;
      if (request.invocation.rule == "Lucky" &&
          request.invocation.instances == std::vector<uint64_t>{16})
        request.invocation.parameters =
            config.parameters.signed_cardinality_proof_reference;
      if (request.invocation.rule == "Lucky" &&
          request.invocation.instances == std::vector<uint64_t>{10})
        request.invocation.parameters =
            config.parameters.balanced_signature_model_reference;
      if (request.invocation.rule == "Lucky" &&
          request.invocation.instances == std::vector<uint64_t>{6})
        request.invocation.parameters = config.parameters.parity_model_reference;
      if (request.invocation.rule == "Lucky" &&
          (request.invocation.instances == std::vector<uint64_t>{7} ||
           request.invocation.instances == std::vector<uint64_t>{9}))
        request.invocation.parameters = config.parameters.onehot_model_reference;
      if (request.invocation.rule == "Lucky" &&
          request.invocation.instances == std::vector<uint64_t>{8})
        request.invocation.parameters = config.parameters.affine_model_reference;
      if (request.invocation.rule == "Lucky" &&
          request.invocation.instances == std::vector<uint64_t>{11})
        request.invocation.parameters = config.parameters.fermat_model_reference;
      if (request.invocation.rule == "Lucky" &&
          request.invocation.instances == std::vector<uint64_t>{13})
        request.invocation.parameters =
            config.parameters.lookup_circuit_model_reference;
      if (request.invocation.rule == "Lucky" &&
          request.invocation.instances == std::vector<uint64_t>{14})
        request.invocation.parameters = config.parameters.simon_model_reference;
      if (request.invocation.rule == "Lucky" &&
          (request.invocation.instances ==
               std::vector<uint64_t>{LUCKY_STARTUP_PARITY} ||
           request.invocation.instances ==
               std::vector<uint64_t>{LUCKY_PARITY_PROOF}))
        request.invocation.parameters = config.parameters.parity_model_reference;
      if (request.invocation.rule == "Lucky" &&
          request.invocation.instances ==
              std::vector<uint64_t>{LUCKY_EXACT_ONE_PROOF})
        request.invocation.parameters = config.parameters.onehot_model_reference;
      if (request.invocation.rule == "Lucky" &&
          !request.invocation.instances.empty() &&
          (request.invocation.instances.front() <= LUCKY_BACKWARD_TRUE ||
           request.invocation.instances.front() == LUCKY_STARTUP_COUNT))
        request.invocation.parameters = std::monostate{};
      // Guidance is a policy choice over the pure candidate snapshot. Bind
      // the exact publication in the request, including an explicit unseeded
      // Walk choice, so replacement cannot silently change a queued action.
      if (!observation.local_search_candidates.empty()) {
        if (request.invocation.rule == "Walk") {
          auto &parameters = std::get<WalkParameters>(request.invocation.parameters);
          const auto selected = draw(observation.local_search_candidates.size() + 1);
          parameters.seed_candidate = selected < observation.local_search_candidates.size()
              ? observation.local_search_candidates[selected].id : 0;
        } else if (request.invocation.rule == "LocalSearchGuidance") {
          request.invocation.granularity = InvocationGranularity::OneInstance;
          request.invocation.instances = {observation.local_search_candidates[
              draw(observation.local_search_candidates.size())].id};
        }
      }
    }
    if (stop && *stop) {
      error = "controller selection interrupted";
      return false;
    }
    const auto allowance = config.allowance(request.invocation.rule);
    request.invocation.repeat = {RepeatMode::Once, 1};
    request.invocation.work_unit = allowance.unit;
    request.invocation.budget = WorkLimit::bounded(allowance.amount);
    request.invocation.rule_work_budget = allowance.native;
  }
  // Discovery can refresh a native scope. Bind to the resulting safe boundary
  // rather than falsely report the chooser's own discovery as external ABA.
  request.expected = state_version(k);
  request.producer_effort = std::move(producer_effort);
  return true;
}

int RandomControllerPolicy::solve(Kernel &k, volatile sig_atomic_t *stop) {
  coverage_ = {};
  for (const auto &rule : tactix_rules())
    if (root_exclusion(rule).empty())
      coverage_.rules.emplace(rule.name, ControllerRuleCoverage{});
  for (const auto configuration : search_configurations())
    coverage_.search.emplace(name(configuration), ControllerSearchCoverage{});
  uint64_t root_actions = 0;
  KernelStopPublisher publish_stop(k, stop);
  RestartController controller(k, config_.search, feedback_);
  RandomControllerSelector selector(trace_ && trace_->replaying() ? trace_->seed() : seed_);
  ControllerSelection &selection = selection_ ? *selection_ : selector;
  struct FeedbackEnd {
    RestartController &controller;
    int verdict = -1;
    std::optional<StopReason> reason = std::nullopt;
    ~FeedbackEnd() { controller.finish_feedback(verdict, reason); }
  } feedback_end{controller};
  const auto emit = [&](const std::string &event) {
    if (!trace_ || trace_->event(event))
      return true;
    std::fprintf(stderr, "c ERROR: %s\n", trace_->error().c_str());
    return false;
  };
  const auto finish = [&](int verdict, std::optional<StopReason> reason = std::nullopt) {
    feedback_end.verdict = verdict;
    feedback_end.reason = reason;
    for (const auto &[rule, counts] : coverage_.rules)
      std::printf("c controller-rule %s requests=%llu applications=%llu "
                  "effective=%llu exhausted=%llu stale=%llu interrupted=%llu "
                  "work=%llu overshoot=%llu\n", rule.c_str(),
                  (unsigned long long)counts.requests,
                  (unsigned long long)counts.applications,
                  (unsigned long long)counts.effective,
                  (unsigned long long)counts.exhausted,
                  (unsigned long long)counts.stale,
                  (unsigned long long)counts.interrupted,
                  (unsigned long long)counts.work,
                  (unsigned long long)counts.overshoot);
    for (const auto &[rule, counts] : coverage_.rules)
      if (counts.producer_calls)
        std::printf("c controller-producer %s calls=%llu work=%llu "
                    "candidates=%llu offers=%llu\n", rule.c_str(),
                    (unsigned long long)counts.producer_calls,
                    (unsigned long long)counts.producer_work,
                    (unsigned long long)counts.producer_candidates,
                    (unsigned long long)counts.producer_offers);
    for (const auto &[configuration, counts] : coverage_.search)
      std::printf("c controller-search %s requests=%llu intervals=%llu "
                  "decisions=%llu conflicts=%llu continuations=%llu\n", configuration.c_str(),
                  (unsigned long long)counts.requests,
                  (unsigned long long)counts.intervals,
                  (unsigned long long)counts.decisions,
                  (unsigned long long)counts.conflicts,
                  (unsigned long long)counts.continuations);
    std::printf("c controller-loop full-restarts=%llu partial-restarts=%llu configuration-changes=%llu "
                "maximum-root-actions=%llu\n",
                (unsigned long long)coverage_.full_restarts,
                (unsigned long long)coverage_.partial_restarts,
                (unsigned long long)coverage_.configuration_changes,
                (unsigned long long)coverage_.maximum_root_actions);
    if (trace_ && !trace_->finish(verdict)) {
      feedback_end.verdict = -1;
      feedback_end.reason = StopReason::Error;
      std::fprintf(stderr, "c ERROR: %s\n", trace_->error().c_str());
      return -1;
    }
    return verdict;
  };
  const auto &analysis = tactix_rules()[tactix_rule_id("ConflictAnalysis")];
  const auto &propagation = tactix_rules()[tactix_rule_id("Propagate")];
  uint64_t actions = 0;
  if (k.level && !k.inconsistent && !(stop && *stop)) {
    const auto result = controller.full_restart();
    ++coverage_.full_restarts;
    if (trace_ && !emit(service_record(k, "Restart", 0, result)))
      return -1;
    if (result == ApplyResult::Unsat || k.inconsistent)
      return finish(20);
  }
  for (;;) {
    if (stop && *stop)
      return finish(0);
    if (k.inconsistent)
      return finish(20);
    // Root conflicts are terminal search-service obligations, never a random
    // inprocessing choice that might destroy a live antecedent.
    if (k.conflict_valid) {
      const auto before = controller.observe();
      const auto glue = k.stats.learned_glue_sum;
      if (before.searching)
        controller.capture_before_backtrack();
      // The registered ConflictAnalysis boundary fuses learning and backjump;
      // Analyze's internal primitive alone leaves a pending assertion.
      const auto result = controller.search_service("ConflictAnalysis", INSTANCE_CANONICAL, [&] {
        const auto native = apply_rule(analysis, k);
        heuristics_consume_root_assignments(k, 0);
        heuristics_consume_rule_advice(k);
        heuristics_consume_retirements(k);
        return native;
      });
      if (trace_ && !emit(service_record(k, "ConflictAnalysis", 0, result)))
        return -1;
      if (result == ApplyResult::Unsat || k.inconsistent)
        return finish(20);
      if (before.searching)
        ++coverage_.search[name(before.search)].conflicts;
      if (before.searching)
        controller.search_conflict(static_cast<uint32_t>(
            k.stats.learned_glue_sum - glue));
      continue;
    }
    if (!k.unassigned_active && kernel_propagation_fixpoint(k))
      return finish(10);
    const auto observation = controller.observe();
    if (observation.searching) {
      if (!kernel_propagation_fixpoint(k)) {
        const size_t trail = k.trail.size();
        const auto result = controller.search_service("Propagate", INSTANCE_CANONICAL, [&] {
          const auto native = apply_rule(propagation, k);
          heuristics_consume_root_assignments(k, trail);
          return native;
        });
        if (trace_ && !emit(service_record(k, "Propagate", 0, result)))
          return -1;
        continue;
      }
      if (controller.restart_due()) {
        const uint32_t keep_level =
            config_.reuse_search_trail && k.level && k.unassigned_active
                ? heuristics_restart_reuse_level_for(
                      k, var_of(controller.next_decision()))
                : 0;
        const auto result = controller.restart(keep_level);
        if (keep_level) {
          ++coverage_.partial_restarts;
          ++coverage_.search[name(observation.search)].continuations;
        } else {
          ++coverage_.full_restarts;
        }
        if (trace_ && !emit(service_record(k, "Restart", keep_level, result)))
          return -1;
        if (result == ApplyResult::Unsat)
          return finish(20);
        continue;
      }
      const auto literal = controller.next_decision();
      const auto identity = lit_instance(k, literal);
      const auto result = controller.search_service("Decide", identity, [&] {
        const auto native = execution::execute_action(k, execution::DecisionRequest{literal});
        if (native == ApplyResult::Ok) {
          heuristics_note_target_decision(
              k, literal, observation.search.phase == Phase::Target);
          ++k.strategy_epoch;
        }
        return native;
      });
      if (result != ApplyResult::Ok) {
        std::fprintf(stderr, "c ERROR: controller ranked an invalid decision\n");
        return finish(0, StopReason::Error);
      }
      ++coverage_.search[name(observation.search)].decisions;
      if (trace_ && !emit(service_record(k, "Decide", identity, result)))
        return -1;
      continue;
    }
    ActionRequest request;
    std::string error;
    if (!selection.choose(k, observation, config_, request, error, stop)) {
      if (!(stop && *stop))
        std::fprintf(stderr, "c ERROR: controller selection: %s\n", error.c_str());
      return finish(0, stop && *stop ? StopReason::Interrupted : StopReason::Error);
    }
    for (const auto &effort : request.producer_effort) {
      auto &counts = coverage_.rules[effort.family];
      ++counts.producer_calls;
      counts.producer_work = counts.producer_work > UINT64_MAX - effort.total_work
                                 ? UINT64_MAX
                                 : counts.producer_work + effort.total_work;
      counts.producer_candidates =
          counts.producer_candidates > UINT64_MAX - effort.candidates
              ? UINT64_MAX
              : counts.producer_candidates + effort.candidates;
      counts.producer_offers = counts.producer_offers > UINT64_MAX - effort.offers
                                   ? UINT64_MAX
                                   : counts.producer_offers + effort.offers;
    }
    if (trace_ && !emit(request_record(request)))
      return -1;
    if (request.type == ActionType::ResumeSearch)
      ++coverage_.search[name(request.search)].requests;
    else {
      ++coverage_.rules[request.invocation.rule].requests;
      ++root_actions;
      coverage_.maximum_root_actions =
          std::max(coverage_.maximum_root_actions, root_actions);
    }
    const auto result = request.type == ActionType::ResumeSearch
                            ? controller.resume_search(request)
                            : controller.apply_rule(
                                  request, config_.allowance(request.invocation.rule),
                                  stop);
    if (result.resumed_search) {
      ++coverage_.search[name(request.search)].intervals;
      coverage_.configuration_changes +=
          observation.have_search_configuration &&
          observation.search != request.search;
      root_actions = 0;
    } else if (request.type == ActionType::Rule) {
      auto &counts = coverage_.rules[request.invocation.rule];
      counts.applications += result.invocation.applications;
      counts.effective += result.invocation.made_rule_progress;
      counts.exhausted += result.invocation.status == InvocationStatus::BudgetExhausted;
      counts.stale += result.invocation.status == InvocationStatus::StaleRequest;
      counts.interrupted += result.invocation.status == InvocationStatus::Interrupted;
      counts.work += result.invocation.work;
      counts.overshoot += result.composition_overshoot;
    }
    if (trace_ && !emit(result_record(result)))
      return -1;
    if (trace_ && !trace_->timing(result.elapsed_nanos)) {
      std::fprintf(stderr, "c ERROR: %s\n", trace_->error().c_str());
      return -1;
    }
    ++actions;
    if ((actions & 255) == 0)
      kernel_check_invariants(k);
    if (result.invocation.status == InvocationStatus::Unsat || k.inconsistent)
      return finish(20);
    if (result.invocation.status == InvocationStatus::Sat)
      return finish(10);
    if (result.invocation.status == InvocationStatus::Interrupted)
      return finish(0, StopReason::Interrupted);
    if (result.invocation.status == InvocationStatus::InvalidRequest ||
        result.invocation.status == InvocationStatus::AccountingError) {
      std::fprintf(stderr, "c ERROR: controller execution status=%u action=%s\n",
                   static_cast<unsigned>(result.invocation.status),
                   request.type == ActionType::ResumeSearch
                       ? "ResumeSearch" : request.invocation.rule.c_str());
      return finish(0, StopReason::Error);
    }
    // Completed work stays committed. Stale scopes and native interrupted
    // progress are reselected at their existing boundaries, with no fallback.
  }
}

} // namespace tactix
