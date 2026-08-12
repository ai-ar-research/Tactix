#undef NDEBUG
#include "kernel/kernel.hpp"
#include "diagnostics/rule_journal.hpp"

#include <cassert>
#include <cstdlib>
#include <string>

using namespace tactix;

int main() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  const uint64_t digest = rule_journal_digest(k);
  struct Case {
    const char *candidate;
    const char *outcome;
    const char *decline;
    const char *guards;
  } cases[] = {
      {"applied", "applied", "none", "eligible=1"},
      {"declined", "declined", "precondition-false", "eligible=0"},
      {"no-op", "no-op", "none", "eligible=1;changed=0"},
      {"budget", "declined", "budget-exhausted", "eligible=1;budget=0"},
      {"refuted", "refuted", "none", "eligible=1;refutation=1"},
  };
  for (const Case &test : cases) {
    assert(rule_journal_wants(k, "JournalTest", test.candidate));
    RuleJournalRecord record;
    record.rule = "JournalTest";
    record.event = "outcome-fixture";
    record.candidate = test.candidate;
    record.trigger = "unit-test";
    record.context = "fixture=1";
    record.formula_epoch = k.arena_generation;
    record.root_epoch = k.touch_fixed_head;
    record.pre_digest = digest;
    record.guards = test.guards;
    record.antecedents = "fixture-clause=1";
    record.outcome = test.outcome;
    record.decline = test.decline;
    record.deltas = !std::string(test.outcome).compare("applied")
                        ? "unit=1"
                        : "none";
    record.witnesses = "fixture";
    record.effort = 7;
    record.budget = !std::string(test.candidate).compare("budget") ? 0 : 11;
    record.post_digest = digest;
    rule_journal_emit(k, record);
  }
  rule_journal_close();
  return 0;
}
