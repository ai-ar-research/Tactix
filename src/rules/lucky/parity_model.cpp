#include "rules/lucky/parity_model.hpp"
#include "rules/lucky/model.hpp"
#include "rules/lucky/parity_refutation.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tactix {
namespace {

struct Triple {
  std::array<Var, 3> vars;
  friend bool operator==(const Triple &, const Triple &) = default;
};

struct TripleHash {
  size_t operator()(const Triple &key) const {
    size_t h = key.vars[0];
    h = h * 0x9e3779b1u + key.vars[1];
    return h * 0x9e3779b1u + key.vars[2];
  }
};

using Row = std::vector<uint64_t>;

inline bool bit(const Row &row, uint32_t column) {
  return (row[column >> 6] >> (column & 63)) & 1;
}

inline void flip(Row &row, uint32_t column) {
  row[column >> 6] ^= uint64_t{1} << (column & 63);
}

void xor_row(Row &target, const Row &source) {
  for (size_t i = 0; i < target.size(); ++i)
    target[i] ^= source[i];
}

int first_coefficient(const Row &row, uint32_t nvars) {
  const uint32_t full = nvars >> 6;
  for (uint32_t word = 0; word < full; ++word)
    if (row[word])
      return static_cast<int>(64 * word + std::countr_zero(row[word]));
  const uint32_t rest = nvars & 63;
  if (rest) {
    const uint64_t masked = row[full] & ((uint64_t{1} << rest) - 1);
    if (masked)
      return static_cast<int>(64 * full + std::countr_zero(masked));
  }
  return -1;
}

bool add_row(std::vector<Row> &basis, Row row, uint32_t nvars) {
  for (;;) {
    const int pivot = first_coefficient(row, nvars);
    if (pivot < 0)
      return !bit(row, nvars);
    if (basis[static_cast<size_t>(pivot)].empty()) {
      basis[static_cast<size_t>(pivot)] = std::move(row);
      return true;
    }
    xor_row(row, basis[static_cast<size_t>(pivot)]);
  }
}


} // namespace

RuleEligibility parity_model_guard(const Kernel &k) {
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

ApplyResult parity_model_apply(Kernel &k) {
  const uint64_t candidates_before = k.stats.parity_model_candidates;
  assert(parity_model_guard(k));
  ++k.stats.parity_model_calls;
  const strategy::ParityModelParameters parameters =
      strategy::ParityModelParameters::from_kernel(k);
  if (!parameters.valid() || !k.nvars)
    return ApplyResult::NoOp;
  if (kernel_rule_interrupt(k, k.stats.parity_model_interrupted))
    return ApplyResult::NoOp;
  const ApplyResult refutation = parity_refutation_apply(k, parameters);
  if (refutation == ApplyResult::Unsat)
    return refutation;
  if (k.nvars > parameters.maximum_variables || !parameters.candidate_budget)
    return ApplyResult::NoOp;

  std::unordered_map<Triple, uint8_t, TripleHash> groups;
  groups.reserve(static_cast<size_t>(
      std::min<uint64_t>({k.live_irredundant_large, k.nvars,
                          parameters.maximum_groups})));
  uint64_t constraints = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    if (!clause->garbage() &&
        ++constraints > parameters.maximum_constraints)
      return ApplyResult::NoOp;
    if (!clause->garbage() && clause->size == 3) {
      std::array<std::pair<Var, bool>, 3> ordered{{
          {var_of(clause->lits[0]), negated(clause->lits[0])},
          {var_of(clause->lits[1]), negated(clause->lits[1])},
          {var_of(clause->lits[2]), negated(clause->lits[2])}}};
      std::sort(ordered.begin(), ordered.end());
      if (ordered[0].first != ordered[1].first &&
          ordered[1].first != ordered[2].first) {
        const Triple key{{ordered[0].first, ordered[1].first,
                          ordered[2].first}};
        const uint8_t mask = static_cast<uint8_t>(
            unsigned(ordered[0].second) |
            (unsigned(ordered[1].second) << 1) |
            (unsigned(ordered[2].second) << 2));
        const auto found = groups.find(key);
        if (found == groups.end()) {
          if (groups.size() == parameters.maximum_groups)
            return ApplyResult::NoOp;
          groups.emplace(key, static_cast<uint8_t>(1u << mask));
        } else {
          found->second |= static_cast<uint8_t>(1u << mask);
        }
      }
    }
    ref += clause_actual_words(clause);
    if (kernel_rule_interrupt(k, k.stats.parity_model_interrupted))
      return ApplyResult::NoOp;
  }
  for (Lit lit = 0; lit < 2 * k.nvars; ++lit) {
    for (const WatchEntry watch : k.watches.entries(lit))
      if (watch.binary() && lit <= watch.other() &&
          ++constraints > parameters.maximum_constraints)
        return ApplyResult::NoOp;
    if (kernel_rule_interrupt(k, k.stats.parity_model_interrupted))
      return ApplyResult::NoOp;
  }

  const size_t words = (static_cast<size_t>(k.nvars) + 1 + 63) / 64;
  std::vector<Row> equations;
  std::vector<uint8_t> present(k.nvars, 0);
  equations.reserve(groups.size());
  std::vector<std::pair<Triple, uint8_t>> ordered_groups(groups.begin(),
                                                          groups.end());
  std::sort(ordered_groups.begin(), ordered_groups.end(),
            [](const auto &a, const auto &b) {
              return a.first.vars < b.first.vars;
            });
  for (const auto &[triple, masks] : ordered_groups) {
    if (static_cast<uint32_t>(std::popcount(masks)) <
        parameters.minimum_group_clauses)
      continue;
    int forbidden_parity = -1;
    bool common = true;
    for (unsigned mask = 0; mask != 8; ++mask) {
      if (!(masks & (1u << mask)))
        continue;
      const int parity = std::popcount(mask) & 1;
      if (forbidden_parity < 0)
        forbidden_parity = parity;
      else if (forbidden_parity != parity) {
        common = false;
        break;
      }
    }
    if (!common || forbidden_parity < 0)
      continue;
    Row row(words, 0);
    for (const Var var : triple.vars) {
      flip(row, var);
      present[var] = 1;
    }
    if (!forbidden_parity)
      flip(row, k.nvars);
    equations.push_back(std::move(row));
  }
  k.stats.parity_model_equations += equations.size();
  if (equations.size() < parameters.minimum_equations)
    return ApplyResult::NoOp;

  std::vector<Row> basis(k.nvars);
  for (Row &row : equations) {
    if (!add_row(basis, std::move(row), k.nvars))
      return ApplyResult::NoOp;
    if (kernel_rule_interrupt(k, k.stats.parity_model_interrupted))
      return ApplyResult::NoOp;
  }
  for (Var var = 0; var < k.nvars; ++var) {
    const Lit positive = make_lit(var, false);
    if (!present[var] || !k.value(positive))
      continue;
    Row row(words, 0);
    flip(row, var);
    if (k.value(positive) > 0)
      flip(row, k.nvars);
    if (!add_row(basis, std::move(row), k.nvars))
      return ApplyResult::NoOp;
  }

  std::vector<Var> free;
  uint32_t rank = 0;
  for (Var var = 0; var < k.nvars; ++var)
    if (!basis[var].empty())
      ++rank;
    else if (present[var])
      free.push_back(var);
  k.stats.parity_model_rank += rank;
  if (free.size() > parameters.maximum_free_variables)
    return ApplyResult::NoOp;

  const uint64_t affine_size = uint64_t{1} << free.size();
  const uint64_t candidates =
      std::min(affine_size, parameters.candidate_budget);
  std::vector<uint8_t> assignment(k.nvars, 0);
  std::vector<uint8_t> guidance;
  for (uint64_t candidate = 0; candidate < candidates; ++candidate) {
    std::fill(assignment.begin(), assignment.end(), uint8_t{0});
    for (Var var = 0; var < k.nvars; ++var)
      if (k.value(make_lit(var, false)) > 0)
        assignment[var] = 1;
    for (size_t i = 0; i < free.size(); ++i)
      assignment[free[i]] = static_cast<uint8_t>((candidate >> i) & 1);
    for (Var p = k.nvars; p-- > 0;) {
      if (basis[p].empty())
        continue;
      bool value = bit(basis[p], k.nvars);
      for (Var column = p + 1; column < k.nvars; ++column)
        if (bit(basis[p], column) && assignment[column])
          value = !value;
      assignment[p] = static_cast<uint8_t>(value);
    }
    ++k.stats.parity_model_candidates;
    if (lucky_model_valid(k, assignment)) {
      lucky_publish_model(k, assignment);
      ++k.stats.parity_model_sat;
      return ApplyResult::Sat;
    }
    if (guidance.empty())
      guidance = assignment;
    if (kernel_rule_interrupt(k, k.stats.parity_model_interrupted)) {
      kernel_offer_local_search_hint(k, strategy::LocalSearchKind::Parity,
                                     guidance, UINT64_MAX, false,
                                     {k.stats.parity_model_candidates - candidates_before,
                                      strategy::LocalSearchEffortUnit::CandidateChecks});
      return ApplyResult::NoOp;
    }
  }
  if (!guidance.empty())
    kernel_offer_local_search_hint(k, strategy::LocalSearchKind::Parity,
                                   guidance, UINT64_MAX, false,
                                   {k.stats.parity_model_candidates - candidates_before,
                                    strategy::LocalSearchEffortUnit::CandidateChecks});
  return ApplyResult::NoOp;
}

} // namespace tactix
