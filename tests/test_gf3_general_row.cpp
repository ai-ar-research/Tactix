#undef NDEBUG
#include "gf3_formula_support.hpp"
#include "clause_oracle.hpp"
#include "rules/semantic_extension/gf3_general_row.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

using namespace tactix;

namespace {

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

bool rup(const std::vector<std::vector<int32_t>> &active,
         const std::vector<int32_t> &candidate, uint32_t maximum_variable) {
  return rup_implied(active, candidate, ClauseLiterals{}, maximum_variable);
}

gf3_general_row::Result build(const gf3_affine::Theory &theory,
                              const gf3_gaussian_bridge::Plan &bridge,
                              const gf3_general_row::Request &request,
                              const gf3_general_row::Limits &bounds = limits()) {
  return gf3_general_row::build(theory, bridge, request, bounds,
                                [](uint64_t) { return true; });
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
  prepared.theory = recognize(gf3_row_fixture());
  prepared.bridge = bridge(prepared.theory);
  const auto &outcomes = prepared.bridge.gaussian.outcomes;
  for (uint32_t outcome = 0; outcome < outcomes.size(); ++outcome)
    for (uint64_t tuple = 0; tuple < 9; ++tuple) {
      prepared.result = build(prepared.theory, prepared.bridge, {outcome, tuple});
      if (prepared.result.status == gf3_general_row::Status::Planned)
        return prepared;
    }
  assert(false && "expected one general-row request");
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

  std::vector<std::vector<int32_t>> active = gf3_row_fixture().clauses;
  assert(!rup(active, plan.consequence, prepared.theory.maximum_variable));
  for (const auto &event : plan.events) {
    assert(rup(active, event.clause, prepared.theory.maximum_variable));
    active.push_back(event.clause);
  }
  check_models(prepared.theory, plan.consequence);
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
  assert(build(prepared.theory, prepared.bridge, request).status ==
         gf3_general_row::Status::NoConsequence);
  request.outcome = static_cast<uint32_t>(prepared.bridge.gaussian.outcomes.size());
  assert(build(prepared.theory, prepared.bridge, request).status ==
         gf3_general_row::Status::InvalidRequest);

  gf3_affine::Theory duplicate_theory = prepared.theory;
  duplicate_theory.source.push_back(
      {duplicate_theory.source.back().occurrence + 1,
       prepared.result.plan.consequence, 123456});
  assert(build(duplicate_theory, prepared.bridge,
               prepared.result.plan.request).status ==
         gf3_general_row::Status::Duplicate);
}

void test_limits_interruptions_and_tampering_are_atomic() {
  Prepared prepared = prepare();
  auto bounded = limits();
  bounded.maximum_row_width = 1;
  auto result = build(prepared.theory, prepared.bridge,
                      prepared.result.plan.request, bounded);
  assert(result.status == gf3_general_row::Status::LimitExceeded);
  assert(result.limit == gf3_general_row::LimitKind::RowWidth);
  assert(result.plan.events.empty());

  bounded = limits();
  bounded.maximum_events = 1;
  result = build(prepared.theory, prepared.bridge,
                 prepared.result.plan.request, bounded);
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
  result = build(prepared.theory, tampered, prepared.result.plan.request);
  assert(result.status == gf3_general_row::Status::InvalidBridge);
  assert(result.plan.events.empty());
}

} // namespace

int main() {
  test_nonunit_row_clause_is_bounded_rup_and_model_preserving();
  test_predicate_declines_satisfying_nonrow_and_duplicate_requests();
  test_limits_interruptions_and_tampering_are_atomic();
  std::cout << "GF(3) general-row consequence tests passed\n";
}
