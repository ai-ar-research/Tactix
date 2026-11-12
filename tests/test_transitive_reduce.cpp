// V4 TransitiveReduce: a binary (src ∨ dst) is the BIG edge ¬src → dst,
// deletable when dst stays reachable from ¬src along other binaries.
#undef NDEBUG
#include "rule_test_utils.hpp"
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/transitive_reduce/transitive_reduce.hpp"

#include <cassert>
#include <cstdio>

using namespace tactix;

// Count (l ∨ other) entries over l's whole watch list, without relying on
// the kernel's binaries-first layout.
static uint32_t copies(const Kernel &k, Lit l, Lit other) {
  uint32_t n = 0;
  for (const WatchEntry w : k.watches.entries(l))
    n += w.binary() && w.other() == other;
  return n;
}

// Is (a ∨ b) live? The watch lists are the liveness signal (`binary_ids`
// keeps a deleted binary's id).
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
  assert(k.transitive_early_exit); // Kissat's candidate termination semantics
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

// Pinned transitive.c commits a failed-source unit immediately: b implies d
// and ¬d, so assuming ¬a refutes and {a} is learned. Vars: a=1, b=2, c=3, d=4.
static void test_failed_literal_committed() {
  Kernel k;
  k.transitive_early_exit = false; // exhaustive diagnostic owns this behavior
  Proof proof;
  assert(proof.open("/dev/null", ProofFormat::DsrAscii));
  k.proof = &proof;
  kernel_reserve_vars(k, 4);
  add_bin(k, 1, 2);   // ¬a → b
  add_bin(k, 1, 3);   // ¬a → c
  add_bin(k, -2, 4);  // b → d
  add_bin(k, -2, -4); // b → ¬d   (so ¬a → b → d ∧ ¬d)
  assert(transitive_reduce_try(k, internalize(1)) == ApplyResult::Ok);
  assert(k.stats.transitive_units == 1);
  assert(k.value(internalize(1)) > 0);
  assert(k.stats.transitive_deleted == 0);
  assert(live(k, 1, 2) && live(k, 1, 3));
}

// A failed walk keeps deletions already proved for the same source: Kissat's
// transitive.c compacts the earlier reduction before learning {a} (Rank 34).
static void test_failed_literal_retains_earlier_deletion() {
  Kernel k;
  assert(k.transitive_early_exit);
  kernel_reserve_vars(k, 5);
  add_bin(k, 1, 2);   // candidate b: excluded during its own walk
  add_bin(k, 1, 3);   // -a -> c
  add_bin(k, 1, 4);   // later candidate d
  add_bin(k, -3, 2);  // c -> b: proves (a v b) redundant
  add_bin(k, -2, 5);  // b -> e
  add_bin(k, -2, -5); // b -> -e: a later walk fails
  assert(transitive_reduce_try(k, internalize(1)) == ApplyResult::Ok);
  assert(k.stats.transitive_deleted == 1);
  assert(k.stats.transitive_units == 1);
  assert(!live(k, 1, 2));
  assert(k.value(internalize(1)) > 0);
}

// No self-justification through the contrapositive: excluding the candidate in
// both directions prevents an invalid deletion, while the surviving path
// still proves pinned Kissat's failed-source unit {a}.
static void test_no_contrapositive_self_justification() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  add_bin(k, 1, 2);   // ¬a → b
  add_bin(k, -2, -3); // b → ¬c
  add_bin(k, 1, 3);   // ¬a → c
  assert(transitive_reduce_apply(k) == ApplyResult::Ok);
  assert(k.value(internalize(1)) > 0);
  assert(k.stats.transitive_units == 1);
  assert(k.stats.transitive_deleted == 0);
  assert(live(k, 1, 2) && live(k, -2, -3) && live(k, 1, 3));
}

// Kissat processes physical duplicates independently: the second copy proves
// the first redundant, then survives when re-evaluated after that tombstone.
static void test_duplicate_pair_processed_per_copy() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  add_bin(k, 1, 2); // ¬a → b   (id 1)
  add_bin(k, 1, 2); // ¬a → b   again (id 2) — two live copies of one pair
  add_bin(k, 1, 3); // a second distinct out-edge, so the source is enumerated
  assert(transitive_reduce_apply(k) == ApplyResult::Ok);
  assert(k.stats.transitive_deleted == 1);
  assert(copies(k, internalize(1), internalize(2)) == 1);
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
  // Kissat schedules both active polarities even when their source list cannot
  // produce a candidate; the rule charges the scan and returns a total no-op.
  assert(srcs.size() == 6);
  for (Lit lit = 0; lit < 6; lit++)
    assert(srcs[lit] == lit);
  assert(transitive_reduce_try(k, internalize(3)) == ApplyResult::NoOp);
  assert(transitive_reduce_try(k, internalize(1)) == ApplyResult::Ok);
  assert(!live(k, 1, 3));
  // out-of-range payloads are rejected, not asserted on
  assert(transitive_reduce_try(k, internalize(99)) == ApplyResult::NoOp);
}

// Binary ownership follows Kissat's compact (import) literal order, not the
// stable identities or the activation order.
static void test_reference_compact_order_owns_binary_candidates() {
  Kernel k;
  ScopedHeuristics heuristics(k);
  kernel_reserve_vars(k, 3);
  kernel_record_import(k, 2);
  kernel_record_import(k, 0);
  kernel_record_import(k, 1);
  heuristics_seed_activation_order(k, {0, 1, 2});
  add_bin(k, 1, 3);
  add_bin(k, 2, 3);
  std::vector<Lit> sources;
  transitive_reduce_sources(k, sources);
  assert(kernel_compact_import_lit_order(k, internalize(3)) <
         kernel_compact_import_lit_order(k, internalize(1)));
  assert(kernel_compact_import_lit_order(k, internalize(1)) <
         kernel_compact_import_lit_order(k, internalize(2)));
}

// Kissat prices the raw 4-byte watch vector: 17 large watches are 34 words,
// crossing the 32-word cache-line boundary.
static void test_raw_watch_word_tick_boundary() {
  Kernel k;
  kernel_reserve_vars(k, 19);
  const Lit source = internalize(1);
  for (int other = 2; other <= 18; other++)
    add_external_clause(k, {1, other, 19}, false, 0, ++k.next_clause_id);
  assert(k.watches.size(source) == 34);
  assert(transitive_reduce_try(k, source) == ApplyResult::NoOp);
  assert(k.stats.transitive_ticks == 3); // 1 + ceil(34 / 32)
}

// Kissat still runs (and charges) the walk for a single owned candidate.
static void test_single_candidate_walk_is_charged() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  add_bin(k, 1, 2);
  assert(transitive_reduce_try(k, internalize(1)) == ApplyResult::NoOp);
  assert(k.stats.transitive_checked == 1);
  assert(k.stats.transitive_ticks == 4); // source scan 2 + root scan 2
  assert(live(k, 1, 2));
}

// Kissat polls the effort ceiling after each candidate of a source.
static void test_candidate_budget_stops_after_charged_walk() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  add_bin(k, 1, 2);
  add_bin(k, 1, 3);
  k.work_budget = 1;
  assert(transitive_reduce_try(k, internalize(1)) == ApplyResult::NoOp);
  assert(k.stats.transitive_checked == 1);
  assert(live(k, 1, 2) && live(k, 1, 3));
}

int main() {
  test_transitive_redundancy();
  test_no_redundancy();
  test_mutual_dependency();
  test_failed_literal_committed();
  test_failed_literal_retains_earlier_deletion();
  test_no_contrapositive_self_justification();
  test_duplicate_pair_processed_per_copy();
  test_root_fixed_skipped();
  test_instance_surface();
  test_reference_compact_order_owns_binary_candidates();
  test_raw_watch_word_tick_boundary();
  test_single_candidate_walk_is_charged();
  test_candidate_budget_stops_after_charged_walk();
  puts("test_transitive_reduce OK");
  return 0;
}
