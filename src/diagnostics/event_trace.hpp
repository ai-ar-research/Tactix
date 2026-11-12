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
  void implications(const Kernel &, size_t trail_begin);
  void conflict(const Kernel &);
  void learned(const Kernel &, const std::vector<Lit> &);
  void heap_after_decision(const Kernel &);
  void mode_clock(const Kernel &, const char *stage, uint64_t limit,
                  uint64_t anchor, uint64_t count);
  void randec_draw(const Kernel &, uint32_t raw, uint32_t slot, Var,
                   bool accepted);
  void slot_boundary(const Kernel &, const char *name, const char *stage);
  void bve_candidate(const Kernel &, Var, const char *stage, uint64_t ticks,
                     uint64_t limit, bool applied);
  void bve_schedule(const Kernel &, const Var *members, const double *scores,
                    uint32_t size, uint32_t bound,
                    uint64_t insertion_hash);
  void formula_checkpoint(const Kernel &);
  void checkpoint(const Kernel &);
  void reorder_begin(const Kernel &);
  void reorder_end(const Kernel &);
  void reduction(const Kernel &, const std::vector<Ref> &, size_t victims);

  // The pure-forwarder events (event_trace_forwarders.inc).
#define TACTIX_TRACE_FORWARD(name, parameters, arguments) void name parameters;
#include "diagnostics/event_trace_forwarders.inc"

private:
  void emit(const Kernel &, const char *, const char *, bool snapshot);
  // Emits a non-snapshot record whose payload concatenates `parts`.
  template <class... Parts>
  void emit_parts(const Kernel &, const char *event, const Parts &...parts);
  void emit_heap_scores(const Kernel &);
  bool heap_var_selected(const Kernel &, Var) const;
  // Whether a stable-heap event may be traced at all.
  bool heap_event_admitted(const Kernel &) const;
  bool want_detail(const Kernel &);
  int selected_sweep_seed(const Kernel &, Var seed, const char *environment);
  // Whether the explicit conflict window, if any, holds the current conflict.
  bool in_detail_window(const Kernel &k) const {
    return !detail_window_ || (k.stats.conflicts >= detail_conflict_from_ &&
                               k.stats.conflicts <= detail_conflict_to_);
  }

  std::FILE *out_ = nullptr;
  uint64_t sequence_ = 0;
  uint64_t emitted_ = 0;
  uint64_t limit_ = 50000;
  uint64_t post_reorder_details_ = 0;
  uint64_t checkpoint_bucket_ = 0;
  uint64_t detail_conflict_from_ = 0;
  uint64_t detail_conflict_to_ = UINT64_MAX;
  uint64_t stop_after_conflict_ = UINT64_MAX;
  uint64_t heap_remove_decision_ = UINT64_MAX;
  uint64_t exit_conflict_ = UINT64_MAX;
  bool saw_reorder_ = false;
  bool detail_window_ = false;
  bool compact_probe_ = false;
  bool boundary_only_ = false;
  bool target_only_ = false;
  std::string exit_match_;
  bool skip_heap_ = false;
  bool skip_randec_draws_ = false;
  bool skip_analysis_ticks_ = false;
  bool keep_snapshots_ = false;
  bool search_only_ = false;
  bool glue_only_ = false;
  bool formula_only_ = false;
  bool trajectory_only_ = false;
  bool learned_only_ = false;
  bool imply_only_ = false;
  bool factor_fresh_only_ = false;
  bool factor_update_only_ = false;
  bool factor_literal_only_ = false;
  bool factor_seed_only_ = false;
  bool clause_lifecycle_only_ = false;
  std::vector<int> imply_literals_;
  bool bve_pop_only_ = false;
  bool conflict_only_ = false;
  bool sweep_only_ = false;
  bool watch_only_ = false;
  bool watch_only_strict_ = false;
  bool watch_cost_ = false;
  bool search_watch_cost_only_ = false;
  bool watch_within_vivify_ = false;
  bool vivify_watch_active_ = false;
  uint64_t vivify_watch_ordinal_ = 0;
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
  void close() {}
  void macro(const Kernel &, const char *, const char * = "-") {}
  void detail(const Kernel &, const char *, int64_t = 0) {}
  void implications(const Kernel &, size_t) {}
  void conflict(const Kernel &) {}
  void learned(const Kernel &, const std::vector<Lit> &) {}
  void heap_after_decision(const Kernel &) {}
  void heap_scores(const Kernel &) {}
  void mode_clock(const Kernel &, const char *, uint64_t, uint64_t,
                  uint64_t) {}
  void checkpoint(const Kernel &) {}
  void reorder_begin(const Kernel &) {}
  void reorder_end(const Kernel &) {}
  void reduction(const Kernel &, const std::vector<Ref> &, size_t) {}
};
#endif

#ifdef TACTIX_ENABLE_EVENT_TRACE
void event_trace_detail(const Kernel &, const char *event, int64_t payload);
void event_trace_randec_draw(const Kernel &, uint32_t raw, uint32_t slot, Var,
                             bool accepted);
void event_trace_slot_boundary(const Kernel &, const char *name,
                               const char *stage);
// Memory-only checkpoints must not fire semantic boundary selectors.
void event_trace_memory_checkpoint(const Kernel &, const char *name,
                                   const char *stage, size_t temporary_bytes = 0);
void event_trace_bve_candidate(const Kernel &, Var, const char *, uint64_t,
                               uint64_t, bool);
void event_trace_bve_definition(const Kernel &, Var, uint64_t attempt,
                                uint32_t core, const char *outcome,
                                uint64_t ticks, uint64_t budget,
                                uint64_t generator, uint32_t variables,
                                uint32_t side0, uint32_t side1,
                                const uint32_t *ids, uint32_t size);
void event_trace_bve_schedule(const Kernel &, const Var *members,
                              const double *scores, uint32_t size,
                              uint32_t bound, uint64_t insertion_hash);
// Each forwards to the active trace's member of the same name.
#define TACTIX_TRACE_FORWARD(name, parameters, arguments)                     \
  void event_trace_##name parameters;
#include "diagnostics/event_trace_forwarders.inc"
#else
inline void event_trace_detail(const Kernel &, const char *, int64_t) {}
inline void event_trace_vivify_counts(const Kernel &, const uint32_t *,
                                      size_t) {}
inline void event_trace_watch_scan(const Kernel &, Lit) {}
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
inline void event_trace_retier(const Kernel &, const RetierSchedule &,
                               uint32_t, const uint64_t *) {}
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
inline void event_trace_binary_add(const Kernel &, Lit, Lit) {}
inline void event_trace_randec_draw(const Kernel &, uint32_t, uint32_t, Var,
                                    bool) {}
inline void event_trace_randec_state(const Kernel &, const char *, uint64_t,
                                     uint64_t, uint64_t, uint64_t) {}
inline void event_trace_phase_pick(const Kernel &, Var, int8_t, int8_t,
                                   int8_t) {}
inline void event_trace_target_save(const Kernel &) {}
inline void event_trace_slot_boundary(const Kernel &, const char *,
                                      const char *) {}
inline void event_trace_bve_definition(const Kernel &, Var, uint64_t,
                                       uint32_t, const char *, uint64_t,
                                       uint64_t, uint64_t, uint32_t, uint32_t,
                                       uint32_t, const uint32_t *, uint32_t) {}
inline void event_trace_bve_plan(const Kernel &, Var, uint32_t, uint32_t,
                                 uint32_t, uint32_t, uint32_t, uint64_t,
                                 bool) {}
inline void event_trace_bve_schedule(const Kernel &, const Var *,
                                     const double *, uint32_t, uint32_t,
                                     uint64_t) {}
inline void event_trace_bve_pop(const Kernel &, uint64_t, uint64_t, Var,
                                double, Var, uint32_t) {}
inline void event_trace_eliminate_cleanup(const Kernel &, const char *) {}
#endif

} // namespace tactix
