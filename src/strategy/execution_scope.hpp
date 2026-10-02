#pragma once

#include "kernel/kernel.hpp"

namespace tactix::execution {

enum class ExecutionContext : uint8_t {
  Startup,      // exclude work from search ticks, preserve phase saving
  Inprocessing, // exclude search ticks and suppress speculative phase saving
  Speculation,  // suppress phase saving without changing work attribution
};

// Mechanical wires are shared; policies choose the context and scope lifetime.
// Restore only the fields owned by that context, including nested scopes.
template <ExecutionContext Context> class ExecutionScope {
public:
  explicit ExecutionScope(Kernel &k)
      : kernel_(k), previous_inprocessing_(k.inprocessing),
        previous_phase_saving_(k.phase_saving_suppressed) {
    if constexpr (Context != ExecutionContext::Speculation)
      kernel_.inprocessing = true;
    if constexpr (Context != ExecutionContext::Startup)
      kernel_.phase_saving_suppressed = true;
  }
  ~ExecutionScope() {
    if constexpr (Context != ExecutionContext::Startup)
      kernel_.phase_saving_suppressed = previous_phase_saving_;
    if constexpr (Context != ExecutionContext::Speculation)
      kernel_.inprocessing = previous_inprocessing_;
  }
  ExecutionScope(const ExecutionScope &) = delete;
  ExecutionScope &operator=(const ExecutionScope &) = delete;

private:
  Kernel &kernel_;
  bool previous_inprocessing_;
  bool previous_phase_saving_;
};

} // namespace tactix::execution
