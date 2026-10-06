#include "rules/semantic_extension/nary_csp_equality.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace tactix::nary_csp_equality;

namespace {

struct Fixture {
  std::vector<Premise> source;
  Relation relation;
  uint64_t next = 1;

  uint64_t add(std::vector<int32_t> clause) {
    const uint64_t occurrence = next++;
    source.push_back({occurrence, std::move(clause), 1000 + occurrence});
    return occurrence;
  }

  Domain domain(std::vector<int32_t> values) {
    Domain result;
    result.values = std::move(values);
    result.at_least_one = add(result.values);
    const size_t n = result.values.size();
    result.at_most_one.assign(n * n, 0);
    for (size_t i = 0; i < n; ++i)
      for (size_t j = i + 1; j < n; ++j) {
        const uint64_t occurrence =
            add({-result.values[i], -result.values[j]});
        result.at_most_one[i * n + j] = occurrence;
        result.at_most_one[j * n + i] = occurrence;
      }
    return result;
  }

  void build(std::vector<int32_t> left = {1, 2, 3},
             std::vector<int32_t> right = {4, 5, 6},
             std::vector<int32_t> residual = {7, 8, 9}) {
    relation.domains.push_back(domain(std::move(left)));
    relation.domains.push_back(domain(std::move(right)));
    relation.domains.push_back(domain(std::move(residual)));
    for (uint32_t i = 0; i < relation.domains.front().values.size(); ++i) {
      std::vector<int32_t> clause;
      for (const Domain &candidate : relation.domains)
        clause.push_back(-candidate.values[i]);
      relation.forbidden.push_back({{i, i, i}, add(std::move(clause))});
    }
  }
};

Bounds generous(int32_t extension = 10) {
  return {1000, 10000, 16, 1000, 1000, 10000, extension};
}

uint32_t variable(int32_t literal) {
  return static_cast<uint32_t>(literal < 0 ? -int64_t(literal) : literal);
}

bool literal_value(int32_t literal, uint64_t assignment) {
  const bool value = assignment & (uint64_t{1} << (variable(literal) - 1));
  return literal > 0 ? value : !value;
}

bool clause_value(const std::vector<int32_t> &clause, uint64_t assignment) {
  return std::any_of(clause.begin(), clause.end(), [&](int32_t literal) {
    return literal_value(literal, assignment);
  });
}

bool rup(const std::vector<std::vector<int32_t>> &formula,
         const std::vector<int32_t> &addition) {
  std::map<uint32_t, bool> assigned;
  auto assign = [&](int32_t literal) {
    const uint32_t var = variable(literal);
    const bool value = literal > 0;
    const auto [where, inserted] = assigned.emplace(var, value);
    return inserted || where->second == value;
  };
  for (int32_t literal : addition)
    if (!assign(-literal)) return true;
  for (;;) {
    bool changed = false;
    for (const auto &clause : formula) {
      bool satisfied = false;
      int32_t unit = 0;
      uint32_t open = 0;
      for (int32_t literal : clause) {
        const auto found = assigned.find(variable(literal));
        if (found == assigned.end()) {
          unit = literal;
          ++open;
        } else if (found->second == (literal > 0)) {
          satisfied = true;
          break;
        }
      }
      if (satisfied) continue;
      if (!open) return true;
      if (open == 1) {
        const size_t before = assigned.size();
        if (!assign(unit)) return true;
        changed |= assigned.size() != before;
      }
    }
    if (!changed) return false;
  }
}

bool tautological(const std::vector<int32_t> &clause) {
  std::set<int32_t> literals(clause.begin(), clause.end());
  for (int32_t literal : literals)
    if (literals.contains(-literal)) return true;
  return false;
}

void verify_rat(const std::vector<std::vector<int32_t>> &formula,
                const Addition &addition) {
  assert(!addition.clause.empty() && addition.clause.front() == addition.pivot);
  for (const auto &other : formula) {
    if (std::find(other.begin(), other.end(), -addition.pivot) == other.end())
      continue;
    std::vector<int32_t> resolvent;
    for (int32_t literal : addition.clause)
      if (literal != addition.pivot) resolvent.push_back(literal);
    for (int32_t literal : other)
      if (literal != -addition.pivot) resolvent.push_back(literal);
    std::sort(resolvent.begin(), resolvent.end());
    resolvent.erase(std::unique(resolvent.begin(), resolvent.end()),
                    resolvent.end());
    assert(tautological(resolvent) || rup(formula, resolvent));
  }
}

void verify_proof_shape(const Fixture &fixture, const Plan &plan) {
  std::vector<std::vector<int32_t>> formula;
  for (const Premise &premise : fixture.source)
    formula.push_back(premise.literals);
  for (const Addition &addition : plan.additions) {
    if (addition.proof == ProofStep::Rup)
      assert(rup(formula, addition.clause));
    else
      verify_rat(formula, addition);
    formula.push_back(addition.clause);
  }
}

void verify_projection(const Fixture &fixture, const Plan &plan,
                       uint32_t original_variables) {
  const uint64_t original_limit = uint64_t{1} << original_variables;
  for (uint64_t assignment = 0; assignment < original_limit; ++assignment) {
    bool source_model = true;
    for (const Premise &premise : fixture.source)
      source_model &= clause_value(premise.literals, assignment);
    if (!source_model) continue;
    bool equal = false;
    for (size_t i = 0; i < plan.projection.left_values.size(); ++i)
      equal |= literal_value(plan.projection.left_values[i], assignment) &&
               literal_value(plan.projection.right_values[i], assignment);
    const uint64_t extended =
        assignment | (equal ? uint64_t{1} << original_variables : 0);
    for (const Addition &addition : plan.additions)
      assert(clause_value(addition.clause, extended));
    // The opposite extension value must violate the exact definition.
    const uint64_t opposite = extended ^ (uint64_t{1} << original_variables);
    assert(std::any_of(plan.additions.begin(), plan.additions.end(),
                       [&](const Addition &addition) {
                         return !clause_value(addition.clause, opposite);
                       }));
  }
}

void planned_equality_and_transfer() {
  Fixture fixture;
  fixture.build();
  const Plan plan = plan_equality(fixture.source, fixture.relation, {},
                                  generous(), [](uint64_t) { return true; });
  assert(plan.status == Status::Planned);
  assert(plan.diagonal_tuple_premises.size() == 3);
  assert(plan.additions.size() == 9);
  assert((plan.additions[0].clause == std::vector<int32_t>{10, -1, -4}));
  assert((plan.additions[3].clause == std::vector<int32_t>{-10, -1, 4}));
  assert((plan.additions[6].clause == std::vector<int32_t>{-10, -1, -7}));
  for (size_t i = 0; i < 3; ++i) {
    assert(plan.additions[i].proof == ProofStep::FreshPositiveRat);
    assert(plan.additions[3 + i].proof == ProofStep::EqualityReverseRat);
    assert(plan.additions[6 + i].proof == ProofStep::Rup);
    assert(plan.additions[6 + i].antecedents.size() == 2);
  }
  assert(plan.projection.extension_variable == 10);
  assert(plan.projection.exact_aligned_equality);
  assert(plan.projection.project_from_external_model);
  assert(!plan.projection.reconstruction_witness_required);
  verify_proof_shape(fixture, plan);
  verify_projection(fixture, plan, 9);
}

void signed_values_and_coordinate_permutation() {
  Fixture fixture;
  fixture.build({-1, 2, 3}, {4, -5, 6}, {7, 8, -9});
  const Selection selection{1, 0, 2};
  const Plan plan = plan_equality(fixture.source, fixture.relation, selection,
                                  generous(), [](uint64_t) { return true; });
  assert(plan.status == Status::Planned);
  assert((plan.additions[0].clause == std::vector<int32_t>{10, -4, 1}));
  assert((plan.additions[3].clause == std::vector<int32_t>{-10, -4, -1}));
  assert((plan.additions[6].clause == std::vector<int32_t>{-10, -4, -7}));
  verify_proof_shape(fixture, plan);
  verify_projection(fixture, plan, 9);
}

void exact_declines_and_bounds() {
  Fixture fixture;
  fixture.build();

  Relation missing = fixture.relation;
  missing.forbidden.erase(missing.forbidden.begin() + 1);
  std::vector<Premise> missing_source = fixture.source;
  const uint64_t removed = fixture.relation.forbidden[1].occurrence;
  missing_source.erase(std::remove_if(missing_source.begin(), missing_source.end(),
                                      [&](const Premise &premise) {
                                        return premise.occurrence == removed;
                                      }),
                       missing_source.end());
  Plan plan = plan_equality(missing_source, missing, {}, generous(),
                            [](uint64_t) { return true; });
  assert(plan.status == Status::NoConsequence && plan.additions.empty());

  std::vector<Premise> extra = fixture.source;
  extra.push_back({999, {1, -4}, 1999});
  plan = plan_equality(extra, fixture.relation, {}, generous(),
                       [](uint64_t) { return true; });
  assert(plan.status == Status::IncompleteTheory && plan.additions.empty());

  Relation malformed = fixture.relation;
  malformed.domains[0].at_most_one[1] = 0;
  malformed.domains[0].at_most_one[3] = 0;
  plan = plan_equality(fixture.source, malformed, {}, generous(),
                       [](uint64_t) { return true; });
  assert(plan.status == Status::IncompleteTheory && plan.additions.empty());

  plan = plan_equality(fixture.source, fixture.relation, {0, 0, 2},
                       generous(), [](uint64_t) { return true; });
  assert(plan.status == Status::InvalidRequest && plan.additions.empty());

  Bounds bounded = generous();
  bounded.maximum_additions = 8;
  plan = plan_equality(fixture.source, fixture.relation, {}, bounded,
                       [](uint64_t) { return true; });
  assert(plan.status == Status::BoundExceeded && plan.additions.empty());

  uint64_t allowance = 20;
  plan = plan_equality(fixture.source, fixture.relation, {}, generous(),
                       [&](uint64_t amount) {
                         if (amount > allowance) return false;
                         allowance -= amount;
                         return true;
                       });
  assert(plan.status == Status::EffortExhausted && plan.additions.empty());
  assert(plan.consumed_work > 0);
}

void emit_checked_fixture(const std::string &stem) {
  Fixture fixture;
  fixture.build();
  const Plan plan = plan_equality(fixture.source, fixture.relation, {},
                                  generous(), [](uint64_t) { return true; });
  assert(plan.status == Status::Planned);
  std::ofstream cnf(stem + ".cnf");
  assert(cnf);
  // Keep the theory slice satisfiable while every equality lemma is checked.
  // A disjoint, non-root-UP two-variable square closes UNSAT only after all
  // planned additions have been serialized.
  cnf << "p cnf 12 " << fixture.source.size() + 4 << '\n';
  for (const Premise &premise : fixture.source) {
    for (int32_t literal : premise.literals) cnf << literal << ' ';
    cnf << "0\n";
  }
  cnf << "11 12 0\n11 -12 0\n-11 12 0\n-11 -12 0\n";
  cnf.close();

  std::ofstream proof(stem + ".dsr");
  assert(proof);
  for (const Addition &addition : plan.additions) {
    for (int32_t literal : addition.clause) proof << literal << ' ';
    proof << "0\n";
  }
  proof << "11 0\n-11 0\n0\n";
}

} // namespace

int main(int argc, char **argv) {
  if (argc == 2) {
    emit_checked_fixture(argv[1]);
    std::puts("test_nary_csp_equality: DSR fixture emitted");
    return 0;
  }
  assert(argc == 1);
  planned_equality_and_transfer();
  signed_values_and_coordinate_permutation();
  exact_declines_and_bounds();
  std::puts("test_nary_csp_equality: passed");
}
