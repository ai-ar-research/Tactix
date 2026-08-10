#include "eventtrace.h"

#include "internal.h"
#include "inline.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *trace_file;
static bool trace_initialized, trace_truncated, trace_saw_reorder;
static bool trace_detail_window;
static uint64_t trace_sequence, trace_emitted, trace_limit = 50000;
static uint64_t trace_checkpoint_bucket;
static uint64_t trace_detail_from, trace_detail_to = UINT64_MAX;
static FILE *reduce_trace_file;
static bool reduce_trace_initialized;
static uint64_t reduce_trace_from, reduce_trace_to = UINT64_MAX;
static int compare_external_literals (const void *, const void *);

void kissat_event_trace_retier (kissat *solver) {
  char payload[4096];
  const bool stable = solver->stable;
  uint64_t total = 0;
  int written = snprintf (payload, sizeof payload, "tier1=%u|tier2=%u|used=",
                          solver->tier1[stable], solver->tier2[stable]);
  for (unsigned glue = 0; glue <= MAX_GLUE_USED && written > 0 &&
                          (size_t) written < sizeof payload;
       glue++) {
    const uint64_t used = solver->statistics.used[stable].glue[glue];
    total += used;
    written += snprintf (payload + written, sizeof payload - written,
                         "%s%u:%" PRIu64, glue ? "," : "", glue, used);
  }
  if (written > 0 && (size_t) written < sizeof payload)
    snprintf (payload + written, sizeof payload - written, "|total=%" PRIu64,
              total);
  kissat_event_trace (solver, "retier", payload, true);
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

static void open_reduce_trace (void) {
  if (reduce_trace_initialized)
    return;
  reduce_trace_initialized = true;
  const char *path = getenv ("KISSAT_REDUCE_TRACE");
  if (!path || !*path)
    return;
  reduce_trace_file = fopen (path, "w");
  if (!reduce_trace_file)
    return;
  fputs ("# tactix-kissat-reduce\t1\n", reduce_trace_file);
  fputs ("# solver\tkind\treduction\tconflict\tmode\ttier1\ttier2\t"
         "live_red\tcandidates\tvictims\tref\tsize\tglue\tused\t"
         "reason\teligibility\trank\tselected\tliterals\n",
         reduce_trace_file);
  const char *from = getenv ("KISSAT_REDUCE_TRACE_CONFLICT_FROM");
  const char *to = getenv ("KISSAT_REDUCE_TRACE_CONFLICT_TO");
  if (from && *from)
    reduce_trace_from = strtoull (from, 0, 10);
  if (to && *to)
    reduce_trace_to = strtoull (to, 0, 10);
}

static bool reduce_trace_enabled (kissat *solver) {
  open_reduce_trace ();
  if (!reduce_trace_file)
    return false;
  const uint64_t conflicts = solver->statistics.conflicts;
  return reduce_trace_from <= conflicts && conflicts <= reduce_trace_to;
}

void kissat_reduce_trace_clause (kissat *solver, clause *c, unsigned ref,
                                 unsigned used, unsigned tier1,
                                 unsigned tier2, const char *eligibility) {
  if (!reduce_trace_enabled (solver))
    return;
  int *literals = malloc (c->size * sizeof *literals);
  unsigned size = 0;
  for (all_literals_in_clause (lit, c))
    literals[size++] = external_literal (solver, lit);
  qsort (literals, size, sizeof *literals, compare_external_literals);
  fprintf (reduce_trace_file,
           "kissat\tclause\t%" PRIu64 "\t%" PRIu64 "\t%s\t%u\t%u\t"
           "-\t-\t-\t%u\t%u\t%u\t%u\t%u\t%s\t-\t0\t",
           solver->statistics.reductions, solver->statistics.conflicts,
           solver->stable ? "stable" : "focused", tier1, tier2, ref,
           c->size, c->glue, used, c->reason, eligibility);
  for (unsigned i = 0; i != size; i++)
    fprintf (reduce_trace_file, "%s%d", i ? "," : "", literals[i]);
  fputc ('\n', reduce_trace_file);
  free (literals);
}

void kissat_reduce_trace_summary (kissat *solver, unsigned tier1,
                                  unsigned tier2, size_t candidates,
                                  size_t victims) {
  if (!reduce_trace_enabled (solver))
    return;
  uint64_t live = 0;
  ward *const arena = BEGIN_STACK (solver->arena);
  clause *c = (clause *) arena;
  const clause *const end = (clause *) END_STACK (solver->arena);
  while (c != end) {
    if (c->redundant && !c->garbage)
      live++;
    c = kissat_next_clause (c);
  }
  fprintf (reduce_trace_file,
           "kissat\tsummary\t%" PRIu64 "\t%" PRIu64 "\t%s\t%u\t%u\t"
           "%" PRIu64 "\t%zu\t%zu\t-\t-\t-\t-\t-\t-\t-\t-\t-\n",
           solver->statistics.reductions, solver->statistics.conflicts,
           solver->stable ? "stable" : "focused", tier1, tier2, live,
           candidates, victims);
  fflush (reduce_trace_file);
}

void kissat_reduce_trace_rank (kissat *solver, unsigned ref, size_t rank,
                               bool selected) {
  if (!reduce_trace_enabled (solver))
    return;
  fprintf (reduce_trace_file,
           "kissat\trank\t%" PRIu64 "\t%" PRIu64 "\t%s\t-\t-\t-\t"
           "-\t-\t%u\t-\t-\t-\t-\t-\t%zu\t%u\t-\n",
           solver->statistics.reductions, solver->statistics.conflicts,
           solver->stable ? "stable" : "focused", ref, rank, selected);
}

static uint64_t mix (uint64_t hash, uint64_t value) {
  return (hash ^ value) * UINT64_C (1099511628211);
}

static uint64_t state_digest (kissat *solver) {
  uint64_t hash = UINT64_C (1469598103934665603);
  hash = mix (hash, solver->level);
  hash = mix (hash, SIZE_ARRAY (solver->trail));
  for (all_variables (idx)) {
    hash = mix (hash, external_variable (solver, idx));
    hash = mix (hash, ACTIVE (idx));
    hash = mix (hash, (unsigned) (solver->values[LIT (idx)] + 1));
  }
  unsigned seen = 0;
  for (unsigned idx = solver->queue.first;
       !DISCONNECTED (idx) && seen++ <= solver->vars;
       idx = solver->links[idx].next)
    hash = mix (hash, external_variable (solver, idx));
  return hash;
}

static bool already_ranked (const unsigned *ranked, unsigned size,
                            unsigned idx) {
  for (unsigned i = 0; i != size; i++)
    if (ranked[i] == idx)
      return true;
  return false;
}

static unsigned stable_top (kissat *solver, unsigned *ranked) {
  unsigned size = 0;
  while (size != 32) {
    unsigned best = INVALID_IDX;
    for (all_variables (idx)) {
      if (!ACTIVE (idx) || solver->values[LIT (idx)] ||
          already_ranked (ranked, size, idx))
        continue;
      if (best == INVALID_IDX || solver->scores.score[idx] > solver->scores.score[best] ||
          (solver->scores.score[idx] == solver->scores.score[best] && idx < best))
        best = idx;
    }
    if (best == INVALID_IDX)
      break;
    ranked[size++] = best;
  }
  return size;
}

static unsigned focused_top (kissat *solver, unsigned *ranked) {
  unsigned size = 0, seen = 0;
  for (unsigned idx = solver->queue.last;
       !DISCONNECTED (idx) && seen++ <= solver->vars;
       idx = solver->links[idx].prev)
    if (ACTIVE (idx) && !solver->values[LIT (idx)] && size != 32)
      ranked[size++] = idx;
  return size;
}

static void open_trace (void) {
  if (trace_initialized)
    return;
  trace_initialized = true;
  const char *path = getenv ("KISSAT_EVENT_TRACE");
  if (!path || !*path)
    return;
  const char *limit = getenv ("KISSAT_EVENT_TRACE_LIMIT");
  if (limit && *limit)
    trace_limit = strtoull (limit, 0, 10);
  const char *from = getenv ("KISSAT_EVENT_TRACE_CONFLICT_FROM");
  if (from && *from) {
    trace_detail_from = strtoull (from, 0, 10);
    trace_detail_window = true;
  }
  const char *to = getenv ("KISSAT_EVENT_TRACE_CONFLICT_TO");
  if (to && *to) {
    trace_detail_to = strtoull (to, 0, 10);
    trace_detail_window = true;
  }
  trace_file = fopen (path, "w");
  if (!trace_file)
    return;
  fputs ("# tactix-event-trace\t1\n", trace_file);
  if (trace_detail_window)
    fprintf (trace_file, "# detail-conflicts\t%" PRIu64 "\t%" PRIu64 "\n",
             trace_detail_from, trace_detail_to);
  fputs ("# solver\tseq\tevent\tconflicts\tdecisions\tpropagations\t"
         "level\tmode\tactive\tunassigned\ttrail\tqueue\theap\t"
         "retired_queue\tretired_heap\tirred\tred\tbinary\tdigest\t"
         "search_external\ttop32\tpayload\n", trace_file);
}

static bool detail_enabled (kissat *solver) {
  open_trace ();
  if (!trace_file)
    return false;
  if (!trace_detail_window)
    return true;
  const uint64_t conflicts = solver->statistics.conflicts;
  return trace_detail_from <= conflicts && conflicts <= trace_detail_to;
}

void kissat_event_trace (kissat *solver, const char *event,
                         const char *payload, bool snapshot) {
  open_trace ();
  if (!trace_file)
    return;
  if (trace_emitted >= trace_limit) {
    if (!trace_truncated) {
      fprintf (trace_file, "# truncated at %" PRIu64 " records\n", trace_limit);
      trace_truncated = true;
    }
    return;
  }
  trace_emitted++;
  uint64_t queue = 0, retired_queue = 0, retired_heap = 0, digest = 0;
  size_t heap = 0;
  char top[384] = "-";
  if (snapshot) {
    unsigned seen = 0;
    for (unsigned idx = solver->queue.first;
         !DISCONNECTED (idx) && seen++ <= solver->vars;
         idx = solver->links[idx].next) {
      queue++;
      retired_queue += !ACTIVE (idx);
    }
    heap = SIZE_STACK (solver->scores.stack);
    for (all_stack (unsigned, idx, solver->scores.stack))
      retired_heap += !ACTIVE (idx);
    unsigned ranked[32];
    const unsigned size = solver->stable ? stable_top (solver, ranked)
                                         : focused_top (solver, ranked);
    if (size) {
      char *p = top;
      size_t left = sizeof top;
      for (unsigned i = 0; i != size; i++) {
        const int written = snprintf (p, left, "%s%u", i ? "," : "",
                                      external_variable (solver, ranked[i]));
        if (written < 0 || (size_t) written >= left)
          break;
        p += written;
        left -= (size_t) written;
      }
    }
    digest = state_digest (solver);
  }
  int search_external = 0;
  if (solver->queue.search.idx < solver->vars)
    search_external = (int) external_variable (solver, solver->queue.search.idx);
  fprintf (trace_file,
           "kissat\t%" PRIu64 "\t%s\t%" PRIu64 "\t%" PRIu64
           "\t%" PRIu64 "\t%u\t%s\t%u\t%u\t%zu\t%" PRIu64
           "\t%zu\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64
           "\t%" PRIu64 "\t%" PRIu64 "\t%016" PRIx64
           "\t%d\t%s\t%s\n",
           trace_sequence++, event, solver->statistics.conflicts,
           solver->statistics.decisions, solver->statistics.propagations,
           solver->level, solver->stable ? "stable" : "focused",
           snapshot ? solver->active : 0,
           snapshot ? solver->unassigned : 0,
           snapshot ? SIZE_ARRAY (solver->trail) : 0,
           queue, heap, retired_queue, retired_heap,
           solver->statistics.clauses_irredundant,
           solver->statistics.clauses_redundant,
           solver->statistics.clauses_binary, digest, search_external, top,
           payload ? payload : "-");
  if (!strcmp (event, "reorder_begin")) {
    trace_saw_reorder = true;
    trace_checkpoint_bucket = solver->statistics.conflicts / 1000;
  }
}

void kissat_event_trace_detail (kissat *solver, const char *event, int payload) {
  if (!detail_enabled (solver))
    return;
  char buffer[32];
  snprintf (buffer, sizeof buffer, "%d", payload);
  // Match Tactix's diagnostic post-analysis ranking snapshot.  This localizes
  // a latent bump-order difference at the conflict that creates it rather
  // than at the later decision that exposes it.
  const bool snapshot = !strcmp (event, "analysis_end");
  kissat_event_trace (solver, event, buffer, snapshot);
}

void kissat_event_trace_literal (kissat *solver, const char *event,
                                 unsigned lit) {
  kissat_event_trace_detail (solver, event, external_literal (solver, lit));
}

static int compare_external_literals (const void *a, const void *b) {
  const int x = *(const int *) a, y = *(const int *) b;
  return (x > y) - (x < y);
}

void kissat_event_trace_watch_scan (kissat *solver, unsigned propagated) {
  if (!detail_enabled (solver) || trace_emitted >= trace_limit)
    return;
  const unsigned watched = NOT (propagated);
  watches entries = WATCHES (watched);
  const watch *p = BEGIN_WATCHES (entries);
  const watch *const end = END_WATCHES (entries);
  while (p != end) {
    if (trace_emitted >= trace_limit)
      return;
    const watch head = *p++;
    const unsigned blocking = head.blocking.lit;
    size_t size;
    int *external;
    if (head.type.binary) {
      size = 2;
      external = malloc (size * sizeof *external);
      external[0] = external_literal (solver, watched);
      external[1] = external_literal (solver, blocking);
    } else {
      const reference ref = p++->raw;
      clause *c = kissat_dereference_clause (solver, ref);
      size = c->size;
      external = malloc (size * sizeof *external);
      size_t i = 0;
      for (all_literals_in_clause (lit, c))
        external[i++] = external_literal (solver, lit);
    }
    qsort (external, size, sizeof *external, compare_external_literals);
    char payload[4096];
    int written = snprintf (payload, sizeof payload,
                            "scan=%d|block=%d|clause=",
                            external_literal (solver, propagated),
                            external_literal (solver, blocking));
    char *q = payload + written;
    size_t left = sizeof payload - (size_t) written;
    for (size_t i = 0; i != size; i++) {
      written = snprintf (q, left, "%s%d", i ? "," : "", external[i]);
      if (written < 0 || (size_t) written >= left)
        break;
      q += written;
      left -= (size_t) written;
    }
    free (external);
    kissat_event_trace (solver, "watch", payload, false);
  }
}

void kissat_event_trace_implications (kissat *solver, size_t trail_begin) {
  if (!detail_enabled (solver) || trace_emitted >= trace_limit)
    return;
  const size_t trail_size = SIZE_ARRAY (solver->trail);
  for (size_t pos = trail_begin; pos != trail_size; pos++) {
    if (trace_emitted >= trace_limit)
      return;
    const unsigned assigned_lit = PEEK_ARRAY (solver->trail, pos);
    const assigned *assignment = solver->assigned + IDX (assigned_lit);
    if (assignment->reason == DECISION_REASON ||
        assignment->reason == UNIT_REASON)
      continue;
    size_t size;
    int *external;
    if (assignment->binary) {
      size = 2;
      external = malloc (size * sizeof *external);
      external[0] = external_literal (solver, assigned_lit);
      external[1] = external_literal (solver, assignment->reason);
    } else {
      clause *reason = kissat_dereference_clause (solver, assignment->reason);
      size = reason->size;
      external = malloc (size * sizeof *external);
      size_t i = 0;
      for (all_literals_in_clause (lit, reason))
        external[i++] = external_literal (solver, lit);
    }
    qsort (external, size, sizeof *external, compare_external_literals);
    char payload[4096];
    int written = snprintf (payload, sizeof payload, "lit=%d|level=%u|reason=",
                            external_literal (solver, assigned_lit),
                            assignment->level);
    char *p = payload + written;
    size_t left = sizeof payload - (size_t) written;
    for (size_t i = 0; i != size; i++) {
      written = snprintf (p, left, "%s%d", i ? "," : "", external[i]);
      if (written < 0 || (size_t) written >= left)
        break;
      p += written;
      left -= (size_t) written;
    }
    free (external);
    kissat_event_trace (solver, "imply", payload, false);
  }
}

void kissat_event_trace_conflict (kissat *solver, clause *conflict) {
  if (!detail_enabled (solver))
    return;
  const size_t size = conflict->size;
  int *external = malloc (size * sizeof *external);
  char payload[4096];
  int written = snprintf (payload, sizeof payload, "order=");
  char *p = payload + written;
  size_t left = sizeof payload - (size_t) written;
  size_t i = 0;
  for (all_literals_in_clause (lit, conflict))
    external[i++] = external_literal (solver, lit);
  for (i = 0; i != size; i++) {
    written = snprintf (p, left, "%s%d", i ? "," : "", external[i]);
    if (written < 0 || (size_t) written >= left)
      break;
    p += written;
    left -= (size_t) written;
  }
  qsort (external, size, sizeof *external, compare_external_literals);
  written = snprintf (p, left, "|set=");
  if (written < 0 || (size_t) written >= left) {
    free (external);
    return;
  }
  p += written;
  left -= (size_t) written;
  for (i = 0; i != size; i++) {
    written = snprintf (p, left, "%s%d", i ? "," : "", external[i]);
    if (written < 0 || (size_t) written >= left)
      break;
    p += written;
    left -= (size_t) written;
  }
  free (external);
  kissat_event_trace (solver, "conflict", payload, false);
}

void kissat_event_trace_clause (kissat *solver, const char *event) {
  if (!detail_enabled (solver) || trace_emitted >= trace_limit)
    return;
  const size_t size = SIZE_STACK (solver->clause);
  int *external = malloc (size * sizeof *external);
  size_t i = 0;
  for (all_stack (unsigned, lit, solver->clause))
    external[i++] = external_literal (solver, lit);
  qsort (external, size, sizeof *external, compare_external_literals);
  char payload[4096];
  int written = snprintf (payload, sizeof payload, "order=");
  char *p = payload + written;
  size_t left = sizeof payload - (size_t) written;
  i = 0;
  for (all_stack (unsigned, lit, solver->clause)) {
    written = snprintf (p, left, "%s%d", i ? "," : "",
                        external_literal (solver, lit));
    if (written < 0 || (size_t) written >= left)
      break;
    p += written;
    left -= (size_t) written;
    i++;
  }
  written = snprintf (p, left, "|set=");
  if (written < 0 || (size_t) written >= left) {
    free (external);
    return;
  }
  p += written;
  left -= (size_t) written;
  for (i = 0; i != size; i++) {
    written = snprintf (p, left, "%s%d", i ? "," : "", external[i]);
    if (written < 0 || (size_t) written >= left)
      break;
    p += written;
    left -= (size_t) written;
  }
  free (external);
  kissat_event_trace (solver, event, payload, false);
}

void kissat_event_trace_learned (kissat *solver) {
  kissat_event_trace_clause (solver, "learned");
}

void kissat_event_trace_checkpoint (kissat *solver) {
  if (!trace_saw_reorder)
    return;
  const uint64_t bucket = solver->statistics.conflicts / 1000;
  if (bucket <= trace_checkpoint_bucket)
    return;
  trace_checkpoint_bucket = bucket;
  kissat_event_trace (solver, "checkpoint", "-", true);
}

void kissat_event_trace_close (void) {
  if (!trace_file)
    return;
  fflush (trace_file);
  fclose (trace_file);
  trace_file = 0;
  if (reduce_trace_file) {
    fflush (reduce_trace_file);
    fclose (reduce_trace_file);
    reduce_trace_file = 0;
  }
}
