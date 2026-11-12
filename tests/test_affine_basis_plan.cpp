#undef NDEBUG
#include "rules/semantic_extension/affine_basis_plan.hpp"

#include <cassert>
#include <cstdio>

using namespace tactix::affine_semantic_extension;

static Equation equation(std::initializer_list<External> variables, bool rhs,
                         std::initializer_list<uint64_t> premises) {
  Equation result;
  result.variables = variables;
  result.rhs = rhs;
  result.premises = premises;
  return result;
}

static BasisPlanLimits limits(uint32_t direct = 3) {
  return {1000000, 64, 64, 1000, 1000, direct};
}

static BasisPlanStatus plan_rows(const std::vector<Equation> &input,
                                 const BasisPlanLimits &bounds, BasisPlan &plan,
                                 bool charge = true) {
  return build_selectable_basis_plan(
      input, bounds, [charge](uint64_t) { return charge; }, plan);
}

static bool clause_value(const BasisClause &clause, uint64_t assignment) {
  for (const BasisLiteral &literal : clause)
    if (bool(assignment & (uint64_t{1} << (literal.variable - 1))) ==
        literal.positive)
      return true;
  return false;
}

static bool parity(const std::vector<External> &variables,
                   uint64_t assignment) {
  bool result = false;
  for (External variable : variables)
    result ^= bool(assignment & (uint64_t{1} << (variable - 1)));
  return result;
}

static bool row_value(const BasisRow &row, uint64_t assignment) {
  for (const BasisClause &clause : row.clauses)
    if (!clause_value(clause, assignment)) return false;
  return true;
}

static void direct_rows() {
  const std::vector<Equation> input = {
      equation({1}, true, {10}), equation({2, 3}, false, {20, 21}),
      equation({4, 5, 6}, true, {30, 31, 32, 33})};
  BasisPlan plan;
  assert(plan_rows(input, limits(), plan) == BasisPlanStatus::Complete);
  assert(plan.rows.size() == 3);
  assert(plan.rows[0].kind == BasisRowKind::DirectUnit);
  assert(plan.rows[1].kind == BasisRowKind::DirectBinary);
  assert(plan.rows[2].kind == BasisRowKind::DirectSmall);
  for (uint64_t assignment = 0; assignment < 64; ++assignment)
    for (const BasisRow &row : plan.rows)
      assert(row_value(row, assignment) ==
             (parity(row.variables, assignment) == row.rhs));
}

static void cancellation_provenance_and_contradiction() {
  std::vector<Equation> input = {
      equation({1, 2, 3, 4}, false, {10}),
      equation({1, 2, 5}, true, {11, 12})};
  BasisPlan plan;
  assert(plan_rows(input, limits(), plan) == BasisPlanStatus::Complete);
  assert(plan.rows.size() == 2);
  // A row wider than the direct bound is source metadata without clauses.
  assert(plan.rows[0].kind == BasisRowKind::Extended &&
         plan.rows[0].clauses.empty());
  assert((plan.rows[1].variables == std::vector<External>{3, 4, 5}));
  assert(plan.rows[1].rhs);
  assert((plan.rows[1].source_equations == std::vector<size_t>{0, 1}));
  assert((plan.rows[1].premises == std::vector<uint64_t>{10, 11, 12}));
  for (uint64_t assignment = 0; assignment < 32; ++assignment) {
    const bool sources =
        parity(input[0].variables, assignment) == input[0].rhs &&
        parity(input[1].variables, assignment) == input[1].rhs;
    if (sources)
      assert(parity(plan.rows[1].variables, assignment) == plan.rows[1].rhs);
  }

  input = {equation({1, 2, 3}, false, {20}),
           equation({1, 2, 3}, true, {21})};
  assert(plan_rows(input, limits(), plan) == BasisPlanStatus::Complete);
  assert(plan.rows.size() == 2);
  const BasisRow &conflict = plan.rows.back();
  assert(conflict.kind == BasisRowKind::DirectContradiction);
  assert(conflict.variables.empty() && conflict.rhs);
  assert((conflict.source_equations == std::vector<size_t>{0, 1}));
  assert(conflict.clauses.size() == 1 && conflict.clauses[0].empty());
}

static void transactional_failures() {
  BasisPlan sentinel;
  sentinel.input_rows = 777;
  const auto good = std::vector<Equation>{equation({1, 2, 3, 4}, true, {1})};
  assert(plan_rows({}, limits(), sentinel) == BasisPlanStatus::NoStructure);
  assert(sentinel.input_rows == 777);
  auto bad = good;
  bad[0].variables = {2, 1};
  assert(plan_rows(bad, limits(), sentinel) == BasisPlanStatus::Invalid);
  assert(sentinel.input_rows == 777);
  assert(plan_rows(good, limits(), sentinel, false) == BasisPlanStatus::Interrupted);
  assert(sentinel.input_rows == 777);
  auto bounded = limits(2);
  bounded.maximum_work = 1;
  assert(plan_rows(good, bounded, sentinel) == BasisPlanStatus::Exhausted);
  assert(sentinel.input_rows == 777);
}

int main() {
  direct_rows();
  cancellation_provenance_and_contradiction();
  transactional_failures();
  std::puts("affine basis plan: provenance, direct rows, wide metadata, and "
            "transactional bounds PASS");
}
