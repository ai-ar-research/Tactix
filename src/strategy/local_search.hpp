#pragma once

#include <cstdint>
#include <vector>

namespace tactix::strategy {

// A structural model search can either finish with a verified model or leave
// its best complete assignment as search guidance.  Kinds stay distinct so a
// policy can tune, rotate, or compose them without comparing unlike objective
// functions (violated CSP edges, affine residuals, and CNF clauses).
enum class LocalSearchKind : uint8_t {
  DirectCsp,
  Affine,
  Parity,
  Onehot,
  BalancedSignature,
  Count,
};

constexpr size_t LOCAL_SEARCH_KIND_COUNT =
    static_cast<size_t>(LocalSearchKind::Count);

constexpr bool valid_local_search_kind(LocalSearchKind kind) {
  return static_cast<size_t>(kind) < LOCAL_SEARCH_KIND_COUNT;
}

struct LocalSearchHint {
  // Indexed by stable external variable identity.  Preprocessing may compact
  // internal variables after the hint is produced.
  std::vector<int8_t> external_phases;
  uint64_t objective = UINT64_MAX;
  bool objective_exact = false;

  bool available() const { return !external_phases.empty(); }
};

} // namespace tactix::strategy
