// Actual scheduler entry: retained roots precede every simplifier slot.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "rules/propagate/propagate.hpp"
#include <cassert>
#include <cstdio>

using namespace tactix;

static void entry(SlotGroup group) {
  Kernel k;
  kernel_reserve_vars(k, 4);
  ScopedHeuristics h(k);
  k.h->stable = true;
  const Lit lits[] = {internalize(-1), internalize(3), internalize(4)};
  kernel_add_large_clause(k, lits, 3, false, 0);
  kernel_assign_decision(k, internalize(-1));
  kernel_assign_unit(k, internalize(-3));
  kernel_assign_unit(k, internalize(-4));
  assert(propagate_apply(k) == ApplyResult::Ok);
  assert(kernel_propagation_fixpoint(k) && k.assigned[0].level == 1);
  k.h->saved[0] = 1;
  k.stats.conflicts = 1;
  k.stats.restarts = 90;
  const auto props = k.stats.propagations, ticks = k.stats.search_ticks;
  DefaultPolicy p;
  p.inprocess().pose_initialized();
  // An empty pipeline prevents a rule from repairing the fixture: test the
  // scheduler lifecycle with an empty legal pipeline.
  assert(p.inprocess().entries.empty());
  assert(p.inprocess().run_phase(k, group, nullptr) == 0);
  assert(k.value(internalize(-1)) > 0 && k.assigned[0].level == 0);
  assert(kernel_propagation_fixpoint(k) && k.unflushed_root == 0);
  assert(k.stats.propagations > props);
  assert((k.stats.search_ticks > ticks) == (group == SlotGroup::Eliminate));
  assert(k.h->saved[0] == (group == SlotGroup::Probe ? 1 : -1));
  assert(k.stats.restarts == 90 && !k.dense && k.large_watches_connected);
  assert(!k.inprocessing && !k.phase_saving_suppressed);
  std::printf("%s entry: root value %d, propagated %llu, search ticks %llu\n",
      group == SlotGroup::Probe ? "probe" : "eliminate",
      k.value(internalize(-1)),
      static_cast<unsigned long long>(k.stats.propagations - props),
      static_cast<unsigned long long>(k.stats.search_ticks - ticks));
}

int main() {
  entry(SlotGroup::Probe);
  entry(SlotGroup::Eliminate);
  for (const auto group : {SlotGroup::Probe, SlotGroup::Eliminate}) {
    Kernel k;
    kernel_reserve_vars(k, 2);
    ScopedHeuristics h(k);
    kernel_add_binary_clause(k, internalize(-1), internalize(2));
    kernel_assign_unit(k, internalize(1));
    kernel_assign_unit(k, internalize(-2));
    DefaultPolicy p;
    p.inprocess().pose_initialized();
    assert(p.inprocess().run_phase(k, group, nullptr) == 20);
    assert(k.inconsistent && !k.conflict_valid && !k.dense);
    assert(!k.inprocessing && !k.phase_saving_suppressed);
  }
}
