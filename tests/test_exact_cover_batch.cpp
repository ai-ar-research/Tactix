#undef NDEBUG
#include "exact_cover_test_support.hpp"
#include "rules/semantic_extension/exact_cover_batch.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>

using namespace tactix;
using namespace tactix::exact_cover_semantic;

static std::vector<Premise> source_formula(bool missing_edge = false) {
  std::vector<Premise> result;
  for (auto &clause : exact_cover_clauses(missing_edge))
    result.push_back({result.size() + 1, std::move(clause)});
  return result;
}

static BatchPlanningRequest planning_request() {
  BatchPlanningRequest request;
  request.arity = 3;
  request.maximum_items = 100;
  request.maximum_source_clauses = 100;
  request.maximum_source_literals = 1000;
  request.maximum_premise_references = 1000;
  request.maximum_definitions = 10;
  request.maximum_additions = 100;
  request.maximum_literal_occurrences = 1000;
  request.maximum_extent_growth = 10;
  request.first_output_external = 10;
  request.maximum_external_extent = 100;
  request.action_id = 700;
  return request;
}

static BatchPlan make_plan(const std::vector<Premise> &source = source_formula()) {
  BatchPlan plan;
  uint64_t work = 0;
  auto charge = [&](uint64_t amount) { work += amount; return true; };
  assert(plan_batch(source, planning_request(), charge, plan));
  assert(work > source.size());
  return plan;
}

// Planning is deterministic, and each retained definition is an exact
// conservative definition output <-> OR(inputs) for every assignment.
static void deterministic_complete_plan() {
  const BatchPlan first = make_plan();
  assert(first == make_plan());
  assert(first.definitions().size() == 3);
  for (uint32_t slot = 0; slot < first.definitions().size(); ++slot) {
    const auto &definition = first.definitions()[slot];
    assert(definition.block == slot && definition.output == 10 + int32_t(slot));
    assert((definition.semantic == SemanticIdentity{700, slot}));
    assert(definition.duplicate_key.disjunction == definition.inputs);
    assert(definition.source_premises.size() == 9);
    for (uint32_t mask = 0; mask < 16; ++mask) {
      // Bit 3 is the output, bit i the truth of inputs[i].
      const auto value = [&](int32_t literal) {
        if (literal == definition.output || literal == -definition.output)
          return (literal > 0) == bool(mask & 8u);
        const auto found = std::find_if(
            definition.inputs.begin(), definition.inputs.end(),
            [&](int32_t input) { return variable(input) == variable(literal); });
        assert(found != definition.inputs.end());
        const bool input = mask & (1u << (found - definition.inputs.begin()));
        return literal == *found ? input : !input;
      };
      bool satisfied = true;
      for (const auto &clause : definition.formula)
        satisfied &= std::any_of(clause.begin(), clause.end(), value);
      assert(satisfied == (bool(mask & 8u) == bool(mask & 7u)));
    }
  }
}

static void no_partial_plan() {
  BatchPlan sentinel = make_plan();
  const BatchPlan expected = sentinel;
  auto bounded_request = planning_request();
  uint64_t left = 8;
  auto bounded = [&](uint64_t amount) {
    if (amount > left) return false;
    left -= amount;
    return true;
  };
  BatchPlanningStatus status = BatchPlanningStatus::Complete;
  assert(!plan_batch(source_formula(), bounded_request, bounded, sentinel,
                     &status));
  assert(status == BatchPlanningStatus::ResourceExhausted);
  assert(sentinel == expected);
  auto unlimited = [](uint64_t) { return true; };
  assert(!plan_batch(source_formula(true), bounded_request, unlimited, sentinel,
                     &status));
  assert(status == BatchPlanningStatus::NoStructure);
  assert(sentinel == expected);
  bounded_request.first_output_external = INT32_MAX - 1;
  bounded_request.maximum_external_extent = UINT32_MAX;
  assert(!plan_batch(source_formula(), bounded_request, unlimited, sentinel,
                     &status));
  assert(status == BatchPlanningStatus::ResourceExhausted);
  assert(sentinel == expected);
}

int main() {
  deterministic_complete_plan();
  no_partial_plan();
  std::puts("test_exact_cover_batch: passed");
}
