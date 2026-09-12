#pragma once

#include <cstddef>
#include <cstdint>

namespace tactix {

struct Kernel;

namespace strategy {

struct CongruenceParameters {
  static constexpr uint32_t AND_ARITY_DEFAULT = 1000000;
  static constexpr size_t GATE_COUNT_DEFAULT = 20000000;
  static constexpr uint32_t XOR_ARITY_DEFAULT = 4;
  static constexpr uint32_t XOR_ARITY_CAPACITY = 4;

  // Zero removes a discovery limit except xor_arity_limit, where zero disables
  // XOR extraction. The fixed-key implementation currently validates against
  // XOR_ARITY_CAPACITY rather than silently truncating a caller request.
  uint32_t and_arity_limit = AND_ARITY_DEFAULT;
  size_t gate_count_limit = GATE_COUNT_DEFAULT;
  uint32_t xor_arity_limit = XOR_ARITY_DEFAULT;

  constexpr bool valid() const {
    return xor_arity_limit <= XOR_ARITY_CAPACITY;
  }
};

class ScopedCongruenceParameters {
public:
  ScopedCongruenceParameters(Kernel &, const CongruenceParameters &);
  ScopedCongruenceParameters(const ScopedCongruenceParameters &) = delete;
  ScopedCongruenceParameters &operator=(const ScopedCongruenceParameters &) =
      delete;
  ~ScopedCongruenceParameters();

private:
  Kernel &kernel_;
  CongruenceParameters saved_;
};

} // namespace strategy
} // namespace tactix
