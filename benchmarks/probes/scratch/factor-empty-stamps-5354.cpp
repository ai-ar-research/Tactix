// Fixed fidelity regression; exit 1 means the expected old missing transition.
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "rules/factor/factor.hpp"
#include <cstdio>
#include <vector>
using namespace tactix;

int main() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  k.input_activation_order = {0, 1, 2};
  ScopedHeuristics h(k);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(2), internalize(3));
  heuristics_queue_bump(k, 0);
  k.h->search_idx = k.h->queue_first;
  k.h->search_stamp = k.h->links[k.h->queue_first].stamp;
  const auto before = k.h->queue_stamp;
  std::vector<Var> order;
  for (Var v = k.h->queue_first; v != INVALID_VAR; v = k.h->links[v].next)
    order.push_back(v);
  if (factor_apply(k) != ApplyResult::NoOp || k.stats.factorings || k.nvars != 3)
    return 2;
  // Rule application must not touch H; the registered policy advice owns it.
  if (k.h->queue_stamp != before)
    return 3;
  heuristics_consume_rule_advice(k);
  bool ok = k.h->queue_stamp == order.size();
  for (size_t i = 0; i < order.size(); ++i)
    ok &= k.h->links[order[i]].stamp == i + 1;
  ok &= k.h->search_idx == k.h->queue_last;
  ok &= k.h->search_stamp == k.h->queue_stamp;
  ok &= k.stats.factor_demotions == 0 && k.stats.factor_demoted_vars == 0;
  std::printf("empty Tactix Factor: stamp %u -> %u, dense=%d, cursor_tail=%d\n",
              before, k.h->queue_stamp, ok, k.h->search_idx == k.h->queue_last);
  return ok ? 0 : 1;
}
