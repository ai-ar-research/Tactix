// Observe the actual scheduler entry through its existing candidate-order API.
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include <algorithm>
#include <cstdio>
using namespace tactix;
struct ObserveEntry : CandidateOrderPolicy {
  bool called = false, connected = false;
  unsigned large_words = 0;
  const char *name() const override { return "observe-entry"; }
  void rank(const Kernel &k, uint32_t, uint64_t *, uint32_t) override {
    if (called) return;
    called = true; connected = k.large_watches_connected;
    for (Lit l = 0; l < 2 * k.nvars; ++l)
      large_words += k.watches.size(l) - k.watches.binary_size(l);
  }
};
static void watch_words(const Kernel &k, unsigned *large, unsigned *binary) {
  *large = *binary = 0;
  for (Lit l = 0; l < 2 * k.nvars; ++l) {
    *binary += k.watches.binary_size(l);
    *large += k.watches.size(l) - k.watches.binary_size(l);
  }
}
int main() {
  Kernel k; ScopedHeuristics heuristics(k);
  kernel_reserve_vars(k, 4);
  const Lit lits[] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, lits, 3, false, 0, 1);
  kernel_add_binary_clause(k, internalize(-1), internalize(4), 2);
  unsigned large_before, binary_before;
  watch_words(k, &large_before, &binary_before);
  // Eliminate is a search-time phase (BVE defers preprocessing, kissat
  // fastel=0); slot admission declines non-preprocess entries at 0 conflicts.
  k.stats.conflicts = 1;
  // The retry advice is consulted only after ranking: zero keeps every
  // candidate out of the dose, so exit restoration is observable.
  std::fill(k.bve_retry.begin(), k.bve_retry.end(), uint8_t{0});
  DefaultPolicy p; ObserveEntry observer;
  p.inprocess().pose_initialized();
  p.inprocess().candidate_order = &observer;
  DefaultInprocessScheduler::SlotEntry e;
  e.rule = tactix_rule_id("BVE"); e.group = SlotGroup::Eliminate;
  e.dosed = true; e.order = CandidateOrder::Reverse; e.max_applies = 1;
  e.meter = [](const Kernel &) -> uint64_t { return 0; };
  p.inprocess().entries.push_back(e);
  const int result = p.inprocess().run_phase(k, SlotGroup::Eliminate, nullptr);
  const DefaultInprocessScheduler::SlotEntry &ran = p.inprocess().entries[0];
  unsigned large_after, binary_after;
  watch_words(k, &large_after, &binary_after);
  const bool reached = ran.applies == 1 && ran.tm.admitted == 1 && ran.ord.lists == 1;
  const bool restored = k.large_watches_connected && !k.dense &&
                        large_after == large_before && binary_after == binary_before;
  std::printf("reached=%u called=%u entry_connected=%u large_words=%u eliminated=%llu "
              "exit_connected=%u large=%u/%u binary=%u/%u result=%d\n",
              reached, observer.called, observer.connected, observer.large_words,
              (unsigned long long)k.stats.bve_eliminated, k.large_watches_connected,
              large_before, large_after, binary_before, binary_after, result);
  return reached && observer.called && !observer.connected && !observer.large_words &&
         !k.stats.bve_eliminated && restored && result != 20 ? 0 : 1;
}
