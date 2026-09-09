// Fixed exhaustive small watch layouts, not a solver/fuzz or timing run.
// Compile against the unmodified 6424 source. This freezes the contract that
// a proposed keep-binaries fast path would have to reproduce exactly.
#include "kernel/watch.hpp"
#include <cstdio>
#include <cstdlib>

using namespace tactix;

static void require(bool condition) {
  if (!condition) {
    std::fputs("binary-retention contract mismatch\n", stderr);
    std::abort();
  }
}

static uint64_t digest = 14695981039346656037ull;
static unsigned transitions = 0, conservative_binary_only = 0;

static void check(WatchLists &w, const std::vector<uint32_t> &binaries) {
  const auto old_heads = w.heads;
  auto expected_arena = w.arena;
  const auto old_holes = w.holes;
  const auto old_capacity = w.arena.capacity();
  const auto *old_data = w.arena.data();
  const auto &old = old_heads[0];
  if (old.size == binaries.size() && !old.binaries_first())
    ++conservative_binary_only;
  for (size_t i = 0; i < binaries.size(); ++i)
    expected_arena[old.offset + i] = binaries[i];

  w.compact(0, [](WatchEntry entry) { return entry.binary(); });

  require(w.arena == expected_arena);
  require(w.arena.capacity() == old_capacity && w.arena.data() == old_data);
  require(w.holes == old_holes + old.size - binaries.size());
  require(w.heads.size() == old_heads.size());
  for (size_t i = 0; i < old_heads.size(); ++i) {
    const auto &a = w.heads[i], &b = old_heads[i];
    require(a.offset == b.offset && a.capacity == b.capacity);
    require(a.size == (i ? b.size : binaries.size()));
    require(a.binary_count_and_order ==
            (i ? b.binary_count_and_order : (binaries.size() & ~uint32_t{1})));
  }
  require(w.heads[0].binary_count() == binaries.size());
  require(w.heads[0].binaries_first() && !w.heads[0].has_large());
  for (uint32_t word : w.arena) {
    digest ^= word;
    digest *= 1099511628211ull;
  }
  digest ^= w.holes;
  digest *= 1099511628211ull;
  ++transitions;
}

int main() {
  unsigned cases = 0;
  for (unsigned length = 0; length <= 8; ++length)
    for (unsigned mask = 0; mask < (1u << length); ++mask)
      for (unsigned conservative = 0; conservative < 2; ++conservative) {
        WatchLists watches;
        watches.resize_lits(4);
        watches.push_binary(1, 7); // allocated arena, untouched neighboring list
        std::vector<uint32_t> binaries;
        for (unsigned i = 0; i < length; ++i) {
          if (mask & (1u << i)) {
            // Repeated literals exercise retained duplicate copies.
            const Lit other = 2 + (i % 3);
            watches.push_binary(0, other);
            binaries.push_back(binary_watch(other).raw);
          } else {
            // Odd and even refs must never be reinterpreted as watch entries.
            watches.push_large(0, i + 10, 100 + i);
          }
          watches.push_large(2, 20 + i, 200 + i); // force slice relocations
        }
        if (conservative)
          watches.heads[0].set_binaries_first(false);
        check(watches, binaries);
        check(watches, binaries); // exact no-op, including stale tail words
        for (unsigned round = 0; round < 3; ++round) {
          watches.push_large(0, 30 + round, 300 + round);
          watches.push_binary(0, 40 + round); // mixed order again
          binaries.push_back(binary_watch(40 + round).raw);
          watches.push_binary(3, 50 + round);
          check(watches, binaries);
        }
        ++cases;
      }
  require(cases == 1022 && transitions == 5110);
  require(conservative_binary_only == 9);
  std::printf("PASS cases=%u transitions=%u conservative_binary_only=%u "
              "digest=%016llx\n", cases, transitions, conservative_binary_only,
              static_cast<unsigned long long>(digest));
}
