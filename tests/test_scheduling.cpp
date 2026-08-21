// Scheduling regression tests. Each one pins the reference's own identity (a
// kissat/cadical formula) or an N1 obligation, so it fails on drift from the
// reference rather than on any change here.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "rules/propagate/propagate.hpp"
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

static void test_reason_bump_depth_matches_kissat(void) {
  DefaultPolicy p;
  assert(p.strategy_config().analysis.reason_depth == 1);
  std::printf("  reason bump: Kissat direct reasons in both modes\n");
}

static void test_reason_bump_delay_lives_at_final_clause_gate(void) {
  DefaultPolicy::ReasonBumpDelay delay{2, 2};
  // Conflict reuse and failed-literal analysis exit before Kissat calls
  // DELAYING(bumpreasons), so neither consumes the delay.
  assert(delay.count == 2);
  // The first two ordinary finalized clauses consume it one gate at a time.
  delay.reached_gate(false, false);
  assert(delay.count == 1 && delay.current == 2);
  delay.reached_gate(false, false);
  assert(delay.count == 0 && delay.current == 2);
  // A successful pass halves the interval; an over-budget pass bumps it.
  delay.reached_gate(true, false);
  assert(delay.count == 1 && delay.current == 1);
  delay.count = 0;
  delay.reached_gate(true, true);
  assert(delay.count == 2 && delay.current == 2);
  std::printf("  reason bump delay: consumed only at finalized-clause gate\n");
}

static void test_analysis_trail_reuse_defaults_to_kissat(void) {
  DefaultPolicy p;
  assert(!p.chrono_reuse_trail_ &&
         "Kissat is the tie-break and has no analysis-side trail reuse");
  // The CaDiCaL strategy remains a first-class diagnostic/experimental arm.
  p.chrono_reuse_trail_ = true;
  assert(p.chrono_reuse_trail_);
  assert(DefaultPolicy::BUMP_REASONS_RATE == 10.0 &&
         "Kissat's bumpreasonsrate=10 is the conflicting-default tie-break");
  std::printf("  analysis trail reuse: Kissat default off, CaDiCaL arm retained\n");
}

static void test_factor_material_gate_uses_literal_marks(void) {
  Kernel k;
  kernel_reserve_vars(k, 8);
  k.stats.conflicts = 1; // search-time call: the material gate is live
  std::fill(k.factor_marks.begin(), k.factor_marks.end(), 0);

  DefaultPolicy p;
  p.inprocess().build(k);
  DefaultInprocessScheduler::SlotEntry *factor = nullptr;
  const auto &rules = tactix_rules();
  for (auto &e : p.inprocess().entries) {
    const bool is_factor = !std::strcmp(rules[e.rule].name, "Factor");
    e.enabled = is_factor;
    if (is_factor)
      factor = &e;
  }
  assert(factor);
  assert(p.inprocess().run_phase(k, SlotGroup::Probe, nullptr) == 0);
  assert(factor->tm.dec_material == 1 && factor->tm.admitted == 0);

  // Pose unchanged coarse watermarks.  A newly marked polarity must still
  // admit Factor, as Kissat's `flags->factor` / `literals_factor` wire does.
  factor->binaries_mark = k.binary_ids.size();
  factor->arena_mark = k.arena.size();
  factor->root_fixed_mark = kernel_root_fixed(k);
  k.factor_marks[make_lit(3, true)] = 1;
  assert(p.inprocess().run_phase(k, SlotGroup::Probe, nullptr) == 0);
  assert(factor->tm.dec_material == 1 && factor->tm.admitted == 1);
  std::printf("  factor material gate: exact per-literal Kissat marks\n");
}

static void test_large_formula_factor_decline_is_solve_latched(void) {
  Kernel k;
  kernel_reserve_vars(k, 8);
  DefaultStrategyConfig cfg;
  cfg.inprocess.factor_active_ceiling = 6;
  DefaultPolicy p(cfg);
  p.inprocess().build(k);

  DefaultInprocessScheduler::SlotEntry *factor = nullptr;
  const auto &rules = tactix_rules();
  for (auto &e : p.inprocess().entries) {
    const bool is_factor = !std::strcmp(rules[e.rule].name, "Factor");
    e.enabled = is_factor;
    if (is_factor)
      factor = &e;
  }
  assert(factor);
  assert(p.inprocess().run_phase(k, SlotGroup::Probe, nullptr) == 0);
  assert(factor->tm.dec_warmup == 1 && factor->tm.admitted == 0);

  // Model preprocessing retiring half the variables.  The current active
  // population is now below the ceiling, but this solve began in the large
  // class and must not acquire Factor's first 700M-tick dose later.
  k.stats.bve_eliminated = 4;
  k.factor_marks[make_lit(1, true)] = 1;
  assert(p.inprocess().run_phase(k, SlotGroup::Probe, nullptr) == 0);
  assert(factor->tm.dec_warmup == 2 && factor->tm.admitted == 0);
  std::printf("  factor large-formula gate: latched for the solve\n");
}

static void test_retier_schedule_advances_only_on_otfs_conflicts(void) {
  Kernel k;
  kernel_reserve_vars(k, 1);
  ScopedHeuristics h(k);
  DefaultPolicy p;
  p.reset_schedule(k);
  k.stats.conflicts = 3;
  assert(!p.update_tiers_after_analysis(k));
  assert(k.stats.retiered == 0 &&
         "ordinary learned conflicts must not advance Kissat's glue limit");
  AnalysisConflictCheckpoint checkpoint;
  checkpoint.conflicts = k.stats.conflicts;
  checkpoint.mode = 0;
  checkpoint.used_glue[3] = 1;
  k.analysis_conflict_checkpoints.push_back(checkpoint);
  assert(p.update_tiers_after_analysis(k));
  assert(k.stats.retiered == 1 &&
         "an OTFS re-analysis conflict must advance the glue limit");
  assert(k.analysis_conflict_checkpoints.empty() &&
         "policy must consume each internal observation exactly once");
  assert(!p.update_tiers_after_analysis(k));
  k.h->stable = true;
  k.tier1_by_mode[0] = 2;
  k.tier2_by_mode[0] = 6;
  k.tier1_by_mode[1] = 2;
  k.tier2_by_mode[1] = 5;
  checkpoint.conflicts = 70000;
  checkpoint.mode = 1;
  checkpoint.used_glue[3] = 1;
  k.analysis_conflict_checkpoints.push_back(checkpoint);
  assert(p.update_tiers_after_analysis(k));
  assert(k.tier1 == k.tier1_by_mode[1] &&
         k.tier2 == k.tier2_by_mode[1] &&
         "an internal stable-mode retier must be posed before OTFS resumes");
  std::printf("  retier cadence: only OTFS re-analysis advances glue limit\n");
}

// --- the resumption cursor, a policy object --------------------------------
// A dose cut partway into a candidate list must resume past what it consumed,
// or it re-applies the same prefix and starves the tail (kissat's
// `transitivekeep` bit serves the same end). The cursor lives in
// DefaultInprocessScheduler::SlotEntry: the rule enumerates its set, the policy decides
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
  DefaultInprocessScheduler::SlotEntry e;
  e.rule = static_cast<int>(v4->id);
  e.dosed = true;
  e.cursor_enabled = true;

  p.inprocess().order_candidates(k, e, buf.data(), n);
  assert(e.ord.rotated == 0 && "a fresh cursor must not rotate");
  assert(buf == native && "the native arm must leave the order alone");

  // Consume the first two candidates as the dose loop does, then re-enumerate.
  Lit consumed;
  assert(lit_of_instance(native[1], &consumed));
  e.cursor = static_cast<uint64_t>(consumed) + 1;
  buf = native;
  p.inprocess().order_candidates(k, e, buf.data(), n);
  assert(e.ord.rotated == 1 && "the cursor must rotate a resumed walk");
  assert(buf[0] != native[0] &&
         "the walk replayed its head: a truncated dose would starve the tail "
         "forever (SCHED-B3)");
  assert(buf[0] == native[2] && "resume at the first key past the cursor");
  assert(same_multiset(buf, native) && "rotation must be a PERMUTATION");

  // A cursor past the whole set wraps to the head rather than emptying it.
  e.cursor = UINT64_MAX;
  buf = native;
  p.inprocess().order_candidates(k, e, buf.data(), n);
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
  DefaultInprocessScheduler::SlotEntry e;
  e.rule = static_cast<int>(v4->id);
  e.dosed = true;
  e.cursor_enabled = true;
  std::vector<uint64_t> walk = before;
  Lit deep;
  assert(lit_of_instance(before[n / 2], &deep));
  e.cursor = static_cast<uint64_t>(deep); // resume mid-set
  p.inprocess().order_candidates(k, e, walk.data(), n);
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
    q.inprocess().candidate_order = &co;
    DefaultInprocessScheduler::SlotEntry qe;
    qe.rule = static_cast<int>(v4->id);
    qe.dosed = true;
    qe.order = arm;
    std::vector<uint64_t> out = before;
    q.inprocess().order_candidates(k, qe, out.data(), n);
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
    q.inprocess().candidate_order = &co;
    DefaultInprocessScheduler::SlotEntry qe;
    qe.rule = static_cast<int>(v4->id);
    qe.dosed = true;
    qe.order = arm;
    std::vector<uint64_t> out = before;
    q.inprocess().order_candidates(k, qe, out.data(), n);
    assert(qe.ord.ranked == 1 && qe.ord.moved > 0 &&
           "arm did not reorder anything -- it would be untestable");
  }
  {
    DefaultPolicy q; // no attached policy, arm stays Native
    DefaultInprocessScheduler::SlotEntry qe;
    qe.rule = static_cast<int>(v4->id);
    qe.dosed = true;
    std::vector<uint64_t> out = before;
    q.inprocess().order_candidates(k, qe, out.data(), n);
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

// A stable-mode OTFS retier poses its current-mode limits so conflict analysis
// can resume under them.  Kissat's shipped `vivifyfocusedtiers=1` nevertheless
// makes the later Vivify consumer read tier[focused], not whichever snapshot
// most recently occupied the shared live fields.
static void test_vivify_dose_poses_focused_tiers(void) {
  Kernel k;
  kernel_reserve_vars(k, 64);
  ScopedHeuristics h(k);
  k.h->stable = true;
  k.stats.conflicts = 1;

  const Lit clause[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, clause, 3, true, 20, 0);

  DefaultPolicy p;
  p.inprocess().build(k);
  DefaultInprocessScheduler::SlotEntry *vivify = nullptr;
  const auto &rules = tactix_rules();
  for (auto &e : p.inprocess().entries) {
    const bool selected = !std::strcmp(rules[e.rule].name, "Vivify");
    e.enabled = selected;
    if (selected)
      vivify = &e;
  }
  assert(vivify);

  k.tier1_by_mode[0] = 20;
  k.tier2_by_mode[0] = 38;
  k.tier1_by_mode[1] = 19;
  k.tier2_by_mode[1] = 46;
  k.tier1 = k.tier1_by_mode[1];
  k.tier2 = k.tier2_by_mode[1];

  assert(p.inprocess().run_phase(k, SlotGroup::Probe, nullptr) == 0);
  assert(vivify->tm.admitted > 0 && k.stats.vivify_rounds > 0 &&
         "the fixture must execute a real Vivify dose");
  assert(k.tier1 == k.tier1_by_mode[0] &&
         k.tier2 == k.tier2_by_mode[0] &&
         "Vivify consumed the last posed stable snapshot instead of Kissat's "
         "shipped focused tier snapshot");
  std::printf("  vivify tiers: stable retier -> focused 20/38 at dose entry\n");
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

static void test_default_slot_membership_matches_kissat(void) {
  Kernel k;
  kernel_reserve_vars(k, 4);
  DefaultPolicy p;
  p.inprocess().build(k);
  const auto &rules = tactix_rules();
  static constexpr const char *POSED_ONLY[] = {
      "HyperBinaryRes", "Probe", "HyperTernaryRes",
      "Instantiate",    "BCE",   "CCE",
  };
  for (const auto &entry : p.inprocess().entries) {
    const char *name = rules[entry.rule].name;
    bool posed_only = false;
    for (const char *candidate : POSED_ONLY)
      posed_only = posed_only || !std::strcmp(name, candidate);
    assert(entry.enabled != posed_only &&
           "production membership must be exactly the original-Kissat "
           "pipeline; non-Kissat proof rules remain explicitly poseable");
  }
  std::printf("  slot membership: original Kissat; six extra rules poseable\n");
}

// --- the two clocks are actually two ----------------------------------------
// kissat runs probing on probeint=100 x NLOGN (probe.c:85) and elimination on
// eliminateint=500 x NLOG2N (eliminate.c:600), so at the same phase count the
// probing clock must fire strictly sooner or the split has collapsed into one.
static void test_two_slot_clocks_diverge(void) {
  Kernel k;
  kernel_reserve_vars(k, 4);
  DefaultPolicy p;
  p.inprocess().pose_initialized();
  for (uint64_t n = 0; n < 8; n++) {
    p.inprocess().probe_clock.rounds = n;
    p.inprocess().elim_clock.rounds = n;
    p.inprocess().update_limit(k, SlotGroup::Probe);
    p.inprocess().update_limit(k, SlotGroup::Eliminate);
    assert(p.inprocess().probe_clock.next_conflicts < p.inprocess().elim_clock.next_conflicts &&
           "the probing clock is not firing sooner than the elimination clock "
           "-- the two clocks have collapsed into one (SCHED-D2)");
    // and the gap must widen: NLOG2N outgrows NLOGN.
    if (n) {
      p.inprocess().probe_clock.rounds = n - 1;
      p.inprocess().elim_clock.rounds = n - 1;
      p.inprocess().update_limit(k, SlotGroup::Probe);
      const uint64_t prev_probe = p.inprocess().probe_clock.next_conflicts;
      p.inprocess().update_limit(k, SlotGroup::Eliminate);
      const uint64_t prev_elim = p.inprocess().elim_clock.next_conflicts;
      p.inprocess().probe_clock.rounds = n;
      p.inprocess().elim_clock.rounds = n;
      p.inprocess().update_limit(k, SlotGroup::Probe);
      p.inprocess().update_limit(k, SlotGroup::Eliminate);
      assert(p.inprocess().elim_clock.next_conflicts - prev_elim >=
                 p.inprocess().probe_clock.next_conflicts - prev_probe &&
             "the elimination interval is not growing at least as fast as the "
             "probing one");
    }
  }
  std::printf("  slot clocks: probe next=%llu < eliminate next=%llu at 8 phases\n",
              (unsigned long long)p.inprocess().probe_clock.next_conflicts,
              (unsigned long long)p.inprocess().elim_clock.next_conflicts);
}

// Initial preprocessing consumes the probe pipeline only.  Kissat initializes
// the separate first-search elimination deadline from eliminateinit=500 with
// formula-size scaling (kimits.c:99-121), so it cannot fire at conflict one.
static void test_initial_elimination_deadline_matches_kissat(void) {
  Kernel k;
  kernel_reserve_vars(k, 12);
  const Lit a[3] = {internalize(1), internalize(2), internalize(3)};
  const Lit b[3] = {internalize(4), internalize(5), internalize(6)};
  kernel_add_large_clause(k, a, 3, false, 0);
  kernel_add_large_clause(k, b, 3, false, 0);
  DefaultPolicy p;
  p.inprocess().build(k);

  const double f =
      std::log10(static_cast<double>(kernel_irredundant_clauses(k)) + 1) - 5;
  const uint64_t delta = static_cast<uint64_t>(
      (4.5 * f * f + 25) *
      static_cast<double>(DefaultInprocessScheduler::SUBSUME_INTERVAL));
  assert(p.inprocess().probe_clock.next_conflicts == 0 &&
         "the probe clock must admit initial preprocessing");
  assert(p.inprocess().elim_clock.next_conflicts == delta &&
         "first-search elimination must start at scaled eliminateinit=500");
  k.stats.conflicts = 1;
  assert(!p.inprocess().want_phase(k, SlotGroup::Eliminate) &&
         "elimination must not fire immediately after conflict one");
  k.stats.conflicts = delta;
  assert(!p.inprocess().want_phase(k, SlotGroup::Eliminate) &&
         "Kissat enters only after, not at, the stored conflict limit");
  k.stats.conflicts++;
  assert(p.inprocess().want_phase(k, SlotGroup::Eliminate));
  std::printf("  initial elimination: waits past scaled conflict %llu\n",
              (unsigned long long)delta);
}

// kimits.c scales from BINIRR_CLAUSES, whose binary component is live.  The
// kernel's binary-id table is intentionally historical so proof deletions and
// later re-additions remain nameable; it must not leak into this scheduler
// statistic.
static void test_inprocess_scale_ignores_deleted_binary_ids(void) {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit a = internalize(1), b = internalize(2);
  kernel_add_binary_clause(k, a, b, 41);
  assert(kernel_irredundant_clauses(k) == 1);
  assert(kernel_delete_binary(k, a, b));
  assert(k.binary_ids.size() == 1 && "the proof identity remains archived");
  assert(kernel_irredundant_clauses(k) == 0 &&
         "scaled inprocessing deadlines count live binaries only");
}

static void test_congruence_preprocess_bumps_first_search_delay(void) {
  Kernel k;
  kernel_reserve_vars(k, 4);
  DefaultPolicy p;
  p.inprocess().build(k);
  const auto &rules = tactix_rules();
  DefaultInprocessScheduler::SlotEntry *congruence = nullptr;
  for (auto &e : p.inprocess().entries) {
    const bool selected = !std::strcmp(rules[e.rule].name, "Congruence");
    e.enabled = selected;
    if (selected)
      congruence = &e;
  }
  assert(congruence);
  assert(p.inprocess().run_phase(k, SlotGroup::Probe, nullptr) == 0);
  assert(congruence->tm.admitted == 1);
  assert(congruence->delay_current == 1 && congruence->delay_count == 1 &&
         "Kissat bumps an unproductive Congruence delay during preprocessing");

  // New formula material makes the closure watermark eligible, but the named
  // delay must be consumed before Congruence does any setup.
  kernel_add_binary_clause(k, internalize(1), internalize(2), 51);
  k.stats.conflicts = 1;
  assert(p.inprocess().run_phase(k, SlotGroup::Probe, nullptr) == 0);
  assert(congruence->tm.admitted == 1 && congruence->tm.dec_delay == 1);
  assert(congruence->delay_count == 0);
}

// --- the elapsed interval is necessary, not sufficient ----------------------
// kissat eliminate.c:33-38 and cadical elim.cpp:75-81 refuse an elimination
// phase with no new material even once its interval elapsed; cadical
// probe.cpp:23 refuses an inprobe phase with no reduce since the last one.
static void test_slot_gates_decline_without_new_material(void) {
  Kernel k;
  kernel_reserve_vars(k, 4);
  DefaultPolicy p;
  p.inprocess().pose_initialized();
  k.stats.conflicts = 100;
  // Past the first phase, intervals elapsed, marks == live: the interval says
  // go and the gates must still say no.
  p.inprocess().probe_clock.rounds = p.inprocess().elim_clock.rounds = 1;
  p.inprocess().probe_clock.next_conflicts = p.inprocess().elim_clock.next_conflicts = 0;
  p.inprocess().probe_clock.reductions_mark = k.stats.reductions;
  p.inprocess().elim_clock.marks_mark = k.stats.bve_marks;
  p.inprocess().elim_clock.fixed_mark = kernel_root_fixed(k);
  assert(!p.inprocess().want_phase(k, SlotGroup::Eliminate) &&
         "an elimination phase fired with no new material (SCHED-D3)");
  assert(!p.inprocess().want_phase(k, SlotGroup::Probe) &&
         "a probing phase fired with no reduce since the last one (SCHED-D3)");
  assert(p.inprocess().elim_clock.blocked_episodes == 1 &&
         p.inprocess().probe_clock.blocked_episodes == 1);
  // An episode is counted once, not once per search step.
  (void)p.inprocess().want_phase(k, SlotGroup::Eliminate);
  (void)p.inprocess().want_phase(k, SlotGroup::Probe);
  assert(p.inprocess().elim_clock.blocked_episodes == 1 &&
         p.inprocess().probe_clock.blocked_episodes == 1);
  // New elimination material re-admits; so does a reduce for probing.
  k.stats.bve_marks++;
  assert(p.inprocess().want_phase(k, SlotGroup::Eliminate));
  k.stats.reductions++;
  assert(p.inprocess().want_phase(k, SlotGroup::Probe));
  // The first phase is exempt: preprocessing has no previous phase to be new
  // relative to (cadical `stats.inprobingphases &&`).
  DefaultPolicy q;
  q.inprocess().pose_initialized();
  q.inprocess().probe_clock.next_conflicts = q.inprocess().elim_clock.next_conflicts = 0;
  q.inprocess().probe_clock.reductions_mark = k.stats.reductions;
  q.inprocess().elim_clock.marks_mark = k.stats.bve_marks;
  q.inprocess().elim_clock.fixed_mark = kernel_root_fixed(k);
  assert(q.inprocess().want_phase(k, SlotGroup::Probe) &&
         q.inprocess().want_phase(k, SlotGroup::Eliminate) &&
         "the preprocessing phase must not be gated");
  std::printf("  slot gates: decline without new material, re-admit with it\n");
}

// --- the effort budget is kissat's SET_EFFORT_LIMIT ------------------------
// kimits.h:135-170, and nothing else: REFERENCE = search_ticks since THIS
// PHASE's anchor, floored at mineffort*1e6; LIMIT = the technique's own counter
// now + effort/1000 * REFERENCE. No upper clamp (kissat ships no `maxeffort`)
// and no active floor (cadical's propagation scheme only). The anchor being the
// PHASE's is the load-bearing part: two entries in one phase must price against
// the same reference, so neither can earn allowance by abstaining.
static void test_effort_limit_is_kissats(void) {
  const RuleVTable *bve = find_rule("BVE");
  assert(bve);
  Kernel k;
  kernel_reserve_vars(k, 64);
  DefaultPolicy p;
  DefaultInprocessScheduler::SlotEntry e;
  e.rule = static_cast<int>(bve->id);
  e.permille = 100;
  e.group = SlotGroup::Eliminate;
  e.meter = [](const Kernel &) -> uint64_t { return 0; };

  // During search, the reference floors at MIN_EFFORT_TICKS, so the delta is
  // 1e7/1000*100 == 1e6. A large formula must NOT lift it — the active floor
  // that used to do so was cadical's and is gone.
  k.stats.conflicts = 1;
  k.unassigned = 400000;
  p.inprocess().set_entry_limit(e, k);
  assert(e.limit == 1000000 &&
         "no active floor: 2*active() must not raise a floored reference");

  // A second entry in the SAME phase, at a different rate, prices against the
  // same reference — the property a per-entry anchor cannot give.
  DefaultInprocessScheduler::SlotEntry e2 = e;
  e2.permille = 20;
  p.inprocess().set_entry_limit(e2, k);
  assert(e2.limit == 200000 && "same phase reference, rate is the only term");

  // The meter is added, not replaced: the limit is a ceiling on that counter.
  e.meter = [](const Kernel &) -> uint64_t { return 500000; };
  p.inprocess().set_entry_limit(e, k);
  assert(e.limit == 1500000 && "LIMIT = counter_now + delta (kimits.h:162)");
  std::printf("  effort limit: kissat's formula, phase anchor, no clamps\n");
}

static void test_preprocess_is_reduced_and_bounded(void) {
  Kernel k;
  kernel_reserve_vars(k, 64);
  DefaultPolicy p;
  p.inprocess().build(k);
  std::vector<const char *> offered;
  const auto &rules = tactix_rules();
  for (const auto &e : p.inprocess().entries)
    if (e.preprocess)
      offered.push_back(rules[e.rule].name);
  static const char *const expected[] = {
      "Congruence", "Substitute", "Backbone",
      "Sweep", "Substitute", "Factor"};
  assert(offered.size() == std::size(expected));
  for (size_t i = 0; i < offered.size(); i++)
    assert(!std::strcmp(offered[i], expected[i]));
  std::vector<FollowupKind> followups;
  for (const auto &e : p.inprocess().entries)
    if (e.preprocess && !std::strcmp(rules[e.rule].name, "Substitute"))
      followups.push_back(e.followup);
  assert(followups.size() == 2);
  assert(followups[0] == FollowupKind::AfterCongruence);
  assert(followups[1] == FollowupKind::AfterSweepOrFinal);
  assert(p.inprocess().want_phase(k, SlotGroup::Probe));
  assert(!p.inprocess().want_phase(k, SlotGroup::Eliminate));

  DefaultInprocessScheduler::SlotEntry e;
  e.rule = static_cast<int>(find_rule("Backbone")->id);
  e.group = SlotGroup::Probe;
  e.permille = 20;
  e.meter = [](const Kernel &) -> uint64_t { return 7; };
  p.inprocess().set_entry_limit(e, k);
  assert(e.limit == 200007 &&
         "preprocessing must use 20 permille of Kissat mineffort=10m");

  // Factor is the deliberate exception: kissat factor.c uses
  // `factoriniticks=700` on its first invocation, and that invocation is the
  // preprocessing call in probe_initially. It must not fall through to the
  // ordinary 50-permille preprocessing allowance.
  e.rule = static_cast<int>(find_rule("Factor")->id);
  e.permille = 50;
  e.budget_aware = true;
  p.inprocess().set_entry_limit(e, k);
  assert(e.limit ==
             DefaultStrategyConfig::FACTOR_INITIAL_EFFORT_DEFAULT + 7 &&
         "preprocessing Factor must receive Kissat's first-dose allowance");
  std::printf("  preprocess: one reduced probe pipeline, 10m tick reference\n");
}

static void test_preprocess_followup_truth_table(void) {
  using M = FollowupMode;
  using K = FollowupKind;
  const auto mode = DefaultInprocessScheduler::preprocessing_followup_mode;
  assert(mode(K::AfterCongruence, false, false) == M::None);
  assert(mode(K::AfterSweepOrFinal, false, false) == M::Final);
  assert(mode(K::AfterCongruence, true, false) == M::Eager);
  assert(mode(K::AfterSweepOrFinal, true, false) == M::None);
  assert(mode(K::AfterCongruence, false, true) == M::None);
  assert(mode(K::AfterSweepOrFinal, false, true) == M::Eager);
  assert(mode(K::AfterCongruence, true, true) == M::Eager);
  assert(mode(K::AfterSweepOrFinal, true, true) == M::Eager);
  std::printf("  preprocess followups: all four producer outcomes covered\n");
}

static void test_preprocess_final_followup_is_observed(void) {
  Kernel k;
  kernel_reserve_vars(k, 4);
  DefaultPolicy p;
  p.inprocess().build(k);
  assert(p.inprocess().run_phase(k, SlotGroup::Probe, nullptr) == 0);
  const auto &rules = tactix_rules();
  unsigned seen = 0;
  for (const auto &e : p.inprocess().entries) {
    if (!e.preprocess || std::strcmp(rules[e.rule].name, "Substitute"))
      continue;
    if (seen++ == 0) {
      assert(e.tm.followup_offered == 1 && e.tm.followup_taken == 0);
    } else {
      assert(e.tm.followup_offered == 1 && e.tm.followup_taken == 1);
      assert(e.tm.followup_eager == 0 && e.tm.followup_final == 1);
    }
  }
  assert(seen == 2);
  std::printf("  preprocess followups: neither-producer path takes final once\n");
}

static void test_inprocess_does_not_save_probe_phases(void) {
  Kernel k;
  ScopedHeuristics h(k);
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(3));

  DefaultStrategyConfig cfg;
  cfg.inprocess.bve_occurrence_limit = 37;
  DefaultPolicy p(cfg);
  p.inprocess().build(k);
  const auto &rules = tactix_rules();
  for (auto &e : p.inprocess().entries)
    e.enabled = !std::strcmp(rules[e.rule].name, "Backbone");
  assert(p.inprocess().run_phase(k, SlotGroup::Probe, nullptr) == 0);
  for (Var v = 0; v < k.nvars; v++) {
    assert(k.h->saved[v] == 0 &&
           "Kissat probing assignments must not become saved phases");
    assert(!negated(heuristics_decide_lit(k, v)) &&
           "zero saved phase must fall back to positive initial phase");
  }
  assert(!k.phase_saving_suppressed && !k.inprocessing);
  assert(k.bve_occurrence_limit == 37 &&
         "DefaultPolicy must wire the named BVE occurrence limit");
  std::printf("  inprocess phases: probe assignments leave saved phases unset\n");
}

static void test_jump_reason_formula_classification(void) {
  const auto add_chain = [](Kernel &k) {
    kernel_reserve_vars(k, 3);
    const Lit p = internalize(1), q = internalize(2), r = internalize(3);
    kernel_add_binary_clause(k, p, q);
    kernel_add_binary_clause(k, negate(q), r);
    kernel_assign_decision(k, negate(p));
  };

  // Kissat classifies every formula with at most `smallclauses` clauses as
  // not-bigbig, even if every clause is binary.  Ordinary search propagation
  // must therefore retain the direct antecedent on this tiny instance.
  Kernel small;
  add_chain(small);
  DefaultPolicy default_policy;
  default_policy.update_formula_classification(small);
  assert(!small.jump_reasons_bigbig);
  assert(propagate_apply(small) == ApplyResult::Ok);
  const Var small_r = var_of(internalize(3));
  assert(!small.assigned[small_r].jumped);
  assert(small.assigned[small_r].reason == internalize(-2));
  assert(small.stats.jumped_reasons == 0);

  // Lower only the named size limit to exercise the binary-fraction arm on a
  // fixed micro-fixture.  This avoids a 100k-clause regression test while
  // pinning the exact same classification-dependent propagation behavior.
  DefaultStrategyConfig config;
  config.analysis.small_clause_limit = 0;
  Kernel large;
  add_chain(large);
  DefaultPolicy classified_policy(config);
  classified_policy.update_formula_classification(large);
  assert(large.jump_reasons_bigbig);
  assert(propagate_apply(large) == ApplyResult::Ok);
  const Var large_r = var_of(internalize(3));
  assert(large.assigned[large_r].jumped);
  assert(large.assigned[large_r].reason == internalize(1));
  assert(large.stats.jumped_reasons == 1);
  assert(large.stats.jump_reason_classifications == 1);
  assert(large.stats.jump_reason_bigbig == 1);
  std::printf("  jump reasons: Kissat small/bigbig classification gates search\n");
}

static void test_preprocess_followup_legacy_arm(void) {
  assert(setenv("TACTIX_PREPROCESS_FOLLOWUPS", "0", 1) == 0);
  Kernel k;
  kernel_reserve_vars(k, 4);
  DefaultPolicy p;
  p.inprocess().configure_from_env();
  p.inprocess().build(k);
  assert(p.inprocess().run_phase(k, SlotGroup::Probe, nullptr) == 0);
  assert(unsetenv("TACTIX_PREPROCESS_FOLLOWUPS") == 0);
  const auto &rules = tactix_rules();
  unsigned seen = 0;
  for (const auto &e : p.inprocess().entries)
    if (e.preprocess && !std::strcmp(rules[e.rule].name, "Substitute")) {
      seen++;
      assert(e.applies == 1);
      assert(e.tm.followup_offered == 0 && e.tm.followup_taken == 0);
    }
  assert(seen == 2);
  std::printf("  preprocess followups: legacy arm calls both entries\n");
}

static void test_reorder_ships_in_both_modes(void) {
  Kernel k;
  kernel_reserve_vars(k, 4);
  ScopedHeuristics h(k);
  DefaultPolicy p;
  k.stats.conflicts = DefaultPolicy::REORDER_INTERVAL_INIT;
  assert(p.want_reorder(k) &&
         "Kissat's shipped reorder=2 default must admit focused mode");
  k.h->stable = true;
  assert(p.want_reorder(k) &&
         "Kissat's shipped reorder=2 default must admit stable mode");
  k.level = 1;
  assert(!p.want_reorder(k) && "the scheduled event remains root-only");
  std::printf("  reorder default: both modes, policy trigger remains root-only\n");
}

static void test_startup_sparse_collection_precedes_probe(void) {
  Kernel k;
  kernel_reserve_vars(k, 4);
  kernel_assign_unit(k, internalize(1));
  const Lit satisfied[3] = {internalize(1), internalize(2),
                            internalize(4)};
  const Lit shortened[4] = {internalize(-1), internalize(2),
                            internalize(3), internalize(4)};
  kernel_add_large_clause(k, satisfied, 3, false, 0);
  kernel_add_large_clause(k, shortened, 4, false, 0);
  const Lit unsat[4][2] = {
      {internalize(2), internalize(3)},
      {internalize(2), internalize(-3)},
      {internalize(-2), internalize(3)},
      {internalize(-2), internalize(-3)},
  };
  for (const auto &binary : unsat)
    kernel_add_binary_clause(k, binary[0], binary[1]);

  DefaultPolicy p;
  assert(p.solve(k) == 20);
  assert(k.stats.sparse_collections == 1);
  // The sparse collector removes the satisfied large clause, rewrites the
  // shortened one, and drops the three root-satisfied binary pairs first.
  assert(k.stats.sparse_collected == 5);
  assert(k.stats.strengthen_rounds == 0 &&
         "startup collection is scaffolding, not an authored C2 slot");
}

int main() {
  test_mode_growth_key_matches_kissat();
  test_reason_bump_depth_matches_kissat();
  test_reason_bump_delay_lives_at_final_clause_gate();
  test_analysis_trail_reuse_defaults_to_kissat();
  test_factor_material_gate_uses_literal_marks();
  test_large_formula_factor_decline_is_solve_latched();
  test_retier_schedule_advances_only_on_otfs_conflicts();
  test_effort_limit_is_kissats();
  test_preprocess_is_reduced_and_bounded();
  test_preprocess_followup_truth_table();
  test_preprocess_final_followup_is_observed();
  test_inprocess_does_not_save_probe_phases();
  test_jump_reason_formula_classification();
  test_preprocess_followup_legacy_arm();
  test_reorder_ships_in_both_modes();
  test_startup_sparse_collection_precedes_probe();
  test_slot_cursor_rotates();
  test_cursor_preserves_n1();
  test_order_arms_reorder();
  test_subsume_meters_partner_visits();
  test_vivify_scope_tracks_tier2();
  test_vivify_dose_poses_focused_tiers();
  test_authored_slot_order_covers_registry();
  test_default_slot_membership_matches_kissat();
  test_two_slot_clocks_diverge();
  test_initial_elimination_deadline_matches_kissat();
  test_inprocess_scale_ignores_deleted_binary_ids();
  test_congruence_preprocess_bumps_first_search_delay();
  test_slot_gates_decline_without_new_material();
  std::printf("test_scheduling: all passed\n");
  return 0;
}
