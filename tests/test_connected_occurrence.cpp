#undef NDEBUG
#include "kernel/connected_occurrence.hpp"
#include "kernel/kernel.hpp"

#include <algorithm>
#include <cstdio>
#include <vector>

using namespace tactix;
using Store = ConnectedOccurrences;
using Entry = Store::Entry;
using Image = std::vector<std::vector<Entry>>;

static void check(const Store &store, const Image &image) {
  assert(store.lits() == image.size());
  for (Lit lit = 0; lit < image.size(); ++lit) {
    const auto stream = store.entries(lit);
    assert(store.size(lit) == image[lit].size());
    assert(std::equal(stream.begin(), stream.end(), image[lit].begin(), image[lit].end()));
    std::vector<Ref> expected;
    for (const Entry entry : image[lit])
      if (!entry.binary)
        expected.push_back(entry.payload);
    const auto range = store.large_refs(lit);
    const std::vector<Ref> actual(range.begin(), range.end());
    assert(actual == expected);
    assert(range.size() == expected.size() && range.empty() == expected.empty());
    assert(store.large_size(lit) == expected.size());
    assert(store.binary_size(lit) == image[lit].size() - expected.size());
    auto a = range.begin(), b = a;
    if (a != range.end()) {
      const Ref first = *a;
      const auto old = a++;
      assert(*old == first && *b == first); // independent multipass iterators
    }
  }
}

static Store build(const Image &image) {
  Store result;
  result.begin_exact_layout(image.size());
  for (Lit lit = 0; lit < image.size(); ++lit)
    result.count(lit, static_cast<uint32_t>(image[lit].size()));
  result.finish_exact_layout();
  for (Lit lit = 0; lit < image.size(); ++lit)
    for (const Entry entry : image[lit])
      result.push(lit, entry);
  check(result, image);
  return result;
}

static size_t exhaustive() {
  const Entry alphabet[] = {Entry::from_binary(0), Entry::from_binary(7),
                            Entry::from_large(0), Entry::from_large(7)};
  size_t states = 0, words = 1;
  for (size_t length = 0; length <= 7; ++length, words *= 4) {
    for (size_t word = 0; word < words; ++word) {
      Image image(3);
      image[1] = {Entry::from_large(Entry::MAX_PAYLOAD), Entry::from_binary(Entry::MAX_PAYLOAD)};
      size_t code = word;
      for (size_t i = 0; i < length; ++i, code /= 4)
        image[0].push_back(alphabet[code % 4]);
      Store original = build(image);
      for (unsigned mask = 0; mask != 16; ++mask) {
        Store next = original; // independent owner, no self-referential alias
        Image expected = image;
        std::vector<Entry> visited;
        const auto keep = [&](Entry entry) {
          const unsigned bit = (entry.binary ? 0 : 2) + (entry.payload == 7);
          return (mask >> bit) & 1;
        };
        next.compact(0, [&](Entry entry) { visited.push_back(entry); return keep(entry); });
        assert(visited == image[0]);
        std::erase_if(expected[0], [&](Entry entry) { return !keep(entry); });
        check(next, expected);
        next.push(0, Entry::from_large(11));
        expected[0].push_back(Entry::from_large(11));
        check(next, expected);
      }
      for (size_t at = 0; at < length; ++at) {
        Store next = original;
        Image expected = image;
        next.erase_at(0, at);
        expected[0].erase(expected[0].begin() + at);
        check(next, expected);
        if (!expected[0].empty()) {
          for (const Entry entry : alphabet) {
            next.replace_at(0, 0, entry);
            expected[0][0] = entry;
            check(next, expected);
          }
        }
      }
      Store next = original;
      Image expected = image;
      const auto first = std::find(expected[0].begin(), expected[0].end(), Entry::from_large(0));
      const bool present = first != expected[0].end();
      assert(next.erase_large_once(0, 0) == present);
      if (present) expected[0].erase(first);
      check(next, expected);
      next.compact_large(0, [](Ref ref) { return ref != 7; });
      std::erase_if(expected[0], [](Entry entry) { return !entry.binary && entry.payload == 7; });
      check(next, expected);
      check(original, image);
      ++states;
    }
  }
  return states;
}

static void growth_and_pack() {
  Image image(4);
  Store store = build(image);
  for (unsigned i = 0; i < 300000; ++i) {
    const Lit lit = i % 4;
    const Entry entry = i % 3 ? Entry::from_binary(i) : Entry::from_large(i);
    store.push(lit, entry);
    image[lit].push_back(entry);
  }
  assert(store.relocations() > 0);
  check(store, image);
  for (Lit lit = 0; lit < 4; ++lit) {
    store.compact(lit, [](Entry entry) { return entry.payload < 16; });
    std::erase_if(image[lit], [](Entry entry) { return entry.payload >= 16; });
  }
  const uint64_t before = store.defrags();
  store.defrag_if_needed();
  assert(store.defrags() == before + 1);
  check(store, image);
  store.resize_lits(6); image.resize(6);
  store.push(5, Entry::from_large(0)); image[5].push_back(Entry::from_large(0));
  check(store, image);
  store.clear(0); image[0].clear(); check(store, image);
  store.release();
  assert(store.lits() == 0 && store.payload_capacity_bytes() == 0 && store.count_capacity_bytes() == 0);
  // Reuse at a smaller literal universe, then republish large-only data.
  store.begin_exact_layout(2); store.count(1, 2); store.finish_exact_layout();
  store.push(1, Entry::from_large(Entry::MAX_PAYLOAD));
  store.push(1, Entry::from_large(0));
  check(store, Image{{}, {Entry::from_large(Entry::MAX_PAYLOAD), Entry::from_large(0)}});
}

static void retained_rebuild_and_move() {
  Image large(17);
  for (unsigned i = 0; i < 4096; ++i)
    large[i % large.size()].push_back(i % 2 ? Entry::from_binary(i)
                                           : Entry::from_large(i));
  Store store = build(large);
  const void *const allocation = store.arena_data();
  const size_t capacity = store.payload_capacity_bytes();
  // Rebuild without releasing the old owner: counts must reset, payload
  // capacity must be reusable, and the new logical universe may shrink.
  const Image small{{Entry::from_binary(3), Entry::from_large(0)}, {},
                    {Entry::from_large(7), Entry::from_binary(1)}};
  store.begin_exact_layout(small.size());
  for (Lit lit = 0; lit < small.size(); ++lit)
    store.count(lit, small[lit].size());
  store.finish_exact_layout();
  for (Lit lit = 0; lit < small.size(); ++lit)
    for (Entry entry : small[lit]) store.push(lit, entry);
  check(store, small);
  assert(store.arena_data() == allocation &&
         store.payload_capacity_bytes() == capacity);
  Store moved(std::move(store));
  check(moved, small);
  Store assigned = build(large);
  assigned = std::move(moved);
  check(assigned, small);
  // Reuse moved-from owners only through full reinitialization.
  store.begin_exact_layout(1); store.count(0); store.finish_exact_layout();
  store.push(0, Entry::from_large(9));
  check(store, Image{{Entry::from_large(9)}});
  check(assigned, small);
}

int main() {
  static_assert(sizeof(Entry) == sizeof(OccIndex::Entry));
  static_assert(Entry::MAX_PAYLOAD == OccIndex::Entry::MAX_PAYLOAD);
  const size_t states = exhaustive();
  assert(states == 21845);
  growth_and_pack();
  retained_rebuild_and_move();
  std::printf("connected occurrence storage: %zu sequences, 16 stable filters each, erases/replacements, first-duplicate deletion, growth/pack/reuse PASS\n", states);
}
