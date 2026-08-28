// The pose surface for the inprocessing slot (schedule-as-data, plan §5.6): a
// silently inert knob makes an A/B arm vacuous. Pinned: a preposed
// `inprocess_slot` is used verbatim and collects telemetry; a disabled entry is
// reached, declines as disabled at every occasion, and leaves its rule's own
// counter at 0 while the enabled arm's moves; and the order and per-entry
// setters land on the entry the policy walks. The `reject()` abort paths are
// not covered — they need a subprocess to observe.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "rules/registry.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace tactix;

// Two identical AND gates over the same inputs, so Congruence has a real merge
// to make at the conflicts==0 preprocessing phase. Satisfiable.
static void fixture(Kernel &k) {
  kernel_reserve_vars(k, 8);
  const auto cl3 = [&](int a, int b, int c) {
    const Lit l[3] = {internalize(a), internalize(b), internalize(c)};
    kernel_add_large_clause(k, l, 3, false, 0);
  };
  // 5 <-> (1 & 2)
  kernel_add_binary_clause(k, internalize(-5), internalize(1));
  kernel_add_binary_clause(k, internalize(-5), internalize(2));
  cl3(5, -1, -2);
  // 6 <-> (1 & 2), the same gate: 5 and 6 are congruent
  kernel_add_binary_clause(k, internalize(-6), internalize(1));
  kernel_add_binary_clause(k, internalize(-6), internalize(2));
  cl3(6, -1, -2);
  // search material, so the solve is not decided by propagation alone
  cl3(7, 8, -5);
  cl3(-7, 8, 6);
  cl3(7, -8, 1);
}

static uint32_t slot_index(const DefaultPolicy &p, const char *name) {
  const std::vector<RuleVTable> &rules = tactix_rules();
  for (size_t i = 0; i < p.inprocess().entries.size(); i++)
    if (!std::strcmp(rules[p.inprocess().entries[i].rule].name, name))
      return static_cast<uint32_t>(i);
  return UINT32_MAX;
}

static void test_substitute_hand_rolled_budget() {
  Kernel k;
  Heuristics h;
  k.h = &h;
  k.stats.conflicts = 1;
  k.stats.search_ticks = 999;
  DefaultPolicy p;
  p.inprocess().build(k);
  const uint32_t i = slot_index(p, "Substitute");
  assert(i != UINT32_MAX);
  DefaultInprocessScheduler::SlotEntry &e = p.inprocess().entries[i];
  assert(e.permille == 10 && e.meter);
  p.inprocess().set_entry_limit(e, k);
  assert(e.limit == 9 &&
         "Substitute's 10-per-mille budget must not use the 1e7 floor");
}

// The budget knob must not be inert: with a zero allowance, the round-local
// effort comparison (kissat substitute.c substitute_rounds) suppresses the
// optional second round of the call, while the first round stays mandatory —
// there is no call-entry effort gate, so dec_budget stays 0 and the break is
// visible as the call left incomplete. phase_rounds=1 isolates one authored
// call; each later call would own a fresh mandatory round.
static void test_substitute_budget_stops_second_round() {
  Kernel k;
  Heuristics h;
  k.h = &h;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(1), internalize(-2));
  k.stats.conflicts = 1;
  k.stats.search_ticks = 1; // 10 per mille rounds down to zero, with no floor

  DefaultStrategyConfig cfg;
  cfg.inprocess.phase_rounds = 1;
  DefaultPolicy p(cfg);
  p.inprocess().build(k);
  const uint32_t i = slot_index(p, "Substitute");
  assert(i != UINT32_MAX);
  for (uint32_t j = 0; j < p.inprocess().entries.size(); j++)
    p.inprocess().entries[j].enabled = j == i;
  assert(p.inprocess().run_phase(k, SlotGroup::Probe, nullptr) != 20);
  const auto &e = p.inprocess().entries[i];
  assert(k.stats.substitute_rounds == 1 &&
         "an over-budget productive first round must suppress only the "
         "optional second round of the same call");
  assert(k.stats.substitute_ticks > 0);
  assert(e.tm.admitted == 1 && e.tm.dec_budget == 0 &&
         "Substitute has no call-entry effort gate");
  assert(e.tm.handed == 0 && e.tm.completed == 0 &&
         "the zero allowance must land on the entry and mark the "
         "over-budget call incomplete");
}

// --- posed BVE ladder drives Factor's phase snapshot -----------------------
// An irregular injected ladder detects both ways this wire previously went
// inert: advancing with a hardcoded doubling sequence and stopping at a
// hardcoded ceiling.  FactorMatchPolicy::set_bound observes the snapshot taken
// at phase entry, before that phase's BVE dose advances the level.
class PosedBveLadder final : public BveBoundPolicy {
public:
  const char *name() const override { return "posed-0-3-5-13"; }
  uint32_t num_passes() const override { return 4; }
  uint32_t bound(uint32_t pass) const override {
    static constexpr uint32_t levels[4] = {0, 3, 5, 13};
    assert(pass < 4);
    return levels[pass];
  }
};

class BoundRecorder final : public FactorMatchPolicy {
public:
  const char *name() const override { return "bound-recorder"; }
  void set_bound(uint32_t bound) override { seen.push_back(bound); }
  double seed_score(const Kernel &, Lit, double) override { return 0; }
  double next_factor_score(const Kernel &, Lit, uint32_t, Lit,
                           double) override {
    return 0;
  }
  bool accept(uint32_t, uint32_t, int) override { return false; }

  std::vector<uint32_t> seen;
};

static uint64_t zero_meter(const Kernel &) { return 0; }

static void test_factor_bound_walks_posed_bve_ladder() {
  Kernel k;
  Heuristics h;
  k.h = &h;
  k.stats.conflicts = 1;

  PosedBveLadder ladder;
  BoundRecorder recorder;
  DefaultPolicy p;
  p.inprocess().bve_bound = &ladder;
  p.inprocess().factor_match = &recorder;

  DefaultInprocessScheduler::SlotEntry bve;
  bve.rule = tactix_rule_id("BVE");
  assert(bve.rule >= 0);
  bve.group = SlotGroup::Eliminate;
  bve.dosed = true;
  bve.meter = zero_meter;
  p.inprocess().entries.push_back(bve);

  static constexpr uint32_t expected[5] = {0, 3, 5, 13, 13};
  for (const uint32_t bound : expected) {
    assert(p.inprocess().run_phase(k, SlotGroup::Eliminate, nullptr) != 20);
    assert(k.bve_bound == bound &&
           "BVE did not use the posed ladder's current level");
  }
  assert(recorder.seen.size() == 5);
  for (size_t i = 0; i < recorder.seen.size(); i++)
    assert(recorder.seen[i] == expected[i] &&
           "Factor did not receive BVE's posed phase-entry bound");
  assert(p.inprocess().entries[0].tm.admitted == 5 &&
         "the posed BVE dose must engage in every tested phase");
  std::printf("  SP-FACTOR-1: posed BVE ladder 0,3,5,13,13 reached Factor\n");
}

// --- pose / disable / re-enable --------------------------------------------
// `enabled` is the policy declining to schedule, not the rule declining to
// apply (N1), so the rule's own counter is the observable. The phase runs
// through `run_slot_phase`: via solve() an untaken macro trigger would leave
// occasions == 0, which reads exactly like a working disable.
static uint64_t pose_congruence(bool enabled, DefaultPolicy &p, Kernel &k) {
  k.stats.conflicts = 1;
  const int cong = tactix_rule_id("Congruence");
  const int subst = tactix_rule_id("Substitute");
  assert(cong >= 0 && subst >= 0);
  DefaultInprocessScheduler::SlotEntry a;
  a.rule = cong;
  a.enabled = enabled;
  a.group = SlotGroup::Eliminate;
  a.closure = true;
  DefaultInprocessScheduler::SlotEntry b; // a second entry, so the slot is a real schedule
  b.rule = subst;
  b.enabled = true;
  b.group = SlotGroup::Eliminate;
  b.closure = true;
  p.inprocess().entries.push_back(a);
  p.inprocess().entries.push_back(b);
  assert(p.inprocess().run_phase(k, SlotGroup::Eliminate, nullptr) != 20);
  return k.stats.congruence_merges;
}

static void test_pose_and_disable() {
  Kernel k;
  Heuristics h;
  k.h = &h;
  fixture(k);
  DefaultPolicy on;
  const uint64_t merges_on = pose_congruence(true, on, k);
  assert(on.inprocess().entries.size() == 2 &&
         "a preposed slot must be used verbatim, not rebuilt from the registry");
  assert(slot_index(on, "Congruence") == 0 && slot_index(on, "Substitute") == 1 &&
         "a preposed slot's ORDER must survive too");
  assert(merges_on > 0 &&
         "the fixture must give Congruence something to merge, or the disabled "
         "arm below proves nothing");
  const DefaultInprocessScheduler::SlotEntry &e_on = on.inprocess().entries[0];
  assert(e_on.tm.occasions > 0 && e_on.tm.admitted > 0);
  assert(e_on.tm.dec_disabled == 0);

  Kernel k2;
  Heuristics h2;
  k2.h = &h2;
  fixture(k2);
  DefaultPolicy off;
  const uint64_t merges_off = pose_congruence(false, off, k2);
  const DefaultInprocessScheduler::SlotEntry &e_off = off.inprocess().entries[0];
  assert(e_off.tm.occasions > 0 &&
         "the disabled entry was never even REACHED, so this arm shows that the "
         "slot loop skipped it, not that `enabled` works");
  assert(e_off.tm.dec_disabled == e_off.tm.occasions &&
         "a disabled entry must decline for the DISABLED reason at every "
         "occasion — a decline counted elsewhere means something else stopped it");
  assert(e_off.tm.admitted == 0 && e_off.tm.ok == 0);
  assert(merges_off == 0 &&
         "the rule ran despite being disabled (counter at the source)");
  std::printf("  pose/disable: %llu merges enabled, %llu disabled; %llu "
              "occasions all declined as DISABLED\n",
              (unsigned long long)merges_on, (unsigned long long)merges_off,
              (unsigned long long)e_off.tm.occasions);
}

// --- the environment surface -----------------------------------------------
// Each arm gets a fresh policy, so the slot is rebuilt with the environment on.
static void with_env(const char *const *kv, size_t n,
                     void (*check)(DefaultPolicy &, Kernel &)) {
  for (size_t i = 0; i < n; i += 2)
    setenv(kv[i], kv[i + 1], 1);
  {
    // one solve to build the slot from the environment, then one phase on a
    // fresh formula through it: checked at the entry and at the rule counter
    Kernel kb;
    Heuristics hb;
    kb.h = &hb;
    fixture(kb);
    DefaultPolicy p;
    assert(p.solve(kb) == 10);
    assert(!p.inprocess().entries.empty());
    Kernel k;
    Heuristics h;
    k.h = &h;
    fixture(k);
    k.stats.conflicts = 1;
    assert(p.inprocess().run_phase(k, SlotGroup::Eliminate, nullptr) != 20);
    assert(p.inprocess().run_phase(k, SlotGroup::Probe, nullptr) != 20);
    check(p, k);
  }
  for (size_t i = 0; i < n; i += 2)
    unsetenv(kv[i]);
}

static void check_disable(DefaultPolicy &p, Kernel &k) {
  const uint32_t i = slot_index(p, "Congruence");
  assert(i != UINT32_MAX);
  assert(!p.inprocess().entries[i].enabled &&
         "TACTIX_SLOT_DISABLE did not reach the entry the policy walks");
  assert(p.inprocess().entries[i].tm.occasions > 0 &&
         p.inprocess().entries[i].tm.dec_disabled == p.inprocess().entries[i].tm.occasions);
  assert(k.stats.congruence_merges == 0);
}

static void check_enable_over_ablation(DefaultPolicy &p, Kernel &) {
  const uint32_t i = slot_index(p, "BVE");
  assert(i != UINT32_MAX);
  assert(p.inprocess().entries[i].enabled &&
         "TACTIX_SLOT_ENABLE must override the TACTIX_NO_BVE ablation knob");
}

static void check_enable_default_off(DefaultPolicy &p, Kernel &) {
  const uint32_t i = slot_index(p, "SubsumeForward");
  assert(i != UINT32_MAX);
  assert(p.inprocess().entries[i].enabled &&
         "TACTIX_SLOT_ENABLE must expose default-off standalone C1");
}

static void check_legacy_order(DefaultPolicy &p, Kernel &) {
  const size_t n = p.inprocess().entries.size();
  assert(n >= 2);
  const uint32_t bve = slot_index(p, "BVE"), factor = slot_index(p, "Factor");
  assert(factor == n - 1 && bve == n - 2 &&
         "TACTIX_SLOT_ORDER=legacy must move BVE then Factor to the back");
}

static void check_authored_order(DefaultPolicy &p, Kernel &) {
  const size_t n = p.inprocess().entries.size();
  const uint32_t factor = slot_index(p, "Factor");
  assert(factor != n - 1 &&
         "the authored order is what `legacy` is an arm AGAINST; if they agree "
         "the order A/B is vacuous");
}

static void check_knobs(DefaultPolicy &p, Kernel &) {
  const uint32_t sw = slot_index(p, "Sweep");
  assert(sw != UINT32_MAX);
  assert(p.inprocess().entries[sw].meter != nullptr &&
         "the PERMILLE knob must be checked on a metered entry");
  assert(p.inprocess().entries[sw].permille == 7 &&
         "TACTIX_SLOT_PERMILLE did not reach the entry");
  // Congruence is unmetered like Kissat's: no EFFORT_SPECS row, so the
  // per-mille knob cannot reach it. Kissat's named productivity delay remains
  // its governor; the generic closure/new-material gate is not in the default.
  const uint32_t c = slot_index(p, "Congruence");
  assert(c != UINT32_MAX);
  assert(p.inprocess().entries[c].meter == nullptr &&
         p.inprocess().entries[c].permille == 0 &&
         !p.inprocess().entries[c].closure &&
         "Congruence must stay unmetered without an invented material gate; "
         "TACTIX_SLOT_PERMILLE must not meter it");
  assert(p.inprocess().entries[c].max_applies == 1 &&
         "TACTIX_SLOT_MAX_APPLIES did not reach the entry");
  const uint32_t pr = slot_index(p, "Probe");
  assert(!p.inprocess().entries[pr].cursor_enabled &&
         "TACTIX_CURSOR_PROBE=0 did not reach the entry (it defaults ON)");
  assert(p.inprocess().entries[pr].order == CandidateOrder::Reverse &&
         "TACTIX_ORDER_PROBE=reverse did not reach the entry");
}

int main() {
  test_substitute_hand_rolled_budget();
  test_substitute_budget_stops_second_round();
  test_factor_bound_walks_posed_bve_ladder();
  test_pose_and_disable();

  const char *disable[] = {"TACTIX_SLOT_DISABLE", "Congruence"};
  with_env(disable, 2, check_disable);
  std::printf("  TACTIX_SLOT_DISABLE=Congruence: entry off, 0 merges\n");

  const char *enable[] = {"TACTIX_NO_BVE", "1", "TACTIX_SLOT_ENABLE", "BVE"};
  with_env(enable, 4, check_enable_over_ablation);
  std::printf("  TACTIX_SLOT_ENABLE overrides TACTIX_NO_BVE\n");

  const char *enable_c1[] = {"TACTIX_SLOT_ENABLE", "SubsumeForward"};
  with_env(enable_c1, 2, check_enable_default_off);
  std::printf("  TACTIX_SLOT_ENABLE exposes default-off SubsumeForward\n");

  with_env(nullptr, 0, check_authored_order);
  const char *legacy[] = {"TACTIX_SLOT_ORDER", "legacy"};
  with_env(legacy, 2, check_legacy_order);
  std::printf("  TACTIX_SLOT_ORDER=legacy moves BVE/Factor to the back\n");

  const char *knobs[] = {"TACTIX_SLOT_PERMILLE",   "Sweep:7,Congruence:7",
                         "TACTIX_SLOT_MAX_APPLIES", "Congruence:1",
                         "TACTIX_CURSOR_PROBE",     "0",
                         "TACTIX_ORDER_PROBE",      "reverse"};
  with_env(knobs, 8, check_knobs);
  std::printf("  PERMILLE / MAX_APPLIES / CURSOR_* / ORDER_* all land on the "
              "posed entry\n");

  std::printf("test_slot_pose: all passed\n");
  return 0;
}
