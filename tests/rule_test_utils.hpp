#pragma once
// Shared helpers for the rule tests. They use only long-stable kernel
// accessors (WatchEntry, the manual arena walk), so frozen-parent controls
// that rebuild these fixtures against predecessor objects still compile.
// Include after `#undef NDEBUG`: live_large asserts.
#include "kernel_fixture.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>
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

// An inline literal list avoids a throwaway array and a duplicated size.
inline tactix::Ref add_large_clause(tactix::Kernel &k,
                                    std::initializer_list<tactix::Lit> lits,
                                    bool redundant = false, uint32_t glue = 0,
                                    uint64_t id = 0) {
  return tactix::kernel_add_large_clause(k, lits.begin(), lits.size(),
                                         redundant, glue, id);
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

// Native binary/large clauses, without ingestion or fresh proof IDs.
inline tactix::Kernel kernel_with_clauses(
    uint32_t variables, std::initializer_list<std::initializer_list<int>> clauses) {
  auto kernel = kernel_with_vars(variables);
  for (const auto &clause : clauses)
    add_external_clause(kernel, clause);
  return kernel;
}

// Whether the current assignment satisfies every live large or binary clause.
inline bool live_formula_satisfied(const tactix::Kernel &k) {
  for (tactix::Ref ref = 0; ref < k.arena.size();) {
    const tactix::Clause *clause = tactix::clause_at(k.arena, ref);
    if (!clause->garbage()) {
      bool sat = false;
      for (const tactix::Lit lit : *clause)
        sat |= k.value(lit) > 0;
      if (!sat)
        return false;
    }
    ref += tactix::clause_actual_words(clause);
  }
  for (tactix::Lit lit = 0; lit < 2 * k.nvars; ++lit)
    for (const tactix::WatchEntry watch : k.watches.entries(lit))
      if (watch.binary() && lit <= watch.other() && k.value(lit) <= 0 &&
          k.value(watch.other()) <= 0)
        return false;
  return true;
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

// Three-coloring K4 with a pendant canonical-order distractor (vertex zero).
inline void add_clique_coloring_fixture(tactix::Kernel &k) {
  using namespace tactix;
  constexpr unsigned vertices = 5, colors = 3;
  kernel_reserve_vars(k, vertices * colors);
  const auto variable = [](unsigned vertex, unsigned color) {
    return static_cast<int>(vertex * colors + color + 1);
  };
  const auto add_binary = [&](int first, int second) {
    kernel_add_binary_clause(k, internalize(first), internalize(second),
                             ++k.next_clause_id);
  };
  for (unsigned vertex = 0; vertex < vertices; ++vertex) {
    Lit atleast[colors];
    for (unsigned color = 0; color < colors; ++color)
      atleast[color] = internalize(variable(vertex, color));
    kernel_add_large_clause(k, atleast, colors, false, 0, ++k.next_clause_id);
    for (unsigned first = 0; first < colors; ++first)
      for (unsigned second = first + 1; second < colors; ++second)
        add_binary(-variable(vertex, first), -variable(vertex, second));
  }
  // Vertex zero is a canonical-order distractor. Vertices 1..4 form K4;
  // edge 0--1 keeps the column-alignment graph connected.
  constexpr unsigned edges[][2] = {
      {0, 1}, {1, 2}, {1, 3}, {1, 4}, {2, 3}, {2, 4}, {3, 4}};
  for (const auto &edge : edges)
    for (unsigned color = 0; color < colors; ++color)
      add_binary(-variable(edge[0], color), -variable(edge[1], color));
}

// Reconstruct and emit a SAT model in original external-variable order.
inline void emit_sat_model(tactix::Kernel &k) {
  using namespace tactix;
  kernel_extend_model(k);
  std::puts("s SATISFIABLE");
  std::printf("v");
  for (Var v = 0; v < kernel_num_output_vars(k); ++v) {
    const Lit lit = make_lit(v, false);
    const int external = static_cast<int>(v) + 1;
    std::printf(" %d", k.model_values[lit] > 0 ? external : -external);
  }
  std::puts(" 0");
}
