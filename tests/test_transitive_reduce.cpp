// V4 TransitiveReduce: transitive reduction of the binary implication graph.
// A binary (src ∨ dst) is the BIG edge ¬src → dst; it is deletable when dst
// stays reachable from ¬src along OTHER binaries.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "kernel/watch.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/transitive_reduce/transitive_reduce.hpp"

#include <cassert>
#include <cstdio>

using namespace tactix;

// is the binary (a ∨ b) live? (the watch lists are the only liveness signal —
// binary_ids keeps the id of a deleted binary)
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

// (a ∨ b), (¬b ∨ c), (a ∨ c): the third is redundant — ¬a → b → c reaches c
// without it. Vars: a=1, b=2, c=3
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
  // idempotent: a second round finds nothing
  assert(transitive_reduce_apply(k) == ApplyResult::NoOp);
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

// MUTUAL DEPENDENCY — the tombstone's reason for existing. b ≡ c, so ¬a → b
// reaches c and ¬a → c reaches b: each edge is redundant *via the other*, but
// deleting BOTH would leave `a` unconstrained (a model gained = unsound).
// Exactly one must survive. Vars: a=1, b=2, c=3
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
  // `a` is still constrained: ¬a still implies both b and c
  assert(transitive_reduce_apply(k) == ApplyResult::NoOp); // fixpoint
}

// FAILED LITERAL -> abandon the source to V1 Probe (hpp deviation 5). ¬a → b
// and b implies both d and ¬d, so ¬a is a failed literal — and the walk for
// candidate (a ∨ c) sees it, because the contradiction does not depend on the
// excluded edge. Nothing is deleted; the delta stays delete-only.
// Vars: a=1, b=2, c=3, d=4
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

// SELF-JUSTIFICATION THROUGH THE CONTRAPOSITIVE (hpp deviation 2). ¬a → b,
// b → ¬c, ¬a → c. Testing candidate (a ∨ c), a walk allowed to use that
// candidate's OWN contrapositive edge ¬c → a would run ¬a → b → ¬c → a and
// from `a` could reach anything — justifying the candidate by itself. Our
// walk excludes the pair in BOTH directions, so it reaches neither `a` nor
// `c` and deletes nothing. (kissat, excluding only the candidate's watch
// slot, instead walks into `a`, trips its both-polarities check and reports
// ¬a as a failed literal — which it genuinely is. We leave that unit to V1
// Probe's full propagation, per deviation 5: fewer detections here, never a
// wrong deletion, and never a unit V4 has to justify.) Vars: a=1, b=2, c=3
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

// DUPLICATE PAIRS ARE NOT CANDIDATES (hpp deviation 8): binary_ids holds one
// id per PAIR, so deleting one copy would retire the id the survivor is still
// cited by. Note the duplicate slot IS itself an alternate path to b, so a
// rule without the filter would find (a v b) "redundant" and delete it.
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

// ROOT-FIXED endpoints are never touched: a root-satisfied binary is C2's
// flush's job, and a reason binary must keep its reason. Vars: a=1, b=2, c=3
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

// the instance surface (N1): sources are enumerable and each is individually
// applicable, with the same effect as the canonical round
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

// guard: only a quiescent root fixpoint is applicable
static void test_guard() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  assert(transitive_reduce_guard(k));    // empty trail = trivial fixpoint
  kernel_assign_unit(k, internalize(1)); // un-propagated unit
  assert(!transitive_reduce_guard(k));   // pending propagation
  assert(propagate_apply(k) == ApplyResult::Ok);
  assert(transitive_reduce_guard(k)); // back at fixpoint
}

int main() {
  test_transitive_redundancy();
  test_no_redundancy();
  test_mutual_dependency();
  test_failed_literal_deferred();
  test_duplicate_pair_skipped();
  test_root_fixed_skipped();
  test_instance_surface();
  test_guard();
  puts("test_transitive_reduce OK");
  return 0;
}
