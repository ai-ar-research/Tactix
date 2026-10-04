#include "rules/semantic_extension/affine_basis_plan.hpp"

#include <algorithm>
#include <climits>
#include <map>
#include <set>

namespace tactix::affine_semantic_extension {
namespace {

struct ReducedRow {
  std::vector<External> variables;
  bool rhs = false;
  std::vector<size_t> sources;
};

struct AtomPair {
  BasisAtom first;
  BasisAtom second;
  friend bool operator<(const AtomPair &a, const AtomPair &b) {
    if (a.first < b.first) return true;
    if (b.first < a.first) return false;
    return a.second < b.second;
  }
};

template <class T>
void xor_sorted(const std::vector<T> &a, const std::vector<T> &b,
                std::vector<T> &result) {
  result.clear();
  result.reserve(a.size() + b.size());
  std::set_symmetric_difference(a.begin(), a.end(), b.begin(), b.end(),
                                std::back_inserter(result));
}

bool valid_equation(const Equation &equation) {
  if (!std::is_sorted(equation.variables.begin(), equation.variables.end()) ||
      std::adjacent_find(equation.variables.begin(), equation.variables.end()) !=
          equation.variables.end())
    return false;
  for (External variable : equation.variables)
    if (variable <= 0 || variable == INT32_MIN) return false;
  std::set<uint64_t> premises;
  for (uint64_t occurrence : equation.premises)
    if (!occurrence || !premises.insert(occurrence).second) return false;
  return true;
}

std::vector<BasisClause> direct_clauses(const std::vector<External> &variables,
                                        bool rhs) {
  std::vector<BasisClause> result;
  for (const auto &clause : clausal_form(variables, rhs)) {
    BasisClause converted;
    converted.reserve(clause.size());
    for (External literal : clause) {
      const External variable = literal < 0 ? -literal : literal;
      converted.push_back(
          {{BasisAtomKind::Existing, static_cast<uint32_t>(variable)},
           literal > 0});
    }
    result.push_back(std::move(converted));
  }
  return result;
}

std::vector<BasisClause> definition_clauses(BasisAtom first, BasisAtom second,
                                            BasisAtom output) {
  // first xor second xor output = 0.
  return {{{first, false}, {second, true}, {output, true}},
          {{first, true}, {second, false}, {output, true}},
          {{first, true}, {second, true}, {output, false}},
          {{first, false}, {second, false}, {output, false}}};
}

struct Planner {
  const std::vector<Equation> &input;
  const BasisPlanLimits &limits;
  const std::function<bool(uint64_t)> &charge;
  BasisPlan plan;
  std::map<AtomPair, BasisAtom> nodes;
  bool exhausted = false;
  bool interrupted = false;

  Planner(const std::vector<Equation> &input_, const BasisPlanLimits &limits_,
          const std::function<bool(uint64_t)> &charge_)
      : input(input_), limits(limits_), charge(charge_) {}

  bool consume(uint64_t amount, bool elimination = false) {
    if (amount > limits.maximum_work - plan.planning_work) {
      exhausted = true;
      return false;
    }
    if (!charge(amount)) {
      interrupted = true;
      return false;
    }
    plan.planning_work += amount;
    if (elimination) plan.elimination_work += amount;
    return true;
  }

  bool collect_premises(const std::vector<size_t> &sources,
                        std::vector<uint64_t> &premises) {
    std::set<uint64_t> unique;
    for (size_t source : sources) {
      if (source >= input.size()) return false;
      for (uint64_t occurrence : input[source].premises) {
        if (!consume(1)) return false;
        unique.insert(occurrence);
        if (unique.size() > limits.maximum_premises) {
          exhausted = true;
          return false;
        }
      }
    }
    premises.assign(unique.begin(), unique.end());
    return true;
  }

  BasisAtom tree(const std::vector<External> &variables, size_t begin,
                 size_t end, size_t row) {
    if (end - begin == 1)
      return {BasisAtomKind::Existing,
              static_cast<uint32_t>(variables[begin])};
    const size_t middle = begin + (end - begin) / 2;
    BasisAtom first = tree(variables, begin, middle, row);
    if (interrupted || exhausted) return {};
    BasisAtom second = tree(variables, middle, end, row);
    if (interrupted || exhausted) return {};
    if (second < first) std::swap(first, second);
    const AtomPair key{first, second};
    const auto found = nodes.find(key);
    if (found != nodes.end()) {
      auto &uses = plan.definitions[found->second.value - 1].used_by_rows;
      if (uses.empty() || uses.back() != row) uses.push_back(row);
      return found->second;
    }
    if (plan.definitions.size() == limits.maximum_nodes) {
      exhausted = true;
      return {};
    }
    // One node plus its four ternary defining clauses.
    if (!consume(13)) return {};
    const BasisAtom output{BasisAtomKind::Extension,
                           static_cast<uint32_t>(plan.definitions.size() + 1)};
    XorDefinition definition;
    definition.output = output;
    definition.first = first;
    definition.second = second;
    definition.clauses = definition_clauses(first, second, output);
    definition.used_by_rows.push_back(row);
    plan.definitions.push_back(std::move(definition));
    plan.projection.push_back(
        {output, first, second, ProjectionOperation::OmitExtension,
         ProjectionOperation::EvaluateOperandXor});
    nodes.emplace(key, output);
    return output;
  }

  bool materialize(ReducedRow reduced) {
    BasisRow row;
    row.variables = std::move(reduced.variables);
    row.rhs = reduced.rhs;
    row.source_equations = std::move(reduced.sources);
    if (!collect_premises(row.source_equations, row.premises)) return false;
    if (!consume(1 + row.variables.size() + row.source_equations.size()))
      return false;
    const size_t row_index = plan.rows.size();
    if (row.variables.empty()) {
      // An inconsistent zero row is the empty clause.  Tautological zero rows
      // are discarded by the caller before materialization.
      row.kind = BasisRowKind::DirectContradiction;
      if (!consume(1)) return false;
      row.clauses.emplace_back();
    } else if (row.variables.size() == 1) {
      row.kind = BasisRowKind::DirectUnit;
      row.root = {BasisAtomKind::Existing,
                  static_cast<uint32_t>(row.variables.front())};
      if (!consume(2)) return false;
      row.clauses = direct_clauses(row.variables, row.rhs);
    } else if (row.variables.size() == 2) {
      row.kind = BasisRowKind::DirectBinary;
      if (!consume(6)) return false;
      row.clauses = direct_clauses(row.variables, row.rhs);
    } else if (row.variables.size() <= limits.maximum_direct_width) {
      row.kind = BasisRowKind::DirectSmall;
      const uint64_t clauses = uint64_t{1} << (row.variables.size() - 1);
      if (!consume(clauses * (row.variables.size() + 1))) return false;
      row.clauses = direct_clauses(row.variables, row.rhs);
    } else {
      row.kind = BasisRowKind::Extended;
      row.root = tree(row.variables, 0, row.variables.size(), row_index);
      if (interrupted || exhausted) return false;
      if (!consume(2)) return false;
      row.clauses = {{{row.root, row.rhs}}};
    }
    plan.rows.push_back(std::move(row));
    return true;
  }
};

} // namespace

BasisPlanStatus build_basis_plan(const std::vector<Equation> &input,
                                 const BasisPlanLimits &limits,
                                 const std::function<bool(uint64_t)> &charge,
                                 BasisPlan &output) {
  if (!charge || !limits.maximum_work ||
      !limits.maximum_intermediate_width || !limits.maximum_sources ||
      !limits.maximum_premises || !limits.maximum_nodes ||
      limits.maximum_direct_width < 2 || limits.maximum_direct_width > 8)
    return BasisPlanStatus::Invalid;
  if (input.empty()) return BasisPlanStatus::NoStructure;

  Planner planner{input, limits, charge};
  planner.plan.input_rows = input.size();
  std::map<External, ReducedRow> basis;
  std::vector<ReducedRow> rows;
  for (size_t index = 0; index < input.size(); ++index) {
    const Equation &equation = input[index];
    if (equation.variables.size() > limits.maximum_intermediate_width ||
        equation.premises.size() > limits.maximum_premises)
      return BasisPlanStatus::Exhausted;
    if (!valid_equation(equation)) return BasisPlanStatus::Invalid;
    if (!planner.consume(1 + equation.variables.size() +
                         equation.premises.size()))
      return planner.interrupted ? BasisPlanStatus::Interrupted
                                 : BasisPlanStatus::Exhausted;
    ReducedRow row{equation.variables, equation.rhs, {index}};
    while (!row.variables.empty()) {
      const uint64_t amount = 1 + row.variables.size() + row.sources.size();
      if (!planner.consume(amount, true))
        return planner.interrupted ? BasisPlanStatus::Interrupted
                                   : BasisPlanStatus::Exhausted;
      const auto found = basis.find(row.variables.front());
      if (found == basis.end()) break;
      std::vector<External> variables;
      std::vector<size_t> sources;
      xor_sorted(row.variables, found->second.variables, variables);
      xor_sorted(row.sources, found->second.sources, sources);
      row.variables = std::move(variables);
      row.sources = std::move(sources);
      row.rhs ^= found->second.rhs;
      if (row.variables.size() > limits.maximum_intermediate_width ||
          row.sources.size() > limits.maximum_sources)
        return BasisPlanStatus::Exhausted;
    }
    if (row.sources.size() > limits.maximum_sources)
      return BasisPlanStatus::Exhausted;
    if (row.variables.empty()) {
      if (row.rhs) rows.push_back(std::move(row));
      continue;
    }
    basis.emplace(row.variables.front(), row);
    rows.push_back(std::move(row));
  }
  if (rows.empty()) return BasisPlanStatus::NoStructure;

  for (ReducedRow &row : rows)
    if (!planner.materialize(std::move(row)))
      return planner.interrupted ? BasisPlanStatus::Interrupted
                                 : BasisPlanStatus::Exhausted;
  output = std::move(planner.plan);
  return BasisPlanStatus::Complete;
}

} // namespace tactix::affine_semantic_extension
