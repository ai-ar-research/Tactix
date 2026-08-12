#undef NDEBUG
#include "kernel/kernel.hpp"
#include "diagnostics/rule_journal.hpp"
#include "rules/registry.hpp"

#include <cassert>
#include <cstdlib>
#include <string>

using namespace tactix;

int main() {
  Kernel k;
  kernel_reserve_vars(k, 2);
#ifndef TACTIX_TEST_STANDALONE_JOURNAL
  // Every registered proof rule must expose the evaluator-produced guard facts
  // consumed by the journal wrapper.  A new rule cannot silently fall back to
  // an observer-reconstructed eligibility predicate.
  uint32_t proof_rules = 0;
  for (const RuleVTable &rule : tactix_rules()) {
    if (rule.kind != ActionKind::ProofRule)
      continue;
    proof_rules++;
    const RuleEligibility eligibility = rule.guard(k);
    assert(eligibility.fact_count > 0);
  }
  assert(proof_rules == 22);
#endif
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
    const uint64_t digest = rule_journal_digest(k);
    const bool applied = !std::string(test.outcome).compare("applied");
    if (applied) {
      rule_journal_transaction_begin("JournalTest", test.candidate);
      kernel_add_binary_clause(k, internalize(1), internalize(-2), 17);
      kernel_assign_unit(k, internalize(1));
      const Lit rest[] = {internalize(-2)};
      kernel_push_witness(k, internalize(1), rest, 1);
      rule_journal_note_variable_status(k, var_of(internalize(2)),
                                       "substituted");
      const uint64_t chain[] = {3, 5, 8};
      rule_journal_note_rup(17, chain, 3);
#ifdef TACTIX_TEST_STANDALONE_JOURNAL
      // In a production configuration tactix_lib deliberately has no journal
      // hooks.  This executable links the protocol implementation privately,
      // so exercise that mutation callback explicitly.  A journal-enabled
      // build reaches the same callback from kernel_add_binary_clause itself.
      rule_journal_note_binary_add(k, internalize(1), internalize(-2), 17);
      rule_journal_note_reconstruction(k, internalize(1), rest, 1);
#endif
    }
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
    record.antecedents = applied ? rule_journal_transaction_antecedents()
                                 : "fixture-clause=1";
    record.outcome = test.outcome;
    record.decline = test.decline;
    record.deltas = applied ? rule_journal_transaction_deltas() : "none";
    record.witnesses =
        applied ? rule_journal_transaction_witnesses() : "none";
    record.effort = 7;
    record.budget = !std::string(test.candidate).compare("budget") ? 0 : 11;
    record.post_digest = rule_journal_digest(k);
    rule_journal_emit(k, record);
    if (applied)
      rule_journal_transaction_end();
  }
  rule_journal_close();
  return 0;
}
