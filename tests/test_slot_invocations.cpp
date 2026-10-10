// Inprocessing slots as typed online invocations (gap plan Step 5, A10/C4).
// The typed surfaces DefaultPolicy's slot scheduler now poses are public:
// Substitute's round batch and per-round effort ceiling, a Vivify band's
// cumulative ledger, Strengthen's forward-closure post-application, and the
// forward-round and BVE-reschedule rule-state supports. Each converts from C
// exactly, rejects permanent installation, and a public application performs
// the same transition as DefaultPolicy's composition of the shared request.
// The BVE session owns the score-heap ordering profile and its kernel hook.
#undef NDEBUG
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix

#include "api/api_internal.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "policies/rule_publication.hpp"
#include "rules/registry.hpp"
#include "strategy/bve_invocation.hpp"
#include "strategy/occurrence_execution.hpp"
#include "strategy/online_inprocess.hpp"
#include "strategy/publication_execution.hpp"
#include "strategy/slot_execution.hpp"
#include "strategy/substitute_execution.hpp"
#include "strategy/vivify_execution.hpp"
#include "strategy/work_execution.hpp"

#include <cassert>
#include <cstring>
#include <vector>

using namespace tactix;

namespace {

int rule_id(const char *name) {
  const int id = tactix_rule_id(name);
  assert(id >= 0);
  return id;
}

// x1 <-> x2 by binaries plus two ternaries over x3: Substitute's first round
// retires one of x1/x2 and continues into a second round.
tactix_c_handle *substitute_solver() {
  tactix_c_handle *s = tactix_init();
  assert(tactix_set_guidance_publication(s, TACTIX_GUIDANCE_NATIVE) == 0);
  for (const auto &clause : std::vector<std::vector<int>>{
           {-1, 2}, {-2, 1}, {1, -3, 2}, {-1, 3, -2}, {3, 4, 5}, {-4, -5}}) {
    for (int lit : clause) tactix_add(s, lit);
    tactix_add(s, 0);
  }
  assert(tactix_status(s) == 0);
  return s;
}

Kernel &kernel(tactix_c_handle *s) { return api_detail::kernel_for_fixed_testing(s); }

tactix_online_step substitute_step(uint32_t rounds, int ceiling, int budget_mode,
                                   uint64_t budget) {
  tactix_online_step step{};
  step.step_id = 1;
  step.rule_id = rule_id("Substitute");
  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_SUBSTITUTE;
  step.parameters.value.substitute.max_rounds = rounds;
  step.parameters.value.substitute.round_effort_ceiling = ceiling;
  step.granularity = TACTIX_ONLINE_GRANULARITY_CANONICAL;
  step.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.repeat_limit = 1;
  step.rule_budget_mode = budget_mode;
  step.rule_budget = budget;
  step.budget_unlimited = 1;
  return step;
}

int configure(tactix_c_handle *s, const tactix_online_step &step) {
  const tactix_online_program program{&step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  return tactix_online_configure(s, &program, 1, TACTIX_ONLINE_RECONFIGURE_RESET);
}

void test_conversion_and_rejections() {
  tactix_c_handle *s = substitute_solver();
  // Substitute's round batch converts exactly.
  assert(configure(s, substitute_step(2, 1, TACTIX_ONLINE_RULE_BUDGET_BOUNDED, 7)) == 0);
  {
    const auto &step = api_detail::online_program_for_fixed_testing(s)->sequence.front();
    const auto &p = std::get<strategy::SubstituteParameters>(step.parameters);
    assert(p.max_rounds == 2 && p.round_effort_ceiling && !p.demote_substituted);
  }
  // A per-round ceiling needs a round batch.
  assert(configure(s, substitute_step(0, 1, TACTIX_ONLINE_RULE_BUDGET_INHERIT, 0)) != 0);
  // A Vivify band converts exactly; its ledger fields need the band flag.
  tactix_online_step vivify{};
  vivify.step_id = 2;
  vivify.rule_id = rule_id("Vivify");
  vivify.parameters.kind = TACTIX_ONLINE_PARAMETERS_VIVIFY;
  vivify.parameters.value.vivify = {3, 10, 1, 0, 1, 500, 20};
  vivify.granularity = TACTIX_ONLINE_GRANULARITY_CANONICAL;
  vivify.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  vivify.repeat_limit = 1;
  vivify.budget_unlimited = 1;
  assert(configure(s, vivify) == 0);
  {
    const auto &step = api_detail::online_program_for_fixed_testing(s)->sequence.front();
    const auto &p = std::get<strategy::VivifyParameters>(step.parameters);
    assert(p.scope == 3 && p.band && p.band_budget == 500 && p.band_offset == 20);
  }
  vivify.parameters.value.vivify.band = 0;
  assert(configure(s, vivify) != 0);
  // The forward closure follows only Strengthen.
  vivify.parameters.value.vivify = {3, 10, 1, 0, 0, 0, 0};
  vivify.post_application = TACTIX_ONLINE_POST_FORWARD_CLOSURE;
  assert(configure(s, vivify) != 0);
  tactix_online_step strengthen{};
  strengthen.step_id = 3;
  strengthen.rule_id = rule_id("Strengthen");
  strengthen.granularity = TACTIX_ONLINE_GRANULARITY_CANONICAL;
  strengthen.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  strengthen.repeat_limit = 1;
  strengthen.budget_unlimited = 1;
  strengthen.post_application = TACTIX_ONLINE_POST_FORWARD_CLOSURE;
  assert(configure(s, strengthen) == 0);
  assert(api_detail::online_program_for_fixed_testing(s)->sequence.front().post_application ==
         strategy::PostApplication::ForwardClosure);
  // Round batches and band ledgers belong to an application.
  tactix_online_parameters install{};
  install.kind = TACTIX_ONLINE_PARAMETERS_SUBSTITUTE;
  install.value.substitute.max_rounds = 2;
  assert(tactix_install_parameters(s, rule_id("Substitute"), &install) != 0);
  install.value.substitute.max_rounds = 0;
  assert(tactix_install_parameters(s, rule_id("Substitute"), &install) == 0);
  install = {};
  install.kind = TACTIX_ONLINE_PARAMETERS_VIVIFY;
  install.value.vivify = {0, 10, 1, 0, 1, 5, 0};
  assert(tactix_install_parameters(s, rule_id("Vivify"), &install) != 0);
  tactix_release(s);
}

// DefaultPolicy's composition of the typed Substitute application.
ApplyResult native_substitute(Kernel &k, const strategy::SubstituteParameters &p,
                              uint64_t budget) {
  const RuleVTable &rule = tactix_rules()[static_cast<size_t>(rule_id("Substitute"))];
  execution::WorkSettingsScope work(k, {budget, std::nullopt, std::nullopt, std::nullopt},
                                    execution::WorkSettingsExit::ClearBudget);
  return execution::execute_action(k, execution::RuleApplicationRequest{}, [&] {
    return apply_rule_structured(rule, k, "rounds", [&] {
      return execution::execute_substitute_rounds(k, p, [&](size_t begin) {
        heuristics_consume_root_assignments(k, begin);
        heuristics_consume_retirements(k);
      }).result;
    });
  }, HeuristicRulePublisher{}).result;
}

void same_formula(const Kernel &a, const Kernel &b) {
  assert(a.trail == b.trail && a.inconsistent == b.inconsistent);
  assert(a.live_irredundant_large == b.live_irredundant_large);
  assert(a.binary_ids.size() == b.binary_ids.size());
  assert(a.stats.substitute_rounds == b.stats.substitute_rounds);
  assert(a.stats.substitute_vars == b.stats.substitute_vars);
  for (Var v = 0; v < a.nvars; ++v) {
    assert(kernel_var_retired(a, v) == kernel_var_retired(b, v));
    assert(heuristics_var_active(a, v) == heuristics_var_active(b, v));
  }
}

void test_substitute_rounds() {
  for (const bool ceiling : {false, true}) {
    tactix_c_handle *pub = substitute_solver();
    tactix_c_handle *native = substitute_solver();
    // A zero rule budget with the ceiling stops after the first productive
    // round; without it the batch continues into round two.
    assert(configure(pub, substitute_step(2, ceiling, TACTIX_ONLINE_RULE_BUDGET_BOUNDED, 0)) == 0);
    tactix_online_result result{};
    assert(tactix_online_advance(pub, &result) == 0);
    assert(result.applications == 1 && result.made_rule_progress);
    strategy::SubstituteParameters p;
    p.max_rounds = 2;
    p.round_effort_ceiling = ceiling;
    assert(native_substitute(kernel(native), p, 0) == ApplyResult::Ok);
    same_formula(kernel(pub), kernel(native));
    assert(kernel(pub).stats.substitute_rounds == (ceiling ? 1u : 2u));
    assert(kernel(pub).large_watches_connected && kernel(native).large_watches_connected);
    tactix_release(pub);
    tactix_release(native);
  }
}

void test_rule_state_supports() {
  tactix_c_handle *s = substitute_solver();
  Kernel &k = kernel(s);
  k.subsume_complete = false;
  assert(tactix_support(s, TACTIX_SUPPORT_BEGIN_FORWARD_ROUND, 0) == 0);
  assert(k.subsume_complete && k.strengthen_complete);
  assert(tactix_support(s, TACTIX_SUPPORT_BEGIN_FORWARD_ROUND, 0) == 1);
  assert(tactix_support(s, TACTIX_SUPPORT_BEGIN_FORWARD_ROUND, 1) == -1);
  k.bve_retry.assign(k.nvars, 0);
  assert(tactix_support(s, TACTIX_SUPPORT_RESCHEDULE_BVE, 0) == 0);
  for (const uint8_t flag : k.bve_retry) assert(flag == 1);
  assert(tactix_support(s, TACTIX_SUPPORT_RESCHEDULE_BVE, 0) == 1);
  assert(tactix_support(s, TACTIX_SUPPORT_RESCHEDULE_BVE, 2) == -1);
  tactix_release(s);
}

// tests/test_native_policy_feedback.cpp's Vivify fixture: redundant tier-1
// clauses a scope-3 band vivifies.
void vivify_fixture(Kernel &k) {
  k.stats.conflicts = 1;
  kernel_reserve_vars(k, 160);
  for (uint32_t clause = 0; clause < 8; ++clause) {
    Lit lits[20];
    for (uint32_t i = 0; i < 20; ++i) lits[i] = make_lit(clause * 20 + i, true);
    kernel_add_large_clause(k, lits, 20, true, 2, 0);
  }
  k.tier1 = k.tier2 = 2;
}

void test_vivify_band() {
  for (const uint64_t budget : {uint64_t{0}, uint64_t{1}, uint64_t{1000000}}) {
    Kernel online, native;
    vivify_fixture(online);
    vivify_fixture(native);
    ScopedHeuristics h_online(online), h_native(native);
    strategy::VivifyParameters band = strategy::VivifyParameters::from_kernel(native);
    band.scope = 3;
    band.band = true;
    band.band_budget = budget;
    band.band_offset = 0;
    // Native: one band inside a dose, as DefaultPolicy's Vivify dose runs it.
    {
      const RuleVTable &rule = tactix_rules()[static_cast<size_t>(rule_id("Vivify"))];
      execution::VivifyDoseScope dose(native);
      (void)execution::execute_action(native, rule,
                                      execution::VivifyBandRequest{band, nullptr, nullptr});
    }
    // Public: the same typed parameters as one online canonical application.
    strategy::OnlineInvocation step;
    step.id = 1;
    step.rule = "Vivify";
    step.parameters = band;
    step.granularity = strategy::InvocationGranularity::CanonicalApplication;
    strategy::OnlineStrategyProgram program;
    program.sequence.push_back(step);
    strategy::OnlineStrategyExecutor executor;
    std::string error;
    assert(executor.configure(program, 1, strategy::ReconfigureMode::ResetRuntime, &error));
    (void)executor.advance(online);
    assert(online.stats.vivify_checked == native.stats.vivify_checked);
    assert(online.stats.vivify_rounds == native.stats.vivify_rounds);
    assert(online.stats.vivify_shrunken == native.stats.vivify_shrunken);
    assert(online.stats.vivify_subsumed == native.stats.vivify_subsumed);
    assert(online.arena.size() == native.arena.size());
    assert(online.vivify_scope == 0 && !online.work_budget && !online.work_budget_offset);
  }
}

// The session owns the score-heap ordering profile and its kernel hook.
void test_bve_ordering_profile() {
  tactix_c_handle *s = substitute_solver();
  Kernel &k = kernel(s);
  const RuleVTable &rule = tactix_rules()[static_cast<size_t>(rule_id("BVE"))];
  OccIndex index;
  execution::OccurrenceSession occurrences(
      k, execution::OccurrenceSessionRequest{index, execution::OccurrenceAttachment::Build, true});
  std::vector<uint64_t> candidates;
  for (Var v = 0; v < k.nvars; ++v) candidates.push_back(lit_instance(k, make_lit(v, false)));
  k.bve_retry.assign(k.nvars, 1);
  strategy::InvocationBudget budget(strategy::WorkUnit::BveTicks, strategy::WorkLimit::unlimited());
  {
    strategy::BveInvocationSession session(rule, k, strategy::BveParameters::from_kernel(k), budget);
    session.order_by_score_heap(candidates.data(), static_cast<uint32_t>(candidates.size()));
    assert(session.ordered() && k.bve_score_update_hook && k.bve_occurrence_count_image);
    uint32_t popped = 0;
    while (!session.ordering_empty()) {
      (void)session.next_ordered();
      ++popped;
    }
    assert(popped == k.nvars);
    session.finish_ordering();
    assert(!k.bve_score_update_hook && !k.bve_score_update_state &&
           !k.bve_occurrence_count_image && !session.ordered());
    // An unfinished ordering is released with the session.
    session.order_by_score_heap(candidates.data(), static_cast<uint32_t>(candidates.size()));
  }
  assert(!k.bve_score_update_hook && !k.bve_occurrence_count_image);
  occurrences.close();
  tactix_release(s);
}

void test_serialization() {
  using namespace strategy;
  OnlineStrategyProgram program;
  OnlineInvocation substitute;
  substitute.id = 1;
  substitute.rule = "Substitute";
  substitute.parameters = SubstituteParameters{};
  program.sequence.push_back(substitute);
  // Default fields keep the established text.
  const std::string plain = serialize_online_strategy_program(program);
  assert(plain.find(":substitute(0)") != std::string::npos);
  SubstituteParameters rounds;
  rounds.max_rounds = 3;
  rounds.round_effort_ceiling = true;
  program.sequence.front().parameters = rounds;
  OnlineInvocation vivify;
  vivify.id = 2;
  vivify.rule = "Vivify";
  VivifyParameters band;
  band.band = true;
  band.band_budget = 11;
  band.band_offset = 4;
  vivify.parameters = band;
  program.sequence.push_back(vivify);
  OnlineInvocation strengthen;
  strengthen.id = 3;
  strengthen.rule = "Strengthen";
  strengthen.post_application = PostApplication::ForwardClosure;
  program.sequence.push_back(strengthen);
  const std::string text = serialize_online_strategy_program(program);
  OnlineStrategyProgram parsed;
  std::string error;
  assert(deserialize_online_strategy_program(text, parsed, &error));
  assert(serialize_online_strategy_program(parsed) == text);
  const auto &p = std::get<SubstituteParameters>(parsed.sequence[0].parameters);
  assert(p.max_rounds == 3 && p.round_effort_ceiling);
  const auto &v = std::get<VivifyParameters>(parsed.sequence[1].parameters);
  assert(v.band && v.band_budget == 11 && v.band_offset == 4);
  assert(parsed.sequence[2].post_application == PostApplication::ForwardClosure);
}

} // namespace

int main() {
  test_conversion_and_rejections();
  test_substitute_rounds();
  test_rule_state_supports();
  test_vivify_band();
  test_bve_ordering_profile();
  test_serialization();
  return 0;
}
