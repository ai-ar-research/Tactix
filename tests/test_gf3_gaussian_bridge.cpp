#undef NDEBUG
#include "rules/semantic_extension/gf3_gaussian_bridge.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
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

gf3_affine::Theory recognize(const Fixture &fixture) {
  const gf3_affine::RecognitionBounds bounds{1000, 10000, 100, 100, 8,
                                              10000};
  gf3_affine::RecognitionResult result = gf3_affine::recognize(
      fixture.source, bounds, [](uint64_t) { return true; });
  assert(result.status == gf3_affine::RecognitionStatus::Recognized);
  return std::move(result.theory);
}

Fixture chain_fixture() {
  Fixture fixture;
  fixture.domain({1, 2, 3});
  fixture.domain({4, 5, 6});
  fixture.domain({7, 8, 9});
  fixture.equation({0, 1}, {1, 1}, 0);
  fixture.equation({1, 2}, {1, 1}, 0);
  return fixture;
}

Fixture conflict_fixture() {
  Fixture fixture;
  fixture.domain({1, 2, 3});
  fixture.domain({4, 5, 6});
  fixture.domain({7, 8, 9});
  fixture.equation({0, 1}, {1, 1}, 0);
  fixture.equation({1, 2}, {1, 1}, 0);
  // The first two imply A + 2C = 0.
  fixture.equation({0, 2}, {1, 2}, 1);
  return fixture;
}

gf3_gaussian_bridge::Limits limits() {
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

uint8_t evaluate(const std::vector<gf3_gaussian::Term> &row,
                 const std::vector<uint8_t> &assignment) {
  uint8_t result = 0;
  for (const auto &term : row)
    result = static_cast<uint8_t>(
        (result + term.coefficient * assignment[term.variable]) % 3);
  return result;
}

void semantic_equivalence(const gf3_gaussian_bridge::Plan &plan,
                          uint32_t domains) {
  uint64_t assignments = 1;
  for (uint32_t i = 0; i < domains; ++i) assignments *= 3;
  for (uint64_t encoded = 0; encoded < assignments; ++encoded) {
    uint64_t rest = encoded;
    std::vector<uint8_t> values(domains + 1, 0);
    for (uint32_t domain = 1; domain <= domains; ++domain) {
      values[domain] = static_cast<uint8_t>(rest % 3);
      rest /= 3;
    }
    bool source_model = true;
    for (const auto &row : plan.sources)
      source_model &= evaluate(row.terms, values) == row.rhs;
    bool output_model = true;
    for (const auto &outcome : plan.gaussian.outcomes) {
      const auto &row = plan.gaussian.derivations[outcome.derivation - 1];
      output_model &= evaluate(row.row, values) == row.rhs;
    }
    assert(source_model == output_model);
  }
}

void assert_invalid_plan(const gf3_affine::Theory &theory,
                         const gf3_gaussian_bridge::Plan &plan) {
  const auto result = gf3_gaussian_bridge::validate(
      theory, plan, limits(), [](uint64_t) { return true; });
  assert(result.status == gf3_gaussian_bridge::Status::InvalidPlan);
  assert(result.plan.sources.empty());
}

void test_bridge_and_exact_authentication() {
  const gf3_affine::Theory theory = recognize(chain_fixture());
  auto result = gf3_gaussian_bridge::build(
      theory, limits(), [](uint64_t) { return true; });
  assert(result.status == gf3_gaussian_bridge::Status::Complete);
  assert(result.plan.sources.size() == 2);
  assert(result.plan.authentication.size() == 2);
  assert(result.consumed.source_clauses == 24);
  assert(result.consumed.source_literals == 51);
  assert(result.consumed.terms == 4);
  assert(result.consumed.authentication_references == 28);

  const auto &first = result.plan.sources[0];
  assert(first.source_id == 1 && first.rhs == 0);
  assert((first.terms == std::vector<gf3_gaussian::Term>{{1, 1},
                                                         {2, 1}}));
  const auto &second = result.plan.sources[1];
  assert(second.source_id == 2 && second.rhs == 0);
  assert((second.terms == std::vector<gf3_gaussian::Term>{{2, 1},
                                                          {3, 1}}));

  for (size_t i = 0; i < result.plan.authentication.size(); ++i) {
    const auto &authentication = result.plan.authentication[i];
    assert(authentication.source_id == i + 1);
    assert(authentication.equation_index == i);
    assert(authentication.domains.size() == 2);
    assert(authentication.forbidden_tuples.size() == 6);
    for (const auto &domain : authentication.domains) {
      assert(domain.at_least_one.native_proof_id ==
             9000 + domain.at_least_one.occurrence);
      for (const auto &amo : domain.at_most_one)
        assert(amo.native_proof_id == 9000 + amo.occurrence);
    }
    for (const auto &tuple : authentication.forbidden_tuples) {
      assert(tuple.premise.occurrence);
      assert(tuple.premise.native_proof_id ==
             9000 + tuple.premise.occurrence);
    }
  }
  assert(result.plan.gaussian.outcomes.size() == 2);
  semantic_equivalence(result.plan, 3);

  auto replayed = gf3_gaussian_bridge::validate(
      theory, result.plan, limits(), [](uint64_t) { return true; });
  assert(replayed.status == gf3_gaussian_bridge::Status::Complete);
  assert(replayed.plan.gaussian.outcomes.size() == 2);
}

void test_validator_rejects_every_boundary_corruption() {
  const gf3_affine::Theory theory = recognize(chain_fixture());
  const auto built = gf3_gaussian_bridge::build(
      theory, limits(), [](uint64_t) { return true; });
  assert(built.status == gf3_gaussian_bridge::Status::Complete);

  auto corrupt = built.plan;
  corrupt.sources[0].terms[0].coefficient = 2;
  assert_invalid_plan(theory, corrupt);

  corrupt = built.plan;
  ++corrupt.authentication[0].domains[0].at_least_one.native_proof_id;
  assert_invalid_plan(theory, corrupt);

  corrupt = built.plan;
  auto &node = corrupt.gaussian.derivations.back();
  node.rhs = static_cast<uint8_t>((node.rhs + 1) % 3);
  assert_invalid_plan(theory, corrupt);

  corrupt = built.plan;
  ++corrupt.gaussian.statistics.fill_entries;
  assert_invalid_plan(theory, corrupt);

  corrupt = built.plan;
  auto provenance = std::find_if(
      corrupt.gaussian.derivations.begin(),
      corrupt.gaussian.derivations.end(), [](const auto &candidate) {
        return candidate.provenance.size() > 1;
      });
  assert(provenance != corrupt.gaussian.derivations.end());
  provenance->provenance[0].coefficient =
      provenance->provenance[0].coefficient == 1 ? 2 : 1;
  assert_invalid_plan(theory, corrupt);

  corrupt = built.plan;
  corrupt.gaussian.outcomes.pop_back();
  assert_invalid_plan(theory, corrupt);

  corrupt = built.plan;
  corrupt.gaussian.outcomes[0].kind =
      gf3_gaussian::RowOutcomeKind::ZeroRowConflict;
  assert_invalid_plan(theory, corrupt);

  corrupt = built.plan;
  auto derived = std::find_if(
      corrupt.gaussian.derivations.begin(),
      corrupt.gaussian.derivations.end(), [](const auto &candidate) {
        return candidate.kind == gf3_gaussian::DerivationKind::Add;
      });
  assert(derived != corrupt.gaussian.derivations.end());
  derived->first_parent = 0;
  assert_invalid_plan(theory, corrupt);
}

void test_inconsistent_system_requires_replayed_conflict() {
  const gf3_affine::Theory theory = recognize(conflict_fixture());
  const auto built = gf3_gaussian_bridge::build(
      theory, limits(), [](uint64_t) { return true; });
  assert(built.status == gf3_gaussian_bridge::Status::Complete);
  assert(built.plan.gaussian.outcomes.size() == 1);
  assert(built.plan.gaussian.outcomes[0].kind ==
         gf3_gaussian::RowOutcomeKind::ZeroRowConflict);
  semantic_equivalence(built.plan, 3);
  assert(gf3_gaussian_bridge::validate(
             theory, built.plan, limits(), [](uint64_t) { return true; })
             .status == gf3_gaussian_bridge::Status::Complete);

  auto missing = built.plan;
  const auto source = std::find_if(
      missing.gaussian.derivations.begin(),
      missing.gaussian.derivations.end(), [](const auto &node) {
        return node.kind == gf3_gaussian::DerivationKind::Source;
      });
  assert(source != missing.gaussian.derivations.end());
  const auto source_index =
      source - missing.gaussian.derivations.begin() + 1;
  missing.gaussian.outcomes = {{gf3_gaussian::RowOutcomeKind::Derived,
                                static_cast<uint32_t>(source_index)}};
  assert_invalid_plan(theory, missing);
}

void assert_invalid_theory(gf3_affine::Theory theory) {
  const auto result = gf3_gaussian_bridge::build(
      theory, limits(), [](uint64_t) { return true; });
  assert(result.status == gf3_gaussian_bridge::Status::InvalidTheory);
  assert(result.plan.sources.empty());
}

void test_theory_reauthentication() {
  const gf3_affine::Theory original = recognize(chain_fixture());
  auto theory = original;
  ++theory.maximum_variable;
  assert_invalid_theory(theory);

  theory = original;
  theory.equations[0].coefficients[0] = 2;
  assert_invalid_theory(theory);

  theory = original;
  theory.equations[0].forbidden_by_tuple[0] =
      theory.equations[0].forbidden_by_tuple[1];
  assert_invalid_theory(theory);

  theory = original;
  theory.domains[0].at_most_one[1] =
      theory.domains[0].at_most_one[2];
  assert_invalid_theory(theory);

  theory = original;
  theory.source.back().literals[0] = 1;
  assert_invalid_theory(theory);

  theory = original;
  theory.source.push_back({10000, {1, -4}, 77});
  assert_invalid_theory(theory);

  // Native proof identities are opaque but must cross the bridge exactly.
  theory = original;
  theory.source.back().native_proof_id = 424242;
  const auto rebuilt = gf3_gaussian_bridge::build(
      theory, limits(), [](uint64_t) { return true; });
  assert(rebuilt.status == gf3_gaussian_bridge::Status::Complete);
  bool found = false;
  for (const auto &authentication : rebuilt.plan.authentication)
    for (const auto &tuple : authentication.forbidden_tuples)
      found |= tuple.premise.native_proof_id == 424242;
  assert(found);
}

void assert_build_limit(const gf3_affine::Theory &theory,
                        gf3_gaussian_bridge::Limits limited,
                        gf3_gaussian_bridge::LimitKind expected) {
  const auto result = gf3_gaussian_bridge::build(
      theory, limited, [](uint64_t) { return true; });
  assert(result.status == gf3_gaussian_bridge::Status::LimitExceeded);
  assert(result.limit == expected);
  assert(result.plan.sources.empty());
}

void test_limits_and_interruption() {
  const gf3_affine::Theory theory = recognize(chain_fixture());
  auto limited = limits();
  limited.maximum_work = 0;
  assert_build_limit(theory, limited,
                     gf3_gaussian_bridge::LimitKind::Work);
  limited = limits();
  limited.maximum_source_clauses = 23;
  assert_build_limit(theory, limited,
                     gf3_gaussian_bridge::LimitKind::SourceClauses);
  limited = limits();
  limited.maximum_source_literals = 50;
  assert_build_limit(theory, limited,
                     gf3_gaussian_bridge::LimitKind::SourceLiterals);
  limited = limits();
  limited.maximum_domains = 2;
  assert_build_limit(theory, limited,
                     gf3_gaussian_bridge::LimitKind::Domains);
  limited = limits();
  limited.maximum_equations = 1;
  assert_build_limit(theory, limited,
                     gf3_gaussian_bridge::LimitKind::Equations);
  limited = limits();
  limited.maximum_terms = 3;
  assert_build_limit(theory, limited,
                     gf3_gaussian_bridge::LimitKind::Terms);
  limited = limits();
  limited.maximum_authentication_references = 27;
  assert_build_limit(
      theory, limited,
      gf3_gaussian_bridge::LimitKind::AuthenticationReferences);
  limited = limits();
  limited.maximum_references_per_equation = 13;
  assert_build_limit(theory, limited,
                     gf3_gaussian_bridge::LimitKind::ReferencesPerEquation);
  limited = limits();
  limited.gaussian.maximum_derivations = 2;
  assert_build_limit(theory, limited,
                     gf3_gaussian_bridge::LimitKind::Gaussian);

  uint32_t calls = 0;
  auto interrupted = gf3_gaussian_bridge::build(
      theory, limits(), [&](uint64_t) { return calls++ < 5; });
  assert(interrupted.status == gf3_gaussian_bridge::Status::Interrupted);
  assert(interrupted.plan.sources.empty());

  const auto built = gf3_gaussian_bridge::build(
      theory, limits(), [](uint64_t) { return true; });
  assert(built.status == gf3_gaussian_bridge::Status::Complete);
  assert(built.consumed.work > built.plan.gaussian.statistics.work);
  const uint64_t authentication_work =
      built.consumed.work - built.plan.gaussian.statistics.work;
  uint64_t accepted_work = 0;
  interrupted = gf3_gaussian_bridge::build(
      theory, limits(), [&](uint64_t amount) {
        if (amount > authentication_work - accepted_work) return false;
        accepted_work += amount;
        return true;
      });
  assert(accepted_work == authentication_work);
  assert(interrupted.status == gf3_gaussian_bridge::Status::Interrupted);
  assert(interrupted.plan.sources.empty());

  calls = 0;
  interrupted = gf3_gaussian_bridge::validate(
      theory, built.plan, limits(), [&](uint64_t) { return calls++ < 30; });
  assert(interrupted.status == gf3_gaussian_bridge::Status::Interrupted);
  assert(interrupted.plan.sources.empty());

  limited = limits();
  limited.gaussian.maximum_derivations = 1;
  const auto validation_limit = gf3_gaussian_bridge::validate(
      theory, built.plan, limited, [](uint64_t) { return true; });
  assert(validation_limit.status ==
         gf3_gaussian_bridge::Status::LimitExceeded);
  assert(validation_limit.limit ==
         gf3_gaussian_bridge::LimitKind::Gaussian);

  limited = limits();
  limited.gaussian.maximum_work = built.plan.gaussian.statistics.work - 1;
  assert(gf3_gaussian_bridge::validate(
             theory, built.plan, limited, [](uint64_t) { return true; })
             .status == gf3_gaussian_bridge::Status::LimitExceeded);
  limited = limits();
  limited.gaussian.maximum_fill_entries =
      built.plan.gaussian.statistics.fill_entries - 1;
  assert(gf3_gaussian_bridge::validate(
             theory, built.plan, limited, [](uint64_t) { return true; })
             .status == gf3_gaussian_bridge::Status::LimitExceeded);
  limited = limits();
  limited.gaussian.maximum_retained_plan_bytes =
      built.plan.gaussian.statistics.retained_plan_bytes - 1;
  assert(gf3_gaussian_bridge::validate(
             theory, built.plan, limited, [](uint64_t) { return true; })
             .status == gf3_gaussian_bridge::Status::LimitExceeded);
}

} // namespace

int main() {
  test_bridge_and_exact_authentication();
  test_validator_rejects_every_boundary_corruption();
  test_inconsistent_system_requires_replayed_conflict();
  test_theory_reauthentication();
  test_limits_and_interruption();
  std::cout << "gf3 gaussian bridge tests passed\n";
  return 0;
}
