#undef NDEBUG
#include "gf3_formula_support.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include <iostream>
#include <vector>

using namespace tactix;

namespace {

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
      theory, plan, bridge_limits(), [](uint64_t) { return true; });
  assert(result.status == gf3_gaussian_bridge::Status::InvalidPlan);
  assert(result.plan.sources.empty());
}

void test_bridge_and_exact_authentication() {
  const gf3_affine::Theory theory = recognize(gf3_chain());
  auto result = gf3_gaussian_bridge::build(
      theory, bridge_limits(), [](uint64_t) { return true; });
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
      theory, result.plan, bridge_limits(), [](uint64_t) { return true; });
  assert(replayed.status == gf3_gaussian_bridge::Status::Complete);
  assert(replayed.plan.gaussian.outcomes.size() == 2);
}

void test_validator_rejects_every_boundary_corruption() {
  const gf3_affine::Theory theory = recognize(gf3_chain());
  const auto built = gf3_gaussian_bridge::build(
      theory, bridge_limits(), [](uint64_t) { return true; });
  assert(built.status == gf3_gaussian_bridge::Status::Complete);

  using Plan = gf3_gaussian_bridge::Plan;
  // One corruption per validated component: rows, authentication, derivation
  // replay, accounting, outcomes and parent references.
  const std::vector<void (*)(Plan &)> corruptions = {
      [](Plan &p) { p.sources[0].terms[0].coefficient = 2; },
      [](Plan &p) { ++p.authentication[0].domains[0].at_least_one.native_proof_id; },
      [](Plan &p) {
        auto &node = p.gaussian.derivations.back();
        node.rhs = static_cast<uint8_t>((node.rhs + 1) % 3);
      },
      [](Plan &p) { ++p.gaussian.statistics.fill_entries; },
      [](Plan &p) {
        for (auto &node : p.gaussian.derivations)
          if (node.provenance.size() > 1) {
            auto &coefficient = node.provenance[0].coefficient;
            coefficient = coefficient == 1 ? 2 : 1;
            return;
          }
        assert(false);
      },
      [](Plan &p) { p.gaussian.outcomes.pop_back(); },
      [](Plan &p) {
        p.gaussian.outcomes[0].kind = gf3_gaussian::RowOutcomeKind::ZeroRowConflict;
      },
      [](Plan &p) {
        for (auto &node : p.gaussian.derivations)
          if (node.kind == gf3_gaussian::DerivationKind::Add) {
            node.first_parent = 0;
            return;
          }
        assert(false);
      }};
  for (const auto corrupt : corruptions) {
    Plan plan = built.plan;
    corrupt(plan);
    assert_invalid_plan(theory, plan);
  }
}

void test_inconsistent_system_requires_replayed_conflict() {
  const gf3_affine::Theory theory = recognize(gf3_conflict());
  const auto built = gf3_gaussian_bridge::build(
      theory, bridge_limits(), [](uint64_t) { return true; });
  assert(built.status == gf3_gaussian_bridge::Status::Complete);
  assert(built.plan.gaussian.outcomes.size() == 1);
  assert(built.plan.gaussian.outcomes[0].kind ==
         gf3_gaussian::RowOutcomeKind::ZeroRowConflict);
  semantic_equivalence(built.plan, 3);
  assert(gf3_gaussian_bridge::validate(
             theory, built.plan, bridge_limits(), [](uint64_t) { return true; })
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

void test_theory_reauthentication() {
  using Theory = gf3_affine::Theory;
  const Theory original = recognize(gf3_chain());
  const std::vector<void (*)(Theory &)> corruptions = {
      [](Theory &t) { ++t.maximum_variable; },
      [](Theory &t) { t.equations[0].coefficients[0] = 2; },
      [](Theory &t) {
        t.equations[0].forbidden_by_tuple[0] = t.equations[0].forbidden_by_tuple[1];
      },
      [](Theory &t) { t.domains[0].at_most_one[1] = t.domains[0].at_most_one[2]; },
      [](Theory &t) { t.source.back().literals[0] = 1; },
      [](Theory &t) { t.source.push_back({10000, {1, -4}, 77}); }};
  for (const auto corrupt : corruptions) {
    Theory theory = original;
    corrupt(theory);
    const auto result = gf3_gaussian_bridge::build(
        theory, bridge_limits(), [](uint64_t) { return true; });
    assert(result.status == gf3_gaussian_bridge::Status::InvalidTheory &&
           result.plan.sources.empty());
  }

  // Native proof identities are opaque but must cross the bridge exactly.
  Theory theory = original;
  theory.source.back().native_proof_id = 424242;
  const auto rebuilt = gf3_gaussian_bridge::build(
      theory, bridge_limits(), [](uint64_t) { return true; });
  assert(rebuilt.status == gf3_gaussian_bridge::Status::Complete);
  bool found = false;
  for (const auto &authentication : rebuilt.plan.authentication)
    for (const auto &tuple : authentication.forbidden_tuples)
      found |= tuple.premise.native_proof_id == 424242;
  assert(found);
}

void test_limits_and_interruption() {
  using Kind = gf3_gaussian_bridge::LimitKind;
  using Limits = gf3_gaussian_bridge::Limits;
  const gf3_affine::Theory theory = recognize(gf3_chain());
  // Each bound one below what the chain needs.
  const std::vector<std::pair<void (*)(Limits &), Kind>> bounds = {
      {[](Limits &l) { l.maximum_work = 0; }, Kind::Work},
      {[](Limits &l) { l.maximum_source_clauses = 23; }, Kind::SourceClauses},
      {[](Limits &l) { l.maximum_source_literals = 50; }, Kind::SourceLiterals},
      {[](Limits &l) { l.maximum_domains = 2; }, Kind::Domains},
      {[](Limits &l) { l.maximum_equations = 1; }, Kind::Equations},
      {[](Limits &l) { l.maximum_terms = 3; }, Kind::Terms},
      {[](Limits &l) { l.maximum_authentication_references = 27; },
       Kind::AuthenticationReferences},
      {[](Limits &l) { l.maximum_references_per_equation = 13; },
       Kind::ReferencesPerEquation},
      {[](Limits &l) { l.gaussian.maximum_derivations = 2; }, Kind::Gaussian},
  };
  for (const auto &[change, expected] : bounds) {
    Limits limited = bridge_limits();
    change(limited);
    const auto result = gf3_gaussian_bridge::build(
        theory, limited, [](uint64_t) { return true; });
    assert(result.status == gf3_gaussian_bridge::Status::LimitExceeded);
    assert(result.limit == expected);
    assert(result.plan.sources.empty());
  }

  uint32_t calls = 0;
  auto interrupted = gf3_gaussian_bridge::build(
      theory, bridge_limits(), [&](uint64_t) { return calls++ < 5; });
  assert(interrupted.status == gf3_gaussian_bridge::Status::Interrupted);
  assert(interrupted.plan.sources.empty());

  const auto built = gf3_gaussian_bridge::build(
      theory, bridge_limits(), [](uint64_t) { return true; });
  assert(built.status == gf3_gaussian_bridge::Status::Complete);
  assert(built.consumed.work > built.plan.gaussian.statistics.work);
  const uint64_t authentication_work =
      built.consumed.work - built.plan.gaussian.statistics.work;
  uint64_t accepted_work = 0;
  interrupted = gf3_gaussian_bridge::build(
      theory, bridge_limits(), [&](uint64_t amount) {
        if (amount > authentication_work - accepted_work) return false;
        accepted_work += amount;
        return true;
      });
  assert(accepted_work == authentication_work);
  assert(interrupted.status == gf3_gaussian_bridge::Status::Interrupted);
  assert(interrupted.plan.sources.empty());

  calls = 0;
  interrupted = gf3_gaussian_bridge::validate(
      theory, built.plan, bridge_limits(), [&](uint64_t) { return calls++ < 30; });
  assert(interrupted.status == gf3_gaussian_bridge::Status::Interrupted);
  assert(interrupted.plan.sources.empty());

  // Validation replays the Gaussian plan under the same bounds.
  // maximum_derivations stops replay early; the other three reach the
  // recomputed-statistics comparison.
  const auto &statistics = built.plan.gaussian.statistics;
  const std::vector<std::function<void(Limits &)>> replay_bounds = {
      [](Limits &l) { l.gaussian.maximum_derivations = 1; },
      [&](Limits &l) { l.gaussian.maximum_work = statistics.work - 1; },
      [&](Limits &l) {
        l.gaussian.maximum_fill_entries = statistics.fill_entries - 1;
      },
      [&](Limits &l) {
        l.gaussian.maximum_retained_plan_bytes =
            statistics.retained_plan_bytes - 1;
      },
  };
  for (const auto &change : replay_bounds) {
    Limits limited = bridge_limits();
    change(limited);
    const auto result = gf3_gaussian_bridge::validate(
        theory, built.plan, limited, [](uint64_t) { return true; });
    assert(result.status == gf3_gaussian_bridge::Status::LimitExceeded &&
           result.limit == Kind::Gaussian);
  }
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
