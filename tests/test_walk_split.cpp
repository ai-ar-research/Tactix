#undef NDEBUG
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix
#include "policies/heuristics.hpp"
#include "policies/restart_controller.hpp"
#include "policies/ops/walk/walk.hpp"
#include "strategy/feedback_trace.hpp"
#include "strategy/guidance_execution.hpp"
#include "strategy/random_online.hpp"
#include <cassert>
#include <cstdio>
#include <sstream>
#include <string>
using namespace tactix;
using namespace tactix::strategy;
using namespace tactix::strategy::controller;

static void fixture(Kernel &k) {
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(-2));
  kernel_add_binary_clause(k, internalize(-1), internalize(-3));
  kernel_add_binary_clause(k, internalize(-2), internalize(-3));
  const Lit cover[] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, cover, 3, false, 0);
  k.h->saved = {1, 1, 1};
  k.h->target = {-1, 1, -1};
  k.h->best = {1, -1, 1};
  kernel_offer_local_search_partial_hint(k, LocalSearchKind::DirectCsp, {1, -1, -1});
  k.h->target_candidate.assign(3, k.local_search_hints[0].id);
}

static WalkParameters split(const Kernel &k, uint64_t prep = 10000,
                            uint64_t search = 0) {
  WalkParameters p;
  p.seed_candidate = k.local_search_hints[0].id;
  p.objective = WalkParameters::Objective::FullCurrentCnf;
  p.admission = WalkParameters::Admission::StrictlyBetter;
  p.preparation_budget = prep;
  p.step_budget = search;
  assert(p.valid());
  return p;
}

// Representation and all phase/provenance state, excluding deliberate effort
// telemetry and the Walk attempt clock.
static std::string image(const Kernel &k) {
  std::ostringstream s;
  s << k.nvars << ':' << k.next_clause_id << ':' << k.arena_generation << ':'
    << k.touch_epoch << ':' << k.strategy_epoch << ':' << k.h->target_assigned
    << ':' << k.h->best_assigned << ':' << k.propagate_head;
  for (auto word : k.arena) s << ":a" << word;
  for (auto word : k.watches.arena) s << ":w" << word;
  for (auto head : k.watches.heads)
    s << ":h" << head.offset << ',' << head.size << ',' << head.capacity;
  for (auto v : k.h->saved) s << ":s" << int(v);
  for (auto v : k.h->target) s << ":t" << int(v);
  for (auto v : k.h->best) s << ":b" << int(v);
  for (auto v : k.h->target_candidate) s << ":p" << v;
  for (auto lit : k.trail) s << ":l" << lit;
  return s.str();
}

static void preparation_and_export() {
  uint64_t preparation = 0;
  {
    Kernel k; ScopedHeuristics h(k); fixture(k);
    const auto best = k.h->best;
    const auto p = split(k);
    { ScopedWalkParameters scoped(k, p); assert(walk_apply(k) == ApplyResult::Ok); }
    preparation = k.stats.walk_preparation_steps;
    assert(preparation > 0 && !k.stats.walk_steps && !k.stats.walk_search_steps);
    assert(k.stats.walk_seed_ordinary_unsat == 3 &&
           k.stats.walk_seed_candidate_unsat == 0 &&
           k.stats.walk_seed_admitted == 1 && k.stats.walk_seed_exported == 1);
    assert(k.h->saved == std::vector<int8_t>({1, -1, -1}) &&
           k.h->target == k.h->saved && k.h->best == best);
    assert(k.h->target_candidate == std::vector<uint64_t>(3, 0));
    assert(!k.walk_seed_candidate && !k.walk_preparation_budget &&
           !k.walk_objective && !k.walk_admission);
    kernel_check_invariants(k);
  }
  // Boundary failure includes export reservation: no partial table/import or
  // insufficient preparation is allowed to change formula or phase memory.
  for (auto allowance : {uint64_t{0}, uint64_t{1}, preparation - 1}) {
    Kernel k; ScopedHeuristics h(k); fixture(k);
    const auto before = image(k);
    auto p = split(k, allowance, 1000);
    ScopedWalkParameters scoped(k, p);
    assert(walk_apply(k) == ApplyResult::NoOp && image(k) == before);
    assert(k.stats.walk_preparation_steps <= allowance &&
           k.stats.walk_preparation_budget_exhausted == 1 &&
           !k.stats.walk_search_steps && !k.stats.walk_seed_admitted &&
           !k.stats.walk_seed_exported && !k.stats.rephased);
  }
}

static void rejected_and_full_objective() {
  for (bool worse : {false, true}) {
    Kernel k; ScopedHeuristics h(k); fixture(k);
    k.h->saved = {1, -1, -1};
    if (worse) kernel_offer_local_search_partial_hint(k, LocalSearchKind::DirectCsp, {1, 1, 1});
    const auto before = image(k);
    ScopedWalkParameters scoped(k, split(k, 10000, 1000));
    assert(walk_apply(k) == ApplyResult::NoOp && image(k) == before);
    assert(k.stats.walk_seed_rejected == 1 && !k.stats.walk_seed_admitted &&
           !k.stats.walk_search_steps && !k.stats.rephased);
  }
  Kernel k; ScopedHeuristics h(k); fixture(k);
  // On irredundant clauses this seed improves 3 -> 1. Four learned copies
  // of the entailed cover reverse that preference. Full-CNF admission must
  // include those valid redundant clauses rather than admit the worse seed.
  kernel_offer_local_search_partial_hint(k, LocalSearchKind::DirectCsp, {-1, -1, -1});
  const Lit learned[] = {internalize(1), internalize(2), internalize(3)};
  for (unsigned i = 0; i != 4; ++i)
    kernel_add_large_clause(k, learned, 3, true, 0);
  const auto before = image(k);
  ScopedWalkParameters scoped(k, split(k));
  assert(walk_apply(k) == ApplyResult::NoOp && image(k) == before);
  assert(k.stats.walk_seed_ordinary_unsat == 3 &&
         k.stats.walk_seed_candidate_unsat == 5 && k.stats.walk_seed_rejected == 1);
}

static void search_independence_and_mapping() {
  for (uint64_t prep : {uint64_t{10000}, uint64_t{20000}}) {
    Kernel k; ScopedHeuristics h(k); fixture(k);
    // Ordinary assignment costs 3; candidate costs 1, allowing a productive
    // flip with one independently available native search credit.
    kernel_offer_local_search_partial_hint(k, LocalSearchKind::DirectCsp, {-1, -1, -1});
    ScopedWalkParameters scoped(k, split(k, prep, 1000));
    assert(walk_apply(k) == ApplyResult::Ok && k.stats.walk_flips == 1);
    assert(k.stats.walk_search_steps > 0 && k.stats.walk_search_steps <= 1000 &&
           k.stats.walk_steps == k.stats.walk_search_steps &&
           k.stats.walk_preparation_steps < prep &&
           k.stats.walk_seed_candidate_unsat == 1 && k.stats.walk_minimum == 0);
  }
  {
    Kernel k; ScopedHeuristics h(k); fixture(k);
    kernel_offer_local_search_partial_hint(k, LocalSearchKind::DirectCsp, {-1, -1, -1});
    // Repeated physical binaries make the selected variable's occurrence lists
    // large; one search credit cannot permit an unchecked full flip.
    for (unsigned i = 0; i != 128; ++i)
      kernel_add_binary_clause(k, internalize(-1), internalize(-2));
    const auto before = image(k);
    ScopedWalkParameters scoped(k, split(k, 100000, 1));
    assert(walk_apply(k) == ApplyResult::NoOp && image(k) == before &&
           k.stats.walk_search_budget_exhausted == 1 &&
           k.stats.walk_search_steps <= 1 && !k.stats.walk_flips &&
           !k.stats.walk_seed_exported);
  }
  {
    Kernel k; ScopedHeuristics h(k); fixture(k);
    kernel_reserve_vars(k, 5);
    k.h->saved[3] = -1; k.h->saved[4] = 1;
    k.var_status.assign(k.nvars, VarStatus::Active);
    k.var_status[3] = VarStatus::Eliminated;
    kernel_offer_local_search_partial_hint(k, LocalSearchKind::DirectCsp,
                                          {1, -1, -1, 1, 0});
    const auto best = k.h->best;
    ScopedWalkParameters scoped(k, split(k));
    assert(walk_apply(k) == ApplyResult::Ok && k.h->best == best &&
           k.h->saved[3] == -1 && k.h->saved[4] == 1 &&
           k.stats.walk_seed_imported_phases == 3);
  }
  Kernel k; ScopedHeuristics h(k); fixture(k);
  kernel_reserve_vars(k, 5); // unknown extension phases stay native
  k.h->saved[3] = -1; k.h->saved[4] = 1;
  kernel_assign_unit(k, internalize(1));
  assert(!kernel_propagate(k));
  // The root already satisfies the complete objective: no import can improve
  // it. Root-satisfied clauses add zero and are not retired by preparation.
  const auto before = image(k);
  ScopedWalkParameters scoped(k, split(k));
  assert(walk_apply(k) == ApplyResult::NoOp && image(k) == before);
  assert(!k.stats.walk_seed_exported && !k.stats.walk_search_steps);
}

struct Receiver : FeedbackReceiver {
  std::vector<ActionFeedback> actions;
  void after_action(const ActionFeedback &a) override { actions.push_back(a); }
  void search_interval(const SearchIntervalFeedback &) override {}
};
static void replay_and_feedback() {
  Kernel k; ScopedHeuristics h(k); fixture(k);
  OnlineInvocation invocation;
  invocation.id = 73; invocation.rule = "Walk"; invocation.parameters = split(k, 0, 19);
  invocation.granularity = InvocationGranularity::OneInstance;
  invocation.instances = {0}; // exact zero search dose overrides typed dose
  OnlineStrategyProgram program{{invocation}, {}};
  const auto wire = serialize_online_strategy_program(program);
  assert(wire.find(":walk3(") != std::string::npos);
  OnlineStrategyProgram decoded; std::string error;
  assert(deserialize_online_strategy_program(wire, decoded, &error));
  assert(serialize_online_strategy_program(decoded) == wire);
  const auto &p = std::get<WalkParameters>(decoded.sequence[0].parameters);
  assert(p.preparation_budget == 0 && p.step_budget == 19 &&
         p.objective == WalkParameters::Objective::FullCurrentCnf &&
         p.admission == WalkParameters::Admission::StrictlyBetter);
  Receiver receiver;
  RestartController controller(k, {}, &receiver);
  ActionRequest request;
  request.sequence = 1; request.expected = state_version(k); request.invocation = invocation;
  const auto result = controller.apply_rule(request, {});
  assert(result.invocation.status == InvocationStatus::BudgetExhausted &&
         result.invocation.applications == 1 && !result.invocation.made_rule_progress);
  assert(receiver.actions.size() == 1 && receiver.actions[0].stop == StopReason::Budget);
  assert(serialize_feedback(receiver.actions[0]).find("walk_preparation_budget_exhausted") != std::string::npos);
}

static void shared_success() {
  Kernel native, controlled;
  ScopedHeuristics nh(native), ch(controlled);
  fixture(native); fixture(controlled);
  kernel_offer_local_search_partial_hint(native, LocalSearchKind::DirectCsp, {-1, -1, -1});
  kernel_offer_local_search_partial_hint(controlled, LocalSearchKind::DirectCsp, {-1, -1, -1});
  const auto selected = split(native);
  execution::WalkRequest native_request{0, 0, selected.seed_candidate,
      selected.objective, selected.admission, selected.preparation_budget};
  assert(execution::execute_action(native, native_request) == ApplyResult::Ok);
  assert(!native.walk_objective && !native.walk_admission &&
         !native.walk_preparation_budget && !native.walk_seed_candidate);
  Receiver receiver;
  RestartController controller(controlled, {}, &receiver);
  ActionRequest request;
  request.sequence = 1; request.expected = state_version(controlled);
  request.invocation.rule = "Walk"; request.invocation.parameters = split(controlled, 10000, 999);
  request.invocation.granularity = InvocationGranularity::OneInstance;
  request.invocation.instances = {0};
  const auto result = controller.apply_rule(request, {});
  assert(result.invocation.status == InvocationStatus::Completed &&
         result.invocation.made_rule_progress && result.invocation.applications == 1);
  assert(controlled.h->saved == native.h->saved &&
         controlled.h->target == native.h->target &&
         controlled.h->target_candidate == native.h->target_candidate);
  assert(controlled.stats.walk_preparation_steps == native.stats.walk_preparation_steps &&
         controlled.stats.walk_seed_admitted == 1 &&
         controlled.stats.walk_seed_exported == 1 && !controlled.stats.walk_search_steps);
  assert(receiver.actions.size() == 1 &&
         receiver.actions[0].after.cumulative.walk_seed_exported == 1);
}

static void stale_stop_and_empty() {
  for (bool stopped : {false, true}) {
    Kernel k; ScopedHeuristics h(k); fixture(k);
    auto p = split(k); const auto before = image(k);
    volatile sig_atomic_t stop = stopped;
    KernelStopPublisher publish(k, &stop);
    if (!stopped) kernel_offer_local_search_partial_hint(k, LocalSearchKind::DirectCsp, {-1, 1, -1});
    ScopedWalkParameters scoped(k, p);
    assert(walk_apply(k) == ApplyResult::NoOp && image(k) == before);
    assert(!k.stats.walk_seed_exported && !k.stats.walk_search_steps);
    if (stopped) assert(k.stats.walk_seed_interrupted == 1);
    else assert(k.stats.local_search_hints_stale == 1);
  }
  Kernel k; ScopedHeuristics h(k); kernel_reserve_vars(k, 3);
  kernel_offer_local_search_partial_hint(k, LocalSearchKind::DirectCsp, {1, -1, 1});
  const auto before = image(k);
  ScopedWalkParameters scoped(k, split(k));
  assert(walk_apply(k) == ApplyResult::NoOp && image(k) == before &&
         k.stats.walk_seed_rejected == 1 && !k.stats.rephased);
}

static void late_stop_preserves_export_state() {
  Kernel k; ScopedHeuristics h(k); fixture(k);
  // Invert the fixed LCG for this fixture's 108th stop poll: only the final
  // pre-export poll fires. This is one fixed microcase, not a seed campaign.
  k.stop_fuzz = UINT32_MAX;
  k.stop_fuzz_state = UINT64_C(2924202282665541252);
  const auto before = image(k);
  ScopedWalkParameters scoped(k, split(k));
  assert(walk_apply(k) == ApplyResult::NoOp && image(k) == before &&
         k.stats.stop_fuzz_fires == 1 && k.stats.walk_seed_interrupted == 1 &&
         k.stats.walk_seed_admitted == 1 && !k.stats.walk_seed_exported &&
         !k.stats.rephased);
}

static void late_stop_after_positive_search() {
  Kernel k; ScopedHeuristics h(k); fixture(k);
  kernel_offer_local_search_partial_hint(k, LocalSearchKind::DirectCsp, {-1, -1, -1});
  // This fixture completes one flip; only its final pre-export poll fires.
  k.stop_fuzz = UINT32_MAX;
  k.stop_fuzz_state = UINT64_C(10993615960876256494);
  const auto before = image(k);
  ScopedWalkParameters scoped(k, split(k, 10000, 1000));
  assert(walk_apply(k) == ApplyResult::NoOp && image(k) == before &&
         k.stats.stop_fuzz_fires == 1 && k.stats.walk_seed_interrupted == 1 &&
         k.stats.walk_flips == 1 && k.stats.walk_minimum == 0 &&
         k.stats.walk_seed_improved == 1 && k.stats.walk_seed_admitted == 1 && !k.stats.walk_seed_exported);
}

static void c_v13_validation() {
  auto *solver = tactix_init(); assert(solver);
  tactix_add(solver, 1); tactix_add(solver, 2); tactix_add(solver, 0);
  tactix_online_step_v13 step{};
  auto &base = step.base.base.base.base.base.base.base.base.base.base.base.base;
  base.step_id = 1;
  for (int i = 0; i < tactix_rule_count(); ++i)
    if (std::string(tactix_rule_name(i)) == "Walk") base.rule_id = i;
  base.granularity = TACTIX_ONLINE_GRANULARITY_ONE;
  uint64_t instance = 0;
  base.instances = &instance; base.instance_count = 1;
  base.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE; base.repeat_limit = 1;
  base.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS; base.budget = 1;
  base.parameters.kind = TACTIX_ONLINE_PARAMETERS_WALK;
  base.parameters.value.walk.seed_candidate = 256;
  base.parameters.value.walk.step_budget = 0;
  tactix_online_walk_score_point curve[] = {{0, 2000}, {3000, 2500}};
  base.parameters.value.walk.score_curve = curve;
  base.parameters.value.walk.score_curve_count = 2;
  step.walk_profile = {0, TACTIX_WALK_OBJECTIVE_FULL_CURRENT_CNF,
                         TACTIX_WALK_ADMISSION_STRICTLY_BETTER};
  tactix_online_program_v13 program{&step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  step.walk_profile.admission = 99;
  assert(tactix_online_configure_v13(solver, &program, 1, TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  step.walk_profile.admission = TACTIX_WALK_ADMISSION_STRICTLY_BETTER;
  assert(tactix_online_configure_v13(solver, &program, 2, TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  curve[0].break_base_milli = 0; step.walk_profile.objective = 99;
  tactix_online_result result{};
  assert(tactix_online_advance(solver, &result) == 0 && result.status == TACTIX_ONLINE_STALE_REQUEST);
  tactix_release(solver);
}
int main() {
  preparation_and_export(); rejected_and_full_objective();
  search_independence_and_mapping(); replay_and_feedback();
  shared_success(); stale_stop_and_empty(); late_stop_preserves_export_state(); late_stop_after_positive_search(); c_v13_validation();
}
