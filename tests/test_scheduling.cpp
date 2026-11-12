// Scheduling regression tests: each pins a Kissat/CaDiCaL reference formula
// or an N1 obligation.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "rules/reduce/reduce.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/registry.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace tactix;

static const RuleVTable *find_rule(const char *name) {
  for (const RuleVTable &r : tactix_rules())
    if (!std::strcmp(r.name, name))
      return &r;
  return nullptr;
}

// Builds p's slots for k and enables only the entries of `name` (and `also`);
// returns the last `name` entry.
static DefaultInprocessScheduler::SlotEntry *
enable_only(DefaultPolicy &p, Kernel &k, const char *name,
            const char *also = nullptr) {
  p.inprocess().build(k);
  DefaultInprocessScheduler::SlotEntry *last = nullptr;
  for (auto &e : p.inprocess().entries) {
    const char *rule = tactix_rules()[e.rule].name;
    e.enabled = !std::strcmp(rule, name) || (also && !std::strcmp(rule, also));
    if (!std::strcmp(rule, name))
      last = &e;
  }
  assert(last);
  return last;
}

// Kissat mode.c:90, kimits.c:27: focused interval = modeint * nlogpown(count/2, 4).
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
  assert(first == DefaultStrategyConfig::MODE_INTERVAL_DEFAULT &&
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
        DefaultStrategyConfig::MODE_INTERVAL_DEFAULT * key * l * l * l * l);
    const uint64_t got = p.mode.conflicts_limit - k.stats.conflicts;
    assert(got == want && "focused interval must be modeint * nlogpown(c/2, 4)");
  }
}

// Kissat bumpreasons: direct reasons only, bumpreasonsrate=10.
static void test_reason_bump_defaults_match_kissat(void) {
  DefaultPolicy p;
  assert(p.strategy_config().analysis.reason_depth == 1);
  assert(DefaultStrategyConfig::BUMP_REASON_RATE_DEFAULT == 10.0);
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
}

static void test_factor_material_gate_uses_literal_marks(void) {
  Kernel k;
  kernel_reserve_vars(k, 8);
  k.stats.conflicts = 1; // search-time call: the material gate is live
  std::fill(k.factor_marks.begin(), k.factor_marks.end(), 0);
  k.factor_literals_marked = 0;

  DefaultPolicy p;
  DefaultInprocessScheduler::SlotEntry *factor = enable_only(p, k, "Factor");
  assert(p.inprocess().run_phase(k, SlotGroup::Probe, nullptr) == 0);
  assert(factor->tm.dec_material == 1 && factor->tm.admitted == 0);

  // Pose unchanged coarse watermarks.  A newly marked polarity must still
  // admit Factor, as Kissat's `flags->factor` / `literals_factor` wire does.
  factor->binaries_mark = k.binary_ids.size();
  factor->arena_mark = k.arena.size();
  factor->root_fixed_mark = kernel_root_fixed(k);
  kernel_mark_factor_literal(k, make_lit(3, true));
  kernel_mark_factor_literal(k, make_lit(3, true));
  assert(k.factor_literals_marked == 1 &&
         "only a factor-bit zero-to-one transition advances the counter");
  assert(p.inprocess().run_phase(k, SlotGroup::Probe, nullptr) == 0);
  assert(factor->tm.dec_material == 1 && factor->tm.admitted == 1);
  // With no later addition, Kissat's limits.factor.marked suppresses the next call.
  assert(p.inprocess().run_phase(k, SlotGroup::Probe, nullptr) == 0);
  assert(factor->tm.dec_material == 2 && factor->tm.admitted == 1);
}

static void test_large_formula_factor_decline_is_solve_latched(void) {
  Kernel k;
  kernel_reserve_vars(k, 8);
  DefaultStrategyConfig cfg;
  cfg.inprocess.factor_active_ceiling = 6;
  cfg.inprocess.structured_root_stages.clear(); // isolate the Factor gate
  DefaultPolicy p(cfg);
  DefaultInprocessScheduler::SlotEntry *factor = enable_only(p, k, "Factor");
  assert(p.inprocess().run_phase(k, SlotGroup::Probe, nullptr) == 0);
  assert(factor->tm.dec_warmup == 1 && factor->tm.admitted == 0);

  // Retiring half the variables drops below the ceiling, but the solve stays
  // in the large class.
  k.stats.bve_eliminated = 4;
  k.factor_marks[make_lit(1, true)] = 1;
  assert(p.inprocess().run_phase(k, SlotGroup::Probe, nullptr) == 0);
  assert(factor->tm.dec_warmup == 2 && factor->tm.admitted == 0);
}

static void test_retier_schedule_advances_only_on_otfs_conflicts(void) {
  // The retier schedule step itself: deadline, geometric interval, pose.
  Kernel k;
  kernel_reserve_vars(k, 1);
  ScopedHeuristics h(k);
  DefaultPolicy p;
  p.reset_schedule(k);
  RetierSchedule tiers = p.retier_schedule();
  assert(tiers.enabled && tiers.next_conflicts == 2 && tiers.interval == 2);
  k.stats.conflicts = 2;
  assert(!reduce_retier_at_conflict(k, tiers, k.h->stable ? 1u : 0u) && k.stats.retiered == 0 &&
         "the deadline itself must not retier");
  k.stats.conflicts = 3;
  k.stats.used_glue[0][3] = 1;
  assert(reduce_retier_at_conflict(k, tiers, k.h->stable ? 1u : 0u));
  assert(k.stats.retiered == 1 && tiers.interval == 4 &&
         tiers.next_conflicts == 7 && "an OTFS conflict past the deadline "
                                      "retiers and grows the interval");
  assert(!reduce_retier_at_conflict(k, tiers, k.h->stable ? 1u : 0u));
  k.h->stable = true;
  k.stats.conflicts = 70000;
  k.stats.used_glue[1][3] = 1;
  assert(reduce_retier_at_conflict(k, tiers, k.h->stable ? 1u : 0u));
  assert(k.tier1 == tiers.tier1_by_mode[1] &&
         k.tier2 == tiers.tier2_by_mode[1] &&
         "an internal stable-mode retier must be posed before OTFS resumes");
  RetierSchedule disabled = tiers;
  disabled.enabled = false;
  k.stats.conflicts = 1u << 30;
  assert(!reduce_retier_at_conflict(k, disabled, 1));
}

// A truncated dose resumes past the consumed prefix (Kissat transitivekeep);
// the rule enumerates the set, the policy's cursor picks the start.
static void build_transitive_fan_fixture(Kernel &k) {
  // A source needs out-degree >= 2 and a destination above it, so ten fans
  // rather than a chain, which would give every literal out-degree 1.
  kernel_reserve_vars(k, 128);
  for (uint32_t s = 0; s < 10; s++)
    for (uint32_t t = 0; t < 4; t++)
      kernel_add_binary_clause(k, make_lit(s, true),
                               make_lit(20 + s * 4 + t, true));
}

// Orders `out` for `rule` under a fresh policy whose candidate-order hook
// selects `arm`; returns the slot entry with its ordering telemetry.
static DefaultInprocessScheduler::SlotEntry
order_with_arm(const Kernel &k, const RuleVTable *rule, CandidateOrder arm,
               std::vector<uint64_t> &out) {
  DefaultCandidateOrder co;
  co.arms.assign(tactix_rules().size(), CandidateOrder::Native);
  co.arms[rule->id] = arm;
  DefaultPolicy q;
  q.inprocess().candidate_order = &co;
  DefaultInprocessScheduler::SlotEntry qe;
  qe.rule = static_cast<int>(rule->id);
  qe.dosed = true;
  qe.order = arm;
  q.inprocess().order_candidates(k, qe, out.data(),
                                 static_cast<uint32_t>(out.size()));
  return qe;
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
}

// N1: the cursor moves the start, never the set, and every order arm permutes.
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
    std::vector<uint64_t> out = before;
    order_with_arm(k, v4, arm, out);
    assert(same_multiset(out, before) && candidate_order_name(arm) &&
           "an order arm must be a PERMUTATION, never a filter");
  }
}

// Every arm moves something on a non-constant key; Native moves nothing.
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
    std::vector<uint64_t> out = before;
    const auto qe = order_with_arm(k, v4, arm, out);
    assert(qe.ord.ranked == 1 && qe.ord.moved > 0 &&
           "arm did not reorder anything -- it would be untestable");
  }
  {
    // Kissat transitive.c sorts by activity of IDX(IDX(lit)) and pops from the back.
    size_t best = 0;
    Lit best_lit;
    assert(lit_of_instance(k, before[best], &best_lit));
    for (size_t i = 1; i < before.size(); i++) {
      Lit lit;
      assert(lit_of_instance(k, before[i], &lit));
      const Var proxy = var_of(lit) >> 1;
      const Var best_proxy = var_of(best_lit) >> 1;
      const double score = k.h->scores.score[proxy];
      const double best_score = k.h->scores.score[best_proxy];
      if (score > best_score ||
          (score == best_score && var_of(lit) < var_of(best_lit))) {
        best = i;
        best_lit = lit;
      }
    }
    DefaultCandidateOrder co;
    co.arms.assign(tactix_rules().size(), CandidateOrder::Native);
    co.arms[v4->id] = CandidateOrder::Heuristic;
    DefaultPolicy q;
    q.inprocess().candidate_order = &co;
    DefaultInprocessScheduler::SlotEntry qe;
    qe.rule = static_cast<int>(v4->id);
    qe.dosed = true;
    qe.order = CandidateOrder::Heuristic;
    std::vector<uint64_t> out = before;
    q.inprocess().order_candidates(k, qe, out.data(), n);
    Lit out_lit;
    assert(lit_of_instance(k, out.front(), &out_lit));
    assert(k.h->scores.score[var_of(out_lit) >> 1] ==
               k.h->scores.score[var_of(best_lit) >> 1] &&
           "TransitiveReduce must consume a maximum reference activity pose");

    // The second IDX applies to Kissat's compact import index, so the policy maps
    // through the compact import permutation.
    std::vector<Var> compact_order(k.nvars);
    std::vector<Var> compact_position(k.nvars);
    for (Var i = 0; i < k.nvars; i++)
      compact_order[i] = (37u * i) % k.nvars;
    for (Var i = 0; i < k.nvars; i++)
      compact_position[compact_order[i]] = i;
    k.compact_import_order = compact_order;
    for (Var v = 0; v < k.nvars; v++)
      k.h->scores.score[v] = static_cast<double>((v * 3571u) % 2053u);
    auto better_reference = [&](uint64_t a, uint64_t b) {
      Lit al, bl;
      assert(lit_of_instance(a, &al) && lit_of_instance(b, &bl));
      const Var ac = compact_position[var_of(al)];
      const Var bc = compact_position[var_of(bl)];
      const double as = k.h->scores.score[compact_order[ac >> 1]];
      const double bs = k.h->scores.score[compact_order[bc >> 1]];
      if (as != bs)
        return as > bs;
      if ((ac >> 1) != (bc >> 1))
        return (ac >> 1) < (bc >> 1);
      return ac < bc;
    };
    size_t mapped_best = 0;
    for (size_t i = 1; i < before.size(); i++)
      if (better_reference(before[i], before[mapped_best]))
        mapped_best = i;
    out = before;
    q.inprocess().order_candidates(k, qe, out.data(), n);
    Lit mapped_out_lit, mapped_best_lit;
    assert(lit_of_instance(k, out.front(), &mapped_out_lit));
    assert(lit_of_instance(k, before[mapped_best], &mapped_best_lit));
    const Var mapped_out_compact = compact_position[var_of(mapped_out_lit)];
    const Var mapped_best_compact = compact_position[var_of(mapped_best_lit)];
    assert(k.h->scores.score[compact_order[mapped_out_compact >> 1]] ==
               k.h->scores.score[compact_order[mapped_best_compact >> 1]] &&
           "TransitiveReduce must resolve IDX through the compact import "
           "permutation before reading activity");
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
}

// CaDiCaL subsume.cpp:86: subchecks counts partner visits, not candidates.
static void test_subsume_meters_partner_visits(void) {
  Kernel k;
  kernel_reserve_vars(k, 8);
  // All 4-subsets of eight literals: 35 clauses sharing literals with partners.
  for (uint32_t a = 0; a < 5; ++a)
    for (uint32_t b = a + 1; b < 6; ++b)
      for (uint32_t c = b + 1; c < 7; ++c)
        for (uint32_t d = c + 1; d < 8; ++d) {
          const Lit c4[4] = {make_lit(a, true), make_lit(b, true),
                             make_lit(c, true), make_lit(d, true)};
          kernel_add_large_clause(k, c4, 4, false, 0, 0);
        }

  const RuleVTable *c1 = find_rule("SubsumeForward");
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
}

// Kissat vivify.c:132: vivifyfocusedtiers=1 reads tier2, so Vivify's redundant
// scope tracks Kernel::tier2.
static void test_vivify_scope_tracks_tier2(void) {
  const RuleVTable *viv = find_rule("Vivify");
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
         "the dynamic Vivify tier is unwired again (SCHED-C1)");
}

// Kissat vivifyfocusedtiers=1: a Vivify dose reads the focused tier snapshot
// even after a stable-mode OTFS retier.
static void test_vivify_dose_poses_focused_tiers(void) {
  Kernel k;
  kernel_reserve_vars(k, 64);
  ScopedHeuristics h(k);
  k.h->stable = true;
  k.stats.conflicts = 1;

  const Lit clause[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, clause, 3, true, 20, 0);

  DefaultPolicy p;
  DefaultInprocessScheduler::SlotEntry *vivify = enable_only(p, k, "Vivify");

  RetierSchedule tiers;
  tiers.tier1_by_mode[0] = 20;
  tiers.tier2_by_mode[0] = 38;
  tiers.tier1_by_mode[1] = 19;
  tiers.tier2_by_mode[1] = 46;
  p.inprocess().set_retier_schedule(&tiers);
  k.tier1 = tiers.tier1_by_mode[1];
  k.tier2 = tiers.tier2_by_mode[1];

  assert(p.inprocess().run_phase(k, SlotGroup::Probe, nullptr) == 0);
  assert(vivify->tm.admitted > 0 && k.stats.vivify_rounds > 0 &&
         "the fixture must execute a real Vivify dose");
  assert(k.tier1 == tiers.tier1_by_mode[0] &&
         k.tier2 == tiers.tier2_by_mode[0] &&
         "Vivify consumed the last posed stable snapshot instead of Kissat's "
         "shipped focused tier snapshot");

  Kernel selected;
  kernel_reserve_vars(selected, 64);
  ScopedHeuristics selected_h(selected);
  selected.h->stable = true;
  selected.stats.conflicts = 1;
  kernel_add_large_clause(selected, clause, 3, true, 20, 0);
  DefaultStrategyConfig config;
  config.inprocess.vivify_tier_mode = 1;
  DefaultPolicy selected_policy(config);
  DefaultInprocessScheduler::SlotEntry *selected_vivify =
      enable_only(selected_policy, selected, "Vivify");
  selected_policy.inprocess().set_retier_schedule(&tiers);
  assert(selected_policy.inprocess().run_phase(
             selected, SlotGroup::Probe, nullptr) == 0);
  assert(selected_vivify->tm.admitted > 0 &&
         selected.tier1 == tiers.tier1_by_mode[1] &&
         selected.tier2 == tiers.tier2_by_mode[1] &&
         "typed Vivify tier mode must select the requested snapshot");
}

// Kissat tier limits are cumulative: unused tier-1/tier-2 shares go to tier 3.
static void test_vivify_dose_carries_early_tier_budget(void) {
  Kernel k;
  kernel_reserve_vars(k, 160);
  ScopedHeuristics h(k);
  k.stats.conflicts = 1;
  for (uint32_t clause = 0; clause < 8; clause++) {
    Lit lits[20];
    for (uint32_t i = 0; i < 20; i++)
      lits[i] = make_lit(clause * 20 + i, true);
    kernel_add_large_clause(k, lits, 20, true, 20, 0);
  }

  DefaultStrategyConfig cfg;
  cfg.inprocess.minimum_effort_ticks = 1000;
  cfg.inprocess.phase_rounds = 1;
  cfg.inprocess.vivify_permille = 1000;
  DefaultPolicy p(cfg);
  enable_only(p, k, "Vivify");

  assert(p.inprocess().run_phase(k, SlotGroup::Probe, nullptr) == 0);
  assert(k.stats.vivify_tier3 == 8 &&
         "tier 3 lost the unused cumulative tier-1/tier-2 allowance");
}

// Equal tier-1/tier-2 limits fold tier 2 into tier 1 (vivify.c:kissat_vivify).
static void test_vivify_dose_merges_tier2_into_tier1(void) {
  Kernel k;
  kernel_reserve_vars(k, 160);
  ScopedHeuristics h(k);
  k.stats.conflicts = 1;
  for (uint32_t clause = 0; clause < 8; clause++) {
    Lit lits[20];
    for (uint32_t i = 0; i < 20; i++)
      lits[i] = make_lit(clause * 20 + i, true);
    kernel_add_large_clause(k, lits, 20, true, 2, 0);
  }
  RetierSchedule tiers;
  tiers.tier1_by_mode[0] = tiers.tier2_by_mode[0] = 2;
  tiers.tier1_by_mode[1] = tiers.tier2_by_mode[1] = 2;
  k.tier1 = k.tier2 = 2;

  DefaultStrategyConfig cfg;
  cfg.inprocess.minimum_effort_ticks = 1000;
  cfg.inprocess.phase_rounds = 1;
  cfg.inprocess.vivify_permille = 1000;
  DefaultPolicy p(cfg);
  enable_only(p, k, "Vivify");
  p.inprocess().set_retier_schedule(&tiers);

  assert(p.inprocess().run_phase(k, SlotGroup::Probe, nullptr) == 0);
  assert(k.stats.vivify_rounds == 3 &&
         "equal tier limits must fold tier 2 into tier 1 without a round");
  assert(k.stats.vivify_checked == 8);
}

// Kissat counts a pure subsumption as Vivify irredundant productivity.
static void test_vivify_subsumption_reduces_irredundant_delay(void) {
  Kernel k;
  kernel_reserve_vars(k, 3);
  ScopedHeuristics h(k);
  k.stats.conflicts = 1;
  const Lit clause[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, clause, 3, false, 0);
  kernel_add_binary_clause(k, internalize(1), internalize(2));

  DefaultStrategyConfig cfg;
  cfg.inprocess.minimum_effort_ticks = 1000;
  cfg.inprocess.phase_rounds = 1;
  cfg.inprocess.vivify_permille = 1000;
  DefaultPolicy p(cfg);
  DefaultInprocessScheduler::SlotEntry *vivify = enable_only(p, k, "Vivify");

  assert(p.inprocess().run_phase(k, SlotGroup::Probe, nullptr) == 0);
  assert(k.stats.vivify_subsumed == 1 && k.stats.vivify_shrunken == 0);
  assert(vivify->delay_current == 0 && vivify->delay_count == 0 &&
         "a successful subsumption was omitted from Vivify productivity");
}

// Kissat rates Sweep by kissat_average(eliminated, swept) over swept variables,
// not candidate pairs.
static void test_sweep_delay_rates_per_swept_variable(void) {
  Kernel k;
  kernel_reserve_vars(k, 5);
  ScopedHeuristics h(k);
  k.stats.conflicts = 1;
  const auto equate = [&](int left, int right, int helper) {
    for (uint32_t mask = 0; mask < 4; mask++) {
      const int sign = (mask & 1) ? 1 : -1;
      const int hl = (mask & 2) ? helper : -helper;
      const Lit clause[3] = {internalize(-sign * left),
                             internalize(sign * right), internalize(hl)};
      kernel_add_large_clause(k, clause, 3, false, 0);
    }
  };
  equate(1, 2, 4);
  equate(2, 3, 5);
  // Ternaries that force nothing give each seed several candidate pairs.
  kernel_reserve_vars(k, 17);
  for (int v = 6; v < 17; v++) {
    const Lit a[3] = {internalize(1), internalize(v), internalize(-(v + 1 > 16 ? 6 : v + 1))};
    const Lit b[3] = {internalize(-2), internalize(-v), internalize(v + 1 > 16 ? 6 : v + 1)};
    const Lit c[3] = {internalize(3), internalize(v), internalize(v + 1 > 16 ? 6 : v + 1)};
    kernel_add_large_clause(k, a, 3, false, 0);
    kernel_add_large_clause(k, b, 3, false, 0);
    kernel_add_large_clause(k, c, 3, false, 0);
  }

  DefaultStrategyConfig cfg;
  cfg.inprocess.minimum_effort_ticks = 1000000;
  cfg.inprocess.phase_rounds = 1;
  cfg.inprocess.sweep_permille = 1000;
  // A threshold no per-pair rate reaches while the per-variable rate does.
  cfg.inprocess.sweep_productivity_permille = 600;
  DefaultPolicy p(cfg);
  DefaultInprocessScheduler::SlotEntry *sweep =
      enable_only(p, k, "Sweep", "Substitute");
  assert(p.inprocess().run_phase(k, SlotGroup::Probe, nullptr) == 0);
  const uint64_t yield = k.stats.sweep_equivalences + k.stats.sweep_units;
  assert(yield >= 2 && "the chain x1 = x2 = x3 was not proved");
  assert(k.stats.sweep_swept >= 1 &&
         k.stats.sweep_candidates > k.stats.sweep_swept);
  const bool productive_per_variable =
      yield * 1000 >= 600 * k.stats.sweep_swept;
  const bool productive_per_pair =
      yield * 1000 >= 600 * k.stats.sweep_candidates;
  assert(productive_per_variable && !productive_per_pair &&
         "fixture no longer separates the two denominators");
  assert(sweep->delay_current == 0 && sweep->delay_count == 0 &&
         "Sweep's ladder must rate yield per swept variable, as kissat does");
}

static void add_substitute_second_round_fixture(Kernel &k) {
  kernel_reserve_vars(k, 5);
  // Round one rewrites x2 to x1, exposing x1 == x3 for the optional round two.
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(1));
  const Lit a[3] = {internalize(1), internalize(-3), internalize(2)};
  const Lit b[3] = {internalize(-1), internalize(3), internalize(-2)};
  kernel_add_large_clause(k, a, 3, false, 0);
  kernel_add_large_clause(k, b, 3, false, 0);
}

static DefaultInprocessScheduler::SlotEntry *
enable_only_first_substitute(DefaultPolicy &p, Kernel &k) {
  p.inprocess().build(k);
  const auto &rules = tactix_rules();
  DefaultInprocessScheduler::SlotEntry *first = nullptr;
  for (auto &e : p.inprocess().entries) {
    const bool substitute = !std::strcmp(rules[e.rule].name, "Substitute");
    e.enabled = substitute && !first;
    if (e.enabled)
      first = &e;
  }
  assert(first);
  assert(!first->closure &&
         "Kissat has no Substitute material or productivity-delay gate");
  return first;
}

static void test_substitute_budget_is_per_round_and_per_call(void) {
  Kernel k;
  ScopedHeuristics h(k);
  add_substitute_second_round_fixture(k);
  k.stats.conflicts = 1;
  k.stats.search_ticks = 1000000000;
  DefaultStrategyConfig cfg;
  cfg.inprocess.phase_rounds = 1;
  cfg.inprocess.substitute_rounds = 2;
  DefaultPolicy p(cfg);
  auto *const substitute = enable_only_first_substitute(p, k);

  assert(p.inprocess().run_phase(k, SlotGroup::Probe, nullptr) == 0);
  assert(substitute->tm.admitted == 1 && substitute->tm.dec_budget == 0);
  assert(k.stats.substitute_rounds == 2 &&
         "one authored call must own Kissat's two internal rounds");
  assert(kernel_var_retired(k, 1) && kernel_var_retired(k, 2));
}

static void test_substitute_overshoot_stops_only_optional_round(void) {
  Kernel k;
  ScopedHeuristics h(k);
  add_substitute_second_round_fixture(k);
  k.stats.conflicts = 1;
  k.stats.search_ticks = 1; // 10 per mille truncates to a zero allowance.
  DefaultStrategyConfig cfg;
  cfg.inprocess.phase_rounds = 1;
  cfg.inprocess.substitute_rounds = 2;
  DefaultPolicy p(cfg);
  auto *const substitute = enable_only_first_substitute(p, k);

  assert(p.inprocess().run_phase(k, SlotGroup::Probe, nullptr) == 0);
  assert(substitute->tm.admitted == 1 && substitute->tm.dec_budget == 0);
  assert(k.stats.substitute_rounds == 1 &&
         "an over-budget productive round must suppress only round two");
  assert(kernel_var_retired(k, 1) && !kernel_var_retired(k, 2));
}

static void test_rule_boundary_deactivates_root_before_substitute_member(void) {
  Kernel k;
  ScopedHeuristics h(k);
  k.h->stable = true;
  kernel_reserve_vars(k, 4);
  // Kissat deactivates the root-fixed x1 while rewriting, then the substituted
  // x2, leaving the heap [x4,x3].
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(1), internalize(-2));
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  k.stats.conflicts = 1;
  k.stats.search_ticks = 1000000000;

  DefaultStrategyConfig cfg;
  cfg.inprocess.phase_rounds = 1;
  cfg.inprocess.substitute_rounds = 1;
  DefaultPolicy p(cfg);
  enable_only_first_substitute(p, k);
  assert(p.inprocess().run_phase(k, SlotGroup::Probe, nullptr) == 0);

  assert(k.values[internalize(1)] > 0);
  assert(kernel_var_retired(k, 1));
  assert(k.retired_events.empty() &&
         "rule-local retirements must not drift to phase exit");
  assert(!heuristics_heap_contains(k, 0));
  assert(!heuristics_heap_contains(k, 1));
  assert(k.h->scores.stack == std::vector<Var>({3, 2}) &&
         "root-fixed deactivation must precede substituted deactivation");
}

// Every default Inprocess-tier rule has an authored slot or startup position.
static void test_authored_slot_order_covers_registry(void) {
  size_t norder = 0;
  const SlotOrderRow *order = default_slot_order(&norder);
  const DefaultStrategyConfig defaults;
  const std::vector<RuleVTable> &rules = tactix_rules();
  for (const RuleVTable &r : rules) {
    if (r.tier != RuleTier::Inprocess || !r.default_inprocess)
      continue;
    // X5's only current owner is its explicit variable-length public action.
    // The pinned reference also ships Condition off; no default slot exists.
    if (!std::strcmp(r.name, "Condition"))
      continue;
    bool found = false;
    for (size_t o = 0; o < norder; o++)
      found = found || !std::strcmp(order[o].name, r.name);
    for (const auto &stage : defaults.inprocess.structured_root_stages)
      for (const auto &step : stage.program.sequence)
        found = found || step.rule == r.name;
    if (!found)
      std::printf("  UNAUTHORED Inprocess rule: %s\n", r.name);
    assert(found &&
           "an Inprocess-tier rule has no authored slot position -- give it one "
           "in the reference strategy program (SCHED-D1); the append fallback puts it last "
           "by accident, which is the T6 defect");
  }
  for (size_t o = 0; o < norder; o++) {
    bool found = false;
    for (const RuleVTable &r : rules)
      found = found ||
              (r.tier == RuleTier::Inprocess && !std::strcmp(order[o].name, r.name));
    if (!found)
      std::printf("  STALE authored row: %s\n", order[o].name);
    assert(found && "the reference strategy program names an unregistered "
                    "the Inprocess tier");
  }
  for (const auto &stage : defaults.inprocess.structured_root_stages)
    for (const auto &step : stage.program.sequence) {
      const int id = tactix_rule_id(step.rule.c_str());
      assert(id >= 0 && (rules[id].tier == RuleTier::Inprocess ||
                         step.rule == "Reduce" || step.rule == "Propagate" ||
                         step.rule == "Lucky"));
    }
}

// Inprocessing, Lucky and Walk need the root; other H-only operations do not.
static void test_inprocess_registry_requires_root(void) {
  Kernel k;
  kernel_reserve_vars(k, 4);
  ScopedHeuristics heuristics(k);
  for (const RuleVTable &rule : tactix_rules()) {
    if (rule.retired || rule.tier != RuleTier::Inprocess)
      continue;
    // Certified theory actions also need provenance this fixture lacks.
    const bool theory_action =
        !std::strcmp(rule.name, "ParityConsequence") ||
        !std::strcmp(rule.name, "AffineConsequence") ||
        !std::strcmp(rule.name, "ExactCoverExtension") ||
        !std::strcmp(rule.name, "IndependentSetCover") ||
        !std::strcmp(rule.name, "Gf3AffineConsequence") ||
        !std::strcmp(rule.name, "DirectCspSupport") ||
        !std::strcmp(rule.name, "DirectPhpRefutation") ||
        !std::strcmp(rule.name, "Gf3GaussianConsequence") ||
        !std::strcmp(rule.name, "Gf3GeneralRowConsequence") ||
        !std::strcmp(rule.name, "CardinalitySearch") ||
        !std::strcmp(rule.name, "TseitinCycleBreaking") ||
        !std::strcmp(rule.name, "NaryCspEquality") ||
        !std::strcmp(rule.name, "NaryCspEqualityAmo") ||
        !std::strcmp(rule.name, "NaryCspEqualityTransitivity") ||
        !std::strcmp(rule.name, "GraphIntervalEquivalence") ||
        !std::strcmp(rule.name, "AffineMatrixInitialize");
    if (!theory_action)
      assert(rule.guard(k));
    else
      assert(!rule.guard(k));
    k.level = 1;
    assert(!rule.guard(k));
    k.level = 0;
  }
  const RuleVTable &lucky = tactix_rules()[tactix_rule_id("Lucky")];
  const RuleVTable &walk = tactix_rules()[tactix_rule_id("Walk")];
  const RuleVTable &rephase = tactix_rules()[tactix_rule_id("Rephase")];
  const RuleVTable &switchmode = tactix_rules()[tactix_rule_id("SwitchMode")];
  k.level = 1;
  assert(!lucky.guard(k));
  assert(!walk.guard(k));
  assert(rephase.guard(k));
  assert(switchmode.guard(k));
}

static void test_typed_program_preserves_default_and_composes_occurrences() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  DefaultStrategyConfig defaults;
  DefaultPolicy reference(defaults);
  reference.inprocess().build(k);
  assert(reference.inprocess().entries.size() == defaults.inprocess.program.size());
  const auto &rules = tactix_rules();
  for (size_t i = 0; i < defaults.inprocess.program.size(); ++i) {
    const strategy::InprocessStep &step = defaults.inprocess.program[i];
    const auto &entry = reference.inprocess().entries[i];
    const SlotGroup expected_group =
        step.phase == strategy::InprocessPhase::Probe ? SlotGroup::Probe
                                                       : SlotGroup::Eliminate;
    const FollowupKind expected_followup =
        step.followup == strategy::Followup::AfterCongruence
            ? FollowupKind::AfterCongruence
            : step.followup == strategy::Followup::AfterSweepOrFinal
                  ? FollowupKind::AfterSweepOrFinal
                  : FollowupKind::None;
    assert(entry.step_id == step.id);
    assert(!std::strcmp(rules[entry.rule].name, step.rule.c_str()));
    assert(entry.group == expected_group);
    assert(entry.enabled == step.enabled);
    assert(entry.preprocess == step.preprocessing);
    assert(entry.followup == expected_followup);
  }

  DefaultStrategyConfig custom;
  custom.inprocess.append_unlisted_rules = false;
  strategy::BveParameters posed_bve;
  posed_bve.growth_bound = 7;
  strategy::StrengthenParameters posed_strengthen;
  posed_strengthen.clause_size_limit = 11;
  posed_strengthen.occurrence_limit = 13;
  custom.inprocess.program = {
      {91, "Sweep", strategy::InprocessPhase::Probe, true, true},
      {92, "BVE", strategy::InprocessPhase::Eliminate, true},
      {93, "Sweep", strategy::InprocessPhase::Eliminate, false},
      {94, "Strengthen", strategy::InprocessPhase::Probe, true, false,
       strategy::Followup::None, posed_strengthen},
  };
  custom.inprocess.program[1].parameters = posed_bve;
  custom.inprocess.program[1].effort_permille = 7;
  custom.inprocess.program[1].max_applications = 3;
  custom.inprocess.program[1].cursor_enabled = true;
  custom.inprocess.program[1].candidate_order = CandidateOrder::Reverse;
  DefaultPolicy composed(custom);
  composed.inprocess().build(k);
  assert(composed.inprocess().entries.size() == 4);
  static constexpr const char *names[] = {"Sweep", "BVE", "Sweep",
                                           "Strengthen"};
  for (size_t i = 0; i < 4; ++i) {
    const auto &entry = composed.inprocess().entries[i];
    assert(entry.step_id == 91 + i);
    assert(!std::strcmp(rules[entry.rule].name, names[i]));
  }
  assert(composed.inprocess().entries[0].preprocess);
  assert(composed.inprocess().entries[0].group == SlotGroup::Probe);
  assert(composed.inprocess().entries[1].group == SlotGroup::Eliminate);
  assert(composed.inprocess().entries[1].permille == 7);
  assert(composed.inprocess().entries[1].max_applies == 3);
  assert(composed.inprocess().entries[1].cursor_enabled);
  assert(composed.inprocess().entries[1].order == CandidateOrder::Reverse);
  const auto *posed_entry = std::get_if<strategy::BveParameters>(
      &composed.inprocess().entries[1].parameters);
  assert(posed_entry && posed_entry->growth_bound == 7);
  assert(composed.inprocess().entries[2].group == SlotGroup::Eliminate);
  assert(!composed.inprocess().entries[2].enabled);
  const auto *forward = std::get_if<strategy::StrengthenParameters>(
      &composed.inprocess().entries[3].parameters);
  assert(forward && forward->clause_size_limit == 11 &&
         forward->occurrence_limit == 13);
}

static void test_default_slot_membership_matches_kissat(void) {
  Kernel k;
  kernel_reserve_vars(k, 4);
  DefaultPolicy p;
  p.inprocess().build(k);
  const auto &rules = tactix_rules();
  static constexpr const char *POSED_ONLY[] = {
      "HyperBinaryRes", "Probe", "HyperTernaryRes",
      "Instantiate",    "BCE",   "CCE", "SubsumeForward",
      "ParityConsequence",
      "AffineConsequence", "ExactCoverExtension", "IndependentSetCover",
      "Gf3AffineConsequence", "DirectCspSupport",
      "DirectPhpRefutation", "Gf3GaussianConsequence",
      "TseitinCycleBreaking", "NaryCspEquality",
      "NaryCspEqualityAmo", "NaryCspEqualityTransitivity",
      "Gf3GeneralRowConsequence",
      "CardinalitySearch",
      "GraphIntervalEquivalence",
      "AffineMatrixInitialize",
  };
  for (const auto &entry : p.inprocess().entries) {
    const char *name = rules[entry.rule].name;
    bool posed_only = false;
    for (const char *candidate : POSED_ONLY)
      posed_only = posed_only || !std::strcmp(name, candidate);
    assert(entry.enabled != posed_only &&
           "production membership must be exactly the original-Kissat "
           "pipeline; non-Kissat proof rules remain explicitly poseable");
    if (!std::strcmp(name, "TransitiveReduce"))
      assert(entry.order == CandidateOrder::Heuristic &&
             "TransitiveReduce must ship with the reference activity order");
  }

  // Kissat probe.c:35-43,54-139: Vivify runs in search-time Probe only.
  static constexpr const char *SEARCH_PROBE[] = {
      "Congruence", "Substitute", "Backbone", "Vivify", "Sweep",
      "Substitute", "TransitiveReduce", "Backbone", "Factor",
  };
  static constexpr const char *INITIAL_PROBE[] = {
      "Congruence", "Substitute", "Backbone", "Sweep", "Substitute",
      "Factor",
  };
  std::vector<const char *> search_probe, initial_probe;
  // Startup preprocessing scans every entry: Kissat fastel=0 keeps Eliminate out.
  for (const auto &entry : p.inprocess().entries) {
    const char *name = rules[entry.rule].name;
    if (entry.preprocess)
      initial_probe.push_back(name);
    if (entry.enabled && entry.group == SlotGroup::Probe)
      search_probe.push_back(name);
  }
  assert(search_probe.size() ==
         sizeof SEARCH_PROBE / sizeof *SEARCH_PROBE);
  for (size_t i = 0; i < search_probe.size(); i++)
    assert(!std::strcmp(search_probe[i], SEARCH_PROBE[i]) &&
           "search-time Probe interleaving diverged from Kissat");
  assert(initial_probe.size() ==
         sizeof INITIAL_PROBE / sizeof *INITIAL_PROBE);
  for (size_t i = 0; i < initial_probe.size(); i++)
    assert(!std::strcmp(initial_probe[i], INITIAL_PROBE[i]) &&
           "startup Probe interleaving diverged from Kissat");
}

static void test_default_structured_root_sequence_is_effective() {
  for (const bool before_lucky : {false, true}) {
  Kernel k;
  kernel_defer_cold_state(k);
  constexpr unsigned vertices = 5, colors = 3;
  kernel_reserve_vars(k, vertices * colors);
  const auto variable = [](unsigned vertex, unsigned color) {
    return static_cast<int>(vertex * colors + color + 1);
  };
  const auto add_binary = [&](int first, int second) {
    kernel_add_binary_clause(k, internalize(first), internalize(second),
                             ++k.next_clause_id);
  };
  for (unsigned vertex = 0; vertex < vertices; ++vertex) {
    Lit at_least[colors];
    for (unsigned color = 0; color < colors; ++color)
      at_least[color] = internalize(variable(vertex, color));
    kernel_add_large_clause(k, at_least, colors, false, 0,
                            ++k.next_clause_id);
    for (unsigned first = 0; first < colors; ++first)
      for (unsigned second = first + 1; second < colors; ++second)
        add_binary(-variable(vertex, first), -variable(vertex, second));
  }
  constexpr unsigned edges[][2] = {
      {0, 1}, {1, 2}, {1, 3}, {1, 4}, {2, 3}, {2, 4}, {3, 4}};
  for (const auto &edge : edges)
    for (unsigned color = 0; color < colors; ++color)
      add_binary(-variable(edge[0], color), -variable(edge[1], color));

  DefaultStrategyConfig config;
  config.inprocess.structured_root_before_lucky = before_lucky;
  DefaultPolicy policy(config);
  assert(policy.solve(k) == 20);
  const auto &telem = policy.inprocess().structured_root;
  assert(telem.runs == 1 && telem.applications > 0 &&
         telem.effective > 0);
  assert(telem.terminal == strategy::InvocationStatus::Unsat);
  assert(k.inconsistent);
  assert(k.stats.lucky_early_calls == (before_lucky ? 0u : 1u));
  assert(!k.inprocessing && !k.phase_saving_suppressed);
  kernel_check_invariants(k);
  }
}

static void test_structured_root_exhaustion_finishes_selected_scope() {
  using namespace strategy;
  for (const auto disposition : {BudgetExhaustionPolicy::Stop,
                                 BudgetExhaustionPolicy::Continue,
                                 BudgetExhaustionPolicy::FinishRoundThenStop}) {
    Kernel k;
    kernel_reserve_vars(k, 3);
    kernel_add_binary_clause(k, internalize(1), internalize(2), ++k.next_clause_id);
    const Lit clause[] = {internalize(1), internalize(2), internalize(3)};
    kernel_add_large_clause(k, clause, 3, false, 0, ++k.next_clause_id);
    OnlineInvocation limited;
    limited.id = 1171;
    limited.rule = "SubstitutionRedundancy";
    SubstitutionRedundancyParameters parameters;
    parameters.producer = SubstitutionRedundancyProducer::GraphSymmetry;
    parameters.graph_symmetry.maximum_graph_vertices = 1;
    limited.parameters = parameters;
    OnlineInvocation tail;
    tail.id = 1172;
    tail.rule = "SubstitutionRedundancy";
    tail.parameters = SubstitutionRedundancyParameters{};
    tail.granularity = InvocationGranularity::OneInstance;
    tail.substitution_redundancy_instances = {
        SubstitutionRedundancyInstance{{internalize(1)}, {}, {}}};
    OnlineStrategyProgram program;
    program.sequence = {limited, tail};
    program.repeat = {RepeatMode::UntilScopedFixpoint, 4};
    DefaultStrategyConfig config;
    config.inprocess.structured_root_stages = {
        {program, disposition}, {{}, BudgetExhaustionPolicy::Continue}};
    DefaultPolicy policy(config);
    assert(policy.inprocess().run_structured_root(k, nullptr) == 0);
    const auto &stages = policy.inprocess().structured_root.stages;
    assert(stages.size() == 2);
    if (stages[0].terminal != InvocationStatus::BudgetExhausted)
      std::fprintf(stderr, "exhaustion disposition=%u status=%u rounds=%llu effective=%llu\n",
                   static_cast<unsigned>(disposition), static_cast<unsigned>(stages[0].terminal),
                   (unsigned long long)stages[0].sequence_passes,
                   (unsigned long long)stages[0].effective);
    assert(stages[0].terminal == InvocationStatus::BudgetExhausted);
    const bool immediate = disposition == BudgetExhaustionPolicy::Stop;
    assert(stages[0].effective == (immediate ? 0u : 1u));
    assert(stages[0].sequence_passes == (immediate ? 0u :
        disposition == BudgetExhaustionPolicy::FinishRoundThenStop ? 1u : 2u));
    assert(stages[1].runs == (disposition == BudgetExhaustionPolicy::Continue ? 1u : 0u));
    assert(!k.inprocessing && !k.phase_saving_suppressed);
    kernel_check_invariants(k);
  }
}

static void test_structured_root_publishes_before_probe_slots() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const auto add_binary = [&](int first, int second) {
    kernel_add_binary_clause(k, internalize(first), internalize(second),
                             ++k.next_clause_id);
  };
  add_binary(1, 2);
  add_binary(3, 4);
  add_binary(-1, -3);
  add_binary(-2, -4);
  ScopedHeuristics heuristics(k);
  const std::vector<int8_t> saved_before = k.h->saved;

  DefaultStrategyConfig config;
  config.inprocess.program.clear();
  config.inprocess.append_unlisted_rules = false;
  DefaultPolicy policy(config);
  policy.inprocess().build(k);
  assert(policy.inprocess().run_phase(k, SlotGroup::Probe, nullptr) == 0);
  assert(policy.inprocess().structured_root.runs == 1);
  assert(policy.inprocess().structured_root.effective > 0);
  assert(kernel_propagation_fixpoint(k));
  assert(k.value(internalize(-1)) > 0);
  assert(!heuristics_var_active(k, var_of(internalize(1))));
  assert(k.h->saved == saved_before &&
         "startup SR validation and closure escaped InprocessScope");
  // A later root Probe cannot re-enter the consumed startup program.
  assert(policy.inprocess().run_phase(k, SlotGroup::Probe, nullptr) == 0);
  assert(policy.inprocess().structured_root.runs == 1);
  kernel_check_invariants(k);
}

// Kissat probe.c:85 (probeint NLOGN) and eliminate.c:600 (eliminateint NLOG2N):
// the probe clock fires sooner and the gap widens.
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
}

// Kissat start_search: the first Eliminate deadline (eliminateinit=500) is
// scaled after preprocessing and due at equality.
static void test_initial_elimination_deadline_matches_kissat(void) {
  Kernel k;
  kernel_reserve_vars(k, 12);
  const Lit a[3] = {internalize(1), internalize(2), internalize(3)};
  const Lit b[3] = {internalize(4), internalize(5), internalize(6)};
  kernel_add_large_clause(k, a, 3, false, 0);
  kernel_add_large_clause(k, b, 3, false, 0);
  DefaultPolicy p;
  p.inprocess().build(k);

  // The deadline sees the post-preprocessing population.
  const Lit c[3] = {internalize(7), internalize(8), internalize(9)};
  kernel_add_large_clause(k, c, 3, false, 0);
  p.inprocess().initialize_search_limits(k);

  const double f =
      std::log10(static_cast<double>(kernel_irredundant_clauses(k)) + 10) - 5;
  const uint64_t delta = static_cast<uint64_t>(
      (4.5 * f * f + 25) *
      static_cast<double>(DefaultStrategyConfig::INPROCESS_ELIMINATE_INTERVAL_DEFAULT));
  assert(p.inprocess().probe_clock.next_conflicts == 0 &&
         "the probe clock must admit initial preprocessing");
  assert(p.inprocess().elim_clock.next_conflicts == delta &&
         "first-search elimination must use post-preprocessing scaling");
  k.stats.conflicts = 1;
  assert(!p.inprocess().want_phase(k, SlotGroup::Eliminate) &&
         "elimination must not fire immediately after conflict one");
  k.stats.conflicts = delta;
  assert(p.inprocess().want_phase(k, SlotGroup::Eliminate) &&
         "Kissat Eliminate enters at the stored conflict limit");
}

static void test_bve_default_order_adds_search_relevancy() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, c, 3, false, 0);
  ScopedHeuristics scoped(k);
  k.h->stable = true;
  k.h->scores.score[0] = 3.0;
  k.h->scores.score[1] = 1.0;
  k.h->scores.score[2] = 2.0;

  DefaultPolicy p;
  p.inprocess().build(k);
  const int rid = tactix_rule_id("BVE");
  assert(rid >= 0);
  auto it = std::find_if(p.inprocess().entries.begin(),
                         p.inprocess().entries.end(), [&](const auto &e) {
                           return e.rule == rid;
                         });
  assert(it != p.inprocess().entries.end());
  assert(it->order == CandidateOrder::Heuristic);
  uint64_t insts[8];
  const uint32_t n = tactix_rules()[rid].instances(k, insts, 8);
  assert(n == 3);
  p.inprocess().order_candidates(k, *it, insts, n);
  Lit first, second, third;
  assert(lit_of_instance(k, insts[0], &first));
  assert(lit_of_instance(k, insts[1], &second));
  assert(lit_of_instance(k, insts[2], &third));
  assert(kernel_externalize(k, first) == 2);
  assert(kernel_externalize(k, second) == 3);
  assert(kernel_externalize(k, third) == 1);
}

static void test_bve_score_preserves_kissat_binary64_pose() {
  // Kissat's named-intermediate binary64 pose rounds these two scores equal.
  constexpr double relevance_a = 0x1.800cf1d2bfffep+2;
  constexpr double relevance_b = 0x1.800cf1d2bfffdp+2;
  const double score_a = kissat_bve_variable_score(relevance_a, 4, 4, 2000);
  const double score_b = kissat_bve_variable_score(relevance_b, 4, 4, 2000);
  assert(score_a == score_b);
  assert(-score_a == 0x1.e8478ffe61c5bp+21);
}

// kissat_scale_delta(C+1) through kissat_logn adds nine: log10(C+10).
static void test_inprocess_scale_keeps_kissat_logn_offset(void) {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit clause[3] = {internalize(1), internalize(2), internalize(3)};
  for (uint32_t i = 0; i < 22575; i++)
    kernel_add_large_clause(k, clause, 3, false, 0);
  assert(kernel_irredundant_clauses(k) == 22575);

  DefaultPolicy p;
  p.inprocess().pose_initialized();
  p.inprocess().probe_clock.rounds = 2;
  k.stats.conflicts = 2688;
  p.inprocess().update_limit(k, SlotGroup::Probe);
  assert(p.inprocess().probe_clock.next_conflicts == 8278 &&
         "kissat_logn(C+1) means log10(C+10), yielding delta 5590");
}

// Kimits BINIRR_CLAUSES counts live binaries, not archived binary ids.
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
  // Isolate the Congruence delay from structured startup.
  DefaultStrategyConfig cfg;
  cfg.inprocess.structured_root_stages.clear();
  DefaultPolicy p(cfg);
  DefaultInprocessScheduler::SlotEntry *congruence =
      enable_only(p, k, "Congruence");
  assert(p.inprocess().run_phase(k, SlotGroup::Probe, nullptr) == 0);
  assert(congruence->tm.admitted == 1);
  assert(congruence->delay_current == 1 && congruence->delay_count == 1 &&
         "Kissat bumps an unproductive Congruence delay during preprocessing");

  // New formula material does not bypass the named delay. Congruence has no
  // separate production closure/new-material admission gate in Kissat.
  kernel_add_binary_clause(k, internalize(1), internalize(2), 51);
  k.stats.conflicts = 1;
  assert(p.inprocess().run_phase(k, SlotGroup::Probe, nullptr) == 0);
  assert(congruence->tm.admitted == 1 && congruence->tm.dec_delay == 1);
  assert(congruence->delay_count == 0);

  // Kissat has no Congruence material gate: a consumed delay admits the call.
  k.stats.conflicts = 2;
  assert(p.inprocess().run_phase(k, SlotGroup::Probe, nullptr) == 0);
  assert(congruence->tm.admitted == 2);
  assert(congruence->tm.dec_material == 0);
}

// Kissat eliminate.c:33-38, CaDiCaL elim.cpp:75-81 and probe.cpp:23: a phase
// needs new material (or a reduce for probing) besides the elapsed interval.
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
  p.inprocess().elim_clock.material_mark = k.subsume_material_count;
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
  q.inprocess().elim_clock.material_mark = k.subsume_material_count;
  assert(q.inprocess().want_phase(k, SlotGroup::Probe) &&
         q.inprocess().want_phase(k, SlotGroup::Eliminate) &&
         "the preprocessing phase must not be gated");
}

// Kissat's Reduce timestamp suppresses both phases only at that conflict.
static void test_reduce_suppresses_same_conflict_phases(void) {
  Kernel k;
  kernel_reserve_vars(k, 4);
  DefaultPolicy p;
  auto &s = p.inprocess();
  s.pose_initialized();
  k.stats.conflicts = 100;
  k.stats.reductions = 1;
  k.stats.bve_marks = 1;
  s.probe_clock.rounds = s.elim_clock.rounds = 1;
  s.probe_clock.next_conflicts = s.elim_clock.next_conflicts = 99;
  assert(s.want_phase(k, SlotGroup::Probe));
  assert(s.want_phase(k, SlotGroup::Eliminate));

  s.note_reduce(k);
  assert(!s.want_phase(k, SlotGroup::Probe));
  assert(!s.want_phase(k, SlotGroup::Eliminate));
  ++k.stats.conflicts;
  assert(s.want_phase(k, SlotGroup::Probe));
  assert(s.want_phase(k, SlotGroup::Eliminate));

  s.reset(k);
  s.pose_initialized();
  s.probe_clock.rounds = s.elim_clock.rounds = 1;
  s.probe_clock.next_conflicts = s.elim_clock.next_conflicts = 99;
  assert(s.want_phase(k, SlotGroup::Probe));
  assert(s.want_phase(k, SlotGroup::Eliminate));
}

// Kissat kimits.h:135-170 SET_EFFORT_LIMIT: limit = counter + effort/1000 *
// max(search ticks since the phase anchor, mineffort), no clamps.
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

  // The floored reference gives 1e7/1000*100 == 1e6; no active floor.
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

  // Scale the whole reference, then truncate once.
  k.stats.search_ticks = 191898539;
  e.meter = [](const Kernel &) -> uint64_t { return 0; };
  p.inprocess().set_entry_limit(e, k);
  assert(e.limit == 19189853 &&
         "per-mille scaling truncated the reference before multiplication");
}

static void test_first_search_effort_keeps_input_propagation_ticks(void) {
  Kernel k;
  kernel_reserve_vars(k, 64);
  // Pre-solve input propagation ticks belong to the first effort reference.
  k.stats.search_ticks = 276045;
  DefaultPolicy p;
  p.reset_schedule(k);
  assert(p.inprocess().probe_clock.anchor == 0 &&
         p.inprocess().elim_clock.anchor == 0 &&
         "solve reset must preserve Kissat's zero initial phase anchors");

  k.stats.conflicts = 3654;
  k.stats.search_ticks = 22649450;
  DefaultInprocessScheduler::SlotEntry e;
  e.rule = static_cast<int>(find_rule("Vivify")->id);
  e.group = SlotGroup::Probe;
  e.permille = 100;
  e.meter = [](const Kernel &) -> uint64_t { return 0; };
  p.inprocess().set_entry_limit(e, k);
  assert(e.limit == 2264945 &&
         "first Probe dose dropped pre-solve input-propagation ticks");

  e.rule = static_cast<int>(find_rule("BVE")->id);
  e.group = SlotGroup::Eliminate;
  p.inprocess().set_entry_limit(e, k);
  assert(e.limit == 2264945 &&
         "first Eliminate dose dropped pre-solve input-propagation ticks");
}

static void test_eliminate_sparse_resume_replays_root_suffix_as_search(void) {
  const auto run = [](bool charge_search_ticks) {
    Kernel k;
    kernel_reserve_vars(k, 3);
    const Lit old_root = internalize(1);
    const Lit suffix = internalize(2);
    const Lit implied = internalize(3);
    kernel_assign_unit(k, old_root);
    assert(!kernel_propagate(k));
    const uint32_t suffix_begin = static_cast<uint32_t>(k.trail.size());
    kernel_add_binary_clause(k, negate(suffix), implied);
    kernel_assign_unit(k, suffix);
    k.inprocessing = true;
    const uint64_t before = k.stats.search_ticks;
    assert(default_resume_sparse_propagation(k, suffix_begin,
                                             charge_search_ticks) ==
           ApplyResult::Ok);
    assert(k.inprocessing && "resume must restore the enclosing phase scope");
    assert(k.value(implied) > 0 &&
           "sparse resume must replay the simplifier-produced root suffix");
    return k.stats.search_ticks - before;
  };
  assert(run(true) == 4 &&
         "Eliminate resume must use Kissat's ordinary search tick currency");
  assert(run(false) == 0 &&
         "probing resume must keep its scans out of search tick currency");
}

static void test_preprocess_is_reduced_and_bounded(void) {
  Kernel k;
  kernel_reserve_vars(k, 64);
  ScopedHeuristics heuristics(k);
  DefaultPolicy p;
  p.inprocess().build(k);
  const auto &rules = tactix_rules();
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

  // Kissat factor.c factoriniticks=700 prices the preprocessing Factor call.
  e.rule = static_cast<int>(find_rule("Factor")->id);
  e.permille = 50;
  e.budget_aware = true;
  p.inprocess().set_entry_limit(e, k);
  assert(e.limit ==
             DefaultStrategyConfig::FACTOR_INITIAL_EFFORT_DEFAULT + 7 &&
         "preprocessing Factor must receive Kissat's first-dose allowance");
}

static void test_initial_fast_elimination_composes_only_proof_rules(void) {
  Kernel k;
  kernel_reserve_vars(k, 4);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  const Lit subsumed[] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, subsumed, 3, false, 0);
  kernel_add_binary_clause(k, internalize(-1), internalize(4));
  ScopedHeuristics heuristics(k);
  DefaultStrategyConfig config;
  DefaultPolicy policy(config);
  policy.inprocess().build(k);
  assert(policy.inprocess().run_initial_fast_elimination(
             k, nullptr, nullptr, {true, 0, 0, true}) == 0);
  assert(k.stats.forward_subsumed == 1 &&
         "root growth without an eligible extension must still run FastEl");
  const auto &rules = tactix_rules();
  for (const auto &entry : policy.inprocess().entries) {
    if (entry.group != SlotGroup::Eliminate)
      continue;
    if (!std::strcmp(rules[entry.rule].name, "BVE"))
      assert(entry.tm.occasions > 0);
    else
      assert(entry.tm.occasions == 0 &&
             "fastel must not replay the ordinary Eliminate pipeline");
  }
  kernel_check_invariants(k);
}

// A fresh solver extension x with clauses (x v p) and (-x v q) for the given
// external p and q.
static Lit add_extension(Kernel &k, std::initializer_list<int> positive,
                         std::initializer_list<int> negative) {
  const Lit x = make_lit(kernel_new_extension_vars(k, 1), false);
  for (const int p : positive)
    kernel_add_binary_clause(k, x, internalize(p));
  for (const int q : negative)
    kernel_add_binary_clause(k, negate(x), internalize(q));
  return x;
}

// Startup FastEl whose structured owner covers every external created so far.
static int fastel_owning_all(DefaultPolicy &p, Kernel &k, bool grew_root,
                             bool symmetry = true) {
  return p.inprocess().run_initial_fast_elimination(
      k, nullptr, nullptr,
      {grew_root, k.original_external_vars,
       static_cast<uint32_t>(k.external_to_internal.size()), symmetry});
}

static void test_initial_fast_elimination_budgets_discovery_before_mutation(void) {
  Kernel k;
  kernel_reserve_vars(k, 4);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(3));
  kernel_add_binary_clause(k, internalize(-2), internalize(4));
  ScopedHeuristics heuristics(k);
  DefaultStrategyConfig config;
  config.inprocess.preprocess_initial_ticks = 10;
  config.inprocess.bve_permille = 100;
  DefaultPolicy policy(config);
  policy.inprocess().build(k);
  assert(policy.inprocess().run_initial_fast_elimination(k, nullptr) == 0);
  assert(k.stats.fastel_rounds == 1 && k.stats.fastel_active == 4);
  assert(k.stats.fastel_budget_stops == 1);
  assert(k.stats.fastel_discovery_ticks == 0 &&
         k.stats.fastel_candidates == 0 && !k.stats.bve_eliminated);
  for (Var variable = 0; variable < k.nvars; ++variable)
    assert(!kernel_var_retired(k, variable));
  kernel_check_invariants(k);

  // An owner-side discovery-budget decline returns before mutation and is
  // not root suppression.
  Kernel owned;
  kernel_reserve_vars(owned, 4);
  ScopedHeuristics owned_heuristics(owned);
  add_extension(owned, {1}, {2});
  DefaultPolicy owned_policy(config);
  owned_policy.inprocess().build(owned);
  assert(fastel_owning_all(owned_policy, owned, true) == 0);
  assert(owned.stats.fastel_root_suppressed == 0 &&
         owned.stats.fastel_budget_stops == 1 &&
         owned.stats.fastel_discovery_ticks == 0);
  kernel_check_invariants(owned);
}

static void test_structured_startup_records_coupled_factor_range(void) {
  Kernel k;
  kernel_reserve_vars(k, 7);
  ScopedHeuristics heuristics(k);
  // Independent 2x3 grid: Factor replaces the six binaries by a fresh helper.
  for (const Lit factor : {internalize(1), internalize(2)})
    for (const Lit remainder :
         {internalize(3), internalize(4), internalize(5)})
      kernel_add_binary_clause(k, factor, remainder, ++k.next_clause_id);
  for (const Lit literal : {internalize(1), internalize(2), internalize(3),
                            internalize(4), internalize(5)})
    kernel_mark_factor_literal(k, literal);
  // These two clauses imply -6. The explicit SR unit therefore grows the root
  // without consuming any member of the Factor grid.
  kernel_add_binary_clause(k, internalize(-6), internalize(7),
                           ++k.next_clause_id);
  kernel_add_binary_clause(k, internalize(-6), internalize(-7),
                           ++k.next_clause_id);

  DefaultStrategyConfig config;
  strategy::OnlineInvocation fixing;
  fixing.id = 112;
  fixing.rule = "SubstitutionRedundancy";
  SubstitutionRedundancyParameters parameters;
  parameters.producer = SubstitutionRedundancyProducer::ExplicitOnly;
  fixing.parameters = parameters;
  fixing.granularity = strategy::InvocationGranularity::OneInstance;
  fixing.substitution_redundancy_instances = {
      SubstitutionRedundancyInstance{{internalize(-6)}, {}, {}}};
  strategy::OnlineStrategyProgram structured;
  structured.sequence = {fixing};
  config.inprocess.structured_root_stages = {
      {structured, strategy::BudgetExhaustionPolicy::Continue}};
  config.inprocess.structured_root_before_lucky = false;
  config.inprocess.program = {
      {15, "Factor", strategy::InprocessPhase::Probe, true, true}};
  config.inprocess.append_unlisted_rules = false;

  DefaultPolicy policy(config);
  policy.inprocess().build(k);
  assert(policy.inprocess().run_phase(k, SlotGroup::Probe, nullptr) == 0);
  const auto &ownership = policy.inprocess().structured_fastel_ownership();
  assert(ownership.symmetry_applied && ownership.grew_root_trail);
  assert(k.stats.factorings == 1);
  assert(ownership.external_begin == ownership.external_end);
  assert(ownership.coupled_factor_ranges.size() == 1);
  const auto range = ownership.coupled_factor_ranges.front();
  assert(range.begin == 7 && range.end == 8 &&
         ownership.factor_owns(7) && ownership.composed_owns(7) &&
         ownership.protects_at_application(7));
  assert(!ownership.directly_owns(7));
  kernel_check_invariants(k);
}

static void test_initial_fast_elimination_defers_solver_extensions(void) {
  {
    Kernel plain;
    kernel_reserve_vars(plain, 4);
    ScopedHeuristics heuristics(plain);
    add_extension(plain, {1, 2}, {3, 4});
    DefaultPolicy policy;
    policy.inprocess().build(plain);
    assert(policy.inprocess().run_initial_fast_elimination(plain, nullptr) == 0);
    assert(!plain.stats.fastel_extensions_protected &&
           "extensions are not protected when structured SR made no progress");
    kernel_check_invariants(plain);
  }

  // A 2x2 definition: eliminating the helper reconstructs four resolvents,
  // while the pure originals still give startup FastEl ordinary work.
  Kernel k;
  kernel_reserve_vars(k, 4);
  ScopedHeuristics heuristics(k);
  const Lit x = add_extension(k, {1, 2}, {3, 4});
  const Var extension = var_of(x);
  assert(extension == 4 && kernel_is_solver_extension(k, extension));
  k.stats.sr_applied = 1; // deferral is specific to the symmetry composition
  DefaultPolicy policy;
  policy.inprocess().build(k);
  assert(fastel_owning_all(policy, k, false) == 0);
  assert(k.stats.fastel_rounds > 0);
  assert(k.stats.fastel_extensions_protected > 0);
  assert(k.stats.fastel_eliminated > 0 &&
         "protecting extensions must not disable ordinary startup BVE");
  assert(!kernel_var_retired(k, extension));

  // The protection is a DefaultPolicy choice, not an N1 restriction: the
  // public BVE action still eliminates the extension.
  const RuleVTable *bve = find_rule("BVE");
  assert(bve && bve->guard(k));
  assert(bve->apply_instance(k, lit_instance(k, x)) == ApplyResult::Ok);
  assert(kernel_var_retired(k, extension));
  kernel_check_invariants(k);

  {
    Kernel rooted;
    kernel_reserve_vars(rooted, 4);
    ScopedHeuristics rooted_heuristics(rooted);
    const Lit rooted_x = add_extension(rooted, {1, 2}, {3, 4});
    rooted.stats.sr_applied = 1;
    const Lit subsumed[] = {rooted_x, internalize(1), internalize(2)};
    kernel_add_large_clause(rooted, subsumed, 3, false, 0);
    DefaultPolicy rooted_policy;
    rooted_policy.inprocess().build(rooted);
    const std::vector<uint8_t> retry_before = rooted.bve_retry;
    assert(fastel_owning_all(rooted_policy, rooted, true) == 0);
    assert(rooted.stats.fastel_rounds == 0 &&
           rooted.stats.fastel_discovery_ticks == 0 &&
           rooted.stats.fastel_candidates == 0);
    assert(rooted.stats.fastel_root_suppressed == 1);
    assert(rooted.stats.fastel_eliminated == 0);
    assert(rooted.stats.forward_subsumed == 0 &&
           "the structured preflight must return before local subsumption");
    assert(rooted.bve_retry == retry_before &&
           "the structured preflight must not reschedule BVE candidates");
    assert(!kernel_var_retired(rooted, var_of(rooted_x)));
    kernel_check_invariants(rooted);
  }
  {
    // The read-only preflight predicts the dense entry's deduplicated
    // population without deleting the repeated binary itself.
    Kernel repeated;
    kernel_reserve_vars(repeated, 2);
    ScopedHeuristics repeated_heuristics(repeated);
    add_extension(repeated, {1, 1}, {2});
    DefaultStrategyConfig repeated_config;
    repeated_config.inprocess.initial_fast_elimination_parameters
        .occurrence_limit = 2;
    DefaultPolicy repeated_policy(repeated_config);
    repeated_policy.inprocess().build(repeated);
    const uint64_t binaries_before = repeated.watches.binary_clauses();
    assert(fastel_owning_all(repeated_policy, repeated, true) == 0);
    assert(repeated.stats.fastel_root_suppressed == 1 &&
           repeated.stats.fastel_rounds == 0);
    assert(repeated.watches.binary_clauses() == binaries_before &&
           "the preflight must not perform dense-entry deduplication");
    kernel_check_invariants(repeated);
  }
  {
    // A published but unpropagated root is not classified by the preflight;
    // Eliminate closes it first and pointwise protection still applies.
    Kernel pending;
    kernel_reserve_vars(pending, 5);
    ScopedHeuristics pending_heuristics(pending);
    kernel_assign_unit(pending, internalize(1));
    assert(pending.propagate_head == 0 && pending.trail.size() == 1);
    const Lit pending_x = add_extension(pending, {2, 3}, {4, 5});
    DefaultPolicy pending_policy;
    pending_policy.inprocess().build(pending);
    assert(fastel_owning_all(pending_policy, pending, true) == 0);
    assert(pending.stats.fastel_root_suppressed == 0 &&
           pending.stats.fastel_rounds > 0 &&
           pending.stats.fastel_extensions_protected > 0);
    assert(!kernel_var_retired(pending, var_of(pending_x)));
    assert(kernel_propagation_fixpoint(pending));
    kernel_check_invariants(pending);
  }
  {
    // Only the recorded structured range participates: a later unrelated
    // extension does not trigger whole-pass suppression.
    Kernel mixed;
    kernel_reserve_vars(mixed, 2);
    ScopedHeuristics mixed_heuristics(mixed);
    const Lit structured_x = add_extension(mixed, {1}, {2});
    const uint32_t structured_external_begin =
        mixed.internal_to_external[var_of(structured_x)];
    const uint32_t structured_external_end =
        static_cast<uint32_t>(mixed.external_to_internal.size());
    const Var later_extension = kernel_new_extension_vars(mixed, 1);
    kernel_add_binary_clause(mixed, negate(make_lit(later_extension, false)),
                             internalize(1));
    DefaultStrategyConfig mixed_config;
    mixed_config.inprocess.initial_fast_elimination_parameters
        .occurrence_limit = 1;
    DefaultPolicy mixed_policy(mixed_config);
    mixed_policy.inprocess().build(mixed);
    assert(mixed_policy.inprocess().run_initial_fast_elimination(
               mixed, nullptr, nullptr,
               {true, structured_external_begin,
                structured_external_end, true}) == 0);
    assert(mixed.stats.fastel_root_suppressed == 0 &&
           mixed.stats.fastel_rounds > 0 &&
           "ordinary Probe extensions must not masquerade as structured helpers");
    kernel_check_invariants(mixed);
  }
  {
    // A startup Factor helper consumes the SR root representation, so its
    // coupled range joins the composed preflight cohort.
    Kernel coupled;
    kernel_reserve_vars(coupled, 2);
    ScopedHeuristics coupled_heuristics(coupled);
    const uint32_t coupled_begin =
        static_cast<uint32_t>(coupled.external_to_internal.size());
    add_extension(coupled, {1}, {2});
    const uint32_t coupled_end =
        static_cast<uint32_t>(coupled.external_to_internal.size());
    DefaultPolicy coupled_policy;
    coupled_policy.inprocess().build(coupled);
    DefaultInprocessScheduler::StructuredFastelOwnership ownership{
        true, coupled_begin, coupled_begin, true};
    ownership.coupled_factor_ranges.push_back(
        {coupled_begin, coupled_end});
    assert(!ownership.directly_owns(coupled_begin) &&
           ownership.factor_owns(coupled_begin) &&
           ownership.protects_at_application(coupled_begin));
    assert(coupled_policy.inprocess().run_initial_fast_elimination(
               coupled, nullptr, nullptr, ownership) == 0);
    assert(coupled.stats.fastel_root_suppressed == 1 &&
           coupled.stats.fastel_rounds == 0 &&
           "startup Factor helpers must participate in composed suppression");
    kernel_check_invariants(coupled);
  }
  {
    // The owner gathers on twice the positive population before its
    // two-polarity check, so this extension is not a round-one candidate.
    Kernel gather_boundary;
    kernel_reserve_vars(gather_boundary, 2);
    ScopedHeuristics gather_heuristics(gather_boundary);
    add_extension(gather_boundary, {1, 2}, {});
    DefaultStrategyConfig gather_config;
    gather_config.inprocess.initial_fast_elimination_parameters
        .occurrence_limit = 3;
    DefaultPolicy gather_policy(gather_config);
    gather_policy.inprocess().build(gather_boundary);
    assert(fastel_owning_all(gather_policy, gather_boundary, true) == 0);
    assert(gather_boundary.stats.fastel_root_suppressed == 0 &&
           gather_boundary.stats.fastel_rounds > 0);
    kernel_check_invariants(gather_boundary);
  }
  {
    // Root growth and an extension interval need an SR application too.
    Kernel nonsymmetry;
    kernel_reserve_vars(nonsymmetry, 2);
    ScopedHeuristics nonsymmetry_heuristics(nonsymmetry);
    add_extension(nonsymmetry, {1}, {2});
    DefaultPolicy nonsymmetry_policy;
    nonsymmetry_policy.inprocess().build(nonsymmetry);
    assert(fastel_owning_all(nonsymmetry_policy, nonsymmetry, true, false) == 0);
    assert(nonsymmetry.stats.fastel_root_suppressed == 0 &&
           nonsymmetry.stats.fastel_rounds > 0);
    kernel_check_invariants(nonsymmetry);
  }
}

static void test_initial_fast_elimination_keeps_owner_candidate_order(void) {
  Kernel k;
  kernel_reserve_vars(k, 10);

  // The owner's fastel score (twice the positive occurrences) tries y first
  // (4 < 6); eliminating either pivot blocks the other.
  const Lit x = internalize(1), y = internalize(2);
  kernel_add_binary_clause(k, x, internalize(3));
  kernel_add_binary_clause(k, x, internalize(4));
  kernel_add_binary_clause(k, x, internalize(5));
  kernel_add_binary_clause(k, negate(x), internalize(6));
  kernel_add_binary_clause(k, negate(x), y);
  kernel_add_binary_clause(k, y, internalize(7));
  kernel_add_binary_clause(k, negate(y), internalize(8));
  kernel_add_binary_clause(k, negate(y), internalize(9));
  kernel_add_binary_clause(k, negate(y), internalize(10));

  // Supports get four positive ring occurrences, ranking after x and y.
  for (int external = 3; external <= 10; ++external)
    for (int offset = 1; offset <= 4; ++offset) {
      const int other = 3 + (external - 3 + offset) % 8;
      kernel_add_binary_clause(k, internalize(external), internalize(other));
    }

  ScopedHeuristics heuristics(k);
  k.h->stable = true;
  heuristics_score_set(k, var_of(x), 0.0);
  heuristics_score_set(k, var_of(y), 100.0);

  // One round keeps the first choice observable.
  DefaultStrategyConfig config;
  config.inprocess.initial_fast_elimination_rounds = 1;
  DefaultPolicy policy(config);
  policy.inprocess().build(k);
  assert(policy.inprocess().run_initial_fast_elimination(k, nullptr) == 0);
  assert(!kernel_var_retired(k, var_of(x)));
  assert(kernel_var_retired(k, var_of(y)) &&
         "startup fastel must preserve its owner radix order instead of "
         "re-ranking candidates with ordinary BVE activity");
  const auto &rules = tactix_rules();
  for (const auto &entry : policy.inprocess().entries)
    if (entry.group == SlotGroup::Eliminate &&
        !std::strcmp(rules[entry.rule].name, "BVE"))
      assert(entry.limit == config.inprocess.preprocess_initial_ticks *
                                config.inprocess.initial_fast_elimination_bve_permille /
                                1000 &&
             "startup fastel must use its bounded typed BVE effort dose");
  kernel_check_invariants(k);
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
}

static void test_inprocess_does_not_save_probe_phases(void) {
  Kernel k;
  ScopedHeuristics h(k);
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(3));

  DefaultStrategyConfig cfg;
  cfg.inprocess.bve_occurrence_limit = 37;
  cfg.inprocess.bve_clause_size_limit = 41;
  DefaultPolicy p(cfg);
  enable_only(p, k, "Backbone");
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
  assert(k.bve_clause_size_limit == 41 &&
         "DefaultPolicy must wire the named BVE antecedent-size limit");
}

static void test_probe_entry_does_not_snapshot_target_phases(void) {
  Kernel k;
  ScopedHeuristics h(k);
  kernel_reserve_vars(k, 3);
  k.h->stable = true;
  kernel_assign_decision(k, internalize(-1));
  kernel_assign_decision(k, internalize(-2));
  k.h->target.assign(k.nvars, 1);
  k.h->target_assigned = 0;
  k.h->best_assigned = 0;
  k.stats.restarts = 90;
  const uint64_t restarts = k.stats.restarts;

  DefaultPolicy p;
  p.inprocess().build(k);
  for (auto &e : p.inprocess().entries)
    e.enabled = false;
  assert(p.inprocess().run_phase(k, SlotGroup::Probe, nullptr) == 0);
  assert(k.level == 0);
  assert(k.stats.restarts == restarts &&
         "Probe's lifecycle root clear is not a scheduled Restart");
  assert(k.stats.target_saved == 0 && k.stats.best_saved == 0);
  assert(k.h->target[0] == 1 && k.h->target[1] == 1 &&
         "Kissat enters Probe with solver->probing set, so its root restart "
         "must not overwrite target phases");
}

static void test_jump_reason_formula_classification(void) {
  const auto add_chain = [](Kernel &k) {
    kernel_reserve_vars(k, 3);
    const Lit p = internalize(1), q = internalize(2), r = internalize(3);
    kernel_add_binary_clause(k, p, q);
    kernel_add_binary_clause(k, negate(q), r);
    kernel_assign_decision(k, negate(p));
  };

  // Kissat: at most smallclauses clauses is never bigbig.
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

  // Lowering the size limit exercises the binary-fraction arm.
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

  // Kissat's second jumpreasons site (learned-binary assertion and conflict
  // reuse) deliberately bypasses classification.bigbig.
  Kernel learned;
  kernel_reserve_vars(learned, 3);
  const Lit p = internalize(1), q = internalize(2), r = internalize(3);
  kernel_assign_decision(learned, negate(p));
  kernel_assign_binary(learned, q, p);
  assert(!learned.assigned[var_of(q)].jumped);
  kernel_assign_binary(learned, r, negate(q), /*require_bigbig=*/false);
  assert(!learned.jump_reasons_bigbig);
  assert(learned.assigned[var_of(r)].jumped);
  assert(learned.assigned[var_of(r)].reason == p);
}

static void test_preprocess_followup_typed_arm(void) {
  Kernel k;
  kernel_reserve_vars(k, 4);
  DefaultStrategyConfig config;
  config.inprocess.preprocess_followups = false;
  DefaultPolicy p(config);
  p.inprocess().build(k);
  assert(p.inprocess().run_phase(k, SlotGroup::Probe, nullptr) == 0);
  const auto &rules = tactix_rules();
  unsigned seen = 0;
  for (const auto &e : p.inprocess().entries)
    if (e.preprocess && !std::strcmp(rules[e.rule].name, "Substitute")) {
      seen++;
      assert(e.applies == 1);
      assert(e.tm.followup_offered == 0 && e.tm.followup_taken == 0);
    }
  assert(seen == 2);
}

static void test_reorder_ships_in_both_modes(void) {
  Kernel k;
  kernel_reserve_vars(k, 4);
  ScopedHeuristics h(k);
  DefaultPolicy p;
  k.stats.conflicts = DefaultStrategyConfig::REORDER_INITIAL_INTERVAL_DEFAULT;
  assert(p.want_reorder(k) &&
         "Kissat's shipped reorder=2 default must admit focused mode");
  k.h->stable = true;
  assert(p.want_reorder(k) &&
         "Kissat's shipped reorder=2 default must admit stable mode");
  k.level = 1;
  assert(!p.want_reorder(k) && "the scheduled event remains root-only");
}

static void test_startup_sparse_collection_precedes_probe(void) {
  Kernel k;
  kernel_reserve_vars(k, 7);
  kernel_assign_unit(k, internalize(1));
  const Lit satisfied[3] = {internalize(1), internalize(2),
                            internalize(4)};
  const Lit shortened[4] = {internalize(-1), internalize(2),
                            internalize(3), internalize(4)};
  kernel_add_large_clause(k, satisfied, 3, false, 0);
  kernel_add_large_clause(k, shortened, 4, false, 0);
  // Every assignment to 5,6,7 violates one clause; no Lucky assumption refutes it.
  for (int signs = 0; signs < 8; ++signs) {
    const Lit clause[3] = {
        internalize((signs & 1) ? -5 : 5),
        internalize((signs & 2) ? -6 : 6),
        internalize((signs & 4) ? -7 : 7),
    };
    kernel_add_large_clause(k, clause, 3, false, 0);
  }

  DefaultStrategyConfig config;
  config.inprocess.structured_root_before_lucky = false; // reference collection boundary
  DefaultPolicy p(config);
  assert(p.solve(k) == 20);
  assert(k.stats.sparse_collections == 2);
  // Sparse collection removes the satisfied clause and shortens the other
  // before Probe; the second collection is the later inprocessing closure.
  assert(k.stats.sparse_collected == 2);
  assert(k.stats.strengthen_rounds == 0 &&
         "startup collection is scaffolding, not an authored C2 slot");
}

static void test_eliminate_owns_physical_dense_transition() {
  struct Observe : CandidateOrderPolicy {
    bool called = false, connected = false;
    uint32_t large_words = 0;
    const char *name() const override { return "observe-dense-entry"; }
    void rank(const Kernel &k, uint32_t, uint64_t *, uint32_t) override {
      if (called)
        return;
      called = true;
      connected = k.large_watches_connected;
      for (Lit l = 0; l < 2 * k.nvars; ++l)
        large_words += k.watches.size(l) - k.watches.binary_size(l);
    }
  } observer;
  Kernel k;
  ScopedHeuristics heuristics(k);
  kernel_reserve_vars(k, 4);
  const Lit lits[] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, lits, 3, false, 0, 1);
  kernel_add_binary_clause(k, internalize(-1), internalize(4), 2);
  const auto watch_words = [&k](uint32_t *large, uint32_t *binary) {
    *large = *binary = 0;
    for (Lit l = 0; l < 2 * k.nvars; ++l) {
      *binary += k.watches.binary_size(l);
      *large += k.watches.size(l) - k.watches.binary_size(l);
    }
  };
  uint32_t large_before = 0, binary_before = 0;
  watch_words(&large_before, &binary_before);
  assert(large_before > 0 && binary_before > 0 && k.large_watches_connected);
  // Pose a search-time Eliminate call (BVE defers preprocessing, Kissat fastel=0).
  k.stats.conflicts = 1;
  // Zero retry advice keeps every candidate out of the dose.
  std::fill(k.bve_retry.begin(), k.bve_retry.end(), uint8_t{0});
  DefaultPolicy p;
  p.inprocess().pose_initialized();
  p.inprocess().candidate_order = &observer;
  DefaultInprocessScheduler::SlotEntry e;
  e.rule = tactix_rule_id("BVE");
  e.group = SlotGroup::Eliminate;
  e.dosed = true;
  e.order = CandidateOrder::Reverse;
  e.max_applies = 1;
  e.meter = [](const Kernel &) -> uint64_t { return 0; };
  p.inprocess().entries.push_back(e);
  assert(p.inprocess().run_phase(k, SlotGroup::Eliminate, nullptr) != 20);
  const DefaultInprocessScheduler::SlotEntry &ran = p.inprocess().entries[0];
  assert(ran.applies == 1 && ran.tm.admitted == 1 && ran.ord.lists == 1 &&
         "the dosed BVE entry must reach its candidate ranking");
  assert(observer.called && !observer.connected && observer.large_words == 0);
  assert(k.stats.bve_eliminated == 0 && "the retry advice kept the formula");
  assert(k.large_watches_connected && !k.dense);
  uint32_t large_after = 0, binary_after = 0;
  watch_words(&large_after, &binary_after);
  assert(large_after == large_before && binary_after == binary_before &&
         "phase exit must restore the ordinary sparse watches");
  kernel_check_invariants(k);
}

int main() {
  test_eliminate_owns_physical_dense_transition();
  test_mode_growth_key_matches_kissat();
  test_reason_bump_defaults_match_kissat();
  test_reason_bump_delay_lives_at_final_clause_gate();
  test_factor_material_gate_uses_literal_marks();
  test_large_formula_factor_decline_is_solve_latched();
  test_retier_schedule_advances_only_on_otfs_conflicts();
  test_effort_limit_is_kissats();
  test_first_search_effort_keeps_input_propagation_ticks();
  test_eliminate_sparse_resume_replays_root_suffix_as_search();
  test_preprocess_is_reduced_and_bounded();
  test_initial_fast_elimination_composes_only_proof_rules();
  test_initial_fast_elimination_budgets_discovery_before_mutation();
  test_structured_startup_records_coupled_factor_range();
  test_initial_fast_elimination_defers_solver_extensions();
  test_initial_fast_elimination_keeps_owner_candidate_order();
  test_preprocess_followup_truth_table();
  test_preprocess_final_followup_is_observed();
  test_inprocess_does_not_save_probe_phases();
  test_probe_entry_does_not_snapshot_target_phases();
  test_jump_reason_formula_classification();
  test_preprocess_followup_typed_arm();
  test_reorder_ships_in_both_modes();
  test_startup_sparse_collection_precedes_probe();
  test_slot_cursor_rotates();
  test_cursor_preserves_n1();
  test_order_arms_reorder();
  test_subsume_meters_partner_visits();
  test_vivify_scope_tracks_tier2();
  test_vivify_dose_poses_focused_tiers();
  test_vivify_dose_merges_tier2_into_tier1();
  test_vivify_dose_carries_early_tier_budget();
  test_vivify_subsumption_reduces_irredundant_delay();
  test_sweep_delay_rates_per_swept_variable();
  test_substitute_budget_is_per_round_and_per_call();
  test_substitute_overshoot_stops_only_optional_round();
  test_rule_boundary_deactivates_root_before_substitute_member();
  test_authored_slot_order_covers_registry();
  test_inprocess_registry_requires_root();
  test_typed_program_preserves_default_and_composes_occurrences();
  test_default_slot_membership_matches_kissat();
  test_default_structured_root_sequence_is_effective();
  test_structured_root_exhaustion_finishes_selected_scope();
  test_structured_root_publishes_before_probe_slots();
  test_two_slot_clocks_diverge();
  test_initial_elimination_deadline_matches_kissat();
  test_bve_default_order_adds_search_relevancy();
  test_bve_score_preserves_kissat_binary64_pose();
  test_inprocess_scale_keeps_kissat_logn_offset();
  test_inprocess_scale_ignores_deleted_binary_ids();
  test_congruence_preprocess_bumps_first_search_delay();
  test_slot_gates_decline_without_new_material();
  test_reduce_suppresses_same_conflict_phases();
  std::printf("test_scheduling: all passed\n");
  return 0;
}
