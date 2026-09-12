#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "rules/registry.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace tactix;

static unsigned missing = 0, observations = 0;

static void observe(uint32_t actual, uint32_t expected) {
  ++observations;
  missing += actual != expected;
}

static void fixture(bool reverse_import, bool stable, bool retire,
                    uint32_t stamp) {
  Kernel k;
  ScopedHeuristics owner(k);
  kernel_reserve_vars(k, 6);
  const std::vector<Var> order = reverse_import
      ? std::vector<Var>{5, 3, 4, 1, 2, 0}
      : std::vector<Var>{0, 1, 2, 3, 4, 5};
  for (Var v : order)
    kernel_record_import(k, v);
  heuristics_seed_activation_order(k, order);
  owner.h.stable = stable;
  kernel_assign_unit(k, make_lit(1, false));
  kernel_assign_unit(k, make_lit(3, true));
  heuristics_deactivate_inactive_vars(k);
  const Var first = reverse_import ? 3 : 1;
  const Var other = reverse_import ? 1 : 3;
  owner.h.links[first].stamp = stamp;
  owner.h.links[other].stamp = stamp ^ 0x55555555u;
  const uint32_t other_stamp = owner.h.links[other].stamp;
  if (retire) {
    k.var_status.assign(k.nvars, VarStatus::Active);
    k.var_status[0] = VarStatus::Eliminated;
    heuristics_deactivate_var(k, 0);
    --k.unassigned_active;
  }
  heuristics_compact_random_order_fixed(k);
  assert(owner.h.fixed_representative == first);
  const auto original_order = k.compact_import_order;
  std::vector<int> queue;
  for (Var v = owner.h.queue_first; v != INVALID_VAR;
       v = owner.h.links[v].next)
    queue.push_back(kernel_externalize(k, make_lit(v, false)));
  assert(!queue.empty());
  owner.h.search_idx = var_of(kernel_internalize(k, queue.front()));
  const uint64_t instance = lit_instance(k, kernel_internalize(k, 6));

  for (unsigned repeat = 0; repeat != 3; ++repeat) {
    assert(kernel_compact_variables(k));
    heuristics_compact_random_order_fixed(k);
    const Var mapped_first = var_of(kernel_internalize(k, first + 1));
    const Var mapped_other = var_of(kernel_internalize(k, other + 1));
    assert(owner.h.fixed_representative == mapped_first);
    observe(owner.h.links[mapped_first].stamp, stamp);
    observe(owner.h.links[mapped_other].stamp, other_stamp);
    assert(!heuristics_queue_contains(k, mapped_first));
    assert(!heuristics_queue_contains(k, mapped_other));
    assert(owner.h.scores.pos[mapped_first] == INVALID_VAR);
    assert(owner.h.stable == stable);
    assert(kernel_externalize(k, make_lit(owner.h.search_idx, false)) == queue.front());
    assert(owner.h.search_stamp == 1);
    unsigned pos = 0;
    for (Var v = owner.h.queue_first; v != INVALID_VAR;
         v = owner.h.links[v].next) {
      assert(pos < queue.size());
      assert(kernel_externalize(k, make_lit(v, false)) == queue[pos]);
      assert(owner.h.links[v].stamp == ++pos);
    }
    assert(pos == queue.size() && owner.h.queue_stamp == pos);
    assert(k.value(kernel_internalize(k, 2)) == 1);
    assert(k.value(kernel_internalize(k, -4)) == 1);
    assert(lit_instance(k, kernel_internalize(k, 6)) == instance);
    kernel_check_invariants(k);
    // Compaction does not narrow the public two-phase Decide surface.
    for (const RuleVTable &rule : tactix_rules()) {
      if (std::strcmp(rule.name, "Decide"))
        continue;
      uint64_t actions[12];
      assert(rule.instances(k, actions, 12) == 2 * queue.size());
    }
  }
  assert(k.compact_import_order.size() == original_order.size() - retire);
}

int main(int argc, char **argv) {
  const bool old = argc == 2 && !std::strcmp(argv[1], "--expect-old-missing");
  assert(argc == 1 || old);
  for (bool reverse : {false, true})
    for (bool stable : {false, true})
      for (bool retire : {false, true})
        for (uint32_t stamp : {11u, 73u, UINT32_MAX})
          fixture(reverse, stable, retire, stamp);
  Kernel bare;
  kernel_reserve_vars(bare, 1);
  heuristics_compact_vars(bare, {0}, 1);
  heuristics_compact_random_order_fixed(bare);
  assert(!bare.h && bare.nvars == 1);
  std::printf("compact-stamp cases=24 observations=%u missing=%u\n", observations, missing);
  // The frozen parent must miss every stamp observation, without crashing.
  return old ? missing != observations : missing != 0;
}
