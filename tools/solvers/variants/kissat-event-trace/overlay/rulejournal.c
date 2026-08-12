#define _GNU_SOURCE
#include "rulejournal.h"

#include "inline.h"
#include "internal.h"

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *output, *replay;
static bool initialized;
static uint64_t sequence, emitted, limit = UINT64_MAX;
static uint64_t conflict_from, conflict_to = UINT64_MAX;
static const char *selected_rule, *selected_candidate;

static uint64_t mix (uint64_t hash, uint64_t value) {
  return (hash ^ value) * UINT64_C (1099511628211);
}

static unsigned external_variable (kissat *solver, unsigned idx) {
  if (idx < SIZE_STACK (solver->export)) {
    const int external = PEEK_STACK (solver->export, idx);
    if (external)
      return (unsigned) (external < 0 ? -external : external);
  }
  return idx + 1;
}

static int external_literal (kissat *solver, unsigned lit) {
  const int external = (int) external_variable (solver, IDX (lit));
  return NEGATED (lit) ? -external : external;
}

void kissat_rule_journal_literals (kissat *solver, const unsigned *lits,
                                   size_t size, char *buffer, size_t capacity) {
  if (!capacity)
    return;
  char *p = buffer;
  size_t left = capacity;
  *p = 0;
  for (size_t i = 0; i != size; i++) {
    const int written = snprintf (p, left, "%s%d", i ? "," : "",
                                  external_literal (solver, lits[i]));
    if (written < 0 || (size_t) written >= left) {
      buffer[capacity - 1] = 0;
      return;
    }
    p += written;
    left -= (size_t) written;
  }
}

uint64_t kissat_rule_journal_digest (kissat *solver) {
  uint64_t hash = UINT64_C (1469598103934665603);
  hash = mix (hash, solver->level);
  hash = mix (hash, SIZE_ARRAY (solver->trail));
  hash = mix (hash, solver->statistics.clauses_irredundant);
  hash = mix (hash, solver->statistics.clauses_redundant);
  hash = mix (hash, solver->statistics.clauses_binary);
  for (all_variables (idx)) {
    hash = mix (hash, external_variable (solver, idx));
    hash = mix (hash, ACTIVE (idx));
    hash = mix (hash, (unsigned) (solver->values[LIT (idx)] + 1));
  }
  return hash;
}

static void open_journal (void) {
  if (initialized)
    return;
  initialized = true;
  const char *value;
  if ((value = getenv ("KISSAT_RULE_JOURNAL_LIMIT")) && *value)
    limit = strtoull (value, 0, 10);
  if ((value = getenv ("KISSAT_RULE_JOURNAL_CONFLICT_FROM")) && *value)
    conflict_from = strtoull (value, 0, 10);
  if ((value = getenv ("KISSAT_RULE_JOURNAL_CONFLICT_TO")) && *value)
    conflict_to = strtoull (value, 0, 10);
  selected_rule = getenv ("KISSAT_RULE_JOURNAL_RULE");
  selected_candidate = getenv ("KISSAT_RULE_JOURNAL_CANDIDATE");
  if ((value = getenv ("KISSAT_RULE_JOURNAL")) && *value) {
    output = fopen (value, "w");
    if (!output) {
      fprintf (stderr, "kissat: can not open rule journal '%s': %s\n", value,
               strerror (errno));
      abort ();
    }
    fputs ("# tactix-rule-journal\t1\n", output);
    fputs ("# solver\tseq\tkind\trule\tevent\tconflicts\tcandidate\t"
           "trigger\tcontext\tformula_epoch\troot_epoch\tpre_digest\t"
           "guards\tantecedents\toutcome\tdecline\tdeltas\twitnesses\t"
           "effort\tbudget\tpost_digest\n",
           output);
  }
  if ((value = getenv ("KISSAT_RULE_JOURNAL_REPLAY")) && *value) {
    replay = fopen (value, "r");
    if (!replay) {
      fprintf (stderr, "kissat: can not open replay journal '%s': %s\n",
               value, strerror (errno));
      abort ();
    }
  }
}

static char *next_replay_line (void) {
  char *line = 0;
  size_t capacity = 0;
  while (getline (&line, &capacity, replay) >= 0) {
    const size_t size = strlen (line);
    if (size && line[size - 1] == '\n')
      line[size - 1] = 0;
    if (*line != '#')
      return line;
  }
  free (line);
  return 0;
}

static void verify (const char *actual) {
  char *expected = next_replay_line ();
  if (!expected) {
    fprintf (stderr,
             "rule-journal replay mismatch at record %" PRIu64
             ": unexpected actual record\n  actual: %s\n",
             emitted, actual);
    abort ();
  }
  if (!strcmp (expected, actual)) {
    free (expected);
    return;
  }
  static const char *names[] = {
      "solver",      "seq",         "kind",       "rule",
      "event",       "conflicts",   "candidate",  "trigger",
      "context",     "formula_epoch", "root_epoch", "pre_digest",
      "guards",      "antecedents", "outcome",    "decline",
      "deltas",      "witnesses",   "effort",     "budget",
      "post_digest",
  };
  char *want = expected, *got = strdup (actual);
  char *wp = want, *gp = got;
  size_t field = 0;
  while (field != sizeof names / sizeof *names) {
    char *we = strchr (wp, '\t'), *ge = strchr (gp, '\t');
    if (we)
      *we = 0;
    if (ge)
      *ge = 0;
    if (strcmp (wp, gp))
      break;
    if (!we || !ge) {
      field++;
      break;
    }
    wp = we + 1;
    gp = ge + 1;
    field++;
  }
  fprintf (stderr,
           "rule-journal replay mismatch at record %" PRIu64
           " field '%s'\n  expected: %s\n  actual:   %s\n",
           emitted, field < sizeof names / sizeof *names ? names[field]
                                                             : "field-count",
           wp, gp);
  free (got);
  free (expected);
  abort ();
}

void kissat_rule_journal_emit (
    kissat *solver, const char *kind, const char *rule, const char *event,
    const char *candidate, const char *trigger, const char *context,
    uint64_t formula_epoch, uint64_t root_epoch, uint64_t pre_digest,
    const char *guards, const char *antecedents, const char *outcome,
    const char *decline, const char *deltas, const char *witnesses,
    uint64_t effort, uint64_t budget, uint64_t post_digest) {
  open_journal ();
  const uint64_t current = sequence++;
  if (!output && !replay)
    return;
  const uint64_t conflicts = solver->statistics.conflicts;
  if (conflicts < conflict_from || conflicts > conflict_to || emitted >= limit)
    return;
  if (selected_rule && *selected_rule && strcmp (selected_rule, rule))
    return;
  if (selected_candidate && *selected_candidate &&
      strcmp (selected_candidate, candidate))
    return;
  char *line = 0;
  const int length = asprintf (
      &line,
      "kissat\t%" PRIu64 "\t%s\t%s\t%s\t%" PRIu64
      "\t%s\t%s\t%s\t%" PRIu64 "\t%" PRIu64 "\t%016" PRIx64
      "\t%s\t%s\t%s\t%s\t%s\t%s\t%" PRIu64 "\t%" PRIu64
      "\t%016" PRIx64,
      current, kind, rule, event, conflicts, candidate, trigger, context,
      formula_epoch, root_epoch, pre_digest, guards, antecedents, outcome,
      decline, deltas, witnesses, effort, budget, post_digest);
  if (length < 0 || !line)
    abort ();
  emitted++;
  if (output) {
    fprintf (output, "%s\n", line);
    fflush (output);
  }
  if (replay)
    verify (line);
  free (line);
}

void kissat_rule_journal_sweep_core (
    kissat *solver, unsigned core, unsigned ordinal, const char *literal_text,
    size_t old_size, bool satisfied, size_t new_size, unsigned unit,
    const char *outcome, const char *decline, uint64_t pre_digest) {
  char candidate[2200], context[96], guards[160];
  char antecedents[96], deltas[96];
  snprintf (candidate, sizeof candidate, "core=%u;lemma=%u;lits=%s", core,
            ordinal, literal_text);
  snprintf (context, sizeof context, "path=add-core;proof=%u;level=%u",
            solver->proof != 0, solver->level);
  snprintf (guards, sizeof guards,
            "satisfied=%u;old_size=%zu;new_size=%zu;unit=%u", satisfied,
            old_size, new_size, unit != INVALID_LIT);
  snprintf (antecedents, sizeof antecedents,
            "kitten-core=%u;lemma=%u", core, ordinal);
  if (!strcmp (outcome, "no-op"))
    strcpy (deltas, "none");
  else if (unit != INVALID_LIT)
    snprintf (deltas, sizeof deltas, "unit=%d",
              external_literal (solver, unit));
  else if (!new_size)
    strcpy (deltas, "empty-clause");
  else if (!strcmp (outcome, "applied"))
    snprintf (deltas, sizeof deltas, "proof-clause-size=%zu", new_size);
  else
    strcpy (deltas, "none");
  kissat_rule_journal_emit (
      solver, "transaction", "Sweep", "core-consequence", candidate,
      "embedded-refutation", context, solver->statistics.clauses_added,
      SIZE_ARRAY (solver->trail), pre_digest, guards, antecedents, outcome,
      decline, deltas, "kitten-clausal-core",
      solver->statistics.kitten_ticks, 0,
      kissat_rule_journal_digest (solver));
}

void kissat_rule_journal_close (void) {
  if (!initialized)
    return;
  if (replay) {
    char *extra = next_replay_line ();
    if (extra) {
      fprintf (stderr,
               "rule-journal replay mismatch after record %" PRIu64
               ": expected additional record\n  expected: %s\n",
               emitted, extra);
      free (extra);
      abort ();
    }
    fclose (replay);
  }
  if (output)
    fclose (output);
  output = replay = 0;
  initialized = false;
  sequence = emitted = conflict_from = 0;
  conflict_to = limit = UINT64_MAX;
  selected_rule = selected_candidate = 0;
}
