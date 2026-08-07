// V4 TransitiveReduce: a binary (src ∨ dst) is the BIG edge ¬src → dst,
// deletable when dst stays reachable from ¬src along other binaries.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "kernel/watch.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/transitive_reduce/transitive_reduce.hpp"

#include <cassert>
#include <cstdio>

using namespace tactix;

// is (a ∨ b) live? the watch lists are the only liveness signal, since
// `binary_ids` keeps the id of a deleted binary
static uint32_t copies(const Kernel &k, Lit l, Lit other) {
  const uint32_t *ws = k.watches.arena.data() + k.watches.heads[l].offset;
  const uint32_t sz = k.watches.heads[l].size;
  uint32_t n = 0;
  for (uint32_t i = 0; i < sz;) {
    const Watch w{ws[i]};
    if (!w.binary()) {
      i += 2;
      continue;
    }
    if (w.other() == other)
      n++;
    i += 1;
  }
  return n;
}

static bool live(const Kernel &k, int ea, int eb) {
  const Lit a = internalize(ea), b = internalize(eb);
  const uint32_t in_a = copies(k, a, b), in_b = copies(k, b, a);
  assert(in_a == in_b); // mirror symmetry: entries go in pairs or not at all
  return in_a > 0;
}

static void add_bin(Kernel &k, int ea, int eb) {
  kernel_add_binary_clause(k, internalize(ea), internalize(eb),
                           ++k.next_clause_id);
}

// (a ∨ b), (¬b ∨ c), (a ∨ c): the third is redundant. Vars: a=1, b=2, c=3
static void test_transitive_redundancy() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  add_bin(k, 1, 2);  // ¬a → b
  add_bin(k, -2, 3); // b → c
  add_bin(k, 1, 3);  // ¬a → c   (transitively redundant)
  assert(transitive_reduce_guard(k));
  assert(transitive_reduce_apply(k) == ApplyResult::Ok);
  assert(!live(k, 1, 3));  // deleted
  assert(live(k, 1, 2));   // the path survives
  assert(live(k, -2, 3));  // ...both edges of it
  assert(k.stats.transitive_deleted == 1);
  assert(k.stats.binaries_deleted == 1);
  // the id ledger is monotone: the deleted pair keeps its id
  assert(kernel_binary_id(k, internalize(1), internalize(3)) != 0);
  assert(transitive_reduce_apply(k) == ApplyResult::NoOp); // idempotent
  assert(k.stats.transitive_deleted == 1);
}

// no alternate paths: nothing is deleted
static void test_no_redundancy() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  add_bin(k, 1, 2);
  add_bin(k, 3, 4);
  assert(transitive_reduce_apply(k) == ApplyResult::NoOp);
  assert(live(k, 1, 2));
  assert(live(k, 3, 4));
  assert(k.stats.transitive_deleted == 0);
}

// Mutual dependency (the tombstone's reason for existing): with b ≡ c each of
// ¬a → b, ¬a → c is redundant via the other, but deleting both gains a model.
static void test_mutual_dependency() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  add_bin(k, 1, 2);  // ¬a → b
  add_bin(k, 1, 3);  // ¬a → c
  add_bin(k, -2, 3); // b → c
  add_bin(k, -3, 2); // c → b   (so b ≡ c)
  assert(transitive_reduce_apply(k) == ApplyResult::Ok);
  const bool ab = live(k, 1, 2), ac = live(k, 1, 3);
  assert(ab != ac);                     // exactly one deleted, never both
  assert(k.stats.transitive_deleted == 1);
  assert(live(k, -2, 3) && live(k, -3, 2)); // the equivalence survives
  assert(transitive_reduce_apply(k) == ApplyResult::NoOp); // fixpoint
}

// A failed literal hands the source to V1 Probe (hpp deviation 5): b implies d
// and ¬d, so ¬a is failed and nothing is deleted. Vars: a=1, b=2, c=3, d=4
static void test_failed_literal_deferred() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  add_bin(k, 1, 2);   // ¬a → b
  add_bin(k, 1, 3);   // ¬a → c
  add_bin(k, -2, 4);  // b → d
  add_bin(k, -2, -4); // b → ¬d   (so ¬a → b → d ∧ ¬d)
  assert(transitive_reduce_apply(k) == ApplyResult::NoOp);
  assert(k.stats.transitive_failed == 1);  // handed to V1 Probe
  assert(k.stats.transitive_deleted == 0); // delta stays delete-only
  assert(live(k, 1, 2) && live(k, 1, 3));
}

// No self-justification through the contrapositive (hpp deviation 2): a walk
// for (a ∨ c) allowed the candidate's own edge ¬c → a could reach anything, so
// the pair is excluded both ways. kissat instead reports ¬a as failed.
static void test_no_contrapositive_self_justification() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  add_bin(k, 1, 2);   // ¬a → b
  add_bin(k, -2, -3); // b → ¬c
  add_bin(k, 1, 3);   // ¬a → c
  assert(transitive_reduce_apply(k) == ApplyResult::NoOp);
  assert(k.stats.transitive_deleted == 0);
  assert(live(k, 1, 2) && live(k, -2, -3) && live(k, 1, 3));
}

// A duplicate pair is not a candidate (hpp deviation 8): `binary_ids` holds one
// id per pair, and the duplicate slot is itself the alternate path.
static void test_duplicate_pair_skipped() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  add_bin(k, 1, 2); // ¬a → b   (id 1)
  add_bin(k, 1, 2); // ¬a → b   again (id 2) — two live copies of one pair
  add_bin(k, 1, 3); // a second distinct out-edge, so the source is enumerated
  assert(transitive_reduce_apply(k) == ApplyResult::NoOp);
  assert(k.stats.transitive_deleted == 0);
  assert(copies(k, internalize(1), internalize(2)) == 2); // both survive
  assert(live(k, 1, 2));
}

// Root-fixed endpoints are never touched: that is C2's flush's job.
static void test_root_fixed_skipped() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  add_bin(k, 1, 2);
  add_bin(k, -2, 3);
  add_bin(k, 1, 3);
  kernel_assign_unit(k, internalize(1)); // a true: every (a ∨ ·) satisfied
  assert(propagate_apply(k) == ApplyResult::Ok);
  assert(transitive_reduce_guard(k));
  assert(transitive_reduce_apply(k) == ApplyResult::NoOp);
  assert(live(k, 1, 3)); // untouched: root-fixed source is not a candidate
}

// Instance surface (N1): each source is individually applicable.
static void test_instance_surface() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  add_bin(k, 1, 2);
  add_bin(k, -2, 3);
  add_bin(k, 1, 3);
  std::vector<Lit> srcs;
  transitive_reduce_sources(k, srcs);
  // ¬a is the only literal with 2 binary out-edges and a canonical candidate
  assert(srcs.size() == 1);
  assert(srcs[0] == internalize(1));
  // a literal that is not a source is a total no-op (payload totality)
  assert(transitive_reduce_try(k, internalize(3)) == ApplyResult::NoOp);
  assert(transitive_reduce_try(k, internalize(1)) == ApplyResult::Ok);
  assert(!live(k, 1, 3));
  // out-of-range payloads are rejected, not asserted on
  assert(transitive_reduce_try(k, internalize(99)) == ApplyResult::NoOp);
}

// Guard: pure quiescent-root precondition, no fixpoint (ledger row D-LEVEL).
static void test_guard() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  assert(transitive_reduce_guard(k));    // empty trail = trivial fixpoint
  kernel_assign_unit(k, internalize(1)); // un-propagated root unit
  assert(transitive_reduce_guard(k));    // pure guard: holds off a fixpoint
  assert(propagate_apply(k) == ApplyResult::Ok);
  assert(transitive_reduce_guard(k));
  kernel_assign_decision(k, internalize(2));
  assert(!transitive_reduce_guard(k)); // level > 0 is still not a root state
}

int main() {
  test_transitive_redundancy();
  test_no_redundancy();
  test_mutual_dependency();
  test_failed_literal_deferred();
  test_no_contrapositive_self_justification();
  test_duplicate_pair_skipped();
  test_root_fixed_skipped();
  test_instance_surface();
  test_guard();
  puts("test_transitive_reduce OK");
  return 0;
}
