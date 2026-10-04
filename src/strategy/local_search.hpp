#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace tactix::strategy {

// A structural model search can either finish with a verified model or leave
// its best partial or complete assignment as search guidance. Kinds stay distinct so a
// policy can tune, rotate, or compose them without comparing unlike objective
// functions (violated CSP edges, affine residuals, and CNF clauses).
enum class LocalSearchKind : uint8_t {
  DirectCsp,
  Affine,
  Parity,
  Onehot,
  BalancedSignature,
  GraphLabeling,
  Count,
};

constexpr size_t LOCAL_SEARCH_KIND_COUNT =
    static_cast<size_t>(LocalSearchKind::Count);

constexpr bool valid_local_search_kind(LocalSearchKind kind) {
  return static_cast<size_t>(kind) < LOCAL_SEARCH_KIND_COUNT;
}

// Public candidate payload: low eight bits name the producer, the remaining
// bits name one publication. Zero is reserved for an explicitly unseeded Walk.
// Old kind-only LocalSearchGuidance payloads remain aliases for the latest
// candidate; enumerated identities always bind an exact publication.
constexpr uint64_t LOCAL_SEARCH_GENERATION_MAX = UINT64_MAX >> 8;
constexpr LocalSearchKind local_search_candidate_kind(uint64_t id) {
  return static_cast<LocalSearchKind>(id & 255);
}
constexpr bool valid_local_search_candidate(uint64_t id) {
  return (id >> 8) && valid_local_search_kind(local_search_candidate_kind(id));
}

enum class LocalSearchEffortUnit : uint8_t {
  Unspecified, CspWork, AffineWork, CandidateChecks, SignatureSteps, GraphWork
};
struct LocalSearchEffort {
  uint64_t amount = 0;
  LocalSearchEffortUnit unit = LocalSearchEffortUnit::Unspecified;
};

struct LocalSearchCandidateInfo {
  uint64_t id = 0;
  uint64_t formula_touch_epoch = 0;
  uint64_t formula_root_assignments = 0;
  uint64_t external_variables = 0;
  uint64_t objective = UINT64_MAX;
  bool objective_exact = false;
  LocalSearchEffort effort;
};

struct LocalSearchHint : LocalSearchCandidateInfo {
  // Indexed by stable external variable identity.  Preprocessing may compact
  // internal variables after the hint is produced.
  std::vector<int8_t> external_phases;

  bool available() const { return !external_phases.empty(); }
};

} // namespace tactix::strategy
