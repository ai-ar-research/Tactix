#undef NDEBUG
#include "exact_cover_test_support.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/semantic_extension/exact_cover_batch.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <optional>
#include <string>

using namespace tactix;
using namespace tactix::exact_cover_semantic;

static std::vector<Premise> source_formula(bool missing_edge = false) {
  std::vector<Premise> result;
  for (auto &clause : exact_cover_clauses(missing_edge))
    result.push_back({result.size() + 1, std::move(clause)});
  return result;
}

static Kernel parse_source(const std::vector<Premise> &source) {
  std::vector<std::vector<int32_t>> clauses;
  for (const Premise &premise : source) clauses.push_back(premise.literals);
  return parse_dimacs_text(dimacs_text(9, clauses));
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

static void deterministic_complete_plan() {
  const BatchPlan first = make_plan();
  const BatchPlan second = make_plan();
  assert(first == second);
  assert(first.arity() == 3 && first.source().size() == 15);
  assert(first.definitions().size() == 3 && first.extent_growth() == 3);
  assert(first.proof_additions() == 12 && first.literal_occurrences() == 30);
  for (uint32_t slot = 0; slot < first.definitions().size(); ++slot) {
    const auto &definition = first.definitions()[slot];
    assert(definition.block == slot);
    assert((definition.semantic == SemanticIdentity{700, slot}));
    assert(definition.output == 10 + int32_t(slot));
    assert(definition.kind == TheoryVariableKind::BlockOccupancy);
    assert(definition.projection == ModelProjection::OmitFreshOutput);
    assert(definition.lifetime == VariableLifetime::OrdinaryLiveClauses);
    assert(definition.formula.size() == 4);
    assert(definition.formula.back().front() == -definition.output);
    assert(definition.duplicate_key.disjunction == definition.inputs);
    assert(definition.proof_additions == 4);
    assert(definition.literal_occurrences == 10);
    assert(definition.extent_growth == 1);
    assert(definition.source_premises.size() == 9);
    // The retained concrete formula is an exact conservative definition for
    // every assignment, not a phase suggestion or one-way implication.
    for (uint32_t mask = 0; mask < 8; ++mask)
      for (bool output : {false, true}) {
        bool satisfied = true;
        for (const auto &clause : definition.formula) {
          bool clause_satisfied = false;
          for (int32_t literal : clause) {
            bool literal_value = false;
            if (literal == definition.output)
              literal_value = output;
            else if (literal == -definition.output)
              literal_value = !output;
            else {
              const auto found = std::find_if(
                  definition.inputs.begin(), definition.inputs.end(),
                  [&](int32_t input) {
                    return variable(input) == variable(literal);
                  });
              assert(found != definition.inputs.end());
              const bool input_value =
                  mask & (1u << (found - definition.inputs.begin()));
              assert(literal == *found || literal == -*found);
              literal_value = literal == *found ? input_value : !input_value;
            }
            clause_satisfied |= literal_value;
          }
          satisfied &= clause_satisfied;
        }
        assert(satisfied == (output == (mask != 0)));
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
  bounded_request.maximum_additions = 11;
  assert(!plan_batch(source_formula(), bounded_request, unlimited, sentinel,
                     &status));
  assert(status == BatchPlanningStatus::ResourceExhausted);
  assert(sentinel == expected);
  bounded_request = planning_request();
  bounded_request.maximum_premise_references = 26;
  assert(!plan_batch(source_formula(), bounded_request, unlimited, sentinel,
                     &status));
  assert(status == BatchPlanningStatus::ResourceExhausted);
  assert(sentinel == expected);

  bounded_request = planning_request();
  bounded_request.first_output_external = INT32_MAX - 1;
  bounded_request.maximum_external_extent = UINT32_MAX;
  assert(!plan_batch(source_formula(), bounded_request, unlimited, sentinel,
                     &status));
  assert(status == BatchPlanningStatus::ResourceExhausted);
  assert(sentinel == expected);
}

// Publication declines (with `expected`, when given) and changes nothing.
static void declines(Kernel &kernel, const BatchPlan &plan, uint64_t action,
                     uint64_t journal_bytes,
                     std::optional<BatchPublicationStatus> expected) {
  FILE *records = std::tmpfile();
  assert(records);
  const ExactCoverUnchanged unchanged{kernel, records};
  const auto result = publish_batch(kernel, plan, {action, journal_bytes, records});
  assert(result.result == ApplyResult::NoOp);
  assert(!expected || result.status == *expected);
  unchanged.check();
  std::fclose(records);
}

static void atomic_publication_uses_retained_plan() {
  const BatchPlan plan = make_plan();
  Kernel kernel = parse_source(source_formula());
  const uint32_t original_outputs = kernel_num_output_vars(kernel);
  FILE *records = std::tmpfile();
  assert(records);
  const auto published = publish_batch(kernel, plan, {700, 100000, records});
  assert(published.result == ApplyResult::Ok && published.definitions == 3);
  assert(published.status == BatchPublicationStatus::Published);
  assert(published.journal_bytes > 0);
  assert(published.additions == 12 && published.literal_occurrences == 30);
  assert(published.extent_growth == 3 && kernel.nvars == 12);
  assert(kernel_num_output_vars(kernel) == original_outputs);
  // Definitions two and three were published from the retained plan after the
  // first definition had already made strict live re-recognition impossible.
  for (int output : {10, 11, 12})
    assert(kernel_internalize(kernel, output) != INVALID_LIT);
  kernel_assign_decision(kernel, kernel_internalize(kernel, 1));
  assert(propagate_service(kernel) == ApplyResult::Ok);
  assert(kernel.value(kernel_internalize(kernel, 10)) > 0);
  const std::string journal = stream_contents(records);
  assert(journal.find("single-retained-source-image") != std::string::npos);
  assert(journal.find("block-occupancy") != std::string::npos);
  assert(journal.find("duplicate_key") != std::string::npos);
  assert(journal.find("omit-fresh-output") != std::string::npos);
  std::fclose(records);

  // Reusing a consumed plan declines before mutating either state or records.
  declines(kernel, plan, 700, 100000, std::nullopt);
}

// Each stale, changed or out-of-bound publication declines before mutation.
static void publication_preflight_is_pure() {
  const BatchPlan plan = make_plan();
  {
    Kernel kernel = parse_source(source_formula());
    declines(kernel, plan, 700, 1, BatchPublicationStatus::ResourceExhausted);
    declines(kernel, plan, 701, 100000, BatchPublicationStatus::Invalid);
  }
  {
    Kernel kernel = parse_source(source_formula());
    const Premise &lost = plan.source().front();
    assert(lost.literals.size() == 2);
    assert(kernel_delete_binary(kernel,
        kernel_internalize(kernel, lost.literals[0]),
        kernel_internalize(kernel, lost.literals[1])));
    declines(kernel, plan, 700, 100000, BatchPublicationStatus::Stale);
  }
  {
    auto extended = source_formula();
    extended.push_back({extended.size() + 1, {1, 2, 3}});
    Kernel kernel = parse_source(extended);
    declines(kernel, plan, 700, 100000, BatchPublicationStatus::Stale);
  }
}

int main() {
  deterministic_complete_plan();
  no_partial_plan();
  atomic_publication_uses_retained_plan();
  publication_preflight_is_pure();
  std::puts("test_exact_cover_batch: passed");
}
