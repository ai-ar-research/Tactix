#pragma once

#include <cstdint>

namespace tactix {
enum class FxGate : uint8_t { Xor = 1, Ite = 2 };
enum class FxClauseScope : uint8_t { Irredundant, Redundant, All };
struct FxOptions {
  uint32_t maximum_clause_size = 20;
  uint32_t minimum_reduction = 1;
  uint8_t gates = 3; // XOR=1, ITE=2.
  FxClauseScope scope = FxClauseScope::All;
  constexpr bool valid() const {
    return maximum_clause_size >= 3 && gates && !(gates & ~3u) &&
           scope <= FxClauseScope::All;
  }
};
} // namespace tactix
