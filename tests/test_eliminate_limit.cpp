// The authored phase's resolution ceiling also bounds optional forward rounds:
// Kissat eliminate.c eliminate_variables compares eliminate_resolutions with
// its limit (strictly greater) before every candidate and before another
// forward-subsumption plus elimination round.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "rules/registry.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace tactix;

// Pivot 5 has two binaries per side: bound 0 admits it after exactly four
// counted resolutions (k.stats.bve_resolutions).
static constexpr uint64_t PIVOT_RESOLUTIONS = 4;
static constexpr int PIVOT[4][2] = {{5, 1}, {5, 2}, {-5, 3}, {-5, 4}};
static constexpr int RESOLVENT[4][2] = {{1, 3}, {1, 4}, {2, 3}, {2, 4}};

// Whether all four pairs have the given liveness.
static bool all_live(const Kernel &k, const int (*pairs)[2], bool alive) {
  for (int i = 0; i < 4; i++)
    if (kernel_binary_live(k, internalize(pairs[i][0]), internalize(pairs[i][1])) != alive)
      return false;
  return true;
}
static bool live(const Kernel &k, const int (*pairs)[2]) { return all_live(k, pairs, true); }
static bool dead(const Kernel &k, const int (*pairs)[2]) { return all_live(k, pairs, false); }

// `allowance` is the ceiling above the meter: 3 strict exhaustion, 4 equality,
// 5 one to spare.
static void check(unsigned allowance, bool forward, bool bve_enabled) {
  Kernel k;
  Heuristics h;
  k.h=&h;
  kernel_reserve_vars(k,5);
  for (const auto &pair : PIVOT)
    kernel_add_binary_clause(k,internalize(pair[0]),internalize(pair[1]));
  k.stats.conflicts=1;
  k.bve_gate_mask=0;
  k.bve_backward=false;
  DefaultStrategyConfig cfg;
  // Above the largest expected round count, so it never ends a phase.
  cfg.inprocess.phase_rounds=4;
  cfg.inprocess.minimum_effort_ticks=1000;
  DefaultPolicy p(cfg);
  auto &scheduler=p.inprocess();
  scheduler.build(k);
  const auto &rules=tactix_rules();
  DefaultInprocessScheduler::SlotEntry *bve=nullptr,*strengthen=nullptr;
  for(auto &entry:scheduler.entries) {
    const char *name=rules[entry.rule].name;
    entry.enabled=false;
    if(!std::strcmp(name,"BVE")) {
      bve=&entry; entry.enabled=bve_enabled; entry.permille=allowance;
    }
    if(!std::strcmp(name,"Strengthen")) {
      strengthen=&entry; entry.enabled=forward;
    }
  }
  assert(bve && strengthen);
  // Only the pivot is admitted; its pure neighbours are popped and skipped first,
  // then rescheduled by its deletion (eliminate.c update_after_removing_variable).
  // Above the ceiling the next pop breaks with four members left; otherwise the
  // four pure eliminations complete at no resolution cost.
  std::fill(k.bve_retry.begin(),k.bve_retry.end(),uint8_t{0});
  k.bve_retry[var_of(internalize(5))]=1;
  assert(scheduler.run_phase(k,SlotGroup::Eliminate,nullptr)==0);
  std::printf("allowance=%u forward=%u enabled=%u resolutions=%llu ceiling=%llu "
              "bve=%llu admitted=%llu strengthen=%llu eliminated=%llu\n",
      allowance,forward,bve_enabled,(unsigned long long)k.stats.bve_resolutions,
      (unsigned long long)bve->limit,(unsigned long long)bve->tm.occasions,
      (unsigned long long)bve->tm.admitted,
      (unsigned long long)strengthen->tm.occasions,
      (unsigned long long)k.stats.bve_eliminated);
  std::fflush(stdout); // keep the line when an assertion below aborts
  const bool exhausted=PIVOT_RESOLUTIONS>allowance;
  if(bve_enabled) {
    assert(k.stats.bve_resolutions==PIVOT_RESOLUTIONS);
    assert(bve->limit==allowance);
    assert(dead(k,PIVOT));
    uint64_t rounds;
    if(exhausted) {
      // The phase ends after round 0's cleanup, as eliminate.c does.
      assert(k.stats.bve_eliminated==1);
      assert(live(k,RESOLVENT));
      rounds=1;
    } else {
      // Round 0 finishes the pure eliminations; round 1 finds the fixpoint.
      assert(k.stats.bve_eliminated==5);
      assert(dead(k,RESOLVENT));
      rounds=2;
    }
    assert(bve->tm.occasions==rounds && bve->tm.admitted==rounds);
    assert(strengthen->tm.occasions==rounds);
  } else {
    assert(k.stats.bve_resolutions==0 && k.stats.bve_eliminated==0);
    assert(bve->limit==0 && live(k,PIVOT));
    assert(bve->tm.occasions==1 && strengthen->tm.occasions==1);
  }
  assert(k.propagate_head==k.trail.size());
  assert(!k.dense && k.large_watches_connected);
  assert(rules[bve->rule].guard(k) && rules[strengthen->rule].guard(k));
  kernel_check_invariants(k);
  // A new phase derives a fresh ceiling from the current meter, even after
  // exhaustion, and finishes the four remaining pure variables.
  const uint64_t occasions=bve->tm.occasions;
  assert(scheduler.run_phase(k,SlotGroup::Eliminate,nullptr)==0);
  std::printf("  fresh phase: ceiling=%llu bve=%llu strengthen=%llu "
              "resolutions=%llu eliminated=%llu\n",
      (unsigned long long)bve->limit,(unsigned long long)bve->tm.occasions,
      (unsigned long long)strengthen->tm.occasions,
      (unsigned long long)k.stats.bve_resolutions,
      (unsigned long long)k.stats.bve_eliminated);
  std::fflush(stdout);
  const uint64_t fresh_rounds=bve_enabled && exhausted?2:1;
  assert(bve->tm.occasions-occasions==fresh_rounds);
  assert(strengthen->tm.occasions-occasions==fresh_rounds);
  if(bve_enabled) {
    assert(bve->limit==PIVOT_RESOLUTIONS+allowance);
    assert(bve->tm.admitted==occasions+fresh_rounds);
    assert(k.stats.bve_resolutions==PIVOT_RESOLUTIONS);
    assert(k.stats.bve_eliminated==5 && dead(k,RESOLVENT));
  } else {
    assert(bve->limit==0 && k.stats.bve_eliminated==0 && live(k,PIVOT));
  }
  assert(k.propagate_head==k.trail.size());
  assert(!k.dense && k.large_watches_connected);
  kernel_check_invariants(k);
}

int main() {
  for(unsigned allowance:{3u,4u,5u})
    for(bool forward:{false,true})
      for(bool enabled:{false,true}) check(allowance,forward,enabled);
  std::puts("eliminate-limit: 12 fixed phase scenarios passed");
}
