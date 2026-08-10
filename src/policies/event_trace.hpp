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
  void watch_scan(const Kernel &, Lit propagated);
  void implications(const Kernel &, size_t trail_begin);
  void conflict(const Kernel &);
  void learned(const Kernel &, const std::vector<Lit> &);
  void learned_stage(const Kernel &, const char *, const std::vector<Lit> &);
  void analyzed(const Kernel &, const std::vector<Var> &,
                const std::vector<uint8_t> &, uint8_t bump_mask);
  void mode_clock(const Kernel &, const char *stage, uint64_t limit,
                  uint64_t anchor, uint64_t count);
  void randec_draw(const Kernel &, uint32_t raw, uint32_t slot, Var,
                   bool accepted);
  void phase_pick(const Kernel &, Var, int8_t target, int8_t saved,
                  int8_t result);
  void target_save(const Kernel &);
  void phase_save(const Kernel &, Lit);
  void slot_boundary(const Kernel &, const char *name, const char *stage);
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
  void watch_scan(const Kernel &, Lit) {}
  void implications(const Kernel &, size_t) {}
  void conflict(const Kernel &) {}
  void learned(const Kernel &, const std::vector<Lit> &) {}
  void learned_stage(const Kernel &, const char *, const std::vector<Lit> &) {}
  void analyzed(const Kernel &, const std::vector<Var> &,
                const std::vector<uint8_t> &, uint8_t) {}
  void mode_clock(const Kernel &, const char *, uint64_t, uint64_t,
                  uint64_t) {}
  void randec_draw(const Kernel &, uint32_t, uint32_t, Var, bool) {}
  void phase_pick(const Kernel &, Var, int8_t, int8_t, int8_t) {}
  void target_save(const Kernel &) {}
  void phase_save(const Kernel &, Lit) {}
  void slot_boundary(const Kernel &, const char *, const char *) {}
  void checkpoint(const Kernel &) {}
  void reorder_begin(const Kernel &) {}
  void reorder_end(const Kernel &) {}
  void retier(const Kernel &, uint32_t, const uint64_t *) {}
  void reduction(const Kernel &, const std::vector<Ref> &, size_t) {}
};
#endif

#ifdef TACTIX_ENABLE_EVENT_TRACE
void event_trace_watch_scan(const Kernel &, Lit propagated);
void event_trace_learned_stage(const Kernel &, const char *,
                               const std::vector<Lit> &);
void event_trace_retier(const Kernel &, uint32_t mode,
                        const uint64_t *used_glue);
void event_trace_analyzed(const Kernel &, const std::vector<Var> &,
                          const std::vector<uint8_t> &, uint8_t bump_mask);
void event_trace_randec_draw(const Kernel &, uint32_t raw, uint32_t slot, Var,
                             bool accepted);
void event_trace_phase_pick(const Kernel &, Var, int8_t target, int8_t saved,
                            int8_t result);
void event_trace_target_save(const Kernel &);
void event_trace_phase_save(const Kernel &, Lit);
void event_trace_slot_boundary(const Kernel &, const char *name,
                               const char *stage);
#else
inline void event_trace_watch_scan(const Kernel &, Lit) {}
inline void event_trace_learned_stage(const Kernel &, const char *,
                                      const std::vector<Lit> &) {}
inline void event_trace_retier(const Kernel &, uint32_t, const uint64_t *) {}
inline void event_trace_analyzed(const Kernel &, const std::vector<Var> &,
                                 const std::vector<uint8_t> &, uint8_t) {}
inline void event_trace_randec_draw(const Kernel &, uint32_t, uint32_t, Var,
                                    bool) {}
inline void event_trace_phase_pick(const Kernel &, Var, int8_t, int8_t,
                                   int8_t) {}
inline void event_trace_target_save(const Kernel &) {}
inline void event_trace_phase_save(const Kernel &, Lit) {}
inline void event_trace_slot_boundary(const Kernel &, const char *,
                                      const char *) {}
#endif

} // namespace tactix
