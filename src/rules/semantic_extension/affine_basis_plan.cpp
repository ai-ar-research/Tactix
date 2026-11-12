#include "rules/semantic_extension/affine_basis_plan.hpp"

#include "kernel/types.hpp"

#include <map>
#include <set>

namespace tactix::affine_semantic_extension {
namespace {

bool valid_equation(const Equation &equation) {
  if (!positive_scope(equation.variables)) return false;
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
      const External variable =
          static_cast<External>(external_variable_of(literal));
      converted.push_back({static_cast<uint32_t>(variable), literal > 0});
    }
    result.push_back(std::move(converted));
  }
  return result;
}

struct Planner {
  const std::vector<Equation> &input;
  const BasisPlanLimits &limits;
  const std::function<bool(uint64_t)> &charge;
  BasisPlan plan;
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

  bool materialize(ReducedEquation reduced) {
    BasisRow row;
    row.variables = std::move(reduced.variables);
    row.rhs = reduced.rhs;
    row.source_equations = std::move(reduced.sources);
    if (!collect_premises(row.source_equations, row.premises)) return false;
    if (!consume(1 + row.variables.size() + row.source_equations.size()))
      return false;
    if (row.variables.empty()) {
      // An inconsistent zero row is the empty clause.  Tautological zero rows
      // are discarded by the caller before materialization.
      row.kind = BasisRowKind::DirectContradiction;
      if (!consume(1)) return false;
      row.clauses.emplace_back();
    } else if (row.variables.size() == 1) {
      row.kind = BasisRowKind::DirectUnit;
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
    }
    plan.rows.push_back(std::move(row));
    return true;
  }
};

} // namespace

BasisPlanStatus build_selectable_basis_plan(
    const std::vector<Equation> &input, const BasisPlanLimits &limits,
    const std::function<bool(uint64_t)> &charge, BasisPlan &output) {
  if (!charge || !limits.maximum_work ||
      !limits.maximum_intermediate_width || !limits.maximum_sources ||
      !limits.maximum_premises || !limits.maximum_nodes ||
      limits.maximum_direct_width < 2 || limits.maximum_direct_width > 8)
    return BasisPlanStatus::Invalid;
  if (input.empty()) return BasisPlanStatus::NoStructure;

  Planner planner{input, limits, charge};
  planner.plan.input_rows = input.size();
  std::map<External, ReducedEquation> basis;
  std::vector<ReducedEquation> rows;
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
    ReducedEquation row{equation.variables, equation.rhs, {index}};
    if (!reduce_equation(
            row, basis, limits.maximum_intermediate_width,
            limits.maximum_sources, false,
            [&](uint64_t amount) { return planner.consume(amount, true); }))
      return planner.interrupted ? BasisPlanStatus::Interrupted
                                 : BasisPlanStatus::Exhausted;
    if (row.variables.empty()) {
      if (row.rhs) rows.push_back(std::move(row));
      continue;
    }
    basis.emplace(row.variables.front(), row);
    rows.push_back(std::move(row));
  }
  if (rows.empty()) return BasisPlanStatus::NoStructure;

  for (ReducedEquation &row : rows) {
    if (!planner.materialize(std::move(row)))
      return planner.interrupted ? BasisPlanStatus::Interrupted
                                 : BasisPlanStatus::Exhausted;
  }
  if (planner.plan.rows.empty()) return BasisPlanStatus::NoStructure;
  output = std::move(planner.plan);
  return BasisPlanStatus::Complete;
}

} // namespace tactix::affine_semantic_extension
