#pragma once

#include "engine/subsolver.hpp"
#include "kernel/propagation_region.hpp"
#include "population/population_observer.hpp"
#include "rules/registry.hpp"
#include <array>
#include <span>

namespace tactix {

struct RegionQuery {
  int rule, literal;
  uint32_t region;
  uint64_t ticks;
};

// A solver-owned, opt-in inference session. Its observer follows actual clause
// installation/retirement; no caller-supplied clause is trusted as a premise.
class RegionSession final : public ForwardingPopulationObserver {
  Kernel &owner_;
  uint64_t binding_epoch_ = 0;
  int probe_, sweep_;
  struct Slice { uint32_t begin, end; };
  struct Definition { int output; Slice clauses; };
  std::vector<std::array<int32_t, 3>> clauses_;
  std::vector<Definition> definitions_;
  std::vector<int32_t> vertices_;
  std::vector<Slice> regions_;
  bool valid_ = true, capturing_ = false;
  uint32_t capture_begin_ = 0, epoch_ = 0;
  std::vector<uint32_t> stamps_, local_;
  std::vector<Var> mapped_;
  std::vector<SubLit> scratch_;
  SubSolver solver_;
  uint64_t work_ = 0;
  ScopedPopulationObserverBinding binding_;

  void invalidate();
  const Definition *definition(int) const;
  bool prepare(const RegionQuery &);
  bool compile(const RegionQuery &);
  ApplyResult infer(const RegionQuery &);
  ApplyResult unit(Lit);
public:
  explicit RegionSession(Kernel &);
  RegionSession(const RegionSession &) = delete;
  RegionSession &operator=(const RegionSession &) = delete;
  void begin_definition();
  void end_definition(int output);
  int add_region(std::span<const int32_t>);
  bool valid_query(const RegionQuery &) const;
  ApplyResult apply(const RegionQuery &);

private:
  void intercept(const SolverPopulationEvent &) noexcept override;
  void intercept(const SolverReplacementEvent &) noexcept override;
};
} // namespace tactix
