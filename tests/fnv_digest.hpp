#pragma once
// The FNV-1a state the digest-printing contract tests fold their observations
// into; each test executable owns one digest.
#include <cstdint>

inline uint64_t digest = UINT64_C(1469598103934665603);
inline void hash(uint64_t value) {
  digest = (digest ^ value) * UINT64_C(1099511628211);
}
template <class T> inline void hash_vector(const T &values) {
  hash(values.size());
  for (auto x : values) hash(x);
}
