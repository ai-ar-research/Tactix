#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <numeric>
#include <set>
#include <vector>

namespace tactix::exact_cover_semantic {

// An occurrence is local to one immutable source snapshot.  It deliberately
// does not pretend to be an LRAT identity: binary identities are not retained
// in every kernel proof mode.
struct Premise {
  uint64_t occurrence = 0;
  std::vector<int32_t> literals;
  bool operator==(const Premise &) const = default;
};

struct Block {
  // The signed literal made true when this exact-cover item is selected.
  std::vector<int32_t> items;
  // Row-major provenance for the binary clauses excluding two items.  The
  // diagonal is zero; both symmetric entries name the same occurrence.
  std::vector<uint64_t> exclusions;
};

struct Cover {
  uint64_t premise = 0;
  std::vector<int32_t> items;
};

struct Item {
  int32_t literal = 0;
  uint32_t block = 0;
  std::array<uint32_t, 2> covers{};
};

struct Image {
  uint32_t arity = 0;
  std::vector<Block> blocks;
  std::vector<Cover> covers;
  std::vector<Item> items;
};

inline uint32_t variable(int32_t literal) {
  return static_cast<uint32_t>(literal < 0 ? -int64_t(literal) : literal);
}

namespace detail {

struct DisjointSets {
  std::vector<uint32_t> parent;
  explicit DisjointSets(size_t size) : parent(size) {
    std::iota(parent.begin(), parent.end(), uint32_t{0});
  }
  uint32_t find(uint32_t item) {
    while (parent[item] != item) {
      parent[item] = parent[parent[item]];
      item = parent[item];
    }
    return item;
  }
  void unite(uint32_t first, uint32_t second) {
    first = find(first);
    second = find(second);
    if (first != second) parent[second] = first;
  }
};

} // namespace detail

// Recognize the complete signed SGEN incidence theory used by LymphoSAT:
// every binary clause excludes two items in one arity-sized block, every
// block contains the complete pairwise exclusion clique, there are twice as
// many covering clauses as blocks, and every item occurs in exactly two
// distinct covers.  Every source clause must belong to that theory.
//
// The caller supplies a monotone work/termination meter.  `out` is assigned
// only after all checks succeed, so exhaustion and near misses preserve both
// the formula and any previously published image.
template <class Charge>
bool recognize(const std::vector<Premise> &source, uint32_t arity,
               uint64_t maximum_items, Charge &&charge, Image &out) {
  if (arity < 3 || !maximum_items || source.empty()) return false;

  struct Edge {
    int32_t first = 0, second = 0;
    uint64_t premise = 0;
  };
  std::vector<Edge> edges;
  std::vector<Cover> covers;
  std::set<uint64_t> occurrences;
  std::set<std::vector<int32_t>> clauses;
  std::set<int32_t> item_literals;

  for (const Premise &premise : source) {
    if (!charge(uint64_t{1} + premise.literals.size()) ||
        !premise.occurrence || !occurrences.insert(premise.occurrence).second)
      return false;
    if (premise.literals.size() != 2 && premise.literals.size() != arity)
      return false;
    std::vector<int32_t> normalized = premise.literals;
    for (int32_t literal : normalized)
      if (!literal || literal == INT32_MIN) return false;
    std::sort(normalized.begin(), normalized.end());
    if (std::adjacent_find(normalized.begin(), normalized.end()) !=
        normalized.end())
      return false;
    for (size_t i = 1; i < normalized.size(); ++i)
      if (variable(normalized[i - 1]) == variable(normalized[i])) return false;
    if (!clauses.insert(normalized).second) return false;

    if (normalized.size() == 2) {
      const int32_t first = -normalized[0], second = -normalized[1];
      edges.push_back({first, second, premise.occurrence});
      item_literals.insert(first);
      item_literals.insert(second);
    } else {
      covers.push_back({premise.occurrence, std::move(normalized)});
    }
  }
  if (edges.empty() || covers.empty() || item_literals.size() > maximum_items)
    return false;

  std::vector<int32_t> literals(item_literals.begin(), item_literals.end());
  std::map<int32_t, uint32_t> index;
  std::map<uint32_t, int32_t> orientation;
  for (uint32_t i = 0; i < literals.size(); ++i) {
    if (!charge(1)) return false;
    const uint32_t variable_id = variable(literals[i]);
    const auto [where, inserted] = orientation.emplace(variable_id, literals[i]);
    if (!inserted && where->second != literals[i]) return false;
    index.emplace(literals[i], i);
  }

  detail::DisjointSets sets(literals.size());
  std::map<std::pair<uint32_t, uint32_t>, uint64_t> edge_premise;
  for (const Edge &edge : edges) {
    if (!charge(2)) return false;
    uint32_t first = index.at(edge.first), second = index.at(edge.second);
    if (first > second) std::swap(first, second);
    if (first == second ||
        !edge_premise.emplace(std::make_pair(first, second), edge.premise)
             .second)
      return false;
    sets.unite(first, second);
  }

  std::map<uint32_t, std::vector<uint32_t>> components;
  for (uint32_t i = 0; i < literals.size(); ++i) {
    if (!charge(1)) return false;
    components[sets.find(i)].push_back(i);
  }
  if (components.empty() || covers.size() != uint64_t{2} * components.size())
    return false;

  Image image;
  image.arity = arity;
  std::vector<uint32_t> block_of(literals.size(), UINT32_MAX);
  for (auto &[root, members] : components) {
    (void)root;
    if (members.size() != arity || !charge(uint64_t{arity} * arity))
      return false;
    std::sort(members.begin(), members.end(), [&](uint32_t first, uint32_t second) {
      return literals[first] < literals[second];
    });
    Block block;
    block.exclusions.assign(size_t(arity) * arity, 0);
    const uint32_t block_index = static_cast<uint32_t>(image.blocks.size());
    for (uint32_t member : members) {
      block.items.push_back(literals[member]);
      block_of[member] = block_index;
    }
    for (uint32_t first = 0; first < arity; ++first)
      for (uint32_t second = first + 1; second < arity; ++second) {
        uint32_t a = members[first], b = members[second];
        if (a > b) std::swap(a, b);
        const auto found = edge_premise.find({a, b});
        if (found == edge_premise.end()) return false;
        block.exclusions[size_t(first) * arity + second] = found->second;
        block.exclusions[size_t(second) * arity + first] = found->second;
      }
    image.blocks.push_back(std::move(block));
  }
  // A complete clique contains every admitted edge; this also rules out a
  // duplicate or cross-block binary occurrence hidden by connectivity.
  const uint64_t expected_edges =
      uint64_t{image.blocks.size()} * arity * (arity - 1) / 2;
  if (edge_premise.size() != expected_edges) return false;

  std::vector<std::vector<uint32_t>> item_covers(literals.size());
  for (Cover &cover : covers) {
    if (!charge(uint64_t{1} + cover.items.size())) return false;
    const uint32_t cover_index = static_cast<uint32_t>(image.covers.size());
    for (int32_t literal : cover.items) {
      const auto found = index.find(literal);
      if (found == index.end()) return false;
      item_covers[found->second].push_back(cover_index);
    }
    image.covers.push_back(std::move(cover));
  }

  image.items.reserve(literals.size());
  for (uint32_t i = 0; i < literals.size(); ++i) {
    if (!charge(1) || block_of[i] == UINT32_MAX || item_covers[i].size() != 2 ||
        item_covers[i][0] == item_covers[i][1])
      return false;
    image.items.push_back(
        {literals[i], block_of[i], {item_covers[i][0], item_covers[i][1]}});
  }
  out = std::move(image);
  return true;
}

} // namespace tactix::exact_cover_semantic
