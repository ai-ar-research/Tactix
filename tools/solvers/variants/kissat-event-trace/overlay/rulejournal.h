#ifndef _rulejournal_h_INCLUDED
#define _rulejournal_h_INCLUDED

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

struct kissat;

uint64_t kissat_rule_journal_digest (struct kissat *);
void kissat_rule_journal_literals (struct kissat *, const unsigned *, size_t,
                                   char *, size_t);
void kissat_rule_journal_emit (
    struct kissat *, const char *kind, const char *rule, const char *event,
    const char *candidate, const char *trigger, const char *context,
    uint64_t formula_epoch, uint64_t root_epoch, uint64_t pre_digest,
    const char *guards, const char *antecedents, const char *outcome,
    const char *decline, const char *deltas, const char *witnesses,
    uint64_t effort, uint64_t budget, uint64_t post_digest);
void kissat_rule_journal_sweep_core (
    struct kissat *, unsigned core, unsigned ordinal, const char *literals,
    size_t old_size, bool satisfied, size_t new_size, unsigned unit,
    const char *outcome, const char *decline, uint64_t pre_digest);
void kissat_rule_journal_rule_attempt (struct kissat *, const char *rule,
                                       const char *candidate,
                                       const char *trigger,
                                       uint64_t pre_digest,
                                       uint64_t effort_before,
                                       uint64_t effort_after,
                                       uint64_t budget);
void kissat_rule_journal_close (void);

#endif
