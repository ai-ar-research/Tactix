#pragma once
// The pigeonhole fixture and search-only DefaultPolicy profile of the
// policy-feedback tests.
#include "kernel/kernel.hpp"
#include "policies/default_strategy_config.hpp"

#include <vector>

// PHP(5, 4): small and UNSAT, so a short search records decisions, conflict
// analysis and search intervals. cold defers the kernel's cold state.
inline tactix::Kernel pigeonhole(bool cold = false) {
  using namespace tactix;
  Kernel k;
  if (cold) kernel_defer_cold_state(k);
  constexpr unsigned pigeons = 5, holes = 4;
  kernel_reserve_vars(k, pigeons * holes);
  auto cell = [](unsigned p, unsigned h) { return make_lit(p * holes + h, false); };
  for (unsigned p = 0; p < pigeons; ++p) {
    std::vector<Lit> clause;
    for (unsigned h = 0; h < holes; ++h) clause.push_back(cell(p, h));
    kernel_add_large_clause(k, clause.data(), holes, false, 0, ++k.next_clause_id);
    for (unsigned q = p + 1; q < pigeons; ++q)
      for (unsigned h = 0; h < holes; ++h)
        kernel_add_binary_clause(k, negate(cell(p, h)), negate(cell(q, h)), ++k.next_clause_id);
  }
  return k;
}

// No inprocessing; rephase, mode switch, restart and reduction every few
// conflicts, so a short search exercises each feedback kind.
inline tactix::DefaultStrategyConfig search_profile() {
  tactix::DefaultStrategyConfig config;
  config.inprocess.program.clear();
  config.inprocess.append_unlisted_rules = false;
  config.inprocess.structured_root_stages.clear();
  config.inprocess.structured_root_bounded_backtracking_stages.clear();
  config.inprocess.structured_root_large_domain_stages.clear();
  config.inprocess.initial_fast_elimination = false;
  config.rephase.late_lucky = false;
  config.rephase.stable_only = false;
  config.rephase.initial_interval = config.rephase.interval = 1;
  config.rephase.rotation = {tactix::DefaultRephaseSlot::Walk};
  config.rephase.walk_permille = 1;
  config.mode.initial_conflicts = config.mode.interval = 1;
  config.restart.reluctant_period = 2;
  config.reduction.initial_interval = 5;
  return config;
}

