// Fixed policy fixture: a productive disjoint Sweep followed by two SCC rounds.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "rules/registry.hpp"
#include "rules/substitute/substitute.hpp"
#include "rules/sweep/sweep.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace tactix;

static void build(Kernel &k) {
  kernel_reserve_vars(k, 16);
  for (Var v = 0; v < k.nvars; ++v)
    kernel_record_import(k, v);
  auto binary = [&](int a, int b) {
    kernel_add_binary_clause(k, internalize(a), internalize(b));
  };
  binary(1, -2);
  binary(-2, 3); binary(2, -3);
  const Lit large[] = {internalize(-1),internalize(2),internalize(3)};
  kernel_add_large_clause(k,large,3,false,0);
  binary(-5,6); binary(5,-6);
  for (int v = 7; v < 16; v += 2)
    binary(v,v+1);
  k.sweep_tail = {k.internal_to_external[4]};
  k.sweep_tail_next = 0;
}

int main() {
  assert(setenv("TACTIX_SWEEP_SEEDS","1",1) == 0);
  Kernel expected, actual;
  build(expected); build(actual);
  ScopedHeuristics eh(expected), ah(actual);
  std::fill(expected.h->scores.score.begin(),expected.h->scores.score.end(),0);
  std::fill(actual.h->scores.score.begin(),actual.h->scores.score.end(),0);
  heuristics_update_scores(expected);
  heuristics_update_scores(actual);
  OccIndex occ;
  kernel_dense_connect(expected,occ);
  assert(sweep_apply(expected) == ApplyResult::Ok);
  assert(expected.retired_events.empty());
  assert(substitute_apply_complete(expected) == ApplyResult::Ok);
  std::printf("canonical retirement:");
  for (Var v : expected.retired_events)
    std::printf(" %d",externalize(make_lit(v,false)));
  std::puts("");
  assert(expected.retired_events == std::vector<Var>({2,5,1}));
  heuristics_consume_retirements(expected);
  kernel_dense_disconnect(expected);

  DefaultPolicy p;
  p.inprocess().build(actual);
  const auto &rules = tactix_rules();
  bool seen_sweep = false;
  for (auto &e : p.inprocess().entries) {
    const char *name = rules[e.rule].name;
    const bool sweep = !std::strcmp(name,"Sweep");
    const bool follow = seen_sweep && !std::strcmp(name,"Substitute");
    e.enabled = sweep || follow;
    if (sweep) seen_sweep = true;
    if (follow) seen_sweep = false;
  }
  assert(p.inprocess().run_phase(actual,SlotGroup::Probe,nullptr) == 0);
  assert(actual.stats.sweep_known > 0);
  assert(actual.stats.substitute_vars == expected.stats.substitute_vars);
  std::printf("heap expected:");
  for (Var v : expected.h->scores.stack) std::printf(" %u",v+1);
  std::printf("\nheap actual:");
  for (Var v : actual.h->scores.stack) std::printf(" %u",v+1);
  std::puts("");
  const bool same = actual.h->scores.stack == expected.h->scores.stack;
  std::printf("per-round policy heap: %s\n",same ? "matches" : "DIFFERS");
  assert(unsetenv("TACTIX_SWEEP_SEEDS") == 0);
  return same ? 0 : 1;
}
