#ifndef _eventtrace_h_INCLUDED
#define _eventtrace_h_INCLUDED

#include <stdbool.h>
#include <stddef.h>

struct kissat;
struct clause;
struct heap;

void kissat_event_trace (struct kissat *, const char *, const char *, bool);
void kissat_event_trace_detail (struct kissat *, const char *, int);
void kissat_event_trace_literal (struct kissat *, const char *, unsigned);
void kissat_event_trace_randec_draw (struct kissat *, unsigned raw,
                                     unsigned idx, bool accepted);
void kissat_event_trace_randec_state (struct kissat *, const char *stage);
void kissat_event_trace_watch_scan (struct kissat *, unsigned propagated);
void kissat_event_trace_propagate_cursor (struct kissat *);
void kissat_event_trace_implications (struct kissat *, size_t trail_begin);
void kissat_event_trace_conflict (struct kissat *, struct clause *);
void kissat_event_trace_heap_before_bumps (struct kissat *);
void kissat_event_trace_heap_scores (struct kissat *);
void kissat_event_trace_heap_pop (struct kissat *, const char *, struct heap *,
                                  unsigned, unsigned, unsigned, unsigned);
void kissat_event_trace_binary_assignment (struct kissat *, unsigned assigned,
                                           unsigned immediate_reason,
                                           unsigned stored_reason,
                                           bool jumped);
void kissat_event_trace_binary_add (struct kissat *, unsigned, unsigned);
void kissat_event_trace_clause (struct kissat *, const char *);
void kissat_event_trace_learned (struct kissat *);
void kissat_event_trace_checkpoint (struct kissat *);
void kissat_event_trace_retier (struct kissat *);
void kissat_event_trace_tiers (struct kissat *, const char *);
void kissat_event_trace_factor_grid (struct kissat *, size_t, size_t,
                                     size_t);
void kissat_event_trace_factor_grid_member (struct kissat *, size_t,
                                            unsigned);
void kissat_event_trace_factor_seed (struct kissat *, unsigned, size_t);
void kissat_event_trace_factor_candidate (struct kissat *, unsigned, size_t,
                                          size_t, unsigned, unsigned,
                                          unsigned);
void kissat_event_trace_factor_score_entries (struct kissat *, unsigned,
                                              size_t, unsigned);
void kissat_event_trace_sweep_unit (struct kissat *, unsigned, const char *);
void kissat_event_trace_sweep_rewrite_binary (struct kissat *, unsigned,
                                              unsigned, size_t, unsigned);
void kissat_event_trace_factor_literal_state (struct kissat *, unsigned, bool,
                                              bool, int, size_t);
void kissat_event_trace_substitute_map (struct kissat *, unsigned, unsigned);
void kissat_event_trace_factor_remainder_binary (struct kissat *, unsigned,
                                                 size_t, unsigned);
void kissat_event_trace_factor_remainder_clause (struct kissat *, unsigned,
                                                 size_t, struct clause *);
void kissat_event_trace_factor_candidate_source (
    struct kissat *, unsigned, size_t, unsigned, unsigned, struct clause *,
    struct clause *);
void kissat_event_trace_factor_candidate_binary_source (
    struct kissat *, unsigned, size_t, unsigned, unsigned);
void kissat_event_trace_factor_heap (struct kissat *, unsigned,
                                     const struct heap *);
void kissat_event_trace_close (void);
void kissat_reduce_trace_clause (struct kissat *, struct clause *, unsigned,
                                 unsigned, unsigned, unsigned, const char *);
void kissat_reduce_trace_summary (struct kissat *, unsigned, unsigned, size_t,
                                  size_t);
void kissat_reduce_trace_rank (struct kissat *, unsigned, size_t, bool);

#endif
