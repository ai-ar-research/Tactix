// kernel_delete_binary removes both mirror watch entries, retires a live root
// reason to UNIT_REASON, and keeps binary_ids size-monotone. Invariant: while
// a pair is live, its id names a clause the checker still holds. See
// docs/implementation-notes.md §14.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/propagate/propagate.hpp"
#include "proof/proof.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

using namespace tactix;

static void test_compact_drat_binary_table_keeps_keys_without_ids() {
  BinaryIdMap ids;
  ids.discard_values();
  ids.reserve(100);
  const uint64_t key = kernel_binary_key(internalize(1), internalize(-2));
  ids.insert_or_assign(key, 77);
  assert(!ids.tracks_values());
  assert(ids.size() == 1 && ids.count(key));
  assert(ids.get(key) == 1 && ids.at(key) == 1);
  ids.insert_or_assign(key, 99);
  assert(ids.size() == 1 && ids.get(key) == 1);
  uint64_t visited_key = 0, visited_id = 0;
  ids.for_each([&](uint64_t k, uint64_t id) {
    visited_key = k;
    visited_id = id;
  });
  assert(visited_key == key && visited_id == 1);
}

static void test_storage_free_drat_archive_counts_distinct_adds() {
  Kernel k;
  k.binary_ids.discard_storage();
  kernel_reserve_vars(k, 3);
  const Lit a = internalize(1), b = internalize(2);
  kernel_add_binary_clause(k, a, b, 7);
  kernel_add_binary_clause(k, a, b, 8);
  assert(!k.binary_ids.tracks_keys() && !k.binary_ids.tracks_values());
  assert(k.binary_ids.size() == 1);
  assert(kernel_binary_id(k, a, b) == 1);
  assert(kernel_binary_live(k, a, b));
}

static void test_populated_archive_can_release_storage_without_losing_count() {
  BinaryIdMap ids;
  ids.discard_values();
  const uint64_t ab = kernel_binary_key(internalize(1), internalize(2));
  const uint64_t ac = kernel_binary_key(internalize(1), internalize(3));
  ids.insert_or_assign(ab, 7);
  ids.insert_or_assign(ac, 8);
  assert(ids.size() == 2 && ids.count(ab) && ids.count(ac));
  ids.discard_storage();
  assert(!ids.tracks_keys() && !ids.tracks_values());
  assert(ids.size() == 2);
  ids.insert_or_assign_known(kernel_binary_key(internalize(2), internalize(3)),
                             9, true);
  assert(ids.size() == 3);
}

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

static void test_add_missing_mirror_does_not_duplicate_stable_direction() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit stable = internalize(1), moved = internalize(2);
  kernel_add_binary_clause(k, stable, moved, 7);
  k.factor_marks[stable] = 0;
  k.factor_marks[moved] = 0;
  const uint32_t stable_size = k.watches.size(stable);
  assert(k.watches.size(moved) == 1);
  k.watches.resize_live(moved, 0); // pose an in-place rewritten stable watch
  k.watches.heads[moved].set_layout(0);

  kernel_add_binary_mirror(k, stable, moved, 17);
  assert(k.watches.size(stable) == stable_size);
  assert(k.watches.size(moved) == 1);
  assert(kernel_binary_id(k, stable, moved) == 17);
  assert(!k.factor_marks[stable] && !k.factor_marks[moved]);
  kernel_check_invariants(k);
}

static void test_publish_rebuilt_binary_does_not_rearm_added_material() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit a = internalize(1), b = internalize(2);
  kernel_add_binary_clause(k, a, b, 7);
  k.factor_marks[a] = 0;
  k.factor_marks[b] = 0;

  kernel_publish_binary_clause(k, a, b, 17, false);
  assert(kernel_binary_id(k, a, b) == 17);
  assert(!k.factor_marks[a] && !k.factor_marks[b]);
  kernel_check_invariants(k);
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

static void test_dsr_deletes_each_physical_copy() {
  for (const auto format : {ProofFormat::DsrAscii, ProofFormat::DratAscii,
                             ProofFormat::Lrat}) {
    for (const unsigned copies : {2u, 3u, 5u}) {
      Kernel k;
      kernel_reserve_vars(k, 2);
      char path[] = "/tmp/tactix-copy-proof-XXXXXX";
      const int fd = mkstemp(path);
      assert(fd >= 0);
      close(fd);
      Proof proof;
      assert(proof.open(path, format));
      k.proof = &proof;
      const Lit a = internalize(1), b = internalize(2);
      for (unsigned i = 0; i < copies; ++i)
        kernel_add_binary_clause(k, a, b, ++k.next_clause_id);
      const uint64_t retained_id = kernel_binary_id(k, a, b);
      for (unsigned removed = 1; removed <= copies; ++removed) {
        assert(kernel_delete_binary_one_copy(k, a, b));
        assert(kernel_binary_copies(k, a, b) == copies - removed);
        assert(kernel_binary_id(k, a, b) == retained_id);
        proof.flush();
        FILE *input = std::fopen(path, "rb");
        assert(input);
        std::string bytes;
        for (int c; (c = std::fgetc(input)) != EOF;)
          bytes.push_back(static_cast<char>(c));
        assert(!std::ferror(input));
        std::fclose(input);
        size_t deletions = 0;
        for (const char c : bytes) deletions += c == 'd';
        const size_t expected = format == ProofFormat::DsrAscii ? removed :
                                removed == copies ? 1u : 0u;
        assert(deletions == expected);
        if (format == ProofFormat::DsrAscii)
          assert(bytes == [&] {
            std::string expected_bytes;
            for (unsigned n = 0; n < removed; ++n)
              expected_bytes += "d 1 2 0\n";
            return expected_bytes;
          }());
        kernel_check_invariants(k);
      }
      assert(std::remove(path) == 0);
    }
  }
}

static void test_delete_one_duplicate_copy_keeps_pair_named() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit a = internalize(1), b = internalize(2);
  kernel_add_binary_clause(k, a, b, 3);
  kernel_add_binary_clause(k, a, b, 4);
  assert(kernel_delete_binary_one_copy(k, a, b));
  assert(kernel_binary_copies(k, a, b) == 1);
  assert(kernel_binary_live(k, a, b));
  assert(kernel_binary_id(k, a, b) == 4);
  assert(k.stats.binaries_deleted == 1);
  assert(kernel_delete_binary_one_copy(k, a, b));
  assert(!kernel_binary_live(k, a, b));
  assert(k.stats.binaries_deleted == 2);
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

// Strict DSR deletion semantics require the derived unit to be written while
// its binary reason is still present. This pins both the content and ordering
// of that transition instead of relying on a later propagation drain.
static void test_dsr_materializes_root_reason_before_binary_delete() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit a = internalize(1), b = internalize(2);
  kernel_add_binary_clause(k, a, b, 9);
  kernel_assign_unit(k, negate(a));
  kernel_assign_binary(k, b, a);

  char path[] = "/tmp/tactix-root-reason-proof-XXXXXX";
  const int fd = mkstemp(path);
  assert(fd >= 0);
  close(fd);
  Proof proof;
  assert(proof.open(path, ProofFormat::DsrAscii));
  k.proof = &proof;
  assert(kernel_delete_binary(k, a, b));
  proof.flush();

  FILE *input = std::fopen(path, "rb");
  assert(input);
  std::string bytes;
  for (int c; (c = std::fgetc(input)) != EOF;)
    bytes.push_back(static_cast<char>(c));
  assert(!std::ferror(input));
  std::fclose(input);
  assert(bytes == "2 0\nd 1 2 0\n");
  assert(k.assigned[var_of(b)].reason == UNIT_REASON);
  assert(std::remove(path) == 0);
}

static void test_dsr_materializes_root_reason_before_large_delete() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit a = internalize(1), b = internalize(2), c = internalize(3);
  const Lit clause[3] = {a, b, c};
  const Ref ref = kernel_add_large_clause(k, clause, 3, false, 0, 9);
  kernel_assign_unit(k, negate(a));
  kernel_assign_unit(k, negate(b));
  kernel_assign_ref(k, c, ref);

  char path[] = "/tmp/tactix-large-root-reason-proof-XXXXXX";
  const int fd = mkstemp(path);
  assert(fd >= 0);
  close(fd);
  Proof proof;
  assert(proof.open(path, ProofFormat::DsrAscii));
  k.proof = &proof;
  kernel_delete_clause(k, clause_at(k.arena, ref));
  proof.flush();

  FILE *input = std::fopen(path, "rb");
  assert(input);
  std::string bytes;
  for (int ch; (ch = std::fgetc(input)) != EOF;)
    bytes.push_back(static_cast<char>(ch));
  assert(!std::ferror(input));
  std::fclose(input);
  assert(bytes == "3 0\nd 1 2 3 0\n");
  assert(k.assigned[var_of(c)].reason == UNIT_REASON);
  assert(std::remove(path) == 0);
}

// An in-place clause rewrite emits a proof deletion for the old image but
// keeps the arena Ref. OTFS uses that Ref as a non-root implication reason;
// root-unit retirement must leave it intact for the replacement image.
static void test_large_rewrite_preserves_nonroot_reason() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit a = internalize(1), b = internalize(2);
  const Lit c = internalize(3), d = internalize(4);
  const Lit original[4] = {a, b, c, d};
  const Ref ref = kernel_add_large_clause(k, original, 4, false, 0, 9);
  kernel_assign_decision(k, negate(a));
  kernel_assign_decision(k, negate(b));
  kernel_assign_decision(k, negate(d));
  kernel_assign_ref(k, c, ref);
  const Var vc = var_of(c);
  assert(k.assigned[vc].level == 3 && k.assigned[vc].reason == ref);

  const Lit replacement[3] = {b, c, d};
  k.next_clause_id = 10;
  kernel_replace_large_clause_in_place(k, ref, k.next_clause_id, replacement,
                                       3, 0);
  assert(!clause_at(k.arena, ref)->garbage());
  assert(k.assigned[vc].level == 3 && k.assigned[vc].reason == ref);
  kernel_check_invariants(k);
}

// Under reason jumping the recorded reason is a virtual resolvent, so the
// clause the LRAT chain cites is (lit v jumped_over), which the reason field
// no longer names. The live-reason guard must protect that pair too.
// The default follows Kissat; keep the assertion here to pin that wiring.
static void test_jumped_reason_protects_its_antecedent() {
  Kernel k;
  assert(k.jump_reasons);
  k.jump_reasons_bigbig = true; // ordinary propagation is classification-gated
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
  test_compact_drat_binary_table_keeps_keys_without_ids();
  test_storage_free_drat_archive_counts_distinct_adds();
  test_populated_archive_can_release_storage_without_losing_count();
  test_delete_shrinks_both_lists();
  test_readd_after_delete_installs_fresh_id();
  test_add_missing_mirror_does_not_duplicate_stable_direction();
  test_publish_rebuilt_binary_does_not_rearm_added_material();
  test_duplicate_pair_refused();
  test_dsr_deletes_each_physical_copy();
  test_delete_one_duplicate_copy_keeps_pair_named();
  test_delete_idempotent();
  test_reason_binary_retired_to_unit();
  test_dsr_materializes_root_reason_before_binary_delete();
  test_dsr_materializes_root_reason_before_large_delete();
  test_large_rewrite_preserves_nonroot_reason();
  test_jumped_reason_protects_its_antecedent();
  puts("test_binary_deletion OK");
  return 0;
}
