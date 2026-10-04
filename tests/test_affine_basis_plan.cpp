#undef NDEBUG
#include "rules/semantic_extension/affine_basis_plan.hpp"

#include <cassert>
#include <cstdio>
#include <map>

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

static bool atom_value(const BasisAtom &atom, uint64_t assignment,
                       const std::vector<bool> &extensions) {
  if (atom.kind == BasisAtomKind::Existing)
    return assignment & (uint64_t{1} << (atom.value - 1));
  assert(atom.value && atom.value <= extensions.size());
  return extensions[atom.value - 1];
}

static bool clause_value(const BasisClause &clause, uint64_t assignment,
                         const std::vector<bool> &extensions) {
  for (const BasisLiteral &literal : clause)
    if (atom_value(literal.atom, assignment, extensions) == literal.positive)
      return true;
  return false;
}

static bool local_clause_value(const BasisClause &clause,
                               const std::map<BasisAtom, bool> &values) {
  for (const BasisLiteral &literal : clause) {
    const auto found = values.find(literal.atom);
    assert(found != values.end());
    if (found->second == literal.positive) return true;
  }
  return false;
}

static void verify_exact_definition(const XorDefinition &definition) {
  for (uint32_t mask = 0; mask < 8; ++mask) {
    const bool first = mask & 1, second = mask & 2, output = mask & 4;
    std::map<BasisAtom, bool> values = {
        {definition.first, first},
        {definition.second, second},
        {definition.output, output},
    };
    bool cnf = true;
    for (const BasisClause &clause : definition.clauses)
      cnf &= local_clause_value(clause, values);
    assert(cnf == (output == (first ^ second)));
  }
}

static std::vector<bool> evaluate_extensions(const BasisPlan &plan,
                                             uint64_t assignment) {
  std::vector<bool> values;
  for (const XorDefinition &definition : plan.definitions) {
    assert(definition.output.kind == BasisAtomKind::Extension);
    assert(definition.output.value == values.size() + 1);
    values.push_back(atom_value(definition.first, assignment, values) ^
                     atom_value(definition.second, assignment, values));
    for (const BasisClause &clause : definition.clauses)
      assert(clause_value(clause, assignment, values));
  }
  return values;
}

static bool parity(const std::vector<External> &variables,
                   uint64_t assignment) {
  bool result = false;
  for (External variable : variables)
    result ^= bool(assignment & (uint64_t{1} << (variable - 1)));
  return result;
}

static bool row_value(const BasisRow &row, uint64_t assignment,
                      const std::vector<bool> &extensions) {
  for (const BasisClause &clause : row.clauses)
    if (!clause_value(clause, assignment, extensions)) return false;
  return true;
}

static void direct_rows() {
  const std::vector<Equation> input = {
      equation({1}, true, {10}), equation({2, 3}, false, {20, 21}),
      equation({4, 5, 6}, true, {30, 31, 32, 33})};
  BasisPlan plan;
  assert(build_basis_plan(input, limits(), [](uint64_t) { return true; },
                          plan) == BasisPlanStatus::Complete);
  assert(plan.rows.size() == 3 && plan.definitions.empty());
  assert(plan.rows[0].kind == BasisRowKind::DirectUnit);
  assert(plan.rows[1].kind == BasisRowKind::DirectBinary);
  assert(plan.rows[2].kind == BasisRowKind::DirectSmall);
  for (uint64_t assignment = 0; assignment < 64; ++assignment) {
    const std::vector<bool> extensions;
    for (const BasisRow &row : plan.rows)
      assert(row_value(row, assignment, extensions) ==
             (parity(row.variables, assignment) == row.rhs));
  }
}

static void shared_extension_and_projection() {
  // Balanced trees for these rows share the exact subtree 5 xor 6.
  const std::vector<Equation> input = {
      equation({1, 2, 5, 6}, false, {101}),
      equation({3, 4, 5, 6}, true, {102})};
  BasisPlan plan;
  assert(build_basis_plan(input, limits(2), [](uint64_t) { return true; },
                          plan) == BasisPlanStatus::Complete);
  assert(plan.rows.size() == 2);
  assert(plan.rows[0].kind == BasisRowKind::Extended);
  assert(plan.rows[1].kind == BasisRowKind::Extended);
  assert(plan.definitions.size() == 5);
  assert(plan.projection.size() == plan.definitions.size());
  bool shared = false;
  for (size_t i = 0; i < plan.definitions.size(); ++i) {
    const XorDefinition &definition = plan.definitions[i];
    const ProjectionObligation &projection = plan.projection[i];
    assert(definition.clauses.size() == 4);
    verify_exact_definition(definition);
    assert(projection.extension == definition.output);
    assert(projection.first == definition.first);
    assert(projection.second == definition.second);
    assert(projection.original_projection ==
           ProjectionOperation::OmitExtension);
    assert(projection.extension_reconstruction ==
           ProjectionOperation::EvaluateOperandXor);
    if (definition.used_by_rows.size() == 2) shared = true;
  }
  assert(shared);
  for (uint64_t assignment = 0; assignment < 64; ++assignment) {
    const auto extensions = evaluate_extensions(plan, assignment);
    for (const BasisRow &row : plan.rows)
      assert(row_value(row, assignment, extensions) ==
             (parity(row.variables, assignment) == row.rhs));
  }
}

static void cancellation_provenance_and_contradiction() {
  std::vector<Equation> input = {
      equation({1, 2, 3, 4}, false, {10}),
      equation({1, 2, 5}, true, {11, 12})};
  BasisPlan plan;
  assert(build_basis_plan(input, limits(), [](uint64_t) { return true; },
                          plan) == BasisPlanStatus::Complete);
  assert(plan.rows.size() == 2);
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
  assert(build_basis_plan(input, limits(), [](uint64_t) { return true; },
                          plan) == BasisPlanStatus::Complete);
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
  assert(build_basis_plan({}, limits(), [](uint64_t) { return true; },
                          sentinel) == BasisPlanStatus::NoStructure);
  assert(sentinel.input_rows == 777);
  auto bad = good;
  bad[0].variables = {2, 1};
  assert(build_basis_plan(bad, limits(), [](uint64_t) { return true; },
                          sentinel) == BasisPlanStatus::Invalid);
  assert(sentinel.input_rows == 777);
  assert(build_basis_plan(good, limits(), [](uint64_t) { return false; },
                          sentinel) == BasisPlanStatus::Interrupted);
  assert(sentinel.input_rows == 777);
  auto bounded = limits(2);
  bounded.maximum_nodes = 1;
  assert(build_basis_plan(good, bounded, [](uint64_t) { return true; },
                          sentinel) == BasisPlanStatus::Exhausted);
  assert(sentinel.input_rows == 777);
  bounded = limits(2);
  bounded.maximum_work = 1;
  assert(build_basis_plan(good, bounded, [](uint64_t) { return true; },
                          sentinel) == BasisPlanStatus::Exhausted);
  assert(sentinel.input_rows == 777);
}

int main() {
  direct_rows();
  shared_extension_and_projection();
  cancellation_provenance_and_contradiction();
  transactional_failures();
  std::puts("affine basis plan: provenance, direct rows, shared XOR definitions, "
            "projection, and transactional bounds PASS");
}
