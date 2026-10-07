// Prepared bounded diagnostic driver; links unchanged 76e1f68b8 Release objects.
// All selected mutations use public controller/registry/action execution APIs.
#include "frontend/parse.hpp"
#include "policies/random_controller_policy.hpp"
#include "rules/propagate/propagate.hpp"
#include "proof/proof.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>
// Headers use the Release NDEBUG definition; only driver checks stay active.
#undef assert
#define assert(expression) do { if (!(expression)) { std::fprintf(stderr, "driver check failed at %s:%d: %s\n", __FILE__, __LINE__, #expression); std::abort(); } } while (false)
using namespace tactix;
using namespace tactix::strategy;
using namespace tactix::strategy::controller;
static volatile sig_atomic_t stopped = 0;
static void stop_handler(int) { stopped = 1; }
int main(int argc, char **argv) {
  // CNF proof provenance transcript off|on common.program seconds [replay]
  if (argc != 8 && argc != 9) return 2;
  const bool treatment = std::string(argv[5]) == "on";
  assert(treatment || std::string(argv[5]) == "off");
  const unsigned seconds = std::stoul(argv[7]);
  assert(seconds >= 5 && seconds <= 30);
  std::signal(SIGALRM, stop_handler); std::signal(SIGTERM, stop_handler);
  alarm(seconds);
  uint64_t replay_services = 100000;
  std::string replay_end;
  if (argc == 9) {
    std::ifstream retained(argv[4]); std::string line;
    while (std::getline(retained, line)) if (line.starts_with("END ")) {
      std::istringstream end_record(line); std::string marker;
      uint64_t decisions, conflicts, propagations, bcp, direct;
      assert(end_record >> marker >> replay_end >> decisions >> conflicts >> propagations >> bcp >> direct >> replay_services);
      assert(replay_services <= 100000);
    }
    assert(!replay_end.empty());
  }
  Kernel k;
  KernelStopPublisher publish_stop(k, &stopped);
  NaryCspPrefixActionSession prefix;
  NaryCspEqualitySession equalities;
  NaryCspEqualityAmoSession amos;
  k.nary_csp_prefix_action_session = &prefix;
  k.nary_csp_equality_session = &equalities;
  k.nary_csp_equality_amo_session = &amos;
  FILE *journal = std::fopen(argv[3], "wx"); assert(journal);
  k.structural_provenance = journal;
  Proof proof; assert(proof.open(argv[2], ProofFormat::DsrAscii));
  proof.bind_variable_map(&k.internal_to_external);
  proof.bind_external_variable_map(&k.external_to_internal);
  k.proof = &proof;
  FILE *input = std::fopen(argv[1], "r"); assert(input);
  assert(!parse_dimacs(input, k)); std::fclose(input);
  RandomControllerConfig config;
  config.parameters.rules = {"SubstitutionRedundancy", "NaryCspPrefix",
                            "NaryCspEquality", "NaryCspEqualityAmo", "Propagate"};
  config.nary_csp_action_producer.enabled = true;
  config.nary_csp_equality_action_producer.enabled = true;
  config.nary_csp_equality_amo_producer.enabled = true;
  assert(config.configure_search_configurations(
      {{Branching::Vmtf, Restart::Reluctant, Phase::Saved}}));
  std::ifstream program_input(argv[6]);
  std::ostringstream encoded; encoded << program_input.rdbuf();
  OnlineStrategyProgram program; std::string error;
  assert(deserialize_online_strategy_program(encoded.str(), program, &error, false, true));
  assert(config.configure_startup_prefix(program, &error));
  ControllerTrace trace;
  const std::string signature = "r401-x20-driver-v1:" + std::string(argv[5]);
  assert(argc == 9 ? trace.replay(argv[4], signature)
                   : trace.write(argv[4], 17, signature));
  const auto emit = [&](const std::string &line) { assert(trace.event(line)); };
  RestartController controller(k, config.search);
  RandomControllerSelector selector(trace.seed());
  std::array<std::pair<Lit, Lit>, 3> target{};
  std::array<uint64_t, 3> installed{};
  uint64_t binary_bcp = 0, binary_conflicts = 0, services = 0;
  bool have_targets = false;
  const auto root_snapshot = [&](const char *name) {
    std::ostringstream out; out << name;
    for (Lit literal : k.trail)
      out << ' ' << kernel_externalize(k, literal) << '@'
          << k.assigned[var_of(literal)].level;
    emit(out.str());
  };
  const auto propagate = [&] {
    if (kernel_propagation_fixpoint(k)) { emit("Propagate settled-no-work"); return; }
    assert(propagate_guard(k));
    const size_t before = k.trail.size();
    const auto result = controller.search_service("Propagate", INSTANCE_CANONICAL,
        [&] { const auto native = propagate_apply(k);
          heuristics_consume_root_assignments(k, before); return native; });
    ++services;
    for (size_t pos = before; have_targets && pos < k.trail.size(); ++pos) {
      const Lit assigned = k.trail[pos];
      const auto &record = k.assigned[var_of(assigned)];
      if (!record.binary || record.jumped) continue;
      for (size_t pair = 0; pair < target.size(); ++pair) {
        const auto [a, b] = target[pair];
        if (((assigned == a && record.reason == b) ||
             (assigned == b && record.reason == a)) && installed[pair] &&
            kernel_binary_id(k, a, b) == installed[pair]) {
          assert(kernel_binary_live(k, a, b) && k.value(record.reason) < 0);
          ++binary_bcp;
          emit("X20-BCP " + std::to_string(pair) + " " +
               std::to_string(installed[pair]) + " " +
               std::to_string(kernel_externalize(k, assigned)) + " " +
               std::to_string(kernel_externalize(k, record.reason)) + " " +
               std::to_string(record.level));
        }
      }
    }
    emit("Propagate result " + std::to_string(unsigned(result)) + " " +
         std::to_string(k.stats.propagations) + " " + std::to_string(k.stats.conflicts));
  };
  const auto encode_invocation = [](const OnlineInvocation &invocation) {
    const auto bytes = serialize_online_strategy_program({{invocation}, {}});
    std::string hex; static const char digits[] = "0123456789abcdef";
    for (unsigned char byte : bytes) { hex += digits[byte >> 4]; hex += digits[byte & 15]; }
    return hex; // One trace event must occupy exactly one physical line.
  };
  const auto version_record = [&] {
    const auto version = state_version(k);
    return std::to_string(version.arena_generation) + "/" +
           std::to_string(version.touch_epoch) + "/" + std::to_string(version.strategy_epoch);
  };
  ActionRequest selected_resume;
  for (uint64_t doses = 0; doses < 4096 && !stopped; ++doses) {
    propagate();
    assert(!k.inconsistent && !k.conflict_valid);
    ActionRequest request;
    assert(selector.choose(k, controller.observe(), config, request, error, &stopped));
    if (request.type == ActionType::ResumeSearch) { selected_resume = request; break; }
    emit("common-request " + std::to_string(request.sequence) + " " +
         encode_invocation(request.invocation));
    const auto result = controller.apply_rule(request, config.allowance(request.invocation.rule), &stopped);
    emit("common-result " + std::to_string(result.sequence) + " " +
         std::to_string(unsigned(result.invocation.status)) + " " +
         std::to_string(result.invocation.work) + " " +
         std::to_string(result.invocation.applications) + " " +
         std::to_string(result.invocation.selection_complete));
    assert(result.invocation.status == InvocationStatus::Completed ||
           result.invocation.status == InvocationStatus::ScopedFixpoint ||
           result.invocation.status == InvocationStatus::NoChange);
  }
  assert(!stopped && selected_resume.type == ActionType::ResumeSearch);
  propagate();
  assert(equalities.records().size() == 3);
  const auto domains = equalities.records().front().key.domains;
  std::array<Lit, 3> equality_literals{};
  std::array<bool, 3> seen{};
  for (const auto &record : equalities.records()) {
    assert(record.key.domains == domains && record.key.residual_domain < 3);
    assert(!seen[record.key.residual_domain]); seen[record.key.residual_domain] = true;
    equality_literals[record.key.residual_domain] =
        kernel_internalize(k, -static_cast<int32_t>(record.extension_variable));
    std::ostringstream fingerprint;
    fingerprint << "owner-coordinate " << record.key.residual_domain << ' '
                << record.extension_variable;
    for (const auto &domain : record.key.domains) {
      fingerprint << " |"; for (int32_t value : domain) fingerprint << ' ' << value;
    }
    for (const auto &clause : record.definition_clauses) {
      fingerprint << " ;"; for (int32_t value : clause) fingerprint << ' ' << value;
    }
    emit(fingerprint.str());
  }
  size_t index = 0;
  for (size_t first = 0; first < 3; ++first)
    for (size_t second = first + 1; second < 3; ++second) {
      target[index++] = {equality_literals[first], equality_literals[second]};
      assert(!kernel_binary_live(k, equality_literals[first], equality_literals[second]));
    }
  have_targets = true;
  root_snapshot("common-root"); emit("COMMON-END");
  if (treatment) {
    ActionRequest request;
    request.sequence = controller.observe().sequence + 1;
    request.expected = state_version(k);
    request.invocation.rule = "NaryCspEqualityAmo";
    request.invocation.parameters = config.parameters.nary_csp_equality_amo_reference;
    request.invocation.granularity = InvocationGranularity::OneInstance;
    request.invocation.nary_csp_equality_amo_instances = {{domains}};
    request.invocation.repeat = {RepeatMode::Once, 1};
    request.invocation.work_unit = WorkUnit::Applications;
    request.invocation.budget = WorkLimit::bounded(1);
    request.invocation.rule_work_budget = {RuleWorkBudgetMode::Bounded, 100000000};
    emit("X20-request " + std::to_string(request.sequence) + " " + version_record() + " " + encode_invocation(request.invocation));
    const auto result = controller.apply_rule(request,
        {WorkUnit::Applications, 1, {RuleWorkBudgetMode::Bounded, 100000000}}, &stopped);
    emit("X20-result " + std::to_string(unsigned(result.invocation.status)) + " " +
         std::to_string(result.invocation.work) + " " + std::to_string(result.invocation.applications) + " " + version_record());
    assert(result.invocation.status == InvocationStatus::Completed &&
           result.invocation.made_rule_progress && amos.keys().size() == 1);
    for (size_t pair = 0; pair < target.size(); ++pair) {
      const auto [a,b] = target[pair];
      assert(kernel_binary_live(k, a, b));
      installed[pair] = kernel_binary_id(k, a, b); assert(installed[pair]);
      emit("X20-installed " + std::to_string(pair) + " " +
           std::to_string(installed[pair]) + " " +
           std::to_string(kernel_externalize(k, a)) + " " +
           std::to_string(kernel_externalize(k, b)));
    }
  }
  propagate(); root_snapshot("post-treatment-root");
  selected_resume.sequence = controller.observe().sequence + 1;
  selected_resume.expected = state_version(k);
  emit("Resume-request " + std::to_string(selected_resume.sequence) + " " + version_record());
  assert(controller.resume_search(selected_resume).resumed_search);
  emit("Resume-result resumed " + version_record());
  emit("SEARCH-BEGIN vmtf/reluctant/saved");
  int verdict = 0;
  std::string end = "service-limit";
  while (!stopped && services < replay_services) {
    if (k.inconsistent) { verdict = 20; end = "unsat"; break; }
    if (k.conflict_valid) {
      if (k.conflict_ref == INVALID_REF)
        for (size_t pair = 0; pair < target.size(); ++pair) {
          const auto [a,b] = target[pair];
          if (kernel_binary_key(k.conflict_bin[0], k.conflict_bin[1]) == kernel_binary_key(a,b) &&
              installed[pair] && kernel_binary_id(k,a,b) == installed[pair]) {
            ++binary_conflicts; emit("X20-direct-conflict " + std::to_string(pair) + " " + std::to_string(installed[pair]));
          }
        }
      controller.capture_before_backtrack();
      const auto glue = k.stats.learned_glue_sum;
      emit("ConflictAnalysis-request " + version_record());
      const auto analyzed = controller.search_service("ConflictAnalysis", INSTANCE_CANONICAL, [&] {
        const auto result = apply_rule(tactix_rules()[tactix_rule_id("ConflictAnalysis")], k);
        heuristics_consume_root_assignments(k, 0);
        heuristics_consume_rule_advice(k); heuristics_consume_retirements(k);
        return result;
      });
      emit("ConflictAnalysis-result " + std::to_string(unsigned(analyzed)) + " " + version_record());
      ++services;
      controller.search_conflict(static_cast<uint32_t>(k.stats.learned_glue_sum-glue));
      continue;
    }
    if (!kernel_propagation_fixpoint(k)) { propagate(); continue; }
    if (!k.unassigned_active) { verdict = 10; end = "sat"; break; }
    if (controller.restart_due()) { end = "first-restart-boundary"; break; }
    const Lit literal = controller.next_decision();
    emit("Decide-request " + std::to_string(kernel_externalize(k,literal)) + " " + version_record());
    const auto decided = controller.search_service("Decide", lit_instance(k,literal), [&] {
      return execution::execute_action(k, execution::DecisionRequest{literal});
    });
    assert(decided == ApplyResult::Ok);
    emit("Decide-result " + std::to_string(unsigned(decided)) + " " + version_record());
    ++services;
  }
  if (argc == 9 && services == replay_services) {
    end = replay_end;
    if (end == "sat") { assert(!k.unassigned_active && kernel_propagation_fixpoint(k)); verdict = 10; }
    if (end == "unsat") { assert(k.inconsistent); verdict = 20; }
    if (end == "first-restart-boundary") assert(controller.restart_due());
  }
  if (stopped) end = "deadline";
  emit("END " + end + " " + std::to_string(k.stats.decisions) + " " +
       std::to_string(k.stats.conflicts) + " " + std::to_string(k.stats.propagations) +
       " " + std::to_string(binary_bcp) + " " + std::to_string(binary_conflicts) + " " + std::to_string(services));
  if (verdict == 10) {
    kernel_extend_model(k); std::printf("v");
    for (int32_t variable = 1; uint32_t(variable) <= kernel_num_output_vars(k); ++variable) {
      const int8_t value = k.model_values[internalize(variable)];
      assert(value);
      std::printf(" %d", value > 0 ? variable : -variable);
    }
    std::puts(" 0");
  }
  assert(trace.finish(verdict));
  proof.flush(); std::fflush(journal); k.proof = nullptr;
  std::fclose(journal);
  std::printf("s %s\n", verdict == 10 ? "SATISFIABLE" : verdict == 20 ? "UNSATISFIABLE" : "UNKNOWN");
  return verdict;
}
