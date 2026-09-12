// Deterministic kernel/rule boundary oracle, linked against both real lineages.
// No mirror solver, random generator, timing claim, or proof-validity claim.
#undef NDEBUG
#include "occurrence_test_utils.hpp"
#include "rules/registry.hpp"
#include "rules/bve/bve.hpp"
#include <algorithm>
#include <cstdio>
#include <string_view>

using namespace tactix;
static uint64_t digest = UINT64_C(1469598103934665603);
static unsigned boundaries = 0;
static void mix(uint64_t x) { digest = (digest ^ x) * UINT64_C(1099511628211); }
template <class Range> static void sequence(const Range &r) {
  mix(r.size());
  for (auto x : r) mix(static_cast<uint64_t>(x));
}
static void snapshot(const Kernel &k, const OccIndex &occ) {
  ++boundaries;
  mix(k.nvars); mix(k.inconsistent); mix(k.conflict_valid);
  mix(k.level); mix(k.propagate_head); mix(k.arena_generation);
  sequence(k.arena); sequence(k.trail); sequence(k.values);
  sequence(k.reconstruction); sequence(k.bve_retry); sequence(k.removed);
  sequence(k.touched); sequence(k.factor_marks);
  mix(k.touch_epoch); mix(k.next_clause_id);
  mix(k.stats.bve_resolutions); mix(k.stats.congruence_ticks);
  mix(k.stats.sweep_ticks); mix(k.stats.sweep_ticks_sub);
  for (Lit lit = 0; lit < 2 * k.nvars; ++lit) {
    const auto refs = large_occurrences(occ, lit);
    sequence(refs);
    mix(bve_schedule_occurrence_count(k, lit));
    mix(k.watches.size(lit));
    for (const WatchEntry watch : k.watches.entries(lit)) {
      mix(watch.binary());
      if (watch.binary()) mix(watch.other());
      else { mix(watch.ref()); mix(watch.blocking()); }
    }
    if (occ.track_order) {
      mix(occ.ordered[lit].size());
      for (const auto entry : occ.ordered[lit]) {
        mix(entry.binary); mix(entry.payload);
      }
    }
  }
}
static void fixture(const char *rule, bool mixed, unsigned variant) {
  Kernel k;
  kernel_reserve_vars(k, 10);
  auto literal = [&](int x) {
    return internalize((variant & 1) ? -x : x);
  };
  auto binary = [&](int a, int b) {
    kernel_add_binary_clause(k, literal(a), literal(b));
  };
  auto clause = [&](std::initializer_list<int> input) {
    std::vector<Lit> lits;
    for (int x : input) lits.push_back(literal(x));
    if (variant & 2) std::reverse(lits.begin(), lits.end());
    return kernel_add_large_clause(k, lits.data(), lits.size(), false, 0);
  };
  binary(-1, 2); binary(1, -2);
  binary(-3, 4); binary(-3, 5); clause({3, -4, -5});
  clause({1, 3, 6}); clause({-1, 3, 6});
  const Ref replace = clause({1, 4, 5, 6});
  const Ref stale = clause({7, 8, 9});
  OccIndex occ;
  kernel_flush_large_watches(k);
  kernel_dense_connect(k, occ, mixed);
  snapshot(k, occ);
  // Delayed binaries must stay after earlier large entries in mixed mode.
  binary(1, 8); binary(1, 8);
  clause({1, 8, 10});
  snapshot(k, occ);
  const Lit image[] = {literal(1), literal(4), literal(6)};
  kernel_replace_large_clause_in_place(k, replace, ++k.next_clause_id,
                                       image, 3, 0);
  kernel_delete_clause(k, clause_at(k.arena, stale));
  snapshot(k, occ); // stale entries deliberately still present
  kernel_delete_binary(k, literal(1), literal(8));
  snapshot(k, occ);
  // BVE's canonical connected scan requires mixed mode. Other rule families
  // also exercise public N1 use outside their usual DefaultPolicy grouping.
  const int rule_id = tactix_rule_id(rule);
  assert(rule_id >= 0);
  const auto &vt = tactix_rules()[rule_id];
  assert(vt.guard(k));
  mix(static_cast<uint64_t>(vt.apply(k)));
  assert(k.dense == &occ);
  snapshot(k, occ);
  kernel_collect_garbage(k);
  snapshot(k, occ);
  kernel_dense_disconnect(k);
  kernel_rebuild_large_watches(k, false);
  kernel_check_invariants(k);
  // Reuse the same owner across both directions of the phase transition.
  kernel_flush_large_watches(k);
  kernel_dense_connect(k, occ, !mixed);
  snapshot(k, occ);
  kernel_dense_disconnect(k);
  kernel_rebuild_large_watches(k, false);
  kernel_check_invariants(k);
}
int main() {
  unsigned cases = 0;
  for (const char *rule : {"Sweep", "Substitute", "Congruence", "HyperTernaryRes",
                           "SubsumeForward", "Strengthen", "BVE"})
    for (bool mixed : {false, true}) {
      if (!mixed && std::string_view(rule) == "BVE") continue;
      for (unsigned variant = 0; variant < 4; ++variant) {
        fixture(rule, mixed, variant);
        ++cases;
      }
    }
  std::printf("occurrence-cutover cases=%u boundaries=%u state=%016llx\n",
              cases, boundaries, static_cast<unsigned long long>(digest));
}
