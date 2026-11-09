// Native phase decisions paired with observations available through tactix.h.
#undef NDEBUG
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix
#include "api/api_internal.hpp"
#include "policies/default_inprocess.hpp"
#include <cassert>
#include <iomanip>
#include <iostream>

using namespace tactix;
static void clock_json(const DefaultInprocessScheduler::SlotClock &c) {
  std::cout << "{\"next_conflicts\":" << c.next_conflicts << ",\"rounds\":" << c.rounds
    << ",\"reductions_mark\":" << c.reductions_mark << ",\"marks_mark\":" << c.marks_mark
    << ",\"fixed_mark\":" << c.fixed_mark << ",\"material_mark\":" << c.material_mark
    << ",\"blocked\":" << c.blocked << ",\"blocked_episodes\":" << c.blocked_episodes << '}';
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
  std::cout << std::setprecision(17) << "{\"config\":{\"split_clocks\":" << cfg.split_clocks
    << ",\"watermarks\":" << cfg.watermarks << ",\"preprocess_only\":" << cfg.preprocess_only
    << ",\"probe_interval\":" << cfg.probe_interval << ",\"eliminate_interval\":" << cfg.eliminate_interval
    << ",\"logarithm_offset\":" << cfg.logarithm_offset << ",\"scale_log_shift\":" << cfg.scale_log_shift
    << ",\"scale_quadratic\":" << cfg.scale_quadratic << ",\"scale_constant\":" << cfg.scale_constant
    << "},\"initialized\":" << initialized << ",\"reduced\":" << reduced
    << ",\"clocks\":{\"Probe\":";
  clock_json(native.probe_clock); std::cout << ",\"Eliminate\":"; clock_json(native.elim_clock);
  std::cout << "},\"observation\":{";
  bool comma = false;
  for (const auto &[name, id] : {std::pair{"CONFLICTS", TACTIX_STAT_CONFLICTS},
       {"REDUCTIONS", TACTIX_STAT_REDUCTIONS}, {"BVE_MARKS", TACTIX_STAT_BVE_MARKS},
       {"ROOT_FIXED", TACTIX_STAT_ROOT_FIXED}, {"SUBSUME_MATERIAL", TACTIX_STAT_SUBSUME_MATERIAL},
       {"IRREDUNDANT_CLAUSES", TACTIX_STAT_IRREDUNDANT_CLAUSES}}) {
    if (comma) std::cout << ',';
    std::cout << '"' << name << "\":" << tactix_stat(s, id); comma = true;
  }
  std::cout << "},\"checks\":["; comma = false;
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
int main() {
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
