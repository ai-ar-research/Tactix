// Fixed API control for diagnostic selection; no solver semantics changed.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "rules/registry.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <unistd.h>
using namespace tactix;
static uint64_t zero_meter(const Kernel &) { return 0; }
static void fixed_unit() {
  Kernel k; Heuristics h; k.h=&h; kernel_reserve_vars(k,2);
  kernel_add_binary_clause(k,internalize(1),internalize(2));
  kernel_add_binary_clause(k,internalize(-1),internalize(2));
  k.stats.conflicts=1; k.bve_gate_mask=0; k.bve_backward=false;
  DefaultStrategyConfig cfg; cfg.inprocess.phase_rounds=1;
  DefaultPolicy policy(cfg); DefaultInprocessScheduler::SlotEntry entry;
  entry.rule=tactix_rule_id("BVE"); entry.group=SlotGroup::Eliminate;
  entry.enabled=true; entry.dosed=true; entry.meter=zero_meter;
  entry.order=CandidateOrder::Heuristic;
  policy.inprocess().entries.push_back(entry);
  std::fill(k.bve_retry.begin(),k.bve_retry.end(),uint8_t{0});k.bve_retry[0]=1;
  assert(policy.inprocess().run_phase(k,SlotGroup::Eliminate,nullptr)!=20);
  assert(k.stats.bve_eliminated==1 && k.value(internalize(2))>0);
  kernel_check_invariants(k);
}
int main() {
  char path[]="bve-full-heap-XXXXXX";const int fd=mkstemp(path);assert(fd>=0);close(fd);
  setenv("TACTIX_BVE_FULL_HEAP_TRACE",path,1);
  setenv("SAT_BVE_FULL_HEAP_CONFLICT","2",1);
  setenv("SAT_BVE_FULL_HEAP_EXTERNAL","1",1);
  auto read=[&] {std::ifstream in(path);return std::string(std::istreambuf_iterator<char>(in),{});};
  fixed_unit();assert(read().empty());
  setenv("SAT_BVE_FULL_HEAP_CONFLICT","1",1);fixed_unit();
  const std::string snapshot=read();
#ifdef TACTIX_ENABLE_EVENT_TRACE
  assert(snapshot.find("# bve-full-heap\t1\n# begin\t0\tinitial\t1\t0\t1\n")==0);
  assert(snapshot.find("# begin\t1\tbefore-pop\t1\t1\t1\n")!=std::string::npos);
  assert(std::count(snapshot.begin(),snapshot.end(),'\n')==7);
#else
  assert(snapshot.empty());
#endif
  fixed_unit();assert(read()==snapshot);
  unsetenv("TACTIX_BVE_FULL_HEAP_TRACE");unsetenv("SAT_BVE_FULL_HEAP_CONFLICT");unsetenv("SAT_BVE_FULL_HEAP_EXTERNAL");
  assert(!std::remove(path));
  std::puts("PASS: full-heap conflict selection, initial/pre-pop contents, no duplicate snapshots, API invariants");
}
