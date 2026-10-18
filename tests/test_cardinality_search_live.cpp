#undef NDEBUG
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix
#include "frontend/parse.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/semantic_extension/cardinality_search_action.hpp"
#include "rules/semantic_extension/cardinality_search_live.hpp"
#include "strategy/online_inprocess.hpp"
#include "strategy/random_online.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>
#include <unistd.h>

using namespace tactix;
using namespace tactix::cardinality_search;
using CardinalityClause = tactix::cardinality_search::Clause;

namespace {

Constraint constraint(std::vector<int32_t> literals, uint32_t lower,
                      uint32_t upper) {
  Constraint result;
  result.literals = std::move(literals);
  result.lower = lower;
  result.upper = upper;
  const uint32_t size = result.literals.size();
  for (uint32_t mask = 1; mask < (1u << size); ++mask) {
    const uint32_t width = __builtin_popcount(mask);
    const bool positive = lower && width == size - lower + 1;
    const bool negative = upper < size && width == upper + 1;
    if (!positive && !negative) continue;
    CardinalityClause clause;
    for (uint32_t at = 0; at < size; ++at)
      if (mask & (1u << at)) clause.push_back(result.literals[at]);
    if (positive) result.premises.push_back(clause);
    if (negative) {
      for (int32_t &literal : clause) literal = -literal;
      result.premises.push_back(clause);
    }
  }
  return result;
}

const std::vector<Constraint> overlap{
    constraint({1, 2, 3, 4}, 2, 4),
    constraint({2, 3, 4, 5}, 0, 2)};
const std::vector<Constraint> odd{
    constraint({1, 2}, 1, 1), constraint({2, 3}, 1, 1),
    constraint({1, 3}, 1, 1)};

template <class T> decltype(auto) oldest_online_step(T &value) {
  if constexpr (requires { value.base; })
    return oldest_online_step(value.base);
  else
    return (value);
}

Kernel install(const std::vector<Constraint> &constraints, bool omit_last) {
  Kernel kernel;
  size_t count = 0, total = 0;
  for (const auto &constraint : constraints) total += constraint.premises.size();
  for (const auto &constraint : constraints)
    for (const CardinalityClause &premise : constraint.premises) {
      if (omit_last && ++count == total) continue;
      std::vector<Lit> internal;
      for (int32_t literal : premise)
        internal.push_back(internalize(literal));
      ingest_clause(kernel, internal);
    }
  finish_input(kernel);
  return kernel;
}

CardinalitySearchLiveRequest request(const std::vector<Constraint> &constraints,
                                     FILE *records) {
  CardinalitySearchLiveRequest result;
  result.constraints = constraints;
  result.action_id = 1;
  result.semantic_id = 1;
  result.records = records;
  return result;
}

void nonroot_implication_and_failure_atomicity() {
  Kernel kernel = install(overlap, false);
  FILE *records = std::tmpfile();
  assert(records);
  auto selected = request(overlap, records);
  kernel_assign_decision(kernel, internalize(5));
  assert(propagate_apply(kernel) == ApplyResult::Ok);
  assert(kernel.value(internalize(1)) == 0);
  const uint64_t before = kernel.next_clause_id;
  const size_t trail = kernel.trail.size();
  const auto checked = cardinality_search_live_inspect(kernel, selected);
  assert(checked.status == CardinalitySearchLiveStatus::Applied &&
         (checked.reason == CardinalityClause{-5, 1}) &&
         checked.proof_ids.size() > 1);
  assert(kernel.next_clause_id == before && kernel.trail.size() == trail);
  auto tight = selected;
  tight.maximum_source_scan_work = 1;
  assert(cardinality_search_live_apply(kernel, tight).status ==
         CardinalitySearchLiveStatus::ResourceExhausted);
  assert(kernel.next_clause_id == before && kernel.trail.size() == trail);
  const auto applied = cardinality_search_live_apply(kernel, selected);
  assert(applied.status == CardinalitySearchLiveStatus::Applied &&
         applied.propagation_pending && applied.installed_clause_id &&
         applied.installed_clause_id == kernel.next_clause_id);
  assert(propagate_apply(kernel) == ApplyResult::Ok);
  assert(kernel.value(internalize(1)) > 0);
  kernel_backtrack(kernel, 0);
  assert(propagate_apply(kernel) == ApplyResult::Ok);
  kernel_assign_decision(kernel, internalize(5));
  assert(propagate_apply(kernel) == ApplyResult::Ok);
  assert(kernel.value(internalize(1)) > 0);
  assert(cardinality_search_live_apply(kernel, selected).status ==
         CardinalitySearchLiveStatus::NoEvent);
  std::fclose(records);

  Kernel missing = install(overlap, true);
  records = std::tmpfile();
  assert(records);
  auto missing_request = request(overlap, records);
  kernel_assign_decision(missing, internalize(5));
  assert(propagate_apply(missing) == ApplyResult::Ok);
  const auto rejected = cardinality_search_live_apply(missing, missing_request);
  assert(rejected.status == CardinalitySearchLiveStatus::NoSource);
  std::fclose(records);
}

void typed_action_uses_same_live_path() {
  Kernel kernel = install(overlap, false);
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  kernel_assign_decision(kernel, internalize(5));
  assert(propagate_apply(kernel) == ApplyResult::Ok);
  assert(cardinality_search_action_guard(kernel));
  const std::vector<CardinalitySearchBound> bounds{
      {{1, 2, 3, 4}, 2, 4}, {{2, 3, 4, 5}, 0, 2}};
  const auto applied = cardinality_search_action_apply(
      kernel, bounds, CardinalitySearchActionLimits{});
  assert(applied.status == CardinalitySearchActionStatus::Applied &&
         applied.proposed_action_id == 1 &&
         applied.committed_action_id == 1 &&
         kernel.structural_action_sequence == 1 &&
         applied.live.installed_clause_id);
  assert(propagate_apply(kernel) == ApplyResult::Ok);
  assert(kernel.value(internalize(1)) > 0);
  const uint64_t before = kernel.next_clause_id;
  const auto rejected = cardinality_search_action_apply(
      kernel, {{{1, 1}, 1, 1}}, CardinalitySearchActionLimits{});
  assert(rejected.status == CardinalitySearchActionStatus::Invalid &&
         kernel.next_clause_id == before &&
         kernel.structural_action_sequence == 1);
  std::fclose(records);
}

void public_online_action_requires_exact_bounds_and_followup() {
  using namespace tactix::strategy;
  OnlineInvocation step;
  step.id = 35;
  step.rule = "CardinalitySearch";
  step.parameters = CardinalitySearchActionLimits{};
  step.granularity = InvocationGranularity::OneInstance;
  step.cardinality_search_instances = {{{{1, 2, 3, 4}, 2, 4},
                                        {{2, 3, 4, 5}, 0, 2}}};
  step.budget = WorkLimit::bounded(1);
  OnlineStrategyProgram program{{step}, {}};
  std::string error;
  assert(!validate_online_strategy_program(program, &error));
  step.post_application = PostApplication::Propagate;
  program.sequence = {step};
  assert(validate_online_strategy_program(program, &error));
  const std::string serialized = serialize_online_strategy_program(program);
  OnlineStrategyProgram replay;
  assert(deserialize_online_strategy_program(serialized, replay, &error));
  assert(replay.sequence.front().cardinality_search_instances ==
         step.cardinality_search_instances);

  Kernel kernel = install(overlap, false);
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  kernel_assign_decision(kernel, internalize(5));
  assert(propagate_apply(kernel) == ApplyResult::Ok);
  OnlineStrategyExecutor executor;
  assert(executor.configure(program, 1, ReconfigureMode::ResetRuntime,
                            &error));
  const auto applied = executor.advance(kernel);
  assert(applied.status == InvocationStatus::Completed &&
         applied.applications == 1 && kernel.value(internalize(1)) > 0);
  assert(kernel.structural_action_sequence == 1);
  std::fclose(records);
}

void c17_deep_copies_bounds_and_executes_search_action() {
  tactix_c_handle *solver = tactix_init();
  FILE *records = std::tmpfile();
  assert(solver && records);
  assert(tactix_bind_structural_provenance(solver, records) == 0);
  for (const auto &constraint : overlap)
    for (const CardinalityClause &premise : constraint.premises) {
      for (int32_t literal : premise) tactix_add(solver, literal);
      tactix_add(solver, 0);
    }
  int64_t first[] = {1, 2, 3, 4};
  int64_t second[] = {2, 3, 4, 5};
  const tactix_online_cardinality_search_bound bounds[] = {
      {first, 4, 2, 4}, {second, 4, 0, 2}};
  const tactix_online_cardinality_search_instance instance{bounds, 2};
  tactix_online_step_v17 step{};
  auto &base = oldest_online_step(step);
  base.step_id = 35;
  base.rule_id = tactix_rule_id("CardinalitySearch");
  assert(base.rule_id == 46);
  base.parameters.kind = TACTIX_ONLINE_PARAMETERS_INHERIT;
  base.granularity = TACTIX_ONLINE_GRANULARITY_ONE;
  base.post_application = TACTIX_ONLINE_POST_PROPAGATE;
  base.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  base.repeat_limit = 1;
  base.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  base.budget = 1;
  const auto limits = CardinalitySearchActionLimits{};
  step.cardinality_search = {
      limits.inspection.maximum_support_variables,
      limits.inspection.maximum_constraints,
      limits.inspection.maximum_source_clauses,
      limits.inspection.maximum_reason_width,
      limits.inspection.maximum_work,
      limits.resolution.maximum_work,
      limits.resolution.maximum_additions,
      limits.resolution.maximum_added_literals,
      limits.maximum_source_scan_work,
      limits.maximum_arena_words,
      limits.maximum_journal_bytes};
  step.cardinality_search_instances = &instance;
  step.cardinality_search_instance_count = 1;
  const tactix_online_program_v17 program{
      &step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_configure_v17(
             solver, &program, 1, TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  first[0] = 9;
  second[0] = 9;
  assert(tactix_decide(solver, 5) == 0);
  assert(tactix_apply(solver, tactix_rule_id("Propagate")) == 0);
  tactix_online_result result{};
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.rule_id == 46 && result.applications == 1 &&
         tactix_value(solver, 1) > 0);
  tactix_release(solver);
  std::fclose(records);
}

void write_cnf(const std::string &path,
               const std::vector<Constraint> &constraints,
               bool checker_closure) {
  size_t clauses = checker_closure ? 4 : 0;
  for (const auto &constraint : constraints) clauses += constraint.premises.size();
  std::ofstream out(path);
  assert(out);
  out << "p cnf " << (checker_closure ? 7 : 3) << ' ' << clauses << '\n';
  for (const auto &constraint : constraints)
    for (const CardinalityClause &premise : constraint.premises) {
      for (int32_t literal : premise) out << literal << ' ';
      out << "0\n";
    }
  if (checker_closure)
    out << "6 7 0\n6 -7 0\n-6 7 0\n-6 -7 0\n";
}

void public_c17_conflict_proof_fixture(const std::string &cnf_path,
                                       const std::string &proof_path) {
  write_cnf(cnf_path, odd, false);
  tactix_c_handle *solver = tactix_init();
  FILE *records = std::tmpfile();
  assert(solver && records);
  assert(tactix_bind_structural_provenance(solver, records) == 0);
  assert(tactix_proof(solver, proof_path.c_str(),
                      TACTIX_PROOF_DSR_ASCII) == 0);
  for (const auto &constraint : odd)
    for (const CardinalityClause &premise : constraint.premises) {
      for (int32_t literal : premise) tactix_add(solver, literal);
      tactix_add(solver, 0);
    }
  const int64_t first[] = {1, 2}, second[] = {2, 3}, third[] = {1, 3};
  const tactix_online_cardinality_search_bound bounds[] = {
      {first, 2, 1, 1}, {second, 2, 1, 1}, {third, 2, 1, 1}};
  const tactix_online_cardinality_search_instance instance{bounds, 3};
  tactix_online_step_v17 step{};
  auto &base = oldest_online_step(step);
  base.step_id = 35;
  base.rule_id = 46;
  base.parameters.kind = TACTIX_ONLINE_PARAMETERS_INHERIT;
  base.granularity = TACTIX_ONLINE_GRANULARITY_ONE;
  base.post_application = TACTIX_ONLINE_POST_PROPAGATE;
  base.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  base.repeat_limit = 1;
  base.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  base.budget = 1;
  const auto limits = CardinalitySearchActionLimits{};
  step.cardinality_search = {
      limits.inspection.maximum_support_variables,
      limits.inspection.maximum_constraints,
      limits.inspection.maximum_source_clauses,
      limits.inspection.maximum_reason_width,
      limits.inspection.maximum_work,
      limits.resolution.maximum_work,
      limits.resolution.maximum_additions,
      limits.resolution.maximum_added_literals,
      limits.maximum_source_scan_work,
      limits.maximum_arena_words,
      limits.maximum_journal_bytes};
  step.cardinality_search_instances = &instance;
  step.cardinality_search_instance_count = 1;
  const tactix_online_program_v17 program{
      &step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_configure_v17(
             solver, &program, 1, TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  tactix_online_result result{};
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.rule_id == 46 && result.status == TACTIX_ONLINE_UNSAT);
  tactix_release(solver);
  std::fclose(records);
}

void proof_fixture(const std::string &cnf_path,
                   const std::string &proof_path, bool conflict,
                   ProofFormat format = ProofFormat::DsrAscii) {
  const auto &constraints = conflict ? odd : overlap;
  write_cnf(cnf_path, constraints, !conflict);
  Kernel kernel = install(constraints, false);
  if (!conflict) {
    kernel_reserve_vars(kernel, 7);
    kernel.next_clause_id += 4; // checker-only clauses precede new proof IDs
  }
  FILE *records = std::tmpfile();
  assert(records);
  auto selected = request(constraints, records);
  Proof proof;
  assert(proof.open(proof_path.c_str(), format));
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  uint64_t input_id = 0;
  for (const auto &constraint : constraints)
    for (const auto &external : constraint.premises) {
      std::vector<Lit> clause;
      for (int32_t literal : external) clause.push_back(internalize(literal));
      proof.seed_population(++input_id, clause.data(), clause.size());
    }
  if (!conflict) {
    const int32_t closure[][2] = {{6, 7}, {6, -7}, {-6, 7}, {-6, -7}};
    for (const auto &external : closure) {
      const Lit clause[] = {internalize(external[0]), internalize(external[1])};
      proof.seed_population(++input_id, clause, 2);
    }
  }
  assert(input_id == kernel.next_clause_id);
  proof.finish_input_transaction();
  kernel.proof = &proof;
  if (!conflict) {
    kernel_assign_decision(kernel, internalize(5));
    assert(propagate_apply(kernel) == ApplyResult::Ok);
  }
  const auto applied = cardinality_search_live_apply(kernel, selected);
  assert(applied.status == CardinalitySearchLiveStatus::Applied);
  if (conflict)
    assert(kernel.inconsistent && applied.reason.empty() &&
           applied.result == ApplyResult::Unsat);
  else {
    assert((applied.reason == CardinalityClause{-5, 1}));
    assert(propagate_apply(kernel) == ApplyResult::Ok);
    assert(kernel.value(internalize(1)) > 0);
    const Lit positive = internalize(6);
    const Lit negative = internalize(-6);
    proof.add_rup(++kernel.next_clause_id, &positive, 1, nullptr, 0);
    proof.add_rup(++kernel.next_clause_id, &negative, 1, nullptr, 0);
    proof.add_rup(++kernel.next_clause_id, nullptr, 0, nullptr, 0);
  }
  proof.flush();
  kernel.proof = nullptr;
  std::fclose(records);
}

} // namespace

int main(int argc, char **argv) {
  nonroot_implication_and_failure_atomicity();
  typed_action_uses_same_live_path();
  public_online_action_requires_exact_bounds_and_followup();
  c17_deep_copies_bounds_and_executes_search_action();
  if (argc == 2) {
    proof_fixture(std::string(argv[1]) + "/implication.cnf",
                  std::string(argv[1]) + "/implication.dsr", false);
    proof_fixture(std::string(argv[1]) + "/conflict.cnf",
                  std::string(argv[1]) + "/conflict.dsr", true);
    public_c17_conflict_proof_fixture(
        std::string(argv[1]) + "/public_conflict.cnf",
        std::string(argv[1]) + "/public_conflict.dsr");
  } else if (argc == 3 && std::string(argv[1]) == "--proof-pbp") {
    proof_fixture(std::string(argv[2]) + "/implication.cnf",
                  std::string(argv[2]) + "/implication.pbp", false,
                  ProofFormat::PbpStreaming);
    proof_fixture(std::string(argv[2]) + "/conflict.cnf",
                  std::string(argv[2]) + "/conflict.pbp", true,
                  ProofFormat::PbpStreaming);
  } else assert(argc == 1);
  std::puts("test_cardinality_search_live: passed");
}
