#undef NDEBUG

#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix

#include "frontend/parse.hpp"
#include "policies/default_policy.hpp"
#include "policies/random_controller_policy.hpp"
#include "strategy/feedback_trace.hpp"
#include "proof/proof.hpp"
#include "rules/semantic_extension/nary_csp_equality_action.hpp"
#include "rules/semantic_extension/nary_csp_equality_amo_action.hpp"
#include "strategy/inprocess_program.hpp"
#include "strategy/online_inprocess.hpp"
#include "strategy/random_online.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace tactix;
using namespace tactix::strategy;
using namespace tactix::strategy::controller;

namespace {

std::string dimacs(bool unsat = false) {
  std::vector<std::vector<int32_t>> clauses{
      {1, 2, 3}, {-1, -2}, {-1, -3}, {-2, -3},
      {4, 5, 6}, {-4, -5}, {-4, -6}, {-5, -6},
      {7, 8, 9}, {-7, -8}, {-7, -9}, {-8, -9},
      {-1, -4, -7}, {-2, -5, -8}, {-3, -6, -9}};
  if (unsat)
    clauses.insert(clauses.end(),
                   {{20, 21}, {20, -21}, {-20, 21}, {-20, -21}});
  std::ostringstream out;
  out << "p cnf " << (unsat ? 21 : 9) << ' ' << clauses.size() << '\n';
  for (const auto &clause : clauses) {
    for (int32_t literal : clause) out << literal << ' ';
    out << "0\n";
  }
  return out.str();
}

void parse_into(Kernel &kernel, bool unsat = false) {
  const std::string text = dimacs(unsat);
  FILE *input = std::tmpfile();
  assert(input);
  assert(std::fwrite(text.data(), 1, text.size(), input) == text.size());
  std::rewind(input);
  assert(parse_dimacs(input, kernel) == nullptr);
  std::fclose(input);
  assert(!kernel.inconsistent && kernel_propagation_fixpoint(kernel));
}

NaryCspPrefixActionLimits prefix_limits() {
  return {100, 1000, 100000, 8, 16, 100, 7, 16,
          10000, 100, 1000, 100000, 1, 100, 100, 100000};
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

void prepare(Kernel &kernel, NaryCspPrefixActionSession &prefix,
             NaryCspEqualitySession &equality, FILE *records) {
  kernel.structural_provenance = records;
  kernel.nary_csp_prefix_action_session = &prefix;
  kernel.nary_csp_equality_session = &equality;
  const NaryCspPrefixActionSelection prefix_selection{
      domains, {2, {{0, 0}, {1, 0}}}};
  const auto retained = nary_csp_prefix_action_apply(
      kernel, prefix, prefix_limits(), prefix_selection);
  assert(retained.status == NaryCspPrefixActionStatus::Applied);
  for (uint32_t residual = 0; residual != 3; ++residual) {
    std::array<uint32_t, 2> equal{};
    size_t next = 0;
    for (uint32_t domain = 0; domain != 3; ++domain)
      if (domain != residual) equal[next++] = domain;
    const NaryCspEqualityActionSelection selection{
        domains, {equal[0], equal[1], residual}};
    const auto applied = nary_csp_equality_action_apply(
        kernel, prefix, equality, equality_limits(), selection);
    assert(applied.status == NaryCspEqualityActionStatus::Applied);
  }
  assert(equality.records().size() == 3);
  for (const auto &record : equality.records())
    assert(record.definition_clauses.size() == 3 * domains.front().size());
}

void exhaustive_model_and_lifecycle() {
  Kernel kernel;
  parse_into(kernel);
  FILE *records = std::tmpfile();
  assert(records);
  NaryCspPrefixActionSession prefix;
  NaryCspEqualitySession equality;
  NaryCspEqualityAmoSession session;
  prepare(kernel, prefix, equality, records);
  kernel.nary_csp_equality_amo_session = &session;

  const auto offered = nary_csp_equality_amo_offer(
      kernel, prefix, equality, session, amo_limits(), {4, 1, 100, false, {}});
  assert(offered.status == NaryCspEqualityAmoOfferStatus::Ready &&
         offered.complete && offered.offers.size() == 1);
  const auto applied = nary_csp_equality_amo_apply(
      kernel, prefix, equality, session, amo_limits(), offered.offers.front(),
      records);
  assert(applied.status == NaryCspEqualityAmoStatus::Applied &&
         applied.emitted_binary_clauses == 3 &&
         applied.equality_variables == 3 && kernel.nvars == 13 &&
         session.keys().size() == 1);

  // All 24 source models satisfy the three binary consequences when each
  // extension is interpreted by its committed equality definition.
  unsigned models = 0;
  for (unsigned a = 0; a != 3; ++a)
    for (unsigned b = 0; b != 3; ++b)
      for (unsigned c = 0; c != 3; ++c) {
        if (a == b && b == c) continue;
        ++models;
        const std::array<bool, 3> eq{b == c, a == c, a == b};
        assert(!(eq[0] && eq[1]) && !(eq[0] && eq[2]) &&
               !(eq[1] && eq[2]));
      }
  assert(models == 24);

  const auto duplicate = nary_csp_equality_amo_apply(
      kernel, prefix, equality, session, amo_limits(), {domains}, records);
  assert(duplicate.status == NaryCspEqualityAmoStatus::Duplicate);
  const auto suppressed = nary_csp_equality_amo_offer(
      kernel, prefix, equality, session, amo_limits(), {4, 1, 100, false, {}});
  assert(suppressed.status == NaryCspEqualityAmoOfferStatus::Ready &&
         suppressed.complete && suppressed.offers.empty() &&
         suppressed.committed_keys_skipped == 1);

  NaryCspEqualityAmoSelection malformed{domains};
  malformed.domains[2].pop_back();
  assert(!nary_csp_equality_amo_selection_valid(malformed));
  Kernel other;
  parse_into(other);
  const auto wrong_session = nary_csp_equality_amo_apply(
      other, prefix, equality, session, amo_limits(), {domains}, records);
  assert(wrong_session.status == NaryCspEqualityAmoStatus::Inapplicable);
  kernel.structural_provenance = nullptr;
  std::fclose(records);
}

void public_identity_and_replay() {
  assert(tactix_rules().size() == 45);
  assert(std::string(tactix_rules()[44].name) == "NaryCspEqualityAmo");
  const auto reference = strategy::reference_inprocess_program();
  const auto found = std::find_if(reference.begin(), reference.end(),
      [](const auto &step) { return step.id == 33; });
  assert(found != reference.end() && found->rule == "NaryCspEqualityAmo" &&
         !found->enabled);
  strategy::NaryCspEqualityAmoParameters parameters{
      100, 1000, 300000, 16, 100, 10000, 100, 1000, 100000, 100, 100};
  strategy::OnlineInvocation invocation;
  invocation.id = 33;
  invocation.rule = "NaryCspEqualityAmo";
  invocation.parameters = parameters;
  invocation.granularity = strategy::InvocationGranularity::OneInstance;
  invocation.nary_csp_equality_amo_instances = {{domains}};
  invocation.repeat = {strategy::RepeatMode::Once, 1};
  invocation.budget = strategy::WorkLimit::bounded(1);
  strategy::OnlineStrategyProgram program{{invocation}, {}};
  std::string error;
  assert(strategy::validate_online_strategy_program(program, &error));
  const std::string wire = strategy::serialize_online_strategy_program(program);
  assert(wire.find(":nary-csp-equality-amo1(") != std::string::npos);
  assert(wire.find(
      ":nary-csp-equality-amo[3,3,1,2,3,3,4,5,6,3,7,8,9]") !=
      std::string::npos);
  strategy::OnlineStrategyProgram replay;
  assert(strategy::deserialize_online_strategy_program(wire, replay, &error));
  assert(strategy::serialize_online_strategy_program(replay) == wire);
}

void stale_definition_declines() {
  Kernel kernel;
  parse_into(kernel);
  FILE *records = std::tmpfile();
  assert(records);
  NaryCspPrefixActionSession prefix;
  NaryCspEqualitySession equality;
  NaryCspEqualityAmoSession session;
  prepare(kernel, prefix, equality, records);
  bool removed = false;
  const auto &target = equality.records().front().definition_clauses.front();
  std::vector<Lit> wanted;
  for (int32_t literal : target) wanted.push_back(kernel_internalize(kernel, literal));
  std::sort(wanted.begin(), wanted.end());
  for (Ref ref = 0; ref < kernel.arena.size();) {
    Clause *clause = clause_at(kernel.arena, ref);
    const uint32_t words = clause_actual_words(clause);
    std::vector<Lit> current(clause->lits, clause->lits + clause->size);
    std::sort(current.begin(), current.end());
    if (!clause->garbage() && current == wanted) {
      clause->flags |= Clause::GARBAGE;
      removed = true;
      break;
    }
    ref += words;
  }
  assert(removed);
  const auto stale = nary_csp_equality_amo_apply(
      kernel, prefix, equality, session, amo_limits(), {domains}, records);
  assert(stale.status == NaryCspEqualityAmoStatus::Stale &&
         session.keys().empty());
  kernel.structural_provenance = nullptr;
  std::fclose(records);
}

void native_controller_offer_applies_once() {
  Kernel kernel;
  parse_into(kernel);
  FILE *records = std::tmpfile();
  assert(records);
  NaryCspPrefixActionSession prefix;
  NaryCspEqualitySession equality;
  NaryCspEqualityAmoSession session;
  prepare(kernel, prefix, equality, records);
  kernel.nary_csp_equality_amo_session = &session;
  RandomControllerConfig config;
  config.parameters.rules = {"NaryCspEqualityAmo"};
  config.parameters.nary_csp_equality_amo_reference = {
      100, 1000, 300000, 16, 100, 10000, 100, 1000, 100000, 100, 100};
  config.nary_csp_equality_amo_producer.enabled = true;
  config.nary_csp_equality_amo_producer.maximum_offers = 1;
  RestartController controller(kernel);
  RandomControllerSelector selector(9);
  ActionRequest request;
  std::string error;
  bool selected = false;
  bool saw_offer_effort = false;
  for (unsigned attempt = 0; attempt != 64 && !selected; ++attempt) {
    assert(selector.choose(kernel, controller.observe(), config, request,
                           error));
    for (const auto &effort : request.producer_effort)
      if (effort.family == "NaryCspEqualityAmo" && effort.offers == 1)
        saw_offer_effort = true;
    selected = request.type == ActionType::Rule;
  }
  assert(selected && request.invocation.rule == "NaryCspEqualityAmo" &&
         request.invocation.nary_csp_equality_amo_instances.size() == 1 &&
         saw_offer_effort);
  const auto applied =
      controller.apply_rule(request, config.allowance("NaryCspEqualityAmo"));
  assert(applied.invocation.status == InvocationStatus::Completed &&
         applied.invocation.made_rule_progress && session.keys().size() == 1 &&
         kernel.stats.nary_csp_equality_amo_attempts == 1 &&
         kernel.stats.nary_csp_equality_amo_applied == 1 &&
         kernel.stats.nary_csp_equality_amo_binary_clauses == 3);
  const std::string raw = serialize_observation(controller.observe());
  assert(raw.find("\"nary_csp_equality_amo_attempts\":1") !=
             std::string::npos &&
         raw.find("\"nary_csp_equality_amo_binary_clauses\":3") !=
             std::string::npos);
  kernel.structural_provenance = nullptr;
  std::fclose(records);
}

void c_v12_deep_copies_and_executes_exact_payload() {
  tactix_c_handle *solver = tactix_init();
  assert(solver);
  FILE *records = std::tmpfile();
  assert(records && tactix_bind_structural_provenance(solver, records) == 0);
  const std::string formula = dimacs();
  std::istringstream clauses(formula);
  std::string line;
  while (std::getline(clauses, line)) {
    if (line.empty() || line[0] == 'c' || line[0] == 'p') continue;
    std::istringstream literals(line);
    int literal = 0;
    while (literals >> literal) tactix_add(solver, literal);
  }

  int d0[] = {1, 2, 3}, d1[] = {4, 5, 6}, d2[] = {7, 8, 9};
  tactix_online_nary_csp_domain c_domains[] = {
      {d0, 3}, {d1, 3}, {d2, 3}};
  tactix_online_nary_csp_choice prefix[] = {{0, 0}, {1, 0}};
  tactix_online_nary_csp_prefix_instance prefix_instance{
      c_domains, 3, 2, prefix, 2};
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
  set_common(steps[0], 28, 39);
  auto &prefix_parameter = base(steps[0]).parameters;
  prefix_parameter.kind = TACTIX_ONLINE_PARAMETERS_NARY_CSP_PREFIX;
  const auto pl = prefix_limits();
  prefix_parameter.value.nary_csp_prefix = {
      pl.maximum_source_clauses, pl.maximum_source_literals,
      pl.maximum_recognition_work, pl.maximum_domains,
      pl.maximum_domain_values, pl.maximum_forbidden_tuples,
      pl.maximum_prefix_arity, pl.maximum_definition_clause_literals,
      pl.maximum_proof_work, pl.maximum_proof_additions,
      pl.maximum_proof_literal_occurrences, pl.maximum_journal_bytes,
      pl.maximum_extent_growth, pl.maximum_external_extent,
      pl.maximum_session_keys, pl.maximum_session_work};
  steps[0].base.base.base.base.nary_csp_prefix_instances = &prefix_instance;
  steps[0].base.base.base.base.nary_csp_prefix_instance_count = 1;

  const auto el = equality_limits();
  set_common(steps[1], 31, 42);
  auto &equality_parameter = base(steps[1]).parameters;
  equality_parameter.kind = TACTIX_ONLINE_PARAMETERS_NARY_CSP_EQUALITY;
  equality_parameter.value.nary_csp_equality = {
      el.maximum_source_clauses, el.maximum_source_literals,
      el.maximum_recognition_work, el.maximum_domain_values,
      el.maximum_forbidden_tuples, el.maximum_proof_work,
      el.maximum_proof_additions, el.maximum_proof_literal_occurrences,
      el.maximum_journal_bytes, el.maximum_extent_growth,
      el.maximum_external_extent, el.maximum_session_keys};
  steps[1].base.base.nary_csp_equality_instances = equality_instances;
  steps[1].base.base.nary_csp_equality_instance_count = 1;

  set_common(steps[2], 33, 44);
  auto &amo_parameter = base(steps[2]).parameters;
  amo_parameter.kind = TACTIX_ONLINE_PARAMETERS_NARY_CSP_EQUALITY_AMO;
  const auto al = amo_limits();
  amo_parameter.value.nary_csp_equality_amo = {
      al.maximum_source_clauses, al.maximum_source_literals,
      al.maximum_recognition_work, al.maximum_domain_values,
      al.maximum_forbidden_tuples, al.maximum_proof_work,
      al.maximum_proof_additions, al.maximum_proof_literal_occurrences,
      al.maximum_journal_bytes, al.maximum_external_extent,
      al.maximum_session_keys};
  steps[2].nary_csp_equality_amo_instances = &amo_instance;
  steps[2].nary_csp_equality_amo_instance_count = 1;

  tactix_online_program_v12 program{
      &steps[0], 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_configure_v12(
             solver, &program, 10, TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  d0[0] = 99;
  tactix_online_result result{};
  assert(tactix_online_advance(solver, &result) == 0 &&
         result.rule_id == 39 && result.status == TACTIX_ONLINE_COMPLETED &&
         result.made_rule_progress);
  d0[0] = 1;

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
  NaryCspPrefixActionSession prefix;
  NaryCspEqualitySession equality;
  NaryCspEqualityAmoSession session;
  prepare(kernel, prefix, equality, records);
  const auto applied = nary_csp_equality_amo_apply(
      kernel, prefix, equality, session, amo_limits(), {domains}, records);
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
  exhaustive_model_and_lifecycle();
  public_identity_and_replay();
  stale_definition_declines();
  native_controller_offer_applies_once();
  c_v12_deep_copies_and_executes_exact_payload();
  std::puts("test_nary_csp_equality_amo: passed");
}
