// Fixed actual-scheduler witness: partial C2 completion is not dense closure.
#include "forward-fixture.hpp"
#include "policies/heuristics.hpp"

static bool observed = false;
static int pending_value = 0;
static uint64_t forward_steps(const Kernel &k) {
  return k.stats.strengthen_visits;
}
static uint64_t observe_next_slot(const Kernel &k) {
  if (!observed && k.stats.strengthened) {
    assert(k.dense && k.dense->track_order && !k.large_watches_connected);
    assert(k.value(internalize(3)) > 0);
    assert(k.stats.strengthen_checked == 1 && !k.strengthen_complete);
    observed = true;
    pending_value = k.value(internalize(5));
    std::printf("next-slot: unit5=%d head=%u trail=%zu fixpoint=%u\n",
                pending_value, k.propagate_head, k.trail.size(),
                kernel_propagation_fixpoint(k));
  }
  return 0;
}

int main() {
  Fixture f;
  f.seed();
  f.add({-3, 4, 5});
  f.add({-4});
  f.k.stats.conflicts = 1; // Search-time slots, not preprocessing follow-ups.
  ScopedHeuristics h(f.k);
  DefaultPolicy policy;
  auto &scheduler = policy.inprocess();
  scheduler.pose_initialized();
  DefaultInprocessScheduler::SlotEntry forward;
  forward.rule = tactix_rule_id("Strengthen");
  forward.group = SlotGroup::Eliminate;
  forward.meter = forward_steps;
  forward.budget_aware = true;
  forward.permille = 0;
  scheduler.entries.push_back(forward);
  // An existing public slot supplies a read-only meter observation before its
  // apply. No solver implementation or private registry is replaced.
  DefaultInprocessScheduler::SlotEntry next;
  next.rule = tactix_rule_id("SubsumeForward");
  next.group = SlotGroup::Eliminate;
  next.meter = observe_next_slot;
  scheduler.entries.push_back(next);
  assert(scheduler.run_phase(f.k, SlotGroup::Eliminate, nullptr) == 0);
  assert(observed && pending_value == 0);
  assert(f.k.value(internalize(5)) > 0);
  assert(kernel_propagation_fixpoint(f.k) && f.k.large_watches_connected);
  kernel_check_invariants(f.k);
  assert(f.finish(false) == 10);
  std::puts("PASS: actual next slot sees unclosed unit;phase exit restores closure and valid SAT model");
}
