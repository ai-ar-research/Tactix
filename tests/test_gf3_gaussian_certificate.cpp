#undef NDEBUG
#include "rules/semantic_extension/gf3_gaussian_certificate.hpp"

#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdint>
#include <iostream>
#include <set>
#include <vector>

using namespace tactix;

namespace {

struct Fixture {
  std::vector<gf3_affine::Premise> source;
  std::vector<std::vector<int32_t>> values;
  uint64_t next = 1;

  uint64_t add(std::vector<int32_t> clause) {
    const uint64_t occurrence = next++;
    source.push_back({occurrence, std::move(clause), 7000 + occurrence});
    return occurrence;
  }

  uint32_t domain(std::vector<int32_t> literals) {
    assert(literals.size() == 3);
    const uint32_t result = static_cast<uint32_t>(values.size());
    values.push_back(literals);
    add(literals);
    for (uint8_t first = 0; first < 3; ++first)
      for (uint8_t second = first + 1; second < 3; ++second)
        add({-literals[first], -literals[second]});
    return result;
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

Fixture unit_fixture() {
  Fixture fixture;
  fixture.domain({1, 2, 3});
  fixture.domain({4, 5, 6});
  fixture.domain({7, 8, 9});
  fixture.equation({0, 1}, {1, 1}, 0);
  fixture.equation({1, 2}, {1, 1}, 0);
  fixture.equation({0, 2}, {1, 1}, 0);
  return fixture;
}

Fixture derived_fixture() {
  Fixture fixture;
  fixture.domain({1, 2, 3});
  fixture.domain({4, 5, 6});
  fixture.domain({7, 8, 9});
  fixture.equation({0, 1}, {1, 1}, 0);
  fixture.equation({1, 2}, {1, 1}, 0);
  return fixture;
}

Fixture conflict_fixture() {
  Fixture fixture = derived_fixture();
  fixture.equation({0, 2}, {1, 2}, 1);
  return fixture;
}

Fixture wide_conflict_fixture() {
  Fixture fixture;
  fixture.domain({1, 2, 3});
  fixture.domain({4, 5, 6});
  fixture.domain({7, 8, 9});
  fixture.domain({10, 11, 12});
  fixture.equation({0, 1, 2}, {1, 1, 1}, 0);
  fixture.equation({0, 1, 3}, {1, 1, 1}, 0);
  fixture.equation({0, 2, 3}, {1, 1, 1}, 0);
  fixture.equation({1, 2, 3}, {1, 1, 1}, 1);
  return fixture;
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

gf3_gaussian_certificate::Limits limits() {
  gf3_gaussian_certificate::Limits result;
  result.bridge = bridge_limits();
  result.maximum_work = 10000000;
  result.maximum_events = 100000;
  result.maximum_event_literals = 1000000;
  result.maximum_active_clauses = 100000;
  result.maximum_retained_source_references = 10000;
  result.maximum_event_width = 32;
  result.maximum_branch_domains = 32;
  result.maximum_consequences = 32;
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

bool assign(std::vector<int8_t> &assignment, int32_t literal) {
  const uint32_t atom = variable(literal);
  const int8_t value = literal > 0 ? 1 : -1;
  if (assignment[atom] == -value) return false;
  assignment[atom] = value;
  return true;
}

bool independently_rup(const std::vector<std::vector<int32_t>> &active,
                       const std::vector<int32_t> &candidate,
                       uint32_t maximum_variable) {
  std::vector<int8_t> assignment(uint64_t(maximum_variable) + 1, 0);
  for (int32_t literal : candidate)
    if (!assign(assignment, -literal)) return true;
  bool changed = true;
  while (changed) {
    changed = false;
    for (const auto &clause : active) {
      bool satisfied = false;
      uint32_t open = 0;
      int32_t last = 0;
      for (int32_t literal : clause) {
        const int8_t value = assignment[variable(literal)];
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
        const int8_t before = assignment[atom];
        if (!assign(assignment, last)) return true;
        changed |= !before;
      }
    }
  }
  return false;
}

void independently_check_events(
    const gf3_affine::Theory &theory,
    const gf3_gaussian_bridge::Plan &bridge,
    const gf3_gaussian_certificate::Plan &plan) {
  uint64_t next_event = 0;
  for (size_t index = 0; index < plan.consequences.size(); ++index) {
    const auto &consequence = plan.consequences[index];
    assert(consequence.first_event == next_event);
    assert(next_event <= plan.events.size());
    assert(consequence.event_count <= plan.events.size() - next_event);
    std::vector<std::vector<int32_t>> active;
    std::set<uint64_t> occurrences;
    const auto take = [&](const gf3_affine::SourceReference &reference) {
      assert(reference.occurrence);
      const auto found = std::find_if(
          theory.source.begin(), theory.source.end(), [&](const auto &premise) {
            return premise.occurrence == reference.occurrence;
          });
      assert(found != theory.source.end());
      assert(found->native_proof_id == reference.native_proof_id);
      if (occurrences.insert(reference.occurrence).second)
        active.push_back(found->literals);
    };
    for (const auto &source : consequence.sources) {
      assert(source.authentication_index < bridge.authentication.size());
      const auto &authentication =
          bridge.authentication[source.authentication_index];
      assert(authentication.source_id == source.combination.source_id);
      for (const auto &domain : authentication.domains) {
        take(domain.at_least_one);
        for (const auto &amo : domain.at_most_one) take(amo);
      }
      for (const auto &tuple : authentication.forbidden_tuples)
        take(tuple.premise);
    }
    for (uint64_t offset = 0; offset < consequence.event_count; ++offset) {
      const auto &event = plan.events[next_event + offset];
      assert(event.consequence == index);
      assert(independently_rup(active, event.clause,
                               theory.maximum_variable));
      active.push_back(event.clause);
    }
    next_event += consequence.event_count;
  }
  assert(next_event == plan.events.size());
}

bool literal_value(int32_t literal,
                   const std::vector<int8_t> &assignment) {
  const int8_t value = assignment[variable(literal)];
  return value && (literal > 0) == (value > 0);
}

bool clause_value(const std::vector<int32_t> &clause,
                  const std::vector<int8_t> &assignment) {
  return std::any_of(clause.begin(), clause.end(),
                     [&](int32_t literal) {
                       return literal_value(literal, assignment);
                     });
}

void independently_check_semantics(
    const gf3_affine::Theory &theory,
    const gf3_gaussian_certificate::Plan &plan,
    bool expect_source_model) {
  uint64_t assignments = 1;
  for (size_t i = 0; i < theory.domains.size(); ++i) assignments *= 3;
  bool found_model = false;
  for (uint64_t encoded = 0; encoded < assignments; ++encoded) {
    uint64_t rest = encoded;
    std::vector<uint8_t> values(theory.domains.size());
    std::vector<int8_t> booleans(uint64_t(theory.maximum_variable) + 1, 0);
    for (size_t domain = 0; domain < theory.domains.size(); ++domain) {
      values[domain] = static_cast<uint8_t>(rest % 3);
      rest /= 3;
      for (uint8_t value = 0; value < 3; ++value) {
        const int32_t selection = theory.domains[domain].values[value];
        assert(assign(booleans, value == values[domain] ? selection
                                                        : -selection));
      }
    }
    bool model = true;
    for (const auto &equation : theory.equations) {
      uint8_t sum = 0;
      for (size_t i = 0; i < equation.domains.size(); ++i)
        sum = static_cast<uint8_t>(
            (sum + equation.coefficients[i] *
                       values[equation.domains[i]]) % 3);
      model &= sum == equation.rhs;
    }
    if (!model) continue;
    found_model = true;
    for (const auto &consequence : plan.consequences)
      for (const auto &clause : consequence.clauses)
        assert(clause_value(clause, booleans));
  }
  assert(found_model == expect_source_model);
}

void test_units_are_pure_rup_and_semantic() {
  const auto theory = recognize(unit_fixture());
  const auto gaussian = bridge(theory);
  const auto result = gf3_gaussian_certificate::build(
      theory, gaussian, limits(), [](uint64_t) { return true; });
  assert(result.status == gf3_gaussian_certificate::Status::Complete);
  assert(result.plan.version == 2);
  assert(result.plan.consequences.size() == 3);
  assert(result.consumed.supported_outcomes == 3);
  assert(result.consumed.unsupported_outcomes == 0);
  assert(result.consumed.events == result.plan.events.size());
  assert(result.consumed.event_literals > 0);
  assert(result.consumed.retained_source_references > 0);
  uint64_t retained_sources = 0;
  for (size_t i = 0; i < result.plan.consequences.size(); ++i) {
    const auto &consequence = result.plan.consequences[i];
    assert(consequence.kind ==
           gf3_gaussian_certificate::ConsequenceKind::UnitEquation);
    assert(consequence.row.size() == 1 && consequence.rhs == 0);
    assert(consequence.sources.size() >= 2);
    retained_sources += consequence.sources.size();
    for (const auto &source : consequence.sources)
      assert(uint64_t{source.authentication_index} + 1 ==
             source.combination.source_id);
    assert(consequence.clauses.size() == 3);
    assert(consequence.first_event + consequence.event_count <=
           result.plan.events.size());
    for (uint64_t event = consequence.first_event;
         event < consequence.first_event + consequence.event_count; ++event)
      assert(result.plan.events[event].consequence == i);
    assert(consequence.clauses[0].size() == 1);
    assert(consequence.clauses[1].size() == 1);
    assert(consequence.clauses[2].size() == 1);
  }
  assert(retained_sources == result.consumed.retained_source_references);
  independently_check_events(theory, gaussian, result.plan);
  independently_check_semantics(theory, result.plan, true);
  const auto replayed = gf3_gaussian_certificate::validate(
      theory, gaussian, result.plan, limits(),
      [](uint64_t) { return true; });
  assert(replayed.status == gf3_gaussian_certificate::Status::Complete);
  assert(replayed.plan == result.plan);
}

void test_conflict_is_terminal_empty_rup() {
  const auto theory = recognize(conflict_fixture());
  const auto gaussian = bridge(theory);
  const auto result = gf3_gaussian_certificate::build(
      theory, gaussian, limits(), [](uint64_t) { return true; });
  assert(result.status == gf3_gaussian_certificate::Status::Complete);
  assert(result.plan.consequences.size() == 1);
  const auto &consequence = result.plan.consequences.front();
  assert(consequence.kind ==
         gf3_gaussian_certificate::ConsequenceKind::Conflict);
  assert(consequence.row.empty() && consequence.rhs != 0);
  assert(consequence.sources.size() == 3);
  assert(consequence.clauses == std::vector<std::vector<int32_t>>{{}});
  assert(!result.plan.events.empty());
  assert(result.plan.events.back().role ==
         gf3_gaussian_certificate::EventRole::Conflict);
  assert(result.plan.events.back().clause.empty());
  independently_check_events(theory, gaussian, result.plan);
  independently_check_semantics(theory, result.plan, false);
}

void test_nonunit_basis_declines_explicitly() {
  const auto theory = recognize(derived_fixture());
  const auto gaussian = bridge(theory);
  const auto result = gf3_gaussian_certificate::build(
      theory, gaussian, limits(), [](uint64_t) { return true; });
  assert(result.status ==
         gf3_gaussian_certificate::Status::NoSupportedOutcome);
  assert(result.consumed.supported_outcomes == 0);
  assert(result.consumed.unsupported_outcomes == 2);
  assert(result.plan.events.empty() && result.plan.consequences.empty());
}

void test_exact_boundaries_and_atomic_declines() {
  const auto theory = recognize(unit_fixture());
  const auto gaussian = bridge(theory);
  const auto baseline = gf3_gaussian_certificate::build(
      theory, gaussian, limits(), [](uint64_t) { return true; });
  assert(baseline.status == gf3_gaussian_certificate::Status::Complete);

  auto expect_limit = [&](gf3_gaussian_certificate::Limits bounded,
                          gf3_gaussian_certificate::LimitKind kind) {
    const auto result = gf3_gaussian_certificate::build(
        theory, gaussian, bounded, [](uint64_t) { return true; });
    assert(result.status ==
           gf3_gaussian_certificate::Status::LimitExceeded);
    assert(result.limit == kind);
    assert(result.plan.events.empty() && result.plan.consequences.empty());
  };

  auto bounded = limits();
  bounded.maximum_work = 1;
  expect_limit(bounded, gf3_gaussian_certificate::LimitKind::Work);
  bounded = limits();
  bounded.maximum_events = baseline.consumed.events - 1;
  expect_limit(bounded, gf3_gaussian_certificate::LimitKind::Events);
  bounded = limits();
  bounded.maximum_event_literals = baseline.consumed.event_literals - 1;
  expect_limit(bounded,
               gf3_gaussian_certificate::LimitKind::EventLiterals);
  bounded = limits();
  bounded.maximum_retained_source_references =
      baseline.consumed.retained_source_references - 1;
  expect_limit(
      bounded,
      gf3_gaussian_certificate::LimitKind::RetainedSourceReferences);
  {
    const auto conflict_theory = recognize(wide_conflict_fixture());
    const auto conflict_gaussian = bridge(conflict_theory);
    bounded = limits();
    bounded.maximum_event_width = 1;
    const auto too_wide = gf3_gaussian_certificate::build(
        conflict_theory, conflict_gaussian, bounded,
        [](uint64_t) { return true; });
    assert(too_wide.status ==
           gf3_gaussian_certificate::Status::LimitExceeded);
    assert(too_wide.limit ==
           gf3_gaussian_certificate::LimitKind::EventWidth);
    assert(too_wide.plan.events.empty());
  }
  bounded = limits();
  bounded.maximum_branch_domains = 1;
  expect_limit(bounded,
               gf3_gaussian_certificate::LimitKind::BranchDomains);
  bounded = limits();
  bounded.maximum_consequences = 1;
  expect_limit(bounded,
               gf3_gaussian_certificate::LimitKind::Consequences);
  bounded = limits();
  bounded.maximum_active_clauses = 1;
  expect_limit(bounded,
               gf3_gaussian_certificate::LimitKind::ActiveClauses);
  bounded = limits();
  bounded.bridge.maximum_work = 1;
  expect_limit(bounded, gf3_gaussian_certificate::LimitKind::Bridge);

  uint64_t bridge_work = 0;
  const auto checked = gf3_gaussian_bridge::validate(
      theory, gaussian, bridge_limits(), [&](uint64_t amount) {
        bridge_work += amount;
        return true;
      });
  assert(checked.status == gf3_gaussian_bridge::Status::Complete);

  const auto expect_exact = [&](gf3_gaussian_certificate::Limits exact) {
    const auto result = gf3_gaussian_certificate::build(
        theory, gaussian, exact, [](uint64_t) { return true; });
    assert(result.status == gf3_gaussian_certificate::Status::Complete);
    assert(result.plan == baseline.plan);
  };
  auto exact = limits();
  exact.maximum_work = baseline.consumed.work;
  expect_exact(exact);
  exact = limits();
  exact.maximum_events = baseline.consumed.events;
  expect_exact(exact);
  exact = limits();
  exact.maximum_event_literals = baseline.consumed.event_literals;
  expect_exact(exact);
  exact = limits();
  exact.maximum_retained_source_references =
      baseline.consumed.retained_source_references;
  expect_exact(exact);
  exact = limits();
  exact.maximum_active_clauses =
      theory.source.size() + baseline.consumed.events;
  expect_exact(exact);
  exact = limits();
  exact.maximum_event_width = baseline.consumed.maximum_event_width;
  expect_exact(exact);
  exact = limits();
  assert(theory.domains.size() <= UINT32_MAX);
  exact.maximum_branch_domains =
      static_cast<uint32_t>(theory.domains.size());
  expect_exact(exact);
  exact = limits();
  assert(baseline.consumed.supported_outcomes <= UINT32_MAX);
  exact.maximum_consequences =
      static_cast<uint32_t>(baseline.consumed.supported_outcomes);
  expect_exact(exact);
  exact = limits();
  exact.bridge.maximum_work = checked.consumed.work;
  expect_exact(exact);

  uint64_t charged = 0;
  const auto interrupted = gf3_gaussian_certificate::build(
      theory, gaussian, limits(), [&](uint64_t amount) {
        if (amount > bridge_work - std::min(bridge_work, charged))
          return false;
        charged += amount;
        return true;
      });
  assert(interrupted.status ==
         gf3_gaussian_certificate::Status::Interrupted);
  assert(interrupted.plan.events.empty());

  // Decline near the end of certificate construction, after bridge replay and
  // substantial recursive proof work, without publishing the partial plan.
  const uint64_t late_allowance =
      checked.consumed.work + baseline.consumed.work - 1;
  charged = 0;
  const auto late_interrupted = gf3_gaussian_certificate::build(
      theory, gaussian, limits(), [&](uint64_t amount) {
        if (amount > late_allowance - std::min(late_allowance, charged))
          return false;
        charged += amount;
        return true;
      });
  assert(late_interrupted.status ==
         gf3_gaussian_certificate::Status::Interrupted);
  assert(charged > checked.consumed.work);
  assert(late_interrupted.plan.events.empty() &&
         late_interrupted.plan.consequences.empty());

  auto invalid = limits();
  invalid.maximum_events = 0;
  const auto invalid_request = gf3_gaussian_certificate::build(
      theory, gaussian, invalid, [](uint64_t) { return true; });
  assert(invalid_request.status ==
         gf3_gaussian_certificate::Status::InvalidRequest);
}

void test_adversarial_identity_and_certificate_corruption() {
  const auto theory = recognize(unit_fixture());
  const auto gaussian = bridge(theory);
  const auto baseline = gf3_gaussian_certificate::build(
      theory, gaussian, limits(), [](uint64_t) { return true; });
  assert(baseline.status == gf3_gaussian_certificate::Status::Complete);

  auto corrupt_bridge = gaussian;
  ++corrupt_bridge.authentication[0]
        .forbidden_tuples[0]
        .premise.native_proof_id;
  auto result = gf3_gaussian_certificate::build(
      theory, corrupt_bridge, limits(), [](uint64_t) { return true; });
  assert(result.status == gf3_gaussian_certificate::Status::InvalidBridge);
  assert(result.plan.events.empty());

  auto corrupt_theory = theory;
  ++corrupt_theory.source[0].native_proof_id;
  result = gf3_gaussian_certificate::build(
      corrupt_theory, gaussian, limits(), [](uint64_t) { return true; });
  assert(result.status == gf3_gaussian_certificate::Status::InvalidBridge);

  auto corrupt_certificate = baseline.plan;
  assert(!corrupt_certificate.events.empty());
  corrupt_certificate.events[0].clause.push_back(INT32_MAX);
  result = gf3_gaussian_certificate::validate(
      theory, gaussian, corrupt_certificate, limits(),
      [](uint64_t) { return true; });
  assert(result.status ==
         gf3_gaussian_certificate::Status::InvalidCertificate);
  assert(result.plan.events.empty());

  corrupt_certificate = baseline.plan;
  ++corrupt_certificate.consequences[0]
        .sources[0]
        .authentication_index;
  result = gf3_gaussian_certificate::validate(
      theory, gaussian, corrupt_certificate, limits(),
      [](uint64_t) { return true; });
  assert(result.status ==
         gf3_gaussian_certificate::Status::InvalidCertificate);
}

} // namespace

int main() {
  test_units_are_pure_rup_and_semantic();
  test_conflict_is_terminal_empty_rup();
  test_nonunit_basis_declines_explicitly();
  test_exact_boundaries_and_atomic_declines();
  test_adversarial_identity_and_certificate_corruption();
  std::cout << "test_gf3_gaussian_certificate: all passed\n";
}
