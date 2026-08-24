#include "eventtrace.h"

#include "internal.h"
#include "inline.h"
#include "heap.h"
#include "kitten.h"

#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *trace_file;
static bool trace_initialized, trace_truncated, trace_saw_reorder;
static bool trace_detail_window;
static bool trace_heap_each_bump;
static bool trace_heap_pop_only;
static bool trace_skip_watch, trace_skip_imply, trace_skip_heap,
            trace_skip_decisions, trace_skip_randec_draws,
            trace_skip_analysis_ticks, trace_keep_snapshots,
            trace_search_only, trace_control_only,
            trace_glue_only, trace_phase_only,
            trace_trajectory_only, trace_sweep_only, trace_compact_probe,
            trace_watch_only, trace_watch_cost,
            trace_watch_within_vivify, trace_vivify_watch_active;
static uint64_t trace_vivify_watch_ordinal;
static uint64_t trace_sequence, trace_emitted, trace_limit = 50000;
static uint64_t trace_checkpoint_bucket;
static uint64_t trace_detail_from, trace_detail_to = UINT64_MAX;
static uint64_t trace_heap_from_decision = UINT64_MAX;
static uint64_t trace_heap_pop_decision = UINT64_MAX;
static uint64_t trace_heap_remove_decision = UINT64_MAX;
static bool trace_bve_heap_initialized;
static unsigned trace_bve_heap_vars[8], trace_bve_heap_last_pos[8];
static double trace_bve_heap_last_score[8];
static size_t trace_bve_heap_vars_size;
static FILE *reduce_trace_file;
static bool reduce_trace_initialized;
static uint64_t reduce_trace_from, reduce_trace_to = UINT64_MAX;
static FILE *congruence_binary_trace_file;
static bool congruence_binary_trace_initialized;
static uint64_t congruence_binary_trace_conflict = UINT64_MAX;
static FILE *congruence_candidate_trace_file;
static bool congruence_candidate_trace_initialized;
static uint64_t congruence_candidate_trace_conflict = UINT64_MAX;
static uint64_t congruence_candidate_trace_ordinal;
static uint64_t congruence_candidate_trace_ticks;
static FILE *binary_lifecycle_file;
static bool binary_lifecycle_initialized;
static uint64_t binary_lifecycle_sequence;
static int binary_lifecycle_pairs[64][2];
static size_t binary_lifecycle_pairs_size;
static FILE *binary_set_snapshot_file;
static bool binary_set_snapshot_initialized;
static uint64_t binary_set_snapshot_sequence;
static FILE *transitive_trace_file;
static bool transitive_trace_initialized;
static uint64_t transitive_trace_source_sequence;
static int transitive_trace_pairs[64][2];
static size_t transitive_trace_pairs_size;
static bool transitive_trace_all;
static int compare_external_literals (const void *, const void *);
static void append_sorted_clause (kissat *, clause *, char **, size_t *);
static void emit_heap_scores (kissat *);
static uint64_t eliminate_cleanup_clause_hash (kissat *, clause *);

static bool formula_checkpoint_selected (const char *spec,
                                         uint64_t conflict) {
  if (!spec || !*spec)
    return false;
  while (*spec) {
    char *end = 0;
    const uint64_t selected = strtoull (spec, &end, 10);
    if (end == spec)
      return false;
    if (selected == conflict)
      return true;
    spec = *end == ',' ? end + 1 : end;
    if (*end && *end != ',')
      return false;
  }
  return false;
}

static void formula_checkpoint (kissat *solver) {
  if (!formula_checkpoint_selected (
          getenv ("KISSAT_EVENT_TRACE_FORMULA_CONFLICTS"),
          solver->statistics.conflicts))
    return;
  uint64_t irredundant = 0, redundant = 0;
  uint64_t irredundant_hash = 0, redundant_hash = 0;
  ward *const arena = BEGIN_STACK (solver->arena);
  clause *c = (clause *) arena;
  const clause *const end = (clause *) END_STACK (solver->arena);
  while (c != end) {
    if (!c->garbage) {
      const uint64_t hash = eliminate_cleanup_clause_hash (solver, c);
      if (c->redundant) {
        redundant++;
        redundant_hash += hash;
      } else {
        irredundant++;
        irredundant_hash += hash;
      }
    }
    c = kissat_next_clause (c);
  }
  char payload[256];
  snprintf (payload, sizeof payload,
            "irredundant=%" PRIu64 "|redundant=%" PRIu64
            "|irredundant_hash=%016" PRIx64
            "|redundant_hash=%016" PRIx64,
            irredundant, redundant, irredundant_hash, redundant_hash);
  kissat_event_trace (solver, "formula_checkpoint", payload, true);
}

static void open_transitive_trace (void) {
  if (transitive_trace_initialized)
    return;
  transitive_trace_initialized = true;
  const char *path = getenv ("KISSAT_TRANSITIVE_TRACE");
  const char *spec = getenv ("KISSAT_TRANSITIVE_TRACE_PAIRS");
  if (!path || !*path || !spec || !*spec)
    return;
  const char *cursor = spec;
  while (*cursor && transitive_trace_pairs_size != 64) {
    char *end = 0;
    const long first = strtol (cursor, &end, 10);
    if (end == cursor || *end != ',')
      return;
    cursor = end + 1;
    const long second = strtol (cursor, &end, 10);
    if (end == cursor || (*end && *end != ';'))
      return;
    int a = (int) first, b = (int) second;
    if (b < a) {
      const int tmp = a;
      a = b;
      b = tmp;
    }
    transitive_trace_pairs[transitive_trace_pairs_size][0] = a;
    transitive_trace_pairs[transitive_trace_pairs_size++][1] = b;
    cursor = *end ? end + 1 : end;
  }
  transitive_trace_file = fopen (path, "w");
  if (!transitive_trace_file)
    return;
  setvbuf (transitive_trace_file, 0, _IOLBF, 0);
  transitive_trace_all = getenv ("KISSAT_TRANSITIVE_TRACE_ALL") != 0;
  fputs ("# tactix-transitive-trace\t2\n", transitive_trace_file);
  fputs ("# solver\tevent\tsource_sequence\tconflicts\tsource\t"
         "candidate_ordinal\tdestination\tticks_before\tticks_after\t"
         "redundant\tfailed\tcandidate_count\tout_degree\t"
         "source_internal\tdestination_internal\tsource_import\t"
         "destination_import\tsource_reference\tdestination_reference\t"
         "source_active\tdestination_active\tsource_value\t"
         "destination_value\tcopies\tskip_reason\n",
         transitive_trace_file);
}

static uint64_t stop_after_conflict (void) {
  const char *stop = getenv ("KISSAT_EVENT_TRACE_STOP_AFTER_CONFLICT");
  return stop && *stop ? strtoull (stop, 0, 10) : UINT64_MAX;
}

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

struct binary_set_pair {
  int a, b;
};

static int compare_binary_set_pairs (const void *p, const void *q) {
  const struct binary_set_pair *a = p, *b = q;
  if (a->a != b->a)
    return a->a < b->a ? -1 : 1;
  return a->b == b->b ? 0 : a->b < b->b ? -1 : 1;
}

static void emit_binary_set_snapshot (kissat *solver, const char *event,
                                      const char *payload) {
  const char *path = getenv ("SAT_EVENT_TRACE_BINARY_SET_PATH");
  const char *match = getenv ("SAT_EVENT_TRACE_BINARY_SET_MATCH");
  const char *conflict = getenv ("SAT_EVENT_TRACE_BINARY_SET_CONFLICT");
  if (!path || !*path || !match || !*match || !conflict || !*conflict)
    return;
  char *end = 0;
  const uint64_t selected = strtoull (conflict, &end, 10);
  if (end == conflict || *end || selected != solver->statistics.conflicts)
    return;
  if (!strstr (event, match) && (!payload || !strstr (payload, match)))
    return;
  if (!binary_set_snapshot_initialized) {
    binary_set_snapshot_initialized = true;
    binary_set_snapshot_file = fopen (path, "w");
    if (!binary_set_snapshot_file)
      return;
    fputs ("# kissat-binary-set-snapshot\t1\n", binary_set_snapshot_file);
    fputs ("# snapshot\ta\tb\n", binary_set_snapshot_file);
  }
  if (!binary_set_snapshot_file)
    return;
  size_t size = 0;
  for (all_literals (lit))
    for (all_binary_large_watches (watch, WATCHES (lit)))
      size += watch.type.binary && lit < watch.binary.lit;
  struct binary_set_pair *pairs = malloc (size * sizeof *pairs);
  if (!pairs && size)
    return;
  size_t used = 0;
  for (all_literals (lit))
    for (all_binary_large_watches (watch, WATCHES (lit))) {
      if (!watch.type.binary || lit >= watch.binary.lit)
        continue;
      int a = external_literal (solver, lit);
      int b = external_literal (solver, watch.binary.lit);
      if (b < a) {
        const int tmp = a;
        a = b;
        b = tmp;
      }
      pairs[used++] = (struct binary_set_pair) {a, b};
    }
  qsort (pairs, used, sizeof *pairs, compare_binary_set_pairs);
  const uint64_t snapshot = binary_set_snapshot_sequence++;
  fprintf (binary_set_snapshot_file,
           "# begin\t%" PRIu64 "\tkissat\t%s\t%" PRIu64 "\t%zu\t%s\n",
           snapshot, event, solver->statistics.conflicts, used,
           payload ? payload : "-");
  for (size_t i = 0; i != used; ++i)
    fprintf (binary_set_snapshot_file, "%" PRIu64 "\t%d\t%d\n", snapshot,
             pairs[i].a, pairs[i].b);
  fprintf (binary_set_snapshot_file, "# end\t%" PRIu64 "\n", snapshot);
  fflush (binary_set_snapshot_file);
  free (pairs);
}

bool kissat_transitive_trace_pair_selected (kissat *solver, unsigned first,
                                            unsigned second) {
  open_transitive_trace ();
  if (!transitive_trace_file)
    return false;
  int a = external_literal (solver, first);
  int b = external_literal (solver, second);
  if (b < a) {
    const int tmp = a;
    a = b;
    b = tmp;
  }
  size_t i = 0;
  while (i != transitive_trace_pairs_size &&
         (transitive_trace_pairs[i][0] != a ||
          transitive_trace_pairs[i][1] != b))
    i++;
  return i != transitive_trace_pairs_size;
}

bool kissat_transitive_trace_all_sources (void) {
  return transitive_trace_all;
}

static bool transitive_trace_endpoint_selected (int external) {
  for (size_t i = 0; i != transitive_trace_pairs_size; i++)
    if (external == transitive_trace_pairs[i][0] ||
        external == transitive_trace_pairs[i][1])
      return true;
  return false;
}

void kissat_transitive_trace_endpoints (kissat *solver) {
  open_transitive_trace ();
  if (!transitive_trace_file)
    return;
  for (unsigned lit = 0; lit != LITS; lit++) {
    const int external = external_literal (solver, lit);
    if (!transitive_trace_endpoint_selected (external))
      continue;
    fprintf (transitive_trace_file,
             "kissat\tendpoint\t0\t%" PRIu64
             "\t%d\t0\t0\t0\t0\t0\t0\t0\t0\t%u\t%u\t%u\t%u\t%u\t%u"
             "\t%d\t0\t%d\t0\t0\t-\n",
             solver->statistics.conflicts, external, lit, INVALID_LIT, lit,
             INVALID_LIT, lit, INVALID_LIT, ACTIVE (IDX (lit)),
             solver->values[lit]);
  }
}

void kissat_transitive_trace_admission (kissat *solver, unsigned src,
                                         unsigned dst, unsigned copies,
                                         const char *skip_reason) {
  if (!kissat_transitive_trace_pair_selected (solver, src, dst))
    return;
  fprintf (transitive_trace_file,
           "kissat\tadmission\t0\t%" PRIu64
           "\t%d\t0\t%d\t0\t0\t0\t0\t0\t0\t%u\t%u\t%u\t%u\t%u\t%u"
           "\t%d\t%d\t%d\t%d\t%u\t%s\n",
           solver->statistics.conflicts, external_literal (solver, src),
           external_literal (solver, dst), src, dst, src, dst, src, dst,
           ACTIVE (IDX (src)), ACTIVE (IDX (dst)), solver->values[src],
           solver->values[dst], copies, skip_reason);
}

void kissat_transitive_trace_walk_node (
    kissat *solver, unsigned src, unsigned dst, unsigned node, size_t ordinal,
    uint64_t ticks_before, uint64_t ticks_after, size_t watch_entries) {
  if (!kissat_transitive_trace_pair_selected (solver, src, dst))
    return;
  char node_payload[48];
  snprintf (node_payload, sizeof node_payload, "node=%d",
            external_literal (solver, node));
  fprintf (transitive_trace_file,
           "kissat\twalk-node\t0\t%" PRIu64
           "\t%d\t%zu\t%d\t%" PRIu64 "\t%" PRIu64
           "\t0\t0\t0\t0\t%u\t%u\t%u\t%u\t%u\t%u\t%d\t%d\t%d\t%d\t%zu\t%s\n",
           solver->statistics.conflicts, external_literal (solver, src),
           ordinal, external_literal (solver, dst), ticks_before, ticks_after,
           node, dst, node, dst, node, dst, ACTIVE (IDX (node)),
           ACTIVE (IDX (dst)), solver->values[node], solver->values[dst],
           watch_entries, node_payload);
}

void kissat_transitive_trace_walk_entries (kissat *solver, unsigned src,
                                            unsigned dst, unsigned node,
                                            unsigned watched) {
  if (!kissat_transitive_trace_pair_selected (solver, src, dst))
    return;
  const char *selected = getenv ("KISSAT_TRANSITIVE_TRACE_WATCH_LITERAL");
  if (!selected || atoi (selected) != external_literal (solver, watched))
    return;
  size_t ordinal = 0;
  const watch *p = BEGIN_WATCHES (WATCHES (watched));
  const watch *const end = END_WATCHES (WATCHES (watched));
  while (p != end) {
    char payload[4096];
    unsigned other = INVALID_LIT;
    uint64_t id = 0;
    const watch head = *p++;
    int written;
    if (head.type.binary) {
      other = head.binary.lit;
      written = snprintf (
          payload, sizeof payload,
          "node=%d;watch=%d;kind=binary;clause=%d,%d",
          external_literal (solver, node), external_literal (solver, watched),
          external_literal (solver, watched),
          external_literal (solver, other));
    } else {
      assert (p != end);
      const unsigned blocker = head.blocking.lit;
      const watch tail = *p++;
      id = tail.large.ref;
      clause *c = kissat_dereference_clause (solver, tail.large.ref);
      written = snprintf (payload, sizeof payload,
                          "node=%d;watch=%d;kind=large;blocker=%d;clause=",
                          external_literal (solver, node),
                          external_literal (solver, watched),
                          external_literal (solver, blocker));
      if (written > 0 && (size_t) written < sizeof payload) {
        char *p = payload + written;
        size_t left = sizeof payload - (size_t) written;
        append_sorted_clause (solver, c, &p, &left);
      }
    }
    fprintf (
        transitive_trace_file,
        "kissat\twalk-watch\t0\t%" PRIu64
        "\t%d\t%zu\t%d\t0\t0\t%d\t0\t0\t0\t%u\t%u\t%u\t%u\t%u\t%u"
        "\t%d\t%d\t%d\t%d\t%" PRIu64 "\t%s\n",
        solver->statistics.conflicts, external_literal (solver, watched),
        ordinal, other == INVALID_LIT ? 0 : external_literal (solver, other),
        head.type.binary ? 0 : 1, watched, other, watched, other, watched,
        other, ACTIVE (IDX (watched)),
        other != INVALID_LIT && ACTIVE (IDX (other)), solver->values[watched],
        other == INVALID_LIT ? 0 : solver->values[other], id, payload);
    ordinal++;
  }
}

static void transitive_trace_row (
    kissat *solver, const char *event, uint64_t sequence, unsigned source,
    size_t ordinal, unsigned destination, uint64_t ticks_before,
    uint64_t ticks_after, bool redundant, bool failed, size_t candidate_count,
    unsigned out_degree) {
  if (!transitive_trace_file || sequence == UINT64_MAX)
    return;
  const int external_destination =
      destination == INVALID_LIT ? 0 : external_literal (solver, destination);
  fprintf (transitive_trace_file,
           "kissat\t%s\t%" PRIu64 "\t%" PRIu64
           "\t%d\t%zu\t%d\t%" PRIu64 "\t%" PRIu64
           "\t%d\t%d\t%zu\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%d\t%d\t%d\t%d"
           "\t0\t-\n",
           event, sequence, solver->statistics.conflicts,
           external_literal (solver, source), ordinal, external_destination,
           ticks_before, ticks_after, redundant, failed, candidate_count,
           out_degree, source,
           destination == INVALID_LIT ? INVALID_LIT : destination, source,
           destination == INVALID_LIT ? INVALID_LIT : destination, source,
           destination == INVALID_LIT ? INVALID_LIT : destination,
           ACTIVE (IDX (source)),
           destination != INVALID_LIT && ACTIVE (IDX (destination)),
           solver->values[source],
           destination == INVALID_LIT ? 0 : solver->values[destination]);
}

uint64_t kissat_transitive_trace_source_begin (
    kissat *solver, bool selected, unsigned source, uint64_t ticks_before,
    size_t candidate_count, unsigned out_degree) {
  if (!selected || !transitive_trace_file)
    return UINT64_MAX;
  const uint64_t sequence = transitive_trace_source_sequence++;
  transitive_trace_row (solver, "source-begin", sequence, source, 0,
                        INVALID_LIT, ticks_before,
                        solver->statistics.transitive_ticks, false, false,
                        candidate_count, out_degree);
  return sequence;
}

void kissat_transitive_trace_candidate (
    kissat *solver, uint64_t sequence, unsigned source, size_t ordinal,
    unsigned destination, uint64_t ticks_before, uint64_t ticks_after,
    bool redundant, bool failed, size_t candidate_count,
    unsigned out_degree) {
  transitive_trace_row (solver, "candidate", sequence, source, ordinal,
                        destination, ticks_before, ticks_after, redundant,
                        failed, candidate_count, out_degree);
}

void kissat_transitive_trace_source_end (
    kissat *solver, uint64_t sequence, unsigned source, size_t ordinal,
    uint64_t ticks_before, uint64_t ticks_after, bool changed,
    size_t candidate_count, unsigned out_degree) {
  transitive_trace_row (solver, "source-end", sequence, source, ordinal,
                        INVALID_LIT, ticks_before, ticks_after, changed, false,
                        candidate_count, out_degree);
}

static void binary_lifecycle_event_with_clause (kissat *solver,
                                                const char *event,
                                                unsigned first,
                                                unsigned second,
                                                clause *source) {
  if (!binary_lifecycle_initialized) {
    binary_lifecycle_initialized = true;
    const char *path = getenv ("KISSAT_BINARY_LIFECYCLE_TRACE");
    const char *spec = getenv ("KISSAT_BINARY_LIFECYCLE_PAIRS");
    if (path && *path && spec && *spec) {
      const char *cursor = spec;
      while (*cursor && binary_lifecycle_pairs_size != 64) {
        char *end = 0;
        const long first_external = strtol (cursor, &end, 10);
        if (end == cursor || *end != ',')
          return;
        cursor = end + 1;
        const long second_external = strtol (cursor, &end, 10);
        if (end == cursor || (*end && *end != ';'))
          return;
        int a = (int) first_external, b = (int) second_external;
        if (b < a) {
          const int tmp = a;
          a = b;
          b = tmp;
        }
        binary_lifecycle_pairs[binary_lifecycle_pairs_size][0] = a;
        binary_lifecycle_pairs[binary_lifecycle_pairs_size++][1] = b;
        cursor = *end ? end + 1 : end;
      }
      binary_lifecycle_file = fopen (path, "w");
      if (binary_lifecycle_file) {
        setvbuf (binary_lifecycle_file, 0, _IOLBF, 0);
        fputs ("# tactix-binary-lifecycle\t3\n", binary_lifecycle_file);
        fputs ("# solver\tseq\tevent\tconflicts\tsearch_ticks\t"
               "probing_ticks\ta\tb\tvalue_a\tvalue_b\tid\tcopies\t"
               "payload\n",
               binary_lifecycle_file);
      }
    }
  }
  if (!binary_lifecycle_file)
    return;
  int a = external_literal (solver, first);
  int b = external_literal (solver, second);
  if (b < a) {
    const int tmp = a;
    a = b;
    b = tmp;
    const unsigned tmp_lit = first;
    first = second;
    second = tmp_lit;
  }
  size_t selected = 0;
  while (selected != binary_lifecycle_pairs_size &&
         (binary_lifecycle_pairs[selected][0] != a ||
          binary_lifecycle_pairs[selected][1] != b))
    selected++;
  if (selected == binary_lifecycle_pairs_size)
    return;
  fprintf (binary_lifecycle_file,
           "kissat\t%" PRIu64 "\t%s\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64
           "\t%d\t%d\t%d\t%d\t0\t0\t",
           binary_lifecycle_sequence++, event, solver->statistics.conflicts,
           solver->statistics.search_ticks, solver->statistics.probing_ticks,
           a, b, (int) solver->values[first], (int) solver->values[second]);
  if (!source)
    fputc ('-', binary_lifecycle_file);
  else {
    fprintf (binary_lifecycle_file, "source=");
    bool first_lit = true;
    for (all_literals_in_clause (lit, source)) {
      fprintf (binary_lifecycle_file, "%s%d", first_lit ? "" : ",",
               external_literal (solver, lit));
      first_lit = false;
    }
  }
  fputc ('\n', binary_lifecycle_file);
}

static void binary_lifecycle_event (kissat *solver, const char *event,
                                    unsigned first, unsigned second) {
  binary_lifecycle_event_with_clause (solver, event, first, second, 0);
}

static bool congruence_binary_trace_enabled (kissat *solver) {
  if (!congruence_binary_trace_initialized) {
    congruence_binary_trace_initialized = true;
    const char *path = getenv ("KISSAT_CONGRUENCE_BINARY_TRACE");
    const char *conflict =
        getenv ("KISSAT_CONGRUENCE_BINARY_TRACE_CONFLICT");
    if (path && *path && conflict && *conflict) {
      congruence_binary_trace_conflict = strtoull (conflict, 0, 10);
      congruence_binary_trace_file = fopen (path, "w");
      if (congruence_binary_trace_file) {
        setvbuf (congruence_binary_trace_file, 0, _IOLBF, 0);
        fputs ("# tactix-congruence-binaries\t1\n",
               congruence_binary_trace_file);
        fputs ("# solver\tkind\tstage\tconflicts\tordinal\ta\tb\t"
               "internal_a\tinternal_b\tticks\n",
               congruence_binary_trace_file);
      }
    }
  }
  return congruence_binary_trace_file &&
         solver->statistics.conflicts == congruence_binary_trace_conflict;
}

void kissat_congruence_binary_trace_begin (kissat *solver, const char *stage,
                                           size_t count) {
  if (!congruence_binary_trace_enabled (solver))
    return;
  fprintf (congruence_binary_trace_file,
           "kissat\tboundary\t%s\t%" PRIu64 "\t%zu\t-\t-\t-\t-\t%" PRIu64
           "\n",
           stage, solver->statistics.conflicts, count,
           solver->statistics.probing_ticks);
}

void kissat_congruence_binary_trace_pair (kissat *solver, const char *stage,
                                          size_t ordinal, unsigned first,
                                          unsigned second) {
  if (!congruence_binary_trace_enabled (solver))
    return;
  int a = external_literal (solver, first);
  int b = external_literal (solver, second);
  if (b < a) {
    const int tmp = a;
    a = b;
    b = tmp;
  }
  fprintf (congruence_binary_trace_file,
           "kissat\tpair\t%s\t%" PRIu64 "\t%zu\t%d\t%d\t%u\t%u\t%" PRIu64
           "\n",
           stage, solver->statistics.conflicts, ordinal, a, b, first, second,
           solver->statistics.probing_ticks);
}

void kissat_congruence_sparse_binary_trace (kissat *solver,
                                             const char *stage) {
  if (!congruence_binary_trace_enabled (solver))
    return;
  size_t count = 0;
  for (all_literals (lit))
    for (all_binary_large_watches (watch, WATCHES (lit)))
      count += watch.type.binary && lit < watch.binary.lit;
  kissat_congruence_binary_trace_begin (solver, stage, count);
  size_t ordinal = 0;
  for (all_literals (lit))
    for (all_binary_large_watches (watch, WATCHES (lit)))
      if (watch.type.binary && lit < watch.binary.lit)
        kissat_congruence_binary_trace_pair (
            solver, stage, ordinal++, lit, watch.binary.lit);
}

void kissat_congruence_binary_trace_end (kissat *solver, const char *stage) {
  if (!congruence_binary_trace_enabled (solver))
    return;
  fflush (congruence_binary_trace_file);
  if (!strcmp (stage, "exit") &&
      getenv ("KISSAT_CONGRUENCE_BINARY_TRACE_STOP"))
    raise (SIGTERM);
}

static bool congruence_candidate_trace_enabled (kissat *solver) {
  if (!congruence_candidate_trace_initialized) {
    congruence_candidate_trace_initialized = true;
    const char *path = getenv ("KISSAT_CONGRUENCE_CANDIDATE_TRACE");
    const char *conflict =
        getenv ("KISSAT_CONGRUENCE_CANDIDATE_TRACE_CONFLICT");
    if (path && *path && conflict && *conflict) {
      congruence_candidate_trace_conflict = strtoull (conflict, 0, 10);
      congruence_candidate_trace_file = fopen (path, "w");
      if (congruence_candidate_trace_file) {
        setvbuf (congruence_candidate_trace_file, 0, _IOLBF, 0);
        fputs ("# tactix-congruence-candidates\t1\n",
               congruence_candidate_trace_file);
        fputs ("# solver\tkind\tconflicts\tordinal\tpose_a\tpose_b\t"
               "pose_c\tkey_a\tkey_b\tkey_c\tx\ty\tentry\texit\t"
               "delta\tdisposition\n",
               congruence_candidate_trace_file);
      }
    }
  }
  return congruence_candidate_trace_file &&
         solver->statistics.conflicts == congruence_candidate_trace_conflict;
}

void kissat_congruence_candidate_trace_begin (kissat *solver) {
  if (!congruence_candidate_trace_enabled (solver))
    return;
  congruence_candidate_trace_ordinal = 0;
  congruence_candidate_trace_ticks = 0;
  fprintf (congruence_candidate_trace_file,
           "kissat\tbegin\t%" PRIu64 "\t-\t-\t-\t-\t-\t-\t-\t-\t-"
           "\t0\t0\t0\t-\n",
           solver->statistics.conflicts);
}

void kissat_congruence_candidate_trace (kissat *solver,
                                         const unsigned *lits, unsigned x,
                                         unsigned y,
                                         const char *disposition) {
  if (!congruence_candidate_trace_enabled (solver))
    return;
  int pose[3] = {external_literal (solver, lits[0]),
                 external_literal (solver, lits[1]),
                 external_literal (solver, lits[2])};
  int key[3] = {pose[0], pose[1], pose[2]};
  qsort (key, 3, sizeof *key, compare_external_literals);
  const uint64_t entry = congruence_candidate_trace_ticks;
  congruence_candidate_trace_ticks += 3;
  fprintf (congruence_candidate_trace_file,
           "kissat\tcandidate\t%" PRIu64 "\t%" PRIu64
           "\t%d\t%d\t%d\t%d\t%d\t%d\t",
           solver->statistics.conflicts,
           congruence_candidate_trace_ordinal++, pose[0], pose[1], pose[2],
           key[0], key[1], key[2]);
  if (x == INVALID_LIT)
    fputs ("-\t-\t", congruence_candidate_trace_file);
  else
    fprintf (congruence_candidate_trace_file, "%d\t%d\t",
             external_literal (solver, x), external_literal (solver, y));
  fprintf (congruence_candidate_trace_file,
           "%" PRIu64 "\t%" PRIu64 "\t3\t%s\n", entry,
           congruence_candidate_trace_ticks, disposition);
}

void kissat_congruence_candidate_trace_end (kissat *solver, bool complete,
                                             size_t extracted) {
  if (!congruence_candidate_trace_enabled (solver))
    return;
  fprintf (congruence_candidate_trace_file,
           "kissat\tend\t%" PRIu64 "\t%" PRIu64
           "\t-\t-\t-\t-\t-\t-\t-\t-\t0\t%" PRIu64
           "\t%" PRIu64 "\tcomplete=%u;extracted=%zu;production_ticks=%" PRIu64
           "\n",
           solver->statistics.conflicts, congruence_candidate_trace_ordinal,
           congruence_candidate_trace_ticks, congruence_candidate_trace_ticks,
           complete ? 1u : 0u, extracted, solver->statistics.probing_ticks);
  fflush (congruence_candidate_trace_file);
  if (getenv ("KISSAT_CONGRUENCE_CANDIDATE_TRACE_STOP"))
    raise (SIGTERM);
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
        written = snprintf (
            payload + used, capacity - used,
            "%ssearched=%u,redundant=%u,glue=%u,used=%u,priority=%u,pose=",
            matches ? ";" : "", c->searched, c->redundant, c->glue,
            c->used, c->vivify);
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
  trace_heap_each_bump =
      getenv ("KISSAT_EVENT_TRACE_HEAP_EACH_BUMP") != 0;
  const char *heap_from_decision =
      getenv ("KISSAT_EVENT_TRACE_HEAP_FROM_DECISION");
  if (heap_from_decision && *heap_from_decision)
    trace_heap_from_decision = strtoull (heap_from_decision, 0, 10);
  const char *heap_pop_decision =
      getenv ("KISSAT_EVENT_TRACE_HEAP_POP_DECISION");
  if (heap_pop_decision && *heap_pop_decision)
    trace_heap_pop_decision = strtoull (heap_pop_decision, 0, 10);
  const char *heap_remove_decision =
      getenv ("KISSAT_EVENT_TRACE_HEAP_REMOVE_DECISION");
  if (heap_remove_decision && *heap_remove_decision)
    trace_heap_remove_decision = strtoull (heap_remove_decision, 0, 10);
  trace_heap_pop_only = getenv ("KISSAT_EVENT_TRACE_HEAP_POP_ONLY") != 0;
  trace_skip_decisions = getenv ("KISSAT_EVENT_TRACE_SKIP_DECISIONS") != 0;
  trace_skip_randec_draws =
      getenv ("KISSAT_EVENT_TRACE_SKIP_RANDEC_DRAWS") != 0;
  trace_skip_analysis_ticks =
      getenv ("KISSAT_EVENT_TRACE_SKIP_ANALYSIS_TICKS") != 0;
  trace_keep_snapshots =
      getenv ("KISSAT_EVENT_TRACE_KEEP_SNAPSHOTS") != 0;
  trace_search_only = getenv ("KISSAT_EVENT_TRACE_SEARCH_ONLY") != 0;
  trace_control_only = getenv ("KISSAT_EVENT_TRACE_CONTROL_ONLY") != 0;
  trace_glue_only = getenv ("KISSAT_EVENT_TRACE_GLUE_ONLY") != 0;
  trace_phase_only = getenv ("KISSAT_EVENT_TRACE_PHASE_ONLY") != 0;
  trace_trajectory_only =
      getenv ("KISSAT_EVENT_TRACE_TRAJECTORY_ONLY") != 0;
  trace_sweep_only = getenv ("KISSAT_EVENT_TRACE_SWEEP_ONLY") != 0;
  trace_compact_probe = getenv ("KISSAT_EVENT_TRACE_COMPACT_PROBE") != 0;
  trace_watch_only = getenv ("KISSAT_EVENT_TRACE_WATCH_ONLY") != 0;
  trace_watch_cost = getenv ("KISSAT_EVENT_TRACE_WATCH_COST") != 0;
  trace_watch_within_vivify =
      getenv ("KISSAT_EVENT_TRACE_WATCH_WITHIN_VIVIFY") != 0;
  trace_file = fopen (path, "w");
  if (!trace_file)
    return;
  setvbuf (trace_file, 0, _IOLBF, 0);
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

static bool selected_phase_variable (kissat *solver, unsigned idx) {
  const char *spec = getenv ("SAT_EVENT_TRACE_PHASE_VAR");
  if (!spec || !*spec)
    return true;
  const int selected = atoi (spec);
  const int external = external_literal (solver, LIT (idx));
  return abs (selected) == abs (external);
}

void kissat_event_trace_phase_pick (kissat *solver, unsigned idx,
                                    int result) {
  if (!detail_enabled (solver) || !selected_phase_variable (solver, idx))
    return;
  const int base = external_literal (solver, LIT (idx));
  const int sign = base < 0 ? -1 : 1;
  char payload[192];
  snprintf (payload, sizeof payload,
            "external=%d|target=%d|saved=%d|result=%d", abs (base),
            sign * solver->phases.target[idx],
            sign * solver->phases.saved[idx], sign * result);
  kissat_event_trace (solver, "phase_pick", payload, false);
}

void kissat_event_trace_phase_save (kissat *solver, unsigned lit) {
  const unsigned idx = IDX (lit);
  if (!detail_enabled (solver) || !selected_phase_variable (solver, idx))
    return;
  char payload[64];
  snprintf (payload, sizeof payload, "lit=%d",
            external_literal (solver, lit));
  kissat_event_trace (solver, "phase_save", payload, false);
}

void kissat_event_trace_target_state (kissat *solver, const char *stage,
                                      unsigned assigned) {
  open_trace ();
  if (!trace_phase_only)
    return;
  int watched = 0;
  const char *spec = getenv ("SAT_EVENT_TRACE_PHASE_VAR");
  if (spec && *spec)
    watched = atoi (spec);
  int target = 0, saved = 0, value = 0, active = 0;
  if (watched > 0 && (size_t) watched < SIZE_STACK (solver->import)) {
    const import *const imported = &PEEK_STACK (solver->import, watched);
    if (imported->imported && !imported->eliminated) {
      const unsigned lit = imported->lit;
      const unsigned idx = IDX (lit);
      const int sign = NEGATED (lit) ? -1 : 1;
      target = sign * solver->phases.target[idx];
      saved = sign * solver->phases.saved[idx];
      value = sign * solver->values[LIT (idx)];
      active = ACTIVE (idx);
    }
  }
  char payload[512];
  snprintf (payload, sizeof payload,
            "stage=%s|assigned=%u|target_assigned=%u|best_assigned=%u"
            "|compact_vars=%u|physical_vars=%u|watch=%d|watch_target=%d"
            "|watch_saved=%d|watch_value=%d|watch_active=%d",
            stage, assigned, solver->target_assigned, solver->best_assigned,
            solver->vars, solver->vars, watched, target, saved, value, active);
  kissat_event_trace (solver, "target_state", payload, false);
}

void kissat_event_trace (kissat *solver, const char *event,
                         const char *payload, bool snapshot) {
  if (snapshot)
    emit_binary_set_snapshot (solver, event, payload);
  open_trace ();
  if (!trace_file)
    return;
  if (trace_heap_pop_only && strcmp (event, "heap_pop") &&
      strcmp (event, "heap_remove") &&
      strcmp (event, "heap_scores") && strcmp (event, "decision") &&
      strcmp (event, "analysis_end"))
    return;
  const uint64_t stop = stop_after_conflict ();
  if (snapshot && !trace_keep_snapshots && stop != UINT64_MAX &&
      solver->statistics.conflicts < stop &&
      strcmp (event, "analysis_end"))
    return;
  if (trace_glue_only && strcmp (event, "reason_glue") &&
      strcmp (event, "reason_used") &&
      strcmp (event, "retier") &&
      strcmp (event, "analysis_end"))
    return;
  if (trace_phase_only && strcmp (event, "trace_begin") &&
      strcmp (event, "target_state") && strcmp (event, "phase_pick") &&
      strcmp (event, "phase_save") && strcmp (event, "formula_checkpoint"))
    return;
  if (trace_skip_watch && !strncmp (event, "watch", 5))
    return;
  if (trace_skip_imply && !strcmp (event, "imply"))
    return;
  if (trace_skip_heap && !strncmp (event, "heap_", 5) &&
      strcmp (event, "heap_reinsert"))
    return;
  if (trace_skip_decisions &&
      (!strcmp (event, "decision") || !strcmp (event, "randec_draw") ||
       !strcmp (event, "randec_state") || !strcmp (event, "phase_pick")))
    return;
  if (trace_skip_randec_draws && !strcmp (event, "randec_draw"))
    return;
  if (trace_skip_analysis_ticks && !strncmp (event, "analysis_tick", 13))
    return;
  if (trace_watch_only && !snapshot && strcmp (event, "watch") &&
      strcmp (event, "watch_post") &&
      strcmp (event, "watch_post_summary") &&
      strcmp (event, "watch_cost") &&
      strcmp (event, "reason_glue") &&
      strcmp (event, "vivify_candidate") &&
      strcmp (event, "vivify_candidate_end") &&
      strcmp (event, "vivify_budget_stop"))
    return;
  if (trace_compact_probe && strcmp (event, "trace_begin") &&
      strcmp (event, "probe_begin") && strcmp (event, "probe_end") &&
      strcmp (event, "preprocess_begin") &&
      strcmp (event, "preprocess_end") &&
      strcmp (event, "slot_boundary") &&
      strncmp (event, "factor_", 7) &&
      strncmp (event, "bve_", 4) &&
      strncmp (event, "eliminate_", 10) &&
      strncmp (event, "substitute_", 11))
    return;
  if (trace_search_only && !snapshot && strcmp (event, "decision") &&
      strcmp (event, "randec_draw") && strcmp (event, "randec_state") &&
      strcmp (event, "phase_pick") &&
      strcmp (event, "score_bump") &&
      strcmp (event, "heap_insert") &&
      strcmp (event, "heap_remove") &&
      strcmp (event, "heap_reinsert") &&
      strcmp (event, "assignment") &&
      strcmp (event, "propagate_cursor") &&
      strcmp (event, "heap_scores") &&
      strcmp (event, "binary_assignment") &&
      strcmp (event, "binary_add") &&
      strcmp (event, "sweep_seed") &&
      strcmp (event, "sweep_environment") &&
      strcmp (event, "sweep_environment_clause") &&
      strcmp (event, "sweep_query") &&
      strcmp (event, "sweep_query_model") &&
      strcmp (event, "sweep_equivalence") &&
      strcmp (event, "sweep_rewrite_binary") &&
      strcmp (event, "watch") && strcmp (event, "watch_post") &&
      strcmp (event, "watch_post_summary") && strcmp (event, "imply") &&
      strcmp (event, "conflict") &&
      strcmp (event, "analyzed") && strncmp (event, "analysis_", 9) &&
      strncmp (event, "reason_", 7) &&
      strncmp (event, "learned", 7))
    return;
  if (trace_control_only && !snapshot && strcmp (event, "decision") &&
      strcmp (event, "randec_draw") && strcmp (event, "randec_state") &&
      strcmp (event, "score_bump") &&
      strcmp (event, "heap_scores") &&
      strcmp (event, "assignment") &&
      strcmp (event, "heap_reinsert") &&
      strcmp (event, "watch") && strcmp (event, "watch_post") &&
      strcmp (event, "watch_post_summary") &&
      strcmp (event, "conflict") &&
      strcmp (event, "analyzed") && strncmp (event, "analysis_", 9) &&
      strncmp (event, "learned", 7) && strncmp (event, "factor_", 7) &&
      strncmp (event, "vivify_", 7) &&
      strncmp (event, "reason_", 7))
    return;
  // Minimal portable search trajectory.  Keep macro/snapshot boundaries and
  // only the choices plus learned-clause transaction needed to locate the
  // first semantic divergence before enabling the high-volume internals.
  if (trace_sweep_only && !snapshot && strcmp (event, "sweep_seed") &&
      strcmp (event, "sweep_environment") &&
      strcmp (event, "sweep_environment_clause") &&
      strcmp (event, "sweep_environment_var") &&
      strcmp (event, "sweep_binary_attempt") &&
      strcmp (event, "sweep_effort") &&
      strcmp (event, "sweep_query") &&
      strcmp (event, "sweep_query_model") &&
      strcmp (event, "sweep_equivalence") &&
      strcmp (event, "sweep_rewrite_binary") &&
      strcmp (event, "sweep_rewrite_occurrence") &&
      strcmp (event, "sweep_unit"))
    return;
  if (trace_trajectory_only && !snapshot &&
      !(trace_sweep_only && !strncmp (event, "sweep_", 6)) &&
      strcmp (event, "decision") &&
      strcmp (event, "randec_draw") && strcmp (event, "randec_state") &&
      strcmp (event, "phase_pick") && strcmp (event, "conflict") &&
      strcmp (event, "analysis_begin") &&
      strcmp (event, "analysis_end") && strncmp (event, "learned", 7))
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
    if (getenv ("KISSAT_EVENT_TRACE_FORMULA_MACRO")) {
      uint64_t formula_irredundant = 0, formula_redundant = 0;
      uint64_t formula_irredundant_hash = 0, formula_redundant_hash = 0;
      ward *const arena = BEGIN_STACK (solver->arena);
      clause *c = (clause *) arena;
      const clause *const end = (clause *) END_STACK (solver->arena);
      while (c != end) {
        if (!c->garbage) {
          const uint64_t hash = eliminate_cleanup_clause_hash (solver, c);
          if (c->redundant) {
            formula_redundant++;
            formula_redundant_hash += hash;
          } else {
            formula_irredundant++;
            formula_irredundant_hash += hash;
          }
        }
        c = kissat_next_clause (c);
      }
      const size_t used = strlen (annotated_payload);
      snprintf (annotated_payload + used, sizeof annotated_payload - used,
                "|formula_irredundant=%" PRIu64
                "|formula_redundant=%" PRIu64
                "|formula_irredundant_hash=%016" PRIx64
                "|formula_redundant_hash=%016" PRIx64,
                formula_irredundant, formula_redundant,
                formula_irredundant_hash, formula_redundant_hash);
    }
    const char *selected = getenv ("SAT_EVENT_TRACE_WATCH_LIST");
    if (selected && *selected) {
      const int wanted = atoi (selected);
      int target_a = 0, target_b = 0;
      const char *targets = getenv ("SAT_EVENT_TRACE_WATCH_TARGETS");
      const bool compact_targets =
          targets && sscanf (targets, "%d,%d", &target_a, &target_b) == 2;
      unsigned watched = INVALID_LIT;
      for (all_literals (lit))
        if (external_literal (solver, lit) == wanted) {
          watched = lit;
          break;
        }
      size_t used = strlen (annotated_payload);
      int written = snprintf (annotated_payload + used,
                              sizeof annotated_payload - used,
                              "|watch_lit=%d", wanted);
      if (written > 0)
        used += (size_t) written;
      if (compact_targets) {
        uint64_t hash = UINT64_C (1469598103934665603);
        size_t count = 0, pos_a = 0, pos_b = 0;
        int lit_a = 0, lit_b = 0;
        if (watched != INVALID_LIT) {
          watches entries = WATCHES (watched);
          const watch *p = BEGIN_WATCHES (entries);
          const watch *const end = END_WATCHES (entries);
          while (p != end) {
            const watch head = *p++;
            if (!head.type.binary) {
              p++;
              continue;
            }
            const int other = external_literal (solver, head.binary.lit);
            hash = mix (hash, (uint64_t) (int64_t) other);
            count++;
            if (!pos_a && abs (other) == abs (target_a))
              pos_a = count, lit_a = other;
            if (!pos_b && abs (other) == abs (target_b))
              pos_b = count, lit_b = other;
          }
        }
        snprintf (annotated_payload + used, sizeof annotated_payload - used,
                  "|binary_count=%zu|binary_hash=%016" PRIx64
                  "|target_a=%d|target_a_pos=%zu|target_a_lit=%d"
                  "|target_b=%d|target_b_pos=%zu|target_b_lit=%d",
                  count, hash, target_a, pos_a, lit_a, target_b, pos_b, lit_b);
      } else {
        written = snprintf (annotated_payload + used,
                            sizeof annotated_payload - used, "|binary=");
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
  open_trace ();
  if (!trace_file)
    return;
  // Match Tactix's diagnostic post-analysis ranking snapshot.  This localizes
  // a latent bump-order difference at the conflict that creates it rather
  // than at the later decision that exposes it.
  const bool analysis_end = !strcmp (event, "analysis_end");
  const bool snapshot = !trace_trajectory_only && analysis_end;
  if (analysis_end)
    formula_checkpoint (solver);
  const uint64_t stop = stop_after_conflict ();
  const bool stop_here = analysis_end && stop != UINT64_MAX &&
                         solver->statistics.conflicts >= stop;
  // A conflict counter may skip the exact end of a narrow detail window (for
  // example, a root-level unit can advance the counter without emitting the
  // ordinary analysis tail).  The stop contract still owns the first later
  // analysis_end, exactly as in Tactix; otherwise the observer runs until its
  // timeout while producing an irrelevant post-window trace.
  if (!stop_here && trace_detail_window &&
      (solver->statistics.conflicts < trace_detail_from ||
       solver->statistics.conflicts > trace_detail_to))
    return;
  char buffer[32];
  snprintf (buffer, sizeof buffer, "%d", payload);
  if (stop_here && solver->stable)
    emit_heap_scores (solver);
  kissat_event_trace (solver, event, buffer, snapshot);
  if (stop_here) {
    fflush (trace_file);
    raise (SIGTERM);
  }
}

void kissat_event_trace_bve_candidate (kissat *solver, unsigned idx,
                                       const char *stage, uint64_t ticks,
                                       uint64_t limit, bool applied) {
  open_trace ();
  if (!trace_file || !detail_enabled (solver))
    return;
  char payload[192];
  snprintf (payload, sizeof payload,
            "external=%u|stage=%s|pivot_value=%d|ticks=%" PRIu64
            "|limit=%" PRIu64 "|applied=%u",
            external_variable (solver, idx), stage, solver->values[LIT (idx)],
            ticks, limit, applied);
  kissat_event_trace (solver, "bve_candidate", payload, false);
}

static uint64_t eliminate_cleanup_clause_hash (kissat *solver, clause *c) {
  int *external = malloc (c->size * sizeof *external);
  unsigned size = 0;
  for (all_literals_in_clause (lit, c))
    external[size++] = external_literal (solver, lit);
  qsort (external, size, sizeof *external, compare_external_literals);
  uint64_t hash = UINT64_C (1469598103934665603);
  for (unsigned i = 0; i != size; i++) {
    hash ^= (uint32_t) external[i];
    hash *= UINT64_C (1099511628211);
  }
  free (external);
  return hash;
}

void kissat_event_trace_eliminate_cleanup (kissat *solver,
                                           const char *stage) {
  open_trace ();
  if (!trace_file || !detail_enabled (solver))
    return;
  uint64_t live_irredundant = 0, live_redundant = 0;
  uint64_t root_irredundant = 0, root_redundant = 0;
  uint64_t retired_irredundant = 0, retired_redundant = 0;
  uint64_t both_irredundant = 0, both_redundant = 0;
  uint64_t root_hash = 0, retired_hash = 0, collect_hash = 0;
  uint64_t live_redundant_hash = 0;
  int *retired_external = malloc (solver->vars * sizeof *retired_external);
  size_t retired_variables = 0;
  for (all_variables (idx))
    if (solver->flags[idx].eliminated)
      retired_external[retired_variables++] = external_variable (solver, idx);
  qsort (retired_external, retired_variables, sizeof *retired_external,
         compare_external_literals);
  uint64_t retired_variables_hash = UINT64_C (1469598103934665603);
  for (size_t i = 0; i != retired_variables; i++) {
    retired_variables_hash ^= retired_external[i];
    retired_variables_hash *= UINT64_C (1099511628211);
  }
  free (retired_external);
  ward *const arena = BEGIN_STACK (solver->arena);
  clause *c = (clause *) arena;
  const clause *const end = (clause *) END_STACK (solver->arena);
  while (c != end) {
    if (!c->garbage) {
      const bool redundant = c->redundant;
      if (redundant)
        live_redundant++;
      else
        live_irredundant++;
      const uint64_t clause_hash =
          redundant ? eliminate_cleanup_clause_hash (solver, c) : 0;
      live_redundant_hash += clause_hash;
      bool root = false, retired = false;
      for (all_literals_in_clause (lit, c)) {
        root |= solver->values[lit] > 0;
        retired |= solver->flags[IDX (lit)].eliminated;
      }
      if (root || retired) {
        const uint64_t hash =
            clause_hash ? clause_hash : eliminate_cleanup_clause_hash (solver, c);
        collect_hash += hash;
        if (root) {
          root_hash += hash;
          if (redundant)
            root_redundant++;
          else
            root_irredundant++;
        }
        if (retired) {
          retired_hash += hash;
          if (redundant)
            retired_redundant++;
          else
            retired_irredundant++;
        }
        if (root && retired) {
          if (redundant)
            both_redundant++;
          else
            both_irredundant++;
        }
      }
    }
    c = kissat_next_clause (c);
  }
  char payload[640];
  snprintf (payload, sizeof payload,
            "stage=%s|live_irredundant=%" PRIu64
            "|live_redundant=%" PRIu64 "|root_irredundant=%" PRIu64
            "|root_redundant=%" PRIu64 "|retired_irredundant=%" PRIu64
            "|retired_redundant=%" PRIu64 "|both_irredundant=%" PRIu64
            "|both_redundant=%" PRIu64 "|root_hash=%016" PRIx64
            "|retired_hash=%016" PRIx64 "|collect_hash=%016" PRIx64
            "|live_redundant_hash=%016" PRIx64
            "|retired_variables=%zu|retired_variables_hash=%016" PRIx64,
            stage, live_irredundant, live_redundant, root_irredundant,
            root_redundant, retired_irredundant, retired_redundant,
            both_irredundant, both_redundant, root_hash, retired_hash,
            collect_hash, live_redundant_hash, retired_variables,
            retired_variables_hash);
  kissat_event_trace (solver, "eliminate_cleanup", payload, false);

  const char *clauses = getenv ("KISSAT_EVENT_TRACE_ELIMINATE_CLAUSES");
  if (strcmp (stage, "before") || !clauses || strcmp (clauses, "1"))
    return;
  c = (clause *) arena;
  while (c != end) {
    if (!c->garbage && c->redundant) {
      bool root = false, retired = false;
      for (all_literals_in_clause (lit, c)) {
        root |= solver->values[lit] > 0;
        retired |= solver->flags[IDX (lit)].eliminated;
      }
      char clause_payload[192];
      snprintf (clause_payload, sizeof clause_payload,
                "hash=%016" PRIx64 "|size=%u|root=%u|retired=%u",
                eliminate_cleanup_clause_hash (solver, c), c->size, root,
                retired);
      kissat_event_trace (solver, "eliminate_cleanup_clause", clause_payload,
                          false);
    }
    c = kissat_next_clause (c);
  }
}

void kissat_event_trace_bve_plan (kissat *solver, unsigned idx,
                                  unsigned side0, unsigned side1, bool gate,
                                  size_t gate0, size_t gate1, uint64_t charged,
                                  bool resolve_gate) {
  open_trace ();
  if (!trace_file || !detail_enabled (solver))
    return;
  char payload[224];
  snprintf (payload, sizeof payload,
            "external=%u|side0=%u|side1=%u|gate=%u|gate0=%zu|gate1=%zu|"
            "charged=%" PRIu64 "|resolve_gate=%u",
            external_variable (solver, idx), side0, side1, gate, gate0,
            gate1, charged, resolve_gate);
  kissat_event_trace (solver, "bve_plan", payload, false);
}

static int compare_bve_external_literals (const void *a, const void *b) {
  const int x = *(const int *) a, y = *(const int *) b;
  return (x > y) - (x < y);
}

static uint64_t bve_clause_hash (kissat *solver, clause *c,
                                 unsigned *active_size) {
  int external[101];
  unsigned size = 0;
  for (all_literals_in_clause (lit, c))
    if (solver->values[lit] >= 0)
      external[size++] = external_literal (solver, lit);
  qsort (external, size, sizeof *external, compare_bve_external_literals);
  uint64_t hash = UINT64_C (1469598103934665603);
  for (unsigned i = 0; i < size; i++) {
    hash ^= (uint32_t) external[i];
    hash *= UINT64_C (1099511628211);
  }
  *active_size = size;
  return hash;
}

void kissat_event_trace_bve_pair (kissat *solver, unsigned idx,
                                  uint32_t ordinal, clause *side0,
                                  clause *side1, const char *outcome) {
  const char *selected = getenv ("KISSAT_EVENT_TRACE_BVE_EXTERNAL");
  if (!selected || atoi (selected) != (int) external_variable (solver, idx))
    return;
  open_trace ();
  if (!trace_file || !detail_enabled (solver))
    return;
  unsigned size0, size1;
  const uint64_t hash0 = bve_clause_hash (solver, side0, &size0);
  const uint64_t hash1 = bve_clause_hash (solver, side1, &size1);
  char ref0[32], ref1[32];
  if (side0->size > 2)
    snprintf (ref0, sizeof ref0, "%u",
              kissat_reference_clause (solver, side0));
  else
    strcpy (ref0, "-");
  if (side1->size > 2)
    snprintf (ref1, sizeof ref1, "%u",
              kissat_reference_clause (solver, side1));
  else
    strcpy (ref1, "-");
  char payload[512];
  snprintf (payload, sizeof payload,
            "external=%u|ordinal=%u|side0_size=%u|side0_hash=%016" PRIx64
            "|side0_ref=%s|side1_size=%u|side1_hash=%016" PRIx64
            "|side1_ref=%s|outcome=%s",
            external_variable (solver, idx), ordinal, size0, hash0, ref0,
            size1, hash1, ref1, outcome);
  kissat_event_trace (solver, "bve_pair", payload, false);
}

void kissat_event_trace_bve_heap (kissat *solver, const char *stage,
                                  heap *schedule, unsigned trigger) {
  const char *selected = getenv ("KISSAT_EVENT_TRACE_BVE_HEAP");
  if (!selected || !*selected)
    return;
  if (!trace_bve_heap_initialized) {
    trace_bve_heap_initialized = true;
    const char *p = selected;
    while (*p && trace_bve_heap_vars_size < 8) {
      char *end;
      const long external = strtol (p, &end, 10);
      if (end == p)
        break;
      for (all_variables (idx))
        if ((long) external_variable (solver, idx) == external) {
          const size_t i = trace_bve_heap_vars_size++;
          trace_bve_heap_vars[i] = idx;
          trace_bve_heap_last_pos[i] = UINT_MAX;
          trace_bve_heap_last_score[i] = 0;
          break;
        }
      p = *end ? end + 1 : end;
    }
  }
  open_trace ();
  if (!trace_file || !detail_enabled (solver))
    return;
  const unsigned root = EMPTY_STACK (schedule->stack)
                            ? UINT_MAX
                            : PEEK_STACK (schedule->stack, 0);
  for (size_t i = 0; i < trace_bve_heap_vars_size; i++) {
    const unsigned subject = trace_bve_heap_vars[i];
    const unsigned position = kissat_heap_contains (schedule, subject)
                                  ? schedule->pos[subject]
                                  : UINT_MAX;
    const double score = schedule->score[subject];
    const double relevance = solver->stable
                                 ? solver->scores.score[subject]
                                 : (double) LINK (subject).stamp;
    const size_t positive = SIZE_WATCHES (WATCHES (LIT (subject)));
    const size_t negative = SIZE_WATCHES (WATCHES (NOT (LIT (subject))));
    if (position == trace_bve_heap_last_pos[i] &&
        score == trace_bve_heap_last_score[i])
      continue;
    char payload[512];
    if (position == UINT_MAX)
      snprintf (payload, sizeof payload,
                "stage=%s|trigger=%u|subject=%u|score=%.17g|score_hex=%a|"
                "relevance=%.17g|relevance_hex=%a|positive=%zu|negative=%zu|"
                "pos=-|root=%u|size=%zu",
                stage, trigger == UINT_MAX ? 0 : external_variable (solver, trigger),
                external_variable (solver, subject), score, score, relevance,
                relevance, positive, negative,
                root == UINT_MAX ? 0 : external_variable (solver, root),
                SIZE_STACK (schedule->stack));
    else
      snprintf (payload, sizeof payload,
                "stage=%s|trigger=%u|subject=%u|score=%.17g|score_hex=%a|"
                "relevance=%.17g|relevance_hex=%a|positive=%zu|negative=%zu|"
                "pos=%u|root=%u|size=%zu",
                stage, trigger == UINT_MAX ? 0 : external_variable (solver, trigger),
                external_variable (solver, subject), score, score, relevance,
                relevance, positive, negative, position,
                root == UINT_MAX ? 0 : external_variable (solver, root),
                SIZE_STACK (schedule->stack));
    kissat_event_trace (solver, "bve_heap", payload, false);
    trace_bve_heap_last_pos[i] = position;
    trace_bve_heap_last_score[i] = score;
  }
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

static void emit_watch_state (kissat *solver, unsigned propagated,
                              const char *event, bool emit_summary) {
  open_trace ();
  if ((trace_watch_within_vivify && !trace_vivify_watch_active) ||
      !detail_enabled (solver) || trace_emitted >= trace_limit)
    return;
  const char *selected_scan = getenv ("KISSAT_EVENT_TRACE_WATCH_SCAN_LITERAL");
  if (selected_scan &&
      external_literal (solver, propagated) != atoi (selected_scan))
    return;
  const unsigned watched = NOT (propagated);
  watches entries = WATCHES (watched);
  const watch *p = BEGIN_WATCHES (entries);
  const watch *const end = END_WATCHES (entries);
  size_t selected_entries = 0;
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
    selected_entries++;
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
      if (getenv ("KISSAT_EVENT_TRACE_WATCH_VALUES")) {
        written = snprintf (q, left, "|values=");
        if (written >= 0 && (size_t) written < left) {
          q += written;
          left -= written;
          size_t i = 0;
          for (all_literals_in_clause (lit, large)) {
            written = snprintf (q, left, "%s%d", i++ ? "," : "",
                                (int) solver->values[lit]);
            if (written < 0 || (size_t) written >= left)
              break;
            q += written;
            left -= written;
          }
        }
      }
    }
    free (external);
    kissat_event_trace (solver, event, payload, false);
  }
  if (emit_summary) {
    char payload[192];
    snprintf (payload, sizeof payload, "scan=%d|selected=%zu|entries=%zu",
              external_literal (solver, propagated), selected_entries,
              SIZE_WATCHES (entries));
    kissat_event_trace (solver, "watch_post_summary", payload, false);
  }
}

void kissat_event_trace_watch_scan (kissat *solver, unsigned propagated) {
  emit_watch_state (solver, propagated, "watch", false);
}

void kissat_event_trace_watch_scan_post (kissat *solver,
                                         unsigned propagated) {
  if (!getenv ("KISSAT_EVENT_TRACE_WATCH_POST"))
    return;
  emit_watch_state (solver, propagated, "watch_post", true);
}

void kissat_event_trace_watch_cost (kissat *solver, unsigned propagated,
                                    uint64_t words_before,
                                    uint64_t words_after, uint64_t base,
                                    uint64_t binary_assignments,
                                    uint64_t large_dereferences,
                                    uint64_t relocations,
                                    uint64_t large_assignments,
                                    uint64_t total) {
  open_trace ();
  if (!trace_watch_cost ||
      (trace_watch_within_vivify && !trace_vivify_watch_active) ||
      !detail_enabled (solver))
    return;
  char payload[512];
  snprintf (payload, sizeof payload,
            "ordinal=%" PRIu64 "|scan=%d|words_before=%" PRIu64
            "|words_after=%" PRIu64 "|base=%" PRIu64
            "|binary_assign=%" PRIu64 "|large_deref=%" PRIu64
            "|relocate=%" PRIu64 "|large_assign=%" PRIu64
            "|total=%" PRIu64,
            trace_vivify_watch_ordinal,
            external_literal (solver, propagated), words_before, words_after,
            base, binary_assignments, large_dereferences, relocations,
            large_assignments, total);
  kissat_event_trace (solver, "watch_cost", payload, false);
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
  const reference ref = kissat_reference_clause (solver, conflict);
  written = snprintf (p, left,
                      "|ref=%u|glue=%u|used=%u|redundant=%u", ref,
                      conflict->glue, conflict->used, conflict->redundant);
  if (written < 0 || (size_t) written >= left) {
    free (external);
    return;
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

static void emit_heap_scores (kissat *solver) {
  heap *heap = SCORES;
  char payload[4096];
  uint64_t stack_hash = UINT64_C (1469598103934665603);
  for (all_stack (unsigned, idx, heap->stack)) {
    stack_hash ^= (uint64_t) external_literal (solver, LIT (idx));
    stack_hash *= UINT64_C (1099511628211);
  }
  const int prefix = snprintf (payload, sizeof payload,
                               "hash=%016" PRIx64 "|", stack_hash);
  char *p = payload + prefix;
  size_t left = sizeof payload - (size_t) prefix;
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

void kissat_event_trace_heap_before_bumps (kissat *solver) {
  if (!detail_enabled (solver) || !solver->stable || !trace_heap_each_bump)
    return;
  const uint64_t stop = stop_after_conflict ();
  if (stop == UINT64_MAX || (solver->statistics.conflicts < stop &&
       solver->statistics.decisions < trace_heap_from_decision))
    return;
  emit_heap_scores (solver);
}

void kissat_event_trace_heap_scores (kissat *solver) {
  if (!detail_enabled (solver) || !solver->stable)
    return;
  const uint64_t stop = stop_after_conflict ();
  if (stop != UINT64_MAX) {
    if (!trace_heap_each_bump ||
        (solver->statistics.conflicts < stop &&
         solver->statistics.decisions < trace_heap_from_decision))
      return;
    emit_heap_scores (solver);
    return;
  }
  emit_heap_scores (solver);
  if (stop != UINT64_MAX) {
    fflush (trace_file);
    raise (SIGTERM);
  }
}

void kissat_event_trace_heap_insert (kissat *solver, heap *heap, unsigned idx,
                                     unsigned from, unsigned to) {
  if (!detail_enabled (solver) || trace_heap_pop_only || !solver->stable ||
      heap != SCORES)
    return;
  char payload[192];
  snprintf (payload, sizeof payload,
            "var=%d|score=%.17g|from=%u|to=%u",
            external_literal (solver, LIT (idx)), heap->score[idx], from, to);
  kissat_event_trace (solver, "heap_insert", payload, false);
}

void kissat_event_trace_heap_remove (kissat *solver, heap *heap,
                                     unsigned removed, unsigned last,
                                     unsigned removed_pos, unsigned from,
                                     unsigned to) {
  if (!detail_enabled (solver) || !solver->stable || heap != SCORES ||
      solver->statistics.decisions != trace_heap_remove_decision)
    return;
  char payload[256];
  if (to == UINT_MAX)
    snprintf (payload, sizeof payload,
              "removed=%d:%.17g|removed_pos=%u|last=%d:%.17g|from=%u|to=-",
              external_literal (solver, LIT (removed)), heap->score[removed],
              removed_pos, external_literal (solver, LIT (last)),
              heap->score[last], from);
  else
    snprintf (payload, sizeof payload,
              "removed=%d:%.17g|removed_pos=%u|last=%d:%.17g|from=%u|to=%u",
              external_literal (solver, LIT (removed)), heap->score[removed],
              removed_pos, external_literal (solver, LIT (last)),
              heap->score[last], from, to);
  kissat_event_trace (solver, "heap_remove", payload, false);
}

void kissat_event_trace_heap_pop (kissat *solver, const char *stage,
                                  heap *heap, unsigned held, unsigned other,
                                  unsigned from, unsigned to) {
  if (!detail_enabled (solver) || !solver->stable ||
      solver->statistics.decisions != trace_heap_pop_decision)
    return;
  char payload[256];
  snprintf (payload, sizeof payload,
            "stage=%s|held=%d:%.17g|other=%d:%.17g|from=%u|to=%u", stage,
            external_literal (solver, LIT (held)), heap->score[held],
            external_literal (solver, LIT (other)), heap->score[other], from,
            to);
  kissat_event_trace (solver, "heap_pop", payload, false);
}

void kissat_event_trace_binary_assignment (kissat *solver, unsigned assigned,
                                           unsigned immediate_reason,
                                           unsigned stored_reason,
                                           bool jumped) {
  if (!trace_file)
    return;
  const char *selected = getenv ("KISSAT_EVENT_TRACE_BINARY_ASSIGNMENT");
  if (!selected || !*selected)
    return;
  const int external = external_literal (solver, assigned);
  const int wanted = atoi (selected);
  if (abs (external) != abs (wanted))
    return;
  char payload[256];
  snprintf (payload, sizeof payload,
            "assigned=%d|immediate=%d|jumped=%u|stored=%d", external,
            external_literal (solver, immediate_reason), jumped,
            external_literal (solver, stored_reason));
  kissat_event_trace (solver, "binary_assignment", payload, false);
}

void kissat_event_trace_binary_add (kissat *solver, unsigned first,
                                    unsigned second) {
  binary_lifecycle_event (solver, "add", first, second);
  if (!trace_file)
    return;
  const char *selected = getenv ("SAT_EVENT_TRACE_BINARY_ADD");
  if (!selected || !*selected)
    return;
  const int wanted = atoi (selected);
  const int external_first = external_literal (solver, first);
  const int external_second = external_literal (solver, second);
  if (external_first != wanted && external_second != wanted)
    return;
  char payload[96];
  snprintf (payload, sizeof payload, "a=%d|b=%d", external_first,
            external_second);
  kissat_event_trace (solver, "binary_add", payload, false);
}

void kissat_event_trace_binary_delete (kissat *solver, unsigned first,
                                       unsigned second) {
  binary_lifecycle_event (solver, "delete", first, second);
}

void kissat_event_trace_binary_collapse (kissat *solver, unsigned first,
                                         unsigned second, clause *source) {
  binary_lifecycle_event_with_clause (solver, "sweep-large-collapse-source",
                                      first, second, source);
}

void kissat_event_trace_binary_rewrite (kissat *solver,
                                        unsigned source_first,
                                        unsigned source_second,
                                        unsigned target_first,
                                        unsigned target_second) {
  char event[96];
  snprintf (event, sizeof event, "sweep-binary-rewrite-from=%d,%d",
            external_literal (solver, source_first),
            external_literal (solver, source_second));
  binary_lifecycle_event_with_clause (solver, event, target_first,
                                      target_second, 0);
}

void kissat_event_trace_sweep_rewrite_binary (kissat *solver,
                                              unsigned source,
                                              unsigned representative,
                                              size_t position,
                                              unsigned other) {
  const char *selected = getenv ("SAT_EVENT_TRACE_SWEEP_REWRITE");
  if (!selected || !*selected)
    return;
  const int external_representative =
      external_literal (solver, representative);
  if (external_representative != atoi (selected))
    return;
  char payload[160];
  snprintf (payload, sizeof payload,
            "source=%d|repr=%d|position=%zu|other=%d",
            external_literal (solver, source), external_representative,
            position, external_literal (solver, other));
  kissat_event_trace (solver, "sweep_rewrite_binary", payload, false);
}

void kissat_event_trace_sweep_rewrite_occurrence (
    kissat *solver, unsigned source, unsigned representative, size_t position,
    bool binary, unsigned other, clause *large) {
  const char *selected = getenv ("SAT_EVENT_TRACE_SWEEP_REWRITE");
  if (!selected || !*selected)
    return;
  const int external_representative =
      external_literal (solver, representative);
  if (external_representative != atoi (selected))
    return;
  char payload[4096];
  int written = snprintf (
      payload, sizeof payload, "source=%d|repr=%d|position=%zu|kind=%s",
      external_literal (solver, source), external_representative, position,
      binary ? "binary" : "large");
  if (binary)
    snprintf (payload + written, sizeof payload - (size_t) written,
              "|other=%d", external_literal (solver, other));
  else {
    written += snprintf (payload + written,
                         sizeof payload - (size_t) written, "|clause=");
    bool first = true;
    for (all_literals_in_clause (lit, large)) {
      const int delta = snprintf (payload + written,
                                  sizeof payload - (size_t) written,
                                  "%s%d", first ? "" : ",",
                                  external_literal (solver, lit));
      if (delta < 0 || (size_t) delta >= sizeof payload - (size_t) written)
        break;
      written += delta;
      first = false;
    }
  }
  kissat_event_trace (solver, "sweep_rewrite_occurrence", payload, false);
}

void kissat_event_trace_clause (kissat *solver, const char *event) {
  if (!detail_enabled (solver) || trace_emitted >= trace_limit)
    return;
  const size_t size = SIZE_STACK (solver->clause);
  int *external = malloc (size * sizeof *external);
  size_t i = 0;
  uint64_t order_hash = UINT64_C (1469598103934665603);
  for (all_stack (unsigned, lit, solver->clause)) {
    const int value = external_literal (solver, lit);
    external[i++] = value;
    order_hash = mix (order_hash, (uint64_t) (int64_t) value);
  }
  qsort (external, size, sizeof *external, compare_external_literals);
  if (trace_trajectory_only) {
    uint64_t set_hash = UINT64_C (1469598103934665603);
    for (i = 0; i != size; i++)
      set_hash = mix (set_hash, (uint64_t) (int64_t) external[i]);
    char compact[128];
    snprintf (compact, sizeof compact,
              "size=%zu|order_hash=%016" PRIx64 "|set_hash=%016" PRIx64,
              size, order_hash, set_hash);
    free (external);
    kissat_event_trace (solver, event, compact, false);
    return;
  }
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
  written = snprintf (p, left, "|levels=");
  if (written < 0 || (size_t) written >= left) {
    free (external);
    return;
  }
  p += written;
  left -= (size_t) written;
  unsigned *nonasserting_levels = malloc (size * sizeof *nonasserting_levels);
  size_t nonasserting_glue = 0;
  i = 0;
  for (all_stack (unsigned, lit, solver->clause)) {
    const unsigned level = LEVEL (lit);
    written = snprintf (p, left, "%s%u", i ? "," : "", level);
    if (written < 0 || (size_t) written >= left)
      break;
    p += written;
    left -= (size_t) written;
    if (i) {
      bool found = false;
      for (size_t j = 0; j != nonasserting_glue; ++j)
        found |= nonasserting_levels[j] == level;
      if (!found)
        nonasserting_levels[nonasserting_glue++] = level;
    }
    i++;
  }
  written = snprintf (p, left, "|nonasserting_glue=%zu",
                      nonasserting_glue);
  if (written < 0 || (size_t) written >= left) {
    free (nonasserting_levels);
    free (external);
    return;
  }
  free (nonasserting_levels);
  free (external);
  kissat_event_trace (solver, event, payload, false);
}

void kissat_event_trace_learned (kissat *solver) {
  kissat_event_trace_clause (solver, "learned");
}

void kissat_event_trace_clause_glue (kissat *solver, clause *candidate,
                                     unsigned old_glue,
                                     unsigned new_glue) {
  if (!detail_enabled (solver) || trace_emitted >= trace_limit)
    return;
  char payload[4096];
  const reference ref = (ward *) candidate - BEGIN_STACK (solver->arena);
  int written = snprintf (payload, sizeof payload,
                          "source=promote|ref=%u|old=%u|new=%u|pose=",
                          ref, old_glue, new_glue);
  if (written < 0 || (size_t) written >= sizeof payload)
    return;
  char *p = payload + written;
  size_t left = sizeof payload - (size_t) written;
  size_t i = 0;
  for (all_literals_in_clause (lit, candidate)) {
    written = snprintf (p, left, "%s%d", i++ ? "," : "",
                        external_literal (solver, lit));
    if (written < 0 || (size_t) written >= left)
      return;
    p += written;
    left -= (size_t) written;
  }
  written = snprintf (p, left, "|set=");
  if (written < 0 || (size_t) written >= left)
    return;
  p += written;
  left -= (size_t) written;
  append_sorted_clause (solver, candidate, &p, &left);
  kissat_event_trace (solver, "reason_glue", payload, false);
}

void kissat_event_trace_clause_used (kissat *solver, clause *candidate) {
  if (!detail_enabled (solver) || trace_emitted >= trace_limit)
    return;
  char payload[4096];
  int written = snprintf (payload, sizeof payload, "mode=%u|glue=%u|pose=",
                          solver->stable, candidate->glue);
  if (written < 0 || (size_t) written >= sizeof payload)
    return;
  char *p = payload + written;
  size_t left = sizeof payload - (size_t) written;
  size_t i = 0;
  for (all_literals_in_clause (lit, candidate)) {
    written = snprintf (p, left, "%s%d", i++ ? "," : "",
                        external_literal (solver, lit));
    if (written < 0 || (size_t) written >= left)
      return;
    p += written;
    left -= (size_t) written;
  }
  written = snprintf (p, left, "|set=");
  if (written < 0 || (size_t) written >= left)
    return;
  p += written;
  left -= (size_t) written;
  append_sorted_clause (solver, candidate, &p, &left);
  kissat_event_trace (solver, "reason_used", payload, false);
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

void kissat_event_trace_vivify_candidate (kissat *solver, const char *event,
                                          size_t ordinal, clause *candidate,
                                          uint64_t ticks, uint64_t limit) {
  bool selected = true;
  const char *from = getenv ("KISSAT_EVENT_TRACE_VIVIFY_CANDIDATE_FROM");
  if (from && *from && ordinal < strtoull (from, 0, 10))
    selected = false;
  const char *to = getenv ("KISSAT_EVENT_TRACE_VIVIFY_CANDIDATE_TO");
  if (to && *to && ordinal > strtoull (to, 0, 10))
    selected = false;
  const bool start = !strcmp (event, "vivify_candidate");
  const bool end = !strcmp (event, "vivify_candidate_end");
  open_trace ();
  if (trace_watch_within_vivify && start) {
    trace_vivify_watch_active = selected;
    trace_vivify_watch_ordinal = ordinal;
  }
  if (!detail_enabled (solver) || !selected) {
    if (trace_watch_within_vivify && end)
      trace_vivify_watch_active = false;
    return;
  }
  char payload[4096];
  int written = snprintf (payload, sizeof payload,
                          "ordinal=%zu|ticks=%" PRIu64 "|limit=%" PRIu64
                          "|size=%u|redundant=%u|glue=%u|garbage=%u|clause=",
                          ordinal, ticks, limit, candidate->size,
                          candidate->redundant, candidate->glue,
                          candidate->garbage);
  if (written < 0 || (size_t) written >= sizeof payload)
    return;
  char *p = payload + written;
  size_t left = sizeof payload - (size_t) written;
  append_sorted_clause (solver, candidate, &p, &left);
  if (left) {
    written = snprintf (p, left, "|pose=");
    if (written < 0 || (size_t) written >= left)
      left = 0;
    else
      p += written, left -= (size_t) written;
  }
  size_t pose_ordinal = 0;
  for (all_literals_in_clause (lit, candidate)) {
    if (!left)
      break;
    written = snprintf (p, left, "%s%d", pose_ordinal++ ? "," : "",
                        external_literal (solver, lit));
    if (written < 0 || (size_t) written >= left)
      left = 0;
    else
      p += written, left -= (size_t) written;
  }
  if (left) {
    const reference ref = (ward *) candidate - BEGIN_STACK (solver->arena);
    written = snprintf (p, left,
                        "|priority=%u|arena_order=%" PRIu64
                        "|ref=%u|decisions=",
                        candidate->vivify, (uint64_t) ref, ref);
    if (written < 0 || (size_t) written >= left)
      left = 0;
    else
      p += written, left -= (size_t) written;
  }
  for (unsigned level = 1; level <= solver->level && left; ++level) {
    written = snprintf (p, left, "%s%d", level == 1 ? "" : ",",
                        external_literal (solver, FRAME (level).decision));
    if (written < 0 || (size_t) written >= left)
      left = 0;
    else
      p += written, left -= (size_t) written;
  }
  annotate_clause_set (solver, payload, sizeof payload);
  kissat_event_trace (solver, event, payload, false);
  if (trace_watch_within_vivify && end)
    trace_vivify_watch_active = false;
}

void kissat_event_trace_vivify_counts (kissat *solver,
                                       const unsigned *counts) {
  const char *spec = getenv ("SAT_EVENT_TRACE_VIVIFY_LITERALS");
  if (!spec || !*spec || !detail_enabled (solver))
    return;
  char payload[4096];
  size_t used = (size_t) snprintf (payload, sizeof payload, "literals=");
  const char *cursor = spec;
  size_t selected = 0;
  while (*cursor && selected++ != 64 && used < sizeof payload) {
    char *end = 0;
    const long wanted = strtol (cursor, &end, 10);
    if (end == cursor || (*end && *end != ','))
      return;
    unsigned found = INVALID_LIT;
    for (all_literals (lit))
      if (external_literal (solver, lit) == wanted) {
        found = lit;
        break;
      }
    int written;
    if (found == INVALID_LIT)
      written = snprintf (payload + used, sizeof payload - used,
                          "%s%ld:absent", selected == 1 ? "" : ",",
                          wanted);
    else
      written = snprintf (payload + used, sizeof payload - used,
                          "%s%ld:%u@raw=%u", selected == 1 ? "" : ",",
                          wanted, counts[found], found);
    if (written <= 0 || (size_t) written >= sizeof payload - used)
      return;
    used += (size_t) written;
    if (found != INVALID_LIT &&
        getenv ("SAT_EVENT_TRACE_VIVIFY_CLAUSES")) {
      ward *const arena = BEGIN_STACK (solver->arena);
      clause *c = (clause *) arena;
      const clause *const arena_end = (clause *) END_STACK (solver->arena);
      while (c != arena_end) {
        bool contains = false;
        if (!c->garbage)
          for (all_literals_in_clause (member, c))
            contains |= member == found;
        if (contains) {
          int *semantic = malloc (c->size * sizeof *semantic);
          size_t semantic_size = 0;
          for (all_literals_in_clause (member, c))
            semantic[semantic_size++] = external_literal (solver, member);
          qsort (semantic, semantic_size, sizeof *semantic,
                 compare_external_literals);
          uint64_t hash = UINT64_C (1469598103934665603);
          for (size_t i = 0; i != semantic_size; i++)
            hash = mix (hash, (uint64_t) (int64_t) semantic[i]);
          hash = mix (hash, c->size);
          char clause_payload[4096];
          int clause_written = snprintf (
              clause_payload, sizeof clause_payload,
              "literal=%ld|hash=%016" PRIx64 "|size=%u|redundant=%u"
              "|glue=%u|clause=",
              wanted, hash, c->size, c->redundant, c->glue);
          size_t clause_used = clause_written > 0 ? (size_t) clause_written : 0;
          for (size_t i = 0; i != semantic_size; i++) {
            if (clause_used < sizeof clause_payload) {
              clause_written = snprintf (
                  clause_payload + clause_used,
                  sizeof clause_payload - clause_used, "%s%d",
                  i ? "," : "", semantic[i]);
              if (clause_written > 0 &&
                  (size_t) clause_written < sizeof clause_payload - clause_used)
                clause_used += (size_t) clause_written;
              else
                clause_used = sizeof clause_payload;
            }
          }
          if (clause_used < sizeof clause_payload) {
            clause_written = snprintf (clause_payload + clause_used,
                                       sizeof clause_payload - clause_used,
                                       "|pose=");
            if (clause_written > 0)
              clause_used += (size_t) clause_written;
          }
          size_t pose_ordinal = 0;
          for (all_literals_in_clause (member, c)) {
            if (clause_used >= sizeof clause_payload)
              break;
            clause_written = snprintf (
                clause_payload + clause_used,
                sizeof clause_payload - clause_used, "%s%d",
                pose_ordinal++ ? "," : "",
                external_literal (solver, member));
            if (clause_written <= 0)
              break;
            clause_used += (size_t) clause_written;
          }
          free (semantic);
          kissat_event_trace (solver, "vivify_occurrence_clause",
                              clause_payload, false);
        }
        c = kissat_next_clause (c);
      }
    }
    cursor = *end ? end + 1 : end;
  }
  if (*cursor)
    return;
  kissat_event_trace (solver, "vivify_counts", payload, false);
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
  if (!detail_enabled (solver))
    return;
  const char *selected = getenv ("KISSAT_EVENT_TRACE_SUBSTITUTE_LITERAL");
  const char *selected_representative =
      getenv ("SAT_EVENT_TRACE_SUBSTITUTE_REPRESENTATIVE");
  if (selected) {
    if (atoi (selected) != external_literal (solver, lit))
      return;
  } else if (selected_representative) {
    if (atoi (selected_representative) !=
        external_literal (solver, representative))
      return;
  } else {
    const char *all = getenv ("SAT_EVENT_TRACE_SUBSTITUTE_ALL");
    if (!all || !atoi (all) || representative == lit)
      return;
  }
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

void kissat_event_trace_sweep_environment_clause (
    kissat *solver, unsigned seed, unsigned ordinal, size_t size,
    const unsigned *lits) {
  const char *selected = getenv ("SAT_EVENT_TRACE_SWEEP_CLAUSES");
  const int external_seed = external_literal (solver, seed);
  if (!selected || strtol (selected, 0, 10) != external_seed)
    return;
  const size_t capacity = 96 + 13 * size;
  char *payload = malloc (capacity);
  if (!payload)
    return;
  int written = snprintf (payload, capacity, "seed=%d|ordinal=%u|order=",
                          external_seed, ordinal);
  for (size_t i = 0; i < size && written > 0 &&
                     (size_t) written < capacity;
       i++)
    written += snprintf (payload + written, capacity - (size_t) written,
                         "%s%d", i ? "," : "",
                         external_literal (solver, lits[i]));
  kissat_event_trace (solver, "sweep_environment_clause", payload, false);
  free (payload);
}

void kissat_event_trace_sweep_environment_var (kissat *solver, unsigned seed,
                                               unsigned ordinal, unsigned idx,
                                               unsigned depth) {
  const char *selected = getenv ("SAT_EVENT_TRACE_SWEEP_VARS");
  const int external_seed = external_literal (solver, seed);
  if (!selected || strtol (selected, 0, 10) != external_seed)
    return;
  char payload[160];
  snprintf (payload, sizeof payload,
            "seed=%d|ordinal=%u|external=%u|depth=%u", external_seed,
            ordinal, external_variable (solver, idx), depth);
  kissat_event_trace (solver, "sweep_environment_var", payload, false);
}

void kissat_event_trace_sweep_binary_attempt (
    kissat *solver, unsigned seed, unsigned ordinal, unsigned lit,
    unsigned other, int lit_depth, int other_depth,
    const char *disposition) {
  const char *selected = getenv ("SAT_EVENT_TRACE_SWEEP_BINARIES");
  const int external_seed = external_literal (solver, seed);
  if (!selected || strtol (selected, 0, 10) != external_seed)
    return;
  char payload[224];
  snprintf (payload, sizeof payload,
            "seed=%d|ordinal=%u|lit=%d|other=%d|lit_depth=%d|"
            "other_depth=%d|disposition=%s",
            external_seed, ordinal, external_literal (solver, lit),
            external_literal (solver, other), lit_depth, other_depth,
            disposition);
  kissat_event_trace (solver, "sweep_binary_attempt", payload, false);
}

static bool selected_sweep_query_seed (kissat *solver, unsigned seed) {
  const char *selected = getenv ("SAT_EVENT_TRACE_SWEEP_QUERIES");
  return selected &&
         strtol (selected, 0, 10) == external_literal (solver, seed);
}

void kissat_event_trace_sweep_query (kissat *solver, unsigned seed,
                                     unsigned ordinal, const char *kind,
                                     unsigned a, unsigned b, int result,
                                     uint64_t ticks) {
  if (!selected_sweep_query_seed (solver, seed))
    return;
  const int external_a = a == INVALID_LIT ? 0 : external_literal (solver, a);
  const int external_b = b == INVALID_LIT ? 0 : external_literal (solver, b);
  char payload[256];
  snprintf (payload, sizeof payload,
            "seed=%d|ordinal=%u|kind=%s|a=%d|b=%d|result=%d|ticks=%" PRIu64
            "|conflicts=0",
            external_literal (solver, seed), ordinal, kind, external_a,
            external_b, result, ticks);
  kissat_event_trace (solver, "sweep_query", payload, false);
}

void kissat_event_trace_sweep_query_model (kissat *solver, unsigned seed,
                                           unsigned ordinal, size_t size,
                                           const unsigned *vars) {
  if (!selected_sweep_query_seed (solver, seed))
    return;
  const size_t capacity = 96 + 13 * size;
  char *payload = malloc (capacity);
  if (!payload)
    return;
  int written = snprintf (payload, capacity, "seed=%d|ordinal=%u|model=",
                          external_literal (solver, seed), ordinal);
  for (size_t i = 0; i < size && written > 0 &&
                     (size_t) written < capacity;
       i++) {
    const unsigned lit = LIT (vars[i]);
    const signed char value = kitten_value (solver->kitten, lit);
    const unsigned model_lit = value < 0 ? NOT (lit) : lit;
    written += snprintf (payload + written, capacity - (size_t) written,
                         "%s%d", i ? "," : "",
                         external_literal (solver, model_lit));
  }
  kissat_event_trace (solver, "sweep_query_model", payload, false);
  free (payload);
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
  if (trace_file) {
    fflush (trace_file);
    fclose (trace_file);
    trace_file = 0;
  }
  if (reduce_trace_file) {
    fflush (reduce_trace_file);
    fclose (reduce_trace_file);
    reduce_trace_file = 0;
  }
  if (congruence_binary_trace_file) {
    fflush (congruence_binary_trace_file);
    fclose (congruence_binary_trace_file);
    congruence_binary_trace_file = 0;
  }
  if (binary_lifecycle_file) {
    fflush (binary_lifecycle_file);
    fclose (binary_lifecycle_file);
    binary_lifecycle_file = 0;
  }
  if (transitive_trace_file) {
    fflush (transitive_trace_file);
    fclose (transitive_trace_file);
    transitive_trace_file = 0;
  }
}
