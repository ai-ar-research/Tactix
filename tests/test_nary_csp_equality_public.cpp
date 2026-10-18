#undef NDEBUG
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix

#include "frontend/parse.hpp"
#include "strategy/feedback_trace.hpp"
#include "policies/default_policy.hpp"
#include "policies/random_controller_policy.hpp"
#include "proof/proof.hpp"
#include "rules/semantic_extension/nary_csp_equality_action.hpp"
#include "strategy/online_inprocess.hpp"
#include "strategy/random_online.hpp"

#include <algorithm>
#include <cassert>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace tactix;
using namespace tactix::strategy;
using namespace tactix::strategy::controller;

namespace {

std::vector<std::vector<int32_t>> clauses(bool unsat = false) {
  std::vector<std::vector<int32_t>> result{
      {1, 2, 3}, {-1, -2}, {-1, -3}, {-2, -3},
      {4, 5, 6}, {-4, -5}, {-4, -6}, {-5, -6},
      {7, 8, 9}, {-7, -8}, {-7, -9}, {-8, -9},
      {-1, -4, -7}, {-2, -5, -8}, {-3, -6, -9}};
  if (unsat)
    result.insert(result.end(), {{10, 11}, {10, -11},
                                 {-10, 11}, {-10, -11}});
  return result;
}

std::string dimacs(bool unsat = false) {
  const auto formula = clauses(unsat);
  std::ostringstream out;
  out << "p cnf " << (unsat ? 11 : 9) << ' ' << formula.size() << '\n';
  for (const auto &clause : formula) {
    for (const int32_t literal : clause) out << literal << ' ';
    out << "0\n";
  }
  return out.str();
}

Kernel parse(bool unsat = false) {
  const std::string text = dimacs(unsat);
  FILE *input = std::tmpfile();
  assert(input);
  assert(std::fwrite(text.data(), 1, text.size(), input) == text.size());
  std::rewind(input);
  Kernel kernel;
  assert(parse_dimacs(input, kernel) == nullptr);
  std::fclose(input);
  assert(!kernel.inconsistent && kernel_propagation_fixpoint(kernel));
  return kernel;
}


NaryCspEqualityParameters equality_parameters() {
  NaryCspEqualityParameters result{
      100, 1000, 200000, 16, 100, 10000, 100, 1000,
      100000, 1, 100, 100};
  assert(result.valid());
  return result;
}

NaryCspEqualityActionSelection equality_selection() {
  return {{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}, {0, 1, 2}};
}

void retain_owner(Kernel &kernel, NaryCspRelationOwnerSession &session) {
  NaryCspRelationCaptureRequest request;
  request.domains = equality_selection().domains;
  request.maximum_source_clauses = 100;
  request.maximum_source_literals = 1000;
  request.maximum_recognition_work = 200000;
  request.maximum_domains = 3;
  request.maximum_domain_values = 16;
  request.maximum_forbidden_tuples = 100;
  request.maximum_external_extent = 100;
  NaryCspRelationOwnerSession staged;
  const auto captured = nary_csp_relation_prepare_owner(
      kernel, session, request, {100, 200000}, staged);
  assert(captured.status == NaryCspRelationCaptureStatus::Applied &&
         captured.owner_retained && staged.retained_owners() == 1);
  session = std::move(staged);
}

OnlineInvocation equality_invocation() {
  OnlineInvocation result;
  result.id = 31;
  result.rule = "NaryCspEquality";
  result.parameters = equality_parameters();
  result.granularity = InvocationGranularity::OneInstance;
  result.nary_csp_equality_instances = {equality_selection()};
  result.repeat = {RepeatMode::Once, 1};
  result.budget = WorkLimit::bounded(1);
  result.post_application = PostApplication::Propagate;
  return result;
}

void registry_replay_and_exact_payload() {
  assert(tactix_rule_count() == 47);
  assert(std::strcmp(tactix_rule_name(42), "NaryCspEquality") == 0);
  const auto reference = reference_inprocess_program();
  const auto step = std::find_if(reference.begin(), reference.end(),
      [](const InprocessStep &candidate) { return candidate.id == 31; });
  assert(step != reference.end() && step->rule == "NaryCspEquality" &&
         !step->enabled);

  OnlineStrategyProgram program;
  program.sequence = {equality_invocation()};
  std::string error;
  assert(validate_online_strategy_program(program, &error));
  const std::string wire = serialize_online_strategy_program(program);
  assert(wire.find(":nary-csp-equality1(") != std::string::npos);
  assert(wire.find(
      ":nary-csp-equality[3,3,1,2,3,3,4,5,6,3,7,8,9,0,1,2]") !=
      std::string::npos);
  OnlineStrategyProgram replay;
  assert(deserialize_online_strategy_program(wire, replay, &error));
  assert(serialize_online_strategy_program(replay) == wire);
  assert(replay.sequence.front().nary_csp_equality_instances ==
         program.sequence.front().nary_csp_equality_instances);
  auto malformed = program;
  malformed.sequence.front().nary_csp_equality_instances.front()
      .coordinates = {1, 1, 2};
  assert(!validate_online_strategy_program(malformed, &error));
}

void executor_feedback_and_candidate_followup() {
  Kernel kernel = parse();
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  NaryCspRelationOwnerSession relation_session;
  NaryCspEqualitySession equality_session;
  kernel.nary_csp_relation_owner_session = &relation_session;
  kernel.nary_csp_equality_session = &equality_session;
  OnlineInprocessProgram program;
  program.sequence = {equality_invocation()};
  OnlineInprocessExecutor executor;
  assert(executor.configure(program, 91));
  const auto applied = executor.advance(kernel);
  assert(applied.status == InvocationStatus::Completed &&
         applied.made_rule_progress && applied.applications == 1 &&
         applied.rule_id == 42 && equality_session.keys().size() == 1 &&
         relation_session.retained_owners() == 1);
  assert(kernel.stats.nary_csp_equality_attempts == 1 &&
         kernel.stats.nary_csp_equality_applied == 1 &&
         kernel.stats.nary_csp_equality_source_clauses == 15 &&
         kernel.stats.nary_csp_equality_source_literals == 36 &&
         kernel.stats.nary_csp_equality_recognition_work > 0 &&
         kernel.stats.nary_csp_equality_planning_work > 0 &&
         kernel.stats.nary_csp_equality_proof_work > 0 &&
         kernel.stats.nary_csp_equality_planned_additions == 9 &&
         kernel.stats.nary_csp_equality_emitted_additions == 9 &&
         kernel.stats.nary_csp_equality_session_commits == 1 &&
         kernel.stats.nary_csp_equality_candidate_variables == 1 &&
         kernel.stats.nary_csp_equality_introduced_variables == 1);
  RestartController feedback(kernel);
  const std::string raw = serialize_observation(feedback.observe());
  assert(raw.find("\"nary_csp_equality_attempts\":1") != std::string::npos &&
         raw.find("\"nary_csp_equality_applied\":1") != std::string::npos &&
         raw.find("\"nary_csp_equality_emitted_additions\":9") !=
             std::string::npos);
  const auto candidates = semantic_variable_candidates(kernel);
  assert(candidates.size() == 1);
  const auto candidate_info = std::find_if(
      candidates.begin(), candidates.end(), [](const auto &candidate) {
        return candidate.producer == SemanticVariableProducer::NaryCspEquality;
      });
  assert(candidate_info != candidates.end());
  const auto *candidate = semantic_variable_candidate(kernel,
                                                       candidate_info->id);
  assert(candidate);
  assert(candidate->producer == SemanticVariableProducer::NaryCspEquality &&
         candidate->variables.size() == 1 &&
         candidate->variables.front().external == 10 &&
         candidate->variables.front().role ==
             SemanticVariableRole::IntroducedTheory);
  assert(kernel_propagation_fixpoint(kernel));
  kernel.structural_provenance = nullptr;
  std::fclose(records);
}

void native_offer_paginates_and_suppresses_committed_key() {
  Kernel kernel = parse();
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  NaryCspRelationOwnerSession relation_session;
  NaryCspEqualitySession equality_session;
  kernel.nary_csp_relation_owner_session = &relation_session;
  kernel.nary_csp_equality_session = &equality_session;

  RandomControllerConfig config;
  config.parameters.rules = {"NaryCspEquality"};
  config.parameters.nary_csp_equality_reference = equality_parameters();
  config.nary_csp_equality_action_producer.enabled = true;
  config.nary_csp_equality_action_producer.maximum_offers = 1;
  RestartController controller(kernel);
  RandomControllerSelector selector(7);
  ActionRequest request;
  std::string error;
  bool selected = false;
  for (unsigned attempt = 0; attempt != 64 && !selected; ++attempt) {
    assert(selector.choose(kernel, controller.observe(), config, request,
                           error));
    selected = request.type == ActionType::Rule;
  }
  assert(selected && request.invocation.rule == "NaryCspEquality" &&
         request.invocation.nary_csp_equality_instances.size() == 1 &&
         request.producer_effort.size() == 1 &&
         request.producer_effort.front().family == "NaryCspEquality" &&
         request.producer_effort.front().offers == 1);
  assert(!relation_session.retained());
  const auto exact = request.invocation.nary_csp_equality_instances.front();
  assert(exact.coordinates.left_domain < exact.coordinates.right_domain);
  request.invocation.post_application = PostApplication::Propagate;
  const auto result =
      controller.apply_rule(request, config.allowance("NaryCspEquality"));
  assert(result.invocation.status == InvocationStatus::Completed &&
         result.invocation.made_rule_progress && equality_session.keys().size() == 1 &&
         relation_session.retained_owners() == 1);

  // The committed exact key is never offered again. Pagination advances to a
  // different canonical coordinate choice from the same retained owner.
  ActionRequest next;
  bool found = false;
  for (unsigned attempt = 0; attempt != 64 && !found; ++attempt) {
    assert(selector.choose(kernel, controller.observe(), config, next, error));
    found = next.type == ActionType::Rule;
  }
  assert(found && next.invocation.nary_csp_equality_instances.size() == 1 &&
         !(next.invocation.nary_csp_equality_instances.front() == exact));
  kernel.structural_provenance = nullptr;
  std::fclose(records);
}

void owner_acquisition_is_transactional() {
  Kernel kernel = parse();
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  NaryCspRelationOwnerSession relations;
  NaryCspEqualitySession equality;
  auto bounded = equality_parameters();
  bounded.maximum_proof_additions = 8;
  const auto before_arena = kernel.arena;
  const auto before_id = kernel.next_clause_id;
  const auto declined = nary_csp_equality_action_apply(
      kernel, relations, equality,
      {bounded.maximum_source_clauses, bounded.maximum_source_literals,
       bounded.maximum_recognition_work, bounded.maximum_domain_values,
       bounded.maximum_forbidden_tuples, bounded.maximum_proof_work,
       bounded.maximum_proof_additions,
       bounded.maximum_proof_literal_occurrences,
       bounded.maximum_journal_bytes, bounded.maximum_extent_growth,
       bounded.maximum_external_extent, bounded.maximum_session_keys},
      equality_selection());
  assert(declined.status == NaryCspEqualityActionStatus::ResourceExhausted &&
         !relations.retained() && !equality.retained() &&
         kernel.arena == before_arena && kernel.next_clause_id == before_id &&
         kernel.structural_action_sequence == 0 && std::ftell(records) == 0);
  kernel.structural_provenance = nullptr;
  std::fclose(records);
}

void native_offer_revalidates_changed_owners_with_bounded_work() {
  for (bool assigned : {false, true}) {
    Kernel kernel = parse();
    FILE *records = std::tmpfile();
    assert(records);
    kernel.structural_provenance = records;
    NaryCspRelationOwnerSession relations;
    NaryCspEqualitySession equality;
    retain_owner(kernel, relations);
    const NaryCspEqualityActionOfferWindow window{8, 8, 8, 200000, false, {}};
    const auto parameters = equality_parameters();
    const NaryCspEqualityLimits limits{
        parameters.maximum_source_clauses, parameters.maximum_source_literals,
        parameters.maximum_recognition_work, parameters.maximum_domain_values,
        parameters.maximum_forbidden_tuples, parameters.maximum_proof_work,
        parameters.maximum_proof_additions,
        parameters.maximum_proof_literal_occurrences,
        parameters.maximum_journal_bytes, parameters.maximum_extent_growth,
        parameters.maximum_external_extent, parameters.maximum_session_keys};
    const auto ready = nary_csp_equality_action_offer(
        kernel, relations, equality, limits, window);
    assert(ready.status == NaryCspEqualityActionOfferStatus::Ready &&
           ready.offers.size() == 3 && ready.complete);
    auto smaller_domains = limits;
    smaller_domains.maximum_domain_values = 2;
    const auto skipped = nary_csp_equality_action_offer(
        kernel, relations, equality, smaller_domains, window);
    assert(skipped.status == NaryCspEqualityActionOfferStatus::Ready &&
           skipped.complete && skipped.offers.empty());
    auto small = window;
    // One unit stops before recapture; twelve stop inside its domain census.
    for (uint64_t allowance : {uint64_t{1}, uint64_t{12}}) {
      small.maximum_enumeration_work = allowance;
      const auto exhausted = nary_csp_equality_action_offer(
          kernel, relations, equality, limits, small);
      assert(exhausted.status == NaryCspEqualityActionOfferStatus::ResourceExhausted &&
             exhausted.offers.empty() &&
             exhausted.effort.enumeration_work <= allowance);
    }
    {
      volatile sig_atomic_t stop = 1;
      KernelStopPublisher publisher(kernel, &stop);
      const auto interrupted = nary_csp_equality_action_offer(
          kernel, relations, equality, limits, window);
      assert(interrupted.status == NaryCspEqualityActionOfferStatus::Interrupted &&
             interrupted.offers.empty() &&
             interrupted.effort.enumeration_work <= window.maximum_enumeration_work);
    }
    if (assigned) {
      kernel_assign_unit(kernel, kernel_internalize(kernel, 1));
      assert(!kernel_propagate(kernel));
    } else {
      assert(kernel_delete_binary_all_copies(
          kernel, kernel_internalize(kernel, -1),
          kernel_internalize(kernel, -2)));
    }
    const std::vector<Lit> trail = kernel.trail;
    const auto arena = kernel.arena.size();
    const auto sequence = kernel.structural_action_sequence;
    const auto journal = std::ftell(records);
    const auto stale = nary_csp_equality_action_offer(
        kernel, relations, equality, limits, window);
    assert(stale.status == NaryCspEqualityActionOfferStatus::Stale &&
           stale.offers.empty() && stale.effort.coordinate_candidates == 0 &&
           stale.effort.enumeration_work <= window.maximum_enumeration_work &&
           kernel.trail == trail && kernel.arena.size() == arena &&
           kernel.structural_action_sequence == sequence &&
           std::ftell(records) == journal && relations.retained_owners() == 1 &&
           equality.keys().empty());
    kernel.structural_provenance = nullptr;
    std::fclose(records);
  }
}

void add_formula(tactix_c_handle *solver) {
  for (const auto &clause : clauses()) {
    for (const int32_t literal : clause) tactix_add(solver, literal);
    tactix_add(solver, 0);
  }
}

void c_parameters(tactix_online_parameters &out, bool equality) {
  if (equality) {
    const auto p = equality_parameters();
    out.kind = TACTIX_ONLINE_PARAMETERS_NARY_CSP_EQUALITY;
    out.value.nary_csp_equality = {
        p.maximum_source_clauses, p.maximum_source_literals,
        p.maximum_recognition_work, p.maximum_domain_values,
        p.maximum_forbidden_tuples, p.maximum_proof_work,
        p.maximum_proof_additions, p.maximum_proof_literal_occurrences,
        p.maximum_journal_bytes, p.maximum_extent_growth,
        p.maximum_external_extent, p.maximum_session_keys};
  }
}

void c_v10_deep_copies_and_executes_exact_payload() {
  tactix_c_handle *solver = tactix_init();
  FILE *records = std::tmpfile();
  assert(records && tactix_bind_structural_provenance(solver, records) == 0);
  add_formula(solver);
  int d0[] = {1, 2, 3}, d1[] = {4, 5, 6}, d2[] = {7, 8, 9};
  tactix_online_nary_csp_domain domains[] = {{d0, 3}, {d1, 3}, {d2, 3}};
  tactix_online_nary_csp_equality_instance equality_instance{
      domains, 3, 0, 1, 2};
  tactix_online_step_v10 steps[1]{};
  auto &equality_base =
      steps[0].base.base.base.base.base.base.base.base.base;
  equality_base.step_id = 31;
  equality_base.rule_id = 42;
  c_parameters(equality_base.parameters, true);
  equality_base.granularity = TACTIX_ONLINE_GRANULARITY_ONE;
  equality_base.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  equality_base.repeat_limit = 1;
  equality_base.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  equality_base.budget = 1;
  steps[0].nary_csp_equality_instances = &equality_instance;
  steps[0].nary_csp_equality_instance_count = 1;
  tactix_online_program_v10 program{
      steps, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_configure_v10(
             solver, &program, 100, TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  d0[0] = 99;
  equality_instance.left_domain = 2;
  tactix_online_result result{};
  assert(tactix_online_advance(solver, &result) == 0 &&
         result.rule_id == 42 && result.made_rule_progress);
  tactix_release(solver);
  std::fclose(records);
}

void apply_public_pair(Kernel &kernel, NaryCspRelationOwnerSession &relations,
                       NaryCspEqualitySession &equality) {
  kernel.nary_csp_relation_owner_session = &relations;
  kernel.nary_csp_equality_session = &equality;
  OnlineInprocessProgram program;
  program.sequence = {equality_invocation()};
  OnlineInprocessExecutor executor;
  assert(executor.configure(program, 1));
  assert(executor.advance(kernel).made_rule_progress);
}

void emit_oracles(const std::string &stem) {
  const std::string unsat = dimacs(true);
  std::ofstream(stem + ".cnf") << unsat;
  Kernel kernel;
  Proof proof;
  assert(proof.open((stem + ".dsr").c_str(), ProofFormat::DsrAscii));
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  kernel.proof = &proof;
  FILE *input = std::tmpfile();
  assert(input);
  assert(std::fwrite(unsat.data(), 1, unsat.size(), input) == unsat.size());
  std::rewind(input);
  assert(parse_dimacs(input, kernel) == nullptr);
  std::fclose(input);
  FILE *records = std::fopen((stem + ".records.jsonl").c_str(), "w+");
  assert(records);
  kernel.structural_provenance = records;
  NaryCspRelationOwnerSession relations;
  NaryCspEqualitySession equality;
  apply_public_pair(kernel, relations, equality);
  assert(default_solve(kernel) == 20);
  proof.flush();
  kernel.proof = nullptr;
  kernel.structural_provenance = nullptr;
  std::fclose(records);

  Kernel sat = parse();
  FILE *sat_records = std::tmpfile();
  assert(sat_records);
  sat.structural_provenance = sat_records;
  NaryCspRelationOwnerSession sat_relations;
  NaryCspEqualitySession sat_equality;
  apply_public_pair(sat, sat_relations, sat_equality);
  assert(default_solve(sat) == 10);
  kernel_extend_model(sat, false);
  std::ofstream(stem + ".sat.cnf") << dimacs();
  std::ofstream model(stem + ".model");
  model << "s SATISFIABLE\nv";
  for (Var v = 0; v < kernel_num_output_vars(sat); ++v)
    model << ' ' << (sat.model_values[make_lit(v, false)] > 0
                         ? static_cast<int>(v + 1)
                         : -static_cast<int>(v + 1));
  model << " 0\n";
  sat.structural_provenance = nullptr;
  std::fclose(sat_records);
}

} // namespace

int main(int argc, char **argv) {
  if (argc == 2) {
    emit_oracles(argv[1]);
    std::puts("test_nary_csp_equality_public: oracles emitted");
    return 0;
  }
  assert(argc == 1);
  registry_replay_and_exact_payload();
  executor_feedback_and_candidate_followup();
  owner_acquisition_is_transactional();
  native_offer_paginates_and_suppresses_committed_key();
  native_offer_revalidates_changed_owners_with_bounded_work();
  c_v10_deep_copies_and_executes_exact_payload();
  std::puts("test_nary_csp_equality_public: passed");
}
