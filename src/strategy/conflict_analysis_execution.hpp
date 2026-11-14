#pragma once

#include "rules/analyze/analyze.hpp"
#include "rules/backjump/backjump.hpp"

#include <string>

namespace tactix::execution {

// The per-conflict payload: the bump payload is installed for this analysis and
// restored to neutral (limit 0, depth 1) after; the retier schedule applies at
// every OTFS conflict and returns updated. Chrono is public
// ANALYZE_TARGET_CHRONO; Advised keeps the caller's target.
struct NativeConflictAnalysisRequest {
  uint32_t bump_reason_limit = 0, bump_reason_depth = 1;
  AnalysisTarget target_mode = AnalysisTarget::Advised;
  RetierSchedule retier_schedule{};
};
// Public requests: a pre-set target or the chronological one after learning.
struct ExplicitConflictAnalysisRequest {
  uint32_t target_level = 0;
};
struct ChronoConflictAnalysisRequest {};

struct ConflictAnalysisExecutionResult {
  ApplyResult analysis;
  ApplyResult result;
  bool backjumped;
  // Realized backjump and jump levels, replayable as public ConflictAnalysis.
  uint32_t target = 0, jump = 0;
  // The native request's schedule after the analysis.
  RetierSchedule retier_schedule{};
};

// Schedule feedback text, comma separated: enabled, deadline, interval, maximum
// interval, growth, tier defaults, relative permilles, focused and stable
// tier1/tier2 snapshots.
inline std::string retier_schedule_text(const RetierSchedule &s) {
  std::string out;
  for (const uint64_t value :
       {uint64_t{s.enabled}, s.next_conflicts, s.interval, s.max_interval,
        uint64_t{s.growth}, uint64_t{s.tier1_default}, uint64_t{s.tier2_default},
        uint64_t{s.tier1_relative_permille}, uint64_t{s.tier2_relative_permille},
        uint64_t{s.tier1_by_mode[0]}, uint64_t{s.tier2_by_mode[0]},
        uint64_t{s.tier1_by_mode[1]}, uint64_t{s.tier2_by_mode[1]}})
    out += (out.empty() ? "" : ",") + std::to_string(value);
  return out;
}

// One atomic M3 transaction; advice is an internal scheduler seam (its target
// used only in Advised mode), and only the completed transaction reaches
// external feedback.
template <class Instrumentation, class Advice>
ConflictAnalysisExecutionResult
execute_action(Kernel &k, const NativeConflictAnalysisRequest &request,
               Instrumentation &&instrument, Advice &&advise) {
  ConflictAnalysisExecutionResult out{};
  out.retier_schedule = request.retier_schedule;
  RetierSchedule *const retier =
      out.retier_schedule.enabled ? &out.retier_schedule : nullptr;
  uint32_t chosen = 0;
  k.bump_reason_limit = request.bump_reason_limit;
  k.bump_reason_depth = request.bump_reason_depth;
  const ApplyResult analysis = instrument([&] {
    return analyze_apply_native(k, request.target_mode, retier, &chosen);
  });
  k.bump_reason_limit = 0;
  k.bump_reason_depth = 1;
  const uint32_t advised = advise(analysis);
  out.analysis = out.result = analysis;
  if (analysis != ApplyResult::Ok || !k.pending.valid)
    return out;
  const uint32_t target =
      request.target_mode == AnalysisTarget::Chrono ? chosen : advised;
  out.jump = k.pending.jump_level;
  out.result = backjump_apply_to(k, target);
  out.backjumped = true;
  out.target = k.level;
  return out;
}

inline ApplyResult execute_action(
    Kernel &k, const ExplicitConflictAnalysisRequest &request) {
  const ApplyResult analysis = analyze_apply_with_target(k, request.target_level);
  if (analysis != ApplyResult::Ok || !k.pending.valid)
    return analysis;
  return backjump_apply_to(k, request.target_level);
}

inline ApplyResult execute_action(Kernel &k, ChronoConflictAnalysisRequest) {
  uint32_t chosen = 0;
  const ApplyResult analysis = analyze_apply_chrono(k, &chosen);
  if (analysis != ApplyResult::Ok || !k.pending.valid)
    return analysis;
  return backjump_apply_to(k, chosen);
}

} // namespace tactix::execution
