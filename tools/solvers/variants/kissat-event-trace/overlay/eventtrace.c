#include "eventtrace.h"

#include "internal.h"
#include "inline.h"
#include "heap.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *trace_file;
static bool trace_initialized, trace_truncated, trace_saw_reorder;
static bool trace_detail_window;
static bool trace_skip_watch, trace_skip_imply, trace_skip_heap,
            trace_skip_decisions, trace_skip_randec_draws,
            trace_skip_analysis_ticks, trace_search_only, trace_control_only,
            trace_watch_only;
static uint64_t trace_sequence, trace_emitted, trace_limit = 50000;
static uint64_t trace_checkpoint_bucket;
static uint64_t trace_detail_from, trace_detail_to = UINT64_MAX;
static FILE *reduce_trace_file;
static bool reduce_trace_initialized;
static uint64_t reduce_trace_from, reduce_trace_to = UINT64_MAX;
static int compare_external_literals (const void *, const void *);

void kissat_event_trace_tiers (kissat *solver, const char *event) {
  char payload[128];
  snprintf (payload, sizeof payload,
            "focused=%u,%u|stable=%u,%u|reduce=%u,%u",
            solver->tier1[0], solver->tier2[0], solver->tier1[1],
            solver->tier2[1], TIER1, TIER2);
  kissat_event_trace (solver, event, payload, true);
}

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

static size_t parse_clause_set (const char *spec, int *selected,
                                size_t capacity) {
  size_t size = 0;
  const char *cursor = spec;
  while (*cursor) {
    if (size == capacity)
      return 0;
    char *end = 0;
    const long value = strtol (cursor, &end, 10);
    if (end == cursor)
      return 0;
    selected[size++] = (int) value;
    cursor = end;
    if (*cursor == ',')
      cursor++;
    else if (*cursor)
      return 0;
  }
  return size;
}

static void annotate_clause_set (kissat *solver, char *payload,
                                 size_t capacity) {
  const char *spec = getenv ("SAT_EVENT_TRACE_CLAUSE_SET");
  if (!spec || !*spec)
    return;
  const char *conflict_from =
      getenv ("SAT_EVENT_TRACE_CLAUSE_SET_CONFLICT_FROM");
  if (conflict_from && *conflict_from) {
    char *end = 0;
    const unsigned long long limit = strtoull (conflict_from, &end, 10);
    if (end != conflict_from && !*end && solver->statistics.conflicts < limit)
      return;
  }
  const char *conflict_to =
      getenv ("SAT_EVENT_TRACE_CLAUSE_SET_CONFLICT_TO");
  if (conflict_to && *conflict_to) {
    char *end = 0;
    const unsigned long long limit = strtoull (conflict_to, &end, 10);
    if (end != conflict_to && !*end && solver->statistics.conflicts > limit)
      return;
  }
  int selected[64];
  const size_t selected_size =
      parse_clause_set (spec, selected, sizeof selected / sizeof *selected);
  if (!selected_size)
    return;
  size_t used = strlen (payload);
  int written = snprintf (payload + used, capacity - used,
                          "|clause_set=%s|clause_poses=", spec);
  if (written <= 0 || (size_t) written >= capacity - used)
    return;
  used += (size_t) written;
  size_t matches = 0;
  ward *const arena = BEGIN_STACK (solver->arena);
  clause *c = (clause *) arena;
  const clause *const end = (clause *) END_STACK (solver->arena);
  while (c != end && used < capacity) {
    if (!c->garbage && c->size == selected_size) {
      bool matched[64] = {false};
      bool same = true;
      for (all_literals_in_clause (lit, c)) {
        const int external = external_literal (solver, lit);
        size_t i = 0;
        while (i != selected_size &&
               (matched[i] || selected[i] != external))
          i++;
        if (i == selected_size) {
          same = false;
          break;
        }
        matched[i] = true;
      }
      if (same) {
        written = snprintf (payload + used, capacity - used,
                            "%ssearched=%u,redundant=%u,pose=",
                            matches ? ";" : "", c->searched,
                            c->redundant);
        if (written <= 0 || (size_t) written >= capacity - used)
          return;
        used += (size_t) written;
        size_t i = 0;
        for (all_literals_in_clause (lit, c)) {
          written = snprintf (payload + used, capacity - used, "%s%d",
                              i++ ? "," : "",
                              external_literal (solver, lit));
          if (written <= 0 || (size_t) written >= capacity - used)
            return;
          used += (size_t) written;
        }
        matches++;
      }
    }
    c = kissat_next_clause (c);
  }
  if (!matches && used < capacity - 1) {
    payload[used++] = '-';
    payload[used] = 0;
  }
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
  trace_skip_watch = getenv ("KISSAT_EVENT_TRACE_SKIP_WATCH") != 0;
  trace_skip_imply = getenv ("KISSAT_EVENT_TRACE_SKIP_IMPLY") != 0;
  trace_skip_heap = getenv ("KISSAT_EVENT_TRACE_SKIP_HEAP") != 0;
  trace_skip_decisions = getenv ("KISSAT_EVENT_TRACE_SKIP_DECISIONS") != 0;
  trace_skip_randec_draws =
      getenv ("KISSAT_EVENT_TRACE_SKIP_RANDEC_DRAWS") != 0;
  trace_skip_analysis_ticks =
      getenv ("KISSAT_EVENT_TRACE_SKIP_ANALYSIS_TICKS") != 0;
  trace_search_only = getenv ("KISSAT_EVENT_TRACE_SEARCH_ONLY") != 0;
  trace_control_only = getenv ("KISSAT_EVENT_TRACE_CONTROL_ONLY") != 0;
  trace_watch_only = getenv ("KISSAT_EVENT_TRACE_WATCH_ONLY") != 0;
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
  if (trace_skip_watch && !strcmp (event, "watch"))
    return;
  if (trace_skip_imply && !strcmp (event, "imply"))
    return;
  if (trace_skip_heap && !strncmp (event, "heap_", 5))
    return;
  if (trace_skip_decisions &&
      (!strcmp (event, "decision") || !strcmp (event, "randec_draw") ||
       !strcmp (event, "phase_pick")))
    return;
  if (trace_skip_randec_draws && !strcmp (event, "randec_draw"))
    return;
  if (trace_skip_analysis_ticks && !strncmp (event, "analysis_tick", 13))
    return;
  if (trace_watch_only && !snapshot && strcmp (event, "watch"))
    return;
  if (trace_search_only && !snapshot && strcmp (event, "decision") &&
      strcmp (event, "randec_draw") && strcmp (event, "randec_state") &&
      strcmp (event, "phase_pick") &&
      strcmp (event, "score_bump") &&
      strcmp (event, "assignment") &&
      strcmp (event, "propagate_cursor") &&
      strcmp (event, "heap_scores") &&
      strcmp (event, "watch") && strcmp (event, "imply") &&
      strcmp (event, "conflict") &&
      strcmp (event, "analyzed") && strncmp (event, "analysis_", 9) &&
      strncmp (event, "learned", 7))
    return;
  if (trace_control_only && !snapshot && strcmp (event, "decision") &&
      strcmp (event, "randec_draw") && strcmp (event, "randec_state") &&
      strcmp (event, "score_bump") &&
      strcmp (event, "heap_scores") &&
      strcmp (event, "assignment") &&
      strcmp (event, "heap_reinsert") &&
      strcmp (event, "watch") &&
      strcmp (event, "conflict") &&
      strcmp (event, "analyzed") && strncmp (event, "analysis_", 9) &&
      strncmp (event, "learned", 7) && strncmp (event, "factor_", 7) &&
      strncmp (event, "reason_", 7))
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
  char annotated_payload[4096];
  snprintf (annotated_payload, sizeof annotated_payload, "%s",
            payload ? payload : "-");
  if (snapshot) {
    const char *selected = getenv ("SAT_EVENT_TRACE_WATCH_LIST");
    if (selected && *selected) {
      const int wanted = atoi (selected);
      unsigned watched = INVALID_LIT;
      for (all_literals (lit))
        if (external_literal (solver, lit) == wanted) {
          watched = lit;
          break;
        }
      size_t used = strlen (annotated_payload);
      int written = snprintf (annotated_payload + used,
                              sizeof annotated_payload - used,
                              "|watch_lit=%d|binary=", wanted);
      if (written > 0)
        used += (size_t) written;
      bool first = true;
      if (watched != INVALID_LIT) {
        watches entries = WATCHES (watched);
        const watch *p = BEGIN_WATCHES (entries);
        const watch *const end = END_WATCHES (entries);
        while (p != end && used < sizeof annotated_payload) {
          const watch head = *p++;
          if (!head.type.binary) {
            p++;
            continue;
          }
          written = snprintf (annotated_payload + used,
                              sizeof annotated_payload - used, "%s%d",
                              first ? "" : ",",
                              external_literal (solver, head.binary.lit));
          if (written <= 0)
            break;
          used += (size_t) written;
          first = false;
        }
      }
    }
    annotate_clause_set (solver, annotated_payload,
                         sizeof annotated_payload);
  }
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
           annotated_payload);
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

void kissat_event_trace_randec_draw (kissat *solver, unsigned raw,
                                     unsigned idx, bool accepted) {
  if (!detail_enabled (solver))
    return;
  char payload[160];
  snprintf (payload, sizeof payload,
            "raw=%u|slot=%u|external=%u|accepted=%u", raw, idx,
            external_variable (solver, idx), accepted);
  kissat_event_trace (solver, "randec_draw", payload, false);
}

void kissat_event_trace_randec_state (kissat *solver, const char *stage) {
  if (!detail_enabled (solver))
    return;
  char payload[192];
  snprintf (payload, sizeof payload,
            "stage=%s|remaining=%u|next=%" PRIu64 "|sequence=%" PRIu64,
            stage, solver->randec, solver->limits.randec.conflicts,
            solver->statistics.random_sequences);
  kissat_event_trace (solver, "randec_state", payload, false);
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
    clause *large = 0;
    if (head.type.binary) {
      size = 2;
      external = malloc (size * sizeof *external);
      external[0] = external_literal (solver, watched);
      external[1] = external_literal (solver, blocking);
    } else {
      const reference ref = p++->raw;
      large = kissat_dereference_clause (solver, ref);
      size = large->size;
      external = malloc (size * sizeof *external);
      size_t i = 0;
      for (all_literals_in_clause (lit, large))
        external[i++] = external_literal (solver, lit);
    }
    const char *selected = getenv ("KISSAT_EVENT_TRACE_WATCH_LITERAL");
    if (selected) {
      const int wanted = abs (atoi (selected));
      bool contains = false;
      for (size_t i = 0; i != size; i++)
        contains |= abs (external[i]) == wanted;
      if (!contains) {
        free (external);
        continue;
      }
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
    if (large && getenv ("KISSAT_EVENT_TRACE_WATCH_POSE")) {
      written = snprintf (q, left, "|searched=%u|pose=", large->searched);
      if (written >= 0 && (size_t) written < left) {
        q += written;
        left -= (size_t) written;
        size_t i = 0;
        for (all_literals_in_clause (lit, large)) {
          written = snprintf (q, left, "%s%d", i++ ? "," : "",
                              external_literal (solver, lit));
          if (written < 0 || (size_t) written >= left)
            break;
          q += written;
          left -= (size_t) written;
        }
      }
    }
    free (external);
    kissat_event_trace (solver, "watch", payload, false);
  }
}

void kissat_event_trace_propagate_cursor (kissat *solver) {
  if (!detail_enabled (solver) || solver->propagate == END_ARRAY (solver->trail))
    return;
  char payload[96];
  snprintf (payload, sizeof payload, "next=%d|pos=%zu|trail=%zu",
            external_literal (solver, *solver->propagate),
            (size_t) (solver->propagate - BEGIN_ARRAY (solver->trail)),
            SIZE_ARRAY (solver->trail));
  kissat_event_trace (solver, "propagate_cursor", payload, false);
}

void kissat_event_trace_implications (kissat *solver, size_t trail_begin) {
  if (!detail_enabled (solver) || trace_emitted >= trace_limit)
    return;
  const size_t trail_size = SIZE_ARRAY (solver->trail);
  // Root propagation can flush the complete trail before this diagnostic is
  // called.  In that case the saved pre-propagation position is no longer an
  // index into the current trail; there are no retained implications to emit.
  if (trail_begin > trail_size)
    return;
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
  written = snprintf (p, left, "|levels=");
  if (written < 0 || (size_t) written >= left) {
    free (external);
    return;
  }
  p += written;
  left -= (size_t) written;
  i = 0;
  for (all_literals_in_clause (lit, conflict)) {
    written = snprintf (p, left, "%s%d:%u", i++ ? "," : "",
                        external_literal (solver, lit), LEVEL (lit));
    if (written < 0 || (size_t) written >= left)
      break;
    p += written;
    left -= (size_t) written;
  }
  free (external);
  kissat_event_trace (solver, "conflict", payload, false);
  const char *selected = getenv ("KISSAT_EVENT_TRACE_ASSIGNMENT");
  if (selected && *selected) {
    const unsigned wanted = (unsigned) strtoul (selected, 0, 10);
    for (all_variables (idx)) {
      if (external_variable (solver, idx) != wanted)
        continue;
      assigned *a = solver->assigned + idx;
      char state[256], reason[64];
      if (a->reason == DECISION_REASON)
        strcpy (reason, "decision");
      else if (a->reason == UNIT_REASON)
        strcpy (reason, "unit");
      else if (a->binary)
        snprintf (reason, sizeof reason, "%d",
                  external_literal (solver, a->reason));
      else
        snprintf (reason, sizeof reason, "clause:%u", a->reason);
      snprintf (state, sizeof state,
                "external=%u|level=%u|trail=%u|binary=%u|reason=%s",
                wanted, a->level, a->trail, a->binary, reason);
      kissat_event_trace (solver, "assignment", state, false);
      break;
    }
  }
}

void kissat_event_trace_heap_scores (kissat *solver) {
  if (!detail_enabled (solver) || !solver->stable)
    return;
  heap *heap = SCORES;
  char payload[4096];
  char *p = payload;
  size_t left = sizeof payload;
  const size_t size = MIN ((size_t) 32, SIZE_STACK (heap->stack));
  for (size_t i = 0; i != size; i++) {
    const unsigned idx = PEEK_STACK (heap->stack, i);
    const int written = snprintf (p, left, "%s%d:%.17g", i ? "," : "",
                                  external_literal (solver, LIT (idx)),
                                  heap->score[idx]);
    if (written < 0 || (size_t) written >= left)
      break;
    p += written;
    left -= (size_t) written;
  }
  kissat_event_trace (solver, "heap_scores", payload, false);
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

void kissat_event_trace_factor_grid (kissat *solver, size_t factors,
                                     size_t remainders, size_t delta) {
  char payload[128];
  snprintf (payload, sizeof payload,
            "factors=%zu|remainders=%zu|delta=%zu", factors, remainders,
            delta);
  kissat_event_trace (solver, "factor_grid", payload, false);
}

void kissat_event_trace_factor_grid_member (kissat *solver, size_t ordinal,
                                            unsigned lit) {
  char payload[128];
  snprintf (payload, sizeof payload, "ordinal=%zu|lit=%d", ordinal,
            external_literal (solver, lit));
  kissat_event_trace (solver, "factor_grid_member", payload, false);
}

void kissat_event_trace_factor_seed (kissat *solver, unsigned lit,
                                     size_t remainders) {
  char payload[128];
  snprintf (payload, sizeof payload, "external=%d|remainders=%zu",
            external_literal (solver, lit), remainders);
  kissat_event_trace (solver, "factor_seed", payload, false);
}

void kissat_event_trace_factor_candidate (kissat *solver, unsigned seed,
                                          size_t depth, size_t ordinal,
                                          unsigned candidate, unsigned count,
                                          unsigned score) {
  const char *selected = getenv ("KISSAT_EVENT_TRACE_FACTOR_SEED");
  if (!selected || atoi (selected) != external_literal (solver, seed))
    return;
  char payload[192];
  snprintf (payload, sizeof payload,
            "seed=%d|depth=%zu|ordinal=%zu|candidate=%d|count=%u|score=%u",
            external_literal (solver, seed), depth, ordinal,
            external_literal (solver, candidate), count, score);
  kissat_event_trace (solver, "factor_candidate", payload, false);
}

static bool selected_factor_seed (kissat *solver, unsigned seed) {
  const char *selected = getenv ("KISSAT_EVENT_TRACE_FACTOR_SEED");
  return selected && atoi (selected) == external_literal (solver, seed);
}

void kissat_event_trace_factor_remainder_binary (kissat *solver,
                                                 unsigned seed,
                                                 size_t ordinal,
                                                 unsigned other) {
  if (!selected_factor_seed (solver, seed))
    return;
  char payload[160];
  snprintf (payload, sizeof payload,
            "seed=%d|ordinal=%zu|source=binary|lits=%d",
            external_literal (solver, seed), ordinal,
            external_literal (solver, other));
  kissat_event_trace (solver, "factor_remainder", payload, false);
}

void kissat_event_trace_factor_remainder_clause (kissat *solver,
                                                 unsigned seed,
                                                 size_t ordinal, clause *c) {
  if (!selected_factor_seed (solver, seed))
    return;
  int *external = malloc ((c->size - 1) * sizeof *external);
  size_t size = 0;
  for (all_literals_in_clause (lit, c))
    if (lit != seed)
      external[size++] = external_literal (solver, lit);
  qsort (external, size, sizeof *external, compare_external_literals);
  char payload[4096];
  int written = snprintf (payload, sizeof payload,
                          "seed=%d|ordinal=%zu|source=large|lits=",
                          external_literal (solver, seed), ordinal);
  for (size_t i = 0; i != size && written > 0 &&
                     (size_t) written < sizeof payload;
       i++)
    written += snprintf (payload + written, sizeof payload - written,
                         "%s%d", i ? "," : "", external[i]);
  free (external);
  kissat_event_trace (solver, "factor_remainder", payload, false);
}

static void append_sorted_clause (kissat *solver, clause *c, char **p,
                                  size_t *left) {
  int *external = malloc (c->size * sizeof *external);
  size_t size = 0;
  for (all_literals_in_clause (lit, c))
    external[size++] = external_literal (solver, lit);
  qsort (external, size, sizeof *external, compare_external_literals);
  for (size_t i = 0; i != size && *left; i++) {
    const int written = snprintf (*p, *left, "%s%d", i ? "," : "",
                                  external[i]);
    if (written < 0 || (size_t) written >= *left) {
      *left = 0;
      break;
    }
    *p += written;
    *left -= (size_t) written;
  }
  free (external);
}

void kissat_event_trace_factor_score_entries (kissat *solver, unsigned seed,
                                              size_t depth,
                                              unsigned candidate) {
  const char *selected_seed = getenv ("KISSAT_EVENT_TRACE_FACTOR_SEED");
  const char *selected_candidate =
      getenv ("KISSAT_EVENT_TRACE_FACTOR_CANDIDATE");
  if (!selected_seed || !selected_candidate ||
      atoi (selected_seed) != external_literal (solver, seed) ||
      atoi (selected_candidate) != external_literal (solver, candidate))
    return;
  size_t ordinal = 0;
  for (all_binary_large_watches (watch, WATCHES (candidate))) {
    char payload[4096];
    if (watch.type.binary) {
      snprintf (
          payload, sizeof payload,
          "seed=%d|depth=%zu|candidate=%d|ordinal=%zu|kind=binary|live=1|id=0|lits=%d,%d",
          external_literal (solver, seed), depth,
          external_literal (solver, candidate), ordinal,
          external_literal (solver, candidate),
          external_literal (solver, watch.binary.lit));
    } else {
      clause *c = kissat_dereference_clause (solver, watch.large.ref);
      const int written = snprintf (
          payload, sizeof payload,
          "seed=%d|depth=%zu|candidate=%d|ordinal=%zu|kind=large|live=1|id=%u|lits=",
          external_literal (solver, seed), depth,
          external_literal (solver, candidate), ordinal, watch.large.ref);
      if (written > 0 && (size_t) written < sizeof payload) {
        char *p = payload + written;
        size_t left = sizeof payload - (size_t) written;
        append_sorted_clause (solver, c, &p, &left);
      }
    }
    kissat_event_trace (solver, "factor_score_entry", payload, false);
    ordinal++;
  }
}

void kissat_event_trace_sweep_unit (kissat *solver, unsigned lit,
                                    const char *source) {
  if (!detail_enabled (solver))
    return;
  char payload[192];
  snprintf (payload, sizeof payload, "lit=%d|source=%s",
            external_literal (solver, lit), source);
  kissat_event_trace (solver, "sweep_unit", payload, false);
}

void kissat_event_trace_factor_literal_state (kissat *solver, unsigned lit,
                                              bool active, bool marked,
                                              int value, size_t score) {
  const char *selected = getenv ("KISSAT_EVENT_TRACE_FACTOR_LITERAL");
  if (!selected || atoi (selected) != external_literal (solver, lit))
    return;
  char payload[192];
  snprintf (payload, sizeof payload,
            "external=%d|active=%d|marked=%d|value=%d|score=%zu",
            external_literal (solver, lit), active, marked, value, score);
  kissat_event_trace (solver, "factor_literal_state", payload, false);

  size_t ordinal = 0;
  for (all_binary_large_watches (watch, WATCHES (lit))) {
    char entry[4096];
    if (watch.type.binary) {
      int a = external_literal (solver, lit);
      int b = external_literal (solver, watch.binary.lit);
      if (b < a) {
        const int tmp = a;
        a = b;
        b = tmp;
      }
      snprintf (entry, sizeof entry,
                "external=%d|ordinal=%zu|kind=binary|live=1|id=0|lits=%d,%d",
                external_literal (solver, lit), ordinal, a, b);
    } else {
      clause *c = kissat_dereference_clause (solver, watch.large.ref);
      const int written = snprintf (
          entry, sizeof entry,
          "external=%d|ordinal=%zu|kind=large|live=1|id=%u|lits=",
          external_literal (solver, lit), ordinal, watch.large.ref);
      if (written > 0 && (size_t) written < sizeof entry) {
        char *p = entry + written;
        size_t left = sizeof entry - (size_t) written;
        append_sorted_clause (solver, c, &p, &left);
      }
    }
    kissat_event_trace (solver, "factor_literal_entry", entry, false);
    ordinal++;
  }
}

void kissat_event_trace_substitute_map (kissat *solver, unsigned lit,
                                        unsigned representative) {
  const char *selected = getenv ("KISSAT_EVENT_TRACE_SUBSTITUTE_LITERAL");
  if (!selected || atoi (selected) != external_literal (solver, lit))
    return;
  char payload[192];
  snprintf (payload, sizeof payload,
            "external=%d|representative=%d|active=%d|value=%d",
            external_literal (solver, lit),
            external_literal (solver, representative), ACTIVE (IDX (lit)),
            solver->values[lit]);
  kissat_event_trace (solver, "substitute_map", payload, false);
}

void kissat_event_trace_factor_candidate_source (
    kissat *solver, unsigned seed, size_t depth, unsigned scan,
    unsigned candidate, clause *current, clause *matched) {
  if (!selected_factor_seed (solver, seed))
    return;
  char payload[4096];
  int written = snprintf (
      payload, sizeof payload,
      "seed=%d|depth=%zu|scan=%d|candidate=%d|current=",
      external_literal (solver, seed), depth, external_literal (solver, scan),
      external_literal (solver, candidate));
  char *p = payload + written;
  size_t left = sizeof payload - (size_t) written;
  append_sorted_clause (solver, current, &p, &left);
  if (left) {
    written = snprintf (p, left, "|matched=");
    if (written > 0 && (size_t) written < left) {
      p += written;
      left -= (size_t) written;
      append_sorted_clause (solver, matched, &p, &left);
    }
  }
  kissat_event_trace (solver, "factor_candidate_source", payload, false);
}

void kissat_event_trace_factor_candidate_binary_source (
    kissat *solver, unsigned seed, size_t depth, unsigned q,
    unsigned candidate) {
  if (!selected_factor_seed (solver, seed))
    return;
  char payload[192];
  snprintf (payload, sizeof payload,
            "seed=%d|depth=%zu|q=%d|candidate=%d|internal_seed=%u|internal_q=%u|internal_candidate=%u",
            external_literal (solver, seed), depth,
            external_literal (solver, q),
            external_literal (solver, candidate), seed, q, candidate);
  kissat_event_trace (solver, "factor_candidate_binary_source", payload,
                      false);
}

void kissat_event_trace_factor_heap (kissat *solver, unsigned seed,
                                     const heap *schedule) {
  const char *selected = getenv ("KISSAT_EVENT_TRACE_FACTOR_HEAP");
  int external_a = 0, external_b = 0;
  if (!selected || sscanf (selected, "%d,%d", &external_a, &external_b) != 2)
    return;
  unsigned a = INVALID_LIT, b = INVALID_LIT;
  for (all_literals (lit)) {
    const int external = external_literal (solver, lit);
    if (external == external_a)
      a = lit;
    if (external == external_b)
      b = lit;
  }
  if (a == INVALID_LIT || b == INVALID_LIT)
    return;
  char payload[256];
  snprintf (payload, sizeof payload,
            "seed=%d|a=%d|a_pos=%u|a_score=%.0f|b=%d|b_pos=%u|b_score=%.0f|size=%zu",
            external_literal (solver, seed), external_a,
            kissat_get_heap_pos (schedule, a),
            kissat_get_heap_score (schedule, a), external_b,
            kissat_get_heap_pos (schedule, b),
            kissat_get_heap_score (schedule, b),
            kissat_size_heap ((heap *) schedule));
  kissat_event_trace (solver, "factor_heap", payload, false);
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
