#pragma once

#include "kernel/kernel.hpp"
#include "rules/registry.hpp"
#include <memory>

namespace tactix {

enum class NolHypergraphSliceStatus : uint8_t {
  NoStructure,
  Pending,
  TrajectoryComplete,
  Sat,
  Interrupted,
};

enum class NolHypergraphAuthenticationStatus : uint8_t {
  NoStructure,
  Authenticated,
  Exhausted,
  Interrupted,
};

struct NolHypergraphAuthenticationResult {
  NolHypergraphAuthenticationStatus status =
      NolHypergraphAuthenticationStatus::NoStructure;
  uint64_t work = 0;
  bool recognized = false;
};

struct NolHypergraphSliceResult {
  ApplyResult result = ApplyResult::NoOp;
  NolHypergraphSliceStatus status = NolHypergraphSliceStatus::NoStructure;
  uint64_t work = 0;
  uint64_t objective_before = UINT64_MAX;
  uint64_t objective_after = UINT64_MAX;
  bool recognized = false;
  bool root_changed = false;
  bool hint_published = false;
  bool useful_hint = false;
};

class NolHypergraphWalkSession {
public:
  struct State;
  NolHypergraphWalkSession();
  ~NolHypergraphWalkSession();
  NolHypergraphWalkSession(const NolHypergraphWalkSession &) = delete;
  NolHypergraphWalkSession &operator=(const NolHypergraphWalkSession &) = delete;
  bool authenticated(const Kernel &) const;
  bool initialized() const;
  uint64_t steps() const;
  uint64_t trajectories() const;
  uint64_t candidate_fingerprint() const;
  void clear();
private:
  std::unique_ptr<State> state_;
  friend NolHypergraphAuthenticationResult
  nol_hypergraph_model_authenticate(Kernel &);
  friend NolHypergraphSliceResult nol_hypergraph_model_slice(Kernel &);
  friend ApplyResult nol_hypergraph_model_apply(Kernel &);
};

// SAT-only construction for the noL family: active variables occur in
// consecutive triples, one positive triple requires a color for each active
// vertex, and every negative triple forbids a monochromatic hyperedge.  The
// action retains the authenticated language and a bounded resumable private
// 3-color trajectory. Partial work offers phases only; SAT requires validation
// of the complete assignment against the whole current live CNF.
RuleEligibility nol_hypergraph_model_guard(const Kernel &);
NolHypergraphAuthenticationResult nol_hypergraph_model_authenticate(Kernel &);
NolHypergraphSliceResult nol_hypergraph_model_slice(Kernel &);
ApplyResult nol_hypergraph_model_apply(Kernel &);

} // namespace tactix
