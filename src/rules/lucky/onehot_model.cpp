#include "rules/lucky/onehot_model.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <map>
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

bool satisfies(const Kernel &k, const std::vector<uint8_t> &assignment) {
  const auto true_lit = [&](Lit lit) {
    const bool value = assignment[var_of(lit)] != 0;
    return value != negated(lit);
  };
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    if (!clause->garbage()) {
      bool sat = false;
      for (const Lit lit : *clause)
        if (true_lit(lit)) {
          sat = true;
          break;
        }
      if (!sat)
        return false;
    }
    ref += clause_actual_words(clause);
  }
  for (Lit lit = 0; lit < 2 * k.nvars; ++lit)
    for (const WatchEntry watch : k.watches.entries(lit))
      if (watch.binary() && lit <= watch.other() && !true_lit(lit) &&
          !true_lit(watch.other()))
        return false;
  return true;
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

ApplyResult onehot_model_apply(Kernel &k) {
  assert(onehot_model_guard(k));
  ++k.stats.onehot_model_calls;
  const strategy::OnehotModelParameters parameters =
      strategy::OnehotModelParameters::from_kernel(k);
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
    for (uint32_t index = 0; index < domains.size(); ++index)
      assignment[domains[index][field_values[index]]] = 1;
    ++k.stats.onehot_model_candidates;
    if (satisfies(k, assignment)) {
      for (Var var = 0; var < k.nvars; ++var) {
        const Lit positive = make_lit(var, false);
        if (!k.value(positive) && !kernel_var_retired(k, var))
          kernel_assign_decision(k, make_lit(var, assignment[var] == 0));
      }
      k.propagate_head = static_cast<uint32_t>(k.trail.size());
      ++k.stats.onehot_model_sat;
      return ApplyResult::Sat;
    }
    if (kernel_rule_interrupt(k, k.stats.onehot_model_interrupted))
      return ApplyResult::NoOp;
  }
  return ApplyResult::NoOp;
}

} // namespace tactix
