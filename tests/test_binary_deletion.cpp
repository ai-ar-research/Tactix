// V4 prerequisite: kernel_delete_binary. Removes both mirror watch entries,
// retires a live root reason to UNIT_REASON, and keeps binary_ids size-
// monotone. Also pins the binary_ids INVARIANT the primitive rests on: while a
// pair is live, its id names a clause the checker still holds (so a re-add
// installs the fresh id, and a duplicated pair — one id, two clauses — is not
// deletable). Id-free (no proof); the `delete` emission is exercised by the V4
// fuzz + micro-proof legs.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/propagate/propagate.hpp"

#include <cassert>
#include <cstdio>

using namespace tactix;

// delete drops exactly one slot from each mirror list; the map keeps the key
static void test_delete_shrinks_both_lists() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit a = internalize(1), b = internalize(2);
  kernel_add_binary_clause(k, a, b, 7);
  kernel_add_binary_clause(k, a, internalize(3), 8); // unrelated binary on a
  const uint32_t sa = k.watches.size(a), sb = k.watches.size(b);
  const size_t ids = k.binary_ids.size();
  assert(kernel_binary_id(k, a, b) == 7);
  assert(kernel_delete_binary(k, a, b));
  assert(k.watches.size(a) == sa - 1);
  assert(k.watches.size(b) == sb - 1);
  assert(k.binary_ids.size() == ids); // size-monotone: the key stays
  assert(kernel_binary_id(k, a, internalize(3)) == 8); // other binary intact
  assert(k.stats.binaries_deleted == 1);
}

// THE binary_ids INVARIANT: a pair re-derived after deletion installs its
// FRESH id. Keeping the retired id (emplace's "first id wins") would make
// every later LRAT chain through this binary cite the clause the `del` already
// retired — an LRAT-only rejection the DRAT leg cannot see. Analyze,
// Strengthen and Vivify can all re-derive a binary V4 deleted.
static void test_readd_after_delete_installs_fresh_id() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit a = internalize(1), b = internalize(2);
  kernel_add_binary_clause(k, a, b, 7);
  assert(kernel_delete_binary(k, a, b)); // proof retires id 7
  kernel_add_binary_clause(k, a, b, 42); // e.g. Analyze re-learns (a v b)
  assert(kernel_binary_id(k, a, b) == 42); // NOT the retired 7
  // and the re-added pair is live and deletable again, now under its own id
  assert(kernel_delete_binary(k, a, b));
  assert(k.stats.binaries_deleted == 2);
}

// A DUPLICATED pair is not deletable: one binary_ids id cannot name two live
// clauses, so deleting one copy would retire the id the survivor still needs.
static void test_duplicate_pair_refused() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit a = internalize(1), b = internalize(2);
  kernel_add_binary_clause(k, a, b, 3); // two live copies of the same pair
  kernel_add_binary_clause(k, a, b, 4); // (duplicate input clauses do this)
  const uint32_t sa = k.watches.size(a), sb = k.watches.size(b);
  assert(!kernel_delete_binary(k, a, b)); // refused
  assert(k.watches.size(a) == sa && k.watches.size(b) == sb);
  assert(k.stats.binaries_deleted == 0);
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
  test_readd_after_delete_installs_fresh_id();
  test_duplicate_pair_refused();
  test_delete_idempotent();
  test_reason_binary_retired_to_unit();
  puts("test_binary_deletion OK");
  return 0;
}
