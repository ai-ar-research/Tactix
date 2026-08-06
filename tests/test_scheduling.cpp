// SCHEDULING REGRESSION TESTS.
//
// WHY THIS FILE EXISTS. The rest of the suite is strong where the solver can be
// WRONG (soundness: brute-force oracles, model checks on SAT, proof checks on
// UNSAT, ASan/UBSan) and silent where it can be BADLY SCHEDULED. That gap is
// not hypothetical — every scheduling defect found so far was caught by reading
// reference source or by staring at a telemetry probe, and none of them would
// have failed a test. Their shapes, all pinned below:
//
//   * a batch entry's budget pre-check UNFIREABLE by construction
//   * the mode growth key running one switch ahead of kissat's
//   * an effort meter under-pricing the work by orders of magnitude
//   * instances() re-emitting its head every round and starving the tail
//
// A defect found by measurement costs a cluster window and an argument; the
// same defect found here costs milliseconds. Scheduling is deterministic and
// the policy exposes its transitions so tests can pose schedules directly.
//
// Each test below pins the REFERENCE'S OWN identity, not our current behaviour,
// so it fails when we drift from kissat/cadical rather than when we change.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "rules/registry.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace tactix;

// --- SCHED-B5: the mode growth key ---------------------------------------
//
// kissat mode.c:90 `const uint64_t count = (statistics->switched + 1) / 2;`,
// scaled by kissat_nlogpown(count, 4) = count * (log10(count+9))^4
// (kimits.c:27). `switched` and `limits.mode.count` are incremented at the SAME
// site (mode.c:204-205) and this branch runs only on EVEN mode.count, so the
// key is exactly mode.count/2 -- and the FIRST focused re-entry has key 1,
// where log10(1+9)^4 = 1 exactly. So kissat's first focused interval is
// `modeint` ON THE NOSE, an integer identity that cannot be fudged.
//
// We shipped `(mode.count / 2) + 1`, i.e. key 2, i.e. 2*log10(11)^4 ~= 2.35x
// the reference interval, growing in the same direction forever after. This
// test fails on that exact bug.
static void test_mode_growth_key_matches_kissat(void) {
  Kernel k;
  kernel_reserve_vars(k, 4);
  DefaultPolicy p;

  // Pose the switch sequence directly: count 1 = entered stable (ticks branch),
  // count 2 = entered focused (the conflicts branch under test).
  p.mode.count = 2;
  k.stats.conflicts = 500;
  p.update_mode_limits(k);
  const uint64_t first = p.mode.conflicts_limit - k.stats.conflicts;
  assert(first == DefaultPolicy::MODE_INTERVAL &&
         "kissat's FIRST focused interval is exactly modeint (key 1, "
         "log10(10)^4 == 1). A larger value means the growth key is ahead.");

  // And the whole ladder, not just its first rung.
  for (uint64_t c = 4; c <= 12; c += 2) {
    p.mode.count = c;
    k.stats.conflicts = 1000 * c;
    p.update_mode_limits(k);
    const double key = static_cast<double>(c / 2);
    const double l = std::log10(key + 9);
    const uint64_t want = static_cast<uint64_t>(
        DefaultPolicy::MODE_INTERVAL * key * l * l * l * l);
    const uint64_t got = p.mode.conflicts_limit - k.stats.conflicts;
    assert(got == want && "focused interval must be modeint * nlogpown(c/2, 4)");
  }
  std::printf("  mode growth key: matches kissat's (switched+1)/2 ladder\n");
}

// --- SCHED-B3: instances() must ROTATE, not replay its head ---------------
//
// The policy can only take `cap` instances per occasion. Emitting the first
// `cap` sources every round means a budget-truncated round re-derives the same
// list and starts from the front, so the tail is never reached. That is why a
// tighter, honest effort budget for TransitiveReduce measured NEGATIVE without
// this: it bought starvation, not economy. kissat keeps a per-variable pending
// bit (`transitivekeep`, default 1) to the same end.
//
// N1 note: rotation is the N1-safe shape. It removes no capability, because
// apply_instance still accepts ANY source literal; only the enumeration ORDER
// moves. A cursor that HID instances would be an N1 breach.
static void test_transitive_instances_rotate(void) {
  Kernel k;
  kernel_reserve_vars(k, 128);
  // A source needs out-degree >= 2 in the binary implication graph and at
  // least one destination above it, so build ten FANS rather than a chain
  // (a chain gives every literal out-degree 1 and yields no sources at all --
  // which silently SKIPPED this test on the first attempt).
  for (uint32_t s = 0; s < 10; s++)
    for (uint32_t t = 0; t < 4; t++)
      kernel_add_binary_clause(k, make_lit(s, true),
                               make_lit(20 + s * 4 + t, true));

  const RuleVTable *v4 = nullptr;
  for (const RuleVTable &r : tactix_rules())
    if (!std::strcmp(r.name, "TransitiveReduce"))
      v4 = &r;
  assert(v4 && v4->instances && v4->apply_instance);
  if (!v4->guard(k)) {
    std::printf("  transitive rotation: SKIPPED (guard false)\n");
    return;
  }

  uint64_t a[4] = {0, 0, 0, 0}, b[4] = {0, 0, 0, 0};
  const uint32_t na = v4->instances(k, a, 4);
  // A skip here would make the test VACUOUS, which is worse than absent: it
  // reads green while proving nothing. Demand the fixture instead.
  assert(na >= 4 && "fixture must offer more sources than the cap, or the "
                    "rotation is not observable");
  // Applying advances the cursor, so the NEXT enumeration must not begin at the
  // same place. Without the cursor these two calls are identical.
  v4->apply_instance(k, a[0]);
  const uint32_t nb = v4->instances(k, b, 4);
  assert(nb > 0);
  assert(!(a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3]) &&
         "instances() replayed its head: a truncated round would starve the "
         "tail forever (SCHED-B3)");
  std::printf("  transitive rotation: enumeration advances after apply\n");
}

// --- SCHED-B4: subsumption must meter PARTNER VISITS, not candidates ------
//
// cadical budgets subsumption in `stats.subchecks` (subsume.cpp:86), bumped once
// per candidate partner actually compared, because the cost of "checking a
// clause" is the length of its occurrence lists and not 1. Metering candidates
// under-prices exactly the expensive ones -- by three orders of magnitude on a
// hot-literal formula, which is how a rule can own most of a solve inside a
// handful of "rounds" without ever exceeding its budget.
//
// A hot literal shared by many clauses makes the gap structural: one candidate,
// many partners. If visits ever collapse back to ~checked, the meter has
// silently reverted to counting candidates.
static void test_subsume_meters_partner_visits(void) {
  Kernel k;
  kernel_reserve_vars(k, 96);
  // LARGE clauses (binaries are not forward-subsumption candidates), all
  // sharing one hot literal so a single candidate faces many partners.
  const Lit hot = make_lit(0, true);
  for (uint32_t i = 2; i < 60; i++) {
    const Lit c4[4] = {hot, make_lit(1, true), make_lit(i, true),
                       make_lit(i + 1, true)};
    kernel_add_large_clause(k, c4, 4, false, 0, 0);
  }
  const Lit c3[3] = {hot, make_lit(1, true), make_lit(2, true)};
  kernel_add_large_clause(k, c3, 3, false, 0, 0); // subsumes one of the above

  const RuleVTable *c1 = nullptr;
  for (const RuleVTable &r : tactix_rules())
    if (!std::strcmp(r.name, "SubsumeForward"))
      c1 = &r;
  assert(c1);
  if (!c1->guard(k)) {
    std::printf("  subsume meter: SKIPPED (guard false)\n");
    return;
  }
  c1->apply(k);
  assert(k.stats.forward_visits > 0 &&
         "forward_visits never moved -- the meter is not reachable, so a green "
         "budget test would be vacuous");
  assert(k.stats.forward_visits > k.stats.forward_checked &&
         "visits must exceed candidates on a formula with a shared literal; "
         "equality means the meter is counting candidates again (SCHED-B4)");
  std::printf("  subsume meter: %llu visits vs %llu candidates\n",
              (unsigned long long)k.stats.forward_visits,
              (unsigned long long)k.stats.forward_checked);
}

// --- SCHED-C1: Vivify's redundant scope must track the LIVE tier ----------
//
// kissat's vivify_tier2_limit (vivify.c:132) reads solver->tier2[0] whenever
// `vivifyfocusedtiers` is set, and that option DEFAULTS TO 1 -- the dynamic
// limit is the shipped behaviour. `Kernel::tier2` is recomputed from the glue
// distribution on every reduce, and it is easy for that to be read by nothing
// at all -- the "we own the mechanism and never wire it" shape this file exists
// to catch. Moving the tier must move what Vivify will touch.
static void test_vivify_scope_tracks_tier2(void) {
  const RuleVTable *viv = nullptr;
  for (const RuleVTable &r : tactix_rules())
    if (!std::strcmp(r.name, "Vivify"))
      viv = &r;
  assert(viv);
  auto candidates_at = [&](uint32_t tier2) {
    Kernel k;
    kernel_reserve_vars(k, 32);
    k.tier2 = tier2;
    // redundant clauses spanning a range of glues, all otherwise vivifiable
    for (uint32_t g = 2; g <= 10; g++) {
      const Lit c3[3] = {make_lit(g, true), make_lit(g + 10, true),
                         make_lit(g + 20, true)};
      const Ref r = kernel_add_large_clause(k, c3, 3, true, g, 0);
      (void)r;
    }
    uint64_t inst[64];
    return viv->instances ? viv->instances(k, inst, 64) : 0u;
  };
  const uint32_t narrow = candidates_at(2);
  const uint32_t wide = candidates_at(10);
  assert(wide > narrow &&
         "Vivify's redundant scope ignored Kernel::tier2 -- it is back on the "
         "static VIVIFY_TIER2 and the dynamic tier is unwired again (SCHED-C1)");
  printf("  vivify scope: tier2=2 -> %u candidates, tier2=10 -> %u\n", narrow,
         wide);
}

// --- SCHED-D1: the authored slot order covers the registry -----------------
//
// The slot order is DATA (default_slot_order). A rule the table does not name
// is still scheduled — appended, registry order — which is exactly the T6
// defect coming back for that rule: its position becomes an accident again.
// This test fails when an Inprocess-tier rule is registered without being given
// an authored position, and when the table names a rule that is not registered
// at the Inprocess tier (a stale row orders nothing).
static void test_authored_slot_order_covers_registry(void) {
  size_t norder = 0;
  const SlotOrderRow *order = default_slot_order(&norder);
  const std::vector<RuleVTable> &rules = tactix_rules();
  for (const RuleVTable &r : rules) {
    if (r.tier != RuleTier::Inprocess)
      continue;
    bool found = false;
    for (size_t o = 0; o < norder; o++)
      found = found || !std::strcmp(order[o].name, r.name);
    if (!found)
      std::printf("  UNAUTHORED Inprocess rule: %s\n", r.name);
    assert(found &&
           "an Inprocess-tier rule has no authored slot position -- give it one "
           "in AUTHORED_SLOT_ORDER (SCHED-D1); the append fallback puts it last "
           "by accident, which is the T6 defect");
  }
  for (size_t o = 0; o < norder; o++) {
    bool found = false;
    for (const RuleVTable &r : rules)
      found = found ||
              (r.tier == RuleTier::Inprocess && !std::strcmp(order[o].name, r.name));
    if (!found)
      std::printf("  STALE authored row: %s\n", order[o].name);
    assert(found && "AUTHORED_SLOT_ORDER names a rule that is not registered at "
                    "the Inprocess tier");
  }
  std::printf("  authored slot order: %zu rows, registry-complete\n", norder);
}

// --- SCHED-D2: the two clocks are actually two ------------------------------
//
// kissat runs the probing pipeline on probeint=100 x NLOGN (probe.c:85) and the
// elimination pipeline on eliminateint=500 x NLOG2N (eliminate.c:600). Under
// one clock everything rides the elimination cadence. This test fails if the
// probing clock is ever scheduled no sooner than the elimination clock at the
// same phase count -- i.e. if the split silently collapses back to one clock,
// which is exactly the shape that makes an A/B arm vacuous.
static void test_two_slot_clocks_diverge(void) {
  Kernel k;
  kernel_reserve_vars(k, 4);
  DefaultPolicy p;
  p.inprocess.initialized = true;
  for (uint64_t n = 0; n < 8; n++) {
    p.probe_clock.rounds = n;
    p.elim_clock.rounds = n;
    p.update_slot_limit(k, SlotGroup::Probe);
    p.update_slot_limit(k, SlotGroup::Eliminate);
    assert(p.probe_clock.next_conflicts < p.elim_clock.next_conflicts &&
           "the probing clock is not firing sooner than the elimination clock "
           "-- the two clocks have collapsed into one (SCHED-D2)");
    // and the gap WIDENS: NLOG2N outgrows NLOGN (ledger SP-SLOT-1).
    if (n) {
      p.probe_clock.rounds = n - 1;
      p.elim_clock.rounds = n - 1;
      p.update_slot_limit(k, SlotGroup::Probe);
      const uint64_t prev_probe = p.probe_clock.next_conflicts;
      p.update_slot_limit(k, SlotGroup::Eliminate);
      const uint64_t prev_elim = p.elim_clock.next_conflicts;
      p.probe_clock.rounds = n;
      p.elim_clock.rounds = n;
      p.update_slot_limit(k, SlotGroup::Probe);
      p.update_slot_limit(k, SlotGroup::Eliminate);
      assert(p.elim_clock.next_conflicts - prev_elim >=
                 p.probe_clock.next_conflicts - prev_probe &&
             "the elimination interval is not growing at least as fast as the "
             "probing one");
    }
  }
  std::printf("  slot clocks: probe next=%llu < eliminate next=%llu at 8 phases\n",
              (unsigned long long)p.probe_clock.next_conflicts,
              (unsigned long long)p.elim_clock.next_conflicts);
}

// --- SCHED-D3: the elapsed interval is necessary, not sufficient -----------
//
// kissat eliminate.c:33-38 and cadical elim.cpp:75-81 both refuse an
// elimination phase whose interval has elapsed but which has no new material to
// act on; cadical probe.cpp:23 refuses an inprobe phase with no reduce since the
// last one. Both gates are SAFETY VALVES: on most instances the pipeline keeps
// producing material and they never bind, which is exactly why they need a unit
// test -- a tier run cannot distinguish "gate correct and quiet" from "gate
// wired to nothing".
static void test_slot_gates_decline_without_new_material(void) {
  Kernel k;
  kernel_reserve_vars(k, 4);
  DefaultPolicy p;
  p.inprocess.initialized = true;
  k.stats.conflicts = 100;
  // Past the first phase on both clocks, intervals elapsed, marks == live: the
  // interval says GO and the gates must still say no.
  p.probe_clock.rounds = p.elim_clock.rounds = 1;
  p.probe_clock.next_conflicts = p.elim_clock.next_conflicts = 0;
  p.probe_clock.reductions_mark = k.stats.reductions;
  p.elim_clock.marks_mark = k.stats.bve_marks;
  p.elim_clock.fixed_mark = kernel_root_fixed(k);
  assert(!p.want_slot_phase(k, SlotGroup::Eliminate) &&
         "an elimination phase fired with no new material (SCHED-D3)");
  assert(!p.want_slot_phase(k, SlotGroup::Probe) &&
         "a probing phase fired with no reduce since the last one (SCHED-D3)");
  assert(p.elim_clock.blocked_episodes == 1 &&
         p.probe_clock.blocked_episodes == 1);
  // An episode is counted once, not once per search step.
  (void)p.want_slot_phase(k, SlotGroup::Eliminate);
  (void)p.want_slot_phase(k, SlotGroup::Probe);
  assert(p.elim_clock.blocked_episodes == 1 &&
         p.probe_clock.blocked_episodes == 1);
  // New elimination material re-admits; so does a reduce for probing.
  k.stats.bve_marks++;
  assert(p.want_slot_phase(k, SlotGroup::Eliminate));
  k.stats.reductions++;
  assert(p.want_slot_phase(k, SlotGroup::Probe));
  // The FIRST phase of each clock is exempt -- it is the preprocessing pass and
  // has no previous phase to be new relative to (cadical's
  // `stats.inprobingphases &&`).
  DefaultPolicy q;
  q.inprocess.initialized = true;
  q.probe_clock.next_conflicts = q.elim_clock.next_conflicts = 0;
  q.probe_clock.reductions_mark = k.stats.reductions;
  q.elim_clock.marks_mark = k.stats.bve_marks;
  q.elim_clock.fixed_mark = kernel_root_fixed(k);
  assert(q.want_slot_phase(k, SlotGroup::Probe) &&
         q.want_slot_phase(k, SlotGroup::Eliminate) &&
         "the preprocessing phase must not be gated");
  std::printf("  slot gates: decline without new material, re-admit with it\n");
}

int main() {
  test_mode_growth_key_matches_kissat();
  test_transitive_instances_rotate();
  test_subsume_meters_partner_visits();
  test_vivify_scope_tracks_tier2();
  test_authored_slot_order_covers_registry();
  test_two_slot_clocks_diverge();
  test_slot_gates_decline_without_new_material();
  std::printf("test_scheduling: all passed\n");
  return 0;
}
