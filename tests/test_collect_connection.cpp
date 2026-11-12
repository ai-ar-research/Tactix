#include "unit_test.hpp"
#include "kernel/kernel.hpp"
#include "rule_test_utils.hpp"
#include "policies/default_policy.hpp"
#include "rules/propagate/propagate.hpp"

static void collect(bool disconnected, bool indexed, bool sparse) {
  Kernel k;
  kernel_reserve_vars(k, 8);
  const Ref removed = add_external_clause(k, {1, 2, 3}, false, 0, ++k.next_clause_id);
  add_external_clause(k, {4, 5, 6}, false, 0, ++k.next_clause_id);
  kernel_add_binary_clause(k, internalize(-4), internalize(7), ++k.next_clause_id);
  kernel_assign_unit(k, internalize(1));
  assert(propagate_apply(k) == ApplyResult::Ok);
  if (disconnected) kernel_flush_large_watches(k);
  OccIndex occurrences;
  if (indexed) kernel_dense_connect(k, occurrences, true);
  kernel_delete_clause(k, clause_at(k.arena, removed));
  for (unsigned repeat = 0; repeat < 3; repeat++) {
    kernel_collect_garbage(k, 0, sparse);
    assert(k.large_watches_connected == !disconnected);
    unsigned large = 0, binary = 0;
    for (Lit lit = 0; lit < 2 * k.nvars; lit++)
      for (const WatchEntry entry : k.watches.entries(lit)) {
        if (entry.binary()) ++binary;
        else {
          ++large;
          assert(!disconnected);
          assert(!clause_at(k.arena, entry.ref())->garbage());
        }
      }
    assert(binary == 2 && large == (disconnected ? 0 : 2));
    kernel_check_invariants(k);
  }
  if (indexed) kernel_dense_disconnect(k);
  if (disconnected) kernel_rebuild_large_watches(k);
  assert(default_solve(k) == 10);
  kernel_extend_model(k);
  assert(k.model_values[internalize(1)] > 0);
  assert(k.model_values[internalize(4)] > 0 || k.model_values[internalize(5)] > 0 ||
         k.model_values[internalize(6)] > 0);
  assert(k.model_values[internalize(-4)] > 0 || k.model_values[internalize(7)] > 0);
}

int main() {
  for (bool disconnected : {false, true})
    for (bool indexed : {false, true})
      for (bool sparse : {false, true}) collect(disconnected, indexed, sparse);
  std::puts("collect-connection: 8 connection/cache/cut combinations, repeated GC and models PASS");
}
