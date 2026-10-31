#include "rules/semantic_extension/affine_image.hpp"

#include "kernel/types.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <climits>
#include <map>
#include <set>

namespace tactix::affine_semantic_extension {
namespace {

struct Scope {
  std::vector<External> variables;
  friend bool operator<(const Scope &a, const Scope &b) {
    return a.variables < b.variables;
  }
};

struct Pattern {
  std::vector<uint64_t> words;
  friend bool operator<(const Pattern &a, const Pattern &b) {
    return a.words < b.words;
  }
};

struct Witness {
  uint64_t occurrence = 0;
  std::vector<External> clause;
};

using Group = std::map<Pattern, Witness>;

struct PlanningSink {
  uint64_t maximum;
  uint64_t additions = 0, deletions = 0;
  std::vector<ProofEvent> events;

  bool add(const parity_certificate::SmallClause &clause, bool rat) {
    if (rat || additions == maximum) return false;
    ++additions;
    events.push_back({true, clause});
    return true;
  }
  bool erase(const parity_certificate::SmallClause &clause) {
    ++deletions;
    events.push_back({false, clause});
    return true;
  }
};

bool to_small(const std::vector<External> &input,
              parity_certificate::SmallClause &output) {
  if (input.size() > output.lits.size()) return false;
  output.size = static_cast<uint8_t>(input.size());
  std::copy(input.begin(), input.end(), output.lits.begin());
  return true;
}

} // namespace

std::vector<std::vector<External>> clausal_form(
    const std::vector<External> &variables, bool rhs) {
  if (variables.size() > 8 ||
      !std::is_sorted(variables.begin(), variables.end()) ||
      std::adjacent_find(variables.begin(), variables.end()) != variables.end())
    return {};
  for (External variable : variables)
    if (variable <= 0) return {};
  std::vector<std::vector<External>> clauses;
  const uint32_t assignments = uint32_t{1} << variables.size();
  clauses.reserve(assignments >> (variables.empty() ? 0 : 1));
  for (uint32_t mask = 0; mask < assignments; ++mask) {
    if ((std::popcount(mask) & 1u) == unsigned(rhs)) continue;
    std::vector<External> clause;
    clause.reserve(variables.size());
    for (uint32_t i = 0; i < variables.size(); ++i)
      clause.push_back(mask & (uint32_t{1} << i) ? -variables[i]
                                                 : variables[i]);
    clauses.push_back(std::move(clause));
  }
  return clauses;
}

bool authenticates_clausal_form(const Equation &equation) {
  if (equation.variables.empty() || equation.variables.size() > 8)
    return false;
  if (!std::is_sorted(equation.variables.begin(), equation.variables.end()) ||
      std::adjacent_find(equation.variables.begin(), equation.variables.end()) !=
          equation.variables.end() ||
      std::any_of(equation.variables.begin(), equation.variables.end(),
                  [](External variable) { return variable <= 0; }))
    return false;
  const uint64_t expected_clauses =
      uint64_t{1} << (equation.variables.size() - 1);
  if (equation.premises.size() != expected_clauses ||
      equation.premise_clauses.size() != expected_clauses)
    return false;

  std::array<uint64_t, 4> seen{};
  for (const std::vector<External> &clause : equation.premise_clauses) {
    if (clause.size() != equation.variables.size()) return false;
    uint32_t coordinates = 0;
    uint32_t negative = 0;
    for (const External literal : clause) {
      if (!literal || literal == INT32_MIN) return false;
      const External variable =
          static_cast<External>(external_variable_of(literal));
      const auto found = std::lower_bound(equation.variables.begin(),
                                          equation.variables.end(), variable);
      if (found == equation.variables.end() || *found != variable)
        return false;
      const uint32_t bit = uint32_t{1}
                           << static_cast<uint32_t>(found -
                                                    equation.variables.begin());
      if (coordinates & bit) return false;
      coordinates |= bit;
      if (literal < 0) negative |= bit;
    }
    if ((std::popcount(negative) & 1u) == unsigned(equation.rhs))
      return false;
    uint64_t &word = seen[negative / 64];
    const uint64_t bit = uint64_t{1} << (negative % 64);
    if (word & bit) return false;
    word |= bit;
  }
  return true;
}

Status recognize(const std::vector<Premise> &input, const Limits &limits,
                 const std::function<bool(uint64_t)> &charge, Image &output) {
  if (!charge || !limits.maximum_clauses || !limits.maximum_groups ||
      !limits.maximum_patterns || !limits.maximum_arity ||
      limits.maximum_arity > 8 || !limits.maximum_elimination_work ||
      !limits.maximum_intermediate_width || !limits.maximum_sources ||
      limits.maximum_output_width > 4)
    return Status::Invalid;

  Image result;
  std::map<Scope, Group> groups;
  std::set<uint64_t> identities;
  for (const Premise &premise : input) {
    if (result.clauses_seen == limits.maximum_clauses)
      return Status::Exhausted;
    ++result.clauses_seen;
    if (!charge(1 + premise.literals.size())) return Status::Interrupted;
    if (!premise.occurrence ||
        !identities.insert(premise.occurrence).second)
      return Status::Invalid;
    // Literal-domain validity is independent of whether this recognizer can
    // admit the clause's arity. Root-normalized discovery applies the same
    // rule before shortening; silently skipping an over-width 0/INT32_MIN
    // body would make the two authenticated entry paths disagree.
    if (std::any_of(premise.literals.begin(), premise.literals.end(),
                    [](External literal) {
                      return !literal || literal == INT32_MIN;
                    }))
      return Status::Invalid;
    if (premise.literals.empty() ||
        premise.literals.size() > limits.maximum_arity)
      continue;

    std::vector<std::pair<External, bool>> ordered;
    ordered.reserve(premise.literals.size());
    for (External literal : premise.literals)
      ordered.push_back(
          {static_cast<External>(external_variable_of(literal)), literal < 0});
    std::sort(ordered.begin(), ordered.end());
    bool unsupported = false;
    for (size_t i = 1; i < ordered.size(); ++i)
      if (ordered[i].first == ordered[i - 1].first) {
        unsupported = true;
        break;
      }
    if (unsupported) continue;

    Scope scope;
    Pattern pattern;
    pattern.words.assign((ordered.size() + 63) / 64, 0);
    for (size_t i = 0; i < ordered.size(); ++i) {
      scope.variables.push_back(ordered[i].first);
      if (ordered[i].second) pattern.words[i / 64] |= uint64_t{1} << (i % 64);
    }
    auto group = groups.find(scope);
    if (group == groups.end()) {
      if (groups.size() == limits.maximum_groups) return Status::Exhausted;
      group = groups.emplace(std::move(scope), Group{}).first;
    }
    if (!group->second.contains(pattern)) {
      if (result.patterns_seen == limits.maximum_patterns)
        return Status::Exhausted;
      ++result.patterns_seen;
      group->second.emplace(std::move(pattern),
                            Witness{premise.occurrence, premise.literals});
    }
  }
  result.groups_seen = groups.size();

  for (const auto &[scope, patterns] : groups) {
    const size_t expected = size_t{1} << (scope.variables.size() - 1);
    size_t counts[2] = {0, 0};
    for (const auto &[pattern, witness] : patterns) {
      (void)witness;
      unsigned parity = 0;
      for (uint64_t word : pattern.words) parity ^= std::popcount(word) & 1u;
      ++counts[parity];
    }
    for (unsigned forbidden = 0; forbidden < 2; ++forbidden) {
      if (counts[forbidden] != expected) continue;
      Equation equation;
      equation.variables = scope.variables;
      equation.rhs = !forbidden;
      for (const auto &[pattern, witness] : patterns) {
        unsigned parity = 0;
        for (uint64_t word : pattern.words)
          parity ^= std::popcount(word) & 1u;
        if (parity == forbidden) {
          equation.premises.push_back(witness.occurrence);
          equation.premise_clauses.push_back(witness.clause);
        }
      }
      result.equations.push_back(std::move(equation));
    }
  }
  if (result.equations.empty()) return Status::NoStructure;

  std::map<External, ReducedEquation> basis;
  for (size_t index = 0; index < result.equations.size(); ++index) {
    ReducedEquation row{result.equations[index].variables,
                        result.equations[index].rhs, {index}};
    if (row.variables.size() > limits.maximum_intermediate_width) continue;
    bool interrupted = false;
    if (!reduce_equation(
        row, basis, limits.maximum_intermediate_width,
        limits.maximum_sources, true, [&](uint64_t cost) {
          if (cost > limits.maximum_elimination_work - result.elimination_work)
            return false;
          if (!charge(cost)) {
            interrupted = true;
            return false;
          }
          result.elimination_work += cost;
          return true;
        }))
      return interrupted ? Status::Interrupted : Status::Exhausted;
    if (row.sources.empty()) continue;
    if (row.sources.size() > 1 &&
        row.variables.size() <= limits.maximum_output_width &&
        (!row.variables.empty() || row.rhs)) {
      Consequence consequence{row.variables, row.rhs, row.sources, {}};
      for (size_t source : consequence.sources)
        consequence.premises.insert(consequence.premises.end(),
            result.equations[source].premises.begin(),
            result.equations[source].premises.end());
      result.consequences.push_back(std::move(consequence));
    }
    if (!row.variables.empty())
      basis.emplace(row.variables.front(), std::move(row));
  }

  output = std::move(result);
  return Status::Complete;
}

bool build_proof_plan(const Image &image, size_t consequence_index,
                      uint64_t maximum_additions, ProofPlan &output) {
  if (consequence_index >= image.consequences.size() || !maximum_additions)
    return false;
  const Consequence &consequence = image.consequences[consequence_index];
  if (consequence.variables.size() > 4 || consequence.sources.size() < 2)
    return false;

  parity_certificate::Clauses premises;
  std::set<External> variables;
  ProofPlan result;
  result.premises = consequence.premises;
  for (size_t source : consequence.sources) {
    if (source >= image.equations.size()) return false;
    const Equation &equation = image.equations[source];
    if (equation.premises.size() != equation.premise_clauses.size())
      return false;
    for (const auto &clause : equation.premise_clauses) {
      parity_certificate::SmallClause small;
      if (!to_small(clause, small)) return false;
      premises.push_back(small);
      for (External literal : clause)
        variables.insert(
            static_cast<External>(external_variable_of(literal)));
    }
  }
  if (variables.size() > 16) return false;

  result.targets = clausal_form(consequence.variables, consequence.rhs);
  if (result.targets.empty() && !consequence.variables.empty()) return false;
  PlanningSink sink{maximum_additions, 0, 0, {}};
  parity_certificate::Deriver<PlanningSink> deriver(sink);
  for (const auto &target : result.targets) {
    parity_certificate::SmallClause small, kept;
    if (!to_small(target, small) ||
        !deriver.derive(small, premises, consequence.variables, kept))
      return false;
  }
  result.events = std::move(sink.events);
  result.additions = sink.additions;
  result.deletions = sink.deletions;
  output = std::move(result);
  return true;
}

} // namespace tactix::affine_semantic_extension
