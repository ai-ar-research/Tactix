#ifndef _eventtrace_h_INCLUDED
#define _eventtrace_h_INCLUDED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct kissat;
struct clause;
struct heap;

void kissat_event_trace (struct kissat *, const char *, const char *, bool);
void kissat_event_trace_late (struct kissat *, const char *, const char *,
                              bool);
void kissat_event_trace_detail (struct kissat *, const char *, int);
void kissat_event_trace_mode_clock (struct kissat *, const char *);
void kissat_event_trace_restart_clock (struct kissat *);
void kissat_event_trace_vivify_candidate (struct kissat *, const char *,
                                          size_t, struct clause *, uint64_t,
                                          uint64_t);
void kissat_event_trace_vivify_counts (struct kissat *, const unsigned *);
void kissat_event_trace_vivify_factor_image (struct kissat *, struct clause *,
                                             unsigned, const unsigned *);
void kissat_event_trace_literal (struct kissat *, const char *, unsigned);
void kissat_event_trace_randec_draw (struct kissat *, unsigned raw,
                                     unsigned idx, bool accepted);
void kissat_event_trace_randec_state (struct kissat *, const char *stage);
void kissat_event_trace_phase_pick (struct kissat *, unsigned idx, int result);
void kissat_event_trace_phase_save (struct kissat *, unsigned lit);
void kissat_event_trace_target_state (struct kissat *, const char *stage,
                                      unsigned assigned);
void kissat_event_trace_watch_scan (struct kissat *, unsigned propagated);
void kissat_event_trace_watch_scan_post (struct kissat *, unsigned propagated);
void kissat_event_trace_watch_cost (struct kissat *, unsigned propagated,
                                    uint64_t words_before,
                                    uint64_t words_after, uint64_t base,
                                    uint64_t binary_assignments,
                                    uint64_t large_dereferences,
                                    uint64_t relocations,
                                    uint64_t large_assignments,
                                    uint64_t total);
void kissat_event_trace_search_propagate_begin (struct kissat *);
void kissat_event_trace_search_propagate_end (struct kissat *);
void kissat_event_trace_propagate_cursor (struct kissat *);
void kissat_event_trace_implications (struct kissat *, size_t trail_begin);
void kissat_event_trace_conflict (struct kissat *, struct clause *);
void kissat_event_trace_heap_before_bumps (struct kissat *);
void kissat_event_trace_heap_scores (struct kissat *);
void kissat_event_trace_heap_insert (struct kissat *, struct heap *, unsigned,
                                     unsigned, unsigned);
void kissat_event_trace_heap_remove (struct kissat *, struct heap *, unsigned,
                                     unsigned, unsigned, unsigned, unsigned);
void kissat_event_trace_heap_pop (struct kissat *, const char *, struct heap *,
                                  unsigned, unsigned, unsigned, unsigned);
void kissat_event_trace_root_assign (struct kissat *, unsigned lit,
                                     bool binary, unsigned reason);
void kissat_event_trace_binary_assignment (struct kissat *, unsigned assigned,
                                           unsigned immediate_reason,
                                           unsigned stored_reason,
                                           bool jumped);
void kissat_congruence_binary_trace_begin (struct kissat *, const char *,
                                           size_t);
void kissat_congruence_binary_trace_pair (struct kissat *, const char *,
                                          size_t, unsigned, unsigned);
void kissat_congruence_sparse_binary_trace (struct kissat *, const char *);
void kissat_congruence_binary_trace_end (struct kissat *, const char *);
void kissat_congruence_candidate_trace_begin (struct kissat *);
void kissat_congruence_candidate_trace (struct kissat *, const unsigned *,
                                         unsigned, unsigned, const char *);
void kissat_congruence_candidate_trace_end (struct kissat *, bool, size_t);
void kissat_event_trace_binary_add (struct kissat *, unsigned, unsigned);
void kissat_event_trace_binary_delete (struct kissat *, unsigned, unsigned);
void kissat_event_trace_binary_delete_site (struct kissat *, const char *,
                                            unsigned, unsigned);
void kissat_event_trace_binary_add_site (struct kissat *, const char *,
                                         unsigned, unsigned);
void kissat_event_trace_binary_collapse (struct kissat *, unsigned, unsigned,
                                         struct clause *);
void kissat_event_trace_substitute_binary_collapse (
    struct kissat *, unsigned, unsigned, struct clause *);
void kissat_event_trace_binary_rewrite (struct kissat *, unsigned, unsigned,
                                        unsigned, unsigned);
bool kissat_transitive_trace_pair_selected (struct kissat *, unsigned,
                                            unsigned);
bool kissat_transitive_trace_all_sources (void);
void kissat_transitive_trace_endpoints (struct kissat *);
void kissat_transitive_trace_admission (struct kissat *, unsigned, unsigned,
                                        unsigned, const char *);
void kissat_transitive_trace_walk_node (struct kissat *, unsigned, unsigned,
                                        unsigned, size_t, uint64_t, uint64_t,
                                        size_t);
void kissat_transitive_trace_walk_entries (struct kissat *, unsigned,
                                           unsigned, unsigned, unsigned);
uint64_t kissat_transitive_trace_source_begin (struct kissat *, bool,
                                                unsigned, uint64_t, size_t,
                                                unsigned);
void kissat_transitive_trace_candidate (struct kissat *, uint64_t, unsigned,
                                        size_t, unsigned, uint64_t, uint64_t,
                                        bool, bool, size_t, unsigned);
void kissat_transitive_trace_source_end (struct kissat *, uint64_t, unsigned,
                                         size_t, uint64_t, uint64_t, bool,
                                         size_t, unsigned);
void kissat_event_trace_clause (struct kissat *, const char *);
void kissat_event_trace_learned (struct kissat *);
void kissat_event_trace_clause_glue (struct kissat *, struct clause *,
                                     unsigned old_glue,
                                     unsigned new_glue);
void kissat_event_trace_clause_used (struct kissat *, struct clause *);
void kissat_event_trace_checkpoint (struct kissat *);
void kissat_event_trace_retier (struct kissat *);
void kissat_event_trace_tiers (struct kissat *, const char *);
void kissat_event_trace_factor_grid (struct kissat *, size_t, size_t,
                                     size_t);
void kissat_event_trace_factor_grid_member (struct kissat *, size_t,
                                            unsigned);
void kissat_event_trace_factor_fresh (struct kissat *, unsigned,
                                      const unsigned *, size_t, size_t,
                                      size_t, uint64_t ticks, uint64_t pops,
                                      uint64_t first_ticks,
                                      uint64_t next_ticks,
                                      uint64_t match_ticks);
bool kissat_event_trace_factor_match_selected (struct kissat *, unsigned,
                                                unsigned);
void kissat_event_trace_factor_match_large (
    struct kissat *, unsigned seed, unsigned next, size_t factors,
    size_t column, struct clause *, unsigned current_factor, unsigned scan,
    size_t degree, size_t visits, bool matched);
void kissat_event_trace_factor_update (struct kissat *, size_t, unsigned);
void kissat_event_trace_bve_candidate (struct kissat *, unsigned,
                                       const char *, uint64_t, uint64_t,
                                       bool);
void kissat_event_trace_bve_plan (struct kissat *, unsigned, unsigned,
                                  unsigned, bool, size_t, size_t, uint64_t,
                                  bool);
void kissat_event_trace_bve_pair (struct kissat *, unsigned, uint32_t,
                                  struct clause *, struct clause *,
                                  const char *);
void kissat_event_trace_bve_heap (struct kissat *, const char *, struct heap *,
                                  unsigned);
void kissat_event_trace_bve_schedule (struct kissat *, struct heap *);
void kissat_event_trace_set_boundary (struct kissat *, const char *);
void kissat_event_trace_eliminate_round_boundary (struct kissat *,
                                                   const char *,
                                                   const char *);
void kissat_event_trace_eliminate_cleanup (struct kissat *, const char *);
void kissat_event_trace_forward_clause (struct kissat *, struct clause *,
                                        const char *, int);
void kissat_event_trace_forward_partner (struct kissat *, unsigned, unsigned,
                                         struct clause *, const char *);
void kissat_event_trace_forward_binary (struct kissat *, unsigned, unsigned,
                                        struct clause *);
void kissat_event_trace_clause_garbage (struct kissat *, struct clause *,
                                        const char *);
void kissat_event_trace_factor_seed (struct kissat *, unsigned, size_t);
void kissat_event_trace_factor_summary (struct kissat *, uint64_t);
void kissat_event_trace_factor_schedule (struct kissat *, size_t, unsigned,
                                         size_t);
void kissat_event_trace_factor_mark (struct kissat *, unsigned, bool,
                                     const char *);
void kissat_event_trace_factor_candidate (struct kissat *, unsigned, size_t,
                                          size_t, unsigned, unsigned,
                                          unsigned);
void kissat_event_trace_factor_score_entries (struct kissat *, unsigned,
                                              size_t, unsigned);
void kissat_event_trace_sweep_unit (struct kissat *, unsigned, const char *);
void kissat_event_trace_sweep_equivalence (struct kissat *, unsigned,
                                           unsigned);
void kissat_event_trace_sweep_rewrite_binary (struct kissat *, unsigned,
                                              unsigned, size_t, unsigned);
void kissat_event_trace_sweep_rewrite_occurrence (
    struct kissat *, unsigned, unsigned, size_t, bool, unsigned,
    struct clause *);
void kissat_event_trace_sweep_environment_clause (
    struct kissat *, unsigned, unsigned, size_t, const unsigned *);
void kissat_event_trace_sweep_environment_var (struct kissat *, unsigned,
                                               unsigned, unsigned, unsigned);
void kissat_event_trace_sweep_binary_attempt (
    struct kissat *, unsigned, unsigned, unsigned, unsigned, int, int,
    const char *);
void kissat_event_trace_sweep_query (struct kissat *, unsigned, unsigned,
                                     const char *, unsigned, unsigned, int,
                                     uint64_t);
void kissat_event_trace_sweep_query_model (struct kissat *, unsigned,
                                           unsigned, size_t,
                                           const unsigned *);
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
void kissat_event_trace_factor_heap (struct kissat *, unsigned, const char *,
                                     const struct heap *);
void kissat_event_trace_close (void);
void kissat_reduce_trace_clause (struct kissat *, struct clause *, unsigned,
                                 unsigned, unsigned, unsigned, const char *);
void kissat_reduce_trace_summary (struct kissat *, unsigned, unsigned, size_t,
                                  size_t);
void kissat_reduce_trace_rank (struct kissat *, unsigned, size_t, bool);

#endif
