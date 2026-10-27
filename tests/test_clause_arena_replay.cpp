// Compile this same fixture against the actual parent and candidate libraries.
// Compare capacities, initialized clause words/IDs and all post-GC words/refs.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <utility>
#include <vector>
using namespace tactix;
static uint64_t digest = UINT64_C(1469598103934665603);
static void hash(uint64_t n) { digest = (digest ^ n) * UINT64_C(1099511628211); }

int main() {
  size_t clauses = 0, boundaries = 0;
  for (unsigned mode = 0; mode < 2; ++mode) {
    Kernel k;
    kernel_reserve_vars(k, 256);
    std::vector<uint32_t> capacity_oracle;
    uint64_t id = UINT64_C(0x1234567800000000);
    for (unsigned round = 0; round < 16; ++round) {
      for (unsigned i = 0; i < 1024; ++i) {
        const unsigned size = 3 + (i % 11);
        std::vector<Lit> lits;
        for (unsigned j = 0; j < size; ++j)
          lits.push_back(make_lit((i * 17 + j * 13 + round) % 256, (i + j) & 1));
        const size_t old_capacity = k.arena.capacity();
        const Ref expected = static_cast<Ref>(k.arena.size());
        capacity_oracle.resize(capacity_oracle.size() + clause_words(size));
        const Ref ref = kernel_add_large_clause(k, lits.data(), size,
            (i % 3) == 0, i % 8, ++id);
        assert(ref == expected && k.arena.size() == capacity_oracle.size());
        // ReallocWords preserves libstdc++'s growth policy. libc++ may choose
        // different capacities; the size, contents and GC checks still apply.
#ifdef __GLIBCXX__
        assert(k.arena.capacity() == capacity_oracle.capacity());
#endif
        assert(k.arena.capacity() >= k.arena.size());
        const Clause *c = clause_at(k.arena, ref);
        assert(c->id() == id && c->size == size);
        assert(std::equal(c->begin(), c->end(), lits.begin()));
        if (old_capacity != k.arena.capacity()) {
          ++boundaries;
          hash(k.arena.size()); hash(k.arena.capacity()); hash(ref);
          for (const uint32_t word : k.arena) hash(word);
        }
        if (i % 5 == 0)
          kernel_delete_clause(k, clause_at(k.arena, ref));
        ++clauses;
      }
      OccIndex dense;
      if (mode) {
        kernel_flush_large_watches(k);
        kernel_dense_connect(k, dense, true);
      }
      kernel_collect_garbage(k);
      // The collector still reserves exactly its retained image, including
      // its ordering/forwarding choices. Match std::vector's fresh reserve.
      std::vector<uint32_t> fresh;
      fresh.reserve(k.arena.size());
      fresh.resize(k.arena.size());
      capacity_oracle.swap(fresh);
      assert(k.arena.capacity() == capacity_oracle.capacity());
      hash(k.arena.size()); hash(k.arena.capacity());
      for (const uint32_t word : k.arena) hash(word);
      const Arena copy = k.arena;
      assert(copy == k.arena);
      Arena assigned;
      assigned = copy;
      assert(assigned == copy);
      Arena moved = std::move(assigned);
      assert(moved == copy && assigned.empty());
      if (mode) {
        kernel_dense_disconnect(k);
        kernel_rebuild_large_watches(k, false);
      }
      kernel_check_invariants(k);
    }
  }
  std::printf("clause-arena clauses=%zu growth_boundaries=%zu collection_rounds=32 digest=%016llx\n",
              clauses, boundaries, (unsigned long long)digest);
}
