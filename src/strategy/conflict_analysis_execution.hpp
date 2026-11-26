#pragma once

#include "policies/heuristics.hpp"
#include "rules/analyze/analyze.hpp"
#include "rules/analyze/conflict_er.hpp"
#include "rules/backjump/backjump.hpp"

#include <string>

namespace tactix::execution {

enum class ConflictErMode : uint8_t { Off, Eager, Automatic, Substitute, Nominate };

constexpr uint64_t chrono_analysis_instance(ConflictErMode mode) {
  switch (mode) {
  case ConflictErMode::Eager: return ANALYZE_TARGET_CHRONO_ER;
  case ConflictErMode::Automatic: return ANALYZE_TARGET_CHRONO_ER_AUTO;
  case ConflictErMode::Substitute: return ANALYZE_TARGET_CHRONO_ER_SUBSTITUTE;
  case ConflictErMode::Nominate: return ANALYZE_TARGET_CHRONO_ER_NOMINATE;
  case ConflictErMode::Off: return ANALYZE_TARGET_CHRONO;
  }
  return ANALYZE_TARGET_CHRONO;
}

constexpr bool substitutes_extensions(ConflictErMode mode) {
  return mode != ConflictErMode::Off && mode != ConflictErMode::Nominate;
}

inline bool conflict_er_factor_policy_batch(Kernel &k, bool consume_seeds,
                                            bool before_first_elimination) {
  if (!before_first_elimination && !k.stats.conflict_er_definitions)
    return false;
  const Var first_output = k.nvars;
  const bool attempted = conflict_er_factor_learned_pairs(k, consume_seeds);
  if (first_output < k.nvars)
    heuristics_demote_fresh_vars(k, first_output, k.nvars);
  return attempted;
}

// The per-conflict payload: the bump payload is installed for this analysis and
// restored to neutral (limit 0, depth 1) after; the retier schedule applies at
// every OTFS conflict and returns updated. Chrono is public
// ANALYZE_TARGET_CHRONO; Advised keeps the caller's target.
struct NativeConflictAnalysisRequest {
  uint32_t bump_reason_limit = 0, bump_reason_depth = 1;
  AnalysisTarget target_mode = AnalysisTarget::Advised;
  RetierSchedule retier_schedule{};
  ConflictErMode conflict_er = ConflictErMode::Off;
};
// Public requests: a pre-set target or the chronological one after learning.
struct ExplicitConflictAnalysisRequest {
  uint32_t target_level = 0;
};
struct ChronoConflictAnalysisRequest {
  ConflictErMode conflict_er = ConflictErMode::Off;
};

struct ConflictAnalysisExecutionResult {
  ApplyResult analysis;
  ApplyResult result;
  bool backjumped;
  // Realized backjump and jump levels, replayable as public ConflictAnalysis.
  uint32_t target = 0, jump = 0;
  // The native request's schedule after the analysis.
  RetierSchedule retier_schedule{};
};

inline ApplyResult assert_learned(Kernel &k, uint32_t target, ConflictErMode mode) {
  const ApplyResult result = backjump_apply_to(k, target);
  if (result == ApplyResult::Ok && (mode == ConflictErMode::Eager ||
      mode == ConflictErMode::Automatic || mode == ConflictErMode::Nominate))
    conflict_er_observe(k, k.pending.lits, mode != ConflictErMode::Eager);
  return result;
}

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
    return analyze_apply_native(k, request.target_mode, retier, &chosen,
                                substitutes_extensions(request.conflict_er));
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
  out.result = assert_learned(k, target, request.conflict_er);
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

inline ApplyResult execute_action(Kernel &k, ChronoConflictAnalysisRequest request) {
  uint32_t chosen = 0;
  const ApplyResult analysis =
      analyze_apply_chrono(k, &chosen, substitutes_extensions(request.conflict_er));
  if (analysis != ApplyResult::Ok || !k.pending.valid)
    return analysis;
  return assert_learned(k, chosen, request.conflict_er);
}

} // namespace tactix::execution
