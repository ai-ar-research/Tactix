// Lucky unit tests: constant probes, sweep probes, abandonment restoring the
// entry state with phases untouched, and the registry surface.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "policies/default_policy.hpp"
#include "rules/lucky/lucky.hpp"
#include "rules/registry.hpp"

#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace tactix;

// These reference-lifecycle controls isolate the two Kissat Lucky calls.
static int solve_reference_startup(Kernel &k) {
  DefaultStrategyConfig config;
  config.inprocess.structured_root_before_lucky = false;
  return DefaultPolicy(config).solve(k);
}

static void add3(Kernel &k, int a, int b, int c) {
  const Lit lits[3] = {internalize(a), internalize(b), internalize(c)};
  kernel_add_large_clause(k, lits, 3, false, 0);
}

static void test_all_true() {
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

static void test_default_early_lucky_keeps_cold_state_deferred() {
  for (bool empty_program : {false, true}) {
  Kernel k;
  kernel_defer_cold_state(k);
  kernel_reserve_vars(k, 4);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  add3(k, 1, 3, 4);

  assert(k.cold_state_deferred);
  assert(k.bve_retry.empty() && k.backbone_priority.empty());
  assert(k.touched.empty() && k.removed.empty() && k.touched_kept.empty());
  assert(k.seen.empty() && k.eager_marks.empty() &&
         k.marked_levels.empty() && k.probing.stamp.empty());
  assert(!k.h && !k.saved_phases);
  DefaultStrategyConfig config;
  config.inprocess.structured_root_before_lucky = empty_program;
  if (empty_program) config.inprocess.structured_root_stages.clear();
  assert(DefaultPolicy(config).solve(k) == 10);
  assert(k.stats.lucky_early_calls == 1);
  assert(k.cold_state_deferred &&
         "an early Lucky verdict must not materialize later rule state");
  assert(!k.h && !k.saved_phases &&
         "an early Lucky verdict must not materialize search guidance");
  }
}

static void test_cold_state_materialization_preserves_first_round_predicates() {
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
  assert(deferred.backbone_priority.size() == deferred.nvars);
  assert(deferred.touched.size() == deferred.nvars);
  assert(deferred.removed.size() == deferred.nvars);
  assert(deferred.touched_kept.size() == deferred.nvars);
  assert(deferred.seen.size() == deferred.nvars);
  assert(deferred.eager_marks.size() == 2 * deferred.nvars);
  assert(deferred.marked_levels.size() == deferred.nvars + 1);
  assert(deferred.probing.stamp.size() == 2 * deferred.nvars);
  for (Var v = 0; v < deferred.nvars; v++) {
    assert(kernel_touched_since(eager, v, 0));
    assert(kernel_touched_since(deferred, v, 0));
    assert(!kernel_changed_since(eager, v, eager.touch_epoch));
    assert(!kernel_changed_since(deferred, v, deferred.touch_epoch));
    assert(deferred.bve_retry[v] == eager.bve_retry[v]);
  }
}

static void test_all_false() {
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

static void test_sweep() {
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

static void test_first_level_conflict_commits_failed_literal() {
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

static void test_unsat_probes_commit_root_consequences() {
  // Lucky's level-one failed literals can collectively refute this UNSAT
  // pigeonhole formula. Speculation still leaves saved phases untouched.
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 6);
  auto var = [](int p, int h) { return p * 2 + h + 1; };
  for (int p = 0; p < 3; p++)
    kernel_add_binary_clause(k, internalize(var(p, 0)),
                             internalize(var(p, 1)));
  for (int h = 0; h < 2; h++)
    for (int p1 = 0; p1 < 3; p1++)
      for (int p2 = p1 + 1; p2 < 3; p2++)
        kernel_add_binary_clause(k, internalize(-var(p1, h)),
                                 internalize(-var(p2, h)));
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

  Kernel off;
  ScopedHeuristics _hg_off(off);
  kernel_reserve_vars(off, 6);
  for (int p = 0; p < 3; p++)
    kernel_add_binary_clause(off, internalize(var(p, 0)),
                             internalize(var(p, 1)));
  for (int h = 0; h < 2; h++)
    for (int p1 = 0; p1 < 3; p1++)
      for (int p2 = p1 + 1; p2 < 3; p2++)
        kernel_add_binary_clause(off, internalize(-var(p1, h)),
                                 internalize(-var(p2, h)));
  assert(setenv("TACTIX_SLOT_DISABLE",
                "Congruence,Substitute,Backbone,Sweep,Factor", 1) == 0);
  assert(setenv("TACTIX_LUCKY_LATE", "0", 1) == 0);
  assert(solve_reference_startup(off) == 20);
  assert(unsetenv("TACTIX_LUCKY_LATE") == 0);
  assert(unsetenv("TACTIX_SLOT_DISABLE") == 0);
  assert(off.stats.lucky_early_calls == 1);
  assert(off.stats.lucky_late_calls == 0 && off.stats.lucky_late_sat == 0);
}

// Retire external 3 through BVE's own public instance so the identity stays
// mapped: its sole occurrence is positive, so elimination adds no resolvent.
// Externals 1 and 2 are left with the pair that refutes `-2`.
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

// A retired identity is not a probe opportunity (kissat lucky.c:203-208 skips
// `import.eliminated` and `!ACTIVE` before assuming anything). Assuming one
// spends a probe level on a variable the formula no longer mentions, which
// pushes the next genuine failed literal past the first probe level, where the
// sweep flips it instead of committing it. Retired identities stay mapped
// whenever compaction is declined, so the INVALID_VAR holes compaction leaves
// cannot stand in for the check.
static void test_backward_sweep_skips_retired_identity() {
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

// Both endpoints of a binary polarity scan must be active (kissat lucky.c:29
// and :37): a binary an inactive endpoint already satisfies no longer forces
// that polarity on the active formula. Here `-3` is fixed at the root, so
// `(-1 -3)` is satisfied and must not veto the all-true constant.
static void test_constant_scan_skips_inactive_binary_endpoint() {
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

// Four clauses unsatisfiable unless `guard` takes `releasing_phase`, whose
// refutation needs a decision on `a` rather than propagation: a sweep that
// assumes the guard the other way conflicts on both phases of `a` above its
// first probe level and must abandon (kissat lucky.c:307).
static void add_guarded_core(std::vector<Ternary> &out, int guard,
                             bool releasing_phase, int a, int b, int c) {
  const int g = releasing_phase ? guard : -guard;
  out.push_back({g, a, b});
  out.push_back({g, a, -b});
  out.push_back({g, -a, c});
  out.push_back({g, -a, -c});
}

// Blocks of sixteen variables that defeat all six probes: an all-negative and
// an all-positive clause kill the constants, a guard-before-core pair abandons
// both forward sweeps, and its mirror image abandons both backward ones.
// Satisfiable for every block by releasing both guards.
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

// Every original clause satisfied by the extended model, checked against the
// input rather than the preprocessed kernel copy: a newly reached late Lucky
// runs after BVE and Substitute have pushed reconstruction obligations.
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
static void test_late_call_is_formula_shape_invariant() {
  const uint32_t blocks = 57;
  const uint32_t vars = blocks * 16;
  const uint64_t clauses = uint64_t{blocks} * 16;

  DefaultPolicy policy;
  const auto &profiled = policy.strategy_config();
  assert(!profiled.inprocess.preprocess_only &&
         profiled.branching.random_sequences && profiled.rephase.late_lucky);

  const std::vector<Ternary> formula = lucky_resistant_formula(blocks);
  assert(formula.size() == clauses);
  const auto build = [&](Kernel &k) {
    kernel_reserve_vars(k, vars);
    for (const Ternary &triple : formula) {
      const Lit lits[3] = {internalize(triple[0]), internalize(triple[1]),
                           internalize(triple[2])};
      kernel_add_large_clause(k, lits, 3, false, 0);
    }
  };

  Kernel on;
  ScopedHeuristics _hg_on(on);
  build(on);
  assert(solve_reference_startup(on) == 10);
  assert(on.stats.lucky_early_calls == 1 && on.stats.lucky_late_calls == 1);
  assert(model_satisfies(on, formula));

  // The shipped frontend leaves H unattached until early Lucky fails.  Its
  // parser-retained activation order must reproduce the eager H trajectory.
  Kernel deferred;
  build(deferred);
  assert(!deferred.h && !deferred.saved_phases);
  assert(solve_reference_startup(deferred) == 10);
  assert(!deferred.h && !deferred.saved_phases);
  assert(deferred.stats.lucky_early_calls == on.stats.lucky_early_calls);
  assert(deferred.stats.lucky_late_calls == on.stats.lucky_late_calls);
  assert(deferred.stats.decisions == on.stats.decisions);
  assert(deferred.stats.conflicts == on.stats.conflicts);
  assert(deferred.stats.propagations == on.stats.propagations);
  assert(model_satisfies(deferred, formula));

  // The same fixture with the opportunity declined: without this arm the
  // assertion above could hold for a fixture the late call never reaches.
  Kernel off;
  ScopedHeuristics _hg_off(off);
  build(off);
  assert(setenv("TACTIX_LUCKY_LATE", "0", 1) == 0);
  const int off_result = solve_reference_startup(off);
  assert(unsetenv("TACTIX_LUCKY_LATE") == 0);
  assert(off_result == 10);
  assert(off.stats.lucky_early_calls == 1 && off.stats.lucky_late_calls == 0);
  assert(model_satisfies(off, formula));
}

static void test_registry_surface() {
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
  assert(count == LUCKY_ACTIVE_INSTANCE_COUNT);
  assert(insts[0] == LUCKY_ALL_TRUE);
  for (uint64_t instance = 0; instance < LUCKY_INSTANCE_COUNT; ++instance) {
    assert(insts[instance] == instance);
    assert(!lucky_retired_constructor_instance(instance));
  }
  assert(!lucky_retired_constructor_instance(INSTANCE_CANONICAL));
  assert(lucky_retired_constructor_instance(99));
  for (uint64_t instance = 11; instance <= 14; ++instance)
    assert(std::find(insts, insts + count, instance) != insts + count);
  assert(r.apply_instance(k, 17) == ApplyResult::NoOp);
  assert(k.stats.exact_cover_model_calls == 1);
  assert(r.apply_instance(k, 18) == ApplyResult::NoOp);
  assert(k.stats.independent_set_model_calls == 1);
  assert(r.apply_instance(k, LUCKY_GRAPH_LABELING) == ApplyResult::NoOp);
  assert(k.stats.graph_labeling_calls == 1);
  assert(r.apply_instance(k, 99) == ApplyResult::NoOp); // unknown payload
  assert(r.apply_instance(k, INSTANCE_CANONICAL) == ApplyResult::Sat);
  // the guard reads state shape only; SAT comes from the status predicate
  assert(!r.guard(k)); // no longer quiescent-with-work: all assigned
}

static void test_guard_needs_quiescent_root() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  assert(lucky_guard(k));
  kernel_assign_decision(k, internalize(1));
  assert(!lucky_guard(k)); // not at the root (and unpropagated)
}

static void test_sat_constructors_do_not_refute_root_conflicts() {
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

int main() {
  test_all_true();
  test_default_early_lucky_keeps_cold_state_deferred();
  test_cold_state_materialization_preserves_first_round_predicates();
  test_all_false();
  test_sweep();
  test_first_level_conflict_commits_failed_literal();
  test_unsat_probes_commit_root_consequences();
  test_backward_sweep_skips_retired_identity();
  test_constant_scan_skips_inactive_binary_endpoint();
  test_late_call_is_formula_shape_invariant();
  test_registry_surface();
  test_guard_needs_quiescent_root();
  test_sat_constructors_do_not_refute_root_conflicts();
  puts("test_lucky OK");
  return 0;
}
