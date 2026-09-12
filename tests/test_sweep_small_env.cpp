// Small environments still own the initial solve, model and exact effort.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/sweep/sweep.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace tactix;

static void clause(Kernel &k, const std::vector<Lit> &lits) {
  const uint64_t id = ++k.next_clause_id;
  if (lits.size() == 2)
    kernel_add_binary_clause(k, lits[0], lits[1], id);
  else
    kernel_add_large_clause(k, lits.data(), lits.size(), false, 0, id);
}

static void positive_clause(Kernel &k, uint32_t size) {
  kernel_reserve_vars(k, size + 2);
  std::vector<Lit> lits;
  for (Var v = 0; v < size; v++)
    lits.push_back(make_lit(v, false));
  clause(k, lits);
}

static uint64_t single_clause(uint32_t size, bool old, uint64_t budget = 0) {
  Kernel k;
  positive_clause(k, size);
  SweepLimits lim;
  lim.env_ticks = budget;
  SweepRound round;
  sweep_round_begin(k, round);
  const auto next_id = k.next_clause_id;
  assert(sweep_detect_seed(k, 0, lim, round) == 0);
  assert(round.env.vars.size() == size && round.env.nclauses() == 1);
  assert(k.stats.sweep_environments == (old ? 0u : 1u));
  if (old) {
    assert(k.stats.sweep_solves == 0 && k.stats.sweep_ticks_sub == 0);
    assert(!round.env.solver.has_model());
  } else {
    assert(k.stats.sweep_solves >= 1 && k.stats.sweep_ticks_sub > 0);
    assert(k.stats.sweep_ticks_sub == round.env.solver.kitten_ticks());
    if (!budget) {
      assert(round.env.solver.has_model());
      assert(std::any_of(round.env.solver.model().begin(),
                         round.env.solver.model().begin() + size,
                         [](int8_t value) { return value > 0; }));
    }
  }
  assert(round.found.empty() && round.proofs.empty() && round.units.empty());
  assert(!k.inconsistent && k.trail.empty() && k.next_clause_id == next_id);
  return k.stats.sweep_ticks_sub;
}

static void empty_and_singleton() {
  Kernel k;
  kernel_reserve_vars(k, 1);
  SweepRound round;
  SweepLimits lim;
  sweep_round_begin(k, round);
  assert(sweep_detect_seed(k, 0, lim, round) == 0);
  assert(round.env.vars.size() == 1 && round.env.nclauses() == 0);
  assert(k.stats.sweep_environments == 1 && k.stats.sweep_solves >= 1);
  assert(round.found.empty() && round.units.empty() && !k.inconsistent);
  assert(k.trail.empty());
  // Empty kernel and invalid public identity still decline before detection.
  Kernel empty;
  assert(sweep_try(empty, 0) == ApplyResult::NoOp);
  assert(empty.stats.sweep_solves == 0);
}

static void root_and_retired_still_decline() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  k.unit_ids[0] = ++k.next_clause_id;
  kernel_assign_unit(k, make_lit(0, false));
  k.var_status.resize(k.nvars, VarStatus::Active);
  k.var_status[1] = VarStatus::Eliminated;
  SweepRound round;
  SweepLimits lim;
  sweep_round_begin(k, round);
  assert(sweep_detect_seed(k, 0, lim, round) == 0);
  assert(sweep_detect_seed(k, 1, lim, round) == 0);
  assert(k.stats.sweep_solves == 0 && k.stats.sweep_environments == 0);
}

static void enclosing_budget() {
  Kernel k;
  positive_clause(k, 270);
  clause(k, {make_lit(0, true), make_lit(270, false)});
  // All three seeds have two polarities at entry. The large first clause
  // crosses the variable cap before either negative binary is imported.
  clause(k, {make_lit(1, true), make_lit(271, false)});
  clause(k, {make_lit(2, true), make_lit(270, false)});
  OccIndex occ;
  kernel_dense_connect(k, occ); // Include the third, unwatched large occurrence.
  k.sweep_tail = {0, 1, 2};
  SweepLimits lim;
  lim.round_ticks = 1;
  lim.env_ticks = 0;
  (void)sweep_apply_with_limits(k, lim);
  // Import costs one real Kitten tick; the inclusive inner poll then returns
  // Budget. Equality still admits the second seed under the strict outer
  // limit, but two charged attempts leave the third retained. The old shortcut
  // silently skipped every environment and drained this entire schedule.
  assert(k.stats.sweep_swept == 2 && k.stats.sweep_solves == 2);
  assert(k.stats.sweep_ticks_sub == 2 && k.stats.sweep_budget == 2);
  assert(k.sweep_live == 1 && sweep_next_scheduled(k) == 2);
  assert(!k.inconsistent && k.trail.empty());
  kernel_dense_disconnect(k);
}

static void repeated_session() {
  Kernel k;
  positive_clause(k, 270);
  SweepRound round;
  SweepLimits lim;
  sweep_round_begin(k, round);
  for (uint32_t call = 0; call < 3; call++) {
    const auto before = k.stats.sweep_ticks_sub;
    assert(sweep_detect_seed(k, 0, lim, round) == 0);
    assert(round.env.nclauses() == 1 && round.env.solver.has_model());
    assert(k.stats.sweep_ticks_sub - before == round.env.solver.kitten_ticks());
    assert(k.stats.sweep_environments == call + 1);
    assert(round.found.empty() && round.units.empty());
  }
}

int main(int argc, char **argv) {
  const bool old = argc == 2 && !std::strcmp(argv[1], "--old-counterexample");
  for (uint32_t size : {2u, 3u, 17u, 270u}) {
    const auto ticks = single_clause(size, old);
    std::printf("single clause vars=%u ticks=%llu\n", size,
                (unsigned long long)ticks);
  }
  if (old) {
    std::puts("old counterexample: all one-clause environments skipped");
    return 0;
  }
  for (uint32_t size : {2u, 3u, 17u, 270u})
    (void)single_clause(size, false, 1);
  empty_and_singleton();
  root_and_retired_still_decline();
  enclosing_budget();
  repeated_session();
  std::puts("small-environment solve/model/effort: PASS");
}
