#pragma once
#include <cstdint>

namespace tactix::session {
enum class QuerySolveStatus : uint8_t {
  Complete,
  Unknown,
  Interrupted,
  RestorationRequired,
  InvalidState,
  InvalidModel,
  ProofFailure
};

struct QuerySolveResult {
  QuerySolveStatus status = QuerySolveStatus::InvalidState;
  int result = 0; // 10 SAT, 20 UNSAT; zero publishes no terminal theorem
};
} // namespace tactix::session
