#pragma once

#include "kernel/kernel.hpp"

namespace tactix::execution {

struct ArenaOrderingInstallationRequest {
  ArenaOrdering *ordering;
};
inline void execute_action(Kernel &k, const ArenaOrderingInstallationRequest &request) {
  k.arena_ordering = request.ordering;
}

// The policy chooses the ordering and solve lifetime. Restore the caller's
// exact wire, including nested null installations and early terminal exits.
class ArenaOrderingScope {
public:
  ArenaOrderingScope(Kernel &k, ArenaOrderingInstallationRequest request)
      : kernel_(k), saved_(k.arena_ordering) { execute_action(k, request); }
  ~ArenaOrderingScope() {
    execute_action(kernel_, ArenaOrderingInstallationRequest{saved_});
  }
  ArenaOrderingScope(const ArenaOrderingScope &) = delete;
  ArenaOrderingScope &operator=(const ArenaOrderingScope &) = delete;
private:
  Kernel &kernel_;
  ArenaOrdering *saved_;
};

enum class ExecutionContext : uint8_t {
  Startup,      // exclude work from search ticks, preserve phase saving
  Inprocessing, // exclude search ticks and suppress speculative phase saving
  Speculation,  // suppress phase saving without changing work attribution
};

// The public support argument naming a context (TACTIX_CONTEXT_*).
constexpr uint64_t execution_context_argument(ExecutionContext context) {
  return static_cast<uint64_t>(context) + 1;
}

// The wires a context owns, saved on entry and restored on exit.
struct ExecutionContextState {
  bool inprocessing = false;
  bool phase_saving_suppressed = false;
};

inline ExecutionContextState enter_execution_context(Kernel &k,
                                                     ExecutionContext context) {
  const ExecutionContextState previous{k.inprocessing, k.phase_saving_suppressed};
  if (context != ExecutionContext::Speculation) k.inprocessing = true;
  if (context != ExecutionContext::Startup) k.phase_saving_suppressed = true;
  return previous;
}

inline void leave_execution_context(Kernel &k, ExecutionContext context,
                                    const ExecutionContextState &previous) {
  if (context != ExecutionContext::Startup)
    k.phase_saving_suppressed = previous.phase_saving_suppressed;
  if (context != ExecutionContext::Speculation)
    k.inprocessing = previous.inprocessing;
}

// Mechanical wires are shared; policies choose the context and scope lifetime.
// Restore only the fields owned by that context, including nested scopes.
// Public sessions hold the same transition as an explicit enter/leave pair
// (tactix_support TACTIX_SUPPORT_ENTER_CONTEXT / LEAVE_CONTEXT).
template <ExecutionContext Context> class ExecutionScope {
public:
  explicit ExecutionScope(Kernel &k)
      : kernel_(k), previous_(enter_execution_context(k, Context)) {}
  ~ExecutionScope() { leave_execution_context(kernel_, Context, previous_); }
  ExecutionScope(const ExecutionScope &) = delete;
  ExecutionScope &operator=(const ExecutionScope &) = delete;

private:
  Kernel &kernel_;
  ExecutionContextState previous_;
};

} // namespace tactix::execution
