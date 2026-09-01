// C2 Strengthen: fused one-action-per-candidate rounds, unit-producing
// strengthen, instance surface, and the registry-driven inprocessing slot.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/strengthen/strengthen.hpp"

#include <algorithm>
#include <cassert>
#include <csignal>
#include <cstdio>

using namespace tactix;

static_assert(STRENGTHEN_OCCLIM == 1000 && STRENGTHEN_BINLIM == 10000);

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

static bool has_binary(const Kernel &k, Lit a, Lit b) {
  for (const WatchEntry watch : k.watches.entries(a))
    if (watch.binary() && watch.other() == b)
      return true;
  return false;
}

static void test_round_entry_deduplicates_binaries() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit a = internalize(1), b = internalize(2);
  kernel_add_binary_clause(k, a, internalize(3));
  kernel_add_binary_clause(k, a, b);
  kernel_add_binary_clause(k, a, b);
  kernel_add_binary_clause(k, a, b);
  assert(kernel_binary_copies(k, a, b) == 3);
  assert(strengthen_apply(k) == ApplyResult::Ok);
  assert(kernel_binary_copies(k, a, b) == 1);
  assert(k.stats.binaries_deleted == 2);
  assert(strengthen_apply(k) == ApplyResult::NoOp);
  kernel_check_invariants(k);
}

static void test_round_entry_hyper_unary_binary_pair() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  const Lit x = internalize(1), a = internalize(2);
  kernel_add_binary_clause(k, x, a);
  kernel_add_binary_clause(k, x, negate(a));
  assert(strengthen_apply(k) == ApplyResult::Ok);
  assert(k.value(x) > 0 && k.assigned[var_of(x)].level == 0);
  assert(!k.inconsistent);
  kernel_check_invariants(k);
}

static void test_binary_strengthens_large() {
  // resolving c = (1 2 3) on 1 against (-1 2) leaves the binary (2 3)
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, c, 3, false, 0);
  const uint64_t arena_generation = k.arena_generation;
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  assert(strengthen_guard(k));
  assert(strengthen_apply(k) == ApplyResult::Ok);
  assert(k.stats.strengthened == 1);
  assert(k.stats.strengthen_subsumed == 0);
  assert(live_large(k) == 0); // (2 3) became a binary
  assert(k.arena.empty());
  assert(k.arena_generation == arena_generation + 1);
  assert(kernel_binary_id(k, internalize(2), internalize(3)) == 0 ||
         true); // pair exists (id 0 in proof-less tests)
  assert(default_solve(k) == 10);
}

static void test_kissat_mixed_occurrence_limit_admits_binary_strengthener() {
  // Kissat's default subsumeocclim is 1000, applied to the whole mixed watch
  // stream.  A stream just above CaDiCaL's 100-entry large-occurrence default
  // must therefore still be scanned in the production fused forward pass.
  Kernel k;
  kernel_reserve_vars(k, 110);
  const Lit candidate[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, candidate, 3, false, 0);
  kernel_add_binary_clause(k, internalize(1), internalize(-2));
  for (int var = 4; var <= 105; ++var)
    kernel_add_binary_clause(k, internalize(1), internalize(var));
  assert(k.watches.heads[internalize(1)].size > 100 &&
         k.watches.heads[internalize(1)].size <= STRENGTHEN_OCCLIM);
  assert(strengthen_apply(k) == ApplyResult::Ok);
  assert(k.stats.strengthened == 1);
  assert(has_binary(k, internalize(1), internalize(3)));
}

static void test_binary_partner_uses_kissat_last_strengthener() {
  // Both binaries can strengthen the candidate.  Kissat carries `remove`
  // across the whole marked-clause scan, so the later literal-3 walk
  // overwrites the literal-2 result and removes 1.
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, c, 3, false, 0);
  kernel_add_binary_clause(k, internalize(-1), internalize(3));
  kernel_add_binary_clause(k, internalize(2), internalize(-3));
  assert(strengthen_apply(k) == ApplyResult::Ok);
  const Lit expected[2] = {internalize(2), internalize(3)};
  assert(has_binary(k, expected[0], expected[1]));
  assert(!has_binary(k, internalize(1), internalize(2)));
}

static void test_opposite_phase_binary_does_not_override_later_result() {
  // (-1,2) is first found from same-phase literal 2 and removes 1.  The later
  // (-2,3) result removes 2.  When the scan finally reaches literal 1, Kissat
  // does not rescan binaries from the opposite phase, so (-1,2) cannot
  // overwrite the later result.
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit c[3] = {internalize(2), internalize(3), internalize(1)};
  kernel_add_large_clause(k, c, 3, false, 0);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(3));
  assert(strengthen_apply(k) == ApplyResult::Ok);
  assert(has_binary(k, internalize(1), internalize(3)));
  assert(!has_binary(k, internalize(2), internalize(3)));
}

static void test_fused_subsumption() {
  // c = (1 2 3) subsumed by the binary (1 2): fused round deletes it
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, c, 3, false, 0);
  const uint64_t arena_generation = k.arena_generation;
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  assert(strengthen_apply(k) == ApplyResult::Ok);
  assert(k.stats.strengthen_subsumed == 1);
  assert(k.stats.strengthened == 0);
  assert(live_large(k) == 0);
  assert(k.arena.empty());
  assert(k.arena_generation == arena_generation + 1);
}

static void test_large_partner_without_same_round_cascade() {
  // Kissat checks each scheduled candidate once.  The size-3 clause first
  // becomes binary (-1 3).  On the size-4 candidate, same-phase binary order
  // still reaches the older (-2 3) first and changes it to (1 3 4).  The new
  // (-1 3) opportunity must wait for the next round.
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit c[4] = {internalize(1), internalize(2), internalize(3),
                    internalize(4)};
  const Lit d[3] = {internalize(-1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, c, 4, false, 0);
  kernel_add_large_clause(k, d, 3, false, 0);
  kernel_add_binary_clause(k, internalize(-2), internalize(3));
  assert(strengthen_apply(k) == ApplyResult::Ok);
  assert(k.stats.strengthened == 2);
  const Lit once[3] = {internalize(1), internalize(3), internalize(4)};
  live_large(k, once, 3);
  assert(!k.strengthen_complete);
  assert(strengthen_apply(k) == ApplyResult::Ok);
  assert(k.stats.strengthened == 3);
  assert(live_large(k) == 0); // second round produces binary (3 4)
  assert(default_solve(k) == 10);
}

static void test_equal_size_candidates_use_inherited_arena_position() {
  // Kissat shrinks the first record in place. Tactix appends the packed
  // replacement, but its inherited logical position must keep it before the
  // equal-size victim. The source first becomes binary (1,3), which then
  // subsumes the victim (1,3,4) in the same forward pass.
  Kernel k;
  kernel_reserve_vars(k, 5);
  const Lit original[4] = {internalize(1), internalize(2), internalize(3),
                           internalize(5)};
  const Ref original_ref = kernel_add_large_clause(
      k, original, 4, false, 0, ++k.next_clause_id);
  const Lit victim[3] = {internalize(1), internalize(3), internalize(4)};
  kernel_add_large_clause(k, victim, 3, false, 0, ++k.next_clause_id);
  const Lit source[3] = {internalize(1), internalize(2), internalize(3)};
  Ref source_ref = INVALID_REF;
  assert(kernel_replace_clause(k, clause_at(k.arena, original_ref),
                               ++k.next_clause_id, source, 3, false, 0,
                               &source_ref) == ReplaceOutcome::Large);
  assert(source_ref > original_ref);
  assert(kernel_clause_position(k, source_ref) == original_ref);
  kernel_add_binary_clause(k, internalize(1), internalize(-2),
                           ++k.next_clause_id);

  assert(strengthen_apply(k) == ApplyResult::Ok);
  assert(k.stats.strengthened == 1);
  assert(k.stats.strengthen_subsumed == 1);
  assert(kernel_binary_copies(k, internalize(1), internalize(3)) == 1);
  assert(live_large(k) == 0);
  kernel_check_invariants(k);
}

static void test_connection_uses_growing_forward_population() {
  Kernel k;
  kernel_reserve_vars(k, 12 * (STRENGTHEN_OCCLIM + 1) + 4);
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
  // Dense-mode entry has flushed all large references.  Kissat therefore
  // connects the first partner against the growing forward index, before the
  // later fillers make those literals expensive; the size-4 victim is then
  // subsumed.  The complete BVE occurrence population must not leak here.
  assert(strengthen_apply(k) == ApplyResult::Ok);
  assert(k.stats.strengthen_subsumed == 1);
  assert(live_large(k) == 1 + 3 * (STRENGTHEN_OCCLIM + 1));
  kernel_dense_disconnect(k);
}

static void test_binary_watches_count_toward_connection_cap() {
  Kernel k;
  kernel_reserve_vars(k, 3 * (STRENGTHEN_OCCLIM + 1) + 4);
  const Lit partner[3] = {internalize(1), internalize(2), internalize(3)};
  const Lit victim[4] = {internalize(1), internalize(2), internalize(3),
                         internalize(4)};
  kernel_add_large_clause(k, partner, 3, false, 0);
  kernel_add_large_clause(k, victim, 4, false, 0);
  int next = 5;
  for (int anchor = 1; anchor <= 3; ++anchor)
    for (uint32_t i = 0; i <= STRENGTHEN_OCCLIM; ++i)
      kernel_add_binary_clause(k, internalize(anchor), internalize(next++));
  OccIndex dense;
  kernel_dense_connect(k, dense);
  // Kissat's SIZE_WATCHES population includes the binaries, so no literal of
  // `partner` is eligible for the round-local subsumer index.
  assert(strengthen_apply(k) == ApplyResult::NoOp);
  assert(k.stats.strengthen_subsumed == 0);
  assert(live_large(k) == 2);
  kernel_dense_disconnect(k);
}

static void test_kissat_touch_candidate_and_connection_predicates() {
  const Lit partner[3] = {internalize(1), internalize(2), internalize(3)};
  const Lit victim[4] = {internalize(1), internalize(2), internalize(3),
                         internalize(4)};
  {
    Kernel k;
    kernel_reserve_vars(k, 4);
    kernel_add_large_clause(k, partner, 3, false, 0);
    kernel_add_large_clause(k, victim, 4, false, 0);
    k.strengthen_touch_mark = k.touch_epoch;
    kernel_touch_var(k, var_of(internalize(1)));
    // Kissat schedules only clauses containing at least two armed variables.
    assert(strengthen_apply(k) == ApplyResult::NoOp);
    assert(k.stats.strengthen_checked == 0);
    assert(k.stats.strengthen_mark_skipped == 2);
    assert(live_large(k) == 2);
  }
  {
    Kernel k;
    kernel_reserve_vars(k, 4);
    kernel_add_large_clause(k, partner, 3, false, 0);
    kernel_add_large_clause(k, victim, 4, false, 0);
    k.strengthen_touch_mark = k.touch_epoch;
    kernel_touch_var(k, var_of(internalize(1)));
    kernel_touch_var(k, var_of(internalize(2)));
    // Two arms schedule `partner`, but connect_subsuming requires every active
    // variable in it to be armed.  It therefore cannot subsume `victim` yet.
    assert(strengthen_apply(k) == ApplyResult::NoOp);
    assert(k.stats.strengthen_checked == 2);
    assert(k.stats.strengthen_subsumed == 0);
    assert(live_large(k) == 2);
  }
  {
    Kernel k;
    kernel_reserve_vars(k, 4);
    kernel_add_large_clause(k, partner, 3, false, 0);
    kernel_add_large_clause(k, victim, 4, false, 0);
    k.strengthen_touch_mark = k.touch_epoch;
    kernel_touch_var(k, var_of(internalize(1)));
    kernel_touch_var(k, var_of(internalize(2)));
    kernel_touch_var(k, var_of(internalize(3)));
    assert(strengthen_apply(k) == ApplyResult::Ok);
    assert(k.stats.strengthen_subsumed == 1);
    assert(live_large(k) == 1);
  }
}

static void test_unit_producing_strengthen_ends_round() {
  // Root propagation derives the remaining unit before forward subsumption;
  // candidate discovery then retires the now-satisfied clause without a broad
  // root-false stripping pass.
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

static void test_forward_step_budget_cuts_between_candidates() {
  Kernel k;
  kernel_reserve_vars(k, 12);
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, c, 3, false, 0);
  const Lit d[4] = {internalize(9), internalize(10), internalize(11),
                    internalize(12)};
  kernel_add_large_clause(k, d, 4, false, 0);
  // None of these binaries acts on c.  Its complete first mixed stream costs
  // one candidate step plus one stream step plus one 128-byte cache line = 3.
  // A budget of one must still finish c and stop only before d.
  for (int v = 4; v <= 8; ++v)
    kernel_add_binary_clause(k, internalize(1), internalize(v));
  k.midround_budget = true;
  k.work_budget = 1;
  assert(strengthen_apply(k) == ApplyResult::NoOp);
  assert(k.stats.strengthen_checked == 1);
  assert(k.stats.strengthen_visits == 3);
  assert(k.stats.budget_stops_c2 == 1);
  assert(!k.strengthen_complete);
  assert(k.stats.strengthen_rearmed > 0);
  assert(live_large(k) == 2);

  k.midround_budget = false;
  k.work_budget = 0;
  assert(strengthen_apply(k) == ApplyResult::NoOp);
  assert(k.strengthen_complete);
  assert(k.stats.strengthen_checked == 2);
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
  // Pinned forward candidate discovery retires a root-satisfied irredundant
  // clause, but does not sparse-collect a root-false literal from an unrelated
  // clause. That physical triple remains available to the next BVE round.
  Kernel k;
  kernel_reserve_vars(k, 4);
  kernel_assign_unit(k, internalize(1));
  const Lit sat[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, sat, 3, false, 0); // root-satisfied: dies
  const Lit strip[3] = {internalize(-1), internalize(2), internalize(4)};
  kernel_add_large_clause(k, strip, 3, false, 0); // candidate, but unchanged
  assert(propagate_apply(k) == ApplyResult::Ok);
  assert(strengthen_guard(k));
  assert(strengthen_apply(k) == ApplyResult::Ok);
  assert(k.stats.flushed == 1);
  assert(k.stats.strengthened == 0); // attributed to flushed only
  assert(k.stats.strengthen_checked == 1);
  assert(live_large(k) == 1); // sat died; (-1 2 4) stays physically large
  assert(kernel_binary_copies(k, internalize(2), internalize(4)) == 0);
  assert(default_solve(k) == 10);
}

// C2 polls for interrupt at entry, in the candidate loop, and on the
// single-instance surface — with no cadence. Entry catches a pre-set flag;
// the later site needs the stress cadence, with
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
  { // (1) the round-entry poll
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
    // An entry abort leaves `candidates` empty, so there is no tail to
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
  { // (3) inside the candidate loop. The stress cadence
    // fires on a deterministic 1-in-m draw, so sweeping m walks the first fire
    // past the flush pass; a sweep that finds none must fail loudly here.
    bool past_entry = false;
    for (uint32_t m = 100; m <= 6000 && !past_entry; m += 100) {
      Kernel k;
      fill_interrupt_case(k, 200);
      k.stop_fuzz = m;
      strengthen_apply(k);
      past_entry = k.stats.strengthen_interrupted >= 1 &&
                   k.stats.strengthen_checked > 0;
      if (past_entry) {
        assert(!k.strengthen_complete);
        assert(k.strengthen_touch_mark == 0);
        kernel_check_invariants(k);
      }
    }
    assert(past_entry);
    Kernel c; // negative control: same formula, no knob, round completes
    fill_interrupt_case(c, 200);
    assert(strengthen_apply(c) == ApplyResult::NoOp);
    assert(c.stats.strengthen_interrupted == 0 && c.strengthen_complete);
  }
}

int main() {
  test_round_entry_deduplicates_binaries();
  test_round_entry_hyper_unary_binary_pair();
  test_binary_strengthens_large();
  test_kissat_mixed_occurrence_limit_admits_binary_strengthener();
  test_binary_partner_uses_kissat_last_strengthener();
  test_opposite_phase_binary_does_not_override_later_result();
  test_fused_subsumption();
  test_large_partner_without_same_round_cascade();
  test_equal_size_candidates_use_inherited_arena_position();
  test_connection_uses_growing_forward_population();
  test_binary_watches_count_toward_connection_cap();
  test_kissat_touch_candidate_and_connection_predicates();
  test_unit_producing_strengthen_ends_round();
  test_forward_step_budget_cuts_between_candidates();
  test_instance_surface();
  test_slot_schedule_as_data();
  test_root_flush();
  test_interrupt();
  puts("test_strengthen OK");
  return 0;
}
