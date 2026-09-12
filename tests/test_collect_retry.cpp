// Sparse collection retirement is not a semantic-removal retry producer.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/propagate/propagate.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace tactix;

static unsigned calls;
static void notify(Kernel &, void *, Var, bool reinsert) {
  assert(reinsert);
  ++calls;
}

static unsigned cases, parent_differences;
static void scenario(bool red, unsigned root_at, bool marked, bool collect,
                     bool parent) {
  Kernel k;
  kernel_reserve_vars(k, 5);
  kernel_materialize_cold_state(k);
  Lit lits[] = {internalize(1), internalize(2), internalize(3),
                internalize(4), internalize(5)};
  std::swap(lits[0], lits[root_at]);
  Ref ref = kernel_add_large_clause(k, lits, 5, red, 4, ++k.next_clause_id);
  kernel_assign_unit(k, internalize(1));
  assert(propagate_apply(k) == ApplyResult::Ok);
  std::fill(k.bve_retry.begin(), k.bve_retry.end(), marked);
  std::fill(k.removed.begin(), k.removed.end(), 0);
  const auto old_marks = k.stats.bve_marks;
  calls = 0;
  k.bve_score_update_hook = notify;
  if (collect) {
    assert(kernel_sparse_collect_root(k) == RootCollectOutcome::Changed);
    assert(k.arena.empty());
  } else {
    // The default primitive is also Vivify's satisfied-clause deletion path.
    uint64_t changed = 0;
    assert(kernel_flush_root_clause(k, &ref, &changed, true, true) ==
           RootFlushOutcome::Deleted);
    assert(changed == 1 && clause_at(k.arena, ref)->garbage());
    kernel_collect_garbage(k);
  }
  const bool removal = !red && (!collect || parent);
  assert(calls == (removal ? 5u : 0u));
  assert(k.stats.bve_marks - old_marks == (removal ? 5u : 0u));
  for (Var v = 0; v < 5; ++v) {
    assert(bool(k.bve_retry[v]) == (marked || removal));
    assert(bool(k.removed[v]) == removal);
  }
  if (collect && !red && calls) ++parent_differences;
  assert(k.live_irredundant_large == 0 && k.value(internalize(1)) == 1);
  const auto retries = k.bve_retry;
  const auto removed = k.removed;
  calls = 0;
  assert(kernel_sparse_collect_root(k) == RootCollectOutcome::NoChange);
  assert(!calls && k.bve_retry == retries && k.removed == removed);
  k.bve_score_update_hook = nullptr;
  kernel_check_invariants(k);
  ++cases;
}

static void binary_control() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_materialize_cold_state(k);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_assign_unit(k, internalize(1));
  assert(propagate_apply(k) == ApplyResult::Ok);
  std::fill(k.bve_retry.begin(), k.bve_retry.end(), 0);
  std::fill(k.removed.begin(), k.removed.end(), 0);
  assert(kernel_sparse_collect_root(k) == RootCollectOutcome::Changed);
  assert(!kernel_binary_live(k, internalize(1), internalize(2)));
  assert(k.bve_retry[1] && k.removed[1]);
  kernel_check_invariants(k);
}

int main(int argc, char **argv) {
  const bool parent = argc == 2 && !std::strcmp(argv[1], "--expect-parent");
  assert(argc == 1 || parent);
  for (bool red : {false, true}) for (unsigned pos = 0; pos < 5; ++pos)
    for (bool marked : {false, true}) for (bool collect : {false, true})
      scenario(red, pos, marked, collect, parent);
  binary_control();
  assert(cases == 40 && parent_differences == (parent ? 10u : 0u));
  std::printf("collect retry cases=%u parent differences=%u PASS\n",
              cases, parent_differences);
}
