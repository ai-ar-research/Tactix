// Compile-time diagnostic journal for semantic control choices and proof-rule
// transactions.  Production builds neither compile the implementation nor
// name its environment interface.
#pragma once

#include <cstdint>
#include <string>

namespace tactix {

struct Kernel;

#ifdef TACTIX_ENABLE_RULE_JOURNAL

struct RuleJournalRecord {
  const char *kind = "transaction";
  const char *rule = "-";
  const char *event = "-";
  std::string candidate = "-";
  std::string trigger = "-";
  std::string context = "-";
  uint64_t formula_epoch = 0;
  uint64_t root_epoch = 0;
  uint64_t pre_digest = 0;
  std::string guards = "-";
  std::string antecedents = "-";
  const char *outcome = "-";
  std::string decline = "-";
  std::string deltas = "-";
  std::string witnesses = "-";
  uint64_t effort = 0;
  uint64_t budget = 0;
  uint64_t post_digest = 0;
};

uint64_t rule_journal_digest(const Kernel &);
bool rule_journal_wants(const Kernel &, const char *rule,
                        const std::string &candidate);
using RuleJournalMeter = uint64_t (*)(const Kernel &);
// The scheduler installs the rule's source-defined logical currency around
// registry dispatch.  Generic proof-rule records therefore charge the local
// meter instead of the unrelated global propagation counter.
void rule_journal_set_effort_meter(RuleJournalMeter);
uint64_t rule_journal_effort(const Kernel &);
int rule_journal_selected_literal();
uint64_t rule_journal_selected_clause_id();
void rule_journal_emit(const Kernel &, const RuleJournalRecord &);
// The registry brackets the real proof-rule call with this mutation ledger.
// Kernel primitives append facts at the point where the mutation actually
// occurs; the journal therefore does not reconstruct or duplicate rule logic.
void rule_journal_transaction_begin(const char *rule,
                                    const std::string &candidate);
void rule_journal_note_binary_add(const Kernel &, uint32_t a, uint32_t b,
                                  uint64_t id);
void rule_journal_note_binary_rewrite(const Kernel &, uint32_t source_a,
                                      uint32_t source_b, uint32_t target_a,
                                      uint32_t target_b);
void rule_journal_note_large_add(const Kernel &, const uint32_t *lits,
                                 uint32_t size, uint64_t id, bool redundant,
                                 uint32_t glue);
void rule_journal_note_clause_transition(const Kernel &, const char *event,
                                         const uint32_t *lits, uint32_t size,
                                         uint64_t id);
void rule_journal_note_root_assignment(const Kernel &, uint32_t lit,
                                       uint32_t reason, bool binary);
void rule_journal_note_variable_status(const Kernel &, uint32_t var,
                                       const char *status);
void rule_journal_note_reconstruction(const Kernel &, uint32_t witness,
                                      const uint32_t *rest, uint32_t nrest);
void rule_journal_note_rup(uint64_t id, const uint64_t *chain,
                           uint32_t nchain);
void rule_journal_note_rat(uint64_t id, const int64_t *hints,
                           uint32_t nhints);
std::string rule_journal_transaction_deltas();
std::string rule_journal_transaction_antecedents();
std::string rule_journal_transaction_witnesses();
void rule_journal_transaction_end();
void rule_journal_close();

#else
inline void rule_journal_close() {}
#endif

} // namespace tactix
