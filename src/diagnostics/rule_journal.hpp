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
  const char *decline = "-";
  std::string deltas = "-";
  std::string witnesses = "-";
  uint64_t effort = 0;
  uint64_t budget = 0;
  uint64_t post_digest = 0;
};

uint64_t rule_journal_digest(const Kernel &);
bool rule_journal_wants(const Kernel &, const char *rule,
                        const std::string &candidate);
void rule_journal_emit(const Kernel &, const RuleJournalRecord &);
void rule_journal_close();

#else
inline void rule_journal_close() {}
#endif

} // namespace tactix
