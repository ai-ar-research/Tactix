// TransitiveReduce model-equivalence oracle: every deletion must preserve the
// model set. No proof checker catches a violation, since a `delete` line is
// accepted unconditionally, so each iteration enumerates all 2^5 assignments
// of a random 5-var formula and compares the model set before and after.
// See docs/calculus.md V4 and docs/implementation-notes.md §14.
//
// argv[1] mode: 0 binaries only; 1 adds a propagated root unit; 2 applies
// per-source instances in shuffled order (the order-independence N1 demands);
// 3 adds random ternaries so large and binary watches interleave. argv[2]
// overrides the iteration count.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "kernel/watch.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/transitive_reduce/transitive_reduce.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <set>
#include <vector>

using namespace tactix;

static std::vector<std::pair<Lit, Lit>> live_binaries(const Kernel &k) {
  std::vector<std::pair<Lit, Lit>> out;
  const Lit nlits = static_cast<Lit>(2 * k.nvars);
  for (Lit l = 0; l < nlits; l++) {
    const uint32_t *ws = k.watches.arena.data() + k.watches.heads[l].offset;
    const uint32_t sz = k.watches.heads[l].size;
    for (uint32_t i = 0; i < sz;) {
      const Watch w{ws[i]};
      if (!w.binary()) {
        i += 2;
        continue;
      }
      const Lit o = w.other();
      i += 1;
      if (l < o)
        out.push_back({l, o}); // each undirected clause once
    }
  }
  return out;
}

// satisfying assignments as a bitmask: bit v set means var v is true
static std::set<uint32_t> models(uint32_t nvars,
                                 const std::vector<std::pair<Lit, Lit>> &bins,
                                 const std::vector<Lit> &units,
                                 const std::vector<std::vector<Lit>> &larges) {
  std::set<uint32_t> ms;
  for (uint32_t a = 0; a < (1u << nvars); a++) {
    bool ok = true;
    auto sat = [&](Lit l) {
      const bool tv = (a >> var_of(l)) & 1;
      return negated(l) ? !tv : tv;
    };
    for (Lit u : units)
      if (!sat(u)) {
        ok = false;
        break;
      }
    if (ok)
      for (auto &b : bins)
        if (!sat(b.first) && !sat(b.second)) {
          ok = false;
          break;
        }
    if (ok)
      for (auto &c : larges) {
        bool s2 = false;
        for (Lit l : c)
          if (sat(l)) {
            s2 = true;
            break;
          }
        if (!s2) {
          ok = false;
          break;
        }
      }
    if (ok)
      ms.insert(a);
  }
  return ms;
}

int main(int argc, char **argv) {
  const int mode = argc > 1 ? atoi(argv[1]) : 0;
  const int iters = argc > 2 ? atoi(argv[2]) : 50000;
  std::mt19937 rng(12345 + mode); // seeded: a failure replays exactly
  const uint32_t NV = 5;
  uint64_t deleted_total = 0, ran = 0, del_seen = 0;
  for (int iter = 0; iter < iters; iter++) {
    Kernel k;
    kernel_reserve_vars(k, NV);
    std::uniform_int_distribution<int> nb(2, 9);
    const int m = nb(rng);
    std::uniform_int_distribution<uint32_t> ld(0, 2 * NV - 1);
    std::vector<std::pair<Lit, Lit>> orig;
    std::vector<Lit> units;
    for (int i = 0; i < m; i++) {
      Lit a = ld(rng), b = ld(rng);
      if (var_of(a) == var_of(b))
        continue; // no tautology, no repeated literal
      if (a > b)
        std::swap(a, b);
      orig.push_back({a, b});
      kernel_add_binary_clause(k, a, b, ++k.next_clause_id);
    }
    std::vector<std::vector<Lit>> larges;
    if (mode == 3) {
      std::uniform_int_distribution<int> nl(0, 3);
      const int L = nl(rng);
      for (int i = 0; i < L; i++) {
        Lit x = ld(rng), y = ld(rng), z = ld(rng);
        if (var_of(x) == var_of(y) || var_of(y) == var_of(z) ||
            var_of(x) == var_of(z))
          continue;
        Lit t[3] = {x, y, z};
        kernel_add_large_clause(k, t, 3, false, 0, ++k.next_clause_id);
        larges.push_back({x, y, z});
      }
    }
    if (orig.empty())
      continue;
    if (mode >= 1 && (rng() % 2)) { // sprinkle a root unit + propagate
      const Lit u = ld(rng);
      units.push_back(u);
      kernel_assign_unit(k, u);
      if (propagate_apply(k) != ApplyResult::Ok)
        continue; // conflict: not a quiescent root
    }
    if (!transitive_reduce_guard(k))
      continue;
    // model set of the original formula
    const std::set<uint32_t> before = models(NV, orig, units, larges);
    if (mode == 2 || mode == 3) { // N1: per-source instances, random order
      std::vector<Lit> srcs;
      transitive_reduce_sources(k, srcs);
      std::shuffle(srcs.begin(), srcs.end(), rng);
      for (const Lit s : srcs)
        transitive_reduce_try(k, s);
    } else {
      transitive_reduce_apply(k);
    }
    ran++;
    deleted_total += k.stats.transitive_deleted;
    if (k.stats.transitive_deleted)
      del_seen++;
    kernel_check_invariants(k); // watch lists / mirror symmetry still sane
    const std::set<uint32_t> after = models(NV, live_binaries(k), units, larges);
    if (before != after) {
      printf("MISMATCH iter=%d\nORIGINAL:\n", iter);
      for (auto &b : orig)
        printf("  %d %d 0\n", externalize(b.first), externalize(b.second));
      printf("SURVIVING:\n");
      for (auto &b : live_binaries(k))
        printf("  %d %d 0\n", externalize(b.first), externalize(b.second));
      printf("units:");
      for (Lit u : units)
        printf(" %d", externalize(u));
      printf("\nmodels before=%zu after=%zu (gained: ", before.size(),
             after.size());
      for (uint32_t a : after)
        if (!before.count(a))
          printf("%u ", a);
      printf(")\n");
      return 1;
    }
  }
  printf("test_tr_bruteforce mode=%d OK (ran=%llu, del_instances=%llu, "
         "deleted=%llu)\n",
         mode, (unsigned long long)ran, (unsigned long long)del_seen,
         (unsigned long long)deleted_total);
  return 0;
}
