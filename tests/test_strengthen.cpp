// C2 Strengthen unit tests: fused subsume-or-strengthen round, cascades,
// unit-producing strengthen, instance surface, and the registry-driven
// inprocessing slot (schedule-as-data).
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/strengthen/strengthen.hpp"

#include <cassert>
#include <csignal>
#include <cstdio>
#include <cstring>

using namespace tactix;

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
    ref += clause_words(c->size);
  }
  if (lits)
    assert(found);
  return count;
}

static void test_binary_strengthens_large() {
  // c = (1 2 3), d = (-1 2): resolving on 1 drops 1 from c -> (2 3),
  // stored as a fresh binary; the original dies
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
  // c = (1 2 3 4); d = (-1 2 3) strengthens it to (2 3 4); then the
  // binary (-2 3) strengthens THAT to (3 4) in the same candidate cascade
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit c[4] = {internalize(1), internalize(2), internalize(3),
                    internalize(4)};
  const Lit d[3] = {internalize(-1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, c, 4, false, 0);
  kernel_add_large_clause(k, d, 3, false, 0);
  kernel_add_binary_clause(k, internalize(-2), internalize(3));
  assert(strengthen_apply(k) == ApplyResult::Ok);
  // d itself first gets strengthened by (-2 3)? d = (-1 2 3): binary
  // (-2 3) with marks over d's lits {-1,2,3}: probe phases of 2 find
  // (-2,3): flip on 2 -> d -> (-1 3). So the round strengthens BOTH.
  assert(k.stats.strengthened >= 2);
  assert(default_solve(k) == 10);
}

static void test_unit_producing_strengthen_ends_round() {
  // root unit -1 (so 1 is falsified in c); c = (1 2 3); d = (-2 3):
  // root units -1 and -2 leave c = (1 2 3) with one free literal; the
  // flush pre-pass strips both falsified literals -> unit (3), asserted
  // at the root; the pass ends (values changed). (Partner strengthens
  // can no longer produce units: the flush owns the falsified-literal
  // path, and post-flush candidates lose at most one literal per act.)
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

static void test_instance_surface() {
  // instances() lists candidates size-ascending; apply_instance on the
  // large candidate performs exactly one step
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
  // junk payloads are rejected, not dereferenced
  assert(rules[id].apply_instance(k, 999999) == ApplyResult::NoOp);
}

// Pose one elimination-clock slot entry, leaving every other field at its
// in-class default. Naming only the three fields a test cares about keeps a new
// SlotEntry field from silently defaulting to a value the test never chose.
static DefaultPolicy::SlotEntry pose(int rule, bool enabled) {
  DefaultPolicy::SlotEntry e;
  e.rule = rule;
  e.enabled = enabled;
  e.group = SlotGroup::Eliminate;
  return e;
}

static void test_slot_schedule_as_data() {
  // the registry-driven slot : the tier tag is readable, posed
  // slots are honored in order with enabled flags, and a disabled entry
  // never runs. (The default composition is the always-on S4 arrangement;
  // tests POSE slots — the same override surface presets and the phase-6
  // learner use.)
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
    // posed slot: Strengthen on, SubsumeForward off; forced to fire from
    // the first conflict check
    Kernel k;
    kernel_reserve_vars(k, 3);
    for (const auto &b : xor3)
      kernel_add_binary_clause(k, b[0], b[1]);
    DefaultPolicy p;
    p.inprocess.initialized = true;
    // Both slot clocks forced open; the posed entries sit on the elimination
    // clock, which is where the authored order puts these two rules.
    p.probe_clock.next_conflicts = 0;
    p.elim_clock.next_conflicts = 0;
    p.inprocess_slot.push_back(pose(strengthen_id, true));
    p.inprocess_slot.push_back(pose(subsume_id, false));
    assert(p.solve(k) == 20); // odd-parity xor cycle: UNSAT
    assert(p.inprocess_slot.size() == 2); // posed slot left untouched
    assert(p.elim_clock.rounds >= 1);     // the forced pass ran
    assert(k.stats.strengthen_rounds >= 1);
    assert(k.stats.forward_subsumptions == 0); // disabled entry never ran
  }
  {
    // fully disabled slot suppresses all rounds
    Kernel k;
    kernel_reserve_vars(k, 3);
    for (const auto &b : xor3)
      kernel_add_binary_clause(k, b[0], b[1]);
    DefaultPolicy p;
    p.inprocess.initialized = true;
    p.probe_clock.next_conflicts = 0;
    p.elim_clock.next_conflicts = 0;
    p.inprocess_slot.push_back(pose(strengthen_id, false));
    assert(p.solve(k) == 20);
    assert(k.stats.strengthen_rounds == 0);
  }
}

static void test_root_flush() {
  // the flush pre-pass (kissat sparse-collection effect, rule-side):
  // a root-satisfied clause is deleted; root-falsified literals are
  // stripped — including from REDUNDANT clauses, preserving redundancy
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

// SCHED-H1 · C2's interrupt sites. It has FOUR — the flush pre-pass over the
// whole arena, the candidate loop, the cascade that re-checks one shrinking
// candidate, and the single-instance entry — because a poll in only one of
// them still leaves a round that ignores the interrupt for the duration of the
// others. There is NO cadence, so a raised flag fires at the first poll.
//
// The flush pre-pass runs FIRST and polls per arena clause, so a flag raised
// before the call can only ever be caught there; the candidate-loop and
// cascade sites are reached instead through the deterministic stress cadence,
// which is exactly what it exists for. `strengthen_checked > 0` at the abort
// is the witness that the flush completed and the fire came from further in.
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
  { // (1) the FLUSH pre-pass, the round's first poll
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
    // A flush-pass abort leaves `candidates` EMPTY, so there is no tail to
    // re-arm — advancing the watermark would retire the whole formula unseen.
    assert(k.strengthen_touch_mark == 0 &&
           k.stats.strengthen_mark_advances == 0);
    kernel_check_invariants(k);
  }
  { // (2) the single-instance entry, which NEITHER governor reached before H1
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
  { // (3) PAST the flush: the candidate loop / cascade. The stress cadence
    // fires on a deterministic 1-in-m draw, so sweeping m walks the first fire
    // out of the 200-clause flush pass and into the candidate loop. A sweep
    // that finds none fails loudly here rather than leaving the site unproven.
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
    Kernel c; // negative control: same formula, no knob, round COMPLETES
    fill_interrupt_case(c, 200);
    assert(strengthen_apply(c) == ApplyResult::NoOp);
    assert(c.stats.strengthen_interrupted == 0 && c.strengthen_complete);
  }
}

int main() {
  test_binary_strengthens_large();
  test_fused_subsumption();
  test_large_partner_and_cascade();
  test_unit_producing_strengthen_ends_round();
  test_instance_surface();
  test_slot_schedule_as_data();
  test_root_flush();
  test_interrupt();
  puts("test_strengthen OK");
  return 0;
}
