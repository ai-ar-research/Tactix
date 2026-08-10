// kernel_delete_binary removes both mirror watch entries, retires a live root
// reason to UNIT_REASON, and keeps binary_ids size-monotone. Invariant: while
// a pair is live, its id names a clause the checker still holds. See
// docs/implementation-notes.md §14.
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

// A pair re-derived after deletion installs its fresh id; keeping the retired
// one would make later LRAT chains cite an already retired clause.
static void test_readd_after_delete_installs_fresh_id() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit a = internalize(1), b = internalize(2);
  kernel_add_binary_clause(k, a, b, 7);
  assert(kernel_delete_binary(k, a, b)); // proof retires id 7
  kernel_add_binary_clause(k, a, b, 42); // e.g. Analyze re-learns (a v b)
  assert(kernel_binary_id(k, a, b) == 42); // not the retired 7
  assert(kernel_delete_binary(k, a, b)); // deletable again under the new id
  assert(k.stats.binaries_deleted == 2);
}

// A duplicated pair is not deletable: one id cannot name two live clauses.
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
  assert(k.assigned[vb].reason == UNIT_REASON); // retired
  assert(k.values[b] > 0);                      // b stays assigned
}

// Under reason jumping the recorded reason is a virtual resolvent, so the
// clause the LRAT chain cites is (lit v jumped_over), which the reason field
// no longer names. The live-reason guard must protect that pair too.
// The default follows Kissat; keep the assertion here to pin that wiring.
static void test_jumped_reason_protects_its_antecedent() {
  Kernel k;
  assert(k.jump_reasons);
  kernel_reserve_vars(k, 3);
  const Lit p = internalize(1), q = internalize(2), r = internalize(3);
  kernel_add_binary_clause(k, p, q, 1);          // (p v q)
  kernel_add_binary_clause(k, negate(q), r, 2);  // (~q v r)
  kernel_assign_decision(k, negate(p));          // level 1: p = false
  assert(propagate_apply(k) == ApplyResult::Ok); // q, then r
  const Var vr = var_of(r);
  assert(k.values[r] > 0);
  assert(k.stats.jumped_reasons == 1);
  assert(k.assigned[vr].binary && k.assigned[vr].jumped);
  assert(k.assigned[vr].reason == p);         // the resolvent (r v p)
  assert(k.assigned[vr].reason != negate(q)); // not the real antecedent
  assert(k.jumped_over[vr] == negate(q));
  // both real antecedents are refused while r is a live non-root assignment
  assert(!kernel_delete_binary(k, negate(q), r)); // needs the jumped_over test
  assert(!kernel_delete_binary(k, p, q));         // q's own record still names p
  assert(k.stats.binaries_deleted == 0);
}

int main() {
  test_delete_shrinks_both_lists();
  test_readd_after_delete_installs_fresh_id();
  test_duplicate_pair_refused();
  test_delete_idempotent();
  test_reason_binary_retired_to_unit();
  test_jumped_reason_protects_its_antecedent();
  puts("test_binary_deletion OK");
  return 0;
}
