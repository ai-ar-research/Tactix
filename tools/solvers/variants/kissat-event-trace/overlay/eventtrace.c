#include "eventtrace.h"

#include "internal.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *trace_file;
static bool trace_initialized, trace_truncated, trace_saw_reorder;
static uint64_t trace_sequence, trace_emitted, trace_limit = 50000;
static uint64_t trace_checkpoint_bucket;

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
  trace_file = fopen (path, "w");
  if (!trace_file)
    return;
  fputs ("# tactix-event-trace\t1\n", trace_file);
  fputs ("# solver\tseq\tevent\tconflicts\tdecisions\tpropagations\t"
         "level\tmode\tactive\tunassigned\ttrail\tqueue\theap\t"
         "retired_queue\tretired_heap\tirred\tred\tbinary\tdigest\t"
         "search_external\ttop32\tpayload\n", trace_file);
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

void kissat_event_trace_learned (kissat *solver) {
  open_trace ();
  if (!trace_file)
    return;
  const size_t size = SIZE_STACK (solver->clause);
  int *external = malloc (size * sizeof *external);
  size_t i = 0;
  for (all_stack (unsigned, lit, solver->clause))
    external[i++] = external_literal (solver, lit);
  qsort (external, size, sizeof *external, compare_external_literals);
  char payload[4096];
  int written = snprintf (payload, sizeof payload, "head=");
  char *p = payload + written;
  size_t left = sizeof payload - (size_t) written;
  i = 0;
  for (all_stack (unsigned, lit, solver->clause)) {
    if (i == 2)
      break;
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
  kissat_event_trace (solver, "learned", payload, false);
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
}
