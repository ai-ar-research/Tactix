#include "rules/lucky/onehot_model.hpp"
#include "rules/lucky/exact_one_refutation.hpp"
#include "rules/lucky/model.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <map>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace tactix {
namespace {

using Domain = std::array<Var, 3>;
using FieldRow = std::vector<uint8_t>;

uint64_t binary_key(Lit a, Lit b) {
  if (b < a)
    std::swap(a, b);
  return (uint64_t{a} << 32) | b;
}

bool pow3(uint32_t exponent, uint64_t &result) {
  result = 1;
  while (exponent--) {
    if (result > std::numeric_limits<uint64_t>::max() / 3)
      return false;
    result *= 3;
  }
  return true;
}

uint8_t field_sub(uint8_t a, uint8_t b) {
  return static_cast<uint8_t>((a + 3 - b) % 3);
}

bool add_row(std::vector<FieldRow> &basis, FieldRow row, uint32_t columns) {
  for (;;) {
    uint32_t pivot = 0;
    while (pivot != columns && !row[pivot])
      ++pivot;
    if (pivot == columns)
      return row[columns] == 0;
    if (basis[pivot].empty()) {
      if (row[pivot] == 2)
        for (uint8_t &entry : row)
          entry = static_cast<uint8_t>((2 * entry) % 3);
      basis[pivot] = std::move(row);
      return true;
    }
    const uint8_t factor = row[pivot];
    for (uint32_t column = pivot; column <= columns; ++column)
      row[column] = field_sub(
          row[column], static_cast<uint8_t>((factor * basis[pivot][column]) % 3));
  }
}

// A direct finite-domain encoding has one long clause per object.  Its signed
// literals name the possible values, while binary clauses forbid pairs of
// values.  This is the representation recovered by LymphoSAT's RBSAT solver;
// keeping the recognizer here makes the technique a formula-shape rule rather
// than benchmark dispatch.  Every proposed Boolean completion is still
// checked against the complete live CNF before publication.
struct DirectCspEdge {
  uint32_t a = 0, b = 0;
  uint64_t first_clause_id = 0;
  std::vector<uint64_t> forbidden;
};

struct DirectCsp {
  uint32_t width = 0;
  uint64_t constraints = 0;
  std::vector<std::vector<Lit>> domains;
  std::vector<DirectCspEdge> edges;
  std::vector<std::vector<uint32_t>> incident;
};

struct DirectCspState {
  std::vector<uint32_t> value;
  std::vector<uint32_t> weighted_conflicts;
  std::vector<uint32_t> edge_weight;
  std::vector<uint32_t> bad_edges;
  std::vector<int32_t> bad_position;
};

FILE *direct_csp_trace() {
  static FILE *trace = [] {
    const char *path = std::getenv("TACTIX_DIRECT_CSP_TRACE");
    return path && *path ? std::fopen(path, "w") : nullptr;
  }();
  return trace;
}

void direct_csp_trace_row(const char *stage, uint64_t first = 0,
                          uint64_t second = 0, uint64_t third = 0) {
  FILE *trace = direct_csp_trace();
  if (!trace)
    return;
  std::fprintf(trace, "%s\t%llu\t%llu\t%llu\n", stage,
               (unsigned long long)first, (unsigned long long)second,
               (unsigned long long)third);
  std::fflush(trace);
}

uint64_t direct_csp_state_hash(const DirectCspState &state) {
  uint64_t hash = 1469598103934665603ULL;
  const auto mix = [&hash](uint64_t value) {
    hash ^= value;
    hash *= 1099511628211ULL;
  };
  for (const uint32_t value : state.value)
    mix(value);
  mix(UINT32_MAX);
  for (const uint32_t edge : state.bad_edges)
    mix(edge);
  mix(UINT32_MAX - 1);
  for (const uint32_t weight : state.edge_weight)
    mix(weight);
  return hash;
}

uint64_t direct_csp_edge_key(uint32_t a, uint32_t b) {
  if (b < a)
    std::swap(a, b);
  return (uint64_t{a} << 32) | b;
}

bool direct_csp_violates(const DirectCspEdge &edge, uint32_t va,
                         uint32_t vb) {
  return (edge.forbidden[va] >> vb) & 1u;
}

// Any all-different clique with more objects than values cannot contain a
// model.  This is only a negative admission test for the SAT-only local
// search: the ordinary solver remains responsible for proving UNSAT.  A
// degree-ordered greedy clique is deliberately incomplete but always sound;
// it costs O(edges + domains * width * log(domains)) and stops at width + 1.
uint32_t direct_csp_infeasible_alldifferent_clique(const DirectCsp &csp) {
  std::vector<std::vector<uint32_t>> adjacent(csp.domains.size());
  for (const DirectCspEdge &edge : csp.edges) {
    bool diagonal_forbidden = true;
    for (uint32_t value = 0; value < csp.width; ++value)
      diagonal_forbidden &= direct_csp_violates(edge, value, value);
    if (!diagonal_forbidden)
      continue;
    adjacent[edge.a].push_back(edge.b);
    adjacent[edge.b].push_back(edge.a);
  }
  for (auto &neighbors : adjacent)
    std::sort(neighbors.begin(), neighbors.end());
  std::vector<uint32_t> order(csp.domains.size());
  std::iota(order.begin(), order.end(), 0);
  std::sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) {
    return adjacent[a].size() > adjacent[b].size() ||
           (adjacent[a].size() == adjacent[b].size() && a < b);
  });
  std::vector<uint32_t> clique;
  clique.reserve(std::min<size_t>(csp.width + 1, csp.domains.size()));
  for (const uint32_t candidate : order) {
    bool complete = true;
    for (const uint32_t member : clique)
      if (!std::binary_search(adjacent[member].begin(),
                              adjacent[member].end(), candidate)) {
        complete = false;
        break;
      }
    if (complete)
      clique.push_back(candidate);
    if (clique.size() > csp.width)
      return static_cast<uint32_t>(clique.size());
  }
  return 0;
}

bool recognize_direct_csp(const Kernel &k, DirectCsp &csp,
                          uint64_t maximum_constraints) {
  if (!k.nvars)
    return false;
  uint64_t constraints = 0;
  std::vector<std::vector<Lit>> domains;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    if (!clause->garbage() && ++constraints > maximum_constraints)
      return false;
    if (!clause->garbage() && !clause->redundant() && clause->size > 2) {
      std::vector<Lit> domain(clause->begin(), clause->end());
      std::sort(domain.begin(), domain.end(), [](Lit a, Lit b) {
        return var_of(a) < var_of(b);
      });
      if (domain.size() > 63 ||
          (csp.width && domain.size() != csp.width))
        return false;
      csp.width = static_cast<uint32_t>(domain.size());
      domains.push_back(std::move(domain));
    }
    ref += clause_actual_words(clause);
  }
  if (domains.size() < 2 || csp.width < 2)
    return false;
  std::sort(domains.begin(), domains.end(), [](const auto &a, const auto &b) {
    return var_of(a.front()) < var_of(b.front());
  });

  std::vector<int32_t> domain_of(k.nvars, -1), value_of(k.nvars, -1);
  std::vector<Lit> encoded_lit(k.nvars, INVALID_LIT);
  for (uint32_t i = 0; i < domains.size(); ++i) {
    for (uint32_t value = 0; value < domains[i].size(); ++value) {
      const Lit lit = domains[i][value];
      const Var var = var_of(lit);
      if (var >= k.nvars || domain_of[var] >= 0 ||
          kernel_var_retired(k, var) || k.value(make_lit(var, false)))
        return false;
      domain_of[var] = static_cast<int32_t>(i);
      value_of[var] = static_cast<int32_t>(value);
      encoded_lit[var] = lit;
    }
  }
  for (Var var = 0; var < k.nvars; ++var)
    if (!kernel_var_retired(k, var) && domain_of[var] < 0)
      return false;

  std::unordered_map<uint64_t, uint32_t> edge_by_pair;
  const auto consume_binary = [&](Lit lit, Lit other, uint64_t ordinal) {
      if (++constraints > maximum_constraints) {
        direct_csp_trace_row("constraint-limit", constraints,
                             maximum_constraints, ordinal);
        return false;
      }
      const Var av = var_of(lit), bv = var_of(other);
      if (av >= k.nvars || bv >= k.nvars || domain_of[av] < 0 ||
          domain_of[bv] < 0 || lit != negate(encoded_lit[av]) ||
          other != negate(encoded_lit[bv])) {
        direct_csp_trace_row("binary-mismatch", lit, other, ordinal);
        return false;
      }
      uint32_t a = static_cast<uint32_t>(domain_of[av]);
      uint32_t b = static_cast<uint32_t>(domain_of[bv]);
      uint32_t va = static_cast<uint32_t>(value_of[av]);
      uint32_t vb = static_cast<uint32_t>(value_of[bv]);
      if (a == b)
        return true;
      if (b < a) {
        std::swap(a, b);
        std::swap(va, vb);
      }
      const uint64_t key = direct_csp_edge_key(a, b);
      auto [it, inserted] = edge_by_pair.emplace(
          key, static_cast<uint32_t>(csp.edges.size()));
      if (inserted) {
        DirectCspEdge edge;
        edge.a = a;
        edge.b = b;
        edge.first_clause_id = ordinal;
        edge.forbidden.assign(csp.width, 0);
        csp.edges.push_back(std::move(edge));
      } else {
        const uint64_t id = ordinal;
        if (id && (!csp.edges[it->second].first_clause_id ||
                   id < csp.edges[it->second].first_clause_id))
          csp.edges[it->second].first_clause_id = id;
      }
      csp.edges[it->second].forbidden[va] |= uint64_t{1} << vb;
      return true;
  };
  if (k.input_binary_order_captured) {
    uint64_t ordinal = 0;
    for (const uint64_t key : k.input_binary_order) {
      const Lit lit = static_cast<Lit>(key >> 32);
      const Lit other = static_cast<Lit>(key);
      if (!consume_binary(lit, other, ++ordinal))
        return false;
    }
  } else {
    uint64_t ordinal = 0;
    for (Lit lit = 0; lit < 2 * k.nvars; ++lit) {
      for (const WatchEntry watch : k.watches.entries(lit)) {
        if (!watch.binary() || lit > watch.other())
          continue;
        if (!consume_binary(lit, watch.other(), ++ordinal))
          return false;
      }
    }
  }
  if (csp.edges.empty())
    return false;
  csp.constraints = k.input_binary_order_captured ? k.next_clause_id
                                                   : constraints;
  std::sort(csp.edges.begin(), csp.edges.end(),
            [](const DirectCspEdge &a, const DirectCspEdge &b) {
              if (a.first_clause_id != b.first_clause_id)
                return a.first_clause_id < b.first_clause_id;
              return a.a < b.a || (a.a == b.a && a.b < b.b);
            });
  csp.domains = std::move(domains);
  csp.incident.assign(csp.domains.size(), {});
  for (uint32_t edge = 0; edge < csp.edges.size(); ++edge) {
    csp.incident[csp.edges[edge].a].push_back(edge);
    csp.incident[csp.edges[edge].b].push_back(edge);
  }
  return true;
}

void direct_csp_add_bad(const DirectCsp &csp, DirectCspState &state,
                        uint32_t edge) {
  if (state.bad_position[edge] >= 0)
    return;
  state.bad_position[edge] = static_cast<int32_t>(state.bad_edges.size());
  state.bad_edges.push_back(edge);
  const DirectCspEdge &e = csp.edges[edge];
  state.weighted_conflicts[e.a] += state.edge_weight[edge];
  state.weighted_conflicts[e.b] += state.edge_weight[edge];
}

void direct_csp_remove_bad(const DirectCsp &csp, DirectCspState &state,
                           uint32_t edge) {
  const int32_t position = state.bad_position[edge];
  if (position < 0)
    return;
  const uint32_t last = state.bad_edges.back();
  state.bad_edges[position] = last;
  state.bad_position[last] = position;
  state.bad_edges.pop_back();
  state.bad_position[edge] = -1;
  const DirectCspEdge &e = csp.edges[edge];
  state.weighted_conflicts[e.a] -= state.edge_weight[edge];
  state.weighted_conflicts[e.b] -= state.edge_weight[edge];
}

inline bool direct_csp_charge(Kernel &k, uint64_t amount, uint64_t limit,
                              uint64_t &work) {
  if (work >= limit || amount > limit - work) {
    work = limit;
    return false;
  }
  const uint64_t before = work;
  work += amount;
  if ((before >> 16) != (work >> 16) &&
      kernel_rule_interrupt(k, k.stats.onehot_model_interrupted))
    return false;
  return true;
}

bool direct_csp_edge_bad(const DirectCsp &csp, const DirectCspState &state,
                         uint32_t edge) {
  const DirectCspEdge &e = csp.edges[edge];
  return direct_csp_violates(e, state.value[e.a], state.value[e.b]);
}

bool initialize_direct_csp(const DirectCsp &csp, DirectCspState &state,
                           std::mt19937_64 &random, Kernel &k,
                           uint64_t work_limit, uint64_t &work) {
  state.value.assign(csp.domains.size(), 0);
  std::vector<uint32_t> order(csp.domains.size());
  for (uint32_t i = 0; i < order.size(); ++i)
    order[i] = i;
  std::sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) {
    return csp.incident[a].size() > csp.incident[b].size();
  });
  std::vector<uint8_t> assigned(csp.domains.size(), 0);
  for (const uint32_t variable : order) {
    uint32_t best = UINT32_MAX;
    std::vector<uint32_t> values;
    for (uint32_t value = 0; value < csp.width; ++value) {
      if (!direct_csp_charge(k, csp.incident[variable].size(), work_limit,
                             work))
        return false;
      uint32_t conflicts = 0;
      for (const uint32_t edge : csp.incident[variable]) {
        const DirectCspEdge &e = csp.edges[edge];
        const uint32_t other = e.a == variable ? e.b : e.a;
        if (!assigned[other])
          continue;
        const uint32_t va = e.a == variable ? value : state.value[e.a];
        const uint32_t vb = e.b == variable ? value : state.value[e.b];
        conflicts += direct_csp_violates(e, va, vb);
      }
      if (conflicts < best) {
        best = conflicts;
        values.clear();
      }
      if (conflicts == best)
        values.push_back(value);
    }
    state.value[variable] = values[random() % values.size()];
    assigned[variable] = 1;
  }
  state.weighted_conflicts.assign(csp.domains.size(), 0);
  state.edge_weight.assign(csp.edges.size(), 1);
  state.bad_edges.clear();
  state.bad_position.assign(csp.edges.size(), -1);
  if (!direct_csp_charge(k, csp.edges.size(), work_limit, work))
    return false;
  for (uint32_t edge = 0; edge < csp.edges.size(); ++edge) {
    if (direct_csp_edge_bad(csp, state, edge))
      direct_csp_add_bad(csp, state, edge);
  }
  return true;
}

bool direct_csp_set_value(const DirectCsp &csp, DirectCspState &state,
                          uint32_t variable, uint32_t value, Kernel &k,
                          uint64_t work_limit, uint64_t &work) {
  if (state.value[variable] == value)
    return true;
  if (!direct_csp_charge(k, csp.incident[variable].size(), work_limit, work))
    return false;
  for (const uint32_t edge : csp.incident[variable]) {
    const bool was_bad = state.bad_position[edge] >= 0;
    const DirectCspEdge &e = csp.edges[edge];
    const uint32_t va = e.a == variable ? value : state.value[e.a];
    const uint32_t vb = e.b == variable ? value : state.value[e.b];
    const bool now_bad = direct_csp_violates(e, va, vb);
    if (was_bad && !now_bad)
      direct_csp_remove_bad(csp, state, edge);
    else if (!was_bad && now_bad)
      direct_csp_add_bad(csp, state, edge);
  }
  state.value[variable] = value;
  return true;
}

std::vector<uint8_t>
direct_csp_boolean_assignment(const DirectCsp &csp,
                              const std::vector<uint32_t> &values,
                              uint32_t nvars) {
  std::vector<uint8_t> assignment(nvars, 0);
  for (uint32_t domain = 0; domain < csp.domains.size(); ++domain)
    for (uint32_t value = 0; value < csp.width; ++value) {
      const Lit lit = csp.domains[domain][value];
      const bool selected = value == values[domain];
      assignment[var_of(lit)] = selected != negated(lit);
    }
  return assignment;
}

bool direct_csp_model(Kernel &k,
                      const strategy::OnehotModelParameters &parameters) {
  direct_csp_trace_row("attempt", k.input_binary_order_captured,
                       k.input_binary_order.size(), k.next_clause_id);
  DirectCsp csp;
  if (!recognize_direct_csp(k, csp, parameters.maximum_constraints)) {
    direct_csp_trace_row("declined");
    return false;
  }
  ++k.stats.direct_csp_model_calls;
  k.stats.direct_csp_model_domains += csp.domains.size();
  k.stats.direct_csp_model_edges += csp.edges.size();
  direct_csp_trace_row("recognized", csp.domains.size(), csp.width,
                       csp.edges.size());
  if (const uint32_t clique =
          direct_csp_infeasible_alldifferent_clique(csp)) {
    ++k.stats.direct_csp_model_infeasible;
    direct_csp_trace_row("all-different-bound", clique, csp.width,
                         csp.edges.size());
    return false;
  }

  // `candidate_budget` bounds candidates in the GF(3) branch and restarts in
  // this local search. Formula-work feedback can stop an unproductive distant
  // trajectory early. A residual no larger than one domain's average edge
  // neighborhood is structurally near: changing a single domain can affect
  // that many constraints, so the pass may use the caller's complete restart
  // allowance. Sampling spans a full value neighborhood and distant patience
  // spans one domain width; both scales therefore come from the recovered CSP.
  const uint64_t scans = std::max<uint64_t>(1, parameters.candidate_budget);
  const uint64_t constraints = std::max<uint64_t>(1, csp.edges.size());
  const uint64_t factor = std::max<uint64_t>(1, csp.width);
  const uint64_t work_quantum =
      scans > UINT64_MAX / constraints / factor
          ? UINT64_MAX
          : scans * constraints * factor;
  const uint64_t work_limit = UINT64_MAX;
  uint64_t work = 0;
  uint64_t seed = 1469598103934665603ULL ^
                  uint64_t{k.nvars} * 1099511628211ULL ^ csp.constraints;
  for (const DirectCspEdge &edge : csp.edges) {
    seed ^= uint64_t{edge.a + 0x9e3779b9U} + (seed << 6) + (seed >> 2);
    seed ^= uint64_t{edge.b + 0x85ebca6bU} + (seed << 6) + (seed >> 2);
    for (const uint64_t mask : edge.forbidden)
      seed ^= mask + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
  }
  direct_csp_trace_row("seed", seed, csp.constraints,
                       csp.edges.front().first_clause_id);
  std::mt19937_64 random(seed);
  DirectCspState state;
  std::vector<uint32_t> best_value;
  uint64_t best_seen = UINT64_MAX;
  uint64_t feedback_best = UINT64_MAX;
  uint64_t next_feedback = work_quantum;
  uint32_t quiet_quanta = 0;
  const uint64_t near_residual =
      (csp.edges.size() + csp.domains.size() - 1) / csp.domains.size();
  const auto effective = [&] {
    while (work >= next_feedback) {
      if (best_seen < feedback_best) {
        feedback_best = best_seen;
        quiet_quanta = 0;
      } else {
        ++quiet_quanta;
      }
      direct_csp_trace_row("feedback", feedback_best, quiet_quanta, work);
      if (feedback_best > near_residual && quiet_quanta >= csp.width)
        return false;
      if (next_feedback > work_limit - std::min(work_quantum, work_limit)) {
        next_feedback = work_limit;
        break;
      }
      next_feedback += work_quantum;
    }
    return work < work_limit;
  };
  for (uint64_t restart = 0;
       restart < parameters.candidate_budget && effective(); ++restart) {
    ++k.stats.direct_csp_model_restarts;
    if (!initialize_direct_csp(csp, state, random, k, work_limit, work))
      break;
    if (direct_csp_trace())
      direct_csp_trace_row("restart-begin", restart, state.bad_edges.size(),
                           direct_csp_state_hash(state));
    uint64_t stagnant = 0;
    uint64_t restart_steps = 0;
    const uint64_t stagnant_limit = 50000 + 1200 * csp.domains.size();
    const uint64_t restart_step_limit =
        restart > (UINT64_MAX - 200000 - 15000 * csp.domains.size()) / 1000
            ? UINT64_MAX
            : 200000 + 15000 * csp.domains.size() + 1000 * restart;
    while (!state.bad_edges.empty() && restart_steps < restart_step_limit &&
           effective()) {
      if (state.bad_edges.size() < best_seen) {
        best_seen = state.bad_edges.size();
        best_value = state.value;
        direct_csp_trace_row("best", restart, best_seen, work);
        stagnant = 0;
      } else {
        ++stagnant;
        if (stagnant > stagnant_limit)
          break;
      }
      const uint32_t bad = state.bad_edges[random() % state.bad_edges.size()];
      const DirectCspEdge &bad_edge = csp.edges[bad];
      uint32_t candidates[3] = {bad_edge.a, bad_edge.b, UINT32_MAX};
      if (random() % 100 < 10) {
        const DirectCspEdge &extra =
            csp.edges[state.bad_edges[random() % state.bad_edges.size()]];
        candidates[2] = (random() & 1) ? extra.a : extra.b;
      }
      int64_t best_delta = INT64_MAX;
      std::vector<std::pair<uint32_t, uint32_t>> moves;
      moves.reserve(3 * (csp.width - 1));
      bool exhausted = false;
      for (const uint32_t variable : candidates) {
        if (variable == UINT32_MAX)
          continue;
        const int64_t current = state.weighted_conflicts[variable];
        for (uint32_t value = 0; value < csp.width; ++value) {
          if (value == state.value[variable])
            continue;
          if (!direct_csp_charge(k, csp.incident[variable].size(),
                                 work_limit, work)) {
            exhausted = true;
            break;
          }
          uint64_t conflicts = 0;
          for (const uint32_t edge : csp.incident[variable]) {
            const DirectCspEdge &e = csp.edges[edge];
            const uint32_t va =
                e.a == variable ? value : state.value[e.a];
            const uint32_t vb =
                e.b == variable ? value : state.value[e.b];
            if (direct_csp_violates(e, va, vb))
              conflicts += state.edge_weight[edge];
          }
          if (exhausted)
            break;
          const int64_t delta = static_cast<int64_t>(conflicts) - current;
          if (delta < best_delta) {
            best_delta = delta;
            moves.clear();
          }
          if (delta == best_delta)
            moves.emplace_back(variable, value);
        }
        if (exhausted)
          break;
      }
      if (exhausted || moves.empty())
        break;
      if (best_delta >= 0 && stagnant % 13 == 0) {
        if (!direct_csp_charge(k, state.bad_edges.size(), work_limit, work))
          exhausted = true;
        for (const uint32_t edge : state.bad_edges) {
          if (exhausted)
            break;
          ++state.edge_weight[edge];
          ++state.weighted_conflicts[csp.edges[edge].a];
          ++state.weighted_conflicts[csp.edges[edge].b];
        }
      }
      if (exhausted)
        break;
      uint32_t variable, value;
      bool noisy = false;
      if (random() % 1000 < 18) {
        noisy = true;
        const DirectCspEdge &noise =
            csp.edges[state.bad_edges[random() % state.bad_edges.size()]];
        variable = (random() & 1) ? noise.a : noise.b;
        value = random() % csp.width;
      } else {
        const auto move = moves[random() % moves.size()];
        variable = move.first;
        value = move.second;
      }
      if (!direct_csp_set_value(csp, state, variable, value, k, work_limit,
                                work))
        break;
      if (!restart && restart_steps < 32)
        direct_csp_trace_row(
            noisy ? "noise-move" : "move",
            (uint64_t{variable} << 32) | value, bad,
            state.bad_edges.size());
      ++restart_steps;
    }
    if (state.bad_edges.empty()) {
      std::vector<uint8_t> assignment =
          direct_csp_boolean_assignment(csp, state.value, k.nvars);
      ++k.stats.onehot_model_candidates;
      if (lucky_model_valid(k, assignment)) {
        lucky_publish_model(k, assignment);
        ++k.stats.direct_csp_model_sat;
        ++k.stats.onehot_model_sat;
        k.stats.direct_csp_model_work += work;
        k.stats.direct_csp_model_best = 0;
        direct_csp_trace_row("sat", restart, work, 0);
        return true;
      }
    }
    if (direct_csp_trace())
      direct_csp_trace_row("restart-end", restart, restart_steps,
                           direct_csp_state_hash(state));
    if ((k.stop && *k.stop))
      break;
  }
  k.stats.direct_csp_model_work += work;
  k.stats.direct_csp_model_best =
      std::min(k.stats.direct_csp_model_best, best_seen);
  if (!best_value.empty()) {
    std::vector<uint8_t> assignment =
        direct_csp_boolean_assignment(csp, best_value, k.nvars);
    kernel_offer_local_search_hint(k, strategy::LocalSearchKind::DirectCsp,
                                   assignment, best_seen, true);
  }
  direct_csp_trace_row("stopped", best_seen, work, work_limit);
  return false;
}


} // namespace

RuleEligibility onehot_model_guard(const Kernel &k) {
  RuleEligibility result;
  return result.fact("consistent", !k.inconsistent)
      .fact("no-conflict", !k.conflict_valid)
      .fact("no-pending", !k.pending.valid)
      .fact("root-level", k.level == 0)
      .fact("propagated", k.propagate_head == k.trail.size())
      .fact("sparse-mode", !k.dense)
      .fact("no-probe-transaction", !k.probing.recording)
      .fact("large-watches-connected", k.large_watches_connected);
}

ApplyResult direct_csp_model_apply(Kernel &k) {
  assert(onehot_model_guard(k));
  const auto release_input_order = [&] {
    k.input_binary_order.clear();
    k.input_binary_order.shrink_to_fit();
    k.input_binary_order_captured = false;
  };
  const strategy::OnehotModelParameters parameters =
      strategy::OnehotModelParameters::from_kernel(k);
  if (!parameters.valid() || !k.nvars ||
      k.nvars > parameters.maximum_variables || !parameters.candidate_budget) {
    release_input_order();
    return ApplyResult::NoOp;
  }
  if (kernel_rule_interrupt(k, k.stats.onehot_model_interrupted)) {
    release_input_order();
    return ApplyResult::NoOp;
  }
  const bool sat = direct_csp_model(k, parameters);
  release_input_order();
  return sat ? ApplyResult::Sat : ApplyResult::NoOp;
}

ApplyResult onehot_model_apply(Kernel &k) {
  assert(onehot_model_guard(k));
  ++k.stats.onehot_model_calls;
  const strategy::OnehotModelParameters parameters =
      strategy::OnehotModelParameters::from_kernel(k);
  const ApplyResult refutation = exact_one_refutation_apply(k, parameters);
  if (refutation == ApplyResult::Unsat)
    return refutation;
  if (!parameters.valid() || !k.nvars ||
      k.nvars > parameters.maximum_variables || !parameters.candidate_budget)
    return ApplyResult::NoOp;
  if (kernel_rule_interrupt(k, k.stats.onehot_model_interrupted))
    return ApplyResult::NoOp;

  std::vector<Domain> candidates;
  std::vector<std::vector<Var>> negative_clauses;
  uint64_t constraints = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    if (!clause->garbage() && ++constraints > parameters.maximum_constraints)
      return ApplyResult::NoOp;
    if (!clause->garbage() && !clause->redundant() && clause->size == 3 &&
        !negated(clause->lits[0]) && !negated(clause->lits[1]) &&
        !negated(clause->lits[2])) {
      Domain domain{{var_of(clause->lits[0]), var_of(clause->lits[1]),
                     var_of(clause->lits[2])}};
      std::sort(domain.begin(), domain.end());
      if (domain[0] != domain[1] && domain[1] != domain[2])
        candidates.push_back(domain);
    } else if (!clause->garbage() && !clause->redundant() &&
               clause->size >= 3 &&
               clause->size <= parameters.maximum_equation_arity &&
               std::all_of(clause->begin(), clause->end(), negated)) {
      std::vector<Var> vars;
      vars.reserve(clause->size);
      for (const Lit lit : *clause)
        vars.push_back(var_of(lit));
      negative_clauses.push_back(std::move(vars));
    }
    ref += clause_actual_words(clause);
    if (kernel_rule_interrupt(k, k.stats.onehot_model_interrupted))
      return ApplyResult::NoOp;
  }

  std::unordered_set<uint64_t> binaries;
  for (Lit lit = 0; lit < 2 * k.nvars; ++lit) {
    for (const WatchEntry watch : k.watches.entries(lit)) {
      if (!watch.binary() || lit > watch.other())
        continue;
      if (++constraints > parameters.maximum_constraints)
        return ApplyResult::NoOp;
      binaries.insert(binary_key(lit, watch.other()));
    }
    if (kernel_rule_interrupt(k, k.stats.onehot_model_interrupted))
      return ApplyResult::NoOp;
  }

  std::sort(candidates.begin(), candidates.end());
  candidates.erase(std::unique(candidates.begin(), candidates.end()),
                   candidates.end());
  std::vector<int32_t> domain_of(k.nvars, -1);
  std::vector<uint8_t> value_of(k.nvars, 0);
  std::vector<Domain> domains;
  domains.reserve(std::min<size_t>(candidates.size(), parameters.maximum_domains));
  for (const Domain &domain : candidates) {
    if (domains.size() == parameters.maximum_domains)
      return ApplyResult::NoOp;
    if (domain_of[domain[0]] >= 0 || domain_of[domain[1]] >= 0 ||
        domain_of[domain[2]] >= 0)
      continue;
    const Lit a = make_lit(domain[0], true);
    const Lit b = make_lit(domain[1], true);
    const Lit c = make_lit(domain[2], true);
    if (!binaries.count(binary_key(a, b)) ||
        !binaries.count(binary_key(a, c)) ||
        !binaries.count(binary_key(b, c)))
      continue;
    const int32_t index = static_cast<int32_t>(domains.size());
    domains.push_back(domain);
    for (uint8_t value = 0; value != 3; ++value) {
      domain_of[domain[value]] = index;
      value_of[domain[value]] = value;
    }
  }
  k.stats.onehot_model_domains += domains.size();
  if (domains.size() < parameters.minimum_domains)
    return ApplyResult::NoOp;

  std::map<std::vector<uint32_t>, std::unordered_set<uint64_t>> groups;
  for (const std::vector<Var> &clause : negative_clauses) {
    std::vector<std::pair<uint32_t, uint8_t>> entries;
    entries.reserve(clause.size());
    bool mapped = true;
    for (const Var var : clause) {
      if (domain_of[var] < 0) {
        mapped = false;
        break;
      }
      entries.push_back(
          {static_cast<uint32_t>(domain_of[var]), value_of[var]});
    }
    if (!mapped)
      continue;
    std::sort(entries.begin(), entries.end());
    bool distinct = true;
    for (size_t i = 1; i < entries.size(); ++i)
      distinct &= entries[i - 1].first != entries[i].first;
    if (!distinct)
      continue;
    std::vector<uint32_t> key;
    key.reserve(entries.size());
    uint64_t tuple = 0, place = 1;
    for (const auto &[domain, value] : entries) {
      key.push_back(domain);
      tuple += place * value;
      place *= 3;
    }
    groups[std::move(key)].insert(tuple);
  }

  std::vector<FieldRow> equations;
  equations.reserve(groups.size());
  for (const auto &[columns, forbidden] : groups) {
    const uint32_t arity = static_cast<uint32_t>(columns.size());
    if (!arity || arity > parameters.maximum_equation_arity || arity > 63)
      continue;
    uint64_t tuples = 0;
    if (!pow3(arity, tuples) || forbidden.size() != 2 * (tuples / 3))
      continue;
    const uint64_t coefficient_sets = uint64_t{1} << (arity - 1);
    bool found = false;
    for (uint64_t bits = 0; bits < coefficient_sets && !found; ++bits) {
      std::vector<uint8_t> coefficients(arity, 1);
      for (uint32_t i = 1; i < arity; ++i)
        coefficients[i] = static_cast<uint8_t>(1 + ((bits >> (i - 1)) & 1));
      for (uint8_t rhs = 0; rhs != 3 && !found; ++rhs) {
        bool exact = true;
        for (const uint64_t encoded : forbidden) {
          uint64_t rest = encoded;
          uint8_t sum = 0;
          for (uint32_t i = 0; i < arity; ++i) {
            sum = static_cast<uint8_t>((sum + coefficients[i] * (rest % 3)) % 3);
            rest /= 3;
          }
          if (sum == rhs) {
            exact = false;
            break;
          }
        }
        if (!exact)
          continue;
        FieldRow row(domains.size() + 1, 0);
        for (uint32_t i = 0; i < arity; ++i)
          row[columns[i]] = coefficients[i];
        row[domains.size()] = rhs;
        equations.push_back(std::move(row));
        found = true;
      }
    }
    if (kernel_rule_interrupt(k, k.stats.onehot_model_interrupted))
      return ApplyResult::NoOp;
  }
  k.stats.onehot_model_equations += equations.size();
  if (equations.size() < parameters.minimum_equations)
    return ApplyResult::NoOp;

  std::vector<FieldRow> basis(domains.size());
  for (FieldRow &row : equations) {
    if (!add_row(basis, std::move(row), static_cast<uint32_t>(domains.size())))
      return ApplyResult::NoOp;
    if (kernel_rule_interrupt(k, k.stats.onehot_model_interrupted))
      return ApplyResult::NoOp;
  }
  for (uint32_t index = 0; index < domains.size(); ++index) {
    int fixed = -1;
    uint32_t false_count = 0;
    for (uint8_t value = 0; value != 3; ++value) {
      const int8_t assigned = k.value(make_lit(domains[index][value], false));
      if (assigned > 0) {
        if (fixed >= 0)
          return ApplyResult::NoOp;
        fixed = value;
      } else if (assigned < 0) {
        ++false_count;
      }
    }
    if (fixed < 0 && false_count == 2)
      for (uint8_t value = 0; value != 3; ++value)
        if (!k.value(make_lit(domains[index][value], false)))
          fixed = value;
    if (fixed < 0)
      continue;
    FieldRow row(domains.size() + 1, 0);
    row[index] = 1;
    row[domains.size()] = static_cast<uint8_t>(fixed);
    if (!add_row(basis, std::move(row), static_cast<uint32_t>(domains.size())))
      return ApplyResult::NoOp;
  }

  std::vector<uint32_t> free;
  uint32_t rank = 0;
  for (uint32_t index = 0; index < domains.size(); ++index)
    if (basis[index].empty())
      free.push_back(index);
    else
      ++rank;
  k.stats.onehot_model_rank += rank;
  if (free.size() > parameters.maximum_free_domains)
    return ApplyResult::NoOp;

  std::vector<uint8_t> field_values(domains.size(), 0);
  std::vector<uint8_t> assignment(k.nvars, 0);
  std::vector<uint8_t> guidance;
  for (uint64_t candidate = 0; candidate < parameters.candidate_budget;
       ++candidate) {
    uint64_t digits = candidate;
    std::fill(field_values.begin(), field_values.end(), uint8_t{0});
    for (const uint32_t index : free) {
      field_values[index] = static_cast<uint8_t>(digits % 3);
      digits /= 3;
    }
    if (digits)
      break;
    for (uint32_t p = static_cast<uint32_t>(domains.size()); p-- > 0;) {
      if (basis[p].empty())
        continue;
      uint8_t value = basis[p][domains.size()];
      for (uint32_t column = p + 1; column < domains.size(); ++column)
        value = field_sub(
            value, static_cast<uint8_t>((basis[p][column] * field_values[column]) % 3));
      field_values[p] = value;
    }
    std::fill(assignment.begin(), assignment.end(), uint8_t{0});
    for (Var var = 0; var < k.nvars; ++var)
      if (k.value(make_lit(var, false)) > 0)
        assignment[var] = 1;
    bool root_compatible = true;
    for (uint32_t index = 0; index < domains.size(); ++index) {
      const Var selected = domains[index][field_values[index]];
      // A partially fixed domain can have one forbidden indicator without
      // fixing either of the other two.  The affine candidate still chooses
      // exactly one indicator, but it is not a model when that choice is
      // already false at the root.  Do not overwrite the root value in the
      // temporary assignment: materialization below deliberately skips fixed
      // variables and would otherwise publish a different, falsifying model.
      if (k.value(make_lit(selected, false)) < 0) {
        root_compatible = false;
        break;
      }
      assignment[selected] = 1;
    }
    ++k.stats.onehot_model_candidates;
    if (root_compatible && lucky_model_valid(k, assignment)) {
      lucky_publish_model(k, assignment);
      ++k.stats.onehot_model_sat;
      return ApplyResult::Sat;
    }
    if (root_compatible && guidance.empty())
      guidance = assignment;
    if (kernel_rule_interrupt(k, k.stats.onehot_model_interrupted)) {
      if (!guidance.empty())
        kernel_offer_local_search_hint(k, strategy::LocalSearchKind::Onehot,
                                       guidance);
      return ApplyResult::NoOp;
    }
  }
  if (!guidance.empty())
    kernel_offer_local_search_hint(k, strategy::LocalSearchKind::Onehot,
                                   guidance);
  return ApplyResult::NoOp;
}

} // namespace tactix
