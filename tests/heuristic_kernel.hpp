#pragma once

#include "policies/heuristics.hpp"

// Attach before reserving variables, as a parser does. The owner detaches
// before the Kernel is destroyed; keeping both in one fixture prevents a
// dangling heuristic pointer. ScopedHeuristics makes the fixture noncopyable.
class HeuristicKernel : public tactix::Kernel {
public:
  explicit HeuristicKernel(uint32_t variables) : heuristics_(*this) {
    tactix::kernel_reserve_vars(*this, variables);
  }

private:
  tactix::ScopedHeuristics heuristics_;
};
