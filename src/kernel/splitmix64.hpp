#pragma once

#include <cstdint>

namespace tactix {

inline constexpr uint64_t SPLITMIX64_INCREMENT =
    UINT64_C(0x9e3779b97f4a7c15);

inline uint64_t splitmix64_scramble(uint64_t value) {
  value = (value ^ (value >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
  value = (value ^ (value >> 27)) * UINT64_C(0x94d049bb133111eb);
  return value ^ (value >> 31);
}

inline uint64_t splitmix64_hash(uint64_t value) {
  return splitmix64_scramble(value + SPLITMIX64_INCREMENT);
}

inline uint64_t splitmix64_next(uint64_t &state) {
  state += SPLITMIX64_INCREMENT;
  return splitmix64_scramble(state);
}

struct SplitMix64 {
  uint64_t state;
  uint64_t next() { return splitmix64_next(state); }
  uint64_t below(uint64_t bound) { return next() % bound; }
};

} // namespace tactix
