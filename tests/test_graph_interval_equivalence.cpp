#undef NDEBUG
#include "proof/proof.hpp"
#include "rules/graph_interval_equivalence/graph_interval_equivalence.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/registry.hpp"
#include "strategy/native_feedback.hpp"
#include "strategy/online_inprocess.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
struct tactix_solver;
tactix_solver *tactix_init(void);
void tactix_release(tactix_solver *);
void tactix_add(tactix_solver *, int);
int tactix_bind_structural_provenance(tactix_solver *, FILE *);
int tactix_rule_count(void);
const char *tactix_rule_name(int);
int tactix_action_kind(int);
int tactix_rule_instances(tactix_solver *, int, uint64_t *, int);
int tactix_rule_semantic_instances(tactix_solver *, int, uint64_t *, int);
int tactix_apply_instance(tactix_solver *, int, uint64_t);
}

using namespace tactix;

namespace {

using namespace strategy;
using namespace strategy::controller;

struct Capture final : FeedbackReceiver {
  std::vector<ActionFeedback> actions;
  void after_action(const ActionFeedback &value) override {
    actions.push_back(value);
  }
  void search_interval(const SearchIntervalFeedback &) override {}
};

void add(Kernel &kernel, std::initializer_list<int32_t> external) {
  std::vector<Lit> literals;
  for (int32_t literal : external) literals.push_back(internalize(literal));
  const uint64_t id = ++kernel.next_clause_id;
  if (literals.size() == 2)
    kernel_add_binary_clause(kernel, literals[0], literals[1], id);
  else
    kernel_add_large_clause(kernel, literals.data(), literals.size(), false, 0,
                            id);
}

void fixture(Kernel &kernel, bool complete) {
  kernel_reserve_vars(kernel, 11);
  add(kernel, {1, 4, 7});
  add(kernel, {2, 5, 8});
  add(kernel, {3, 6, 9});
  add(kernel, {-1, -4});
  add(kernel, {1, 2, 10});
  add(kernel, {-1, -10});
  add(kernel, {-2, -10});
  add(kernel, {1, 2, 11});
  add(kernel, {-1, -11});
  if (complete) add(kernel, {-2, -11});
}

std::string contents(FILE *file) {
  std::fflush(file);
  std::rewind(file);
  std::string result;
  char buffer[4096];
  while (const size_t size = std::fread(buffer, 1, sizeof buffer, file))
    result.append(buffer, size);
  assert(!std::ferror(file));
  return result;
}

void positive(const char *proof_path) {
  Kernel kernel;
  fixture(kernel, true);
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  Proof proof;
  assert(proof.open(proof_path, ProofFormat::DsrAscii));
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  kernel.proof = &proof;

  const RuleVTable &rule =
      tactix_rules().at(tactix_rule_id("GraphIntervalEquivalence"));
  assert(rule.guard(kernel));
  uint64_t instances[4]{};
  assert(rule.instances(kernel, instances, 4) == 1);
  assert(instances[0] == INSTANCE_CANONICAL);
  assert(!rule.semantic_instances && !rule.apply_semantic_instance);
  const size_t trail_before = kernel.trail.size();
  assert(rule.apply_instance(kernel, 1) == ApplyResult::Ok);
  assert(kernel_binary_live(kernel, internalize(-10), internalize(11)));
  assert(kernel_binary_live(kernel, internalize(10), internalize(-11)));
  assert(kernel.trail.size() == trail_before); // Propagate is a separate action.
  assert(kernel.stats.graph_interval_equivalence_published == 2);
  assert(kernel.stats.graph_interval_equivalence_proof_literal_occurrences == 4);
  assert(kernel.structural_action_sequence == 1);

  const uint64_t proof_id = kernel.next_clause_id;
  assert(rule.apply_instance(kernel, 1024) == ApplyResult::NoOp);
  assert(kernel.next_clause_id == proof_id);
  assert(kernel.structural_action_sequence == 1);
  assert(kernel.stats.graph_interval_equivalence_duplicates == 2);

  const std::string provenance = contents(records);
  assert(provenance.find("\"kind\":\"authenticated-duplicate-interval\"") !=
         std::string::npos);
  assert(provenance.find("\"row\":0,\"first\":0,\"last\":1") !=
         std::string::npos);
  assert(provenance.find("\"existing_variables_only\":true") !=
         std::string::npos);
  assert(provenance.find("\"proof_additions\":2") != std::string::npos);
  proof.flush();
  kernel.proof = nullptr;
  kernel.structural_provenance = nullptr;
  std::fclose(records);
}

void conflicting_definition_is_rejected() {
  Kernel kernel;
  fixture(kernel, true);
  add(kernel, {2, 3, 10});
  add(kernel, {-2, -10});
  add(kernel, {-3, -10});
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  const RuleVTable &rule =
      tactix_rules().at(tactix_rule_id("GraphIntervalEquivalence"));
  const uint64_t id = kernel.next_clause_id;
  assert(rule.apply_instance(kernel, 1) == ApplyResult::NoOp);
  assert(kernel.next_clause_id == id);
  assert(!kernel_binary_live(kernel, internalize(-10), internalize(11)));
  kernel.structural_provenance = nullptr;
  std::fclose(records);
}

void root_fixed_requires_separate_propagate() {
  Kernel kernel;
  fixture(kernel, true);
  kernel.unit_ids[var_of(internalize(-10))] = ++kernel.next_clause_id;
  kernel_assign_unit(kernel, internalize(-10));
  assert(propagate_service(kernel) == ApplyResult::Ok);
  assert(kernel_propagation_fixpoint(kernel));
  assert(!kernel.value(internalize(-11)));
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  const RuleVTable &rule =
      tactix_rules().at(tactix_rule_id("GraphIntervalEquivalence"));
  assert(rule.apply_instance(kernel, 1) == ApplyResult::Ok);
  assert(!kernel.value(internalize(-11)));
  assert(!kernel_propagation_fixpoint(kernel));
  const RuleVTable &propagate = tactix_rules().at(tactix_rule_id("Propagate"));
  assert(apply_rule(propagate, kernel) == ApplyResult::Ok);
  assert(kernel.value(internalize(-11)) > 0);
  assert(kernel_propagation_fixpoint(kernel));
  kernel.structural_provenance = nullptr;
  std::fclose(records);
}

int c_rule_id(const char *name) {
  for (int i = 0; i < tactix_rule_count(); ++i)
    if (!std::strcmp(tactix_rule_name(i), name)) return i;
  return -1;
}

void c_api_surface() {
  tactix_solver *solver = tactix_init();
  FILE *records = std::tmpfile();
  assert(records && tactix_bind_structural_provenance(solver, records) == 0);
  const int clauses[][4] = {{1, 4, 7, 0}, {2, 5, 8, 0}, {3, 6, 9, 0},
                            {-1, -4, 0, 0}, {1, 2, 10, 0},
                            {-1, -10, 0, 0}, {-2, -10, 0, 0},
                            {1, 2, 11, 0}, {-1, -11, 0, 0},
                            {-2, -11, 0, 0}};
  const int sizes[] = {3, 3, 3, 2, 3, 2, 2, 3, 2, 2};
  for (unsigned i = 0; i != 10; ++i) {
    for (int j = 0; j != sizes[i]; ++j) tactix_add(solver, clauses[i][j]);
    tactix_add(solver, 0);
  }
  const int id = c_rule_id("GraphIntervalEquivalence");
  assert(id >= 0 && tactix_action_kind(id) == 0);
  uint64_t values[4]{};
  assert(tactix_rule_instances(solver, id, values, 4) == 1);
  assert(values[0] == INSTANCE_CANONICAL);
  assert(tactix_rule_semantic_instances(solver, id, values, 4) == 1);
  assert(values[0] == INSTANCE_CANONICAL);
  assert(tactix_apply_instance(solver, id, 1) == 0);
  assert(tactix_apply_instance(solver, id, 1) == 1);
  assert(contents(records).find("\"proof_additions\":2") != std::string::npos);
  assert(tactix_bind_structural_provenance(solver, nullptr) == 0);
  tactix_release(solver);
  std::fclose(records);
}

void replay_and_feedback() {
  OnlineInvocation step;
  step.id = 41;
  step.rule = "GraphIntervalEquivalence";
  step.granularity = InvocationGranularity::OneInstance;
  step.instances = {3};
  step.work_unit = WorkUnit::Applications;
  step.budget = WorkLimit::bounded(1);
  const OnlineStrategyProgram program{{step}, {}};
  const std::string wire = serialize_online_strategy_program(program);
  OnlineStrategyProgram replay;
  std::string error;
  assert(deserialize_online_strategy_program(wire, replay, &error));
  assert(serialize_online_strategy_program(replay) == wire);
  assert(replay.sequence[0].instances == step.instances);

  Kernel kernel;
  fixture(kernel, true);
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  Capture capture;
  auto observe = [](const Kernel &value, const void *) {
    return observe_kernel(value);
  };
  execution::PolicyCompletion completion(kernel, &capture, observe, nullptr);
  completion.begin();
  const RuleVTable &rule =
      tactix_rules().at(tactix_rule_id("GraphIntervalEquivalence"));
  const ApplyResult result = completion.execute(
      FeedbackKind::Rule, "GraphIntervalEquivalence", 1,
      [&] {
        return execution::native_rule_feedback(
            kernel, "GraphIntervalEquivalence", 1,
            "generic bounded duplicate interval publication",
            [](const Kernel &state) {
              return state.stats.graph_interval_equivalence_work;
            },
            "graph-interval-work", WorkLimit::bounded(100000000));
      },
      [&] { return apply_rule_instance(rule, kernel, 1); });
  assert(result == ApplyResult::Ok);
  assert(capture.actions.size() == 1);
  const ActionFeedback &feedback = capture.actions.front();
  assert(feedback.native_request && feedback.native_request->invocation);
  assert(feedback.native_request->invocation->instances ==
         std::vector<uint64_t>{1});
  assert(feedback.native_request->work > 0);
  assert(feedback.after.cumulative.graph_interval_equivalence_published -
             feedback.before.cumulative.graph_interval_equivalence_published ==
         2);
  kernel.structural_provenance = nullptr;
  std::fclose(records);
}

void negative_near_miss() {
  Kernel kernel;
  fixture(kernel, false);
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  const RuleVTable &rule =
      tactix_rules().at(tactix_rule_id("GraphIntervalEquivalence"));
  const uint64_t id = kernel.next_clause_id;
  assert(rule.apply(kernel) == ApplyResult::NoOp);
  assert(kernel.next_clause_id == id);
  assert(kernel.structural_action_sequence == 0);
  assert(!kernel_binary_live(kernel, internalize(-10), internalize(11)));
  kernel.structural_provenance = nullptr;
  std::fclose(records);
}

} // namespace

int main(int argc, char **argv) {
  const char *proof = argc > 1 ? argv[1] : "/tmp/graph-interval-equivalence.dsr";
  positive(proof);
  negative_near_miss();
  conflicting_definition_is_rejected();
  root_fixed_requires_separate_propagate();
  c_api_surface();
  replay_and_feedback();
  std::puts("test_graph_interval_equivalence: all passed");
}
