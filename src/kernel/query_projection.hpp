#pragma once

#include "kernel/types.hpp"
#include <cstdint>
#include <span>
#include <utility>

namespace tactix {
struct Kernel;
enum class QueryModelAdmission : uint8_t { Accepted, Rejected, Restore, Invalid };

// Optional correctness service at transformation boundaries only. Ordinary
// execution leaves this null; propagation, assignment and analysis never
// dispatch through it. The session owns its lifetime and query assumptions.
class QueryProjectionService {
public:
  virtual ~QueryProjectionService() = default;
  virtual std::span<const int32_t> constraints() const = 0;
  virtual bool constrains(Var external) const = 0; // zero-based stable variable
  virtual bool preserves_constant(Lit external) const = 0;
  // Read-only admission for strategies selecting a query-preserving generated
  // domain. Explicit restrictions remain legal and publish through restrict.
  virtual bool preserves_restriction(const Kernel &, std::span<const int8_t>,
                                     std::span<const Lit>) const = 0;
  virtual QueryModelAdmission admit_model(Kernel &) = 0;
  virtual void reconstruction_literal(Lit external) = 0;
  virtual void substitute(Kernel &, std::span<const std::pair<Lit, Lit>>) = 0;
  virtual void restrict(Kernel &, std::span<const int8_t> constants,
                         std::span<const Lit> images) = 0;
  virtual void restrict_constant(Lit external) = 0;
};
} // namespace tactix
