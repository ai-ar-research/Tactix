#undef NDEBUG
#include "rules/congruence/conditional_index.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <random>
#include <utility>
#include <vector>

using namespace tactix;

using Pair = std::pair<size_t, size_t>;

static void compare(ConditionalIndex &index,
                    const std::vector<std::pair<Lit, Lit>> &conditions) {
  std::vector<Pair> expected, actual;
  for (size_t i = 0; i < conditions.size(); ++i)
    for (size_t j = i + 1; j < conditions.size(); ++j)
      if (conditions[i].first == negate(conditions[j].first))
        expected.emplace_back(i, j);

  index.build(conditions);
  for (size_t i = 0; i < conditions.size(); ++i) {
    const Lit complement = negate(conditions[i].first);
    for (auto it = index.after(complement, i);
         it != index.end() && it->first == complement; ++it)
      actual.emplace_back(i, it->second);
  }
  assert(actual == expected);
}

int main() {
  ConditionalIndex index;
  std::mt19937 rng(745700);
  for (unsigned n = 0; n <= 300; ++n) {
    std::vector<std::pair<Lit, Lit>> conditions;
    for (unsigned i = 0; i < n; ++i)
      conditions.emplace_back(rng() % 32, rng() % 32);
    compare(index, conditions);
    conditions.resize(n / 2);
    compare(index, conditions);
  }
  compare(index, {{0, 0}, {1, 0}, {0, 0}, {1, 0}, {0, 0}});
  compare(index, {{UINT32_MAX - 1, 0}, {UINT32_MAX, 0}});

  std::vector<std::pair<Lit, Lit>> one_sided(100000, {0, 0});
  index.build(one_sided);
  for (size_t i = 0; i < one_sided.size(); ++i)
    assert(index.after(1, i) == index.end());
  index.build({});
  assert(index.after(0, 0) == index.end());
  std::puts("Conditional index preserves exact pair order and sparse lookup");
}
