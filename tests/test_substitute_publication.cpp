// Regression: root closure in a later round/reconnection must not move before
// the preceding round's retirement events. Exercise the actual policy and
// rule primitives, not a model of the heap algorithm.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "rules/registry.hpp"
#include "rules/substitute/substitute.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#ifdef TACTIX_ENABLE_RULE_JOURNAL
#include "diagnostics/rule_journal.hpp"
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>
#endif
using namespace tactix;

static void fixture(Kernel &k) {
  kernel_reserve_vars(k, 16);
  for (Var v = 0; v < k.nvars; ++v)
    kernel_record_import(k, v);
  const auto large = [&](int a, int b, int c) {
    const Lit lits[] = {internalize(a), internalize(b), internalize(c)};
    kernel_add_large_clause(k, lits, 3, false, 0);
  };
  const auto binary = [&](int a, int b) {
    kernel_add_binary_clause(k, internalize(a), internalize(b));
  };
  // Arena order matters: x1 becomes a root only after the clauses producing
  // roots x3,x8 have been rewritten and x4,x5,x9,x10 have retired.
  large(-3, -8, 1);
  large(3, 4, 5);
  large(8, 9, 10);
  binary(-3, 4); binary(3, -4); binary(-3, 5); binary(3, -5);
  binary(-8, 9); binary(8, -9); binary(-8, 10); binary(8, -10);
  k.stats.search_ticks = 1000000000;
}

static void test_order(bool budgeted, bool preprocessing, uint32_t rounds) {
  Kernel expected, actual;
  fixture(expected); fixture(actual);
  expected.stats.conflicts = actual.stats.conflicts = preprocessing ? 0 : 1;
  ScopedHeuristics eh(expected), ah(actual);
  for (Kernel *k : {&expected, &actual}) {
    std::fill(k->h->scores.score.begin(), k->h->scores.score.end(), 0);
    heuristics_update_scores(*k);
  }
  // Budget-aware calls and preprocessing consume the configured complete
  // round count. An unbudgeted search call uses its separate default of one.
  const uint32_t effective = (budgeted || preprocessing) ? rounds : 1;
  OccIndex occ;
  kernel_dense_connect(expected, occ);
  const uint32_t entry = static_cast<uint32_t>(expected.trail.size());
  for (uint32_t round = 0; round < effective; ++round) {
    const size_t begin = expected.trail.size();
    const auto result = substitute_apply_round(expected, false);
    assert(result != ApplyResult::Unsat);
    heuristics_consume_root_assignments(expected, begin);
    heuristics_consume_retirements(expected);
    if (round == 0) {
      assert(expected.values[internalize(1)] == 0);
      assert(expected.stats.substitute_vars == 4);
      assert(!heuristics_var_active(expected, 3));
    }
    if (result == ApplyResult::NoOp) break;
  }
  const size_t before_reconnect = expected.trail.size();
  assert(substitute_reconnect_large_watches(expected, entry) != ApplyResult::Unsat);
  heuristics_consume_root_assignments(expected, before_reconnect);
  kernel_dense_disconnect(expected);
  assert(expected.values[internalize(1)] > 0);
  assert(expected.h->scores.stack == std::vector<Var>({10, 1, 14, 13, 12, 5, 6, 15, 11}));

  DefaultStrategyConfig cfg;
  cfg.inprocess.structured_root_stages.clear(); // compare only Substitute
  cfg.inprocess.phase_rounds = 1;
  cfg.inprocess.substitute_rounds = rounds;
  DefaultPolicy policy(cfg);
  policy.inprocess().build(actual);
  bool enabled = false;
  for (auto &e : policy.inprocess().entries) {
    const bool first = !enabled && !std::strcmp(tactix_rules()[e.rule].name, "Substitute");
    e.enabled = first;
    if (first) {
      e.budget_aware = budgeted;
      e.preprocess = true;
      e.followup = decltype(e.followup)::None;
    }
    enabled |= first;
  }
  assert(enabled);
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  // Observe only the actual policy, not the independent rule-only oracle.
  // Close resets the process-wide journal between this fixture's eight cases.
  rule_journal_close();
  char journal_path[] = "substitute-publication-journal-XXXXXX";
  const int journal_fd = mkstemp(journal_path);
  assert(journal_fd >= 0);
  close(journal_fd);
  assert(setenv("TACTIX_RULE_JOURNAL", journal_path, 1) == 0);
  const uint64_t ticks_before = actual.stats.substitute_ticks;
#endif
  assert(policy.inprocess().run_phase(actual, SlotGroup::Probe, nullptr) == 0);
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  rule_journal_close();
  unsetenv("TACTIX_RULE_JOURNAL");
  std::ifstream journal(journal_path);
  std::string line;
  size_t transactions = 0, invocations = 0;
  uint64_t transaction_effort = 0;
  bool reconstruction = false, retirement = false, root_unit = false;
  while (std::getline(journal, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::vector<std::string> fields;
    std::istringstream stream(line);
    std::string field;
    while (std::getline(stream, field, '\t')) fields.push_back(field);
    assert(fields.size() >= 20);
    if (fields[3] != "Substitute") continue;
    if (fields[2] == "schedule") {
      assert(fields[4] == "inprocess-invocation");
      invocations++;
    } else if (fields[2] == "transaction") {
      assert(fields[4] == "rule-attempt");
      assert(fields[6] == std::string("policy-complete-rounds=") +
                              std::to_string(effective));
      transactions++;
      transaction_effort += std::stoull(fields[18]);
      reconstruction |= line.find("reconstruct(") != std::string::npos;
      retirement |= line.find("retire-var(") != std::string::npos;
      // The final reconnect derives x1 after retirement; keep it inside the
      // composed transaction as well as the direct rewrite's x3/x8 roots.
      root_unit |= line.find("root-unit(lit=1;") != std::string::npos;
    }
  }
  journal.close();
  assert(std::remove(journal_path) == 0);
  assert(invocations == 1);
  if (!budgeted) {
    assert(transactions == 1);
    assert(transaction_effort == actual.stats.substitute_ticks - ticks_before);
    assert(transaction_effort > 0 && reconstruction && retirement && root_unit);
  } else {
    // The budget-aware path's preexisting schedule-only journal is unchanged.
    assert(transactions == 0);
  }
#endif
  assert(actual.stats.substitute_vars == expected.stats.substitute_vars);
  assert(actual.stats.substitute_rounds == expected.stats.substitute_rounds);
  assert(actual.values == expected.values);
  assert(actual.h->scores.stack == expected.h->scores.stack);
  assert(actual.h->scores.pos == expected.h->scores.pos);
  assert(actual.h->scores.score == expected.h->scores.score);
  assert(actual.retired_events.empty());
  for (Var v = 0; v < actual.nvars; ++v)
    assert(heuristics_var_active(actual, v) == heuristics_var_active(expected, v));
  std::printf("PASS: budgeted=%d preprocessing=%d cap=%u actual_rounds=%llu\n",
              budgeted, preprocessing, rounds,
              static_cast<unsigned long long>(actual.stats.substitute_rounds));
}

int main() {
  for (bool budgeted : {true, false})
    for (bool preprocessing : {false, true})
      for (uint32_t rounds : {1u, 2u})
        test_order(budgeted, preprocessing, rounds);
}
