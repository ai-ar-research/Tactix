// Shared primitives for the temporary N1 stress policies (stress-policy skill).
//
// Every stress policy needs the same three things: a deterministic per-seed RNG
// (never wall-clock, so a failing case reproduces from its seed alone), a random
// unassigned literal to decide on, and an env knob reader so the fuzz driver can
// sweep aggressiveness without a rebuild. They were open-coded per policy, which
// left ten byte-identical copies of the RNG prefix-renamed only to dodge ODR —
// one of the redundancy findings in
// benchmarks/reports/2026-08-06-review-post-track-sched.md (`RV-S27`). New
// policies use this header; migrating the existing ten is that item's remainder.
#pragma once

#include "kernel/kernel.hpp"

#include <cassert>
#include <cstdint>
#include <cstdlib>

namespace tactix {
namespace stress {

// xorshift64* — deterministic, seeded, no global state.
inline uint64_t random_u64(uint64_t &s) {
  s ^= s >> 12;
  s ^= s << 25;
  s ^= s >> 27;
  return s * 0x2545F4914F6CDD1Dull;
}

// A random unassigned literal, either phase. Caller guarantees k.unassigned > 0.
inline Lit random_unassigned_literal(Kernel &k, uint64_t &rng) {
  assert(k.unassigned > 0);
  for (;;) {
    const Var v = static_cast<Var>(random_u64(rng) % k.nvars);
    if (k.values[make_lit(v, false)] == 0)
      return make_lit(v, random_u64(rng) & 1);
  }
}

// Env knob: a non-numeric or NEGATIVE value falls back to the default rather
// than wrapping into a huge unsigned bound (the drift `RV-S27` records in one of
// the open-coded copies).
inline uint32_t env_u32(const char *name, uint32_t dflt) {
  const char *s = getenv(name);
  if (!s || !*s)
    return dflt;
  const long v = strtol(s, nullptr, 10);
  return v < 0 ? dflt : static_cast<uint32_t>(v);
}

inline bool env_flag(const char *name) { return getenv(name) != nullptr; }

} // namespace stress
} // namespace tactix
