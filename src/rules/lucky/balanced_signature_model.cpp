#include "rules/lucky/balanced_signature_model.hpp"

#include "rules/lucky/model.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <utility>
#include <vector>

namespace tactix {
namespace {

struct Shape {
  uint32_t balls = 0;
  uint32_t width = 0;
  uint64_t signatures = 0;
  uint64_t pair_variables = 0;
  uint64_t boundary = 0;
};

int external_value(const Kernel &k, uint64_t dimacs) {
  if (!dimacs || dimacs > k.external_to_internal.size())
    return 0;
  return kernel_external_value(
      k, make_lit(static_cast<Var>(dimacs - 1), false));
}

bool power_of_three(uint32_t exponent, uint64_t &result) {
  result = 1;
  for (uint32_t i = 0; i < exponent; ++i) {
    if (result > UINT64_MAX / 3)
      return false;
    result *= 3;
  }
  return true;
}

bool validate_shape(const Kernel &k, uint32_t balls, uint32_t width,
                    Shape &out) {
  uint64_t cube = 0;
  if (balls < 3 || !power_of_three(width, cube))
    return false;
  const uint64_t signatures = (cube - 1) / 2;
  if (balls > UINT64_MAX / balls / width)
    return false;
  const uint64_t pairs = uint64_t{width} * balls * balls;
  if (balls > UINT64_MAX / 2 / signatures)
    return false;
  const uint64_t boundary = pairs + uint64_t{2} * balls * signatures;
  if (!boundary || boundary >= k.external_to_internal.size())
    return false;
  for (uint32_t w = 0; w < width; ++w)
    for (uint32_t ball = 0; ball < balls; ++ball) {
      const uint64_t diagonal =
          1 + uint64_t{w} * balls * balls + uint64_t{ball} * balls + ball;
      if (external_value(k, diagonal) >= 0)
        return false;
    }
  if (external_value(k, boundary + 1) <= 0)
    return false;
  out = {balls, width, signatures, pairs, boundary};
  return true;
}

bool infer_shape(const Kernel &k,
                 const strategy::BalancedSignatureModelParameters &p,
                 Shape &out) {
  if (external_value(k, 1) >= 0)
    return false;

  // Original unit clauses have the oldest unit identifiers.  Trying candidate
  // spacings in that order tolerates additional root implications while the
  // full diagonal and boundary checks keep admission structural.
  const auto earlier_root = [](const auto &a, const auto &b) {
    if (a.first && b.first && a.first != b.first)
      return a.first < b.first;
    if (bool(a.first) != bool(b.first))
      return bool(a.first);
    return a.second < b.second;
  };
  std::vector<std::pair<uint64_t, uint32_t>> negative_roots;
  negative_roots.reserve(p.maximum_width);
  for (Var external = 1; external < k.external_to_internal.size(); ++external) {
    if (external_value(k, uint64_t{external} + 1) >= 0)
      continue;
    if (external <= 1)
      continue;
    const uint32_t balls = external - 1;
    if (balls < 3 || balls > p.maximum_balls)
      continue;
    const std::pair<uint64_t, uint32_t> candidate = {
        kernel_external_unit_id(k, external), balls};
    const auto position =
        std::lower_bound(negative_roots.begin(), negative_roots.end(),
                         candidate, earlier_root);
    if (position == negative_roots.end() &&
        negative_roots.size() == p.maximum_width)
      continue;
    negative_roots.insert(position, candidate);
    if (negative_roots.size() > p.maximum_width)
      negative_roots.pop_back();
  }
  // The encoding's diagonal units form its leading root block.  Trying at
  // most one candidate per permitted coordinate keeps a false admission probe
  // at O(nvars + maximum_width^2*maximum_balls), even on formulas with many
  // unrelated root assignments.
  for (size_t i = 0; i < negative_roots.size(); ++i)
    for (uint32_t width = 1; width <= p.maximum_width; ++width)
      if (validate_shape(k, negative_roots[i].second, width, out))
        return true;
  return false;
}

std::vector<int> ternary_vector(uint64_t index, uint32_t width) {
  std::vector<int> result(width);
  for (uint32_t i = width; i-- > 0;) {
    result[i] = static_cast<int>(index % 3) - 1;
    index /= 3;
  }
  return result;
}

uint64_t score(const std::vector<int64_t> &sums) {
  uint64_t result = 0;
  for (const int64_t value : sums) {
    const uint64_t magnitude =
        static_cast<uint64_t>(value < 0 ? -value : value);
    if (magnitude && magnitude > UINT64_MAX / magnitude)
      return UINT64_MAX;
    const uint64_t square = magnitude * magnitude;
    if (result > UINT64_MAX - square)
      return UINT64_MAX;
    result += square;
  }
  return result;
}

struct SearchResult {
  std::vector<uint32_t> rows;
  std::vector<int8_t> signs;
  uint64_t objective = UINT64_MAX;
};

bool search_balanced_signatures(
    Kernel &k, const Shape &shape,
    const strategy::BalancedSignatureModelParameters &p, SearchResult &best) {
  std::vector<std::vector<int>> vectors(shape.signatures);
  for (uint64_t i = 0; i < shape.signatures; ++i)
    vectors[i] = ternary_vector(i, shape.width);

  std::vector<uint32_t> pool;
  pool.reserve(shape.signatures > 0 ? shape.signatures - 1 : 0);
  for (uint64_t i = 1; i < shape.signatures; ++i)
    pool.push_back(static_cast<uint32_t>(i));
  if (pool.size() + 1 < shape.balls)
    return false;

  std::mt19937_64 random(p.seed ^ uint64_t{shape.balls} * 1000003ULL ^
                         shape.width);
  std::vector<uint8_t> selected(shape.signatures);
  std::vector<uint32_t> rows;
  std::vector<int8_t> sign_by_row(shape.signatures);
  std::vector<int64_t> sums(shape.width);
  for (uint64_t restart = 0; restart < p.restarts; ++restart) {
    if (kernel_rule_interrupt(k, k.stats.balanced_signature_interrupted))
      return false;
    ++k.stats.balanced_signature_restarts;
    std::shuffle(pool.begin(), pool.end(), random);
    std::fill(selected.begin(), selected.end(), 0);
    std::fill(sign_by_row.begin(), sign_by_row.end(), 0);
    std::fill(sums.begin(), sums.end(), 0);
    rows.clear();
    rows.reserve(shape.balls);
    selected[0] = 1;
    rows.push_back(0);
    for (uint32_t i = 0; i + 1 < shape.balls; ++i) {
      selected[pool[i]] = 1;
      rows.push_back(pool[i]);
    }
    for (const uint32_t row : rows)
      sign_by_row[row] = (random() & 1) ? 1 : -1;
    sign_by_row[0] = 1;
    for (const uint32_t row : rows)
      for (uint32_t w = 0; w < shape.width; ++w)
        sums[w] += sign_by_row[row] * vectors[row][w];
    uint64_t objective = score(sums);
    double temperature =
        std::max(0.1, static_cast<double>(objective) / 5.0);

    for (uint64_t step = 0; step < p.steps_per_restart; ++step) {
      ++k.stats.balanced_signature_steps;
      if (objective < best.objective) {
        best.objective = objective;
        best.rows = rows;
        best.signs.resize(rows.size());
        for (size_t i = 0; i < rows.size(); ++i)
          best.signs[i] = sign_by_row[rows[i]];
      }
      if (!objective) {
        std::vector<std::pair<uint32_t, int8_t>> ordered;
        ordered.reserve(rows.size());
        for (const uint32_t row : rows)
          ordered.emplace_back(row, sign_by_row[row]);
        std::sort(ordered.begin(), ordered.end());
        for (size_t i = 0; i < ordered.size(); ++i) {
          best.rows[i] = ordered[i].first;
          best.signs[i] = ordered[i].second;
        }
        return true;
      }
      if (!(step & ((uint64_t{1} << 14) - 1)) &&
          kernel_rule_interrupt(k, k.stats.balanced_signature_interrupted))
        return false;

      uint64_t candidate_objective = UINT64_MAX;
      uint32_t candidate_kind = 0, old_row = 0, new_row = 0;
      int8_t new_sign = 0;
      std::vector<int64_t> candidate_sums;
      for (uint32_t sample = 0; sample < p.samples_per_step; ++sample) {
        const bool flip = (random() & 1) != 0;
        const uint32_t row = rows[random() % rows.size()];
        if (!row)
          continue;
        std::vector<int64_t> proposed = sums;
        uint32_t kind = 1;
        uint32_t replacement = row;
        int8_t replacement_sign = -sign_by_row[row];
        if (!flip) {
          replacement = static_cast<uint32_t>(random() % shape.signatures);
          if (selected[replacement])
            continue;
          kind = 2;
          replacement_sign = (random() & 1) ? 1 : -1;
        }
        for (uint32_t w = 0; w < shape.width; ++w)
          proposed[w] += -sign_by_row[row] * vectors[row][w] +
                         replacement_sign * vectors[replacement][w];
        const uint64_t proposed_objective = score(proposed);
        if (proposed_objective < candidate_objective) {
          candidate_objective = proposed_objective;
          candidate_kind = kind;
          old_row = row;
          new_row = replacement;
          new_sign = replacement_sign;
          candidate_sums = std::move(proposed);
        }
      }
      if (!candidate_kind)
        continue;
      const double acceptance =
          candidate_objective <= objective
              ? 1.0
              : std::exp((static_cast<double>(objective) -
                          static_cast<double>(candidate_objective)) /
                         temperature);
      if (candidate_objective <= objective ||
          std::generate_canonical<double, 32>(random) < acceptance) {
        if (candidate_kind == 1) {
          sign_by_row[old_row] = new_sign;
        } else {
          selected[old_row] = 0;
          selected[new_row] = 1;
          sign_by_row[old_row] = 0;
          sign_by_row[new_row] = new_sign;
          *std::find(rows.begin(), rows.end(), old_row) = new_row;
        }
        sums = std::move(candidate_sums);
        objective = candidate_objective;
      }
      // Match the owning solver's per-step annealing schedule exactly.
      temperature *= 0.99996;
    }
  }
  return false;
}

bool set_external(const Kernel &k, std::vector<int8_t> &assignment,
                  uint64_t dimacs, int8_t value) {
  if (!dimacs || dimacs > k.external_to_internal.size())
    return false;
  const Var internal = k.external_to_internal[dimacs - 1];
  if (internal == INVALID_VAR || internal >= assignment.size())
    return false;
  assignment[internal] = value;
  return true;
}

bool build_primary_assignment(const Kernel &k, const Shape &shape,
                              const SearchResult &solution,
                              std::vector<int8_t> &assignment) {
  assignment.assign(k.nvars, 0);
  for (uint64_t dimacs = 1; dimacs <= shape.boundary; ++dimacs)
    if (!set_external(k, assignment, dimacs, -1))
      return false;

  std::vector<std::vector<int>> codes(
      shape.balls, std::vector<int>(shape.width));
  for (uint32_t ball = 0; ball < shape.balls; ++ball) {
    const std::vector<int> row = ternary_vector(solution.rows[ball], shape.width);
    for (uint32_t w = 0; w < shape.width; ++w)
      codes[ball][w] = solution.signs[ball] * row[w];
  }
  for (uint32_t w = 0; w < shape.width; ++w) {
    std::vector<uint32_t> left, right;
    for (uint32_t ball = 0; ball < shape.balls; ++ball) {
      if (codes[ball][w] > 0)
        left.push_back(ball);
      else if (codes[ball][w] < 0)
        right.push_back(ball);
    }
    if (left.size() != right.size())
      return false;
    for (size_t i = 0; i < left.size(); ++i) {
      const uint64_t dimacs = 1 + uint64_t{w} * shape.balls * shape.balls +
                              uint64_t{left[i]} * shape.balls + right[i];
      if (!set_external(k, assignment, dimacs, 1))
        return false;
    }
  }
  for (uint32_t ball = 0; ball < shape.balls; ++ball) {
    const uint64_t error = solution.signs[ball] > 0 ? 1 : 0;
    const uint64_t dimacs =
        shape.pair_variables +
        uint64_t{solution.rows[ball]} * (2 * shape.balls) + 2 * ball +
        error + 1;
    if (dimacs <= shape.pair_variables || dimacs > shape.boundary ||
        !set_external(k, assignment, dimacs, 1))
      return false;
  }
  return true;
}

bool assign_literal(std::vector<int8_t> &assignment, Lit lit) {
  const Var variable = var_of(lit);
  const int8_t value = negated(lit) ? -1 : 1;
  if (assignment[variable] && assignment[variable] != value)
    return false;
  assignment[variable] = value;
  return true;
}

template <class It>
int propagate_clause(std::vector<int8_t> &assignment, It begin, It end) {
  Lit last = INVALID_LIT;
  uint32_t unassigned = 0;
  for (It it = begin; it != end; ++it) {
    const Lit lit = *it;
    const int8_t value = assignment[var_of(lit)];
    if (!value) {
      last = lit;
      ++unassigned;
    } else if ((value > 0) != negated(lit)) {
      return 0;
    }
  }
  if (!unassigned)
    return -1;
  if (unassigned == 1)
    return assign_literal(assignment, last) ? 1 : -1;
  return 0;
}

bool complete_assignment(
    Kernel &k, const strategy::BalancedSignatureModelParameters &parameters,
    std::vector<int8_t> &assignment) {
  for (Var variable = 0; variable < k.nvars; ++variable) {
    const int value = k.value(make_lit(variable, false));
    if (value && !assign_literal(
                     assignment, make_lit(variable, value < 0)))
      return false;
  }
  for (uint32_t round = 0; round < parameters.completion_rounds; ++round) {
    ++k.stats.balanced_signature_completion_rounds;
    bool changed = false;
    uint64_t visited = 0;
    for (Lit lit = 0; lit < 2 * k.nvars; ++lit)
      for (const WatchEntry watch : k.watches.entries(lit)) {
        if (!watch.binary() || lit > watch.other())
          continue;
        const Lit pair[2] = {lit, watch.other()};
        const int result =
            propagate_clause(assignment, std::begin(pair), std::end(pair));
        if (result < 0)
          return false;
        changed |= result > 0;
        if (!(++visited & ((uint64_t{1} << 20) - 1)) &&
            kernel_rule_interrupt(k, k.stats.balanced_signature_interrupted))
          return false;
      }
    for (Ref ref = 0; ref < k.arena.size();) {
      const Clause *clause = clause_at(k.arena, ref);
      const uint32_t words = clause_actual_words(clause);
      if (!clause->garbage()) {
        const int result =
            propagate_clause(assignment, clause->begin(), clause->end());
        if (result < 0)
          return false;
        changed |= result > 0;
      }
      ref += words;
    }
    if (!changed)
      break;
  }
  return true;
}

std::vector<uint8_t> boolean_assignment(const std::vector<int8_t> &values) {
  std::vector<uint8_t> result(values.size());
  for (size_t i = 0; i < values.size(); ++i)
    result[i] = values[i] > 0;
  return result;
}

} // namespace

RuleEligibility balanced_signature_model_guard(const Kernel &k) {
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

ApplyResult balanced_signature_model_apply(Kernel &k) {
  assert(balanced_signature_model_guard(k));
  ++k.stats.balanced_signature_calls;
  const strategy::BalancedSignatureModelParameters parameters =
      strategy::BalancedSignatureModelParameters::from_kernel(k);
  if (!parameters.valid())
    return ApplyResult::NoOp;
  Shape shape;
  if (!infer_shape(k, parameters, shape))
    return ApplyResult::NoOp;
  ++k.stats.balanced_signature_recognized;

  SearchResult solution;
  const bool solved =
      search_balanced_signatures(k, shape, parameters, solution);
  if (solution.rows.empty())
    return ApplyResult::NoOp;
  std::vector<int8_t> signed_assignment;
  if (!build_primary_assignment(k, shape, solution, signed_assignment))
    return ApplyResult::NoOp;
  std::vector<uint8_t> hint = boolean_assignment(signed_assignment);
  kernel_offer_local_search_hint(k,
                                 strategy::LocalSearchKind::BalancedSignature,
                                 hint, solution.objective, true);
  if (!solved)
    return ApplyResult::NoOp;

  ++k.stats.balanced_signature_candidates;
  if (!complete_assignment(k, parameters, signed_assignment))
    return ApplyResult::NoOp;
  std::vector<uint8_t> assignment = boolean_assignment(signed_assignment);
  if (!lucky_model_valid(k, assignment))
    return ApplyResult::NoOp;
  lucky_publish_model(k, assignment);
  ++k.stats.balanced_signature_sat;
  return ApplyResult::Sat;
}

} // namespace tactix
