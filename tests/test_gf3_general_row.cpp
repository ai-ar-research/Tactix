#include "rules/semantic_extension/gf3_general_row.hpp"

#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace tactix;

namespace {

struct Fixture {
  std::vector<gf3_affine::Premise> source;
  std::vector<std::vector<int32_t>> values;
  uint64_t next = 1;

  uint64_t add(std::vector<int32_t> clause) {
    const uint64_t occurrence = next++;
    source.push_back({occurrence, std::move(clause), 9000 + occurrence});
    return occurrence;
  }

  uint32_t domain(std::vector<int32_t> literals) {
    assert(literals.size() == 3);
    const uint32_t index = static_cast<uint32_t>(values.size());
    values.push_back(literals);
    add(literals);
    for (uint8_t first = 0; first < 3; ++first)
      for (uint8_t second = first + 1; second < 3; ++second)
        add({-literals[first], -literals[second]});
    return index;
  }

  void equation(const std::vector<uint32_t> &domains,
                const std::vector<uint8_t> &coefficients, uint8_t rhs) {
    uint64_t tuples = 1;
    for (size_t i = 0; i < domains.size(); ++i) tuples *= 3;
    for (uint64_t tuple = 0; tuple < tuples; ++tuple) {
      uint64_t rest = tuple;
      uint8_t sum = 0;
      std::vector<int32_t> clause;
      for (size_t i = 0; i < domains.size(); ++i) {
        const uint8_t value = static_cast<uint8_t>(rest % 3);
        rest /= 3;
        sum = static_cast<uint8_t>(
            (sum + coefficients[i] * value) % 3);
        clause.push_back(-values[domains[i]][value]);
      }
      if (sum != rhs) add(std::move(clause));
    }
  }
};

Fixture fixture() {
  Fixture result;
  for (int32_t first = 1; first <= 10; first += 3)
    result.domain({first, first + 1, first + 2});
  // x + y + u = 0 and y + z + u = 0 imply the non-unit row x - z = 0.
  // With only x and z fixed, the source remains underdetermined in y/u, so
  // the proof planner must exercise its bounded case-split auxiliaries.
  result.equation({0, 1, 3}, {1, 1, 1}, 0);
  result.equation({1, 2, 3}, {1, 1, 1}, 0);
  return result;
}

gf3_affine::Theory recognize(const Fixture &fixture) {
  const gf3_affine::RecognitionBounds bounds{1000, 10000, 100, 100, 8,
                                              10000};
  auto result = gf3_affine::recognize(
      fixture.source, bounds, [](uint64_t) { return true; });
  assert(result.status == gf3_affine::RecognitionStatus::Recognized);
  return std::move(result.theory);
}

gf3_gaussian_bridge::Limits bridge_limits() {
  gf3_gaussian_bridge::Limits result;
  result.maximum_work = 1000000;
  result.maximum_source_clauses = 10000;
  result.maximum_source_literals = 100000;
  result.maximum_domains = 100;
  result.maximum_equations = 100;
  result.maximum_terms = 1000;
  result.maximum_authentication_references = 10000;
  result.maximum_references_per_equation = 1000;
  result.gaussian = {100000, 100000, 32, 100, 1000, 1000000};
  return result;
}

gf3_general_row::Limits limits() {
  gf3_general_row::Limits result;
  result.bridge = bridge_limits();
  result.maximum_work = 10000000;
  result.maximum_events = 10000;
  result.maximum_event_literals = 100000;
  result.maximum_active_clauses = 10000;
  result.maximum_retained_source_references = 100;
  result.maximum_row_tuples = 10000;
  result.maximum_event_width = 32;
  result.maximum_branch_domains = 32;
  result.maximum_row_width = 8;
  return result;
}

gf3_gaussian_bridge::Plan bridge(const gf3_affine::Theory &theory) {
  auto result = gf3_gaussian_bridge::build(
      theory, bridge_limits(), [](uint64_t) { return true; });
  assert(result.status == gf3_gaussian_bridge::Status::Complete);
  return std::move(result.plan);
}

uint32_t variable(int32_t literal) {
  return static_cast<uint32_t>(literal < 0 ? -int64_t(literal) : literal);
}

bool assign(std::vector<int8_t> &values, int32_t literal) {
  const uint32_t atom = variable(literal);
  const int8_t value = literal > 0 ? 1 : -1;
  if (values[atom] == -value) return false;
  values[atom] = value;
  return true;
}

bool rup(const std::vector<std::vector<int32_t>> &active,
         const std::vector<int32_t> &candidate, uint32_t maximum_variable) {
  std::vector<int8_t> values(uint64_t(maximum_variable) + 1, 0);
  for (int32_t literal : candidate)
    if (!assign(values, -literal)) return true;
  bool changed = true;
  while (changed) {
    changed = false;
    for (const auto &clause : active) {
      bool satisfied = false;
      uint32_t open = 0;
      int32_t last = 0;
      for (int32_t literal : clause) {
        const int8_t value = values[variable(literal)];
        if (value && (literal > 0) == (value > 0)) {
          satisfied = true;
          break;
        }
        if (!value) {
          ++open;
          last = literal;
        }
      }
      if (satisfied) continue;
      if (!open) return true;
      if (open == 1) {
        const uint32_t atom = variable(last);
        const int8_t before = values[atom];
        if (!assign(values, last)) return true;
        changed |= !before;
      }
    }
  }
  return false;
}

std::vector<std::vector<int32_t>> clauses(const gf3_affine::Theory &theory) {
  std::vector<std::vector<int32_t>> result;
  for (const auto &premise : theory.source) result.push_back(premise.literals);
  return result;
}

gf3_general_row::Request find_request(
    const gf3_affine::Theory &theory,
    const gf3_gaussian_bridge::Plan &gaussian) {
  for (uint32_t outcome = 0; outcome < gaussian.gaussian.outcomes.size();
       ++outcome)
    for (uint64_t tuple = 0; tuple < 100; ++tuple) {
      const gf3_general_row::Request request{outcome, tuple};
      const auto inspected =
          gf3_general_row::inspect(theory, gaussian, request, limits());
      if (inspected.status == gf3_general_row::Status::Planned)
        return request;
      if (inspected.status == gf3_general_row::Status::InvalidRequest) break;
    }
  assert(false && "expected one general-row request");
  return {};
}

void check_models(const gf3_affine::Theory &theory,
                  const std::vector<int32_t> &clause) {
  uint64_t assignments = 1;
  for (size_t i = 0; i < theory.domains.size(); ++i) assignments *= 3;
  bool found_model = false;
  for (uint64_t encoded = 0; encoded < assignments; ++encoded) {
    uint64_t rest = encoded;
    std::vector<uint8_t> selected(theory.domains.size());
    for (uint8_t &value : selected) {
      value = static_cast<uint8_t>(rest % 3);
      rest /= 3;
    }
    bool source_model = true;
    for (const auto &equation : theory.equations) {
      uint8_t sum = 0;
      for (size_t i = 0; i < equation.domains.size(); ++i)
        sum = static_cast<uint8_t>(
            (sum + equation.coefficients[i] *
                       selected[equation.domains[i]]) %
            3);
      source_model &= sum == equation.rhs;
    }
    if (!source_model) continue;
    found_model = true;
    bool satisfied = false;
    for (int32_t literal : clause) {
      for (size_t domain = 0; domain < theory.domains.size(); ++domain)
        for (uint8_t value = 0; value < 3; ++value)
          if (theory.domains[domain].values[value] == -literal)
            satisfied |= selected[domain] != value;
    }
    assert(satisfied);
  }
  assert(found_model);
}

struct Prepared {
  gf3_affine::Theory theory;
  gf3_gaussian_bridge::Plan bridge;
  gf3_general_row::Result result;
};

Prepared prepare() {
  Prepared prepared;
  prepared.theory = recognize(fixture());
  prepared.bridge = bridge(prepared.theory);
  const auto request = find_request(prepared.theory, prepared.bridge);
  prepared.result = gf3_general_row::build(
      prepared.theory, prepared.bridge, request, limits(),
      [](uint64_t) { return true; });
  assert(prepared.result.status == gf3_general_row::Status::Planned);
  return prepared;
}

void test_nonunit_row_clause_is_bounded_rup_and_model_preserving() {
  Prepared prepared = prepare();
  const auto &plan = prepared.result.plan;
  assert(plan.duplicate_key.row.size() == 2);
  assert(plan.duplicate_key.tuple == plan.request.tuple);
  assert(plan.consequence.size() == 2);
  assert(plan.projection.extension_variables.empty());
  assert(plan.projection.external_extent_unchanged);
  assert(!plan.projection.reconstruction_witness_required);
  assert(!plan.sources.empty());
  assert(plan.events.size() > 1);
  assert(plan.events.back().role == gf3_general_row::EventRole::Consequence);
  assert(plan.events.back().clause == plan.consequence);
  assert(prepared.result.consumed.events == plan.events.size());
  assert(prepared.result.consumed.row_tuples == 9);

  std::vector<std::vector<int32_t>> active = clauses(prepared.theory);
  assert(!rup(active, plan.consequence, prepared.theory.maximum_variable));
  for (const auto &event : plan.events) {
    assert(rup(active, event.clause, prepared.theory.maximum_variable));
    active.push_back(event.clause);
  }
  check_models(prepared.theory, plan.consequence);

  const auto replayed = gf3_general_row::validate(
      prepared.theory, prepared.bridge, plan, limits(),
      [](uint64_t) { return true; });
  assert(replayed.status == gf3_general_row::Status::Planned);
  assert(replayed.plan == plan);
  auto altered = plan;
  altered.events.front().clause.push_back(1234);
  const auto rejected = gf3_general_row::validate(
      prepared.theory, prepared.bridge, altered, limits(),
      [](uint64_t) { return true; });
  assert(rejected.status == gf3_general_row::Status::InvalidCertificate);
  assert(rejected.plan.events.empty());
}

void test_predicate_declines_satisfying_nonrow_and_duplicate_requests() {
  Prepared prepared = prepare();
  const auto &row = prepared.result.plan.duplicate_key.row;
  uint64_t satisfying = 0;
  bool found = false;
  for (uint64_t tuple = 0; tuple < 9; ++tuple) {
    uint64_t rest = tuple;
    uint8_t sum = 0;
    for (const auto &term : row) {
      sum = static_cast<uint8_t>(
          (sum + term.coefficient * (rest % 3)) % 3);
      rest /= 3;
    }
    if (sum == prepared.result.plan.duplicate_key.rhs) {
      satisfying = tuple;
      found = true;
      break;
    }
  }
  assert(found);
  auto request = prepared.result.plan.request;
  request.tuple = satisfying;
  assert(gf3_general_row::inspect(prepared.theory, prepared.bridge, request,
                                  limits()).status ==
         gf3_general_row::Status::NoConsequence);
  request.outcome = static_cast<uint32_t>(prepared.bridge.gaussian.outcomes.size());
  assert(gf3_general_row::inspect(prepared.theory, prepared.bridge, request,
                                  limits()).status ==
         gf3_general_row::Status::InvalidRequest);

  gf3_affine::Theory duplicate_theory = prepared.theory;
  duplicate_theory.source.push_back(
      {duplicate_theory.source.back().occurrence + 1,
       prepared.result.plan.consequence, 123456});
  assert(gf3_general_row::inspect(duplicate_theory, prepared.bridge,
                                  prepared.result.plan.request,
                                  limits()).status ==
         gf3_general_row::Status::Duplicate);
}

void test_limits_interruptions_and_tampering_are_atomic() {
  Prepared prepared = prepare();
  auto bounded = limits();
  bounded.maximum_row_width = 1;
  auto result = gf3_general_row::build(
      prepared.theory, prepared.bridge, prepared.result.plan.request,
      bounded, [](uint64_t) { return true; });
  assert(result.status == gf3_general_row::Status::LimitExceeded);
  assert(result.limit == gf3_general_row::LimitKind::RowWidth);
  assert(result.plan.events.empty());

  bounded = limits();
  bounded.maximum_events = 1;
  result = gf3_general_row::build(
      prepared.theory, prepared.bridge, prepared.result.plan.request,
      bounded, [](uint64_t) { return true; });
  assert(result.status == gf3_general_row::Status::LimitExceeded);
  assert(result.limit == gf3_general_row::LimitKind::Events);
  assert(result.plan.events.empty());

  uint64_t remaining = 200;
  result = gf3_general_row::build(
      prepared.theory, prepared.bridge, prepared.result.plan.request,
      limits(), [&](uint64_t amount) {
        if (amount > remaining) return false;
        remaining -= amount;
        return true;
      });
  assert(result.status == gf3_general_row::Status::Interrupted);
  assert(result.plan.events.empty());

  auto tampered = prepared.bridge;
  auto &coefficient = tampered.gaussian.derivations
                          [prepared.result.plan.derivation - 1]
                              .provenance.front()
                              .coefficient;
  coefficient = coefficient == 1 ? 2 : 1;
  result = gf3_general_row::build(
      prepared.theory, tampered, prepared.result.plan.request, limits(),
      [](uint64_t) { return true; });
  assert(result.status == gf3_general_row::Status::InvalidBridge);
  assert(result.plan.events.empty());
}

void emit_fixture(const std::string &cnf_path, const std::string &proof_path) {
  Prepared prepared = prepare();
  uint64_t auxiliaries = 0;
  for (const auto &event : prepared.result.plan.events)
    auxiliaries += event.role == gf3_general_row::EventRole::Auxiliary;
  std::cout << "c gf3-general-row outcome="
            << prepared.result.plan.request.outcome << " tuple="
            << prepared.result.plan.request.tuple << " row-width="
            << prepared.result.plan.duplicate_key.row.size()
            << " source-refs=" << prepared.result.plan.sources.size()
            << " branch-domains="
            << prepared.result.consumed.branch_domains << " events="
            << prepared.result.plan.events.size() << " auxiliaries="
            << auxiliaries << " work=" << prepared.result.consumed.work
            << " rup-checks=" << prepared.result.consumed.rup_checks
            << " propagated="
            << prepared.result.consumed.propagated_literals << '\n';
  const uint32_t first_aux = prepared.theory.maximum_variable + 1;
  const uint32_t second_aux = first_aux + 1;
  std::ofstream cnf(cnf_path);
  assert(cnf);
  cnf << "p cnf " << second_aux << ' '
      << prepared.theory.source.size() + 4 << '\n';
  for (const auto &premise : prepared.theory.source) {
    for (int32_t literal : premise.literals) cnf << literal << ' ';
    cnf << "0\n";
  }
  cnf << first_aux << ' ' << second_aux << " 0\n";
  cnf << first_aux << ' ' << -int32_t(second_aux) << " 0\n";
  cnf << -int32_t(first_aux) << ' ' << second_aux << " 0\n";
  cnf << -int32_t(first_aux) << ' ' << -int32_t(second_aux) << " 0\n";
  cnf.close();

  std::vector<std::vector<int32_t>> independent = {
      {int32_t(first_aux), int32_t(second_aux)},
      {int32_t(first_aux), -int32_t(second_aux)},
      {-int32_t(first_aux), int32_t(second_aux)},
      {-int32_t(first_aux), -int32_t(second_aux)},
  };
  assert(!rup(independent, {}, second_aux));
  std::ofstream proof(proof_path);
  assert(proof);
  for (const auto &event : prepared.result.plan.events) {
    for (int32_t literal : event.clause) proof << literal << ' ';
    proof << "0\n";
  }
  proof << first_aux << " 0\n";
  proof << -int32_t(first_aux) << " 0\n";
  proof << "0\n";
}

} // namespace

int main(int argc, char **argv) {
  test_nonunit_row_clause_is_bounded_rup_and_model_preserving();
  test_predicate_declines_satisfying_nonrow_and_duplicate_requests();
  test_limits_interruptions_and_tampering_are_atomic();
  if (argc == 3) emit_fixture(argv[1], argv[2]);
  else assert(argc == 1);
  std::cout << "GF(3) general-row consequence tests passed\n";
}
