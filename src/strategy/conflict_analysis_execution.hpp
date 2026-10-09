#pragma once

#include "rules/analyze/analyze.hpp"
#include "rules/backjump/backjump.hpp"

namespace tactix::execution {

// Native scheduling chooses its target after learning. Public requests supply
// it before analysis, including conflict-reuse paths that need no Backjump.
struct NativeConflictAnalysisRequest {};
struct ExplicitConflictAnalysisRequest {
  uint32_t target_level = 0;
};

struct ConflictAnalysisExecutionResult {
  ApplyResult analysis;
  ApplyResult result;
  bool backjumped;
  // Realized backjump: the level reached and the learned clause's jump level.
  // Feedback records these so the native target is replayable as the public
  // ConflictAnalysis instance; they are meaningful only when backjumped.
  uint32_t target = 0, jump = 0;
};

inline ApplyResult execute_analysis_step(Kernel &k,
                                       NativeConflictAnalysisRequest) {
  return analyze_apply(k);
}

inline ApplyResult execute_analysis_step(
    Kernel &k, const ExplicitConflictAnalysisRequest &request) {
  return analyze_apply_with_target(k, request.target_level);
}

// One atomic M3 transaction. Instrumentation can preserve the native journal's
// analysis-only scope; advice is an internal scheduler seam, never a learner
// selection or observation boundary. Only the completed transaction may be
// published to external feedback. Static templates avoid hot-path type erasure.
template <class Request, class Instrumentation, class Advice>
ConflictAnalysisExecutionResult execute_action(Kernel &k,
                                               const Request &request,
                                               Instrumentation &&instrument,
                                               Advice &&advise) {
  const ApplyResult analysis =
      instrument([&] { return execute_analysis_step(k, request); });
  const uint32_t target = advise(analysis);
  if (analysis != ApplyResult::Ok || !k.pending.valid)
    return {analysis, analysis, false};
  const uint32_t jump = k.pending.jump_level;
  const ApplyResult result = backjump_apply_to(k, target);
  return {analysis, result, true, k.level, jump};
}

inline ApplyResult execute_action(
    Kernel &k, const ExplicitConflictAnalysisRequest &request) {
  return execute_action(
             k, request, [](auto &&apply) { return apply(); },
             [&](ApplyResult) { return request.target_level; })
      .result;
}

} // namespace tactix::execution
