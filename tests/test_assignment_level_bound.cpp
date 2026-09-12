#undef NDEBUG
#include "kernel/kernel.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <vector>
using namespace tactix;

static uint64_t digest = 1469598103934665603ull;
static void add(uint64_t n) { digest = (digest ^ n) * 1099511628211ull; }

static void fixture(unsigned size, unsigned target, unsigned maximum,
                    unsigned mode) {
  Kernel k;
  kernel_reserve_vars(k, size + 7);
  std::vector<Lit> lits;
  for (unsigned i = 0; i < size; ++i)
    lits.push_back(make_lit(i, i % 2));
  const Ref ref = kernel_add_large_clause(k, lits.data(), size, false, 0,
                                          ++k.next_clause_id);
  // Valid level bound, but a deliberately non-level-sorted trail: implied
  // literals at lower true levels can follow higher-level decisions.
  const unsigned ceiling = mode == 0 ? 0 : 7;
  for (unsigned i = 0; i < ceiling; ++i)
    kernel_assign_decision(k, make_lit(size + i, false));
  unsigned ordinal = 0, expected = 0;
  for (unsigned i = 0; i < size; ++i) {
    if (i == target) continue;
    unsigned level = 0;
    if (mode >= 2)
      level = ordinal == maximum ? (mode == 2 ? ceiling : ceiling - 1)
                                  : ordinal % 3;
    if (!level) k.unit_ids[i] = ++k.next_clause_id;
    kernel_assign(k, negate(lits[i]), level,
                  level ? DECISION_REASON : UNIT_REASON, false);
    expected = std::max(expected, level);
    ++ordinal;
  }
  assert(ordinal == size - 1 && k.level == ceiling);
  const auto before = k.trail.size();
  const auto ticks = k.stats.ticks;
  const auto root = k.stats.root_assignments;
  const auto unassigned = k.unassigned;
  kernel_assign_ref(k, lits[target], ref);
  const auto &a = k.assigned[target];
  assert(k.values[lits[target]] == 1 && k.values[negate(lits[target])] == -1);
  assert(a.level == expected && a.trail == before && a.reason == ref);
  assert(!a.binary && !a.jumped && k.trail.back() == lits[target]);
  assert(k.trail.size() == before + 1 && k.unassigned + 1 == unassigned);
  assert(k.stats.ticks == ticks && k.stats.root_assignments == root + !expected);
  assert(k.level == ceiling);
  add(size); add(target); add(maximum); add(mode); add(k.level);
  add(k.unassigned); add(k.unassigned_active); add(k.unflushed_root);
  add(k.stats.root_assignments); add(k.stats.ticks);
  for (Lit l : k.trail) {
    const auto &assigned = k.assigned[var_of(l)];
    assert(assigned.level <= k.level);
    add(l); add(assigned.level); add(assigned.trail); add(assigned.reason);
    add(assigned.binary); add(assigned.jumped);
  }
  for (auto v : k.values) add(v);
  for (auto word : k.arena) add(word);
}

int main() {
  unsigned count = 0;
  for (unsigned size : {3u, 17u, 257u})
    for (unsigned target : {0u, size / 2, size - 1})
      for (unsigned maximum = 0; maximum + 1 < size; ++maximum)
        for (unsigned mode = 0; mode < 4; ++mode) {
          fixture(size, target, maximum, mode);
          ++count;
        }
  assert(count == 3288);
  std::printf("PASS: %u assignment-level fixtures; state digest %016llx\n",
              count, static_cast<unsigned long long>(digest));
}
