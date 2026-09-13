// Observe actual Factor index scores through its existing strategy interface.
// Counting support must not delete the underlying root-satisfied binaries.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "rules/factor/factor.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace tactix;

struct Scores : FactorSelection {
  std::vector<int> seen;
  explicit Scores(size_t n) : seen(n, -1) {}
  double seed_score(const Kernel &, Lit l, double score) override {
    assert(seen[l] == -1);
    seen[l] = static_cast<int>(score);
    return 0; // Observe the complete initial index without selecting a grid.
  }
  double next_factor_score(const Kernel &, Lit, uint32_t, Lit, double) override {
    assert(false); return 0;
  }
  bool accept(uint32_t, uint32_t, int) override { assert(false); return false; }
};

static void fixture(bool reverse, unsigned signs, bool fixed,
                    unsigned rounds, bool forced) {
  Kernel k;
  ScopedHeuristics heuristics(k);
  kernel_reserve_vars(k, 6);
  // Stable internal ids are deliberately different from imported order in
  // half the cases. Endpoint polarity must not replace variable import rank.
  for (Var v = 0; v < k.nvars; ++v) k.internal_import_rank[v] = v;
  if (reverse) {
    k.internal_import_rank[0] = 1;
    k.internal_import_rank[1] = 0;
  }
  k.factor_cand_rounds = rounds;
  const Lit u = make_lit(0, signs & 1), x = make_lit(1, signs & 2);
  const Lit a = make_lit(2, false), b = make_lit(3, false);
  const Lit p = make_lit(4, false), q = make_lit(5, false);
  for (const auto pair : {std::pair<Lit, Lit>{u, x}, {a, p}, {b, q}})
    kernel_add_binary_clause(k, pair.first, pair.second, ++k.next_clause_id);
  const Lit clause[] = {x, a, b};
  const Ref ref = kernel_add_large_clause(k, clause, 3, false, 0, ++k.next_clause_id);
  if (fixed) kernel_assign_unit(k, u);
  Scores scores(2 * k.nvars);
  k.factor_selection = &scores;
  const int id = tactix_rule_id("Factor");
  assert(id >= 0 && factor_guard(k));
  const auto &rule = tactix_rules()[id];
  const auto result = forced ? rule.apply_instance(k, lit_instance(k, x)) : factor_apply(k);
  assert(result == ApplyResult::NoOp && k.stats.factorings == 0);
  bool omitted = fixed && !reverse && rounds;
  assert(scores.seen[x] == (omitted ? 1 : 2));
  if (!forced) assert(scores.seen[a] == scores.seen[x] && scores.seen[b] == scores.seen[x]);
  assert(kernel_binary_copies(k, u, x) == 1);
  assert(kernel_binary_copies(k, a, p) == 1 && kernel_binary_copies(k, b, q) == 1);
  assert(!clause_at(k.arena, ref)->garbage());
  assert(k.value(u) == (fixed ? 1 : 0) && !kernel_var_retired(k, var_of(u)));
  assert(k.nvars == 6);
  std::printf("%u %u %u %u %u %d\n", reverse, signs, fixed, rounds, forced, scores.seen[x]);
}

int main() {
  for (bool reverse : {false, true})
    for (unsigned signs = 0; signs < 4; ++signs)
      for (bool fixed : {false, true})
        for (unsigned rounds : {0u, 1u, 2u})
          for (bool forced : {false, true}) fixture(reverse, signs, fixed, rounds, forced);
  std::puts("PASS: 96 actual Factor support-count cases");
}
