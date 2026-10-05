#include "rules/lucky/nol_hypergraph_model.hpp"

#include "rules/lucky/model.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <numeric>
#include <random>
#include <vector>

namespace tactix {
namespace {

// LymphoSAT gives its first restart 2,000,000 moves. One public invocation is
// exactly that first bounded attempt; the generic rule-work budget can shorten
// it and the controller's repeated-action loop can request another attempt.
// Mix the call ordinal into the deterministic seed so repeated actions explore
// distinct trajectories while replaying the same action sequence exactly.
constexpr uint64_t FIRST_RESTART_STEPS = 2'000'000;
constexpr uint64_t DEFAULT_SEED = 0x9e3779b97f4a7c15ULL;

struct Edge { std::array<uint32_t, 3> vertex{}; };
struct ColoredEdge {
  std::array<uint32_t, 3> group{};
  uint8_t color = 0;
};

struct Shape {
  uint32_t groups = 0;
  std::vector<uint32_t> active_groups;
  std::vector<int32_t> group_to_vertex;
  std::vector<uint8_t> allowed_colors;
  std::vector<Edge> edges;
  std::vector<std::vector<uint32_t>> incident;
  uint64_t validation_work = 0;
  uint64_t seed = DEFAULT_SEED;
};

struct Meter {
  Kernel &kernel;
  uint64_t limit;
  uint64_t ceiling;
  uint64_t used = 0;
  bool exhausted = false;
  bool interrupted = false;

  explicit Meter(Kernel &k)
      : kernel(k), limit(kernel_has_work_budget(k) ? k.work_budget : UINT64_MAX),
        ceiling(limit) {}
  ~Meter() { kernel.stats.nol_hypergraph_model_work += used; }

  bool charge(uint64_t amount) {
    if (exhausted || amount > ceiling - used) {
      if (!exhausted) ++kernel.stats.nol_hypergraph_model_budget_exhausted;
      exhausted = true;
      return false;
    }
    used += amount;
    return true;
  }

  bool reserve_terminal(uint64_t amount) {
    if (limit == UINT64_MAX) return true;
    if (amount > limit || used > limit - amount) {
      if (!exhausted) ++kernel.stats.nol_hypergraph_model_budget_exhausted;
      exhausted = true;
      return false;
    }
    ceiling = limit - amount;
    return true;
  }

  void release_terminal() {
    ceiling = limit;
    exhausted = false;
  }

  bool poll() {
    if (!interrupted &&
        kernel_rule_interrupt(kernel,
                              kernel.stats.nol_hypergraph_model_interrupted))
      interrupted = true;
    return interrupted;
  }
};

bool external_literal(const Kernel &k, Lit literal, uint32_t &group,
                      uint32_t &color) {
  const int dimacs = kernel_externalize(k, literal);
  if (!dimacs) return false;
  const uint64_t variable = dimacs < 0 ? uint64_t(-int64_t(dimacs))
                                       : uint64_t(dimacs);
  if (!variable || variable > k.external_to_internal.size()) return false;
  group = static_cast<uint32_t>((variable - 1) / 3);
  color = static_cast<uint32_t>((variable - 1) % 3);
  return true;
}

bool recognize(Kernel &k, Meter &meter, Shape &shape) {
  const uint64_t external = k.external_to_internal.size();
  if (!external || external % 3 || external / 3 > UINT32_MAX ||
      k.nvars != external)
    return false;
  shape.groups = static_cast<uint32_t>(external / 3);
  shape.group_to_vertex.assign(shape.groups, -1);
  shape.allowed_colors.assign(shape.groups, 7);
  std::vector<uint8_t> active(shape.groups, 0);
  std::vector<ColoredEdge> colored_edges;

  // This constructor is exact for the original three-literal language.  Any
  // binary clause means the live formula has another constraint family (or a
  // transformation has changed the encoding), so decline without guessing.
  for (Lit literal = 0; literal < 2 * k.nvars; ++literal) {
    if (!meter.charge(1)) return false;
    ++shape.validation_work;
    for (const WatchEntry watch : k.watches.entries(literal)) {
      if (!meter.charge(1)) return false;
      ++shape.validation_work;
      if (watch.binary()) return false;
    }
  }

  uint64_t visited = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    const uint32_t words = clause_actual_words(clause);
    shape.validation_work += 1 + (clause->garbage() ? 0 : clause->size);
    if (!clause->garbage() && !clause->redundant()) {
      if (clause->size != 3 || !meter.charge(4)) return false;
      std::array<uint32_t, 3> groups{}, colors{};
      bool all_positive = true, all_negative = true;
      for (uint32_t index = 0; index < 3; ++index) {
        const Lit literal = clause->begin()[index];
        if (!external_literal(k, literal, groups[index], colors[index]))
          return false;
        all_positive &= !negated(literal);
        all_negative &= negated(literal);
        shape.seed ^= uint64_t(kernel_externalize(k, literal) + 1'000'003) +
                      DEFAULT_SEED + (shape.seed << 6) + (shape.seed >> 2);
      }
      if (all_positive) {
        if (groups[0] != groups[1] || groups[0] != groups[2]) return false;
        std::sort(colors.begin(), colors.end());
        if (colors != std::array<uint32_t, 3>{0, 1, 2}) return false;
        active[groups[0]] = 1;
      } else if (all_negative) {
        if (colors[0] != colors[1] || colors[0] != colors[2]) return false;
        std::sort(groups.begin(), groups.end());
        if (groups[0] == groups[1] || groups[1] == groups[2]) return false;
        colored_edges.push_back({groups, static_cast<uint8_t>(colors[0])});
      } else {
        return false;
      }
    }
    ref += words;
    if (!(++visited & ((uint64_t{1} << 18) - 1)) && meter.poll())
      return false;
  }
  if (colored_edges.empty()) return false;

  for (uint32_t group = 0; group < shape.groups; ++group) {
    uint8_t allowed = 7;
    uint32_t true_count = 0, true_color = 0;
    for (uint32_t color = 0; color < 3; ++color) {
      const Lit external_lit = make_lit(3 * group + color, false);
      const int value = kernel_external_value(k, external_lit);
      if (value < 0) allowed &= uint8_t(~(uint8_t{1} << color));
      else if (value > 0) { ++true_count; true_color = color; }
    }
    if (!active[group]) {
      if (true_count) return false;
      shape.allowed_colors[group] = allowed;
      continue;
    }
    if (true_count > 1) return false;
    if (true_count) allowed = uint8_t{1} << true_color;
    if (!allowed) return false;
    shape.allowed_colors[group] = allowed;
    shape.group_to_vertex[group] =
        static_cast<int32_t>(shape.active_groups.size());
    shape.active_groups.push_back(group);
  }
  if (shape.active_groups.empty()) return false;

  std::sort(colored_edges.begin(), colored_edges.end(),
            [](const ColoredEdge &a, const ColoredEdge &b) {
              return a.group != b.group ? a.group < b.group
                                        : a.color < b.color;
            });
  std::vector<Edge> mapped;
  for (size_t begin = 0; begin < colored_edges.size();) {
    size_t end = begin;
    uint8_t mask = 0;
    while (end < colored_edges.size() &&
           colored_edges[end].group == colored_edges[begin].group) {
      mask |= uint8_t{1} << colored_edges[end].color;
      ++end;
    }
    // A noL hyperedge is represented by one negative ternary for each of the
    // three colors.  A partial set is an ordinary CNF constraint, not the
    // theory relation this action optimizes, so conservatively decline it.
    if (mask != 7) return false;
    Edge edge{colored_edges[begin].group};
    for (uint32_t &group : edge.vertex) {
      const int32_t vertex = shape.group_to_vertex[group];
      if (vertex < 0) return false;
      group = static_cast<uint32_t>(vertex);
    }
    mapped.push_back(edge);
    begin = end;
  }
  if (mapped.empty()) return false;
  std::sort(mapped.begin(), mapped.end(), [](const Edge &a, const Edge &b) {
    return a.vertex < b.vertex;
  });
  mapped.erase(std::unique(mapped.begin(), mapped.end(), [](const Edge &a,
                                                            const Edge &b) {
                 return a.vertex == b.vertex;
               }), mapped.end());
  shape.edges = std::move(mapped);
  shape.incident.assign(shape.active_groups.size(), {});
  for (uint32_t edge = 0; edge < shape.edges.size(); ++edge)
    for (const uint32_t vertex : shape.edges[edge].vertex)
      shape.incident[vertex].push_back(edge);
  return true;
}

struct Candidate {
  std::vector<uint8_t> colors;
  std::vector<uint8_t> bad;
  std::vector<uint32_t> bad_edges;
  std::vector<int32_t> bad_position;
};

void add_bad(Candidate &candidate, uint32_t edge) {
  if (candidate.bad[edge]) return;
  candidate.bad[edge] = 1;
  candidate.bad_position[edge] = static_cast<int32_t>(candidate.bad_edges.size());
  candidate.bad_edges.push_back(edge);
}

void remove_bad(Candidate &candidate, uint32_t edge) {
  if (!candidate.bad[edge]) return;
  const uint32_t position = static_cast<uint32_t>(candidate.bad_position[edge]);
  const uint32_t last = candidate.bad_edges.back();
  candidate.bad_edges[position] = last;
  candidate.bad_position[last] = static_cast<int32_t>(position);
  candidate.bad_edges.pop_back();
  candidate.bad_position[edge] = -1;
  candidate.bad[edge] = 0;
}

bool edge_bad_after(const Shape &shape, const Candidate &candidate,
                    uint32_t edge, uint32_t changed, uint8_t color) {
  const auto &vertices = shape.edges[edge].vertex;
  const uint8_t first = vertices[0] == changed ? color
                                               : candidate.colors[vertices[0]];
  for (uint32_t index = 1; index < 3; ++index) {
    const uint32_t vertex = vertices[index];
    const uint8_t current = vertex == changed ? color : candidate.colors[vertex];
    if (current != first) return false;
  }
  return true;
}

bool rebuild(const Shape &shape, Meter &meter, Candidate &candidate) {
  candidate.bad.assign(shape.edges.size(), 0);
  candidate.bad_position.assign(shape.edges.size(), -1);
  candidate.bad_edges.clear();
  for (uint32_t edge = 0; edge < shape.edges.size(); ++edge) {
    if (!meter.charge(3)) return false;
    const auto &vertices = shape.edges[edge].vertex;
    if (candidate.colors[vertices[0]] == candidate.colors[vertices[1]] &&
        candidate.colors[vertices[1]] == candidate.colors[vertices[2]])
      add_bad(candidate, edge);
  }
  return true;
}

bool greedy_initialize(const Shape &shape, Meter &meter,
                       std::mt19937_64 &random, Candidate &candidate) {
  const uint32_t vertices = static_cast<uint32_t>(shape.active_groups.size());
  candidate.colors.assign(vertices, 3);
  std::vector<uint32_t> order(vertices);
  std::iota(order.begin(), order.end(), 0);
  std::stable_sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) {
    const bool fixed_a = (shape.allowed_colors[shape.active_groups[a]] &
                          (shape.allowed_colors[shape.active_groups[a]] - 1)) == 0;
    const bool fixed_b = (shape.allowed_colors[shape.active_groups[b]] &
                          (shape.allowed_colors[shape.active_groups[b]] - 1)) == 0;
    if (fixed_a != fixed_b) return fixed_a;
    return shape.incident[a].size() > shape.incident[b].size();
  });
  for (uint32_t vertex : order) {
    if (meter.poll())
      return false;
    const uint8_t allowed = shape.allowed_colors[shape.active_groups[vertex]];
    uint32_t best_score = UINT32_MAX;
    std::array<uint8_t, 3> best{};
    uint32_t count = 0;
    for (uint8_t color = 0; color < 3; ++color) {
      if (!(allowed & (uint8_t{1} << color))) continue;
      candidate.colors[vertex] = color;
      uint32_t score = 0;
      for (uint32_t edge : shape.incident[vertex]) {
        if (!meter.charge(1)) return false;
        const auto &e = shape.edges[edge].vertex;
        if (candidate.colors[e[0]] == color && candidate.colors[e[1]] == color &&
            candidate.colors[e[2]] == color)
          ++score;
      }
      if (score < best_score) { best_score = score; count = 0; }
      if (score == best_score) best[count++] = color;
    }
    candidate.colors[vertex] = best[random() % count];
  }
  return rebuild(shape, meter, candidate);
}

bool move_delta(const Shape &shape, const Candidate &candidate, Meter &meter,
                uint32_t vertex, uint8_t color, int64_t &delta) {
  delta = 0;
  for (uint32_t edge : shape.incident[vertex]) {
    if (!meter.charge(1)) return false;
    delta += int64_t(edge_bad_after(shape, candidate, edge, vertex, color)) -
             int64_t(candidate.bad[edge]);
  }
  return true;
}

bool flip(const Shape &shape, Meter &meter, Candidate &candidate,
          uint32_t vertex, uint8_t color) {
  if (candidate.colors[vertex] == color) return true;
  if (!meter.charge(shape.incident[vertex].size())) return false;
  for (uint32_t edge : shape.incident[vertex]) remove_bad(candidate, edge);
  candidate.colors[vertex] = color;
  for (uint32_t edge : shape.incident[vertex])
    if (edge_bad_after(shape, candidate, edge, UINT32_MAX, 0))
      add_bad(candidate, edge);
  return true;
}

bool search(Kernel &k, const Shape &shape, Meter &meter,
            std::vector<uint8_t> &best_colors, uint64_t &best_bad) {
  const uint64_t call_ordinal = k.stats.nol_hypergraph_model_calls - 1;
  std::mt19937_64 random(shape.seed ^
                        (call_ordinal * DEFAULT_SEED));
  Candidate candidate;
  if (!greedy_initialize(shape, meter, random, candidate)) return false;
  ++k.stats.nol_hypergraph_model_restarts;
  if (candidate.bad_edges.size() < best_bad) {
    best_bad = candidate.bad_edges.size();
    best_colors = candidate.colors;
  }
  for (uint64_t step = 0; step < FIRST_RESTART_STEPS; ++step) {
      if (candidate.bad_edges.empty()) {
        best_bad = 0;
        best_colors = candidate.colors;
        return true;
      }
      ++k.stats.nol_hypergraph_model_steps;
      if (!(step & 8191) && meter.poll())
        return false;
      const uint32_t edge = candidate.bad_edges[random() % candidate.bad_edges.size()];
      struct Move { int64_t delta; uint32_t vertex; uint8_t color; uint64_t tie; };
      std::array<Move, 6> moves{};
      uint32_t move_count = 0;
      for (uint32_t vertex : shape.edges[edge].vertex) {
        const uint8_t allowed = shape.allowed_colors[shape.active_groups[vertex]];
        for (uint8_t color = 0; color < 3; ++color) {
          if (color == candidate.colors[vertex] ||
              !(allowed & (uint8_t{1} << color)))
            continue;
          int64_t delta = 0;
          if (!move_delta(shape, candidate, meter, vertex, color, delta)) return false;
          moves[move_count++] = {delta, vertex, color, random()};
        }
      }
      if (!move_count) return false;
      std::sort(moves.begin(), moves.begin() + move_count,
                [](const Move &a, const Move &b) {
                  return a.delta != b.delta ? a.delta < b.delta : a.tie < b.tie;
                });
      uint32_t pick_limit = 1;
      const uint64_t roll = random() % 1000;
      if (roll < 35) pick_limit = move_count;
      else if (roll < 140) pick_limit = std::min<uint32_t>(4, move_count);
      const Move move = moves[random() % pick_limit];
      if (!flip(shape, meter, candidate, move.vertex, move.color)) return false;
      if (candidate.bad_edges.size() < best_bad) {
        best_bad = candidate.bad_edges.size();
        best_colors = candidate.colors;
      }
      if ((step & 8191) == 8191 && !candidate.bad_edges.empty() &&
          random() % 1000 < 15) {
        // Perturb only a vertex that has another root-compatible color.
        for (uint32_t attempt = 0; attempt < candidate.colors.size(); ++attempt) {
          const uint32_t vertex = random() % candidate.colors.size();
          const uint8_t allowed = shape.allowed_colors[shape.active_groups[vertex]];
          if (!(allowed & (allowed - 1))) continue;
          uint8_t color = candidate.colors[vertex];
          do color = static_cast<uint8_t>(random() % 3);
          while (color == candidate.colors[vertex] ||
                 !(allowed & (uint8_t{1} << color)));
          if (!flip(shape, meter, candidate, vertex, color)) return false;
          if (candidate.bad_edges.size() < best_bad) {
            best_bad = candidate.bad_edges.size();
            best_colors = candidate.colors;
          }
          break;
        }
      }
  }
  return false;
}

std::vector<int8_t> signed_assignment(const Kernel &k, const Shape &shape,
                                      const std::vector<uint8_t> &colors) {
  // Zero is an unknown phase in the partial-guidance API.  Only variables in
  // recognized active color triples are theory-backed; leave every inactive
  // or unrelated variable unknown.  The validated full-model path below
  // deliberately completes those unknowns to false.
  std::vector<int8_t> assignment(k.nvars, 0);
  for (uint32_t vertex = 0; vertex < colors.size(); ++vertex) {
    const uint32_t group = shape.active_groups[vertex];
    for (uint32_t color = 0; color < 3; ++color) {
      const Var internal = k.external_to_internal[3 * group + color];
      if (internal != INVALID_VAR && internal < assignment.size())
        assignment[internal] = color == colors[vertex] ? 1 : -1;
    }
  }
  return assignment;
}

} // namespace

RuleEligibility nol_hypergraph_model_guard(const Kernel &k) {
  RuleEligibility result;
  return result.fact("consistent", !k.inconsistent)
      .fact("no-conflict", !k.conflict_valid)
      .fact("no-pending", !k.pending.valid)
      .fact("root-level", k.level == 0)
      .fact("propagated", kernel_propagation_fixpoint(k))
      .fact("sparse-mode", !k.dense)
      .fact("no-probe-transaction", !k.probing.recording)
      .fact("large-watches-connected", k.large_watches_connected);
}

ApplyResult nol_hypergraph_model_apply(Kernel &k) {
  assert(nol_hypergraph_model_guard(k));
  ++k.stats.nol_hypergraph_model_calls;
  Meter meter(k);
  Shape shape;
  if (!recognize(k, meter, shape)) return ApplyResult::NoOp;
  ++k.stats.nol_hypergraph_model_recognized;
  k.stats.nol_hypergraph_model_vertices = shape.active_groups.size();
  k.stats.nol_hypergraph_model_edges = shape.edges.size();
  const auto saturated_add = [](uint64_t a, uint64_t b) {
    return b > UINT64_MAX - a ? UINT64_MAX : a + b;
  };
  const uint64_t variables = k.nvars;
  const uint64_t external = k.external_to_internal.size();
  const uint64_t sat_tail = saturated_add(
      shape.validation_work, saturated_add(variables, variables));
  const uint64_t hint_tail = saturated_add(variables, external);
  const uint64_t terminal_reserve = saturated_add(
      variables, std::max(sat_tail, hint_tail));
  if (!meter.reserve_terminal(terminal_reserve)) return ApplyResult::NoOp;
  std::vector<uint8_t> best;
  uint64_t best_bad = UINT64_MAX;
  const bool found = search(k, shape, meter, best, best_bad);
  if (best.empty() || meter.interrupted) return ApplyResult::NoOp;
  meter.release_terminal();
  if (!meter.charge(variables)) return ApplyResult::NoOp;
  std::vector<int8_t> signed_model = signed_assignment(k, shape, best);
  if (found) {
    if (!meter.charge(variables)) return ApplyResult::NoOp;
    std::vector<uint8_t> model(k.nvars);
    for (Var variable = 0; variable < k.nvars; ++variable)
      model[variable] = signed_model[variable] > 0;
    if (lucky_model_valid(k, model, [&](uint64_t amount) {
          return meter.charge(amount);
        })) {
      if (!meter.charge(variables)) return ApplyResult::NoOp;
      lucky_publish_model(k, model);
      ++k.stats.nol_hypergraph_model_sat;
      return ApplyResult::Sat;
    }
  }
  if (!meter.charge(saturated_add(variables, external)))
    return ApplyResult::NoOp;
  const uint64_t offered = k.stats.local_search_hints_offered;
  kernel_offer_local_search_partial_hint(
      k, strategy::LocalSearchKind::HypergraphColoring, signed_model, best_bad,
      true, {meter.used, strategy::LocalSearchEffortUnit::GraphWork});
  return k.stats.local_search_hints_offered != offered ? ApplyResult::Ok
                                                       : ApplyResult::NoOp;
}

} // namespace tactix
