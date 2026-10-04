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
    else if (std::string_view(rule.name) == "Condition" &&
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
  std::vector<const RuleVTable *> eligible;
  for (const auto &entry : controller_action_availability(k, config))
    if (entry.eligible)
      eligible.push_back(&tactix_rules()[tactix_rule_id(entry.rule.c_str())]);
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
    const auto &rule = *eligible[choice];
    const auto offered = config.offered_actions.find(rule.name);
    if (offered != config.offered_actions.end() && !offered->second.empty()) {
      request.invocation = offered->second[draw(offered->second.size())];
      if (request.invocation.rule != rule.name) {
        error = "offered invocation identity differs from its action family";
        return false;
      }
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
          request.invocation.instances == std::vector<uint64_t>{15})
        request.invocation.parameters = config.parameters.stconn_proof_reference;
      if (request.invocation.rule == "Lucky" &&
          request.invocation.instances == std::vector<uint64_t>{16})
        request.invocation.parameters =
            config.parameters.signed_cardinality_proof_reference;
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
        if (native == ApplyResult::Ok) ++k.strategy_epoch;
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
