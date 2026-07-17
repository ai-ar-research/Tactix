// Commit 1 (V4 prerequisite): kernel_delete_binary. Removes both mirror watch
// entries, retires a live root reason to UNIT_REASON, and keeps the binary_ids
// id (monotone ledger). Id-free (no proof); the proof `delete` emission is
// exercised by the V4 fuzz + micro-proof legs once the rule lands.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/propagate/propagate.hpp"

#include <cassert>
#include <cstdio>

using namespace tactix;

// delete drops exactly one slot from each mirror list; the id survives
static void test_delete_shrinks_both_lists() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit a = internalize(1), b = internalize(2);
  kernel_add_binary_clause(k, a, b, 7);
  kernel_add_binary_clause(k, a, internalize(3), 8); // unrelated binary on a
  const uint32_t sa = k.watches.size(a), sb = k.watches.size(b);
  assert(kernel_binary_id(k, a, b) == 7);
  assert(kernel_delete_binary(k, a, b));
  assert(k.watches.size(a) == sa - 1);
  assert(k.watches.size(b) == sb - 1);
  assert(kernel_binary_id(k, a, b) == 7);              // ledger stays monotone
  assert(kernel_binary_id(k, a, internalize(3)) == 8); // other binary intact
  assert(k.stats.binaries_deleted == 1);
}

// deleting an absent binary is a no-op (idempotent)
static void test_delete_idempotent() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit a = internalize(1), b = internalize(2);
  kernel_add_binary_clause(k, a, b, 5);
  assert(kernel_delete_binary(k, a, b));
  const uint32_t sa = k.watches.size(a), sb = k.watches.size(b);
  assert(!kernel_delete_binary(k, a, b)); // already gone
  assert(k.watches.size(a) == sa && k.watches.size(b) == sb);
  assert(k.stats.binaries_deleted == 1);
}

// a root literal propagated by the deleted binary is retired to a unit reason
static void test_reason_binary_retired_to_unit() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit a = internalize(1), b = internalize(2);
  kernel_add_binary_clause(k, a, b, 9);   // (a v b)
  kernel_assign_unit(k, internalize(-1)); // a = false at root
  assert(propagate_apply(k) == ApplyResult::Ok); // (a v b) forces b, reason = a
  const Var vb = var_of(b);
  assert(k.values[b] > 0);
  assert(k.assigned[vb].binary && k.assigned[vb].reason == a);
  assert(k.assigned[vb].level == 0);
  assert(kernel_delete_binary(k, a, b));
  assert(k.assigned[vb].reason == UNIT_REASON); // retired (corpus-003 analog)
  assert(k.values[b] > 0);                      // b stays assigned
}

int main() {
  test_delete_shrinks_both_lists();
  test_delete_idempotent();
  test_reason_binary_retired_to_unit();
  puts("test_binary_deletion OK");
  return 0;
}
