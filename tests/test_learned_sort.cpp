// Fixed private-sort differential cases, not real-instance solver execution.
#undef NDEBUG
#include "rules/analyze/learned_sort.hpp"
#include <array>
#include <cassert>
#include <cstdio>

using namespace tactix;

int main() {
  Kernel k;
  kernel_reserve_vars(k, 514);
  size_t cases = 0;
  const auto check = [&](const std::vector<Lit> &input) {
    auto expected = input;
    std::stable_sort(expected.begin() + 1, expected.end(), [&](Lit a, Lit b) {
      return k.var_level(var_of(a)) > k.var_level(var_of(b));
    });
    auto actual = input;
    const auto assigned = k.assigned;
    radix_sort_learned_by_level(k, actual);
    assert(actual == expected && actual[0] == input[0]);
    radix_sort_learned_by_level(k, actual);
    assert(actual == expected); // idempotence, including ties
    for (Var v = 0; v < k.nvars; ++v)
      assert(k.assigned[v].level == assigned[v].level);
    ++cases;
  };
  // Every combination of skipped/active bytes, including odd active-pass
  // counts (copy back), internal skipped bytes, and full-width uint32 levels.
  for (unsigned mask = 0; mask < 16; ++mask) {
    for (size_t n : {0u, 1u, 2u, 3u, 31u, 32u, 255u, 256u, 513u}) {
      for (unsigned order = 0; order < 3; ++order) {
        std::vector<Lit> input{make_lit(0, false)};
        k.assigned[0].level = UINT32_MAX;
        for (size_t i = 1; i <= n; ++i) {
          uint32_t level = 0;
          for (unsigned byte = 0; byte < 4; ++byte) {
            const uint32_t value = (mask & (1u << byte))
                ? ((i % 7) * (37 + byte * 12)) & 255u : 173u;
            level |= value << (8 * byte);
          }
          k.assigned[i].level = level;
          input.push_back(make_lit(static_cast<Var>(i), i & 1));
        }
        if (order == 1)
          std::reverse(input.begin() + 1, input.end());
        else if (order == 2 && n)
          std::rotate(input.begin() + 1, input.begin() + 1 + n / 2, input.end());
        // Reuse both oversized and undersized scratch with unrelated data.
        k.otfs_scratch.assign(order ? 700 : 1, INVALID_LIT);
        check(input);
      }
    }
  }
  // Exact byte boundaries and duplicate levels with distinct signed literals.
  constexpr std::array<uint32_t, 12> levels{
      0, 1, 255, 256, 65535, 65536, 16777215, 16777216,
      UINT32_MAX, UINT32_MAX, 256, 0};
  std::vector<Lit> input{make_lit(0, true)};
  for (size_t i = 0; i < levels.size(); ++i) {
    k.assigned[i + 1].level = levels[i];
    input.push_back(make_lit(i + 1, i & 1));
  }
  check(input);
  assert(cases == 433);
  std::printf("learned sort: %zu fixed stable-order cases PASS\n", cases);
}
