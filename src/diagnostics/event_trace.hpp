// Disabled-by-default semantic event tracing for solver diagnostics.
// The TSV format uses permanent external variable identities so traces remain
// meaningful across variable compaction and can be normalized against a
// reference solver.  No file is opened and no state is scanned unless
// TACTIX_EVENT_TRACE names an output path.
#pragma once

#include "kernel/kernel.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace tactix {

#ifdef TACTIX_ENABLE_EVENT_TRACE
class EventTrace {
public:
  EventTrace() = default;
  ~EventTrace();

  bool open_from_env(const Kernel &);
  bool enabled() const { return out_ != nullptr; }
  void close();

  // Macro records are always retained until the global record cap is hit.
  void macro(const Kernel &, const char *event, const char *payload = "-");
  // Decision/analysis records cover the deterministic prefix and 1024 calls
  // after the first Reorder. An explicit conflict window overrides that cap;
  // propagation/clause records remain bounded by the global record limit.
  void detail(const Kernel &, const char *event, int64_t payload = 0);
  void detail_text(const Kernel &, const char *event, const char *payload);
  void vivify_candidate(const Kernel &, const char *event, uint64_t ordinal,
                         Ref, uint64_t ticks, uint64_t limit);
  void vivify_counts(const Kernel &, const uint32_t *counts, size_t size);
  void watch_scan(const Kernel &, Lit propagated);
  void watch_scan_post(const Kernel &, Lit propagated);
  void watch_move(const Kernel &, Lit propagated, Lit target, Lit blocking,
                  const Clause *);
  void watch_cost(const Kernel &, Lit propagated, uint64_t words_before,
                  uint64_t words_after, uint64_t base,
                  uint64_t binary_assignments, uint64_t large_dereferences,
                  uint64_t relocations, uint64_t large_assignments,
                  uint64_t total);
  void implications(const Kernel &, size_t trail_begin);
  void conflict(const Kernel &);
  void learned(const Kernel &, const std::vector<Lit> &);
  void learned_stage(const Kernel &, const char *, const std::vector<Lit> &);
  void clause_glue(const Kernel &, const char *source, const Clause *,
                   uint32_t old_glue, uint32_t new_glue);
  void clause_used(const Kernel &, const Clause *, uint32_t mode);
  void analyzed(const Kernel &, const std::vector<Var> &,
                const std::vector<uint8_t> &, uint8_t bump_mask);
  void analyzed_post(const Kernel &, const std::vector<Var> &);
  void heap_scores(const Kernel &);
  void heap_insert(const Kernel &, Var, uint32_t from, uint32_t to);
  void heap_remove(const Kernel &, Var removed, Var last,
                   uint32_t removed_pos, uint32_t from, uint32_t to);
  void heap_pop(const Kernel &, const char *stage, Var held, Var other,
                uint32_t from, uint32_t to);
  void root_assign(const Kernel &, Lit);
  void binary_assignment(const Kernel &, Lit assigned, Lit immediate_reason,
                         Lit stored_reason, bool jumped);
  void binary_add(const Kernel &, Lit a, Lit b);
  void mode_clock(const Kernel &, const char *stage, uint64_t limit,
                  uint64_t anchor, uint64_t count);
  void restart_clock(const Kernel &, uint64_t limit, bool initialized,
                     double fast, double slow, double margin, bool eligible);
  void randec_draw(const Kernel &, uint32_t raw, uint32_t slot, Var,
                   bool accepted);
  void randec_state(const Kernel &, const char *stage, uint64_t remaining,
                    uint64_t next, uint64_t sequence);
  void phase_pick(const Kernel &, Var, int8_t target, int8_t saved,
                  int8_t result);
  void target_save(const Kernel &);
  void target_state(const Kernel &, const char *stage, uint32_t assigned);
  void phase_save(const Kernel &, Lit);
  void slot_boundary(const Kernel &, const char *name, const char *stage);
  void sweep_schedule(const Kernel &, Var, uint64_t, uint64_t, uint64_t,
                      uint64_t);
  void sweep_seed(const Kernel &, Var);
  void sweep_environment(const Kernel &, Var, uint32_t, uint32_t, uint64_t);
  void sweep_environment_clause(const Kernel &, Var, uint32_t, const Lit *,
                                uint32_t);
  void sweep_query(const Kernel &, Var, uint32_t, const char *, Lit, Lit, int,
                   uint64_t, uint64_t);
  void sweep_model(const Kernel &, Var, uint32_t, const Var *, const int8_t *,
                   uint32_t);
  void sweep_flip(const Kernel &, Var, const char *, Lit, bool, uint64_t);
  void sweep_effort(const Kernel &, uint32_t, uint64_t, uint64_t, uint64_t);
  void sweep_environment_var(const Kernel &, Var, uint32_t, Var, uint32_t);
  void sweep_binary_attempt(const Kernel &, Var, uint32_t, Lit, Lit, int32_t,
                            int32_t, const char *);
  void sweep_watch(const Kernel &, Var, uint32_t, Lit);
  void sweep_equivalence(const Kernel &, Lit a, Lit b);
  void sweep_commit(const Kernel &, Lit a, Lit b, int8_t av, int8_t bv,
                    const char *stage);
  void sweep_rewrite_binary(const Kernel &, Lit source, Lit representative,
                            uint32_t position, Lit other);
  void sweep_rewrite_occurrence(const Kernel &, Lit source,
                                Lit representative, uint32_t position,
                                bool binary, uint32_t payload);
  void sweep_unit(const Kernel &, Lit);
  void substitute_map(const Kernel &, Lit, Lit);
  void factor_seed(const Kernel &, Lit, uint32_t score, uint32_t remainders);
  void factor_remainder(const Kernel &, Lit seed, uint32_t ordinal, bool large,
                        const Lit *, uint32_t);
  void factor_candidate(const Kernel &, Lit seed, uint32_t depth,
                        uint32_t ordinal, Lit candidate, uint32_t count,
                        uint32_t score);
  void factor_score_entry(const Kernel &, Lit seed, uint32_t depth,
                          Lit candidate, uint32_t ordinal, bool live,
                          Lit other, uint64_t id, const Clause *clause);
  void factor_literal_state(const Kernel &, Lit, bool marked, bool retired,
                            int value, uint32_t score);
  void factor_literal_entry(const Kernel &, Lit, uint32_t ordinal, bool live,
                            Lit other, uint64_t id, const Clause *clause);
  void factor_candidate_source(const Kernel &, Lit seed, uint32_t depth,
                               uint32_t column, Lit scan, Lit candidate,
                               const Clause *current,
                               const Clause *matched);
  void factor_candidate_binary_source(const Kernel &, Lit seed,
                                      uint32_t depth, uint32_t column, Lit q,
                                      Lit candidate);
  void factor_pair(const Kernel &, Lit grid_seed, const char *stage);
  void factor_heap(const Kernel &, Lit grid_seed, const char *stage, Lit a,
                   uint32_t pos_a, uint32_t score_a, Lit b, uint32_t pos_b,
                   uint32_t score_b, uint32_t size);
  void factor_schedule(const Kernel &, uint32_t ordinal, Lit, uint32_t score);
  void factor_mark_transition(const Kernel &, Lit, bool marked,
                              const char *source);
  void factor_update(const Kernel &, Lit grid_seed, uint32_t ordinal, Lit,
                     uint32_t score);
  void factor_grid(const Kernel &, Lit, uint32_t factors,
                   uint32_t remainders, int delta);
  void factor_grid_member(const Kernel &, uint32_t ordinal, Lit);
  void bve_candidate(const Kernel &, Var, const char *stage, uint64_t ticks,
                     uint64_t limit, bool applied);
  void bve_occurrences(const Kernel &, Var, uint32_t pos_ordered,
                       uint32_t neg_ordered, uint32_t pos_true_binary,
                       uint32_t neg_true_binary, uint32_t pos_satisfied_large,
                       uint32_t neg_satisfied_large,
                       uint32_t pos_satisfied_effective_binary,
                       uint32_t neg_satisfied_effective_binary,
                       uint32_t pos_tautological, uint32_t neg_tautological);
  void bve_plan(const Kernel &, Var, uint32_t side0, uint32_t side1,
                uint32_t gate_kind, uint32_t gate0, uint32_t gate1,
                uint64_t charged, bool resolve_gate);
  void bve_pair(const Kernel &, Var, uint32_t ordinal, const Lit *side0,
                uint32_t size0, Ref ref0, uint64_t id0, const Lit *side1,
                uint32_t size1, Ref ref1, uint64_t id1, const char *outcome);
  void bve_heap(const Kernel &, const char *stage, Var trigger, Var subject,
                double score, uint32_t position, Var root, uint32_t size);
  void bve_schedule(const Kernel &, const Var *members, const double *scores,
                    uint32_t size, uint32_t bound,
                    uint64_t insertion_hash);
  void bve_pop(const Kernel &, uint64_t ordinal, uint64_t cumulative_hash,
               Var popped, double score, Var root, uint32_t size);
  void eliminate_cleanup(const Kernel &, const char *stage);
  void formula_checkpoint(const Kernel &);
  void checkpoint(const Kernel &);
  void reorder_begin(const Kernel &);
  void reorder_end(const Kernel &);
  void retier(const Kernel &, uint32_t mode, const uint64_t *used_glue);
  void reduction(const Kernel &, const std::vector<Ref> &, size_t victims);

private:
  void emit(const Kernel &, const char *, const char *, bool snapshot);
  void emit_heap_scores(const Kernel &);
  bool heap_var_selected(const Kernel &, Var) const;
  void watch_state(const Kernel &, Lit propagated, const char *event,
                   bool emit_summary);
  bool want_detail(const Kernel &);

  std::FILE *out_ = nullptr;
  uint64_t sequence_ = 0;
  uint64_t emitted_ = 0;
  uint64_t limit_ = 50000;
  uint64_t post_reorder_details_ = 0;
  uint64_t checkpoint_bucket_ = 0;
  uint64_t checkpoint_interval_ = 1000;
  uint64_t checkpoint_from_ = 0;
  uint64_t detail_conflict_from_ = 0;
  uint64_t detail_conflict_to_ = UINT64_MAX;
  uint64_t stop_after_conflict_ = UINT64_MAX;
  uint64_t heap_from_decision_ = UINT64_MAX;
  uint64_t heap_pop_decision_ = UINT64_MAX;
  uint64_t heap_remove_decision_ = UINT64_MAX;
  uint64_t bve_pop_from_ = UINT64_MAX;
  uint64_t bve_pop_to_ = 0;
  uint64_t exit_conflict_ = UINT64_MAX;
  bool saw_reorder_ = false;
  bool detail_window_ = false;
  bool compact_probe_ = false;
  bool boundary_only_ = false;
  bool target_only_ = false;
  std::string exit_match_;
  bool skip_watch_ = false;
  bool skip_imply_ = false;
  bool skip_heap_ = false;
  bool skip_decisions_ = false;
  bool skip_randec_draws_ = false;
  bool skip_analysis_ticks_ = false;
  bool keep_snapshots_ = false;
  bool search_only_ = false;
  bool control_only_ = false;
  bool glue_only_ = false;
  bool phase_only_ = false;
  bool formula_only_ = false;
  bool trajectory_only_ = false;
  bool learned_only_ = false;
  bool imply_only_ = false;
  std::vector<int> imply_literals_;
  bool bve_heap_only_ = false;
  bool conflict_only_ = false;
  bool checkpoint_only_ = false;
  bool sweep_only_ = false;
  bool watch_only_ = false;
  bool watch_cost_ = false;
  bool watch_within_vivify_ = false;
  bool vivify_watch_active_ = false;
  uint64_t vivify_watch_ordinal_ = 0;
  bool heap_each_bump_ = false;
  bool heap_pop_only_ = false;
  bool heap_vars_only_ = false;
  std::vector<int> heap_vars_;
  bool truncated_ = false;
  bool formula_threshold_emitted_ = false;
  uint64_t formula_threshold_last_ = 0;
};
#else
// This stub is the production implementation.  Keeping it inline lets an
// optimized build erase every call site, including the per-decision and
// per-conflict sites, instead of paying for a runtime "enabled" branch.
class EventTrace {
public:
  bool open_from_env(const Kernel &) { return false; }
  bool enabled() const { return false; }
  void close() {}
  void macro(const Kernel &, const char *, const char * = "-") {}
  void detail(const Kernel &, const char *, int64_t = 0) {}
  void detail_text(const Kernel &, const char *, const char *) {}
  void vivify_candidate(const Kernel &, const char *, uint64_t, Ref, uint64_t,
                         uint64_t) {}
  void vivify_counts(const Kernel &, const uint32_t *, size_t) {}
  void watch_scan(const Kernel &, Lit) {}
  void watch_scan_post(const Kernel &, Lit) {}
  void watch_move(const Kernel &, Lit, Lit, Lit, const Clause *) {}
  void watch_cost(const Kernel &, Lit, uint64_t, uint64_t, uint64_t, uint64_t,
                  uint64_t, uint64_t, uint64_t, uint64_t) {}
  void implications(const Kernel &, size_t) {}
  void conflict(const Kernel &) {}
  void learned(const Kernel &, const std::vector<Lit> &) {}
  void learned_stage(const Kernel &, const char *, const std::vector<Lit> &) {}
  void clause_glue(const Kernel &, const char *, const Clause *, uint32_t,
                   uint32_t) {}
  void clause_used(const Kernel &, const Clause *, uint32_t) {}
  void analyzed(const Kernel &, const std::vector<Var> &,
                const std::vector<uint8_t> &, uint8_t) {}
  void analyzed_post(const Kernel &, const std::vector<Var> &) {}
  void heap_scores(const Kernel &) {}
  void heap_insert(const Kernel &, Var, uint32_t, uint32_t) {}
  void heap_remove(const Kernel &, Var, Var, uint32_t, uint32_t, uint32_t) {}
  void root_assign(const Kernel &, Lit) {}
  void mode_clock(const Kernel &, const char *, uint64_t, uint64_t,
                  uint64_t) {}
  void restart_clock(const Kernel &, uint64_t, bool, double, double, double,
                     bool) {}
  void randec_draw(const Kernel &, uint32_t, uint32_t, Var, bool) {}
  void phase_pick(const Kernel &, Var, int8_t, int8_t, int8_t) {}
  void target_save(const Kernel &) {}
  void target_state(const Kernel &, const char *, uint32_t) {}
  void phase_save(const Kernel &, Lit) {}
  void slot_boundary(const Kernel &, const char *, const char *) {}
  void sweep_seed(const Kernel &, Var) {}
  void sweep_environment(const Kernel &, Var, uint32_t, uint32_t, uint64_t) {}
  void sweep_environment_clause(const Kernel &, Var, uint32_t, const Lit *,
                                uint32_t) {}
  void sweep_query(const Kernel &, Var, uint32_t, const char *, Lit, Lit, int,
                   uint64_t, uint64_t) {}
  void sweep_model(const Kernel &, Var, uint32_t, const Var *, const int8_t *,
                   uint32_t) {}
  void sweep_flip(const Kernel &, Var, const char *, Lit, bool, uint64_t) {}
  void sweep_effort(const Kernel &, uint32_t, uint64_t, uint64_t, uint64_t) {}
  void sweep_environment_var(const Kernel &, Var, uint32_t, Var, uint32_t) {}
  void sweep_binary_attempt(const Kernel &, Var, uint32_t, Lit, Lit, int32_t,
                            int32_t, const char *) {}
  void sweep_watch(const Kernel &, Var, uint32_t, Lit) {}
  void sweep_equivalence(const Kernel &, Lit, Lit) {}
  void sweep_commit(const Kernel &, Lit, Lit, int8_t, int8_t, const char *) {}
  void sweep_rewrite_binary(const Kernel &, Lit, Lit, uint32_t, Lit) {}
  void sweep_unit(const Kernel &, Lit) {}
  void substitute_map(const Kernel &, Lit, Lit) {}
  void factor_seed(const Kernel &, Lit, uint32_t, uint32_t) {}
  void factor_remainder(const Kernel &, Lit, uint32_t, bool, const Lit *,
                        uint32_t) {}
  void factor_candidate(const Kernel &, Lit, uint32_t, uint32_t, Lit,
                        uint32_t, uint32_t) {}
  void factor_score_entry(const Kernel &, Lit, uint32_t, Lit, uint32_t, bool,
                          Lit, uint64_t, const Clause *) {}
  void factor_literal_state(const Kernel &, Lit, bool, bool, int, uint32_t) {}
  void factor_literal_entry(const Kernel &, Lit, uint32_t, bool, Lit, uint64_t,
                            const Clause *) {}
  void factor_candidate_source(const Kernel &, Lit, uint32_t, uint32_t, Lit,
                               Lit, const Clause *, const Clause *) {}
  void factor_candidate_binary_source(const Kernel &, Lit, uint32_t, uint32_t,
                                      Lit, Lit) {}
  void factor_pair(const Kernel &, Lit, const char *) {}
  void factor_heap(const Kernel &, Lit, const char *, Lit, uint32_t, uint32_t,
                   Lit, uint32_t, uint32_t, uint32_t) {}
  void factor_schedule(const Kernel &, uint32_t, Lit, uint32_t) {}
  void factor_mark_transition(const Kernel &, Lit, bool, const char *) {}
  void factor_update(const Kernel &, Lit, uint32_t, Lit, uint32_t) {}
  void factor_grid(const Kernel &, Lit, uint32_t, uint32_t, int) {}
  void factor_grid_member(const Kernel &, uint32_t, Lit) {}
  void bve_candidate(const Kernel &, Var, const char *, uint64_t, uint64_t,
                     bool) {}
  void bve_occurrences(const Kernel &, Var, uint32_t, uint32_t, uint32_t,
                       uint32_t, uint32_t, uint32_t, uint32_t, uint32_t,
                       uint32_t, uint32_t) {}
  void bve_plan(const Kernel &, Var, uint32_t, uint32_t, uint32_t, uint32_t,
                uint32_t, uint64_t, bool) {}
  void bve_pair(const Kernel &, Var, uint32_t, const Lit *, uint32_t,
                Ref, uint64_t, const Lit *, uint32_t, Ref, uint64_t,
                const char *) {}
  void bve_heap(const Kernel &, const char *, Var, Var, double, uint32_t, Var,
                uint32_t) {}
  void eliminate_cleanup(const Kernel &, const char *) {}
  void formula_checkpoint(const Kernel &) {}
  void checkpoint(const Kernel &) {}
  void reorder_begin(const Kernel &) {}
  void reorder_end(const Kernel &) {}
  void retier(const Kernel &, uint32_t, const uint64_t *) {}
  void reduction(const Kernel &, const std::vector<Ref> &, size_t) {}
};
#endif

#ifdef TACTIX_ENABLE_EVENT_TRACE
void event_trace_detail(const Kernel &, const char *event, int64_t payload);
void event_trace_vivify_candidate(const Kernel &, const char *event,
                                  uint64_t ordinal, Ref, uint64_t ticks,
                                  uint64_t limit);
void event_trace_vivify_counts(const Kernel &, const uint32_t *counts,
                               size_t size);
void event_trace_watch_scan(const Kernel &, Lit propagated);
void event_trace_watch_scan_post(const Kernel &, Lit propagated);
void event_trace_watch_move(const Kernel &, Lit propagated, Lit target,
                            Lit blocking, const Clause *);
void event_trace_watch_cost(const Kernel &, Lit propagated,
                            uint64_t words_before, uint64_t words_after,
                            uint64_t base, uint64_t binary_assignments,
                            uint64_t large_dereferences,
                            uint64_t relocations,
                            uint64_t large_assignments, uint64_t total);
void event_trace_learned_stage(const Kernel &, const char *,
                               const std::vector<Lit> &);
void event_trace_clause_glue(const Kernel &, const char *source,
                             const Clause *, uint32_t old_glue,
                             uint32_t new_glue);
void event_trace_clause_used(const Kernel &, const Clause *, uint32_t mode);
void event_trace_retier(const Kernel &, uint32_t mode,
                        const uint64_t *used_glue);
void event_trace_analyzed(const Kernel &, const std::vector<Var> &,
                          const std::vector<uint8_t> &, uint8_t bump_mask);
void event_trace_analyzed_post(const Kernel &, const std::vector<Var> &);
void event_trace_heap_scores(const Kernel &);
void event_trace_heap_insert(const Kernel &, Var, uint32_t from, uint32_t to);
void event_trace_heap_remove(const Kernel &, Var removed, Var last,
                             uint32_t removed_pos, uint32_t from, uint32_t to);
void event_trace_heap_pop(const Kernel &, const char *stage, Var held,
                          Var other, uint32_t from, uint32_t to);
void event_trace_binary_assignment(const Kernel &, Lit assigned,
                                   Lit immediate_reason, Lit stored_reason,
                                   bool jumped);
void event_trace_binary_add(const Kernel &, Lit a, Lit b);
void event_trace_binary_lifecycle(const Kernel &, const char *event, Lit a,
                                  Lit b, uint64_t id, uint32_t copies,
                                  const char *payload = nullptr);
void event_trace_binary_collapse(const Kernel &, Ref source, Lit a, Lit b);
const char *event_trace_rule_enter(const char *rule);
void event_trace_rule_leave(const char *previous);
void event_trace_randec_draw(const Kernel &, uint32_t raw, uint32_t slot, Var,
                             bool accepted);
void event_trace_randec_state(const Kernel &, const char *stage,
                              uint64_t remaining, uint64_t next,
                              uint64_t sequence);
void event_trace_phase_pick(const Kernel &, Var, int8_t target, int8_t saved,
                            int8_t result);
void event_trace_target_save(const Kernel &);
void event_trace_target_state(const Kernel &, const char *stage,
                              uint32_t assigned);
void event_trace_phase_save(const Kernel &, Lit);
void event_trace_slot_boundary(const Kernel &, const char *name,
                               const char *stage);
void event_trace_sweep_schedule(const Kernel &, Var, uint64_t pos,
                                uint64_t neg, uint64_t rank,
                                uint64_t import_order);
void event_trace_sweep_seed(const Kernel &, Var);
void event_trace_sweep_environment(const Kernel &, Var, uint32_t, uint32_t,
                                   uint64_t);
void event_trace_sweep_environment_clause(const Kernel &, Var, uint32_t,
                                          const Lit *, uint32_t);
void event_trace_sweep_query(const Kernel &, Var, uint32_t, const char *, Lit,
                             Lit, int, uint64_t, uint64_t);
void event_trace_sweep_model(const Kernel &, Var, uint32_t, const Var *,
                             const int8_t *, uint32_t);
void event_trace_sweep_effort(const Kernel &, uint32_t external,
                              uint64_t ticks_before, uint64_t ticks_after,
                              uint64_t solves);
void event_trace_sweep_environment_var(const Kernel &, Var, uint32_t, Var,
                                       uint32_t);
void event_trace_sweep_binary_attempt(const Kernel &, Var, uint32_t, Lit, Lit,
                                      int32_t, int32_t, const char *);
void event_trace_sweep_watch(const Kernel &, Var, uint32_t, Lit);
void event_trace_sweep_equivalence(const Kernel &, Lit a, Lit b);
void event_trace_sweep_commit(const Kernel &, Lit a, Lit b, int8_t av,
                              int8_t bv, const char *stage);
void event_trace_sweep_rewrite_binary(const Kernel &, Lit source,
                                      Lit representative, uint32_t position,
                                      Lit other);
void event_trace_sweep_rewrite_occurrence(const Kernel &, Lit source,
                                          Lit representative,
                                          uint32_t position, bool binary,
                                          uint32_t payload);
void event_trace_sweep_unit(const Kernel &, Lit);
void event_trace_substitute_map(const Kernel &, Lit, Lit);
void event_trace_factor_seed(const Kernel &, Lit, uint32_t score,
                             uint32_t remainders);
void event_trace_factor_remainder(const Kernel &, Lit seed, uint32_t ordinal,
                                  bool large, const Lit *, uint32_t);
void event_trace_factor_candidate(const Kernel &, Lit seed, uint32_t depth,
                                  uint32_t ordinal, Lit candidate,
                                  uint32_t count, uint32_t score);
void event_trace_factor_score_entry(const Kernel &, Lit seed, uint32_t depth,
                                    Lit candidate, uint32_t ordinal, bool live,
                                    Lit other, uint64_t id,
                                    const Clause *clause);
void event_trace_factor_literal_state(const Kernel &, Lit, bool marked,
                                      bool retired, int value, uint32_t score);
void event_trace_factor_literal_entry(const Kernel &, Lit, uint32_t ordinal,
                                      bool live, Lit other, uint64_t id,
                                      const Clause *clause);
void event_trace_factor_candidate_source(const Kernel &, Lit seed,
                                         uint32_t depth, uint32_t column,
                                         Lit scan, Lit candidate,
                                         const Clause *current,
                                         const Clause *matched);
void event_trace_factor_candidate_binary_source(const Kernel &, Lit seed,
                                                uint32_t depth,
                                                uint32_t column, Lit q,
                                                Lit candidate);
void event_trace_factor_pair(const Kernel &, Lit grid_seed, const char *stage);
void event_trace_factor_heap(const Kernel &, Lit grid_seed, const char *stage,
                             Lit a, uint32_t pos_a, uint32_t score_a, Lit b,
                             uint32_t pos_b, uint32_t score_b, uint32_t size);
void event_trace_factor_schedule(const Kernel &, uint32_t ordinal, Lit,
                                 uint32_t score);
void event_trace_factor_mark_transition(const Kernel &, Lit, bool marked,
                                        const char *source);
void event_trace_factor_update(const Kernel &, Lit grid_seed,
                               uint32_t ordinal, Lit, uint32_t score);
void event_trace_factor_grid(const Kernel &, Lit, uint32_t factors,
                             uint32_t remainders, int delta);
void event_trace_factor_grid_member(const Kernel &, uint32_t ordinal, Lit);
void event_trace_bve_candidate(const Kernel &, Var, const char *, uint64_t,
                               uint64_t, bool);
void event_trace_bve_occurrences(const Kernel &, Var, uint32_t pos_ordered,
                                 uint32_t neg_ordered,
                                 uint32_t pos_true_binary,
                                 uint32_t neg_true_binary,
                                 uint32_t pos_satisfied_large,
                                 uint32_t neg_satisfied_large,
                                 uint32_t pos_satisfied_effective_binary,
                                 uint32_t neg_satisfied_effective_binary,
                                 uint32_t pos_tautological,
                                 uint32_t neg_tautological);
void event_trace_bve_definition(const Kernel &, Var, uint64_t attempt,
                                uint32_t core, const char *outcome,
                                uint64_t ticks, uint64_t budget,
                                uint64_t generator, uint32_t variables,
                                uint32_t side0, uint32_t side1,
                                const uint32_t *ids, uint32_t size);
void event_trace_bve_plan(const Kernel &, Var, uint32_t side0, uint32_t side1,
                          uint32_t gate_kind, uint32_t gate0, uint32_t gate1,
                          uint64_t charged, bool resolve_gate);
void event_trace_bve_pair(const Kernel &, Var, uint32_t ordinal, const Lit *,
                          uint32_t, Ref, uint64_t, const Lit *, uint32_t, Ref,
                          uint64_t, const char *outcome);
void event_trace_bve_heap(const Kernel &, const char *stage, Var trigger,
                          Var subject, double score, uint32_t position,
                          Var root, uint32_t size);
void event_trace_bve_schedule(const Kernel &, const Var *members,
                              const double *scores, uint32_t size,
                              uint32_t bound, uint64_t insertion_hash);
void event_trace_bve_pop(const Kernel &, uint64_t ordinal,
                         uint64_t cumulative_hash, Var popped, double score,
                         Var root, uint32_t size);
void event_trace_eliminate_cleanup(const Kernel &, const char *stage);
#else
inline void event_trace_detail(const Kernel &, const char *, int64_t) {}
inline void event_trace_vivify_candidate(const Kernel &, const char *,
                                         uint64_t, Ref, uint64_t, uint64_t) {}
inline void event_trace_vivify_counts(const Kernel &, const uint32_t *,
                                      size_t) {}
inline void event_trace_watch_scan(const Kernel &, Lit) {}
inline void event_trace_watch_scan_post(const Kernel &, Lit) {}
inline void event_trace_watch_move(const Kernel &, Lit, Lit, Lit,
                                   const Clause *) {}
inline void event_trace_watch_cost(const Kernel &, Lit, uint64_t, uint64_t,
                                   uint64_t, uint64_t, uint64_t, uint64_t,
                                   uint64_t, uint64_t) {}
inline void event_trace_learned_stage(const Kernel &, const char *,
                                      const std::vector<Lit> &) {}
inline void event_trace_clause_glue(const Kernel &, const char *,
                                    const Clause *, uint32_t, uint32_t) {}
inline void event_trace_clause_used(const Kernel &, const Clause *, uint32_t) {}
inline void event_trace_retier(const Kernel &, uint32_t, const uint64_t *) {}
inline void event_trace_analyzed(const Kernel &, const std::vector<Var> &,
                                 const std::vector<uint8_t> &, uint8_t) {}
inline void event_trace_analyzed_post(const Kernel &,
                                      const std::vector<Var> &) {}
inline void event_trace_heap_scores(const Kernel &) {}
inline void event_trace_heap_insert(const Kernel &, Var, uint32_t, uint32_t) {}
inline void event_trace_heap_remove(const Kernel &, Var, Var, uint32_t,
                                    uint32_t, uint32_t) {}
inline void event_trace_heap_pop(const Kernel &, const char *, Var, Var,
                                 uint32_t, uint32_t) {}
inline void event_trace_binary_assignment(const Kernel &, Lit, Lit, Lit,
                                          bool) {}
inline void event_trace_binary_add(const Kernel &, Lit, Lit) {}
inline void event_trace_binary_lifecycle(const Kernel &, const char *, Lit,
                                         Lit, uint64_t, uint32_t,
                                         const char * = nullptr) {}
inline void event_trace_binary_collapse(const Kernel &, Ref, Lit, Lit) {}
inline const char *event_trace_rule_enter(const char *) { return nullptr; }
inline void event_trace_rule_leave(const char *) {}
inline void event_trace_randec_draw(const Kernel &, uint32_t, uint32_t, Var,
                                    bool) {}
inline void event_trace_randec_state(const Kernel &, const char *, uint64_t,
                                     uint64_t, uint64_t) {}
inline void event_trace_phase_pick(const Kernel &, Var, int8_t, int8_t,
                                   int8_t) {}
inline void event_trace_target_save(const Kernel &) {}
inline void event_trace_target_state(const Kernel &, const char *, uint32_t) {}
inline void event_trace_phase_save(const Kernel &, Lit) {}
inline void event_trace_slot_boundary(const Kernel &, const char *,
                                      const char *) {}
inline void event_trace_sweep_schedule(const Kernel &, Var, uint64_t,
                                       uint64_t, uint64_t, uint64_t) {}
inline void event_trace_sweep_seed(const Kernel &, Var) {}
inline void event_trace_sweep_environment(const Kernel &, Var, uint32_t,
                                          uint32_t, uint64_t) {}
inline void event_trace_sweep_environment_clause(const Kernel &, Var, uint32_t,
                                                 const Lit *, uint32_t) {}
inline void event_trace_sweep_query(const Kernel &, Var, uint32_t,
                                    const char *, Lit, Lit, int, uint64_t,
                                    uint64_t) {}
inline void event_trace_sweep_model(const Kernel &, Var, uint32_t, const Var *,
                                    const int8_t *, uint32_t) {}
inline void event_trace_sweep_effort(const Kernel &, uint32_t, uint64_t,
                                     uint64_t, uint64_t) {}
inline void event_trace_sweep_environment_var(const Kernel &, Var, uint32_t,
                                              Var, uint32_t) {}
inline void event_trace_sweep_binary_attempt(const Kernel &, Var, uint32_t,
                                             Lit, Lit, int32_t, int32_t,
                                             const char *) {}
inline void event_trace_sweep_watch(const Kernel &, Var, uint32_t, Lit) {}
inline void event_trace_sweep_equivalence(const Kernel &, Lit, Lit) {}
inline void event_trace_sweep_commit(const Kernel &, Lit, Lit, int8_t, int8_t,
                                     const char *) {}
inline void event_trace_sweep_rewrite_binary(const Kernel &, Lit, Lit,
                                             uint32_t, Lit) {}
inline void event_trace_sweep_rewrite_occurrence(const Kernel &, Lit, Lit,
                                                 uint32_t, bool, uint32_t) {}
inline void event_trace_sweep_unit(const Kernel &, Lit) {}
inline void event_trace_substitute_map(const Kernel &, Lit, Lit) {}
inline void event_trace_factor_seed(const Kernel &, Lit, uint32_t,
                                    uint32_t) {}
inline void event_trace_factor_remainder(const Kernel &, Lit, uint32_t, bool,
                                         const Lit *, uint32_t) {}
inline void event_trace_factor_candidate(const Kernel &, Lit, uint32_t,
                                         uint32_t, Lit, uint32_t, uint32_t) {}
inline void event_trace_factor_score_entry(const Kernel &, Lit, uint32_t, Lit,
                                           uint32_t, bool, Lit, uint64_t,
                                           const Clause *) {}
inline void event_trace_factor_literal_state(const Kernel &, Lit, bool, bool,
                                             int, uint32_t) {}
inline void event_trace_factor_literal_entry(const Kernel &, Lit, uint32_t,
                                             bool, Lit, uint64_t,
                                             const Clause *) {}
inline void event_trace_factor_candidate_source(const Kernel &, Lit, uint32_t,
                                                uint32_t, Lit, Lit,
                                                const Clause *,
                                                const Clause *) {}
inline void event_trace_factor_candidate_binary_source(
    const Kernel &, Lit, uint32_t, uint32_t, Lit, Lit) {}
inline void event_trace_factor_pair(const Kernel &, Lit, const char *) {}
inline void event_trace_factor_heap(const Kernel &, Lit, const char *, Lit,
                                    uint32_t, uint32_t, Lit, uint32_t,
                                    uint32_t, uint32_t) {}
inline void event_trace_factor_schedule(const Kernel &, uint32_t, Lit,
                                        uint32_t) {}
inline void event_trace_factor_mark_transition(const Kernel &, Lit, bool,
                                               const char *) {}
inline void event_trace_factor_update(const Kernel &, Lit, uint32_t, Lit,
                                      uint32_t) {}
inline void event_trace_factor_grid(const Kernel &, Lit, uint32_t, uint32_t,
                                    int) {}
inline void event_trace_factor_grid_member(const Kernel &, uint32_t, Lit) {}
inline void event_trace_bve_candidate(const Kernel &, Var, const char *,
                                      uint64_t, uint64_t, bool) {}
inline void event_trace_bve_occurrences(const Kernel &, Var, uint32_t,
                                        uint32_t, uint32_t, uint32_t,
                                        uint32_t, uint32_t, uint32_t,
                                        uint32_t, uint32_t, uint32_t) {}
inline void event_trace_bve_definition(const Kernel &, Var, uint64_t,
                                       uint32_t, const char *, uint64_t,
                                       uint64_t, uint64_t, uint32_t, uint32_t,
                                       uint32_t, const uint32_t *, uint32_t) {}
inline void event_trace_bve_plan(const Kernel &, Var, uint32_t, uint32_t,
                                 uint32_t, uint32_t, uint32_t, uint64_t,
                                 bool) {}
inline void event_trace_bve_pair(const Kernel &, Var, uint32_t, const Lit *,
                                 uint32_t, Ref, uint64_t, const Lit *,
                                 uint32_t, Ref, uint64_t, const char *) {}
inline void event_trace_bve_heap(const Kernel &, const char *, Var, Var,
                                 double, uint32_t, Var, uint32_t) {}
inline void event_trace_bve_schedule(const Kernel &, const Var *,
                                     const double *, uint32_t, uint32_t,
                                     uint64_t) {}
inline void event_trace_bve_pop(const Kernel &, uint64_t, uint64_t, Var,
                                double, Var, uint32_t) {}
inline void event_trace_eliminate_cleanup(const Kernel &, const char *) {}
#endif

} // namespace tactix
