// SymmetryUse: observation-only provenance of symmetry-breaking output. A
// tiny symmetric formula publishes one SR breaking binary; search then uses it
// as a propagation reason and as a 1UIP antecedent, a tagged unit forces a
// root implication, and a tagged large clause is retired.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_strategy_config.hpp"
#include "rules/analyze/analyze.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/substitution_redundancy/substitution_redundancy.hpp"

#include <cassert>
#include <cstdio>

using namespace tactix;

static void add_binary(Kernel &k, int a, int b) {
  kernel_add_binary_clause(k, internalize(a), internalize(b),
                           ++k.next_clause_id);
}

// F = {(-1 3), (-2 -3), (-4 5)} is invariant under 1<->2, 3<->-3. The cut
// (-1 2) is SR with that substitution as witness.
static SubstitutionRedundancyApplicationStatus
publish_cut(Kernel &k, SubstitutionRedundancyProducer producer) {
  SubstitutionRedundancyInstance instance;
  instance.clause = {internalize(-1), internalize(2)};
  instance.substitution = {{internalize(1), internalize(2)},
                           {internalize(2), internalize(1)},
                           {internalize(3), internalize(-3)}};
  SubstitutionRedundancyParameters parameters;
  parameters.producer = producer;
  assert(parameters.valid());
  return substitution_redundancy_apply_detailed(k, instance, parameters)
      .status;
}

static void fixture(Kernel &k, bool record = true) {
  k.symuse.record_symmetry = record;
  kernel_reserve_vars(k, 6); // 6 is a free variable for an outer decision
  add_binary(k, -1, 3);
  add_binary(k, -2, -3);
  add_binary(k, -4, 5);
}

// deep: decide the free 6 first, so the conflict takes the ordinary 1UIP
// path; otherwise it is a level-one failed literal.
static void test_search_uses_breaking_binary(bool deep) {
  Kernel k;
  fixture(k);
  assert(!k.symuse_active);
  assert(publish_cut(k, SubstitutionRedundancyProducer::GraphSymmetry) ==
         SubstitutionRedundancyApplicationStatus::Applied);
  assert(k.symuse_active && k.symuse.runs.size() == 1);
  const SymmetryUseRun &run = k.symuse.runs[0];
  assert(run.clauses == 1 && run.binaries == 1 && run.helpers == 0);

  // A tagged root unit 4 forces 5 through the untagged (-4 5).
  const Lit unit = internalize(4);
  k.unit_ids[var_of(unit)] = ++k.next_clause_id;
  kernel_assign_unit(k, unit);
  kernel_symuse_tag(k, &unit, 1, k.unit_ids[var_of(unit)], false);
  assert(propagate_apply(k) != ApplyResult::Unsat);
  assert(k.values[internalize(5)] > 0 && k.var_level(var_of(internalize(5))) == 0);
  assert(run.root_forced == 1 && run.propagations == 0);

  // Deciding 1 propagates 2 through the tagged cut and 3 through (-1 3); the
  // 1UIP walk resolves the conflict (-2 -3) through both reasons back to 1.
  // The failed-literal walk stops at the first reason naming -1.
  if (deep)
    kernel_assign_decision(k, internalize(6));
  kernel_assign_decision(k, internalize(1));
  propagate_apply(k);
  assert(k.conflict_valid);
  assert(k.assigned[var_of(internalize(2))].binary);
  assert(run.propagations == 1 && run.search_propagations == 1 &&
         run.helper_propagations == 0);
  assert(analyze_apply(k) == ApplyResult::Ok);
  assert(deep ? run.analysis_uses == 1 : run.analysis_uses <= 1);
  // Both paths learn the unit -1 (the 1UIP clause is left pending for
  // Backjump; the failed-literal walk asserts it at the root itself).
  assert(deep ? k.pending.valid && k.pending.lits.size() == 1 &&
                    k.pending.lits[0] == internalize(-1)
              : k.values[internalize(-1)] > 0 && k.level == 0);
  assert(kernel_symuse_alive(k, 0) == 2);
  std::printf("%s search use: %llu propagations, %llu analysis uses, %llu root "
              "forced\n",
              deep ? "1UIP" : "failed-literal",
              (unsigned long long)run.propagations,
              (unsigned long long)run.analysis_uses,
              (unsigned long long)run.root_forced);
}

static void test_explicit_sr_is_untagged() {
  Kernel k;
  fixture(k);
  assert(publish_cut(k, SubstitutionRedundancyProducer::ExplicitOnly) ==
         SubstitutionRedundancyApplicationStatus::Applied);
  assert(!k.symuse_active && k.symuse.runs.empty());
}

// The report is diagnostic and off by default: a symmetry cut is then not
// tagged and no SymmetryUse hook is armed.
static void test_default_off() {
  assert(!DefaultStrategyConfig{}.symmetry_use_observation);
  assert(!Kernel{}.symuse.record_symmetry);
  Kernel k;
  fixture(k, false);
  assert(publish_cut(k, SubstitutionRedundancyProducer::GraphSymmetry) ==
         SubstitutionRedundancyApplicationStatus::Applied);
  assert(!k.symuse_active && k.symuse.runs.empty());
}

static void test_large_retirement_and_compaction() {
  Kernel k;
  kernel_reserve_vars(k, 6);
  const Lit lits[3] = {internalize(1), internalize(2), internalize(3)};
  const uint64_t id = ++k.next_clause_id;
  const Ref ref = kernel_add_large_clause(k, lits, 3, true, 0, id);
  kernel_symuse_tag(k, lits, 3, id, false);
  add_binary(k, -5, 6);
  const Lit pair[2] = {internalize(-5), internalize(6)};
  kernel_symuse_tag(k, pair, 2, k.next_clause_id, true);
  const SymmetryUseRun &run = k.symuse.runs[0];
  assert(run.large == 1 && run.helpers == 1 && kernel_symuse_alive(k, 0) == 2);

  kernel_delete_clause(k, clause_at(k.arena, ref));
  assert(run.large_deleted == 1 && kernel_symuse_alive(k, 0) == 1);

  // The tagged helper keeps its identity after variable compaction.
  k.var_status.assign(k.nvars, VarStatus::Active);
  k.var_status[3] = VarStatus::Eliminated;
  --k.unassigned_active;
  assert(kernel_compact_variables(k));
  assert(k.nvars == 5 && kernel_symuse_alive(k, 0) == 1);
  kernel_assign_decision(k, kernel_internalize(k, 5));
  assert(propagate_apply(k) != ApplyResult::Unsat);
  assert(run.propagations == 1 && run.helper_propagations == 1);
  kernel_backtrack(k, 0);
  assert(kernel_symuse_alive(k, 0) == 1);
}

int main() {
  test_search_uses_breaking_binary(true);
  test_search_uses_breaking_binary(false);
  test_explicit_sr_is_untagged();
  test_default_off();
  test_large_retirement_and_compaction();
  std::puts("symmetry use: tagged cut propagates, enters analysis, forces "
            "root units, survives compaction and retires");
}
