// Fixed bookkeeping transitions, linked against actual old and new objects.
#include "unit_test.hpp"
#include "fnv_digest.hpp"
#include "kernel_fixture.hpp"
#include "occurrence_test_utils.hpp"
#include "rules/substitute/substitute.hpp"

static void check(unsigned size, unsigned shape, bool redundant,
                  unsigned dense_mode, bool transaction) {
  Kernel k = kernel_with_vars(4 * size + 8);
  std::vector<Lit> original;
  for (unsigned i = 0; i < size; ++i)
    original.push_back(make_lit(i, i & 1));
  const Lit neighbour[] = {original[0], original[1], make_lit(3 * size, false)};
  kernel_add_large_clause(k, neighbour, 3, false, 0);
  const Ref ref = kernel_add_large_clause(k, original.data(), size, redundant, 2);
  kernel_add_large_clause(k, neighbour, 3, false, 0);
  kernel_add_binary_clause(k, original[0], make_lit(3 * size + 1, false));
  const auto arena_before = k.arena;
  std::vector<Lit> image = original;
  apply_cursor_image_shape(image, size, shape);
  OccIndex occ;
  if (dense_mode) kernel_dense_connect(k, occ, dense_mode == 2);
  SweepSubstitutionSession session;
  session.begin(k);
  session.prime_large(k, ref);
  if (transaction && dense_mode && !redundant) {
    std::vector<Lit> removed, added, sources;
    for (Lit l : original)
      if (std::find(image.begin(), image.end(), l) == image.end())
        removed.push_back(l);
    for (Lit l : image)
      if (std::find(original.begin(), original.end(), l) == original.end())
        added.push_back(l);
    assert(added.size() <= removed.size());
    std::vector<std::pair<Lit, Lit>> map;
    for (size_t i = 0; i < added.size(); ++i) {
      map.push_back({removed[i], added[i]});
      map.push_back({negate(removed[i]), negate(added[i])});
      sources.push_back(removed[i]);
      sources.push_back(negate(removed[i]));
    }
    std::sort(map.begin(), map.end());
    session.rewrite_binary_occurrences(k, map, sources, {});
  }
  session.rewrite_large_in_place(k, ref, image.data(), image.size());
  // Pending moves are private: flushing exposes their complete resulting order.
  session.flush_moves(k);
  assert(k.arena == arena_before); // this helper must not rewrite the formula
  hash(size); hash(shape); hash(redundant); hash(dense_mode); hash(transaction);
  hash(session.changed); hash(session.dense_enabled);
  hash_vector(session.dense_initialized_lits);
  hash_vector(session.ledger_dirty);
  hash_vector(session.ledger_dirty_mark);
  hash_vector(session.dirty_binary_owners);
  for (Lit l = 0; l < 2 * k.nvars; ++l) {
    const auto found = session.dense_occurrences.find(l);
    hash(found != session.dense_occurrences.end());
    if (found == session.dense_occurrences.end()) {
      assert(redundant || !dense_mode ||
             std::find(image.begin(), image.end(), l) == image.end());
      continue;
    }
    const auto &list = found->second;
    hash(list.initialized); hash(list.entries.size()); hash(list.entries.capacity());
    unsigned copies = 0;
    for (const auto entry : list.entries) {
      hash(entry.binary); hash(entry.payload);
      copies += !entry.binary && entry.payload == ref;
    }
    const bool expected = !redundant && dense_mode &&
        std::find(image.begin(), image.end(), l) != image.end();
    assert(copies == unsigned(expected));
  }
  hash_vector(k.arena);
}

int main() {
  unsigned cases = 0;
  for (unsigned size : {3u, 4u, 7u, 16u, 31u, 64u, 129u})
    for (unsigned shape = 0; shape != 7; ++shape)
      for (bool redundant : {false, true})
        for (unsigned dense = 0; dense != 3; ++dense)
          for (bool transaction : {false, true}) {
            check(size, shape, redundant, dense, transaction);
            ++cases;
          }
  std::printf("sweep-cursor cases=%u state=%016llx\n", cases,
              static_cast<unsigned long long>(digest));
}
