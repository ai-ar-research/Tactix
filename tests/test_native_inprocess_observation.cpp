// Native phase decisions paired with observations available through tactix.h.
#undef NDEBUG
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix
#include "api/api_internal.hpp"
#include "policies/default_inprocess.hpp"
#include <cassert>
#include <algorithm>
#include <array>
#include <utility>
#include <string_view>
#include "proof/proof.hpp"
#include <iomanip>
#include <iostream>

using namespace tactix;
static void clock_json(const DefaultInprocessScheduler::SlotClock &c) {
  std::cout << "{\"next_conflicts\":" << c.next_conflicts << ",\"rounds\":" << c.rounds
    << ",\"reductions_mark\":" << c.reductions_mark << ",\"marks_mark\":" << c.marks_mark
    << ",\"fixed_mark\":" << c.fixed_mark << ",\"material_mark\":" << c.material_mark
    << ",\"blocked\":" << c.blocked << ",\"blocked_episodes\":" << c.blocked_episodes
    << ",\"anchor\":" << c.anchor << '}';
}
static void config_json(const DefaultStrategyConfig::Inprocess &cfg) {
  std::cout << std::setprecision(17) << "{\"split_clocks\":" << cfg.split_clocks
    << ",\"watermarks\":" << cfg.watermarks << ",\"preprocess_only\":" << cfg.preprocess_only
    << ",\"probe_interval\":" << cfg.probe_interval << ",\"eliminate_interval\":" << cfg.eliminate_interval
    << ",\"logarithm_offset\":" << cfg.logarithm_offset << ",\"scale_log_shift\":" << cfg.scale_log_shift
    << ",\"scale_quadratic\":" << cfg.scale_quadratic << ",\"scale_constant\":" << cfg.scale_constant
    << ",\"preprocess_initial_ticks\":" << cfg.preprocess_initial_ticks
    << ",\"minimum_effort_ticks\":" << cfg.minimum_effort_ticks
    << ",\"preprocess_followups\":" << cfg.preprocess_followups
    << ",\"factor_initial_ticks\":" << cfg.factor_initial_ticks
    << ",\"initial_fast_elimination_bve_permille\":" << cfg.initial_fast_elimination_bve_permille
    << '}';
}
static constexpr std::pair<const char *, int> stat_fields[] = {
  {"CONFLICTS", TACTIX_STAT_CONFLICTS}, {"REDUCTIONS", TACTIX_STAT_REDUCTIONS},
  {"BVE_MARKS", TACTIX_STAT_BVE_MARKS}, {"ROOT_FIXED", TACTIX_STAT_ROOT_FIXED},
  {"SUBSUME_MATERIAL", TACTIX_STAT_SUBSUME_MATERIAL},
  {"IRREDUNDANT_CLAUSES", TACTIX_STAT_IRREDUNDANT_CLAUSES},
  {"SEARCH_TICKS", TACTIX_STAT_SEARCH_TICKS}};
using Observation = std::array<uint64_t, std::size(stat_fields)>;
static Observation observe(tactix_c_handle *s) {
  Observation out;
  for (size_t i = 0; i < out.size(); ++i) out[i] = tactix_stat(s, stat_fields[i].second);
  return out;
}
static void observation_json(const Observation &o) {
  std::cout << '{';
  for (size_t i = 0; i < o.size(); ++i) {
    if (i) std::cout << ',';
    std::cout << '"' << stat_fields[i].first << "\":" << o[i];
  }
  std::cout << '}';
}
static void clocks_json(const DefaultInprocessScheduler::SlotClock &probe,
                        const DefaultInprocessScheduler::SlotClock &elim) {
  std::cout << "{\"Probe\":"; clock_json(probe);
  std::cout << ",\"Eliminate\":"; clock_json(elim); std::cout << '}';
}
static void emit(tactix_c_handle *s, const DefaultStrategyConfig::Inprocess &cfg,
                 bool initialized, bool reduced, uint64_t rounds, uint64_t conflicts,
                 unsigned changed, uint64_t elim_rounds = UINT64_MAX,
                 uint64_t fixed_mark = 0) {
  Kernel &k = api_detail::kernel_for_fixed_testing(s);
  k.stats.conflicts = conflicts;
  k.stats.reductions = 7 + (changed == 1) - (changed == 4);
  k.stats.bve_marks = 11 + (changed == 2) - (changed == 5);
  k.subsume_material_count = 17 + (changed == 3) - (changed == 6);
  DefaultInprocessScheduler native(&cfg);
  native.configure(); native.reset(k); native.pose_initialized(initialized);
  if (reduced) native.note_reduce(k);
  for (auto *c : {&native.probe_clock, &native.elim_clock}) {
    c->next_conflicts = conflicts ? 99 : 0;
    c->rounds = rounds; c->reductions_mark = 7; c->marks_mark = 11;
    c->fixed_mark = fixed_mark; c->material_mark = 17;
  }
  native.elim_clock.rounds = elim_rounds == UINT64_MAX ? rounds + 2 : elim_rounds;
  std::cout << "{\"kind\":\"clock\",\"config\":"; config_json(cfg);
  std::cout << ",\"initialized\":" << initialized << ",\"reduced\":" << reduced
    << ",\"clocks\":";
  clocks_json(native.probe_clock, native.elim_clock);
  std::cout << ",\"observation\":"; observation_json(observe(s));
  std::cout << ",\"checks\":["; bool comma = false;
  for (const auto group : {SlotGroup::Probe, SlotGroup::Eliminate}) {
    auto &c = group == SlotGroup::Probe ? native.probe_clock : native.elim_clock;
    for (unsigned repeat = 0; repeat < 3; ++repeat) {
      const bool due = native.want_phase(k, group);
      if (comma) std::cout << ',';
      std::cout << "{\"group\":\"" << (group == SlotGroup::Probe ? "Probe" : "Eliminate")
        << "\",\"due\":" << due << ",\"blocked\":" << c.blocked
        << ",\"episodes\":" << c.blocked_episodes << '}'; comma = true;
    }
  }
  native.update_limit(k, SlotGroup::Probe); native.update_limit(k, SlotGroup::Eliminate);
  std::cout << "],\"limits\":{\"Probe\":" << native.probe_clock.next_conflicts
    << ",\"Eliminate\":" << native.elim_clock.next_conflicts << "}}\n";
}
struct FixtureRamp final : BveBoundPolicy {
  uint32_t passes;
  explicit FixtureRamp(uint32_t count) : passes(count) {}
  const char *name() const override { return "phase-clock-fixture"; }
  uint32_t num_passes() const override { return passes; }
  uint32_t bound(uint32_t pass) const override { return pass; }
};
// Real native phases; only their public before/after observations reach Python.
static void emit_phase(SlotGroup group, uint64_t conflicts, const char *mode) {
  const bool maximum = (std::string_view(mode) == "maximum" || std::string_view(mode) == "maximum-growth");
  const bool advance = std::string_view(mode) == "advance";
  const bool fast = std::string_view(mode) == "fast";
  const bool early = std::string_view(mode) == "early-unsat";
  const bool late = std::string_view(mode) == "late-unsat";
  const bool material = std::string_view(mode) == "material-change";
  const bool growth = (std::string_view(mode) == "root-growth" || std::string_view(mode) == "maximum-growth");
  auto *s = tactix_init(); assert(s);
  const auto add = [&](std::initializer_list<int> clause) {
    for (int literal : clause) tactix_add(s, literal);
    tactix_add(s, 0);
  };
  if (early) { add({1}); add({-1}); }
  else if (late) { add({1, 2}); add({1, -2}); add({-1, 2}); add({-1, -2}); }
  else { add({1, 2}); add({1, -2}); add({-1, 3}); add({-3, 4}); }
  assert(tactix_status(s) == (early ? 20 : 0));
  Kernel &k = api_detail::kernel_for_fixed_testing(s);
  if (growth) {
    // F entails 1 by the first two clauses. Stage its licensed unit without
    // propagation, as a preceding inprocessing batch can do.
    const Lit unit = internalize(1);
    k.unit_ids[var_of(unit)] = ++k.next_clause_id;
    k.proof->add_derived_unit(k.next_clause_id, unit);
    kernel_assign_unit(k, unit);
  }
  k.stats.conflicts = conflicts;
  DefaultStrategyConfig config;
  FixtureRamp ramp(advance ? 2 : 1);
  DefaultInprocessScheduler native(&config.inprocess);
  native.configure(); native.reset(k); native.pose_initialized();
  native.bve_bound = &ramp;
  for (auto *clock : {&native.probe_clock, &native.elim_clock}) {
    clock->rounds = 2; clock->next_conflicts = 99;
    clock->material_mark = 123; clock->anchor = 7;
    clock->blocked = true; clock->blocked_episodes = 9;
  }
  const bool bve = maximum || advance;
  if (bve || fast || late || material) {
    DefaultInprocessScheduler::SlotEntry e;
    e.rule = tactix_rule_id(material ? "BCE" : late ? "Probe" : "BVE"); e.group = group;
    e.preprocess = material;
    e.max_applies = 1; e.meter = [](const Kernel &) -> uint64_t { return 0; };
    e.dosed = !late && !material; e.order = CandidateOrder::Reverse;
    native.entries.push_back(e);
    if (bve) std::fill(k.bve_retry.begin(), k.bve_retry.end(), uint8_t{0});
  }
  const auto before = observe(s);
  const auto before_probe = native.probe_clock, before_elim = native.elim_clock;
  const int result = fast ? native.run_initial_fast_elimination(k, nullptr)
                          : native.run_phase(k, group, nullptr);
  assert(result == (early || late ? 20 : 0));
  if (bve) {
    assert(native.entries.front().applies == 1 && native.entries.front().tm.admitted == 1);
    assert(k.stats.bve_eliminated == 0);
    assert(maximum ? native.elim_clock.material_mark == k.subsume_material_count
                   : native.elim_clock.marks_mark == 0 && native.elim_clock.fixed_mark == 0);
  }
  if (advance || fast)
    assert(std::all_of(k.bve_retry.begin(), k.bve_retry.end(), [](uint8_t flag) { return flag == 1; }));
  if (fast) assert(native.entries.front().applies > 0);
  const auto after = observe(s);
  if (growth) assert(after[3] > before[3]);
  if (material) assert(after[5] < before[5]);
  std::cout << "{\"kind\":\"phase\",\"mode\":\"" << mode << "\",\"group\":\""
    << (group == SlotGroup::Probe ? "Probe" : "Eliminate") << "\",\"config\":";
  config_json(config.inprocess);
  std::cout << ",\"before\":"; observation_json(before);
  std::cout << ",\"after\":"; observation_json(after);
  std::cout << ",\"clocks\":"; clocks_json(before_probe, before_elim);
  std::cout << ",\"finished\":"; clocks_json(native.probe_clock, native.elim_clock);
  std::cout << ",\"result\":" << result << ",\"bve_ran\":" << (bve || fast)
    << ",\"maximum_bound\":" << maximum << ",\"initial_fast_elimination\":" << fast
    << ",\"early\":" << early;
  if (fast) std::cout << ",\"fast_limit\":" << native.entries.front().limit;
  std::cout << "}\n";
  tactix_release(s);
}

// Exercise the actual scheduler budget method, including its shared meter identity.
static void emit_budgets() {
  auto *s = tactix_init(); assert(s);
  Kernel &k = api_detail::kernel_for_fixed_testing(s);
  DefaultStrategyConfig config;
  DefaultInprocessScheduler stock(&config.inprocess);
  stock.build(k);
  for (bool distinct : {false, true}) {
  config.inprocess.preprocess_initial_ticks = distinct ? 1234567 : 10000000;
  config.inprocess.minimum_effort_ticks = distinct ? 7654321 : 10000000;
  for (const char *name : {"BCE", "Substitute", "Factor", "SubsumeForward", "Strengthen"}) {
    const int rule = tactix_rule_id(name);
    auto found = std::find_if(stock.entries.begin(), stock.entries.end(),
                             [=](const auto &e) { return e.rule == rule; });
    assert(found != stock.entries.end());
    for (auto group : {SlotGroup::Probe, SlotGroup::Eliminate})
      for (uint64_t conflicts : {0ull, 1ull})
        for (uint64_t now : std::array<uint64_t, 8>{0, 999, 1001, 7654320, 7654321, 7654322, 100000000, UINT64_MAX})
          for (uint64_t anchor : std::array<uint64_t, 3>{0, 1000, UINT64_MAX})
            for (uint32_t permille : {0u, 1u, 50u, 1000u})
              for (bool first : {false, true})
                for (bool aware : {false, true}) {
      DefaultInprocessScheduler native(&config.inprocess);
      native.configure(); native.reset(k);
      native.probe_clock.anchor = group == SlotGroup::Probe ? anchor : 123;
      native.elim_clock.anchor = group == SlotGroup::Eliminate ? anchor : 456;
      k.stats.conflicts = conflicts; k.stats.search_ticks = now;
      auto e = *found;
      e.group = group; e.permille = permille; e.doses = first ? 0 : 1;
      e.budget_aware = aware; e.tm.wrapped = 0;
      const bool forward = std::string_view(name) == "SubsumeForward" || std::string_view(name) == "Strengthen";
      std::cout << "{\"kind\":\"budget\",\"config\":"; config_json(config.inprocess);
      std::cout << ",\"anchor\":" << anchor << ",\"observation\":"; observation_json(observe(s));
      std::cout << ",\"slot\":{\"rule\":\"" << name << "\",\"group\":\""
        << (group == SlotGroup::Probe ? "Probe" : "Eliminate") << "\",\"permille\":" << permille
        << ",\"doses\":" << e.doses << ",\"budget_aware\":" << aware
        << ",\"forward_pair\":" << forward << "},\"checks\":[";
      for (unsigned repeat = 0; repeat < 3; ++repeat) {
        if (repeat == 1 && forward) {
          const int other = tactix_rule_id(std::string_view(name) == "Strengthen" ? "SubsumeForward" : "Strengthen");
          e = *std::find_if(stock.entries.begin(), stock.entries.end(), [=](const auto &entry) { return entry.rule == other; });
          e.group = group; e.permille = 1000 - permille;
          e.tm.wrapped = 0;
        }
        k.stats.bce_ticks = k.stats.substitute_ticks = k.stats.factor_ticks =
          k.stats.forward_visits = repeat ? UINT64_MAX : 0;
        k.stats.strengthen_visits = repeat ? 17 : 0;
        const uint64_t meter = e.meter(k);
        DefaultInprocessScheduler fresh(&config.inprocess);
        fresh.configure(); fresh.reset(k);
        fresh.probe_clock.anchor = native.probe_clock.anchor;
        fresh.elim_clock.anchor = native.elim_clock.anchor;
        if (repeat == 2) fresh.set_entry_limit(e, k);
        else native.set_entry_limit(e, k);
        if (repeat) std::cout << ',';
        std::cout << "{\"meter\":" << meter << ",\"limit\":" << e.limit
          << ",\"wrapped\":" << e.tm.wrapped << ",\"reset\":" << (repeat == 2)
          << ",\"new_slot\":" << (repeat == 1 && forward)
          << ",\"rule\":\"" << tactix_rules()[e.rule].name << "\",\"permille\":" << e.permille << '}';
      }
      std::cout << "]}\n";
    }
  }
  }
  tactix_release(s);
}


static const char *followup_name(FollowupKind kind) {
  return kind == FollowupKind::AfterCongruence ? "AfterCongruence" :
         kind == FollowupKind::AfterSweepOrFinal ? "AfterSweepOrFinal" : "None";
}
static const char *mode_name(FollowupMode mode) {
  return mode == FollowupMode::Eager ? "Eager" : mode == FollowupMode::Final ? "Final" : "None";
}
static void emit_admission() {
  for (auto kind : {FollowupKind::AfterCongruence, FollowupKind::AfterSweepOrFinal})
    for (bool congruence : {false, true})
      for (bool sweep : {false, true}) {
    DefaultStrategyConfig cfg;
    std::cout << "{\"kind\":\"followup\",\"config\":"; config_json(cfg.inprocess);
    std::cout << ",\"followup\":\"" << followup_name(kind)
      << "\",\"congruence\":" << congruence << ",\"sweep\":" << sweep
      << ",\"mode\":\"" << mode_name(DefaultInprocessScheduler::preprocessing_followup_mode(kind, congruence, sweep)) << "\"}\n";
  }

  for (bool followups : {false, true})
    for (bool search : {false, true})
      for (bool enabled : {false, true})
        for (bool preprocess : {false, true})
          for (bool defer : {false, true})
            for (uint32_t cap : {0u, 1u})
              for (uint32_t applies : {0u, 1u, UINT32_MAX})
                for (auto kind : {FollowupKind::None, FollowupKind::AfterCongruence,
                                  FollowupKind::AfterSweepOrFinal}) {
    auto *s = tactix_init(); assert(s);
    for (int lit : {1, 2, 0, -1, 3, 0, -3, 4, 0}) tactix_add(s, lit);
    Kernel &k = api_detail::kernel_for_fixed_testing(s);
    k.stats.conflicts = search ? 1 : 0;
    DefaultStrategyConfig config;
    config.inprocess.phase_rounds = 1;
    config.inprocess.preprocess_followups = followups;
    DefaultInprocessScheduler native(&config.inprocess);
    native.configure(); native.reset(k); native.pose_initialized();
    DefaultInprocessScheduler::SlotEntry e;
    e.rule = tactix_rule_id("BCE"); e.enabled = enabled; e.preprocess = preprocess;
    e.defer_preprocess = defer; e.max_applies = cap; e.applies = applies; e.followup = kind;
    native.entries.push_back(e);
    const auto before = observe(s);
    assert(native.run_phase(k, SlotGroup::Probe, nullptr) == 0);
    const auto &after = native.entries.front();
    assert(after.tm.occasions == 1);
    std::cout << "{\"kind\":\"admission\",\"config\":"; config_json(config.inprocess);
    std::cout << ",\"observation\":"; observation_json(before);
    std::cout << ",\"slot\":{\"rule\":\"BCE\",\"enabled\":" << enabled
      << ",\"preprocess\":" << preprocess << ",\"defer_preprocess\":" << defer
      << ",\"max_applies\":" << cap << ",\"applies\":" << applies
      << ",\"followup\":\"" << followup_name(kind) << "\"},\"applies\":" << after.applies
      << ",\"mode\":\"" << mode_name(after.followup_mode) << "\",\"decline\":\""
      << (after.tm.dec_preprocess ? "preprocess" : after.tm.dec_disabled ? "disabled" :
          after.tm.dec_cap ? "cap" : "") << "\"}\n";
    tactix_release(s);
  }
}

int main() {
  emit_admission();
  emit_budgets();
  for (auto group : {SlotGroup::Probe, SlotGroup::Eliminate}) {
    for (uint64_t conflicts : {0u, 10u}) {
      emit_phase(group, conflicts, "empty");
      emit_phase(group, conflicts, "root-growth");
      emit_phase(group, conflicts, "early-unsat");
    }
  }
  emit_phase(SlotGroup::Probe, 10, "late-unsat");
  emit_phase(SlotGroup::Probe, 10, "material-change");
  emit_phase(SlotGroup::Eliminate, 10, "material-change");
  emit_phase(SlotGroup::Probe, 0, "material-change");
  emit_phase(SlotGroup::Eliminate, 0, "material-change");
  emit_phase(SlotGroup::Eliminate, 10, "maximum");
  emit_phase(SlotGroup::Eliminate, 10, "maximum-growth");
  emit_phase(SlotGroup::Eliminate, 10, "advance");
  emit_phase(SlotGroup::Eliminate, 0, "fast");
  DefaultStrategyConfig config;
  for (bool root : {false, true}) {
    auto *s = tactix_init(); assert(s);
    for (int literal : {1, 2, 3, 0}) tactix_add(s, literal);
    if (root) { tactix_add(s, 1); tactix_add(s, 0); }
    assert(tactix_status(s) == 0);
    assert(tactix_stat(s, TACTIX_STAT_ROOT_FIXED) == unsigned(root));
    for (bool split : {false, true}) for (bool watermarks : {false, true})
      for (bool preprocess : {false, true}) for (bool initialized : {false, true})
        for (bool reduced : {false, true}) for (uint64_t rounds : {0u, 1u, 2u})
          for (uint64_t conflicts : {0u, 98u, 99u, 100u}) for (unsigned changed = 0; changed < 4; ++changed) {
            config.inprocess.split_clocks = split; config.inprocess.watermarks = watermarks;
            config.inprocess.preprocess_only = preprocess;
            emit(s, config.inprocess, initialized, reduced, rounds, conflicts, changed);
          }
    // Independent round orderings and lowered marks distinguish max/zero,
    // equality and <= semantics without multiplying the full matrix.
    config.inprocess.watermarks = true; config.inprocess.preprocess_only = false;
    for (bool split : {false, true}) {
      config.inprocess.split_clocks = split;
      for (const auto &[probe, eliminate] : {std::pair{0u, 0u}, {2u, 0u}, {1u, 1u}})
        for (unsigned changed : {0u, 4u, 5u, 6u})
          emit(s, config.inprocess, true, false, probe, 100, changed, eliminate, 1);
    }
    tactix_release(s);
  }
  // Integer deadline boundary: Kissat logn(C+1) includes both +1 and +9.
  auto *s = tactix_init(); assert(s);
  Kernel &k = api_detail::kernel_for_fixed_testing(s);
  kernel_reserve_vars(k, 3);
  const Lit clause[] = {internalize(1), internalize(2), internalize(3)};
  for (unsigned i = 0; i < 22575; ++i) kernel_add_large_clause(k, clause, 3, false, 0);
  config.inprocess.split_clocks = true; config.inprocess.preprocess_only = false;
  emit(s, config.inprocess, true, false, 2, 2688, 0);
  tactix_release(s);
}
