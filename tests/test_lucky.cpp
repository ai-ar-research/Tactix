// Lucky unit tests: constant and sweep probes, abandonment restoring the entry
// state with phases untouched, the lifecycle calls and the registry surface.
#include "unit_test.hpp"
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "policies/default_policy.hpp"
#include "rules/lucky/lucky.hpp"
#include "rules/lucky/model.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/registry.hpp"
#include "rules/restart/restart.hpp"

#include <array>

// These reference-lifecycle controls isolate the two Kissat Lucky calls.
static int solve_reference_startup(Kernel &k, bool late_lucky = true) {
  DefaultStrategyConfig config;
  config.inprocess.structured_root_before_lucky = false;
  config.rephase.late_lucky = late_lucky;
  return DefaultPolicy(config).solve(k);
}

static void add3(Kernel &k, int a, int b, int c) {
  const Lit lits[3] = {internalize(a), internalize(b), internalize(c)};
  kernel_add_large_clause(k, lits, 3, false, 0);
}

// PHP(3,2): Lucky's level-one failed literals refute it.
static void add_php32(Kernel &k) {
  kernel_reserve_vars(k, 6);
  const auto var = [](int p, int h) { return p * 2 + h + 1; };
  for (int p = 0; p < 3; p++)
    kernel_add_binary_clause(k, internalize(var(p, 0)),
                             internalize(var(p, 1)));
  for (int h = 0; h < 2; h++)
    for (int p1 = 0; p1 < 3; p1++)
      for (int p2 = p1 + 1; p2 < 3; p2++)
        kernel_add_binary_clause(k, internalize(-var(p1, h)),
                                 internalize(-var(p2, h)));
}

TEST(test_all_true) {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  add3(k, 1, 2, 3);
  assert(lucky_guard(k));
  assert(lucky_apply_probe(k, LUCKY_ALL_TRUE) == ApplyResult::Sat);
  assert(k.unassigned == 0);
  for (Var v = 0; v < 3; v++)
    assert(k.values[make_lit(v, false)] > 0);
  assert(k.stats.lucky_probes == 1);
}

// An early Lucky verdict (SAT with Lucky first, SAT after an empty
// structured-root program, or a terminal refutation) materializes neither
// deferred rule state nor search guidance, and needs no later Propagate to
// notice the inconsistency.
TEST(test_early_lucky_verdict_keeps_cold_state) {
  for (const unsigned mode : {0u, 1u, 2u}) {
    const bool unsat = mode == 2;
    Kernel k;
    kernel_defer_cold_state(k);
    DefaultStrategyConfig config;
    config.inprocess.structured_root_before_lucky = mode == 1;
    if (unsat) {
      add_php32(k);
      config.onehot_model.enabled = false;
    } else {
      kernel_reserve_vars(k, 4);
      kernel_add_binary_clause(k, internalize(1), internalize(2));
      add3(k, 1, 3, 4);
      if (mode == 1) config.inprocess.structured_root_stages.clear();
    }
    assert(DefaultPolicy(config).solve(k) == (unsat ? 20 : 10));
    assert(k.stats.lucky_early_calls == 1 && k.stats.lucky_late_calls == 0);
    assert(!unsat || (k.inconsistent && k.level == 0));
    assert(k.cold_state_deferred && !k.h && (unsat || !k.saved_phases));
  }
}

TEST(test_cold_state_materialization_preserves_first_round_predicates) {
  Kernel eager, deferred;
  kernel_defer_cold_state(deferred);
  kernel_reserve_vars(eager, 4);
  kernel_reserve_vars(deferred, 4);
  for (Kernel *k : {&eager, &deferred}) {
    kernel_add_binary_clause(*k, internalize(1), internalize(-2));
    add3(*k, -1, 3, 4);
  }
  assert(eager.touch_epoch == deferred.touch_epoch);
  assert(eager.stats.touch_marks == deferred.stats.touch_marks);
  assert(eager.stats.bve_marks == deferred.stats.bve_marks);

  kernel_materialize_cold_state(deferred);
  assert(!deferred.cold_state_deferred);
  assert(deferred.bve_retry.size() == deferred.nvars);
  for (Var v = 0; v < deferred.nvars; v++) {
    assert(kernel_touched_since(eager, v, 0));
    assert(kernel_touched_since(deferred, v, 0));
    assert(!kernel_changed_since(eager, v, eager.touch_epoch));
    assert(!kernel_changed_since(deferred, v, deferred.touch_epoch));
    assert(deferred.bve_retry[v] == eager.bve_retry[v]);
  }
}

TEST(test_all_false) {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(-2));
  add3(k, -1, -2, -3);
  assert(lucky_apply_probe(k, LUCKY_ALL_TRUE) == ApplyResult::Ok);
  assert(k.unassigned == 3); // a defeated probe leaves the state untouched
  assert(lucky_apply_probe(k, LUCKY_ALL_FALSE) == ApplyResult::Sat);
  for (Var v = 0; v < 3; v++)
    assert(k.values[make_lit(v, true)] > 0);
}

TEST(test_sweep) {
  // mixed polarity defeats both constants; the forward-false sweep succeeds
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(1), internalize(2));   // no all-F
  kernel_add_binary_clause(k, internalize(-1), internalize(-2)); // no all-T
  assert(lucky_apply_probe(k, LUCKY_ALL_TRUE) == ApplyResult::Ok);
  assert(lucky_apply_probe(k, LUCKY_ALL_FALSE) == ApplyResult::Ok);
  assert(lucky_apply_probe(k, LUCKY_FORWARD_FALSE) == ApplyResult::Sat);
  assert(k.values[internalize(-1)] > 0); // ¬1 assumed
  assert(k.values[internalize(2)] > 0);  // 2 propagated by (1 2)
}

TEST(test_first_level_conflict_commits_failed_literal) {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 2);
  // Assuming -1 forces both 2 and -2. Lucky must keep the proved root unit 1
  // and continue the sweep, exactly like kissat_analyze at probing level one.
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(1), internalize(-2));
  assert(lucky_apply_probe(k, LUCKY_FORWARD_FALSE) == ApplyResult::Sat);
  assert(k.values[internalize(1)] > 0);
  assert(k.assigned[var_of(internalize(1))].level == 0);
  assert(k.unit_ids[var_of(internalize(1))] != 0);
}

TEST(test_unsat_probes_commit_root_consequences) {
  // Speculation leaves saved phases untouched while committing root units.
  Kernel k;
  ScopedHeuristics _hg_k(k);
  add_php32(k);
  const ApplyResult lucky_result = lucky_apply(k);
  assert(lucky_result == ApplyResult::Unsat);
  assert(k.inconsistent);
  assert(k.level == 0);
  for (Var v = 0; v < 6; v++) {
    assert(k.h->saved[v] == 0);  // probe assignments never touch phases
    assert(k.h->target[v] == 0);
    assert(k.h->best[v] == 0);
  }
  assert(k.stats.lucky_probes <= 6);
}

// A published model is a terminal SAT state, not a BCP-derived trail: a
// partial Restart must rescan its retained prefix.
TEST(test_published_model_rescans_after_partial_restart) {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(1), internalize(-2));
  // -2 was chosen by the private model search, not derived from decision -1.
  lucky_publish_model(k, {0, 0});
  assert(k.trail_tainted && k.level == 2);
  assert(kernel_propagation_fixpoint(k));
  kernel_check_invariants(k);

  assert(restart_apply_to(k, 1) == ApplyResult::Ok);
  assert(k.propagate_head == 0);
  assert(!kernel_propagation_fixpoint(k));
  assert(propagate_apply(k) == ApplyResult::Ok);
  assert(k.value(internalize(-2)) > 0);
  assert(k.assigned[1].level == 1);
  kernel_check_invariants(k);
}

// Retires external 3 through BVE's public instance (no resolvent), leaving
// the pair that refutes -2.
static void build_retired_head_fixture(Kernel &k) {
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(2), internalize(1));
  kernel_add_binary_clause(k, internalize(2), internalize(-1));
  kernel_add_binary_clause(k, internalize(3), internalize(1));
  const RuleVTable &bve = tactix_rules()[tactix_rule_id("BVE")];
  assert(bve.apply_instance(k, lit_instance(internalize(3))) ==
         ApplyResult::Ok);
  assert(kernel_var_retired(k, var_of(internalize(3))));
  assert(!kernel_var_retired(k, var_of(internalize(2))));
}

// Kissat lucky.c:203-208 skips eliminated and inactive identities before
// assuming; probing one would push the next failed literal past level one.
TEST(test_backward_sweep_skips_retired_identity) {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  build_retired_head_fixture(k);
  // External 3 is retired and highest, so a backward sweep reaches it first
  // and external 2 second, at the level where `-2` is refutable.
  assert(lucky_apply_probe(k, LUCKY_BACKWARD_FALSE) == ApplyResult::Sat);
  assert(k.values[internalize(2)] > 0);
  assert(k.assigned[var_of(internalize(2))].level == 0);
  assert(k.unit_ids[var_of(internalize(2))] != 0);
  assert(k.values[internalize(3)] == 0); // never assumed

  // The constant loops filter the same way (kissat lucky.c:324-325).
  Kernel c;
  ScopedHeuristics _hg_c(c);
  build_retired_head_fixture(c);
  assert(lucky_apply_probe(c, LUCKY_ALL_TRUE) == ApplyResult::Sat);
  assert(c.unassigned_active == 0);
  assert(c.values[internalize(3)] == 0);
}

// Kissat lucky.c:29,37: a binary satisfied by an inactive endpoint does not
// veto a constant probe.
TEST(test_constant_scan_skips_inactive_binary_endpoint) {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(-3));
  kernel_assign_unit(k, internalize(-3));
  assert(lucky_apply_probe(k, LUCKY_ALL_TRUE) == ApplyResult::Sat);
  assert(k.values[internalize(1)] > 0 && k.values[internalize(2)] > 0);
  assert(k.unassigned_active == 0);
}

using Ternary = std::array<int, 3>;

// Four clauses refuted unless `guard` takes `releasing_phase`; a sweep that
// assumes the other phase must abandon (kissat lucky.c:307).
static void add_guarded_core(std::vector<Ternary> &out, int guard,
                             bool releasing_phase, int a, int b, int c) {
  const int g = releasing_phase ? guard : -guard;
  out.push_back({g, a, b});
  out.push_back({g, a, -b});
  out.push_back({g, -a, c});
  out.push_back({g, -a, -c});
}

// Sixteen-variable blocks that defeat all six probes; satisfiable by
// releasing both guards.
static std::vector<Ternary> lucky_resistant_formula(uint32_t blocks) {
  std::vector<Ternary> out;
  for (uint32_t block = 0; block < blocks; block++) {
    const int base = static_cast<int>(block * 16) + 1;
    add_guarded_core(out, base + 0, true, base + 1, base + 2, base + 3);
    add_guarded_core(out, base + 4, false, base + 5, base + 6, base + 7);
    add_guarded_core(out, base + 11, true, base + 8, base + 9, base + 10);
    add_guarded_core(out, base + 15, false, base + 12, base + 13, base + 14);
  }
  return out;
}

// Checks the extended model against the input formula.
static bool model_satisfies(Kernel &k, const std::vector<Ternary> &formula) {
  kernel_extend_model(k);
  for (const Ternary &clause : formula) {
    bool satisfied = false;
    for (const int literal : clause) {
      const Var v = static_cast<Var>((literal < 0 ? -literal : literal) - 1);
      const bool positive = k.model_values[make_lit(v, false)] > 0;
      if (positive == (literal > 0)) {
        satisfied = true;
        break;
      }
    }
    if (!satisfied)
      return false;
  }
  return true;
}

// Formula classification has no scheduling consumers in Kissat, so it cannot
// withdraw the search-time Probe pipeline or late Lucky.
static void build_lucky_resistant(Kernel &k,
                                  const std::vector<Ternary> &formula,
                                  uint32_t vars) {
  kernel_reserve_vars(k, vars);
  for (const Ternary &triple : formula)
    add3(k, triple[0], triple[1], triple[2]);
}

static void print_late_lucky_result(const char *mode, const Kernel &k) {
  std::printf("LATE_LUCKY_RESULT mode=%s early=%llu late=%llu decisions=%llu "
              "conflicts=%llu propagations=%llu\n",
              mode,
              static_cast<unsigned long long>(k.stats.lucky_early_calls),
              static_cast<unsigned long long>(k.stats.lucky_late_calls),
              static_cast<unsigned long long>(k.stats.decisions),
              static_cast<unsigned long long>(k.stats.conflicts),
              static_cast<unsigned long long>(k.stats.propagations));
}

static void test_late_call_mode(const char *mode) {
  const uint32_t blocks = 57;
  const uint32_t vars = blocks * 16;
  const uint64_t clauses = uint64_t{blocks} * 16;
  const std::vector<Ternary> formula = lucky_resistant_formula(blocks);
  assert(formula.size() == clauses);

  if (!std::strcmp(mode, "eager")) {
    DefaultPolicy policy;
    const auto &profiled = policy.strategy_config();
    assert(!profiled.inprocess.preprocess_only &&
           profiled.branching.random_sequences && profiled.rephase.late_lucky);

    Kernel k;
    ScopedHeuristics heuristics(k);
    build_lucky_resistant(k, formula, vars);
    assert(solve_reference_startup(k) == 10);
    assert(k.stats.lucky_early_calls == 1 && k.stats.lucky_late_calls == 1);
    assert(model_satisfies(k, formula));
    print_late_lucky_result(mode, k);
    return;
  }

  if (!std::strcmp(mode, "deferred")) {
    // The shipped frontend leaves H unattached until early Lucky fails.  Its
    // parser-retained activation order must reproduce the eager H trajectory.
    Kernel k;
    build_lucky_resistant(k, formula, vars);
    assert(!k.h && !k.saved_phases);
    assert(solve_reference_startup(k) == 10);
    assert(!k.h && !k.saved_phases);
    assert(k.stats.lucky_early_calls == 1 && k.stats.lucky_late_calls == 1);
    assert(model_satisfies(k, formula));
    print_late_lucky_result(mode, k);
    return;
  }

  assert(!std::strcmp(mode, "disabled"));
  // The same fixture with the opportunity declined: without this arm the
  // enabled assertions could hold for a fixture the late call never reaches.
  Kernel k;
  ScopedHeuristics heuristics(k);
  build_lucky_resistant(k, formula, vars);
  assert(solve_reference_startup(k, false) == 10);
  assert(k.stats.lucky_early_calls == 1 && k.stats.lucky_late_calls == 0);
  assert(model_satisfies(k, formula));
  print_late_lucky_result(mode, k);
}

static void add_guarded_core(Kernel &k, int guard, int a, int b, int c) {
  std::vector<Ternary> rows;
  add_guarded_core(rows, guard, true, a, b, c);
  build_lucky_resistant(k, rows, 0);
}

// Roots fixed by early and late Lucky leave the search queue and do not take
// rank positions after compaction.
static void test_lucky_roots_leave_the_queue(unsigned roots, bool reverse,
                                             bool fold) {
  Kernel k;
  ScopedHeuristics owner(k);
  const unsigned n = roots + 1 + 16;
  kernel_reserve_vars(k, n);
  std::vector<Var> order;
  for (Var v = 0; v < n; v++)
    order.push_back(v);
  if (reverse)
    std::reverse(order.begin(), order.end());
  for (Var v : order)
    kernel_record_import(k, v);
  heuristics_seed_activation_order(k, order);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(1), internalize(-2));
  for (unsigned ev = 3; ev <= roots + 1; ev++)
    kernel_add_binary_clause(k, internalize(-1),
                             internalize(static_cast<int>(ev)));
  const int base = roots + 2;
  add_guarded_core(k, base, base + 1, base + 2, base + 3);
  add_guarded_core(k, -(base + 4), base + 5, base + 6, base + 7);
  add_guarded_core(k, base + 11, base + 8, base + 9, base + 10);
  add_guarded_core(k, -(base + 15), base + 12, base + 13, base + 14);
  // Startup runs Lucky before observing the stop; inprocessing (and graph
  // authentication) respect it, so only Lucky fixes these roots.
  volatile sig_atomic_t stop = 1;
  DefaultStrategyConfig config;
  config.inprocess.structured_root_before_lucky = false;
  config.graph_labeling_model.enabled = false;
  assert(DefaultPolicy(config).solve(k, &stop) == 0);
  assert(k.stats.lucky_early_calls == 1 && k.stats.lucky_late_calls == 1);
  assert(k.level == 0 && kernel_root_fixed(k) == roots);
  for (Var v = 0; v < k.nvars; v++)
    if (kernel_root_value(k, make_lit(v, false)))
      assert(!heuristics_queue_contains(k, v));
  assert(k.unassigned_active == 17);
  assert(kernel_compact_variables(k, fold));
  heuristics_compact_random_order_fixed(k);
  unsigned live_rank = 0;
  for (Var v = owner.h.queue_first; v != INVALID_VAR;
       v = owner.h.links[v].next)
    if (!kernel_root_value(k, make_lit(v, false))) {
      ++live_rank;
      assert(owner.h.links[v].stamp == live_rank);
    }
  assert(live_rank == 17);
  kernel_check_invariants(k);
}

TEST(test_registry_surface) {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  const int id = tactix_rule_id("Lucky");
  assert(id >= 0);
  const RuleVTable &r = tactix_rules()[id];
  assert(r.guard(k));
  uint64_t insts[LUCKY_INSTANCE_COUNT];
  const uint32_t count = r.instances(k, insts, LUCKY_INSTANCE_COUNT);
  for (uint64_t index = 0; index < count; ++index)
    assert(!lucky_retired_constructor_instance(insts[index]));
  assert(r.apply_instance(k, LUCKY_NOL_HYPERGRAPH) == ApplyResult::NoOp);
  assert(r.apply_instance(k, 99) == ApplyResult::NoOp); // unknown payload
  assert(r.apply_instance(k, INSTANCE_CANONICAL) == ApplyResult::Sat);
  // the guard reads state shape only; SAT comes from the status predicate
  assert(!r.guard(k)); // no longer quiescent-with-work: all assigned
}

TEST(test_guard_needs_quiescent_root) {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  assert(lucky_guard(k));
  kernel_assign_decision(k, internalize(1));
  assert(!lucky_guard(k)); // not at the root (and unpropagated)
}

TEST(test_sat_constructors_do_not_refute_root_conflicts) {
  const int id = tactix_rule_id("Lucky");
  assert(id >= 0);
  const RuleVTable &r = tactix_rules()[id];
  for (const uint64_t instance : {uint64_t{6}, uint64_t{7}}) {
    Kernel k;
    ScopedHeuristics h(k);
    kernel_reserve_vars(k, 2);
    kernel_add_binary_clause(k, internalize(-1), internalize(2));
    kernel_assign_unit(k, internalize(1));
    kernel_assign_unit(k, internalize(-2));
    assert(lucky_guard(k));
    assert(!kernel_propagation_fixpoint(k));
    assert(r.apply_instance(k, instance) == ApplyResult::NoOp);
    assert(!k.inconsistent && !k.conflict_valid);
  }
}

int main(int argc, char **argv) {
  if (argc == 3 && !std::strcmp(argv[1], "--late-mode")) {
    test_late_call_mode(argv[2]);
    return 0;
  }
  assert(argc == 1);
  run_tests();
  // One root leaves the fold archive empty; several roots exercise folding
  // and stamp renumbering in both import orders.
  test_lucky_roots_leave_the_queue(1, false, false);
  test_lucky_roots_leave_the_queue(5, true, true);
  test_lucky_roots_leave_the_queue(29, false, true);
  test_lucky_roots_leave_the_queue(29, true, false);
  puts("test_lucky OK");
  return 0;
}
