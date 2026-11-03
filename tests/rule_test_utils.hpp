#pragma once
// Shared helpers for the rule tests. They use only long-stable kernel
// accessors (WatchEntry, the manual arena walk), so frozen-parent controls
// that rebuild these fixtures against predecessor objects still compile.
// Include after `#undef NDEBUG`: live_large asserts.
#include "kernel/kernel.hpp"

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <vector>

// Whether the binary clause (a ∨ b) is watched from a.
inline bool has_binary(const tactix::Kernel &k, tactix::Lit a, tactix::Lit b) {
  for (const tactix::WatchEntry watch : k.watches.entries(a))
    if (watch.binary() && watch.other() == b)
      return true;
  return false;
}

// The other literals of the binary watches of lit, in watch order.
inline std::vector<tactix::Lit> binary_partners(const tactix::Kernel &k,
                                                tactix::Lit lit) {
  std::vector<tactix::Lit> result;
  for (const tactix::WatchEntry watch : k.watches.entries(lit))
    if (watch.binary())
      result.push_back(watch.other());
  return result;
}

// Count live large clauses; if `lits`/`n` given, require one matching.
inline uint32_t live_large(tactix::Kernel &k, const tactix::Lit *lits = nullptr,
                           uint32_t n = 0) {
  uint32_t count = 0;
  bool found = false;
  for (tactix::Ref ref = 0; ref < k.arena.size();) {
    tactix::Clause *c = tactix::clause_at(k.arena, ref);
    if (!c->garbage()) {
      count++;
      if (lits && c->size == n) {
        bool all = true;
        for (uint32_t i = 0; i < n; i++) {
          bool has = false;
          for (const tactix::Lit l : *c)
            has |= (l == lits[i]);
          all &= has;
        }
        found |= all;
      }
    }
    ref += tactix::clause_actual_words(c);
  }
  if (lits)
    assert(found);
  return count;
}

// An irredundant ternary over external DIMACS literals.
inline void add3(tactix::Kernel &k, int a, int b, int c) {
  const tactix::Lit lits[3] = {tactix::internalize(a), tactix::internalize(b),
                               tactix::internalize(c)};
  tactix::kernel_add_large_clause(k, lits, 3, /*redundant=*/false, /*glue=*/0);
}

// out = AND(x1,x2): binaries (¬out∨x1),(¬out∨x2) + base (out∨¬x1∨¬x2)
inline void and_gate(tactix::Kernel &k, int out, int x1, int x2) {
  tactix::kernel_add_binary_clause(k, tactix::internalize(-out),
                                   tactix::internalize(x1));
  tactix::kernel_add_binary_clause(k, tactix::internalize(-out),
                                   tactix::internalize(x2));
  add3(k, out, -x1, -x2);
}

// out = XOR(x1,x2): the 4 odd-parity clauses over {out,x1,x2}
inline void xor_gate(tactix::Kernel &k, int out, int x1, int x2) {
  add3(k, -out, x1, x2);
  add3(k, out, -x1, x2);
  add3(k, out, x1, -x2);
  add3(k, -out, -x1, -x2);
}

// The value of external literal lit under the assignment bits (bit v-1 = v).
inline bool value(unsigned bits, int lit) {
  const bool positive = (bits >> (std::abs(lit) - 1)) & 1;
  return lit > 0 ? positive : !positive;
}
