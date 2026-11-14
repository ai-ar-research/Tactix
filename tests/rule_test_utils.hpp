#pragma once
// Shared helpers for the rule tests. They use only long-stable kernel
// accessors (WatchEntry, the manual arena walk), so frozen-parent controls
// that rebuild these fixtures against predecessor objects still compile.
// Include after `#undef NDEBUG`: live_large asserts.
#include "kernel/kernel.hpp"

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
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

// Whether lit watches the large clause at ref.
inline bool watched_by(const tactix::Kernel &k, tactix::Lit lit, tactix::Ref ref) {
  for (const tactix::WatchEntry watch : k.watches.entries(lit))
    if (!watch.binary() && watch.ref() == ref)
      return true;
  return false;
}

// The internal literals of external DIMACS literals.
inline std::vector<tactix::Lit> lits_of(std::initializer_list<int> external) {
  std::vector<tactix::Lit> lits;
  for (const int literal : external)
    lits.push_back(tactix::internalize(literal));
  return lits;
}

// Add a clause over external DIMACS literals: two as a binary clause, three or
// more as a large clause with redundant/glue. Id 0 is the kernel's default.
// Returns the large clause's Ref, INVALID_REF for a binary clause.
inline tactix::Ref add_external_clause(tactix::Kernel &k,
                                       std::initializer_list<int> external,
                                       bool redundant = false, uint32_t glue = 0,
                                       uint64_t id = 0) {
  const std::vector<tactix::Lit> lits = lits_of(external);
  assert(lits.size() >= 2);
  if (lits.size() == 2) {
    assert(!redundant);
    tactix::kernel_add_binary_clause(k, lits[0], lits[1], id);
    return tactix::INVALID_REF;
  }
  return tactix::kernel_add_large_clause(k, lits.data(),
                                         static_cast<uint32_t>(lits.size()),
                                         redundant, glue, id);
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
  add_external_clause(k, {a, b, c});
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
