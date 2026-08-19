// Disabled-by-default semantic event tracing for DefaultPolicy diagnostics.
// The TSV format uses permanent external variable identities so traces remain
// meaningful across variable compaction and can be normalized against a
// reference solver.  No file is opened and no state is scanned unless
// TACTIX_EVENT_TRACE names an output path.
#pragma once

#include "kernel/kernel.hpp"

#include <cstdint>
#include <cstdio>

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
  void watch_scan(const Kernel &, Lit propagated);
  void implications(const Kernel &, size_t trail_begin);
  void conflict(const Kernel &);
  void learned(const Kernel &, const std::vector<Lit> &);
  void learned_stage(const Kernel &, const char *, const std::vector<Lit> &);
  void analyzed(const Kernel &, const std::vector<Var> &,
                const std::vector<uint8_t> &, uint8_t bump_mask);
  void analyzed_post(const Kernel &, const std::vector<Var> &);
  void heap_scores(const Kernel &);
  void mode_clock(const Kernel &, const char *stage, uint64_t limit,
                  uint64_t anchor, uint64_t count);
  void randec_draw(const Kernel &, uint32_t raw, uint32_t slot, Var,
                   bool accepted);
  void randec_state(const Kernel &, const char *stage, uint64_t until,
                    uint64_t next, uint64_t sequence);
  void phase_pick(const Kernel &, Var, int8_t target, int8_t saved,
                  int8_t result);
  void target_save(const Kernel &);
  void phase_save(const Kernel &, Lit);
  void slot_boundary(const Kernel &, const char *name, const char *stage);
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
  void sweep_watch(const Kernel &, Var, uint32_t, Lit);
  void sweep_equivalence(const Kernel &, Lit a, Lit b);
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
  void factor_update(const Kernel &, Lit grid_seed, uint32_t ordinal, Lit,
                     uint32_t score);
  void factor_grid(const Kernel &, Lit, uint32_t factors,
                   uint32_t remainders, int delta);
  void factor_grid_member(const Kernel &, uint32_t ordinal, Lit);
  void bve_candidate(const Kernel &, Var);
  void checkpoint(const Kernel &);
  void reorder_begin(const Kernel &);
  void reorder_end(const Kernel &);
  void retier(const Kernel &, uint32_t mode, const uint64_t *used_glue);
  void reduction(const Kernel &, const std::vector<Ref> &, size_t victims);

private:
  void emit(const Kernel &, const char *, const char *, bool snapshot);
  bool want_detail(const Kernel &);

  std::FILE *out_ = nullptr;
  uint64_t sequence_ = 0;
  uint64_t emitted_ = 0;
  uint64_t limit_ = 50000;
  uint64_t post_reorder_details_ = 0;
  uint64_t checkpoint_bucket_ = 0;
  uint64_t detail_conflict_from_ = 0;
  uint64_t detail_conflict_to_ = UINT64_MAX;
  bool saw_reorder_ = false;
  bool detail_window_ = false;
  bool compact_probe_ = false;
  bool skip_watch_ = false;
  bool skip_imply_ = false;
  bool skip_heap_ = false;
  bool skip_decisions_ = false;
  bool skip_randec_draws_ = false;
  bool skip_analysis_ticks_ = false;
  bool search_only_ = false;
  bool control_only_ = false;
  bool truncated_ = false;
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
  void watch_scan(const Kernel &, Lit) {}
  void implications(const Kernel &, size_t) {}
  void conflict(const Kernel &) {}
  void learned(const Kernel &, const std::vector<Lit> &) {}
  void learned_stage(const Kernel &, const char *, const std::vector<Lit> &) {}
  void analyzed(const Kernel &, const std::vector<Var> &,
                const std::vector<uint8_t> &, uint8_t) {}
  void analyzed_post(const Kernel &, const std::vector<Var> &) {}
  void heap_scores(const Kernel &) {}
  void mode_clock(const Kernel &, const char *, uint64_t, uint64_t,
                  uint64_t) {}
  void randec_draw(const Kernel &, uint32_t, uint32_t, Var, bool) {}
  void phase_pick(const Kernel &, Var, int8_t, int8_t, int8_t) {}
  void target_save(const Kernel &) {}
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
  void sweep_watch(const Kernel &, Var, uint32_t, Lit) {}
  void sweep_equivalence(const Kernel &, Lit, Lit) {}
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
  void factor_update(const Kernel &, Lit, uint32_t, Lit, uint32_t) {}
  void factor_grid(const Kernel &, Lit, uint32_t, uint32_t, int) {}
  void factor_grid_member(const Kernel &, uint32_t, Lit) {}
  void bve_candidate(const Kernel &, Var) {}
  void checkpoint(const Kernel &) {}
  void reorder_begin(const Kernel &) {}
  void reorder_end(const Kernel &) {}
  void retier(const Kernel &, uint32_t, const uint64_t *) {}
  void reduction(const Kernel &, const std::vector<Ref> &, size_t) {}
};
#endif

#ifdef TACTIX_ENABLE_EVENT_TRACE
void event_trace_detail(const Kernel &, const char *event, int64_t payload);
void event_trace_watch_scan(const Kernel &, Lit propagated);
void event_trace_learned_stage(const Kernel &, const char *,
                               const std::vector<Lit> &);
void event_trace_retier(const Kernel &, uint32_t mode,
                        const uint64_t *used_glue);
void event_trace_analyzed(const Kernel &, const std::vector<Var> &,
                          const std::vector<uint8_t> &, uint8_t bump_mask);
void event_trace_analyzed_post(const Kernel &, const std::vector<Var> &);
void event_trace_heap_scores(const Kernel &);
void event_trace_randec_draw(const Kernel &, uint32_t raw, uint32_t slot, Var,
                             bool accepted);
void event_trace_randec_state(const Kernel &, const char *stage,
                              uint64_t until, uint64_t next,
                              uint64_t sequence);
void event_trace_phase_pick(const Kernel &, Var, int8_t target, int8_t saved,
                            int8_t result);
void event_trace_target_save(const Kernel &);
void event_trace_phase_save(const Kernel &, Lit);
void event_trace_slot_boundary(const Kernel &, const char *name,
                               const char *stage);
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
                              uint64_t kitten_ticks, uint64_t entry_ticks,
                              uint64_t solves);
void event_trace_sweep_environment_var(const Kernel &, Var, uint32_t, Var,
                                       uint32_t);
void event_trace_sweep_watch(const Kernel &, Var, uint32_t, Lit);
void event_trace_sweep_equivalence(const Kernel &, Lit a, Lit b);
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
void event_trace_factor_update(const Kernel &, Lit grid_seed,
                               uint32_t ordinal, Lit, uint32_t score);
void event_trace_factor_grid(const Kernel &, Lit, uint32_t factors,
                             uint32_t remainders, int delta);
void event_trace_factor_grid_member(const Kernel &, uint32_t ordinal, Lit);
void event_trace_bve_candidate(const Kernel &, Var);
#else
inline void event_trace_detail(const Kernel &, const char *, int64_t) {}
inline void event_trace_watch_scan(const Kernel &, Lit) {}
inline void event_trace_learned_stage(const Kernel &, const char *,
                                      const std::vector<Lit> &) {}
inline void event_trace_retier(const Kernel &, uint32_t, const uint64_t *) {}
inline void event_trace_analyzed(const Kernel &, const std::vector<Var> &,
                                 const std::vector<uint8_t> &, uint8_t) {}
inline void event_trace_analyzed_post(const Kernel &,
                                      const std::vector<Var> &) {}
inline void event_trace_heap_scores(const Kernel &) {}
inline void event_trace_randec_draw(const Kernel &, uint32_t, uint32_t, Var,
                                    bool) {}
inline void event_trace_randec_state(const Kernel &, const char *, uint64_t,
                                     uint64_t, uint64_t) {}
inline void event_trace_phase_pick(const Kernel &, Var, int8_t, int8_t,
                                   int8_t) {}
inline void event_trace_target_save(const Kernel &) {}
inline void event_trace_phase_save(const Kernel &, Lit) {}
inline void event_trace_slot_boundary(const Kernel &, const char *,
                                      const char *) {}
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
inline void event_trace_sweep_watch(const Kernel &, Var, uint32_t, Lit) {}
inline void event_trace_sweep_equivalence(const Kernel &, Lit, Lit) {}
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
inline void event_trace_factor_update(const Kernel &, Lit, uint32_t, Lit,
                                      uint32_t) {}
inline void event_trace_factor_grid(const Kernel &, Lit, uint32_t, uint32_t,
                                    int) {}
inline void event_trace_factor_grid_member(const Kernel &, uint32_t, Lit) {}
inline void event_trace_bve_candidate(const Kernel &, Var) {}
#endif

} // namespace tactix
