#pragma once

#include "rules/lucky/graph_labeling_search.hpp"

#include <cmath>

namespace tactix::graph_labeling {

using ScoreDelta = std::pair<int64_t, int64_t>;

// Source-backed repair choices from LymphoSAT's antibandwidth constructor.
// Work/iteration ceilings and random streams are always supplied by the caller.
struct AnnealChoices {
  uint32_t bad_endpoint_percent = 80;
  uint32_t nearby_label_percent = 70;
  uint32_t refresh_period = 1024;
  uint32_t cooling_ppm = 999970;
  uint32_t minimum_temperature_milli = 50;
};
struct RepairChoices {
  uint32_t bad_edge_probes = 80;
  uint32_t random_labels = 24;
  uint32_t noise_percent = 6;
};

inline Score edge_score(uint32_t a, uint32_t b, uint32_t separation) {
  const uint32_t distance = a > b ? a - b : b - a;
  if (distance >= separation) return {0, 0};
  const uint64_t deficit = separation - distance;
  return {1, deficit * deficit};
}

// Visit exactly the incident edges affected by the swap. The mutual edge is
// counted once; there is no temporary formula or assignment mutation to undo.
template<class Charge>
bool swap_delta(const Shape &shape, const Candidate &current,
                uint32_t a, uint32_t b, Charge &charge, ScoreDelta &out) {
  ScoreDelta delta{0, 0};
  if (a == b) { out = delta; return true; }
  const auto proposed = [&](uint32_t v) {
    return current.labels[v == a ? b : v == b ? a : v];
  };
  for (uint32_t v : {a, b}) {
    for (uint32_t u : shape.adjacency[v]) {
      if (!charge(1)) return false;
      if (v == b && u == a) continue;
      const auto before = edge_score(current.labels[v], current.labels[u], shape.separation);
      const auto after = edge_score(proposed(v), proposed(u), shape.separation);
      delta.first += int64_t(after.first) - int64_t(before.first);
      delta.second += int64_t(after.second) - int64_t(before.second);
    }
  }
  out = delta;
  return true;
}

inline Score offset_score(Score score, ScoreDelta delta) {
  const auto add = [](uint64_t value, int64_t change) {
    return change < 0 ? value - uint64_t(-change) : value + uint64_t(change);
  };
  return {add(score.first, delta.first), add(score.second, delta.second)};
}

template<class Charge>
bool install_swap(Candidate &current, Candidate &best,
                  std::vector<uint32_t> &at, uint32_t a, uint32_t b,
                  ScoreDelta delta, Charge &charge) {
  const Score score = offset_score(current.score, delta);
  const bool improve = score < best.score;
  if (!charge(1 + (improve ? current.labels.size() : 0))) return false;
  std::swap(at[current.labels[a]], at[current.labels[b]]);
  std::swap(current.labels[a], current.labels[b]);
  current.score = score;
  if (improve) best = current;
  return true;
}

template<class Charge>
bool violated_edges(const Shape &shape, const Candidate &current, Charge &charge,
                    std::vector<std::pair<uint32_t, uint32_t>> &bad) {
  bad.clear();
  for (const auto &edge : shape.edges) {
    if (!charge(1)) return false;
    if (edge_score(current.labels[edge.first], current.labels[edge.second],
                   shape.separation).first)
      bad.push_back(edge);
  }
  return true;
}

template<class Charge>
bool anneal_state(const Shape &shape, uint64_t iterations, uint64_t seed,
                  const AnnealChoices &choices, Charge &charge,
                  Candidate &current, std::vector<uint32_t> &at,
                  Candidate &best, uint64_t &attempts) {
  if (!choices.refresh_period || choices.bad_endpoint_percent > 100 ||
      choices.nearby_label_percent > 100 || choices.cooling_ppm > 1000000 ||
      !choices.minimum_temperature_milli) return false;
  std::mt19937_64 random(seed);
  const uint32_t n = shape.vertices;
  double temperature = std::max(1.0, double(shape.separation) * shape.separation / 3.0);
  const double minimum = double(choices.minimum_temperature_milli) / 1000.0;
  std::vector<std::pair<uint32_t, uint32_t>> bad;
  for (uint64_t i = 0; i < iterations && current.score.first; ++i) {
    if (!charge(1)) return false;
    ++attempts;
    if (!(i % choices.refresh_period) && !violated_edges(shape, current, charge, bad))
      return false;
    if (bad.empty()) break;
    uint32_t a;
    if (random() % 100 < choices.bad_endpoint_percent) {
      const auto edge = bad[random() % bad.size()];
      a = (random() & 1) ? edge.first : edge.second;
    } else a = random() % n;
    uint32_t label;
    if (random() % 100 < choices.nearby_label_percent) {
      const int64_t offset = int64_t(random() % (2 * uint64_t(shape.separation) + 1)) -
                             shape.separation;
      label = static_cast<uint32_t>(std::clamp(int64_t(current.labels[a]) + offset,
                                             int64_t{0}, int64_t(n - 1)));
    } else label = random() % n;
    const uint32_t b = at[label];
    if (a == b) continue;
    ScoreDelta delta;
    if (!swap_delta(shape, current, a, b, charge, delta)) return false;
    bool accept = delta.second <= 0;
    if (!accept) {
      // A reproducible [0,1) draw without implementation-specific distributions.
      const double chance = double(random() >> 11) * 0x1.0p-53;
      accept = chance < std::exp(-double(delta.second) / temperature);
    }
    if (accept)
      if (!install_swap(current, best, at, a, b, delta, charge)) return false;
    temperature = std::max(minimum, temperature * choices.cooling_ppm / 1000000.0);
  }
  return true;
}

template<class Charge>
bool prepare_repair(const Shape &shape, const Candidate &best, Charge &charge,
                    Candidate &current, std::vector<uint32_t> &at) {
  if (!shape.vertices || best.labels.size() != shape.vertices ||
      !charge(3 * uint64_t(shape.vertices))) return false;
  at.assign(shape.vertices, UINT32_MAX);
  for (uint32_t v = 0; v < shape.vertices; ++v) {
    const uint32_t label = best.labels[v];
    if (label >= shape.vertices || at[label] != UINT32_MAX) return false;
    at[label] = v;
  }
  current = best;
  return true;
}

template<class Charge>
bool anneal(const Shape &shape, uint64_t iterations, uint64_t seed,
            const AnnealChoices &choices, Charge &&charge,
            Candidate &best, uint64_t &attempts) {
  attempts = 0;
  if (!iterations || !best.score.first) return true;
  Candidate current;
  std::vector<uint32_t> at;
  return prepare_repair(shape, best, charge, current, at) &&
         anneal_state(shape, iterations, seed, choices, charge, current, at, best, attempts);
}

// Min-conflict repair probes both endpoints of shuffled violated edges. Labels
// are drawn from each neighbor's separation boundary and from uniform samples.
// A bitmap gives the reference's sorted unique label order without repeatedly
// allocating/sorting a multiset. On a stall, the independently dosed annealer
// advances the same private state. The best complete candidate is kept even
// when noise moves the current state away from it.
template<class Charge>
bool repair(const Shape &shape, uint64_t rounds, uint64_t anneal_steps_per_stall,
            uint64_t seed, uint64_t anneal_seed, const RepairChoices &choices,
            const AnnealChoices &anneal_choices, Charge &&charge,
            Candidate &best, uint64_t &completed, uint64_t &anneal_attempts) {
  completed = anneal_attempts = 0;
  if (!rounds || !best.score.first) return true;
  if (!choices.bad_edge_probes || choices.noise_percent > 100) return false;
  Candidate current;
  std::vector<uint32_t> at;
  if (!prepare_repair(shape, best, charge, current, at) || !charge(shape.vertices))
    return false;
  const uint32_t n = shape.vertices;
  std::vector<uint8_t> labels(n, 0);
  std::vector<std::pair<uint32_t, uint32_t>> bad;
  std::mt19937_64 random(seed);
  for (uint64_t round = 0; round < rounds && current.score.first; ++round) {
    if (!charge(1) || !violated_edges(shape, current, charge, bad)) return false;
    for (size_t i = bad.size(); i > 1; --i) {
      if (!charge(1)) return false;
      std::swap(bad[i - 1], bad[random() % i]);
    }
    ScoreDelta selected{0, INT64_MAX};
    uint32_t chosen_a = UINT32_MAX, chosen_b = UINT32_MAX;
    const size_t probes = std::min<size_t>(bad.size(), choices.bad_edge_probes);
    for (size_t bi = 0; bi < probes; ++bi) {
      for (uint32_t a : {bad[bi].first, bad[bi].second}) {
        if (!charge(n)) return false;
        std::fill(labels.begin(), labels.end(), 0);
        for (uint32_t neighbor : shape.adjacency[a]) {
          if (!charge(1)) return false;
          const int64_t label = current.labels[neighbor], width = shape.separation;
          for (int64_t target : {label - width, label + width,
                                 label - width + 1, label + width - 1})
            if (target >= 0 && target < n) labels[static_cast<size_t>(target)] = 1;
        }
        for (uint32_t i = 0; i < choices.random_labels; ++i) {
          if (!charge(1)) return false;
          labels[random() % n] = 1;
        }
        for (uint32_t label = 0; label < n; ++label) {
          if (!charge(1)) return false;
          if (!labels[label] || at[label] == a) continue;
          const uint32_t b = at[label];
          ScoreDelta delta;
          if (!swap_delta(shape, current, a, b, charge, delta)) return false;
          if (delta < selected) {
            chosen_a = a;
            chosen_b = b;
            selected = delta;
          }
        }
      }
    }
    const bool move = chosen_a != UINT32_MAX &&
                     (selected.first < 0 || selected.second < 0 ||
                      random() % 100 < choices.noise_percent);
    if (move) {
      if (!install_swap(current, best, at, chosen_a, chosen_b, selected, charge))
        return false;
    } else if (!anneal_state(shape, anneal_steps_per_stall, anneal_seed,
                              anneal_choices, charge, current, at, best, anneal_attempts))
      return false;
    ++completed;
  }
  return true;
}

} // namespace tactix::graph_labeling
