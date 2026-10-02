#undef NDEBUG
#include "policies/heuristics.hpp"
#include "rules/propagate/propagate.hpp"
#include "strategy/guidance_execution.hpp"
#include "strategy/reduction_execution.hpp"
#include "strategy/root_execution.hpp"

#include <cassert>
#include <sstream>

using namespace tactix;
using namespace tactix::execution;

static void fixture(Kernel &k) {
  kernel_reserve_vars(k, 4);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(-2));
  kernel_add_binary_clause(k, internalize(-2), internalize(3));
  k.work_budget = 71; k.work_budget_offset = 13;
  k.work_budget_bounded = true; k.midround_budget = true;
}

static Lit pick(Kernel &k) {
  for (Var v = 0; v < k.nvars; ++v)
    if (!k.values[make_lit(v, false)] && !kernel_var_retired(k, v))
      return make_lit(v, false);
  assert(false);
  return INVALID_LIT;
}

static std::string state(const Kernel &k) {
  std::ostringstream out;
  out << k.level << ':' << k.propagate_head << ':' << k.binary_only_head << ':'
      << k.conflict_valid << ':' << k.inconsistent << ':' << k.warming << ':'
      << k.unassigned_active << ':' << k.work_budget << ':' << k.work_budget_offset
      << ':' << k.work_budget_bounded << ':' << k.midround_budget;
  for (auto value : k.values) out << ":v" << int(value);
  for (Lit lit : k.trail) out << ":t" << lit;
  for (auto phase : k.h->saved) out << ":p" << int(phase);
  for (auto v : k.h->scores.stack) out << ":h" << v;
  const auto &s = k.stats;
  out << ':' << s.decisions << ':' << s.conflicts << ':' << s.propagations << ':'
      << s.root_assignments << ':' << s.search_ticks << ':' << s.ticks << ':'
      << s.warmups << ':' << s.warming_decisions << ':' << s.warming_conflicts
      << ':' << s.warming_propagations;
  return out.str();
}

static void test_warmup(bool stable, int interrupt_after) {
  Kernel native, shared;
  ScopedHeuristics nh(native), sh(shared);
  fixture(native); fixture(shared);
  native.h->stable = shared.h->stable = stable;
  volatile sig_atomic_t ns = interrupt_after == 0, ss = interrupt_after == 0;
  KernelStopPublisher np(native, &ns), sp(shared, &ss);
  std::vector<Lit> nd, sd, ni, si;
  native.warming = true;
  ++native.stats.warmups;
  while (native.unassigned_active) {
    if (kernel_terminating(native)) break;
    const Lit decision = pick(native);
    kernel_assign_decision(native, decision);
    ++native.stats.warming_decisions;
    nd.push_back(decision);
    if (int(nd.size()) == interrupt_after) ns = 1;
    const size_t begin = native.trail.size();
    kernel_propagate_beyond_conflicts(native);
    ni.insert(ni.end(), native.trail.begin() + begin, native.trail.end());
  }
  if (native.level) kernel_backtrack(native, 0);
  native.warming = false;
  const auto result = execute_action(shared, WarmupRequest{}, [&] { return pick(shared); },
      [&](Lit decision) {
        sd.push_back(decision);
        if (int(sd.size()) == interrupt_after) ss = 1;
      }, [&](size_t begin) {
        si.insert(si.end(), shared.trail.begin() + begin, shared.trail.end());
      });
  assert(nd == sd && ni == si && state(native) == state(shared));
  assert(result.interrupted == (interrupt_after >= 0));
  assert(result.decisions == shared.stats.warming_decisions);
  assert(result.propagations == shared.stats.warming_propagations);
  assert(result.conflicts == shared.stats.warming_conflicts);
  assert(result.ticks == shared.stats.ticks);
  assert(!shared.level && !shared.warming && !shared.conflict_valid);
  assert(shared.stats.decisions == 0 && shared.stats.conflicts == 0);
  assert(shared.stats.search_ticks == 0 && shared.stats.root_assignments == 0);
  if (interrupt_after != 0) {
    assert(shared.stats.warming_conflicts && shared.stats.warming_propagations);
    assert(shared.h->saved[0] == 1 && shared.h->saved[1] == 1 && shared.h->saved[2] == 1);
  }
  kernel_check_invariants(shared);
}

static void test_walk_handoff() {
  for (uint64_t allowance : {0ull, 1ull, 17ull}) {
    Kernel native, shared;
    ScopedHeuristics nh(native), sh(shared);
    fixture(native); fixture(shared);
    native.work_budget = allowance;
    native.walk_random_seed = 97;
    const auto expected = walk_apply(native);
    native.work_budget = 0;
    const auto actual = execute_action(shared, WalkRequest{allowance, 97});
    assert(actual == expected && state(native) == state(shared));
    assert(shared.work_budget == 0 && shared.work_budget_offset == 13);
    assert(shared.work_budget_bounded && shared.midround_budget && shared.walk_random_seed == 97);
    assert(native.h->saved == shared.h->saved && native.h->target == shared.h->target);
    kernel_check_invariants(shared);
  }
}

static void test_sparse_resume(bool charge_search, bool terminal) {
  Kernel native, shared;
  ScopedHeuristics nh(native), sh(shared);
  for (Kernel *k : {&native, &shared}) {
    kernel_reserve_vars(*k, 3);
    kernel_add_binary_clause(*k, internalize(-1), internalize(2));
    if (terminal)
      kernel_add_binary_clause(*k, internalize(-1), internalize(-2));
    kernel_assign_unit(*k, internalize(1));
    k->inprocessing = true;
  }
  native.propagate_head = 0;
  native.binary_only_head = UINT32_MAX;
  if (charge_search) native.inprocessing = false;
  const auto expected = propagate_apply(native);
  native.inprocessing = true;
  const auto actual = execute_action(shared, SparseResumeRequest{0, charge_search},
      [&] {
        assert(shared.inprocessing == !charge_search);
        return propagate_apply(shared);
      });
  assert(expected == actual && state(native) == state(shared));
  assert(actual == (terminal ? ApplyResult::Unsat : ApplyResult::Ok));
  assert(shared.inprocessing && shared.stats.ticks);
  assert((shared.stats.search_ticks != 0) == charge_search);
  kernel_check_invariants(shared);
}

static void test_reduction_preparation(bool terminal) {
  Kernel native, shared;
  ScopedHeuristics nh(native), sh(shared);
  for (Kernel *k : {&native, &shared}) {
    kernel_reserve_vars(*k, 4);
    kernel_add_binary_clause(*k, internalize(-1), internalize(2));
    if (terminal)
      kernel_add_binary_clause(*k, internalize(-2), internalize(-1));
    kernel_assign_unit(*k, internalize(1));
    kernel_assign_decision(*k, internalize(-3));
    kernel_assign_decision(*k, internalize(4));
  }
  const auto roots = native.stats.root_assignments;
  reduce_prepare(native);
  const auto added = native.stats.root_assignments - roots;
  assert(added == 1);
  const auto begin = native.trail.size() - added;
  heuristics_consume_root_assignments(native, begin);
  unsigned publications = 0;
  execute_action(shared, ReductionPreparationRequest{}, [&](size_t offset) {
    ++publications;
    assert(offset == begin && offset < 3);
    assert(!shared.level && shared.inconsistent == terminal);
    heuristics_consume_root_assignments(shared, offset);
  });
  assert(publications == 1 && state(native) == state(shared));
  assert(!shared.level && shared.stats.restarts == 0);
  assert(shared.inconsistent == terminal);
  kernel_check_invariants(shared);
}

static void test_reduction_collection(bool compact) {
  Kernel native, shared;
  ScopedHeuristics nh(native), sh(shared);
  Ref victim = INVALID_REF;
  for (Kernel *k : {&native, &shared}) {
    kernel_reserve_vars(*k, 6);
    const Lit prefix[] = {internalize(1), internalize(2), internalize(3)};
    const Lit redundant[] = {internalize(4), internalize(5), internalize(6)};
    kernel_add_large_clause(*k, prefix, 3, false, 0);
    const Ref next = kernel_add_large_clause(*k, redundant, 3, true, 9);
    if (victim != INVALID_REF) assert(victim == next);
    victim = next;
    kernel_assign_unit(*k, internalize(1));
    assert(propagate_apply(*k) == ApplyResult::Ok);
  }
  const auto expected = reduce_apply_refs(native, &victim, 1, compact);
  const auto actual = execute_action(shared, ReductionRequest{{&victim, 1}, compact});
  assert(expected == actual && actual == ApplyResult::Ok);
  assert(state(native) == state(shared) && native.arena.size() == shared.arena.size());
  assert(shared.arena.empty() == compact);
  assert(shared.stats.reductions == 1);
  assert(shared.stats.sparse_collected == native.stats.sparse_collected);
  if (compact) assert(shared.stats.sparse_collected == 1);
  kernel_check_invariants(shared);
}

int main() {
  for (bool stable : {false, true})
    for (int interrupt_after : {-1, 0, 1})
      test_warmup(stable, interrupt_after);
  test_walk_handoff();
  for (bool terminal : {false, true}) {
    test_reduction_preparation(terminal);
    for (bool search : {false, true}) test_sparse_resume(search, terminal);
  }
  for (bool compact : {false, true}) test_reduction_collection(compact);
}
