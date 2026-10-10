#pragma once

#include "rules/analyze/analyze.hpp"
#include "rules/backjump/backjump.hpp"

#include <string>

namespace tactix::execution {

// The native per-conflict payload, by value. The executor installs the bump
// payload for this analysis only and restores the neutral values (limit 0,
// depth 1) afterwards; the retier schedule is applied at every OTFS conflict
// and returned updated. Chrono is the public ANALYZE_TARGET_CHRONO instance
// (DefaultPolicy's); Advised keeps the caller's target after learning.
struct NativeConflictAnalysisRequest {
  uint32_t bump_reason_limit = 0, bump_reason_depth = 1;
  AnalysisTarget target_mode = AnalysisTarget::Advised;
  RetierSchedule retier_schedule{};
};
// Public requests: a target supplied before analysis, or the chronological
// target after learning, under the installed profile and retier schedule.
struct ExplicitConflictAnalysisRequest {
  uint32_t target_level = 0;
};
struct ChronoConflictAnalysisRequest {};

struct ConflictAnalysisExecutionResult {
  ApplyResult analysis;
  ApplyResult result;
  bool backjumped;
  // Realized backjump: the level reached and the learned clause's jump level.
  // Feedback records these so the native target is replayable as the public
  // ConflictAnalysis instance; they are meaningful only when backjumped.
  uint32_t target = 0, jump = 0;
  // The native request's schedule after the analysis.
  RetierSchedule retier_schedule{};
};

// Feedback text of a schedule: enabled, deadline, interval, maximum interval,
// growth, tier defaults, relative permilles, then the focused and stable
// tier1/tier2 snapshots, comma separated.
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

// One atomic M3 transaction. Instrumentation can preserve the native journal's
// analysis-only scope; advice is an internal scheduler seam, never a learner
// selection or observation boundary, and its target is used only in Advised
// mode. Only the completed transaction may be published to external
// feedback. Static templates avoid hot-path type erasure.
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
