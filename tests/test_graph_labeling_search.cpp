#undef NDEBUG
#include "rules/lucky/graph_labeling_search.hpp"

#include <cassert>
#include <numeric>

using namespace tactix::graph_labeling;

static Score independent_score(const Shape &shape, const Candidate &candidate) {
  auto sorted = candidate.labels;
  std::sort(sorted.begin(), sorted.end());
  for (uint32_t i = 0; i < shape.vertices; ++i) assert(sorted[i] == i);
  uint64_t bad = 0, penalty = 0;
  for (uint32_t v = 0; v < shape.vertices; ++v)
    for (uint32_t u : shape.adjacency[v]) {
      if (u <= v) continue;
      const int64_t distance = std::abs(int64_t(candidate.labels[v]) -
                                       int64_t(candidate.labels[u]));
      if (distance >= shape.separation) continue;
      ++bad;
      const uint64_t deficit = shape.separation - distance;
      penalty += deficit * deficit;
    }
  return {bad, penalty};
}

int main() {
  {
    // Final repair can undo the seed's reflection convention. Recover it
    // without changing any pairwise distance or partially mutating on a cut.
    Candidate original;
    original.labels = {3, 0, 2, 1};
    original.score = {7, 19};
    for (uint64_t limit = 0; limit <= 4; ++limit) {
      Candidate candidate = original;
      uint64_t used = 0;
      const auto charge = [&](uint64_t amount) {
        if (amount > limit - used) return false;
        used += amount;
        return true;
      };
      assert(normalize_reflection(candidate, charge) == (limit == 4));
      assert(candidate.score == original.score);
      if (limit < 4) {
        assert(candidate.labels == original.labels && used == 0);
        continue;
      }
      assert((candidate.labels == std::vector<uint32_t>{0, 3, 1, 2}));
      for (size_t a = 0; a < 4; ++a)
        for (size_t b = 0; b < 4; ++b)
          assert(std::abs(int(candidate.labels[a]) - int(candidate.labels[b])) ==
                 std::abs(int(original.labels[a]) - int(original.labels[b])));
      assert(normalize_reflection(candidate, charge));
      assert(used == 4); // Already normalized: no additional work or mutation.
    }
  }
  // Exhaust every simple four-vertex graph, including disconnected graphs and
  // an impossible separation. Check complete permutations and objective values
  // independently of the constructor's edge scorer, plus deterministic replay.
  for (uint32_t mask = 0; mask < 64; ++mask) {
    Shape shape;
    shape.vertices = 4;
    shape.separation = 3;
    shape.adjacency.resize(4);
    uint32_t bit = 0;
    for (uint32_t a = 0; a < 4; ++a)
      for (uint32_t b = a + 1; b < 4; ++b, ++bit)
        if (mask & (1u << bit)) {
          shape.edges.emplace_back(a, b);
          shape.adjacency[a].push_back(b);
          shape.adjacency[b].push_back(a);
        }
    Candidate candidate;
    uint64_t work = 0;
    const auto charge = [&](uint64_t amount) { work += amount; return true; };
    uint32_t completed = 0;
    assert(order_seeds(shape, charge, candidate, completed));
    assert(completed == 24 && candidate.score == independent_score(shape, candidate));
    Candidate replay;
    assert(order_seeds(shape, charge, replay, completed));
    assert(replay.labels == candidate.labels && replay.score == candidate.score);
    // With all starts/choices allowed this finite search must agree with a
    // brute-force permutation oracle, even when the seed family misses a model.
    Candidate brute;
    brute.labels = {0, 1, 2, 3};
    bool exists = false;
    do {
      exists |= independent_score(shape, brute).first == 0;
    } while (std::next_permutation(brute.labels.begin(), brute.labels.end()));
    Candidate searched = candidate;
    uint64_t nodes = 0;
    const WindowLimits limits{1000, 4, 4, 123};
    const auto unbounded = [](uint64_t) { return true; };
    const auto status = window_search(shape, limits, unbounded, searched, nodes);
    assert((status == WindowStatus::Found) == exists);
    assert(status != WindowStatus::Limited && nodes <= limits.nodes);
    if (exists) {
      assert(searched.score.first == 0);
      assert(searched.score == independent_score(shape, searched));
    } else assert(searched.labels == candidate.labels);
    if (!mask) {
      uint64_t dfs_work = 0;
      const auto dfs_count = [&](uint64_t amount) { dfs_work += amount; return true; };
      assert(window_search(shape, limits, dfs_count, searched, nodes) == WindowStatus::Found);
      for (uint64_t limit = 0; limit < dfs_work; ++limit) {
        uint64_t used = 0;
        const auto bounded = [&](uint64_t amount) {
          if (amount > limit - used) return false;
          used += amount;
          return true;
        };
        searched = candidate;
        assert(window_search(shape, limits, bounded, searched, nodes) == WindowStatus::Limited);
        assert(searched.labels == candidate.labels && searched.score == candidate.score);
      }
    }
    if (mask == 0) assert(candidate.score.first == 0);
    if (mask != 63) continue;
    // Every possible early stop retains an evaluated permutation, never an
    // interrupted sort or a half-built assignment.
    const uint64_t full_work = work / 2;
    for (uint64_t limit = 0; limit < full_work; ++limit) {
      uint64_t used = 0;
      const auto bounded = [&](uint64_t amount) {
        if (amount > limit - used) return false;
        used += amount;
        return true;
      };
      Candidate partial;
      assert(!order_seeds(shape, bounded, partial, completed));
      assert(used <= limit && completed < 24);
      if (completed) assert(partial.score == independent_score(shape, partial));
      else assert(partial.labels.empty());
    }
  }
}
