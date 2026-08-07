// Scheduling regression tests. Each one pins the reference's own identity (a
// kissat/cadical formula) or an N1 obligation, so it fails on drift from the
// reference rather than on any change here.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "rules/registry.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace tactix;

// --- the mode growth key ---------------------------------------------------
// kissat scales modeint by kissat_nlogpown(count, 4) with
// count = (switched+1)/2 (mode.c:90, kimits.c:27); the branch runs only on even
// mode.count, so the key is mode.count/2 and the first focused interval is
// `modeint` exactly. A key one switch ahead scales it by ~2.35 forever after.
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

// --- the resumption cursor, a policy object --------------------------------
// A dose cut partway into a candidate list must resume past what it consumed,
// or it re-applies the same prefix and starves the tail (kissat's
// `transitivekeep` bit serves the same end). The cursor lives in
// DefaultPolicy::SlotEntry: the rule enumerates its set, the policy decides
// where the walk starts.
static void build_transitive_fan_fixture(Kernel &k) {
  // A source needs out-degree >= 2 and a destination above it, so ten fans
  // rather than a chain, which would give every literal out-degree 1.
  kernel_reserve_vars(k, 128);
  for (uint32_t s = 0; s < 10; s++)
    for (uint32_t t = 0; t < 4; t++)
      kernel_add_binary_clause(k, make_lit(s, true),
                               make_lit(20 + s * 4 + t, true));
}

static const RuleVTable *find_rule(const char *name) {
  for (const RuleVTable &r : tactix_rules())
    if (!std::strcmp(r.name, name))
      return &r;
  return nullptr;
}

// Same multiset, same size. Every order arm and the cursor rotation must
// satisfy it: dropping a candidate narrows what DefaultPolicy can reach.
static bool same_multiset(std::vector<uint64_t> a, std::vector<uint64_t> b) {
  std::sort(a.begin(), a.end());
  std::sort(b.begin(), b.end());
  return a == b;
}

static void test_slot_cursor_rotates(void) {
  Kernel k;
  build_transitive_fan_fixture(k);
  const RuleVTable *v4 = find_rule("TransitiveReduce");
  assert(v4 && v4->instances && v4->apply_instance);
  if (!v4->guard(k)) {
    std::printf("  slot cursor: SKIPPED (guard false)\n");
    return;
  }
  std::vector<uint64_t> buf(64, 0);
  const uint32_t n = v4->instances(k, buf.data(), 64);
  // A skip here would be vacuous: green while proving nothing.
  assert(n >= 4 && "fixture must offer several sources or rotation is not "
                   "observable");
  buf.resize(n);
  const std::vector<uint64_t> native = buf;

  DefaultPolicy p;
  DefaultPolicy::SlotEntry e;
  e.rule = static_cast<int>(v4->id);
  e.dosed = true;
  e.cursor_enabled = true;

  p.order_candidates(k, e, buf.data(), n);
  assert(e.ord.rotated == 0 && "a fresh cursor must not rotate");
  assert(buf == native && "the native arm must leave the order alone");

  // Consume the first two candidates as the dose loop does, then re-enumerate.
  Lit consumed;
  assert(lit_of_instance(native[1], &consumed));
  e.cursor = static_cast<uint64_t>(consumed) + 1;
  buf = native;
  p.order_candidates(k, e, buf.data(), n);
  assert(e.ord.rotated == 1 && "the cursor must rotate a resumed walk");
  assert(buf[0] != native[0] &&
         "the walk replayed its head: a truncated dose would starve the tail "
         "forever (SCHED-B3)");
  assert(buf[0] == native[2] && "resume at the first key past the cursor");
  assert(same_multiset(buf, native) && "rotation must be a PERMUTATION");

  // A cursor past the whole set wraps to the head rather than emptying it.
  e.cursor = UINT64_MAX;
  buf = native;
  p.order_candidates(k, e, buf.data(), n);
  assert(buf == native && "an exhausted cursor restarts at the head");
  std::printf("  slot cursor: resumes past the consumed prefix, wraps, "
              "permutes\n");
}

// --- N1: the cursor moves the start, never the set -------------------------
// Resumption lives in the policy and must not narrow the rule: `instances()`
// offers the same set wherever the cursor is, `apply_instance` accepts a
// payload rotated past with the same result and effect, and every order arm is
// a permutation.
static void test_cursor_preserves_n1(void) {
  const RuleVTable *v4 = find_rule("TransitiveReduce");
  assert(v4 && v4->instances && v4->apply_instance);

  Kernel k;
  build_transitive_fan_fixture(k);
  if (!v4->guard(k)) {
    std::printf("  cursor N1: SKIPPED (guard false)\n");
    return;
  }
  std::vector<uint64_t> before(64, 0);
  const uint32_t n = v4->instances(k, before.data(), 64);
  before.resize(n);
  assert(n >= 4);

  DefaultPolicy p;
  DefaultPolicy::SlotEntry e;
  e.rule = static_cast<int>(v4->id);
  e.dosed = true;
  e.cursor_enabled = true;
  std::vector<uint64_t> walk = before;
  Lit deep;
  assert(lit_of_instance(before[n / 2], &deep));
  e.cursor = static_cast<uint64_t>(deep); // resume mid-set
  p.order_candidates(k, e, walk.data(), n);
  assert(e.ord.rotated == 1);

  // (1) the rule's surface is untouched by the policy's cursor.
  std::vector<uint64_t> after(64, 0);
  const uint32_t n2 = v4->instances(k, after.data(), 64);
  after.resize(n2);
  assert(n2 == n && after == before &&
         "instances() must not depend on the policy's cursor");

  // (2) a payload rotated past is still applicable, with the same effect
  const uint64_t skipped = before[0];
  assert(walk[0] != skipped && "fixture: the cursor must have rotated past it");
  Kernel k1, k2;
  build_transitive_fan_fixture(k1);
  build_transitive_fan_fixture(k2);
  const ApplyResult r1 = v4->apply_instance(k1, skipped);
  const ApplyResult r2 = v4->apply_instance(k2, skipped);
  assert(r1 == r2);
  assert(k1.stats.transitive_checked == k2.stats.transitive_checked &&
         k1.stats.transitive_deleted == k2.stats.transitive_deleted &&
         "apply_instance must be cursor-independent");

  // (3) every arm permutes; an outside CandidateOrderPolicy is the hook a
  //     preset or the phase-6 learner takes the order over through
  const CandidateOrder arms[] = {
      CandidateOrder::Native,  CandidateOrder::Reverse,
      CandidateOrder::Random,  CandidateOrder::OccAsc,
      CandidateOrder::OccDesc, CandidateOrder::NegOccDesc,
      CandidateOrder::Heuristic};
  for (CandidateOrder arm : arms) {
    DefaultCandidateOrder co;
    co.arms.assign(tactix_rules().size(), CandidateOrder::Native);
    co.arms[v4->id] = arm;
    DefaultPolicy q;
    q.candidate_order = &co;
    DefaultPolicy::SlotEntry qe;
    qe.rule = static_cast<int>(v4->id);
    qe.dosed = true;
    qe.order = arm;
    std::vector<uint64_t> out = before;
    q.order_candidates(k, qe, out.data(), n);
    assert(same_multiset(out, before) && candidate_order_name(arm) &&
           "an order arm must be a PERMUTATION, never a filter");
  }
  std::printf("  cursor N1: instances() unchanged, apply_instance "
              "cursor-independent, all 7 arms permute\n");
}

// --- each ordering arm must actually reorder --------------------------------
// Every arm must move something on a fixture where its key is not constant.
// Native is the control: it must move nothing.
static void test_order_arms_reorder(void) {
  const RuleVTable *v4 = find_rule("TransitiveReduce");
  assert(v4 && v4->instances);
  Kernel k;
  kernel_reserve_vars(k, 512);
  // Fans of differing widths on both polarities, not each other's complement,
  // or occ(x)+occ(-x) is constant and the occurrence arms are inert here.
  for (uint32_t s = 0; s < 10; s++) {
    for (uint32_t t = 0; t < 2 + s; t++)
      kernel_add_binary_clause(k, make_lit(s, true),
                               make_lit(20 + s * 16 + t, true));
    for (uint32_t t = 0; t < 2 + s % 3; t++)
      kernel_add_binary_clause(k, make_lit(s, false),
                               make_lit(300 + s * 16 + t, true));
  }
  // `H` is optional policy state and the Heuristic arm falls back to native
  // without it, so attach one with distinct, scrambled scores.
  ScopedHeuristics h(k);
  k.h->stable = true;
  for (uint32_t v = 0; v < k.nvars; v++)
    k.h->scores.score[v] = static_cast<double>((v * 7919u) % 1013u);
  if (!v4->guard(k)) {
    std::printf("  order arms: SKIPPED (guard false)\n");
    return;
  }
  std::vector<uint64_t> before(2048, 0);
  const uint32_t n = v4->instances(k, before.data(), 2048);
  before.resize(n);
  assert(n >= 4);

  const CandidateOrder movers[] = {
      CandidateOrder::Reverse,    CandidateOrder::Random,
      CandidateOrder::OccAsc,     CandidateOrder::OccDesc,
      CandidateOrder::NegOccDesc, CandidateOrder::Heuristic};
  for (CandidateOrder arm : movers) {
    DefaultCandidateOrder co;
    co.arms.assign(tactix_rules().size(), CandidateOrder::Native);
    co.arms[v4->id] = arm;
    DefaultPolicy q;
    q.candidate_order = &co;
    DefaultPolicy::SlotEntry qe;
    qe.rule = static_cast<int>(v4->id);
    qe.dosed = true;
    qe.order = arm;
    std::vector<uint64_t> out = before;
    q.order_candidates(k, qe, out.data(), n);
    assert(qe.ord.ranked == 1 && qe.ord.moved > 0 &&
           "arm did not reorder anything -- it would be untestable");
  }
  {
    DefaultPolicy q; // no attached policy, arm stays Native
    DefaultPolicy::SlotEntry qe;
    qe.rule = static_cast<int>(v4->id);
    qe.dosed = true;
    std::vector<uint64_t> out = before;
    q.order_candidates(k, qe, out.data(), n);
    assert(qe.ord.ranked == 0 && qe.ord.moved == 0 && out == before &&
           "the native arm must be the identity");
  }
  std::printf("  order arms: 6 arms reorder, native is the identity\n");
}

// --- subsumption meters partner visits, not candidates ----------------------
// cadical bumps `stats.subchecks` once per partner compared (subsume.cpp:86):
// the cost of checking a clause is its occurrence-list length, not 1. Visits
// collapsing back to ~checked means the meter counts candidates again, which
// under-prices exactly the expensive rounds.
static void test_subsume_meters_partner_visits(void) {
  Kernel k;
  kernel_reserve_vars(k, 96);
  // Large clauses (binaries are not candidates) sharing one hot literal, so a
  // single candidate faces many partners.
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

// --- Vivify's redundant scope must track the live tier ----------------------
// kissat's vivify_tier2_limit reads solver->tier2[0] whenever
// `vivifyfocusedtiers` is set, and it defaults to 1 (vivify.c:132), so moving
// `Kernel::tier2` must move what Vivify will touch.
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
    // Scope 1 is the band defined by tier2; scope 0 spans every band and is
    // tier2-independent by construction (N1, tests/test_vivify_n1.cpp).
    k.vivify_scope = 1;
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

// --- the authored slot order covers the registry ----------------------------
// The slot order is data (default_slot_order), and an unnamed rule is still
// scheduled — appended in registry order — so its position becomes an accident.
// Every Inprocess-tier rule needs an authored position, and every authored row
// needs a rule registered at that tier.
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

// --- the two clocks are actually two ----------------------------------------
// kissat runs probing on probeint=100 x NLOGN (probe.c:85) and elimination on
// eliminateint=500 x NLOG2N (eliminate.c:600), so at the same phase count the
// probing clock must fire strictly sooner or the split has collapsed into one.
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
    // and the gap must widen: NLOG2N outgrows NLOGN.
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

// --- the elapsed interval is necessary, not sufficient ----------------------
// kissat eliminate.c:33-38 and cadical elim.cpp:75-81 refuse an elimination
// phase with no new material even once its interval elapsed; cadical
// probe.cpp:23 refuses an inprobe phase with no reduce since the last one.
static void test_slot_gates_decline_without_new_material(void) {
  Kernel k;
  kernel_reserve_vars(k, 4);
  DefaultPolicy p;
  p.inprocess.initialized = true;
  k.stats.conflicts = 100;
  // Past the first phase, intervals elapsed, marks == live: the interval says
  // go and the gates must still say no.
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
  // The first phase is exempt: preprocessing has no previous phase to be new
  // relative to (cadical `stats.inprobingphases &&`).
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

// --- the effort budget's upper clamp ----------------------------------------
// kissat's SET_EFFORT_LIMIT has no ceiling (kimits.h:135-170), so the clamp
// follows cadical's propagation scheme, whose three steps compose in a
// load-bearing order: max(delta, mineff), then min(delta, maxeff), then
// max(delta, 2*active()). The active floor comes last (elim.cpp:787,
// subsume.cpp:357), so on a large formula it lifts the delta back above the
// ceiling — the three are not a simple clamped range.
static void test_effort_ceiling(void) {
  const RuleVTable *bve = find_rule("BVE");
  assert(bve);
  Kernel k;
  kernel_reserve_vars(k, 64);
  DefaultPolicy p;
  DefaultPolicy::SlotEntry e;
  e.rule = static_cast<int>(bve->id);
  e.permille = 100;
  e.meter = [](const Kernel &) -> uint64_t { return 0; };

  // No search yet: the reference floors at MIN_EFFORT_TICKS, so the delta is
  // 1e7/1000*100 == 1e6, and an uncapped entry neither clamps nor counts.
  p.set_entry_limit(e, k);
  assert(e.limit == 1000000 && e.tm.clamped == 0 && e.tm.floored == 0);

  // A ceiling below that must bind and be counted: an uncounted clamp is a
  // dose change nothing can attribute.
  e.max_delta = 250000;
  p.set_entry_limit(e, k);
  assert(e.limit == 250000 && e.tm.clamped == 1);

  // ... and the active floor, applied after it, lifts it back above.
  e.active_floor = true;
  k.unassigned = 400000; // 2*active == 800000 > the 250000 ceiling
  p.set_entry_limit(e, k);
  assert(e.limit == 800000 && e.tm.clamped == 2 && e.tm.floored == 1 &&
         "cadical floors AFTER clamping, so the floor wins on a big formula");

  // A ceiling above the natural delta is inert, which is the shipped case.
  e.active_floor = false;
  e.max_delta = 1000000000ull;
  const uint64_t clamped_before = e.tm.clamped;
  p.set_entry_limit(e, k);
  assert(e.limit == 1000000 && e.tm.clamped == clamped_before);
  std::printf("  effort ceiling: clamps, counts, and the active floor "
              "overrides it\n");
}

int main() {
  test_mode_growth_key_matches_kissat();
  test_effort_ceiling();
  test_slot_cursor_rotates();
  test_cursor_preserves_n1();
  test_order_arms_reorder();
  test_subsume_meters_partner_visits();
  test_vivify_scope_tracks_tier2();
  test_authored_slot_order_covers_registry();
  test_two_slot_clocks_diverge();
  test_slot_gates_decline_without_new_material();
  std::printf("test_scheduling: all passed\n");
  return 0;
}
