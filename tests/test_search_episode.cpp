// DefaultPolicy's search is its search episodes run back to back: after its
// startup, episodes that keep their restarts' trails take the trajectory of
// DefaultPolicy::solve with search-time inprocessing off.
#include "unit_test.hpp"
#include "frontend/parse.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "strategy/policy_feedback.hpp"

#include <csignal>

namespace {
// A seeded random 3-SAT formula near the threshold.
void random_3sat(Kernel &k, uint64_t seed) {
  constexpr unsigned variables = 80, clauses = 344;
  FILE *in = std::tmpfile();
  assert(in);
  std::fprintf(in, "p cnf %u %u\n", variables, clauses);
  for (unsigned c = 0; c < clauses; ++c) {
    for (int i = 0; i < 3; ++i) {
      seed = seed * 6364136223846793005u + 1442695040888963407u;
      const int v = 1 + static_cast<int>((seed >> 33) % variables);
      std::fprintf(in, "%d ", (seed >> 13) & 1 ? v : -v);
    }
    std::fputs("0\n", in);
  }
  std::rewind(in);
  assert(!parse_dimacs(in, k));
  std::fclose(in);
}

// Test instrumentation: interrupts a solve once its startup has consumed the
// root prefix, so the solve returns at its search's first boundary.
struct StopAtSearch final : strategy::controller::FeedbackReceiver {
  volatile sig_atomic_t stop = 0;
  void after_action(const strategy::controller::ActionFeedback &action) override {
    if (action.identity == "ConsumeRootPrefix") stop = 1;
  }
  void search_interval(const strategy::controller::SearchIntervalFeedback &) override {}
};

DefaultStrategyConfig without_search_inprocessing() {
  DefaultStrategyConfig config;
  config.inprocess.preprocess_only = true;
  return config;
}
} // namespace

TEST(native_episode_matches_solve) {
  unsigned compared = 0;
  for (uint64_t seed = 1; seed <= 16; ++seed) {
    Kernel solved, searched;
    random_3sat(solved, seed);
    random_3sat(searched, seed);
    ScopedHeuristics solved_h(solved), searched_h(searched);
    DefaultPolicy solver(without_search_inprocessing());
    const int expected = solver.solve(solved);
    assert(expected == 10 || expected == 20);

    DefaultPolicy startup(without_search_inprocessing());
    StopAtSearch at_search;
    startup.set_feedback(&at_search);
    const int stopped = startup.solve(searched, &at_search.stop);
    if (!at_search.stop) { // startup decided: nothing to compare but the verdict
      assert(stopped == expected && searched.stats.conflicts == solved.stats.conflicts);
      continue;
    }
    assert(stopped == 0 && !searched.level && !searched.stats.decisions);
    DefaultPolicy episodes(without_search_inprocessing());
    int result = 0;
    while (!result) result = episodes.search_episode(searched, 0, /*restart_to_root=*/false);
    assert(result == expected);
    assert(searched.stats.conflicts == solved.stats.conflicts);
    assert(searched.stats.decisions == solved.stats.decisions);
    assert(searched.stats.propagations == solved.stats.propagations);
    assert(searched.stats.restarts == solved.stats.restarts);
    assert(searched.stats.reductions == solved.stats.reductions);
    ++compared;
  }
  std::printf("%u searches compared\n", compared);
  assert(compared >= 4);
}
