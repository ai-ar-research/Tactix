// The retired catalog slot 39, step id 28, the reserved n-ary CSP producer and
// the legacy n-ary CSP prefix payload are rejected by their validators and at
// every public C entry point.
#include "unit_test.hpp"
#include "tactix_c_api.hpp"
#include "strategy/inprocess_program.hpp"
#include "strategy/semantic_variable_advice.hpp"
#include <type_traits>
using namespace tactix::strategy;

template<class T> tactix_online_step &base(T &step) {
  if constexpr (std::is_same_v<T, tactix_online_step>) return step;
  else return base(step.base);
}
template<class T> tactix_online_step_v8 &v8(T &step) {
  if constexpr (std::is_same_v<T, tactix_online_step_v8>) return step;
  else return v8(step.base);
}
int main() {
  constexpr int retired = 39;
  Kernel kernel;
  assert(!stage_semantic_variable_candidate(kernel,
      SemanticVariableProducer::RetiredNaryCsp, 1,
      {{1, SemanticVariableRole::ExistingTheory, 1}}));
  assert(!valid_semantic_variable_producer(SemanticVariableProducer::RetiredNaryCsp));
  InprocessStep retired_step{28, "Probe", InprocessPhase::Probe};
  assert(!validate_inprocess_program(std::span(&retired_step, 1)));
  auto *solver = tactix_init();
  tactix_add(solver, 1); tactix_add(solver, 2); tactix_add(solver, 0);
  assert(tactix_action_kind(retired) == -1);
  uint64_t payload = 0;
  assert(tactix_apply(solver, retired) == -1);
  assert(tactix_apply_instance(solver, retired, 0) == -1);
  assert(tactix_apply_semantic_instance(solver, retired, 0) == -1);
  assert(tactix_rule_instances(solver, retired, &payload, 1) == -1);
  assert(tactix_rule_semantic_instances(solver, retired, &payload, 1) == -1);

  tactix_online_step_v8 step{};
  auto &b = base(step);
  b.step_id = 28; // online IDs name requests, not catalog slots
  b.rule_id = retired;
  b.granularity = TACTIX_ONLINE_GRANULARITY_CANONICAL;
  b.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE; b.repeat_limit = 1;
  b.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS; b.budget = 1;
  tactix_online_program_v8 program{&step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  const auto configure = [&] {
    return tactix_online_configure_v8(solver, &program, 0,
                                    TACTIX_ONLINE_RECONFIGURE_RESET);
  };
  assert(configure() == -1);
  for (int id = 0; id < tactix_rule_count(); ++id)
    if (!std::strcmp(tactix_rule_name(id), "Rephase")) b.rule_id = id;
  assert(configure() == 0);
  b.parameters.kind = TACTIX_ONLINE_PARAMETERS_NARY_CSP_PREFIX;
  assert(configure() == -1); // even an empty legacy tag is rejected
  b.parameters = {};
  step.nary_csp_prefix_instance_count = -1; assert(configure() == -1);
  step.nary_csp_prefix_instance_count = 1; assert(configure() == -1);
  step.nary_csp_prefix_instance_count = 0;
  tactix_online_nary_csp_prefix_instance legacy{};
  step.nary_csp_prefix_instances = &legacy; assert(configure() == -1);
  step.nary_csp_prefix_instance_count = 1; assert(configure() == -1);
  step.nary_csp_prefix_instances = nullptr;
  step.nary_csp_prefix_instance_count = 0; assert(configure() == 0);
  tactix_online_step_v15 later{};
  base(later) = b;
  tactix_online_program_v15 later_program{&later, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_configure_v15(solver, &later_program, 0,
      TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  v8(later).nary_csp_prefix_instances = &legacy;
  assert(tactix_online_configure_v15(solver, &later_program, 0,
      TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  tactix_release(solver);
}
