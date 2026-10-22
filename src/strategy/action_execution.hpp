#pragma once

// Typed native execution shared by policy schedulers. Requests describe the
// transition; scheduling, target selection, heuristic advice and clocks stay
// with the calling policy. These inline paths also serve static hot dispatch.
#include "rules/decide/decide.hpp"
#include "rules/restart/restart.hpp"

namespace tactix::execution {

// End the original-input certificate opportunity before ordinary actions.
// Explicit startup instances remain responsible for their own publication.
struct CloseStartupCertificateRequest {};

inline void execute_action(Kernel &k, CloseStartupCertificateRequest) {
  k.close_startup_certificate();
  if (k.proof && k.proof->startup_pb_available())
    k.proof->commit_clause_stream();
}

// Per-solve lifecycle releases. None changes the formula or the assignment;
// each returns whether it had state to release.
// The graph-labeling session restarts authentication for a new solve.
struct ResetGraphLabelingSessionRequest {};
// The parser's retained input-binary order serves only the original-input
// DirectCsp opportunity (onehot_model.cpp); later opportunities read watches.
struct ReleaseInputBinaryOrderRequest {};
// Preprocessing consumed the propagated root prefix (Kissat clears it
// physically); Reduce preparation then flushes only roots derived later.
struct ConsumeRootPrefixRequest {};

inline bool execute_action(Kernel &k, ResetGraphLabelingSessionRequest) {
  k.graph_labeling_session.clear();
  return true;
}
inline bool execute_action(Kernel &k, ReleaseInputBinaryOrderRequest) {
  if (!k.input_binary_order_captured) return false;
  k.input_binary_order.clear();
  k.input_binary_order.shrink_to_fit();
  k.input_binary_order_captured = false;
  return true;
}
inline bool execute_action(Kernel &k, ConsumeRootPrefixRequest) {
  const bool pending = k.unflushed_root != 0;
  k.unflushed_root = 0;
  return pending;
}

enum class RestartPurpose : uint8_t { Scheduled, Preparation };

struct RestartRequest {
  uint32_t keep_level = 0;
  // Preparing a root-only operation abandons the trail without consuming a
  // scheduled-restart occasion. Preserve that distinction in native Stats.
  RestartPurpose purpose = RestartPurpose::Scheduled;
};

enum class DecisionOrigin : uint8_t { Policy, PublicAPI };

struct DecisionRequest {
  Lit literal = INVALID_LIT;
  DecisionOrigin origin = DecisionOrigin::Policy;
};

inline ApplyResult execute_action(Kernel &k, const RestartRequest &request) {
  return request.purpose == RestartPurpose::Scheduled
             ? restart_apply_to(k, request.keep_level)
             : restart_backtrack_to(k, request.keep_level);
}

inline ApplyResult execute_action(Kernel &k, const DecisionRequest &request) {
  return decide_apply(k, request.literal,
                      request.origin == DecisionOrigin::PublicAPI);
}

} // namespace tactix::execution
