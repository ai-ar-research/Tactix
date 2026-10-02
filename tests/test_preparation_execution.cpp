#undef NDEBUG
#include "policies/heuristics.hpp"
#include "policies/rule_publication.hpp"
#include "rules/factor/factor.hpp"
#include "rules/propagate/propagate.hpp"
#include "strategy/bve_occurrence_cache.hpp"
#include "strategy/inprocess_program.hpp"
#include "strategy/parameter_execution.hpp"
#include "strategy/publication_execution.hpp"
#include "strategy/root_execution.hpp"
#include "strategy/work_execution.hpp"

#include <cassert>
#include <optional>
#include <sstream>

using namespace tactix;
using namespace tactix::execution;
using namespace tactix::strategy;

static void test_parameter_lifetime() {
  Kernel k;
  ScopedHeuristics h(k);
  kernel_reserve_vars(k, 5);
  for (int f : {1, 2})
    for (int r : {3, 4, 5})
      kernel_add_binary_clause(k, internalize(f), internalize(r));
  k.factor_demote_fresh = false;
  ActionParameters parameters = FactorParameters{5, 0, 2, false, true};
  {
    ParameterExecutionScope<FactorParameters, VivifyParameters> outer(k, parameters);
    assert(outer.active() && k.factor_demote_fresh);
    {
      ActionParameters inherited = BceParameters{17};
      ParameterExecutionScope<FactorParameters, VivifyParameters> scope(k, inherited);
      assert(!scope.active() && k.factor_demote_fresh);
      ActionParameters inner_parameters = FactorParameters{4, 9, 3, true, false};
      ParameterExecutionScope<FactorParameters> inner(k, inner_parameters);
      assert(!k.factor_demote_fresh && k.factor_bound == 9);
    }
    assert(k.factor_demote_fresh && k.factor_bound == 0);
    FactorGrid grid;
    grid.factors = {internalize(1), internalize(2)};
    grid.remainders = {{internalize(3)}, {internalize(4)}, {internalize(5)}};
    assert(factor_apply_grid(k, grid) == ApplyResult::Ok);
    assert(!k.fresh_var_events.empty());
    heuristics_consume_rule_advice(k);
    assert(k.stats.factor_demoted_vars == 1 && k.fresh_var_events.empty());
  }
  assert(!k.factor_demote_fresh);
  k.vivify_scope = 2;
  parameters = VivifyParameters{5, false, true, 23};
  {
    ParameterExecutionScope<VivifyParameters> outer(k, parameters);
    assert(k.vivify_scope == 5 && !k.vivify_reuse && k.vivify_flush);
    parameters = VivifyParameters{1, true, false, 7};
    { ParameterExecutionScope<VivifyParameters> inner(k, parameters); }
    assert(k.vivify_scope == 5 && k.vivify_irredundant_sort_ratio == 23);
  }
  assert(k.vivify_scope == 2 && k.vivify_reuse && !k.vivify_flush);
  kernel_check_invariants(k);
}

static void test_walk_budget_precedence() {
  for (bool bounded : {false, true}) {
    Kernel k;
    k.work_budget = 71; k.work_budget_offset = 13;
    k.work_budget_bounded = bounded; k.midround_budget = false;
    k.walk_random_seed = 29;
    WalkParameters p;
    p.step_budget = 43; p.random_seed = 17;
    ActionParameters parameters = p;
    {
      ParameterExecutionScope<WalkParameters> scope(k, parameters);
      assert(k.work_budget == 43 && k.work_budget_bounded && k.walk_random_seed == 17);
      {
        ScopedRuleWorkBudget dose(k, {RuleWorkBudgetMode::Bounded, 0});
        assert(k.work_budget == 0 && k.work_budget_offset == 0 && k.midround_budget);
      }
      assert(k.work_budget == 43 && k.work_budget_offset == 13 && !k.midround_budget);
    }
    assert(k.work_budget == 71 && k.work_budget_offset == 13);
    assert(k.work_budget_bounded == bounded && !k.midround_budget && k.walk_random_seed == 29);
  }
}

static void root_fixture(Kernel &k, bool terminal) {
  kernel_reserve_vars(k, 3);
  k.h->saved.assign(k.nvars, 1);
  k.h->stable = true;
  k.h->target.assign(k.nvars, 1);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  if (terminal) {
    kernel_assign_unit(k, internalize(-1));
    kernel_assign_unit(k, internalize(-2));
  } else {
    kernel_assign_decision(k, internalize(1));
    assert(propagate_apply(k) == ApplyResult::Ok);
    kernel_assign_decision(k, internalize(-3));
    kernel_assign_unit(k, internalize(-2));
  }
  k.unflushed_root = 11;
  k.stats.restarts = 37;
}

static std::string state(const Kernel &k) {
  std::ostringstream out;
  out << k.level << ':' << k.inconsistent << ':' << k.propagate_head << ':'
      << k.unflushed_root << ':' << k.inprocessing << ':' << k.phase_saving_suppressed;
  for (Lit lit : k.trail) out << ':' << lit;
  for (auto phase : k.h->saved) out << ":s" << int(phase);
  for (auto phase : k.h->target) out << ":t" << int(phase);
  const auto &s = k.stats;
  out << ':' << s.restarts << ':' << s.restarts_reused << ':' << s.ticks
      << ':' << s.search_ticks << ':' << s.propagations << ':' << s.target_saved
      << ':' << s.best_saved << ':' << s.root_assignments;
  return out.str();
}

static void test_root_preparation(bool probing, bool terminal) {
  Kernel native, shared;
  ScopedHeuristics nh(native), sh(shared);
  root_fixture(native, terminal); root_fixture(shared, terminal);
  const auto before = shared.stats.search_ticks;
  const auto roots = shared.stats.root_assignments;
  ApplyResult expected;
  {
    std::optional<ExecutionScope<ExecutionContext::Inprocessing>> scope;
    if (probing) scope.emplace(native);
    if (native.level > 0 && restart_guard(native)) {
      if (!probing) heuristics_update_target_phases(native);
      (void)restart_backtrack_to(native, 0);
    }
    expected = propagate_apply(native);
    if (expected != ApplyResult::Unsat) native.unflushed_root = 0;
  }
  unsigned restarts = 0;
  const auto actual = execute_action(
      shared, RootPreparationRequest{probing, true},
      [&] { if (!probing) heuristics_update_target_phases(shared); },
      [&](const RestartRequest &request) {
        assert(request.keep_level == 0 && request.purpose == RestartPurpose::Preparation);
        assert(shared.inprocessing == probing && shared.phase_saving_suppressed == probing);
        ++restarts;
        return execute_action(shared, request);
      },
      [&] { return propagate_apply(shared); });
  assert(actual == expected && state(native) == state(shared));
  assert(restarts == unsigned(!terminal) && shared.stats.restarts == 37);
  assert(!shared.inprocessing && !shared.phase_saving_suppressed);
  if (!terminal) {
    assert(shared.level == 0 && shared.value(internalize(1)) > 0);
    assert(shared.stats.root_assignments > roots);
    assert((shared.stats.search_ticks > before) == !probing);
    assert(shared.unflushed_root == 0);
  } else assert(shared.inconsistent && shared.unflushed_root == 11);
  kernel_check_invariants(shared);
}

static void occurrence_fixture(Kernel &k) {
  kernel_reserve_vars(k, 4);
  const Lit a[] = {internalize(1), internalize(2), internalize(3)};
  const Lit b[] = {internalize(-1), internalize(2), internalize(4)};
  kernel_add_large_clause(k, a, 3, false, 0);
  kernel_add_large_clause(k, b, 3, false, 0);
}

static void test_occurrence_lifetime() {
  Kernel k;
  occurrence_fixture(k);
  OccIndex index;
  {
    OccurrenceSession owner(k, {index, OccurrenceAttachment::Build, true});
    assert(k.dense == &index && index.track_order);
    BveOccurrenceCache cache;
    { BveOccurrenceCache::Scope borrower(cache, k); }
    assert(k.dense == &index && k.large_watches_connected);
    index.large.relocations = 7; index.large.defrags = 3;
    owner.close(); owner.close();
    assert(!k.dense && k.stats.dense_slice_moves == 7 && k.stats.dense_defrags == 3);
  }
  {
    OccurrenceSession retained(k, {index, OccurrenceAttachment::Retained});
    assert(k.dense == &index && index.track_order);
  }
  assert(!k.dense && k.stats.dense_slice_moves == 7 && k.stats.dense_defrags == 3);
  BveOccurrenceCache cache;
  OccIndex *first;
  { BveOccurrenceCache::Scope attached(cache, k); first = k.dense; assert(first); }
  assert(!k.dense);
  { BveOccurrenceCache::Scope attached(cache, k); assert(k.dense == first); }
  // An intervening formula edit invalidates the cached image. Observe the
  // rebuilt contents rather than requiring a different allocator address.
  const Lit c[] = {internalize(-2), internalize(-3), internalize(-4)};
  kernel_add_large_clause(k, c, 3, false, 0);
  {
    BveOccurrenceCache::Scope attached(cache, k);
    assert(k.dense && k.dense->large.size(internalize(-2)) == 1);
  }
  assert(!k.dense && k.large_watches_connected);
  kernel_check_invariants(k);
}

static void test_candidate_publication(RootPublication roots, bool retirements) {
  Kernel native, shared;
  ScopedHeuristics nh(native), sh(shared);
  for (Kernel *k : {&native, &shared}) {
    kernel_reserve_vars(*k, 4);
    k->h->stable = true;
    k->bve_gate_mask = 0;
    kernel_add_binary_clause(*k, internalize(1), internalize(2));
    kernel_add_binary_clause(*k, internalize(-1), internalize(2));
  }
  const auto &bve = tactix_rules()[tactix_rule_id("BVE")];
  const uint64_t pivot = lit_instance(native, internalize(1));
  const size_t begin = native.trail.size();
  const auto expected = apply_rule_instance(bve, native, pivot);
  assert(expected == ApplyResult::Ok && native.stats.bve_eliminated == 1);
  assert(native.stats.root_assignments == 1);
  heuristics_consume_rule_advice(native);
  if (roots == RootPublication::NewAssignments)
    heuristics_consume_root_assignments(native, begin);
  if (retirements) heuristics_consume_retirements(native);
  struct Publisher : HeuristicRulePublisher {
    std::string order;
    void advice(Kernel &k) { order += 'a'; HeuristicRulePublisher::advice(k); }
    void roots(Kernel &k, size_t begin) {
      order += 'u'; HeuristicRulePublisher::roots(k, begin);
    }
    void retirements(Kernel &k) { order += 'r'; HeuristicRulePublisher::retirements(k); }
  } publisher;
  const auto actual = execute_action(shared, RuleApplicationRequest{roots, retirements},
      [&] { return apply_rule_instance(bve, shared, pivot); }, publisher);
  assert(actual.result == expected && actual.trail_before == begin);
  assert(actual.roots_before == 0 && actual.eliminated_before == 0);
  const std::string order = std::string("a") +
      (roots == RootPublication::NewAssignments ? "u" : "") + (retirements ? "r" : "");
  assert(publisher.order == order);
  assert(state(native) == state(shared));
  assert(native.h->scores.stack == shared.h->scores.stack);
  assert(native.h->scores.pos == shared.h->scores.pos);
  assert(native.retired_events == shared.retired_events);
  // Deferred retirement remains observable until its chosen native boundary.
  assert(shared.retired_events.empty() == retirements);
  kernel_check_invariants(shared);
}

int main() {
  test_parameter_lifetime();
  test_walk_budget_precedence();
  for (bool probing : {false, true})
    for (bool terminal : {false, true})
      test_root_preparation(probing, terminal);
  test_occurrence_lifetime();
  for (auto roots : {RootPublication::Deferred, RootPublication::NewAssignments})
    for (bool retirements : {false, true})
      test_candidate_publication(roots, retirements);
}
