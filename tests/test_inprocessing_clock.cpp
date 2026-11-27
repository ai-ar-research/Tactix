// Plan §5.6 conflict clock for search-time Probe and Eliminate: fixed, Luby
// and event clocks must keep every verdict and model of the per-decision
// schedule while opening root windows and replaying the saved decision prefix;
// without replay, a 1-conflict window and the event clock must match it exactly.
// Rename the public alias as api.cpp does: its `tactix` typedef would collide
// with the internal namespace.
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix

#include "unit_test.hpp"
#include "api/api_internal.hpp"
#include "policies/default_policy.hpp"
#include "policies/inprocessing_clock.hpp"

#include <cstdlib>

namespace {

using Formula = std::vector<std::vector<int>>;

Formula random_3sat(int variables, int clauses, uint64_t seed) {
  Formula formula;
  uint64_t state = seed;
  const auto next = [&] {
    state = state * 6364136223846793005ull + 1442695040888963407ull;
    return static_cast<uint32_t>(state >> 33);
  };
  for (int c = 0; c < clauses; ++c) {
    std::vector<int> clause;
    while (clause.size() < 3) {
      const int variable = 1 + static_cast<int>(next() % variables);
      if (std::any_of(clause.begin(), clause.end(),
                      [&](int lit) { return std::abs(lit) == variable; }))
        continue;
      clause.push_back(next() & 1 ? variable : -variable);
    }
    formula.push_back(clause);
  }
  return formula;
}

Formula pigeonhole(int holes) {
  Formula formula;
  const auto var = [&](int pigeon, int hole) { return pigeon * holes + hole + 1; };
  for (int p = 0; p <= holes; ++p) {
    std::vector<int> clause;
    for (int h = 0; h < holes; ++h) clause.push_back(var(p, h));
    formula.push_back(clause);
  }
  for (int h = 0; h < holes; ++h)
    for (int p = 0; p <= holes; ++p)
      for (int q = p + 1; q <= holes; ++q) formula.push_back({-var(p, h), -var(q, h)});
  return formula;
}

struct Outcome {
  int verdict = 0;
  uint64_t conflicts = 0, phases = 0;
};

const char *cadence_name(uint32_t clock) {
  return clock == DefaultStrategyConfig::INPROCESSING_CLOCK_FIXED   ? "fixed"
         : clock == DefaultStrategyConfig::INPROCESSING_CLOCK_LUBY  ? "luby"
         : clock == DefaultStrategyConfig::INPROCESSING_CLOCK_EVENT ? "event"
         : clock == DefaultStrategyConfig::INPROCESSING_CLOCK_DECISIONS ? "decisions"
                                                                    : "decision";
}

// The public solve path reads the clock settings, reconstructs the model and
// reports it through tactix_value.
Outcome solve(const Formula &formula, uint32_t clock, uint64_t interval, bool replay = true,
              uint64_t decision_points = 0) {
  setenv("TACTIX_INPROCESSING_CLOCK", cadence_name(clock), 1);
  setenv("TACTIX_INPROCESSING_CLOCK_INTERVAL", std::to_string(interval).c_str(), 1);
  setenv("TACTIX_INPROCESSING_CLOCK_REPLAY", replay ? "1" : "0", 1);
  setenv("TACTIX_INPROCESSING_DECISION_POINTS", std::to_string(decision_points).c_str(), 1);
  tactix_c_handle *s = tactix_init();
  for (const auto &clause : formula) {
    for (int lit : clause) tactix_add(s, lit);
    tactix_add(s, 0);
  }
  Outcome out;
  out.verdict = tactix_solve(s);
  const Kernel &k = api_detail::kernel_for_fixed_testing(s);
  out.conflicts = k.stats.conflicts;
  out.phases = k.stats.inprocess_phases;
  if (out.verdict == 10)
    for (const auto &clause : formula)
      assert(std::any_of(clause.begin(), clause.end(),
                         [&](int lit) { return (tactix_value(s, std::abs(lit)) > 0) == (lit > 0); }));
  tactix_release(s);
  unsetenv("TACTIX_INPROCESSING_CLOCK");
  unsetenv("TACTIX_INPROCESSING_CLOCK_INTERVAL");
  unsetenv("TACTIX_INPROCESSING_CLOCK_REPLAY");
  unsetenv("TACTIX_INPROCESSING_DECISION_POINTS");
  return out;
}

uint64_t search_window_runs = 0;

void same_verdicts(const Formula &formula) {
  const Outcome stock = solve(formula, DefaultStrategyConfig::INPROCESSING_CLOCK_PER_DECISION, 100);
  assert(stock.verdict == 10 || stock.verdict == 20);
  for (const uint32_t clock : {DefaultStrategyConfig::INPROCESSING_CLOCK_FIXED,
                               DefaultStrategyConfig::INPROCESSING_CLOCK_LUBY,
                               DefaultStrategyConfig::INPROCESSING_CLOCK_EVENT,
                               DefaultStrategyConfig::INPROCESSING_CLOCK_DECISIONS})
    for (const uint64_t interval : {uint64_t{1}, uint64_t{2}, uint64_t{100}}) {
      const Outcome out = solve(formula, clock, interval);
      assert(out.verdict == stock.verdict);
      // Startup runs at most one Probe and one Eliminate phase; any further
      // phase was a search-time pass in a clock window.
      if (out.phases > 2)
        ++search_window_runs;
    }
  // Without replay, a 1-conflict window and the event clock open a check at
  // every point where a due test could change its answer: the search is the
  // per-decision search, conflict for conflict.
  for (const uint32_t clock : {DefaultStrategyConfig::INPROCESSING_CLOCK_FIXED,
                               DefaultStrategyConfig::INPROCESSING_CLOCK_EVENT}) {
    const Outcome out = solve(formula, clock, 1, false);
    assert(out.verdict == stock.verdict && out.conflicts == stock.conflicts &&
           out.phases == stock.phases);
  }
  // Decision points that find no due pass leave the search unchanged.
  for (const uint64_t points : {uint64_t{1}, uint64_t{7}}) {
    const Outcome out = solve(formula, DefaultStrategyConfig::INPROCESSING_CLOCK_EVENT, 1, false, points);
    assert(out.verdict == stock.verdict && out.conflicts == stock.conflicts &&
           out.phases == stock.phases);
  }
}

} // namespace

TEST(test_clock_settings_validate) {
  DefaultStrategyConfig config;
  assert(config.inprocess.inprocessing_clock ==
         DefaultStrategyConfig::INPROCESSING_CLOCK_EVENT);
  assert(!config.inprocess.inprocessing_clock_replay);
  assert(config.inprocess.inprocessing_clock_interval ==
         DefaultStrategyConfig::INPROCESS_PROBE_INTERVAL_DEFAULT);
  config.inprocess.inprocessing_clock = DefaultStrategyConfig::INPROCESSING_CLOCK_DECISIONS + 1;
  assert(!config.valid(nullptr));
  config.inprocess.inprocessing_clock = DefaultStrategyConfig::INPROCESSING_CLOCK_FIXED;
  config.inprocess.inprocessing_clock_interval = 0;
  assert(!config.valid(nullptr));
}

TEST(test_clock_keeps_random_3sat_verdicts) {
  search_window_runs = 0;
  for (uint64_t seed = 1; seed <= 12; ++seed)
    same_verdicts(random_3sat(150, 640, seed));
  assert(search_window_runs > 0);
}

TEST(test_clock_keeps_pigeonhole_refutation) {
  same_verdicts(pigeonhole(7));
}

namespace {
std::vector<uint64_t> ticks(uint32_t cadence, uint64_t base, uint64_t until) {
  InprocessingClock clock;
  clock.start(cadence, base, 0);
  std::vector<uint64_t> out;
  for (uint64_t c = 0; c <= until; ++c) {
    clock.observe(c);
    if (clock.pending()) {
      out.push_back(c);
      clock.checked();
    }
  }
  return out;
}
} // namespace

TEST(test_clock_tick_positions) {
  assert((ticks(DefaultStrategyConfig::INPROCESSING_CLOCK_FIXED, 100, 500) ==
          std::vector<uint64_t>{100, 200, 300, 400, 500}));
  // Luby windows 1,1,2,1,1,2,4 x 100 from the origin.
  assert((ticks(DefaultStrategyConfig::INPROCESSING_CLOCK_LUBY, 100, 1200) ==
          std::vector<uint64_t>{100, 200, 400, 500, 600, 800, 1200}));
  assert(ticks(DefaultStrategyConfig::INPROCESSING_CLOCK_PER_DECISION, 100, 1000).empty());
}

TEST(test_clock_restart_mark_keeps_grid) {
  InprocessingClock clock;
  clock.start(DefaultStrategyConfig::INPROCESSING_CLOCK_LUBY, 100, 0);
  clock.observe(150); // first tick at 100
  assert(clock.pending() && clock.next() == 200);
  clock.checked();
  clock.mark_pending(); // a natural restart between ticks
  assert(clock.pending() && clock.next() == 200);
  clock.checked();
  clock.observe(5000); // one conflict crossing many ticks opens one check
  assert(clock.pending() && clock.next() > 5000);
  InprocessingClock off;
  off.start(DefaultStrategyConfig::INPROCESSING_CLOCK_PER_DECISION, 100, 0);
  off.mark_pending();
  assert(!off.pending());
}
