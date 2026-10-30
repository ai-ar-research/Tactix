#include "rules/lucky/independent_set_model.hpp"

#include "rules/lucky/model.hpp"
#include "rules/lucky/root_guard.hpp"
#include "strategy/configuration_checking_model.hpp"
#include "strategy/rule_parameters.hpp"

#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tactix {
namespace {

struct Shape {
  uint32_t vertices = 0; // N
  uint32_t target = 0;   // K
  std::vector<std::vector<uint32_t>> adjacency; // by vertex, 0-based
  uint64_t edges = 0;
};

uint64_t edge_key(Var first, Var second) {
  if (first > second)
    std::swap(first, second);
  return (uint64_t{first} << 32) | second;
}

struct VariableVectorHash {
  size_t operator()(const std::vector<Var> &variables) const noexcept {
    uint64_t hash = UINT64_C(0xcbf29ce484222325);
    for (const Var variable : variables) {
      hash ^= uint64_t{variable} + UINT64_C(0x9e3779b97f4a7c15) +
              (hash << 6) + (hash >> 2);
      hash *= UINT64_C(0x100000001b3);
    }
    return static_cast<size_t>(hash);
  }
};

enum class StableCoreResult { NoStructure, NoModel, Sat, Interrupted };

// Authenticate the standard directed stable-extension core directly from the
// live clause set.  For every positive cover clause C there must be exactly
// one pivot p that conflicts with every other member of C, and the pivots must
// cover the entire variable universe exactly once.  A guarded clause
// (~v or P) is removable only when P is exactly C\{p} for one authenticated
// cover and (~p or ~v) is an input binary.  It is therefore the binary
// resolvent of two retained clauses.  Partial or ambiguous recognition makes
// no mutation.
StableCoreResult reduce_stable_extension_core(
    Kernel &k, const strategy::IndependentSetModelParameters &parameters) {
  if (!k.trail.empty() || k.nvars < 3 ||
      k.nvars > parameters.maximum_vertices)
    return StableCoreResult::NoStructure;

  std::unordered_set<uint64_t> edges;
  edges.reserve(2 * k.watches.binary_clauses() + 1);
  for (Lit literal = 0; literal < 2 * k.nvars; ++literal) {
    for (const WatchEntry watch : k.watches.entries(literal)) {
      if (!watch.binary() || literal > watch.other())
        continue;
      if (!negated(literal) || !negated(watch.other()))
        return StableCoreResult::NoStructure;
      edges.insert(edge_key(var_of(literal), var_of(watch.other())));
    }
  }
  if (!edges.size())
    return StableCoreResult::NoStructure;

  std::vector<Ref> covers, guarded;
  uint64_t visited = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    const uint32_t words = clause_actual_words(clause);
    if (!clause->garbage() && !clause->redundant()) {
      uint32_t negative = 0;
      for (const Lit literal : *clause)
        negative += negated(literal);
      if (!negative)
        covers.push_back(ref);
      else if (negative == 1 && clause->size > 2)
        guarded.push_back(ref);
      else
        return StableCoreResult::NoStructure;
    }
    ref += words;
    if (!(++visited & ((uint64_t{1} << 16) - 1)) &&
        kernel_rule_interrupt(k, k.stats.independent_set_model_interrupted))
      return StableCoreResult::Interrupted;
  }
  if (covers.size() != k.nvars || guarded.empty())
    return StableCoreResult::NoStructure;

  std::unordered_map<std::vector<Var>, Var, VariableVectorHash> tails;
  tails.reserve(2 * covers.size() + 1);
  std::vector<uint8_t> pivot_seen(k.nvars, 0);
  for (const Ref ref : covers) {
    const Clause *clause = clause_at(k.arena, ref);
    Var pivot = INVALID_VAR;
    uint32_t candidates = 0;
    for (const Lit candidate_literal : *clause) {
      const Var candidate = var_of(candidate_literal);
      bool adjacent_to_all = true;
      for (const Lit other_literal : *clause) {
        const Var other = var_of(other_literal);
        if (other != candidate && !edges.count(edge_key(candidate, other))) {
          adjacent_to_all = false;
          break;
        }
      }
      if (adjacent_to_all) {
        pivot = candidate;
        ++candidates;
      }
    }
    if (candidates != 1 || pivot_seen[pivot]++)
      return StableCoreResult::NoStructure;
    std::vector<Var> tail;
    tail.reserve(clause->size - 1);
    for (const Lit literal : *clause)
      if (var_of(literal) != pivot)
        tail.push_back(var_of(literal));
    std::sort(tail.begin(), tail.end());
    if (std::adjacent_find(tail.begin(), tail.end()) != tail.end() ||
        !tails.emplace(std::move(tail), pivot).second)
      return StableCoreResult::NoStructure;
  }
  if (std::find(pivot_seen.begin(), pivot_seen.end(), uint8_t{0}) !=
      pivot_seen.end())
    return StableCoreResult::NoStructure;

  for (const Ref ref : guarded) {
    const Clause *clause = clause_at(k.arena, ref);
    Var negative = INVALID_VAR;
    std::vector<Var> positive;
    positive.reserve(clause->size - 1);
    for (const Lit literal : *clause) {
      if (negated(literal))
        negative = var_of(literal);
      else
        positive.push_back(var_of(literal));
    }
    std::sort(positive.begin(), positive.end());
    if (negative == INVALID_VAR ||
        std::adjacent_find(positive.begin(), positive.end()) != positive.end())
      return StableCoreResult::NoStructure;
    const auto found = tails.find(positive);
    if (found == tails.end() ||
        !edges.count(edge_key(found->second, negative)))
      return StableCoreResult::NoStructure;
  }

  // Search a private projection in the same canonical order as a DIMACS file:
  // binaries first in import order, then the retained cover clauses.  F is
  // never weakened.  The authenticated resolvents stay live for CDCL and the
  // candidate is accepted only after whole-formula validation below.
  std::vector<std::vector<Lit>> core;
  core.reserve(edges.size() + covers.size());
  std::vector<Var> import_order;
  kernel_vars_in_import_order(k, import_order);
  for (const Var variable : import_order) {
    const Lit literal = make_lit(variable, true);
    for (const WatchEntry watch : k.watches.entries(literal)) {
      if (!watch.binary() ||
          kernel_import_lit_order(k, watch.other()) <
              kernel_import_lit_order(k, literal))
        continue;
      core.push_back({literal, watch.other()});
    }
  }
  for (const Ref ref : covers) {
    const Clause *clause = clause_at(k.arena, ref);
    core.emplace_back(clause->begin(), clause->end());
  }

  ++k.stats.independent_set_stable_cores;
  k.stats.independent_set_stable_cover_clauses += covers.size();
  k.stats.independent_set_stable_resolvents += guarded.size();
  k.stats.independent_set_model_vertices = k.nvars;
  strategy::WalkParameters search;
  search.step_budget = parameters.maximum_steps
                           ? parameters.maximum_steps
                           : UINT64_C(200000000);
  search.random_seed =
      parameters.seed == strategy::IndependentSetModelParameters::SEED_DEFAULT
          ? 1
          : parameters.seed;
  search.algorithm =
      strategy::WalkParameters::Algorithm::ConfigurationChecking;
  search.initialization = strategy::WalkParameters::Initialization::Negative;
  const ConfigurationCheckingModelResult candidate =
      configuration_checking_model(k, core, search);
  k.stats.independent_set_model_steps += candidate.steps;
  if (candidate.interrupted) {
    ++k.stats.independent_set_model_interrupted;
    return StableCoreResult::Interrupted;
  }
  if (!candidate.sat || !lucky_model_valid(k, candidate.assignment))
    return StableCoreResult::NoModel;
  lucky_publish_model(k, candidate.assignment);
  ++k.stats.independent_set_model_sat;
  return StableCoreResult::Sat;
}

// The owner's detection: K = max clause size - 1 and N solves
// variables - N = sum_{i=1..N} (min(i, K) + 1); graph edges are the negative
// binaries among the first N variables. Nothing reads names or comments.
bool recognize(Kernel &k, const strategy::IndependentSetModelParameters &p,
               Shape &shape) {
  const uint64_t variables = k.external_to_internal.size();
  uint32_t largest = 0;
  uint64_t visited = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    if (!clause->garbage())
      largest = std::max(largest, clause->size);
    ref += clause_actual_words(clause);
    if (!(++visited & ((uint64_t{1} << 18) - 1)) &&
        kernel_rule_interrupt(k, k.stats.independent_set_model_interrupted))
      return false;
  }
  if (largest < 3)
    return false;
  const uint64_t target = largest - 1;
  uint64_t states = 0, vertices = 0;
  for (uint64_t n = 1; n <= p.maximum_vertices && n < variables; ++n) {
    states += std::min(n, target) + 1;
    if (states + n == variables) {
      vertices = n;
      break;
    }
    if (states + n > variables)
      return false;
  }
  if (!vertices || target > vertices)
    return false;
  shape.vertices = static_cast<uint32_t>(vertices);
  shape.target = static_cast<uint32_t>(target);
  shape.adjacency.assign(shape.vertices, {});
  for (Lit literal = 0; literal < 2 * k.nvars; ++literal) {
    if (!negated(literal))
      continue;
    const int first = kernel_externalize(k, literal);
    if (first >= 0 || uint64_t(-int64_t(first)) > vertices)
      continue;
    for (const WatchEntry watch : k.watches.entries(literal)) {
      if (!watch.binary() || literal > watch.other() ||
          !negated(watch.other()))
        continue;
      const int second = kernel_externalize(k, watch.other());
      if (second >= 0 || uint64_t(-int64_t(second)) > vertices)
        continue;
      const uint32_t a = static_cast<uint32_t>(-first) - 1;
      const uint32_t b = static_cast<uint32_t>(-second) - 1;
      shape.adjacency[a].push_back(b);
      shape.adjacency[b].push_back(a);
      ++shape.edges;
    }
  }
  if (!shape.edges)
    return false;
  for (auto &row : shape.adjacency) {
    std::sort(row.begin(), row.end());
    row.erase(std::unique(row.begin(), row.end()), row.end());
  }
  return true;
}

struct SplitMix64 {
  uint64_t state;
  uint64_t next() {
    uint64_t z = (state += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
  }
  uint64_t below(uint64_t n) { return next() % n; }
};

// Minimum-degree greedy (ties by vertex index), then a bounded local search:
// a free vertex enters; a vertex with one selected neighbour replaces it
// when a second free vertex then fits; a random selected vertex leaves when
// nothing moves. `blocked[v]` counts selected neighbours of v.
bool search(Kernel &k, const strategy::IndependentSetModelParameters &p,
            const Shape &shape, std::vector<uint8_t> &selected) {
  const uint32_t n = shape.vertices;
  selected.assign(n, 0);
  std::vector<uint32_t> blocked(n, 0);
  uint32_t size = 0;
  const auto add = [&](uint32_t v) {
    selected[v] = 1;
    ++size;
    for (const uint32_t u : shape.adjacency[v])
      ++blocked[u];
  };
  const auto remove = [&](uint32_t v) {
    selected[v] = 0;
    --size;
    for (const uint32_t u : shape.adjacency[v])
      --blocked[u];
  };
  std::vector<uint32_t> order(n);
  for (uint32_t v = 0; v < n; ++v)
    order[v] = v;
  std::stable_sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) {
    return shape.adjacency[a].size() < shape.adjacency[b].size();
  });
  for (const uint32_t v : order)
    if (!blocked[v] && !selected[v])
      add(v);
  k.stats.independent_set_model_greedy_size = size;

  SplitMix64 rng{p.seed};
  std::vector<uint32_t> candidates;
  const uint64_t steps = p.maximum_steps ? p.maximum_steps : n;
  for (uint64_t step = 0; step < steps && size < shape.target; ++step) {
    ++k.stats.independent_set_model_steps;
    if (!(step & 255u) &&
        kernel_rule_interrupt(k, k.stats.independent_set_model_interrupted))
      return false;
    candidates.clear();
    for (uint32_t v = 0; v < n; ++v)
      if (!selected[v] && blocked[v] <= 1)
        candidates.push_back(v);
    for (size_t i = candidates.size(); i-- > 1;)
      std::swap(candidates[i], candidates[rng.below(i + 1)]);
    bool moved = false;
    for (const uint32_t v : candidates) {
      if (!blocked[v]) {
        add(v);
        moved = true;
        break;
      }
      uint32_t u = UINT32_MAX;
      for (const uint32_t w : shape.adjacency[v])
        if (selected[w]) {
          u = w;
          break;
        }
      remove(u);
      add(v);
      for (uint32_t w = 0; w < n; ++w)
        if (!selected[w] && !blocked[w]) {
          add(w);
          moved = true;
          break;
        }
      if (moved)
        break;
      remove(v);
      add(u);
    }
    if (!moved && size) {
      uint32_t victim = static_cast<uint32_t>(rng.below(size));
      for (uint32_t v = 0; v < n; ++v)
        if (selected[v] && !victim--) {
          remove(v);
          break;
        }
    }
  }
  k.stats.independent_set_model_size = size;
  return size >= shape.target;
}

bool build_assignment(Kernel &k,
                      const strategy::IndependentSetModelParameters &p,
                      const Shape &shape, const std::vector<uint8_t> &selected,
                      std::vector<uint8_t> &assignment) {
  std::vector<int8_t> values(k.nvars, 0);
  for (uint32_t v = 0; v < shape.vertices; ++v) {
    const Var internal = k.external_to_internal[v];
    if (internal == INVALID_VAR || internal >= values.size())
      return false;
    values[internal] = selected[v] ? 1 : -1;
  }
  const uint64_t rounds =
      p.completion_rounds ? p.completion_rounds : shape.vertices;
  if (!lucky_complete_by_propagation(
          k, rounds, k.stats.independent_set_model_interrupted,
          k.stats.independent_set_model_completion_rounds, values))
    return false;
  assignment.assign(k.nvars, 0);
  for (Var variable = 0; variable < k.nvars; ++variable)
    assignment[variable] = values[variable] > 0;
  return true;
}

} // namespace

RuleEligibility independent_set_model_guard(const Kernel &k) {
  return lucky_root_eligibility(k);
}

ApplyResult independent_set_model_apply(Kernel &k) {
  assert(independent_set_model_guard(k));
  ++k.stats.independent_set_model_calls;
  const strategy::IndependentSetModelParameters p =
      strategy::IndependentSetModelParameters::from_kernel(k);
  if (!p.valid())
    return ApplyResult::NoOp;
  const StableCoreResult stable = reduce_stable_extension_core(k, p);
  if (stable == StableCoreResult::Sat) {
    ++k.stats.independent_set_model_recognized;
    return ApplyResult::Sat;
  }
  if (stable == StableCoreResult::NoModel) {
    ++k.stats.independent_set_model_recognized;
    return ApplyResult::NoOp;
  }
  if (stable == StableCoreResult::Interrupted)
    return ApplyResult::NoOp;
  Shape shape;
  if (!recognize(k, p, shape))
    return ApplyResult::NoOp;
  ++k.stats.independent_set_model_recognized;
  k.stats.independent_set_model_vertices = shape.vertices;
  k.stats.independent_set_model_target = shape.target;
  std::vector<uint8_t> selected;
  if (!search(k, p, shape, selected))
    return ApplyResult::NoOp;
  std::vector<uint8_t> assignment;
  if (!build_assignment(k, p, shape, selected, assignment) ||
      !lucky_model_valid(k, assignment))
    return ApplyResult::NoOp;
  lucky_publish_model(k, assignment);
  ++k.stats.independent_set_model_sat;
  return ApplyResult::Sat;
}

} // namespace tactix
