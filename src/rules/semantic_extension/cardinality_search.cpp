#include "rules/semantic_extension/cardinality_search.hpp"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <map>
#include <set>

namespace tactix::cardinality_search {
namespace {

int32_t variable(int32_t literal) {
  return static_cast<int32_t>(literal < 0 ? -int64_t(literal) : literal);
}

bool charge(Result &result, const Limits &limits, uint64_t amount) {
  if (amount > limits.maximum_work - result.work) {
    result.status = Status::ResourceExhausted;
    return false;
  }
  result.work += amount;
  return true;
}

Clause canonical(Clause clause) {
  std::sort(clause.begin(), clause.end());
  return clause;
}

bool combinations(const Constraint &constraint, uint32_t width, bool positive,
                  std::vector<Clause> &expected, Result &result,
                  const Limits &limits) {
  Clause selected;
  const auto enumerate = [&](auto &&self, uint32_t at) -> bool {
    if (!charge(result, limits, 1)) return false;
    if (selected.size() == width) {
      if (expected.size() == limits.maximum_source_clauses) {
        result.status = Status::ResourceExhausted;
        return false;
      }
      expected.push_back(canonical(selected));
      return true;
    }
    if (at == constraint.literals.size() ||
        constraint.literals.size() - at < width - selected.size())
      return true;
    selected.push_back(positive ? constraint.literals[at]
                                : -constraint.literals[at]);
    if (!self(self, at + 1)) return false;
    selected.pop_back();
    return self(self, at + 1);
  };
  return enumerate(enumerate, 0);
}

bool validate(const Constraint &constraint, const Limits &limits,
              Result &result) {
  const size_t size = constraint.literals.size();
  if (!size || size > limits.maximum_support_variables ||
      constraint.lower > constraint.upper || constraint.upper > size ||
      (constraint.lower == 0 && constraint.upper == size))
    return false;
  std::set<int32_t> variables;
  for (int32_t literal : constraint.literals)
    if (!literal || literal == INT32_MIN || variable(literal) == INT32_MAX ||
        !variables.insert(variable(literal)).second)
      return false;
  std::vector<Clause> expected;
  if (constraint.lower &&
      !combinations(constraint, static_cast<uint32_t>(size) -
                                     constraint.lower + 1, true, expected,
                    result, limits))
    return false;
  if (constraint.upper < size &&
      !combinations(constraint, constraint.upper + 1, false, expected,
                    result, limits))
    return false;
  if (expected.size() != constraint.premises.size()) return false;
  std::vector<Clause> actual;
  actual.reserve(constraint.premises.size());
  for (const Clause &premise : constraint.premises) {
    if (!charge(result, limits, premise.size() + 1)) return false;
    if (premise.empty()) return false;
    Clause sorted = canonical(premise);
    if (std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end())
      return false;
    actual.push_back(std::move(sorted));
  }
  std::sort(expected.begin(), expected.end());
  std::sort(actual.begin(), actual.end());
  return expected == actual;
}

} // namespace

ConstructResult construct(std::vector<int32_t> literals, uint32_t lower,
                          uint32_t upper, const Limits &limits) {
  ConstructResult output;
  const size_t size = literals.size();
  if (!size || size > limits.maximum_support_variables ||
      lower > upper || upper > size || (lower == 0 && upper == size) ||
      !limits.maximum_source_clauses || !limits.maximum_work)
    return output;
  std::set<int32_t> variables;
  for (int32_t literal : literals)
    if (!literal || literal == INT32_MIN || variable(literal) == INT32_MAX ||
        !variables.insert(variable(literal)).second)
      return output;
  output.constraint.literals = std::move(literals);
  output.constraint.lower = lower;
  output.constraint.upper = upper;
  Result meter;
  if (lower && !combinations(output.constraint,
                             static_cast<uint32_t>(size) - lower + 1, true,
                             output.constraint.premises, meter, limits)) {
    output.status = meter.status == Status::ResourceExhausted
                        ? ConstructStatus::ResourceExhausted
                        : ConstructStatus::Invalid;
    output.work = meter.work;
    return output;
  }
  if (upper < size &&
      !combinations(output.constraint, upper + 1, false,
                    output.constraint.premises, meter, limits)) {
    output.status = meter.status == Status::ResourceExhausted
                        ? ConstructStatus::ResourceExhausted
                        : ConstructStatus::Invalid;
    output.work = meter.work;
    return output;
  }
  output.status = ConstructStatus::Constructed;
  output.work = meter.work;
  return output;
}

Result inspect(const std::vector<Constraint> &constraints,
               const std::vector<int32_t> &trail, const Limits &limits) {
  Result result;
  if (!limits.maximum_support_variables ||
      limits.maximum_support_variables > 20 || !limits.maximum_constraints ||
      !limits.maximum_source_clauses || !limits.maximum_reason_width ||
      !limits.maximum_work || constraints.empty())
    return result;
  if (constraints.size() > limits.maximum_constraints) {
    result.status = Status::ResourceExhausted;
    return result;
  }
  std::set<int32_t> support;
  uint64_t clauses = 0;
  for (const Constraint &constraint : constraints) {
    if (!validate(constraint, limits, result)) {
      if (result.status != Status::ResourceExhausted)
        result.status = Status::Invalid;
      return result;
    }
    clauses += constraint.premises.size();
    if (clauses > limits.maximum_source_clauses) {
      result.status = Status::ResourceExhausted;
      return result;
    }
    for (int32_t literal : constraint.literals)
      support.insert(variable(literal));
  }
  if (support.size() > limits.maximum_support_variables) {
    result.status = Status::ResourceExhausted;
    return result;
  }
  std::vector<int32_t> variables(support.begin(), support.end());
  std::map<int32_t, uint32_t> index;
  for (uint32_t at = 0; at < variables.size(); ++at)
    index.emplace(variables[at], at);
  std::vector<int8_t> assigned(variables.size(), 0);
  for (int32_t literal : trail) {
    if (!charge(result, limits, 1)) return result;
    if (!literal || literal == INT32_MIN || variable(literal) == INT32_MAX) {
      result.status = Status::Invalid;
      return result;
    }
    const auto found = index.find(variable(literal));
    if (found == index.end()) continue;
    const int8_t value = literal > 0 ? 1 : -1;
    if (assigned[found->second] && assigned[found->second] != value) {
      result.status = Status::Invalid;
      return result;
    }
    assigned[found->second] = value;
  }
  std::vector<int8_t> forced(variables.size(), 0);
  for (uint32_t mask = 0; mask < (uint32_t{1} << variables.size()); ++mask) {
    if (!charge(result, limits, 1)) return result;
    bool compatible = true;
    for (uint32_t at = 0; at < variables.size(); ++at)
      if (assigned[at] && ((mask >> at) & 1u) != (assigned[at] > 0)) {
        compatible = false;
        break;
      }
    if (!compatible) continue;
    for (const Constraint &constraint : constraints) {
      if (!charge(result, limits, constraint.literals.size() + 1))
        return result;
      uint32_t count = 0;
      for (int32_t literal : constraint.literals)
        count += (((mask >> index.at(variable(literal))) & 1u) ==
                  (literal > 0));
      if (count < constraint.lower || count > constraint.upper) {
        compatible = false;
        break;
      }
    }
    if (!compatible) continue;
    ++result.models;
    for (uint32_t at = 0; at < variables.size(); ++at) {
      if (assigned[at]) continue;
      const int8_t value = ((mask >> at) & 1u) ? 1 : -1;
      if (result.models == 1) forced[at] = value;
      else if (forced[at] != value) forced[at] = 2;
    }
  }
  for (uint32_t at = 0; at < variables.size(); ++at)
    if (assigned[at]) result.reason.push_back(-assigned[at] * variables[at]);
  if (!result.models) result.status = Status::Conflict;
  else {
    result.status = Status::NoEvent;
    for (uint32_t at = 0; at < variables.size(); ++at)
      if (!assigned[at] && (forced[at] == 1 || forced[at] == -1)) {
        result.reason.push_back(forced[at] * variables[at]);
        result.status = Status::Implication;
        break;
      }
  }
  if (result.status == Status::NoEvent) result.reason.clear();
  if (result.reason.size() > limits.maximum_reason_width)
    result.status = Status::ResourceExhausted;
  return result;
}

ResolutionPlan plan_resolution(const std::vector<Constraint> &constraints,
                               const Clause &reason,
                               const ResolutionLimits &limits) {
  ResolutionPlan result;
  if (!limits.maximum_work || !limits.maximum_additions ||
      !limits.maximum_added_literals || constraints.empty())
    return result;
  std::set<Clause> seen;
  std::vector<Clause> working;
  std::set<int32_t> support;
  for (const Constraint &constraint : constraints) {
    for (int32_t literal : constraint.literals) {
      if (!literal || literal == INT32_MIN || variable(literal) == INT32_MAX)
        return result;
      support.insert(variable(literal));
    }
    for (const Clause &premise : constraint.premises) {
      if (result.work > limits.maximum_work ||
          premise.size() + 1 > limits.maximum_work - result.work) {
        result.status = Status::ResourceExhausted;
        return result;
      }
      result.work += premise.size() + 1;
      const Clause normalized = canonical(premise);
      for (int32_t literal : normalized)
        if (!literal || literal == INT32_MIN ||
            !support.count(variable(literal)))
          return result;
      if (seen.insert(normalized).second) working.push_back(normalized);
    }
  }
  Clause target = canonical(reason);
  std::set<int32_t> target_variables;
  for (int32_t literal : target)
    if (!literal || literal == INT32_MIN ||
        !support.count(variable(literal)) ||
        !target_variables.insert(variable(literal)).second)
      return result;

  const auto contains = [](const Clause &clause, int32_t literal) {
    return std::binary_search(clause.begin(), clause.end(), literal);
  };
  const auto add = [&](Clause clause) -> bool {
    if (result.additions.size() == limits.maximum_additions ||
        result.added_literals > limits.maximum_added_literals ||
        clause.size() > limits.maximum_added_literals -
                            result.added_literals) {
      result.status = Status::ResourceExhausted;
      return false;
    }
    result.added_literals += clause.size();
    result.additions.push_back(std::move(clause));
    return true;
  };
  for (int32_t pivot : support) {
    if (target_variables.count(pivot)) continue;
    std::vector<const Clause *> positive, negative;
    std::vector<Clause> next;
    for (const Clause &clause : working) {
      if (contains(clause, pivot)) positive.push_back(&clause);
      else if (contains(clause, -pivot)) negative.push_back(&clause);
      else next.push_back(clause);
    }
    for (const Clause *pos : positive)
      for (const Clause *neg : negative) {
        const uint64_t work = uint64_t{1} + pos->size() + neg->size();
        if (result.work > limits.maximum_work ||
            work > limits.maximum_work - result.work) {
          result.status = Status::ResourceExhausted;
          return result;
        }
        result.work += work;
        Clause resolvent;
        resolvent.reserve(pos->size() + neg->size() - 2);
        for (int32_t literal : *pos)
          if (literal != pivot) resolvent.push_back(literal);
        for (int32_t literal : *neg)
          if (literal != -pivot) resolvent.push_back(literal);
        resolvent = canonical(std::move(resolvent));
        resolvent.erase(std::unique(resolvent.begin(), resolvent.end()),
                        resolvent.end());
        bool tautology = false;
        for (int32_t literal : resolvent)
          if (contains(resolvent, -literal)) {
            tautology = true;
            break;
          }
        if (tautology || !seen.insert(resolvent).second) continue;
        next.push_back(resolvent);
        if (!add(std::move(resolvent))) return result;
      }
    working = std::move(next);
  }

  const Clause *witness = nullptr;
  for (const Clause &clause : working)
    if (std::includes(target.begin(), target.end(),
                      clause.begin(), clause.end()) &&
        (!witness || clause.size() < witness->size() ||
         (clause.size() == witness->size() && clause < *witness)))
      witness = &clause;
  if (!witness) {
    result.status = Status::NoEvent;
    result.additions.clear();
    result.added_literals = 0;
    return result;
  }
  if (result.additions.empty() || result.additions.back() != target)
    if (!add(target)) return result;
  result.status = target.empty() ? Status::Conflict : Status::Implication;
  return result;
}

} // namespace tactix::cardinality_search
