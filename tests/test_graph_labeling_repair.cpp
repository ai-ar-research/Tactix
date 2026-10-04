#undef NDEBUG
#include "rules/lucky/graph_labeling_repair.hpp"

#include <cassert>

using namespace tactix::graph_labeling;

static Shape graph(uint32_t mask) {
  Shape s;
  s.vertices = 4;
  s.separation = 2;
  s.adjacency.resize(4);
  uint32_t bit = 0;
  for (uint32_t a = 0; a < 4; ++a)
    for (uint32_t b = a + 1; b < 4; ++b, ++bit)
      if (mask & (1u << bit)) {
        s.edges.emplace_back(a, b);
        s.adjacency[a].push_back(b);
        s.adjacency[b].push_back(a);
      }
  return s;
}

static Score oracle(const Shape &s, const Candidate &c) {
  auto sorted = c.labels;
  std::sort(sorted.begin(), sorted.end());
  assert((sorted == std::vector<uint32_t>{0, 1, 2, 3}));
  Score result{0, 0};
  for (uint32_t a = 0; a < s.vertices; ++a)
    for (uint32_t b : s.adjacency[a]) {
      if (b <= a) continue;
      const int64_t distance = std::abs(int64_t(c.labels[a]) - c.labels[b]);
      if (distance >= s.separation) continue;
      ++result.first;
      const uint64_t deficit = s.separation - distance;
      result.second += deficit * deficit;
    }
  return result;
}

int main() {
  auto always = [](uint64_t) { return true; };
  for (uint32_t mask = 0; mask < 64; ++mask) {
    Shape s = graph(mask);
    Candidate c;
    c.labels = {0, 1, 2, 3};
    do {
      c.score = oracle(s, c);
      for (uint32_t a = 0; a < 4; ++a)
        for (uint32_t b = a; b < 4; ++b) {
          ScoreDelta delta;
          assert(swap_delta(s, c, a, b, always, delta));
          Candidate swapped = c;
          std::swap(swapped.labels[a], swapped.labels[b]);
          assert(offset_score(c.score, delta) == oracle(s, swapped));
          auto best = c, current = c;
          std::vector<uint32_t> at(4);
          for (uint32_t v = 0; v < 4; ++v) at[c.labels[v]] = v;
          assert(install_swap(current, best, at, a, b, delta, always));
          assert(current.labels == swapped.labels && current.score == oracle(s, current));
          assert(best.score <= c.score && best.score == oracle(s, best));
          for (uint32_t v = 0; v < 4; ++v) assert(at[current.labels[v]] == v);
        }
    } while (std::next_permutation(c.labels.begin(), c.labels.end()));
    c.score = oracle(s, c);
    Candidate repaired = c, annealed = c;
    uint64_t rounds = 0, attempts = 0;
    assert(repair(s, 3, 20, 123, 456, {}, {}, always, repaired, rounds, attempts));
    assert(repaired.score <= c.score && repaired.score == oracle(s, repaired));
    assert(rounds <= 3 && attempts <= 60);
    assert(anneal(s, 50, 456, {}, always, annealed, attempts));
    assert(annealed.score <= c.score && annealed.score == oracle(s, annealed));
    assert(attempts <= 50);
    Candidate repeated = c;
    assert(repair(s, 3, 20, 123, 456, {}, {}, always, repeated, rounds, attempts));
    assert(repeated.labels == repaired.labels && repeated.score == repaired.score);
  }
  // Every work cutoff of an impossible clique retains a valid, correctly
  // scored best candidate, including cutoffs inside delta evaluation/export.
  Shape s = graph(63);
  Candidate initial;
  initial.labels = {0, 1, 2, 3};
  initial.score = oracle(s, initial);
  for (bool use_anneal : {false, true}) {
    uint64_t work = 0, rounds = 0, attempts = 0;
    const auto count = [&](uint64_t amount) { work += amount; return true; };
    Candidate complete = initial;
    if (use_anneal) assert(anneal(s, 5, 456, {}, count, complete, attempts));
    else assert(repair(s, 1, 3, 123, 456, {}, {}, count, complete, rounds, attempts));
    for (uint64_t limit = 0; limit < work; ++limit) {
      uint64_t used = 0;
      const auto bounded = [&](uint64_t amount) {
        if (amount > limit - used) return false;
        used += amount;
        return true;
      };
      Candidate partial = initial;
      const bool finished = use_anneal
          ? anneal(s, 5, 456, {}, bounded, partial, attempts)
          : repair(s, 1, 3, 123, 456, {}, {}, bounded, partial, rounds, attempts);
      assert(!finished && used <= limit);
      assert(partial.score <= initial.score && partial.score == oracle(s, partial));
    }
  }
}
