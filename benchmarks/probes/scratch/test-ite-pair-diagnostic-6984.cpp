// Exercise repeated ITE inner comparisons, output growth and the 256-pair
// hub boundary through the public extraction API. Compare this exact fixture
// against both actual Congruence objects; no copied extraction algorithm.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/congruence/congruence.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <vector>
using namespace tactix;

static uint64_t digest = 1469598103934665603ull;
static void mix(uint64_t v) { digest ^= v; digest *= 1099511628211ull; }
static void observe(const Kernel &k, const std::vector<CongGate> &gates) {
  mix(k.stats.congruence_ticks); mix(k.stats.rule_interrupted);
  mix(gates.size());
  for (const auto &g : gates) {
    mix(static_cast<unsigned>(g.kind)); mix(g.output); mix(g.base_id());
    mix(g.inputs.size()); for (Lit l : g.inputs) mix(l);
    mix(g.input_bin_ids().size()); for (auto id : g.input_bin_ids()) mix(id);
  }
}

static void exercise(unsigned count, unsigned signs, unsigned mask,
                     bool reverse, unsigned copies) {
  Kernel k; kernel_reserve_vars(k, 1 + 3*count);
  const Lit output = make_lit(0, false);
  std::vector<std::array<Lit, 3>> clauses;
  std::vector<std::array<Lit, 4>> expected;
  for (unsigned i = 0; i < count; i++) {
    Lit c = make_lit(1+3*i, signs&1);
    Lit t = make_lit(2+3*i, signs&2);
    Lit e = make_lit(3+3*i, signs&4);
    const std::array<Lit, 3> rows[] = {
      {c, negate(e), output}, {c, e, negate(output)},
      {negate(c), negate(t), output}, {negate(c), t, negate(output)}
    };
    for (unsigned row = 0; row < 4; row++) if (mask & (1u << row))
      for (unsigned copy = 0; copy < copies; copy++) clauses.push_back(rows[row]);
    if (negated(c)) { c = negate(c); std::swap(t, e); }
    Lit lhs = output;
    if (negated(t)) { lhs = negate(lhs); t = negate(t); e = negate(e); }
    expected.push_back({lhs, c, t, e});
  }
  if (reverse) std::reverse(clauses.begin(), clauses.end());
  for (auto row : clauses) {
    if (reverse) std::reverse(row.begin(), row.end());
    kernel_add_large_clause(k, row.data(), row.size(), false, 0, ++k.next_clause_id);
  }
  const auto arena = k.arena;
  const auto watches = k.watches.arena;
  const auto trail = k.trail;
  for (unsigned repeat = 0; repeat < 2; repeat++) {
    std::vector<CongGate> gates;
    congruence_extract_gates(k, gates, true);
    assert(k.arena == arena && k.watches.arena == watches && k.trail == trail);
    unsigned found = 0;
    for (const auto &g : gates) if (g.kind == GateKind::Ite) {
      assert(g.inputs.size() == 3);
      const std::array<Lit, 4> actual = {g.output, g.inputs[0], g.inputs[1], g.inputs[2]};
      assert(std::count(expected.begin(), expected.end(), actual) == 1);
      found++;
      // Independent semantic check on every model of small original inputs.
      if (count <= 2) for (unsigned bits = 0; bits < (1u << k.nvars); bits++) {
        const auto value = [&](Lit l) { return bool((bits >> var_of(l)) & 1) != negated(l); };
        bool model = true;
        for (const auto &row : clauses) model &= value(row[0]) || value(row[1]) || value(row[2]);
        if (model) assert(value(g.output) == (value(g.inputs[0]) ? value(g.inputs[1]) : value(g.inputs[2])));
      }
    }
    // Each full gate contributes four conditional-list entries per signed
    // output after unique indexing. Exactly64 gates hit256;65 exceed it.
    if (found != (mask == 15 && count <= 64 ? count : 0)) {
      std::fprintf(stderr, "count=%u signs=%u mask=%u reverse=%d copies=%u found=%u\n",
                   count, signs, mask, reverse, copies, found);
      assert(false);
    }
    observe(k, gates);
  }
  mix(count); mix(signs); mix(mask); mix(reverse); mix(copies);
}

int main() {
  unsigned cases = 0;
  for (unsigned count : {0u, 1u, 2u, 8u, 63u, 64u, 65u})
    for (unsigned signs = 0; signs < 8; signs++)
      for (bool reverse : {false, true})
        for (unsigned copies : {1u, 2u}) {
          exercise(count, signs, 15, reverse, copies); cases++;
        }
  for (unsigned mask = 0; mask < 15; mask++)
    for (unsigned signs = 0; signs < 8; signs++)
      for (bool reverse : {false, true}) {
        exercise(1, signs, mask, reverse, 1); cases++;
      }
  std::printf("PASS %u ITE pair cases digest=%016llx\n", cases, (unsigned long long)digest);
}
