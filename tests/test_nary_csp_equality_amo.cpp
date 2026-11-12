#undef NDEBUG
#include "dimacs_text.hpp"

#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix
#include "policies/default_policy.hpp"
#include "policies/restart_controller.hpp"
#include "proof/proof.hpp"
#include "rules/semantic_extension/nary_csp_equality_action.hpp"
#include "rules/semantic_extension/nary_csp_equality_amo_action.hpp"
#include "strategy/online_inprocess.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <fstream>
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
    result.insert(result.end(), {{20, 21}, {20, -21}, {-20, 21}, {-20, -21}});
  return result;
}

std::string dimacs(bool unsat = false) {
  return dimacs_text(unsat ? 21 : 9, clauses(unsat));
}

void parse_into(Kernel &kernel, bool unsat = false) {
  parse_dimacs_text(dimacs(unsat), kernel, ParsedRoot::Consistent);
}


NaryCspEqualityLimits equality_limits() {
  return {100, 1000, 300000, 16, 100, 10000,
          100, 1000, 100000, 1, 100, 100};
}

NaryCspEqualityAmoLimits amo_limits() {
  return {100, 1000, 300000, 16, 100, 10000,
          100, 1000, 100000, 100, 100};
}

const std::vector<std::vector<int32_t>> domains{
    {1, 2, 3}, {4, 5, 6}, {7, 8, 9}};

void prepare(Kernel &kernel, NaryCspRelationOwnerSession &relations,
             NaryCspEqualitySession &equality, FILE *records) {
  kernel.structural_provenance = records;
  kernel.nary_csp_relation_owner_session = &relations;
  kernel.nary_csp_equality_session = &equality;
  for (uint32_t residual = 0; residual != 3; ++residual) {
    std::array<uint32_t, 2> equal{};
    size_t next = 0;
    for (uint32_t domain = 0; domain != 3; ++domain)
      if (domain != residual) equal[next++] = domain;
    const NaryCspEqualityActionSelection selection{
        domains, {equal[0], equal[1], residual}};
    const auto applied = nary_csp_equality_action_apply(
        kernel, relations, equality, equality_limits(), selection);
    assert(applied.status == NaryCspEqualityActionStatus::Applied);
  }
  assert(equality.records().size() == 3);
  for (const auto &record : equality.records())
    assert(record.definition_clauses.size() == 3 * domains.front().size());
}

void lifecycle() {
  Kernel kernel;
  parse_into(kernel);
  FILE *records = std::tmpfile();
  assert(records);
  NaryCspRelationOwnerSession relations;
  NaryCspEqualitySession equality;
  NaryCspEqualityAmoSession session;
  prepare(kernel, relations, equality, records);
  kernel.nary_csp_equality_amo_session = &session;

  const auto applied = nary_csp_equality_amo_apply(
      kernel, relations, equality, session, amo_limits(), {domains}, records);
  assert(applied.status == NaryCspEqualityAmoStatus::Applied &&
         applied.emitted_binary_clauses == 3 &&
         applied.equality_variables == 3 && kernel.nvars == 12 &&
         session.keys().size() == 1);
  const auto duplicate = nary_csp_equality_amo_apply(
      kernel, relations, equality, session, amo_limits(), {domains}, records);
  assert(duplicate.status == NaryCspEqualityAmoStatus::Duplicate);

  NaryCspEqualityAmoSelection malformed{domains};
  malformed.domains[2].pop_back();
  assert(!nary_csp_equality_amo_selection_valid(malformed));
  Kernel other;
  parse_into(other);
  const auto wrong_session = nary_csp_equality_amo_apply(
      other, relations, equality, session, amo_limits(), {domains}, records);
  assert(wrong_session.status == NaryCspEqualityAmoStatus::Inapplicable);
  kernel.structural_provenance = nullptr;
  std::fclose(records);
}

// The serialized program is its identity; the payload must be encoded.
void public_wire_format() {
  strategy::OnlineInvocation invocation;
  invocation.id = 33;
  invocation.rule = "NaryCspEqualityAmo";
  invocation.parameters = strategy::NaryCspEqualityAmoParameters{amo_limits()};
  invocation.granularity = strategy::InvocationGranularity::OneInstance;
  invocation.nary_csp_equality_amo_instances = {{domains}};
  invocation.budget = strategy::WorkLimit::bounded(1);
  strategy::OnlineStrategyProgram program{{invocation}, {}};
  std::string error;
  assert(strategy::validate_online_strategy_program(program, &error));
  assert(strategy::serialize_online_strategy_program(program).find(
             ":nary-csp-equality-amo[3,3,1,2,3,3,4,5,6,3,7,8,9]") !=
         std::string::npos);
}

// Each decline cause through the controller: typed status, feedback, no
// mutation, and for retryable causes a later commit of the same payload.
void controller_decline_feedback() {
  struct Receiver final : FeedbackReceiver {
    std::vector<ActionFeedback> actions;
    void after_action(const ActionFeedback &action) override {
      actions.push_back(action);
    }
    void search_interval(const SearchIntervalFeedback &) override {}
  };
  enum Cause { ProofBudget, Interrupt, Stale, Invalid, Unsupported };
  for (const Cause cause : {ProofBudget, Interrupt, Stale, Invalid, Unsupported}) {
    Kernel kernel;
    parse_into(kernel);
    FILE *records = std::tmpfile();
    assert(records);
    NaryCspRelationOwnerSession relations;
    NaryCspEqualitySession equality;
    NaryCspEqualityAmoSession session;
    prepare(kernel, relations, equality, records);
    kernel.nary_csp_equality_amo_session = &session;
    Proof proof;
    if (cause == Unsupported) {
      assert(proof.open("/dev/null", ProofFormat::PbpStreaming));
      proof.finish_input_transaction();
      assert(proof.begin_pb());
      kernel.proof = &proof;
    }
    if (cause == Stale) {
      const auto &target = equality.records().front().definition_clauses.front();
      std::vector<Lit> wanted;
      for (int32_t literal : target)
        wanted.push_back(kernel_internalize(kernel, literal));
      std::sort(wanted.begin(), wanted.end());
      bool removed = false;
      for (const auto [ref, clause] : arena_records(kernel.arena)) {
        std::vector<Lit> current(clause->lits, clause->lits + clause->size);
        std::sort(current.begin(), current.end());
        if (!removed && !clause->garbage() && current == wanted) {
          kernel_delete_clause(kernel, clause);
          removed = true;
        }
      }
      assert(removed);
    }

    if (cause == Interrupt) {
      // A fixed deterministic stop at live owner capture, after dispatch.
      kernel.stop_fuzz = 1;
      kernel.stop_fuzz_state = 0;
    }
    Receiver receiver;
    RestartController controller(kernel, {}, &receiver);
    ActionRequest request;
    request.sequence = 1;
    request.expected = state_version(kernel);
    request.invocation.id = 33;
    request.invocation.rule = "NaryCspEqualityAmo";
    request.invocation.granularity = InvocationGranularity::OneInstance;
    request.invocation.nary_csp_equality_amo_instances = {{domains}};
    if (cause == Invalid)
      request.invocation.nary_csp_equality_amo_instances.front().domains.pop_back();
    NaryCspEqualityAmoParameters parameters{amo_limits()};
    if (cause == ProofBudget) parameters.maximum_proof_additions = 2;
    request.invocation.parameters = parameters;
    const auto before = state_version(kernel);
    const auto clause_id_before = kernel.next_clause_id;
    const auto variables_before = kernel.nvars;
    const auto first = controller.apply_rule(request, {});
    const bool retryable = cause == ProofBudget || cause == Interrupt;
    const auto expected = cause == Interrupt ? InvocationStatus::Interrupted
        : cause == Stale ? InvocationStatus::StaleRequest
        : cause == Invalid ? InvocationStatus::InvalidRequest
        : cause == Unsupported ? InvocationStatus::Inapplicable
        : InvocationStatus::ResourceExhausted;
    const bool predispatch = cause == Invalid || cause == Unsupported;
    assert(first.invocation.status == expected &&
           first.invocation.applications == (predispatch ? 0u : 1u) &&
           first.invocation.work == (predispatch ? 0u : 1u) &&
           !first.invocation.made_rule_progress &&
           first.invocation.selection_complete == (!retryable && !predispatch));
    assert(before.arena_generation == kernel.arena_generation &&
           kernel.next_clause_id == clause_id_before &&
           kernel.nvars == variables_before && session.keys().empty());
    assert(kernel.stats.nary_csp_equality_amo_attempts == (predispatch ? 0u : 1u) &&
           kernel.stats.nary_csp_equality_amo_applied == 0);
    assert(receiver.actions.size() == 1);
    const auto &feedback = receiver.actions.front();
    assert(feedback.outcome.invocation.status == expected &&
           feedback.stop == stop_reason(expected) &&
           feedback.after.cumulative.nary_csp_equality_amo_attempts -
               feedback.before.cumulative.nary_csp_equality_amo_attempts ==
                   (predispatch ? 0u : 1u));
    if (cause == Stale)
      assert(kernel.stats.nary_csp_equality_amo_stale == 1);
    else
      assert(kernel.stats.nary_csp_equality_amo_declined == (predispatch ? 0u : 1u));
    if (cause == Interrupt) assert(kernel.stats.stop_fuzz_fires == 1);
    if (retryable) {
      // A fresh allowance or cleared interruption commits the same payload.
      kernel.stop_fuzz = 0;
      parameters.maximum_proof_additions = 100;
      request.invocation.parameters = parameters;
      request.sequence = 2;
      request.expected = state_version(kernel);
      const auto retry = controller.apply_rule(request, {});
      assert(retry.invocation.status == InvocationStatus::Completed &&
             retry.invocation.made_rule_progress &&
             retry.invocation.selection_complete && session.keys().size() == 1 &&
             kernel.stats.nary_csp_equality_amo_attempts == 2);
    }
    kernel.proof = nullptr;
    kernel.structural_provenance = nullptr;
    std::fclose(records);
  }
}

void c_v12_deep_copies_and_executes_exact_payload() {
  tactix_c_handle *solver = tactix_init();
  assert(solver);
  FILE *records = std::tmpfile();
  assert(records && tactix_bind_structural_provenance(solver, records) == 0);
  for (const auto &clause : clauses()) {
    for (const int32_t literal : clause) tactix_add(solver, literal);
    tactix_add(solver, 0);
  }

  int d0[] = {1, 2, 3}, d1[] = {4, 5, 6}, d2[] = {7, 8, 9};
  tactix_online_nary_csp_domain c_domains[] = {
      {d0, 3}, {d1, 3}, {d2, 3}};
  tactix_online_nary_csp_equality_instance equality_instances[] = {
      {c_domains, 3, 1, 2, 0},
      {c_domains, 3, 0, 2, 1},
      {c_domains, 3, 0, 1, 2}};
  tactix_online_nary_csp_equality_amo_instance amo_instance{c_domains, 3};
  tactix_online_step_v12 steps[3]{};

  auto base = [](tactix_online_step_v12 &step) -> tactix_online_step & {
    return step.base.base.base.base.base.base.base.base.base.base.base;
  };
  auto set_common = [&](tactix_online_step_v12 &step, uint64_t id,
                        int rule) {
    auto &out = base(step);
    out.step_id = id;
    out.rule_id = rule;
    out.granularity = TACTIX_ONLINE_GRANULARITY_ONE;
    out.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
    out.repeat_limit = 1;
    out.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
    out.budget = 1;
  };
  set_common(steps[1], 31, 42);
  auto &equality_parameter = base(steps[1]).parameters;
  equality_parameter.kind = TACTIX_ONLINE_PARAMETERS_NARY_CSP_EQUALITY;
  equality_parameter.value.nary_csp_equality = {
      100, 1000, 300000, 16, 100, 10000, 100, 1000, 100000, 1, 100, 100};
  steps[1].base.base.nary_csp_equality_instances = equality_instances;
  steps[1].base.base.nary_csp_equality_instance_count = 1;

  set_common(steps[2], 33, 44);
  auto &amo_parameter = base(steps[2]).parameters;
  amo_parameter.kind = TACTIX_ONLINE_PARAMETERS_NARY_CSP_EQUALITY_AMO;
  amo_parameter.value.nary_csp_equality_amo = {
      100, 1000, 300000, 16, 100, 10000, 100, 1000, 100000, 100, 100};
  steps[2].nary_csp_equality_amo_instances = &amo_instance;
  steps[2].nary_csp_equality_amo_instance_count = 1;

  tactix_online_program_v12 program{
      &steps[1], 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  tactix_online_result result{};
  program.steps = &steps[1];
  for (unsigned i = 0; i != 3; ++i) {
    steps[1].base.base.nary_csp_equality_instances = &equality_instances[i];
    assert(tactix_online_configure_v12(
               solver, &program, 11 + i,
               TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
    const uint32_t saved = equality_instances[i].left_domain;
    equality_instances[i].left_domain = equality_instances[i].residual_domain;
    assert(tactix_online_advance(solver, &result) == 0 &&
           result.rule_id == 42 && result.status == TACTIX_ONLINE_COMPLETED &&
           result.made_rule_progress);
    equality_instances[i].left_domain = saved;
  }

  program.steps = &steps[2];
  assert(tactix_online_configure_v12(
             solver, &program, 14, TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  amo_instance.domain_count = 0;
  const int advance = tactix_online_advance(solver, &result);
  assert(advance == 0 &&
         result.rule_id == 44 && result.status == TACTIX_ONLINE_COMPLETED &&
         result.made_rule_progress);
  tactix_release(solver);
  std::fclose(records);
}

void emit_proof(const std::string &stem) {
  const std::string formula_text = dimacs(true);
  std::ofstream formula(stem + ".cnf");
  formula << formula_text;
  formula.close();
  Kernel kernel;
  Proof proof;
  assert(proof.open((stem + ".dsr").c_str(), ProofFormat::DsrAscii));
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  kernel.proof = &proof;
  parse_into(kernel, true);
  FILE *records = std::fopen((stem + ".records.jsonl").c_str(), "w+");
  assert(records);
  NaryCspRelationOwnerSession relations;
  NaryCspEqualitySession equality;
  NaryCspEqualityAmoSession session;
  prepare(kernel, relations, equality, records);
  const auto applied = nary_csp_equality_amo_apply(
      kernel, relations, equality, session, amo_limits(), {domains}, records);
  assert(applied.status == NaryCspEqualityAmoStatus::Applied);
  assert(default_solve(kernel) == 20);
  proof.flush();
  kernel.proof = nullptr;
  kernel.structural_provenance = nullptr;
  std::fclose(records);
}

} // namespace

int main(int argc, char **argv) {
  if (argc == 2) {
    emit_proof(argv[1]);
    return 0;
  }
  assert(argc == 1);
  lifecycle();
  public_wire_format();
  controller_decline_feedback();
  c_v12_deep_copies_and_executes_exact_payload();
  std::puts("test_nary_csp_equality_amo: passed");
}
