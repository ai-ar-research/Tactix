// Shared scaffolding for RandomPolicy and the stress policies — the §5.6
// N1-schedule oracle. These three pieces were copied per policy (ten
// byte-identical RNGs, prefix-renamed only to dodge ODR; ten literal draws; four
// env parsers, one of which had DRIFTED), which is `RV-S27`: a divergent copy in
// the component whose job is to be an oracle makes the oracle the unknown.
#pragma once

#include "kernel/kernel.hpp"

#include <cassert>
#include <cstdlib>

namespace tactix {

// xorshift64* — tiny, deterministic, no libc rand state. A stress leg's
// reproducibility rests entirely on it: same seed, same schedule, same proof.
inline uint64_t stress_random(uint64_t &s) {
  s ^= s >> 12;
  s ^= s << 25;
  s ^= s >> 27;
  return s * 0x2545F4914F6CDD1Dull;
}

// Uniform over ALL unassigned variables (the theoretical instance space), then a
// uniform phase. Deliberately NOT drawn from a heuristic's ranking: posing the
// schedules DefaultPolicy would never pose is the whole purpose (N1).
inline Lit stress_random_unassigned_literal(Kernel &k, uint64_t &rng) {
  assert(k.unassigned > 0);
  for (;;) {
    const Var v = static_cast<Var>(stress_random(rng) % k.nvars);
    if (k.values[make_lit(v, false)] == 0)
      return make_lit(v, stress_random(rng) & 1);
  }
}

// Non-negative env knob with a default. A NEGATIVE value takes the default
// rather than wrapping to a huge unsigned — wrapping would silently turn a
// bound into "no bound", which is the opposite of what the operator asked for.
inline uint32_t stress_env_u32(const char *name, uint32_t dflt) {
  const char *s = getenv(name);
  if (!s || !*s)
    return dflt;
  const long v = strtol(s, nullptr, 10);
  return v < 0 ? dflt : static_cast<uint32_t>(v);
}

// Presence-only knob: a stress flag is set or it is not.
inline bool stress_env_flag(const char *name) { return getenv(name) != nullptr; }

} // namespace tactix
