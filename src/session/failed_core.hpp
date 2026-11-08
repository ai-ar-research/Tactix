#pragma once
#include "session/derivation_history.hpp"
#include "session/retained_clauses.hpp"
#include <cstddef>

namespace tactix {
struct Kernel;
namespace session {

struct CoreLimits {
  RetentionLimits proof;
  uint64_t ticks = 0; // zero keeps the already-refuted complete query cube
};

struct CoreCandidate {
  std::vector<size_t> indices; // positions in the original/effective A vectors
  uint64_t ticks = 0;
  bool complete = false, exhausted = false, interrupted = false;
};

// Read-only CaDiCaL-style final reason-cone traversal. This candidate has no
// original-formula authority: restrictions and aliases need separate licensing.
CoreCandidate trace_failed_core(const Kernel &,std::span<const int32_t> effective,
    int32_t failed_literal,size_t failed_index,bool base_unsat,uint64_t ticks,
    const volatile sig_atomic_t *stop=nullptr);

enum class CoreKind : uint8_t { QueryCube, LicensedSubset };
struct FailedCore {
  std::vector<int32_t> assumptions; // exact signed original client coordinates
  std::vector<RetainedClause> evidence; // F-only dependency-ordered proof program
  CoreKind kind = CoreKind::QueryCube;
  uint64_t ticks = 0;
  bool traced = false, exhausted = false, interrupted = false;
};

// Caller has already established UNSAT for exact F+A and owns its projection.
// A tighter cube is exported only when F entails its negated disjunction.
// Failure of this optional admission retains the sound full original cube.
FailedCore prepare_failed_core(const Kernel &,const QueryHistory &,const QueryRecord &,
    std::span<const int32_t> effective,int32_t failed_literal,size_t failed_index,
    bool base_unsat,std::span<const DerivationRecord>,const CoreLimits &,
    const volatile sig_atomic_t *stop=nullptr);

} // namespace session
} // namespace tactix
