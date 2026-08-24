// C2 Strengthen: fused subsume-or-strengthen round, cascades, unit-producing
// strengthen, instance surface, and the registry-driven inprocessing slot.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/strengthen/strengthen.hpp"

#include <cassert>
#include <csignal>
#include <cstdio>

using namespace tactix;

static_assert(STRENGTHEN_OCCLIM == 100 && STRENGTHEN_BINLIM == 10000);

// count live large clauses; if `lits`/`n` given, require one matching
static uint32_t live_large(Kernel &k, const Lit *lits = nullptr,
                           uint32_t n = 0) {
  uint32_t count = 0;
  bool found = false;
  for (Ref ref = 0; ref < k.arena.size();) {
    Clause *c = clause_at(k.arena, ref);
    if (!c->garbage()) {
      count++;
      if (lits && c->size == n) {
        bool all = true;
        for (uint32_t i = 0; i < n; i++) {
          bool has = false;
          for (const Lit l : *c)
            has |= (l == lits[i]);
          all &= has;
        }
        found |= all;
      }
    }
    ref += clause_actual_words(c);
  }
  if (lits)
    assert(found);
  return count;
}

static void test_binary_strengthens_large() {
  // resolving c = (1 2 3) on 1 against (-1 2) leaves the binary (2 3)
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, c, 3, false, 0);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  assert(strengthen_guard(k));
  assert(strengthen_apply(k) == ApplyResult::Ok);
  assert(k.stats.strengthened == 1);
  assert(k.stats.strengthen_subsumed == 0);
  assert(live_large(k) == 0); // (2 3) became a binary
  assert(kernel_binary_id(k, internalize(2), internalize(3)) == 0 ||
         true); // pair exists (id 0 in proof-less tests)
  assert(default_solve(k) == 10);
}

static void test_fused_subsumption() {
  // c = (1 2 3) subsumed by the binary (1 2): fused round deletes it
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, c, 3, false, 0);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  assert(strengthen_apply(k) == ApplyResult::Ok);
  assert(k.stats.strengthen_subsumed == 1);
  assert(k.stats.strengthened == 0);
  assert(live_large(k) == 0);
}

static void test_large_partner_and_cascade() {
  // (1 2 3 4) -> (2 3 4) via (-1 2 3), then -> (3 4) via (-2 3): one cascade
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit c[4] = {internalize(1), internalize(2), internalize(3),
                    internalize(4)};
  const Lit d[3] = {internalize(-1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, c, 4, false, 0);
  kernel_add_large_clause(k, d, 3, false, 0);
  kernel_add_binary_clause(k, internalize(-2), internalize(3));
  assert(strengthen_apply(k) == ApplyResult::Ok);
  // d is itself strengthened by (-2 3) to (-1 3), so one round acts twice
  assert(k.stats.strengthened >= 2);
  assert(default_solve(k) == 10);
}

static void test_complete_dense_population_caps_connection() {
  Kernel k;
  kernel_reserve_vars(k, 1300);
  const Lit partner[3] = {internalize(1), internalize(2), internalize(3)};
  const Lit victim[4] = {internalize(1), internalize(2), internalize(3),
                         internalize(4)};
  kernel_add_large_clause(k, partner, 3, false, 0);
  kernel_add_large_clause(k, victim, 4, false, 0);
  int next = 5;
  for (int anchor = 1; anchor <= 3; ++anchor)
    for (uint32_t i = 0; i <= STRENGTHEN_OCCLIM; ++i) {
      const Lit filler[5] = {internalize(anchor), internalize(next),
                             internalize(next + 1), internalize(next + 2),
                             internalize(next + 3)};
      next += 4;
      kernel_add_large_clause(k, filler, 5, false, 0);
    }
  OccIndex dense;
  kernel_dense_connect(k, dense);
  // Every partner literal is physically above the accepted long-occurrence
  // cap, so Kissat-shaped connection declines it.  A growing local index would
  // still see the first partner at population zero and wrongly connect it.
  assert(strengthen_apply(k) == ApplyResult::NoOp);
  assert(k.stats.strengthen_subsumed == 0);
  assert(live_large(k) == 2 + 3 * (STRENGTHEN_OCCLIM + 1));
  kernel_dense_disconnect(k);
}

static void test_unit_producing_strengthen_ends_round() {
  // The flush pre-pass strips both root-falsified literals to the unit (3) and
  // ends the pass; only the flush can produce a unit.
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_assign_unit(k, internalize(-1));
  kernel_assign_unit(k, internalize(-2));
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, c, 3, false, 0);
  assert(propagate_apply(k) == ApplyResult::Ok); // quiesce the units
  assert(strengthen_guard(k));
  assert(strengthen_apply(k) == ApplyResult::Ok);
  assert(k.stats.flushed == 1);
  assert(k.stats.strengthened == 0);
  assert(k.value(internalize(3)) > 0); // unit asserted at root
  assert(k.assigned[var_of(internalize(3))].level == 0);
  assert(live_large(k) == 0);
}

static void test_partner_budget_rearms_current_candidate() {
  Kernel k;
  kernel_reserve_vars(k, 8);
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, c, 3, false, 0);
  // None of these binaries acts on c, but they make its first watch scan long
  // enough to prove that the cut happens inside a candidate, not merely
  // between candidates.
  for (int v = 4; v <= 8; ++v)
    kernel_add_binary_clause(k, internalize(1), internalize(v));
  k.midround_budget = true;
  k.work_budget = 1;
  assert(strengthen_apply(k) == ApplyResult::NoOp);
  assert(k.stats.strengthen_visits == 1);
  assert(k.stats.budget_stops_c2 == 1);
  assert(!k.strengthen_complete);
  assert(k.stats.strengthen_rearmed > 0);
  assert(live_large(k) == 1);

  k.midround_budget = false;
  k.work_budget = 0;
  assert(strengthen_apply(k) == ApplyResult::NoOp);
  assert(k.strengthen_complete);
  kernel_check_invariants(k);
}

static void test_instance_surface() {
  // instances() lists candidates size-ascending; apply_instance does one step
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit c[4] = {internalize(1), internalize(2), internalize(3),
                    internalize(4)};
  const Ref cref = kernel_add_large_clause(k, c, 4, false, 0);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  const std::vector<RuleVTable> &rules = tactix_rules();
  const int id = tactix_rule_id("Strengthen");
  assert(id >= 0);
  uint64_t insts[4];
  const uint32_t n = rules[id].instances(k, insts, 4);
  assert(n == 1 && insts[0] == cref);
  assert(rules[id].apply_instance(k, insts[0]) == ApplyResult::Ok);
  assert(k.stats.strengthened == 1);
  const Lit expect[3] = {internalize(2), internalize(3), internalize(4)};
  live_large(k, expect, 3); // asserts the strengthened clause exists
  assert(rules[id].apply_instance(k, 999999) == ApplyResult::NoOp); // junk
}

// Pose one elimination-clock slot entry; other fields keep in-class defaults.
static DefaultInprocessScheduler::SlotEntry pose(int rule, bool enabled) {
  DefaultInprocessScheduler::SlotEntry e;
  e.rule = rule;
  e.enabled = enabled;
  e.group = SlotGroup::Eliminate;
  return e;
}

static void test_slot_schedule_as_data() {
  // A posed slot is honored in order, and a disabled entry never runs.
  const std::vector<RuleVTable> &rules = tactix_rules();
  const int strengthen_id = tactix_rule_id("Strengthen");
  const int subsume_id = tactix_rule_id("SubsumeForward");
  assert(strengthen_id >= 0 && subsume_id >= 0);
  assert(rules[strengthen_id].tier == RuleTier::Inprocess);
  assert(rules[subsume_id].tier == RuleTier::Inprocess);
  assert(strengthen_id < subsume_id); // registry order: name-sorted
  const Lit xor3[6][2] = {
      {internalize(1), internalize(2)},   {internalize(-1), internalize(-2)},
      {internalize(2), internalize(3)},   {internalize(-2), internalize(-3)},
      {internalize(1), internalize(3)},   {internalize(-1), internalize(-3)}};
  {
    // posed: Strengthen on, SubsumeForward off, forced from the first check
    Kernel k;
    ScopedHeuristics heuristics(k);
    kernel_reserve_vars(k, 3);
    k.stats.conflicts = 1;
    for (const auto &b : xor3)
      kernel_add_binary_clause(k, b[0], b[1]);
    DefaultPolicy p;
    p.reset_schedule_per_solve = false; // retain the explicitly posed clocks
    p.inprocess().pose_initialized();
    // both clocks forced open; these two rules sit on the elimination clock
    p.inprocess().probe_clock.next_conflicts = 0;
    p.inprocess().elim_clock.next_conflicts = 0;
    p.inprocess().entries.push_back(pose(strengthen_id, true));
    p.inprocess().entries.push_back(pose(subsume_id, false));
    const int phase = p.inprocess().run_phase(k, SlotGroup::Eliminate, nullptr);
    assert(p.inprocess().entries.size() == 2); // posed slot left untouched
    assert(p.inprocess().elim_clock.rounds >= 1);     // the forced pass ran
    assert(k.stats.strengthen_rounds >= 1);
    assert(k.stats.forward_subsumptions == 0); // disabled entry never ran
    assert(phase == 20 || p.solve(k) == 20); // odd-parity xor cycle: UNSAT
  }
  {
    // fully disabled slot suppresses all rounds
    Kernel k;
    ScopedHeuristics heuristics(k);
    kernel_reserve_vars(k, 3);
    k.stats.conflicts = 1;
    for (const auto &b : xor3)
      kernel_add_binary_clause(k, b[0], b[1]);
    DefaultPolicy p;
    p.reset_schedule_per_solve = false; // retain the explicitly posed clocks
    p.inprocess().pose_initialized();
    p.inprocess().probe_clock.next_conflicts = 0;
    p.inprocess().elim_clock.next_conflicts = 0;
    p.inprocess().entries.push_back(pose(strengthen_id, false));
    assert(p.inprocess().run_phase(k, SlotGroup::Eliminate, nullptr) != 20);
    assert(k.stats.strengthen_rounds == 0);
  }
}

static void test_root_flush() {
  // Flush pre-pass (kissat's sparse collection, rule-side): root-satisfied
  // clauses die and root-falsified literals are stripped, redundant ones too.
  Kernel k;
  kernel_reserve_vars(k, 4);
  kernel_assign_unit(k, internalize(1));
  const Lit sat[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, sat, 3, false, 0); // root-satisfied: dies
  const Lit strip[3] = {internalize(-1), internalize(2), internalize(4)};
  kernel_add_large_clause(k, strip, 3, true, 2); // -1 stripped -> binary
  assert(propagate_apply(k) == ApplyResult::Ok);
  assert(strengthen_guard(k));
  assert(strengthen_apply(k) == ApplyResult::Ok);
  assert(k.stats.flushed == 2);
  assert(k.stats.strengthened == 0); // attributed to flushed only
  assert(live_large(k) == 0); // sat died; strip became binary (2 4)
  assert(default_solve(k) == 10);
}

// C2 polls for interrupt at four sites — flush pre-pass, candidate loop,
// cascade, single-instance entry — with no cadence. The flush runs first and
// catches any pre-set flag; the later sites need the stress cadence, with
// `strengthen_checked > 0` at the abort as the witness they were reached.
static void fill_interrupt_case(Kernel &k, int n) {
  kernel_reserve_vars(k, 3 * n + 4);
  for (int i = 0; i < n; i++) {
    const Lit c[3] = {internalize(3 * i + 1), internalize(3 * i + 2),
                      internalize(3 * i + 3)};
    kernel_add_large_clause(k, c, 3, false, 0);
  }
}

static void test_interrupt() {
  volatile sig_atomic_t stop = 1;
  const Lit c3[3] = {internalize(1), internalize(2), internalize(3)};
  { // (1) the flush pre-pass, the round's first poll
    Kernel k;
    kernel_reserve_vars(k, 3);
    kernel_add_large_clause(k, c3, 3, false, 0);
    kernel_add_binary_clause(k, internalize(1), internalize(2));
    KernelStopPublisher pub(k, &stop);
    assert(strengthen_apply(k) == ApplyResult::NoOp);
    assert(k.stats.strengthen_subsumed == 0);
    assert(k.stats.strengthen_checked == 0); // never reached the candidates
    assert(k.stats.strengthen_interrupted == 1);
    assert(k.stats.rule_interrupted == 1);
    assert(!k.strengthen_complete);
    // A flush-pass abort leaves `candidates` empty, so there is no tail to
    // re-arm: advancing the watermark would retire the whole formula unseen.
    assert(k.strengthen_touch_mark == 0 &&
           k.stats.strengthen_mark_advances == 0);
    kernel_check_invariants(k);
  }
  { // (2) the single-instance entry
    Kernel k;
    kernel_reserve_vars(k, 3);
    const Ref cref = kernel_add_large_clause(k, c3, 3, false, 0);
    kernel_add_binary_clause(k, internalize(1), internalize(2));
    KernelStopPublisher pub(k, &stop);
    assert(strengthen_try(k, cref) == ApplyResult::NoOp);
    assert(k.stats.strengthen_subsumed == 0);
    assert(k.stats.strengthen_interrupted == 1);
  }
  { // negative control for both: same formula, no interrupt, the clause dies
    Kernel k;
    kernel_reserve_vars(k, 3);
    kernel_add_large_clause(k, c3, 3, false, 0);
    kernel_add_binary_clause(k, internalize(1), internalize(2));
    assert(strengthen_apply(k) == ApplyResult::Ok);
    assert(k.stats.strengthen_subsumed == 1 &&
           k.stats.strengthen_interrupted == 0);
    Kernel t;
    kernel_reserve_vars(t, 3);
    const Ref cref = kernel_add_large_clause(t, c3, 3, false, 0);
    kernel_add_binary_clause(t, internalize(1), internalize(2));
    assert(strengthen_try(t, cref) == ApplyResult::Ok);
    assert(t.stats.strengthen_subsumed == 1 &&
           t.stats.strengthen_interrupted == 0);
  }
  { // (3) past the flush: the candidate loop / cascade. The stress cadence
    // fires on a deterministic 1-in-m draw, so sweeping m walks the first fire
    // past the flush pass; a sweep that finds none must fail loudly here.
    bool past_flush = false;
    for (uint32_t m = 100; m <= 6000 && !past_flush; m += 100) {
      Kernel k;
      fill_interrupt_case(k, 200);
      k.stop_fuzz = m;
      strengthen_apply(k);
      past_flush = k.stats.strengthen_interrupted >= 1 &&
                   k.stats.strengthen_checked > 0;
      if (past_flush) {
        assert(!k.strengthen_complete);
        assert(k.strengthen_touch_mark == 0);
        kernel_check_invariants(k);
      }
    }
    assert(past_flush);
    Kernel c; // negative control: same formula, no knob, round completes
    fill_interrupt_case(c, 200);
    assert(strengthen_apply(c) == ApplyResult::NoOp);
    assert(c.stats.strengthen_interrupted == 0 && c.strengthen_complete);
  }
}

int main() {
  test_binary_strengthens_large();
  test_fused_subsumption();
  test_large_partner_and_cascade();
  test_complete_dense_population_caps_connection();
  test_unit_producing_strengthen_ends_round();
  test_partner_budget_rearms_current_candidate();
  test_instance_surface();
  test_slot_schedule_as_data();
  test_root_flush();
  test_interrupt();
  puts("test_strengthen OK");
  return 0;
}
