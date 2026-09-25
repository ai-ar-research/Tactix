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
  k.factor_literals_marked = 0;

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
  kernel_mark_factor_literal(k, make_lit(3, true));
  kernel_mark_factor_literal(k, make_lit(3, true));
  assert(k.factor_literals_marked == 1 &&
         "only a factor-bit zero-to-one transition advances the counter");
  assert(p.inprocess().run_phase(k, SlotGroup::Probe, nullptr) == 0);
  assert(factor->tm.dec_material == 1 && factor->tm.admitted == 1);
  // The completed call consumed the new cumulative event even though its
  // local bit state is an implementation detail.  With no later addition,
  // Kissat's `limits.factor.marked` suppresses the next call.
  assert(p.inprocess().run_phase(k, SlotGroup::Probe, nullptr) == 0);
  assert(factor->tm.dec_material == 2 && factor->tm.admitted == 1);
  std::printf("  factor material gate: cumulative Kissat watermark\n");
}

static void test_large_formula_factor_decline_is_solve_latched(void) {
  Kernel k;
  kernel_reserve_vars(k, 8);
  DefaultStrategyConfig cfg;
  cfg.inprocess.factor_active_ceiling = 6;
  cfg.inprocess.structured_root_stages.clear(); // isolate the Factor gate
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
    // Kissat transitive.c pushes variable indices, but its comparator applies
    // IDX once more before reading activity, and the sorted stack is popped
    // from the back.  Pin that exact reference pose independently of the
    // implementation's sorting code.
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

    // Tactix preserves public/kernel variable identities, while Kissat imports
    // in first-activation order and later compacts those internal indices.
    // The unusual second IDX in transitive.c applies to that reference index,
    // not to Tactix's stable variable id.  Pin the policy-owned permutation
    // bridge independently of the identity-order case above.
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

    // AE-kissat-MAB changes transitive.c itself to consult the selected stable
    // heap.  Give CHB a deliberately different maximum and require the same
    // compact-index/second-IDX pose to follow that arm.
    heuristics_configure_mab(k, true, 4.0, 0.4, 0.06, 0.000001);
    k.h->stable_heuristic = 1;
    Lit chb_target_lit;
    assert(lit_of_instance(k, before[3], &chb_target_lit));
    const Var chb_owner =
        compact_order[compact_position[var_of(chb_target_lit)] >> 1];
    k.h->chb_scores.score[chb_owner] = 1000.0;
    out = before;
    q.inprocess().order_candidates(k, qe, out.data(), n);
    Lit chb_out_lit;
    assert(lit_of_instance(k, out.front(), &chb_out_lit));
    const Var chb_out_compact = compact_position[var_of(chb_out_lit)];
    assert(compact_order[chb_out_compact >> 1] == chb_owner &&
           "TransitiveReduce must use the selected MAB stable heap");
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
  kernel_reserve_vars(k, 8);
  // All 4-subsets of eight literals: every complete physical population is 35
  // (below the accepted cap), while each candidate shares several literals
  // with earlier partners.  This reaches the partner meter without relying on
  // the historical growing-index connection bug.
  for (uint32_t a = 0; a < 5; ++a)
    for (uint32_t b = a + 1; b < 6; ++b)
      for (uint32_t c = b + 1; c < 7; ++c)
        for (uint32_t d = c + 1; d < 8; ++d) {
          const Lit c4[4] = {make_lit(a, true), make_lit(b, true),
                             make_lit(c, true), make_lit(d, true)};
          kernel_add_large_clause(k, c4, 4, false, 0, 0);
        }

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
         "the dynamic Vivify tier is unwired again (SCHED-C1)");
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

  Kernel selected;
  kernel_reserve_vars(selected, 64);
  ScopedHeuristics selected_h(selected);
  selected.h->stable = true;
  selected.stats.conflicts = 1;
  kernel_add_large_clause(selected, clause, 3, true, 20, 0);
  DefaultStrategyConfig config;
  config.inprocess.vivify_tier_mode = 1;
  DefaultPolicy selected_policy(config);
  selected_policy.inprocess().build(selected);
  DefaultInprocessScheduler::SlotEntry *selected_vivify = nullptr;
  for (auto &e : selected_policy.inprocess().entries) {
    const bool is_vivify = !std::strcmp(rules[e.rule].name, "Vivify");
    e.enabled = is_vivify;
    if (is_vivify)
      selected_vivify = &e;
  }
  assert(selected_vivify);
  selected.tier1_by_mode[0] = 20;
  selected.tier2_by_mode[0] = 38;
  selected.tier1_by_mode[1] = 19;
  selected.tier2_by_mode[1] = 46;
  assert(selected_policy.inprocess().run_phase(
             selected, SlotGroup::Probe, nullptr) == 0);
  assert(selected_vivify->tm.admitted > 0 &&
         selected.tier1 == selected.tier1_by_mode[1] &&
         selected.tier2 == selected.tier2_by_mode[1] &&
         "typed Vivify tier mode must select the requested snapshot");
}

// Kissat's tier limits are cumulative absolute ceilings.  With no tier-1 or
// tier-2 candidates, their unused 3/10 + 3/10 shares therefore remain
// available to tier 3 instead of being discarded before that call.
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
  p.inprocess().build(k);
  const auto &rules = tactix_rules();
  for (auto &e : p.inprocess().entries)
    e.enabled = !std::strcmp(rules[e.rule].name, "Vivify");

  assert(p.inprocess().run_phase(k, SlotGroup::Probe, nullptr) == 0);
  std::printf("  vivify cumulative tiers: checked=%llu tier3=%llu ticks=%llu\n",
              (unsigned long long)k.stats.vivify_checked,
              (unsigned long long)k.stats.vivify_tier3,
              (unsigned long long)k.stats.vivify_ticks);
  assert(k.stats.vivify_tier3 == 8 &&
         "tier 3 lost the unused cumulative tier-1/tier-2 allowance");
}

// When the focused tier-1 and tier-2 glue limits coincide, tier 2 is empty:
// Kissat adds its share to tier 1 and never runs the tier-2 round, so a dose
// is tier 1, tier 3 and the irredundant pass (vivify.c:kissat_vivify).
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
  k.tier1_by_mode[0] = k.tier2_by_mode[0] = 2;
  k.tier1_by_mode[1] = k.tier2_by_mode[1] = 2;
  k.tier1 = k.tier2 = 2;

  DefaultStrategyConfig cfg;
  cfg.inprocess.minimum_effort_ticks = 1000;
  cfg.inprocess.phase_rounds = 1;
  cfg.inprocess.vivify_permille = 1000;
  DefaultPolicy p(cfg);
  p.inprocess().build(k);
  const auto &rules = tactix_rules();
  for (auto &e : p.inprocess().entries)
    e.enabled = !std::strcmp(rules[e.rule].name, "Vivify");

  assert(p.inprocess().run_phase(k, SlotGroup::Probe, nullptr) == 0);
  std::printf("  vivify tier merge: rounds=%llu checked=%llu\n",
              (unsigned long long)k.stats.vivify_rounds,
              (unsigned long long)k.stats.vivify_checked);
  assert(k.stats.vivify_rounds == 3 &&
         "equal tier limits must fold tier 2 into tier 1 without a round");
  assert(k.stats.vivify_checked == 8);
}

// Kissat's irredundant productivity numerator counts every changed candidate,
// including a pure subsumption.  Counting only replacement clauses would
// raise vivifyirr's delay after a maximally productive subsumption-only dose.
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

  assert(p.inprocess().run_phase(k, SlotGroup::Probe, nullptr) == 0);
  assert(k.stats.vivify_subsumed == 1 && k.stats.vivify_shrunken == 0);
  assert(vivify->delay_current == 0 && vivify->delay_count == 0 &&
         "a successful subsumption was omitted from Vivify productivity");
}

// kissat rates a Sweep call by `kissat_average (eliminated, swept)`, where
// `swept` counts the scheduled VARIABLES the call processed.  Every seed
// solves several candidate-pair directions, so dividing by those pairs
// instead judged productive calls unproductive, bumped the delay, and skipped
// the following Sweep (Rank 33: the second probe round's Sweep never ran and
// missed the equivalence Kissat proves there).
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
  // Twelve more variables hang off the class through ternaries that force
  // nothing, so every seed's environment holds many partition classes and
  // solves several candidate-pair directions per swept variable.
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
  p.inprocess().build(k);
  DefaultInprocessScheduler::SlotEntry *sweep = nullptr;
  const auto &rules = tactix_rules();
  for (auto &e : p.inprocess().entries) {
    const bool selected = !std::strcmp(rules[e.rule].name, "Sweep") ||
                          !std::strcmp(rules[e.rule].name, "Substitute");
    e.enabled = selected;
    if (!std::strcmp(rules[e.rule].name, "Sweep"))
      sweep = &e;
  }
  assert(sweep);
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
  // The first round sees x1 == x2. Rewriting x2 to x1 shrinks the two large
  // clauses into the two binary directions of x1 == x3, which only the
  // optional second round can consume.
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
  std::printf("  substitute effort: fresh per-call, round-local 10-permille gate\n");
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
  std::printf("  substitute overshoot: mandatory first round, optional second\n");
}

static void test_rule_boundary_deactivates_root_before_substitute_member(void) {
  Kernel k;
  ScopedHeuristics h(k);
  k.h->stable = true;
  kernel_reserve_vars(k, 4);
  // x1 == x2, while (x1 v x2) becomes the unit x1 during substitution.
  // Kissat fixes/deactivates x1 while rewriting, then deactivates substituted
  // x2 in remove_representative_equivalences.  On the all-equal-score heap
  // [x1,x2,x3,x4], that ordered pair leaves [x4,x3]; reversing or batching
  // the transitions leaves a different legal heap and later decision stream.
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
  std::printf("  rule lifecycle: root fixed before substituted heap removal\n");
}

// --- the strategy-owned programs cover the registry ------------------------
// The slot order and the separate startup root composition are strategy data.
// An unnamed rule would otherwise be appended in registry order and acquire an
// accidental position. Every Inprocess-tier rule needs an authored owner.
static void test_authored_slot_order_covers_registry(void) {
  size_t norder = 0;
  const SlotOrderRow *order = default_slot_order(&norder);
  const DefaultStrategyConfig defaults;
  const std::vector<RuleVTable> &rules = tactix_rules();
  for (const RuleVTable &r : rules) {
    if (r.tier != RuleTier::Inprocess)
      continue;
    // X5's only current owner is its explicit variable-length public action.
    // It has no default producer, so no slot is inserted ahead of N4 review.
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
  std::printf("  authored slot/root programs: %zu slot rows, registry-complete\n",
              norder);
}

// Inprocessing, Lucky, and Walk are root-formula phases. A strategy that is
// currently searching must compose an explicit Restart before they become
// legal. Other H-only operations remain available during search.
static void test_inprocess_registry_requires_root(void) {
  Kernel k;
  kernel_reserve_vars(k, 4);
  ScopedHeuristics heuristics(k);
  for (const RuleVTable &rule : tactix_rules()) {
    if (rule.tier != RuleTier::Inprocess)
      continue;
    assert(rule.guard(k));
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

  // Pin the exact two Kissat Probe poses.  Vivify is deliberately absent from
  // `probe_initially` (probe.c:54-139), but is unconditional between the
  // early Backbone and Sweep in every search-time Probe (probe.c:35-43).
  static constexpr const char *SEARCH_PROBE[] = {
      "Congruence", "Substitute", "Backbone", "Vivify", "Sweep",
      "Substitute", "TransitiveReduce", "Backbone", "Factor",
  };
  static constexpr const char *INITIAL_PROBE[] = {
      "Congruence", "Substitute", "Backbone", "Sweep", "Substitute",
      "Factor",
  };
  std::vector<const char *> search_probe, initial_probe;
  for (const auto &entry : p.inprocess().entries) {
    if (!entry.enabled || entry.group != SlotGroup::Probe)
      continue;
    const char *name = rules[entry.rule].name;
    search_probe.push_back(name);
    if (entry.preprocess)
      initial_probe.push_back(name);
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
  std::printf("  slot membership/order: exact Kissat startup/search Probe; "
              "seven extra rules poseable\n");
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
  std::printf("  structured root telemetry: runs=%llu applications=%llu "
              "effective=%llu passes=%llu terminal=%u\n",
              (unsigned long long)telem.runs,
              (unsigned long long)telem.applications,
              (unsigned long long)telem.effective,
              (unsigned long long)telem.sequence_passes,
              static_cast<unsigned>(telem.terminal));
  assert(telem.runs == 1 && telem.applications > 0 &&
         telem.effective > 0);
  assert(telem.terminal == strategy::InvocationStatus::Unsat);
  assert(k.inconsistent);
  assert(k.stats.lucky_early_calls == (before_lucky ? 0u : 1u));
  assert(!k.inprocessing && !k.phase_saving_suppressed);
  kernel_check_invariants(k);
  std::printf("  structured root: typed SR sequence reaches checked state\n");
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
  std::printf("  structured root boundary: propagation and H published once\n");
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
// the separate first-search elimination deadline from eliminateinit=500 in
// start_search, after preprocessing, and Eliminate is due at equality (unlike
// Probe's strict-after guard).
static void test_initial_elimination_deadline_matches_kissat(void) {
  Kernel k;
  kernel_reserve_vars(k, 12);
  const Lit a[3] = {internalize(1), internalize(2), internalize(3)};
  const Lit b[3] = {internalize(4), internalize(5), internalize(6)};
  kernel_add_large_clause(k, a, 3, false, 0);
  kernel_add_large_clause(k, b, 3, false, 0);
  DefaultPolicy p;
  p.inprocess().build(k);

  // Model a preprocessing population change after the scheduler has been
  // built.  The search deadline must observe this final population, not the
  // earlier two-clause state.
  const Lit c[3] = {internalize(7), internalize(8), internalize(9)};
  kernel_add_large_clause(k, c, 3, false, 0);
  p.inprocess().initialize_search_limits(k);

  const double f =
      std::log10(static_cast<double>(kernel_irredundant_clauses(k)) + 10) - 5;
  const uint64_t delta = static_cast<uint64_t>(
      (4.5 * f * f + 25) *
      static_cast<double>(DefaultInprocessScheduler::SUBSUME_INTERVAL));
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
  std::printf("  initial elimination: post-preprocess due at conflict %llu\n",
              (unsigned long long)delta);
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
  std::printf("  BVE ordering: structural score plus search relevancy\n");
}

static void test_bve_score_preserves_kissat_binary64_pose() {
  // This adjacent pair is source-shaped from a stable BVE schedule. Both have
  // four occurrences on each polarity. Kissat's named intermediate pose rounds
  // their final scores equal; a flattened left-associated expression rounds
  // the first one ULP lower and reverses their eventual heap pop.
  constexpr double relevance_a = 0x1.800cf1d2bfffep+2;
  constexpr double relevance_b = 0x1.800cf1d2bfffdp+2;
  const double score_a = kissat_bve_variable_score(relevance_a, 4, 4, 2000);
  const double score_b = kissat_bve_variable_score(relevance_b, 4, 4, 2000);
  assert(score_a == score_b);
  assert(-score_a == 0x1.e8478ffe61c5bp+21);
  std::printf("  BVE score: Kissat named-intermediate binary64 pose\n");
}

// `kissat_scale_delta` receives C+1 and calls kissat_logn, whose definition
// adds nine again.  This corpus-sized boundary catches the easy-to-miss
// log10(C+1) transliteration: it rounds the second Probe deadline one conflict
// late (8279 rather than Kissat's 8278).
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
  std::printf("  inprocess scale: Kissat C+1 then logn(+9) boundary\n");
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
  // This fixture adds a clause between phases. Structured startup may fix a
  // free variable by its negation symmetry, which is sound only for the
  // formula it saw, so isolate the Congruence delay from it.
  DefaultStrategyConfig cfg;
  cfg.inprocess.structured_root_stages.clear();
  DefaultPolicy p(cfg);
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

  // New formula material does not bypass the named delay. Congruence has no
  // separate production closure/new-material admission gate in Kissat.
  kernel_add_binary_clause(k, internalize(1), internalize(2), 51);
  k.stats.conflicts = 1;
  assert(p.inprocess().run_phase(k, SlotGroup::Probe, nullptr) == 0);
  assert(congruence->tm.admitted == 1 && congruence->tm.dec_delay == 1);
  assert(congruence->delay_count == 0);

  // Once the named delay is consumed, the next admitted Probe phase calls
  // Congruence even though no additional binary or root material appeared.
  // A generic closure watermark here would be an extra production consumer
  // absent from Kissat.
  k.stats.conflicts = 2;
  assert(p.inprocess().run_phase(k, SlotGroup::Probe, nullptr) == 0);
  assert(congruence->tm.admitted == 2);
  assert(congruence->tm.dec_material == 0);
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
  std::printf("  slot gates: decline without new material, re-admit with it\n");
}

// Kissat's Reduce timestamp suppresses both phase predicates only at that
// conflict. It is independent of Probe's reduction-count watermark and of
// Eliminate's new-material watermark.
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
  std::printf("  Reduce edge: both phases suppressed only at its conflict\n");
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

  // SET_EFFORT_LIMIT scales the complete reference and truncates once.  A
  // divide-first implementation silently discards this remainder and gives
  // every non-Substitute slot 53 fewer ticks for this source-backed example.
  k.stats.search_ticks = 191898539;
  e.meter = [](const Kernel &) -> uint64_t { return 0; };
  p.inprocess().set_entry_limit(e, k);
  assert(e.limit == 19189853 &&
         "per-mille scaling truncated the reference before multiplication");
  std::printf("  effort limit: kissat's formula, phase anchor, no clamps\n");
}

static void test_first_search_effort_keeps_input_propagation_ticks(void) {
  Kernel k;
  kernel_reserve_vars(k, 64);
  // Input-unit closure happens before policy.solve but is charged by Kissat's
  // ordinary search propagator.  Its initial Probe/Eliminate anchors are zero,
  // so these ticks belong to the first search-time effort reference.
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
  std::printf("  initial effort anchors: input propagation remains priced\n");
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
  std::printf("  Eliminate sparse resume: root suffix replay uses search ticks\n");
}

static void test_preprocess_is_reduced_and_bounded(void) {
  Kernel k;
  kernel_reserve_vars(k, 64);
  ScopedHeuristics heuristics(k);
  DefaultPolicy p;
  p.inprocess().build(k);
  std::vector<const char *> offered;
  const auto &rules = tactix_rules();
  for (const auto &e : p.inprocess().entries)
    if (e.preprocess)
      offered.push_back(rules[e.rule].name);
  static const char *const expected[] = {
      "Congruence", "Substitute", "Backbone", "Sweep", "Substitute",
      "Factor"};
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
  assert(policy.inprocess().run_initial_fast_elimination(k, nullptr) == 0);
  assert(k.stats.forward_subsumed == 1 &&
         "fastel local cleanup must use SubsumeForward");
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
  std::printf("  fastel: typed local subsumption plus bounded BVE only\n");
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
  std::printf("  fastel discovery: mandatory scan declines before mutation\n");
}

static void test_initial_fast_elimination_keeps_owner_candidate_order(void) {
  Kernel k;
  kernel_reserve_vars(k, 10);

  // Two linked pivots have the same ordinary BVE structural score.  The
  // owner's fastel score is twice the positive occurrence count, so y must be
  // tried first (4 < 6).  Eliminating either pivot grows the other past the
  // eight-resolvent bound, making the first choice directly observable.
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

  // Keep the support variables behind x/y in the owner's radix order while
  // making the candidate domain a majority of the live formula. Each support
  // receives four positive ring occurrences, so y's score 4 precedes x's 6,
  // and both precede every support score.
  for (int external = 3; external <= 10; ++external)
    for (int offset = 1; offset <= 4; ++offset) {
      const int other = 3 + (external - 3 + offset) % 8;
      kernel_add_binary_clause(k, internalize(external), internalize(other));
    }

  ScopedHeuristics heuristics(k);
  k.h->stable = true;
  heuristics_score_set(k, var_of(x), 0.0);
  heuristics_score_set(k, var_of(y), 100.0);

  DefaultStrategyConfig config;
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
                                config.inprocess.bve_permille / 1000 &&
             "startup fastel must use the ordinary typed BVE effort dose");
  kernel_check_invariants(k);
  std::printf("  fastel candidate order: owner score survives BVE dispatch\n");
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
  cfg.inprocess.bve_clause_size_limit = 41;
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
  assert(k.bve_clause_size_limit == 41 &&
         "DefaultPolicy must wire the named BVE antecedent-size limit");
  std::printf("  inprocess phases: probe assignments leave saved phases unset\n");
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
  std::printf("  probe entry: root restart suppresses target/best snapshots\n");
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
  std::printf("  jump reasons: Kissat small/bigbig classification gates search\n");
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
  std::printf("  preprocess followups: typed arm calls both entries\n");
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

  DefaultStrategyConfig config;
  config.inprocess.structured_root_before_lucky = false; // reference collection boundary
  DefaultPolicy p(config);
  assert(p.solve(k) == 20);
  assert(k.stats.sparse_collections == 1);
  // The sparse collector removes the satisfied large clause, rewrites the
  // shortened one, and drops the three root-satisfied binary pairs first.
  assert(k.stats.sparse_collected == 5);
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
  // Eliminate is a search-time phase: DefaultPolicy's BVE entry defers
  // preprocessing (kissat fastel=0) and slot admission declines every
  // non-preprocess entry while `conflicts == 0`.  Pose the search-time call.
  k.stats.conflicts = 1;
  // The dosed loop consults the retry advice only after ranking.  Holding it
  // at zero keeps every candidate out of the dose, so the formula survives
  // and the exit's sparse restoration is observable rather than vacuous.
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
  std::printf("  eliminate entry: sparse watches flushed, dense image only, "
              "restored at exit\n");
}

int main() {
  test_eliminate_owns_physical_dense_transition();
  test_mode_growth_key_matches_kissat();
  test_reason_bump_depth_matches_kissat();
  test_reason_bump_delay_lives_at_final_clause_gate();
  test_analysis_trail_reuse_defaults_to_kissat();
  test_factor_material_gate_uses_literal_marks();
  test_large_formula_factor_decline_is_solve_latched();
  test_retier_schedule_advances_only_on_otfs_conflicts();
  test_effort_limit_is_kissats();
  test_first_search_effort_keeps_input_propagation_ticks();
  test_eliminate_sparse_resume_replays_root_suffix_as_search();
  test_preprocess_is_reduced_and_bounded();
  test_initial_fast_elimination_composes_only_proof_rules();
  test_initial_fast_elimination_budgets_discovery_before_mutation();
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
