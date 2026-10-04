#pragma once

#include "rules/lucky/graph_labeling_recognition.hpp"

#include <numeric>
#include <random>

namespace tactix::graph_labeling {

using Score = std::pair<uint64_t, uint64_t>; // violated edges, squared deficits
struct Candidate {
  // Zero-based label for each vertex. No Boolean auxiliary values are implied.
  std::vector<uint32_t> labels;
  Score score{UINT64_MAX, UINT64_MAX};
};

// Repair/annealing may move vertex zero after the initial normalization.
// Reflection preserves every edge distance and thus the cached objective;
// Boolean/root validity is still checked separately before publishing SAT.
template<class Charge>
bool normalize_reflection(Candidate &candidate, Charge &&charge) {
  const size_t n = candidate.labels.size();
  if (!n || candidate.labels[0] + 1 <= n / 2) return true;
  if (!charge(n)) return false;
  for (uint32_t &label : candidate.labels) label = uint32_t(n - 1 - label);
  return true;
}

// Stable metered insertion sorting keeps interruption outside a standard-library
// comparator. Its degree/index ordering is the reference BFS tie break.
template<class Less, class Charge>
bool order_vertices(std::vector<uint32_t> &vertices, Less &&less, Charge &charge) {
  for (size_t i = 1; i < vertices.size(); ++i) {
    const uint32_t vertex = vertices[i];
    size_t j = i;
    while (j) {
      if (!charge(1)) return false;
      if (!less(vertex, vertices[j - 1])) break;
      vertices[j] = vertices[j - 1];
      --j;
    }
    vertices[j] = vertex;
  }
  return true;
}

template<class Charge>
bool breadth_first_order(const Shape &shape, bool high_start, Charge &charge,
                         std::vector<uint32_t> &order) {
  const uint32_t n = shape.vertices;
  if (!charge(3 * uint64_t(n))) return false;
  std::vector<uint32_t> starts(n), pending;
  std::vector<uint8_t> seen(n, 0);
  pending.reserve(n);
  order.clear();
  order.reserve(n);
  std::iota(starts.begin(), starts.end(), 0);
  const auto degree_less = [&](uint32_t a, uint32_t b, bool high) {
    const size_t da = shape.adjacency[a].size(), db = shape.adjacency[b].size();
    return da == db ? a < b : high ? da > db : da < db;
  };
  if (!order_vertices(starts, [&](uint32_t a, uint32_t b) {
        return degree_less(a, b, high_start);
      }, charge)) return false;
  std::vector<uint32_t> neighbors;
  for (uint32_t start : starts) {
    if (!charge(1)) return false;
    if (seen[start]) continue;
    pending.clear();
    pending.push_back(start);
    seen[start] = 1;
    for (size_t head = 0; head < pending.size(); ++head) {
      if (!charge(1)) return false;
      const uint32_t vertex = pending[head];
      order.push_back(vertex);
      neighbors.clear();
      for (uint32_t other : shape.adjacency[vertex]) {
        if (!charge(1)) return false;
        if (!seen[other]) neighbors.push_back(other);
      }
      if (!order_vertices(neighbors, [&](uint32_t a, uint32_t b) {
            return degree_less(a, b, false);
          }, charge)) return false;
      for (uint32_t other : neighbors) {
        if (!charge(1)) return false;
        seen[other] = 1;
        pending.push_back(other);
      }
    }
  }
  return true;
}

template<class Charge>
bool score_labels(const Shape &shape, const std::vector<uint32_t> &labels,
                  Charge &charge, Score &out) {
  Score score{0, 0};
  for (const auto [a, b] : shape.edges) {
    if (!charge(1)) return false;
    const uint32_t distance = labels[a] > labels[b] ? labels[a] - labels[b]
                                                         : labels[b] - labels[a];
    if (distance < shape.separation) {
      ++score.first;
      const uint64_t deficit = shape.separation - distance;
      score.second += deficit * deficit;
    }
  }
  out = score;
  return true;
}

// The reference's complete 2*2*3*2 construction family: low/high-degree BFS
// starts, forward/reverse order, alternating ends/two-at-a-time/straight labels,
// and reflection. A bounded stop retains only fully scored candidates.
// Shape must come from recognize (or an equivalently validated internal graph).
// Caller separately admits workspace: O(vertices+edges), no kernel mutation.
template<class Charge>
bool order_seeds(const Shape &shape, Charge &&charge, Candidate &best,
                 uint32_t &completed) {
  best = {};
  completed = 0;
  const uint32_t n = shape.vertices;
  if (!n || shape.adjacency.size() != n) return false;
  for (bool high : {false, true}) {
    std::vector<uint32_t> order;
    if (!breadth_first_order(shape, high, charge, order)) return false;
    for (bool reverse : {false, true}) {
      for (uint32_t mode = 0; mode < 3; ++mode) {
        for (bool flip : {false, true}) {
          if (!charge(n)) return false;
          std::vector<uint32_t> labels(n);
          uint32_t low = 0, high_label = n - 1;
          for (uint32_t i = 0; i < n; ++i) {
            const uint32_t vertex = order[reverse ? n - 1 - i : i];
            if (mode == 2) labels[vertex] = flip ? n - 1 - i : i;
            else {
              const bool from_low = (((mode == 0 ? i : i / 2) & 1) == 0) != flip;
              labels[vertex] = from_low ? low++ : high_label--;
            }
          }
          if (labels[0] + 1 > n / 2) {
            if (!charge(n)) return false;
            for (auto &label : labels) label = n - 1 - label;
          }
          Score score;
          if (!score_labels(shape, labels, charge, score)) return false;
          ++completed;
          if (score < best.score) {
            best.labels = std::move(labels);
            best.score = score;
          }
        }
      }
    }
  }
  return true;
}

struct WindowLimits {
  uint64_t nodes;
  uint32_t starts;
  uint32_t early_choices;
  uint64_t seed;
};
enum class WindowStatus { Found, NoCandidate, Limited };

// Place one vertex at each successive label, excluding neighbors of the last
// separation-1 vertices. Explicit frames avoid recursion on a caller-sized
// graph. A frame ranks candidates by remaining degree times the number of
// recent vertices, then descending total degree, matching the reference key
// without re-counting the same neighbors for each recent vertex. Random ties
// are stable in shuffled order. Caps/seed belong to the caller's typed profile.
// On a bounded stop the prior fully scored construction remains available.
template<class Charge>
WindowStatus window_search(const Shape &shape, const WindowLimits &limits,
                           Charge &&charge, Candidate &best, uint64_t &nodes) {
  nodes = 0;
  const uint32_t n = shape.vertices;
  if (!n || !shape.separation || shape.adjacency.size() != n)
    return WindowStatus::NoCandidate;
  if (!limits.nodes || !limits.starts || !limits.early_choices)
    return WindowStatus::Limited;
  const uint64_t words = (uint64_t(n) + 63) / 64;
  if (!charge(uint64_t(n) * words + 3 * uint64_t(n)))
    return WindowStatus::Limited;
  std::vector<std::vector<uint64_t>> adjacent(n, std::vector<uint64_t>(words, 0));
  std::vector<uint32_t> starts(n), order;
  std::vector<uint8_t> used(n, 0);
  std::iota(starts.begin(), starts.end(), 0);
  order.reserve(n);
  for (uint32_t v = 0; v < n; ++v)
    for (uint32_t u : shape.adjacency[v]) {
      if (!charge(1)) return WindowStatus::Limited;
      adjacent[v][u / 64] |= uint64_t{1} << (u % 64);
    }
  if (!order_vertices(starts, [&](uint32_t a, uint32_t b) {
        const size_t da = shape.adjacency[a].size(), db = shape.adjacency[b].size();
        return da == db ? a < b : da > db;
      }, charge)) return WindowStatus::Limited;
  struct Frame { std::vector<uint32_t> choices; size_t next = 0; };
  std::vector<Frame> frames;
  frames.reserve(n);
  std::mt19937_64 random(limits.seed);
  std::vector<uint64_t> forbidden(words), remaining_degree(n);
  for (uint32_t si = 0; si < std::min(n, limits.starts); ++si) {
    if (!charge(n)) return WindowStatus::Limited;
    std::fill(used.begin(), used.end(), 0);
    order.assign(1, starts[si]);
    used[starts[si]] = 1;
    frames.clear();
    bool enter = true;
    while (!order.empty()) {
      if (enter) {
        if (nodes == limits.nodes || !charge(1)) return WindowStatus::Limited;
        ++nodes;
        const uint32_t depth = static_cast<uint32_t>(order.size());
        if (depth == n) {
          if (!charge(n)) return WindowStatus::Limited;
          Candidate candidate;
          candidate.labels.resize(n);
          for (uint32_t i = 0; i < n; ++i) candidate.labels[order[i]] = i;
          if (candidate.labels[0] + 1 > n / 2) {
            if (!charge(n)) return WindowStatus::Limited;
            for (auto &label : candidate.labels) label = n - 1 - label;
          }
          if (!score_labels(shape, candidate.labels, charge, candidate.score))
            return WindowStatus::Limited;
          best = std::move(candidate);
          return WindowStatus::Found;
        }
        if (!charge(words)) return WindowStatus::Limited;
        std::fill(forbidden.begin(), forbidden.end(), 0);
        const uint32_t first = depth >= shape.separation
                                   ? depth - shape.separation + 1 : 0;
        for (uint32_t i = first; i < depth; ++i)
          for (uint64_t word = 0; word < words; ++word) {
            if (!charge(1)) return WindowStatus::Limited;
            forbidden[word] |= adjacent[order[i]][word];
          }
        Frame frame;
        for (uint32_t v = 0; v < n; ++v) {
          if (!charge(1)) return WindowStatus::Limited;
          if (used[v] || (forbidden[v / 64] & (uint64_t{1} << (v % 64)))) continue;
          frame.choices.push_back(v);
          remaining_degree[v] = 0;
          for (uint32_t u : shape.adjacency[v]) {
            if (!charge(1)) return WindowStatus::Limited;
            remaining_degree[v] += !used[u];
          }
        }
        // Fisher-Yates with an explicit draw per choice keeps replay stable.
        for (size_t i = frame.choices.size(); i > 1; --i) {
          if (!charge(1)) return WindowStatus::Limited;
          std::swap(frame.choices[i - 1], frame.choices[random() % i]);
        }
        const uint32_t recent = std::min(depth, shape.separation >= 2
                                                     ? shape.separation - 2 : 0);
        if (!order_vertices(frame.choices, [&](uint32_t a, uint32_t b) {
              const uint64_t ra = remaining_degree[a] * recent;
              const uint64_t rb = remaining_degree[b] * recent;
              return ra != rb ? ra < rb
                             : shape.adjacency[a].size() > shape.adjacency[b].size();
            }, charge)) return WindowStatus::Limited;
        if (depth < n / 3 && frame.choices.size() > limits.early_choices)
          frame.choices.resize(limits.early_choices);
        frames.push_back(std::move(frame));
        enter = false;
      }
      Frame &frame = frames.back();
      if (frame.next < frame.choices.size()) {
        const uint32_t vertex = frame.choices[frame.next++];
        order.push_back(vertex);
        used[vertex] = 1;
        enter = true;
      } else {
        frames.pop_back();
        used[order.back()] = 0;
        order.pop_back();
      }
    }
  }
  return WindowStatus::NoCandidate;
}

} // namespace tactix::graph_labeling
