// Constructor-only measurement. The unmodified reference is compiled as a
// control, never linked into the production solver or invoked as a fallback.
#include "rules/lucky/graph_labeling_repair.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#define main lymphosat_reference_main
#include REFERENCE_GRAPH_SOURCE
#undef main

namespace native = tactix::graph_labeling;
using Clock = std::chrono::steady_clock;
struct StageMeasurement {
  const char *name;
  uint64_t work = 0, cap;
  bool exhausted = false;
  double seconds = 0;
};

int main(int argc, char **argv) {
  if (argc != 3) return 2;
  const std::string arm = argv[2];
  if (arm != "native" && arm != "reference") return 2;
  std::ifstream input(argv[1]);
  native::Shape shape;
  if (!(input >> shape.vertices >> shape.separation) || !shape.vertices ||
      shape.vertices > 384 || !shape.separation) return 2;
  shape.adjacency.resize(shape.vertices);
  uint32_t a, b;
  while (input >> a >> b) {
    if (a >= b || b >= shape.vertices) return 2;
    shape.edges.emplace_back(a, b);
    shape.adjacency[a].push_back(b);
    shape.adjacency[b].push_back(a);
  }
  if (!input.eof()) return 2;
  for (auto &row : shape.adjacency) std::sort(row.begin(), row.end());
  native::Candidate best;
  uint32_t seeds = 0;
  uint64_t nodes = 0, rounds = 0, attempts = 0;
  // The unchanged explicit experimental search profile from probe 02.
  // These are test inputs, not selected/calibrated production defaults.
  std::vector<StageMeasurement> stages = {
      {"construction", 0, 100000000}, {"window", 0, 5000000000ULL},
      {"repair", 0, 1000000000}, {"anneal", 0, 500000000},
      {"final_repair", 0, 1000000000}, {"reflection", 0, 1000000}};
  const auto begin = Clock::now();
  if (arm == "reference") {
    Instance instance;
    instance.n = shape.vertices;
    instance.w = shape.separation;
    instance.nvar = instance.prim = instance.n * instance.n;
    instance.adj.resize(shape.vertices);
    for (const auto &[u, v] : shape.edges) {
      instance.edges.emplace_back(u, v);
      instance.adj[u].push_back(v);
      instance.adj[v].push_back(u);
    }
    const auto labels = find_labeling(instance);
    for (int label : labels) best.labels.push_back(uint32_t(label - 1));
    const auto score = score_pos(instance, labels);
    best.score = {uint64_t(score.first), uint64_t(score.second)};
  } else {
    const uint64_t n = shape.vertices;
    const uint64_t repair_seed = 0xC0FFEE1234ULL ^ (n << 8) ^ shape.separation;
    const uint64_t anneal_seed = 0xA17B00D5ULL ^ (n << 16) ^ shape.separation;
    for (size_t stage = 0; stage < stages.size(); ++stage) {
      auto &measurement = stages[stage];
      const auto charge = [&](uint64_t amount) {
        if (measurement.exhausted) return false;
        if (amount > measurement.cap - measurement.work) {
          measurement.exhausted = true;
          return false;
        }
        measurement.work += amount;
        return true;
      };
      const auto start = Clock::now();
      if (!stage) {
        (void)native::order_seeds(shape, charge, best, seeds);
      } else if (best.labels.empty()) return 3;
      else if (stage == 5) (void)native::normalize_reflection(best, charge);
      else if (best.score.first) {
        if (stage == 1 && n <= 140)
          (void)native::window_search(shape, {50000000, 12, 28,
              0x51A7EULL ^ (n << 20) ^ shape.separation}, charge, best, nodes);
        else if (stage == 2 || stage == 4) {
          uint64_t local_rounds = 0, local_attempts = 0;
          (void)native::repair(shape, UINT64_MAX, 100000, repair_seed, anneal_seed,
                              {}, {}, charge, best, local_rounds, local_attempts);
          rounds += local_rounds;
          attempts += local_attempts;
        } else if (stage == 3) {
          uint64_t local_attempts = 0;
          (void)native::anneal(shape, UINT64_MAX, anneal_seed, {}, charge, best,
                              local_attempts);
          attempts += local_attempts;
        }
      }
      measurement.seconds = std::chrono::duration<double>(Clock::now() - start).count();
    }
  }
  const double elapsed = std::chrono::duration<double>(Clock::now() - begin).count();
  std::cout.precision(17);
  std::cout << "{\"arm\":\"" << arm << "\",\"vertices\":" << shape.vertices
            << ",\"separation\":" << shape.separation << ",\"edges\":" << shape.edges.size()
            << ",\"seconds\":" << elapsed << ",\"bad\":" << best.score.first
            << ",\"cost\":" << best.score.second << ",\"labels\":[";
  for (size_t i = 0; i < best.labels.size(); ++i)
    std::cout << (i ? "," : "") << best.labels[i];
  std::cout << "],\"seeds\":" << seeds << ",\"nodes\":" << nodes
            << ",\"rounds\":" << rounds << ",\"anneal_attempts\":" << attempts
            << ",\"stages\":[";
  if (arm == "native") for (size_t i = 0; i < stages.size(); ++i) {
    const auto &s = stages[i];
    std::cout << (i ? "," : "") << "{\"name\":\"" << s.name
              << "\",\"work\":" << s.work << ",\"cap\":" << s.cap
              << ",\"exhausted\":" << (s.exhausted ? "true" : "false")
              << ",\"seconds\":" << s.seconds << '}';
  }
  std::cout << "]}\n";
}
