#pragma once
// The independent-set owner's encoding: vertex variables 1..N, conflict
// binaries for the edges, then a layered counter with min(i, K) + 1 one-hot
// states after vertex i whose last layer forbids counts below K. No unit
// clause is used, so the parser keeps the (K + 1)-wide terminal clause.
#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <utility>
#include <vector>

struct IndependentSetEncoding {
  uint32_t vertices = 0;
  uint32_t target = 0;
  uint32_t variables = 0;
  std::vector<std::vector<int32_t>> layers;
  std::vector<std::pair<uint32_t, uint32_t>> edges;
  std::vector<std::vector<int32_t>> clauses;
};

inline IndependentSetEncoding independent_set_encoding(
    uint32_t vertices, uint32_t target,
    std::vector<std::pair<uint32_t, uint32_t>> edges) {
  IndependentSetEncoding result;
  result.vertices = vertices;
  result.target = target;
  result.edges = std::move(edges);
  result.layers.resize(vertices + 1);
  int32_t next = static_cast<int32_t>(vertices + 1);
  for (uint32_t layer = 1; layer <= vertices; ++layer)
    for (uint32_t state = 0; state <= std::min(layer, target); ++state)
      result.layers[layer].push_back(next++);
  result.variables = static_cast<uint32_t>(next - 1);
  const auto add = [&](std::initializer_list<int32_t> literals) {
    result.clauses.emplace_back(literals);
  };
  for (auto [first, second] : result.edges)
    add({-static_cast<int32_t>(first), -static_cast<int32_t>(second)});
  add({1, result.layers[1][0]});
  add({-1, result.layers[1][1]});
  for (uint32_t layer = 2; layer <= vertices; ++layer) {
    const auto &previous = result.layers[layer - 1];
    const auto &current = result.layers[layer];
    for (uint32_t state = 0; state < previous.size(); ++state) {
      add({-previous[state], static_cast<int32_t>(layer), current[state]});
      add({-previous[state], -static_cast<int32_t>(layer),
           current[std::min(state + 1, target)]});
    }
    for (uint32_t first = 0; first < current.size(); ++first)
      for (uint32_t second = first + 1; second < current.size(); ++second)
        add({-current[first], -current[second]});
  }
  const auto &terminal = result.layers[vertices];
  for (uint32_t state = 0; state < target; ++state)
    add({-terminal[state], terminal[target]});
  result.clauses.push_back(terminal);
  return result;
}
