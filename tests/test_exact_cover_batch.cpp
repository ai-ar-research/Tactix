#undef NDEBUG
#include "frontend/parse.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/semantic_extension/exact_cover_batch.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <sstream>
#include <string>

using namespace tactix;
using namespace tactix::exact_cover_semantic;

static std::vector<Premise> source_formula(bool missing_edge = false) {
  std::vector<Premise> result;
  uint64_t occurrence = 0;
  for (int block = 0; block < 3; ++block)
    for (int first = 0; first < 3; ++first)
      for (int second = first + 1; second < 3; ++second) {
        if (missing_edge && block == 2 && first == 0 && second == 1) continue;
        result.push_back({++occurrence,
            {-(3 * block + first + 1), -(3 * block + second + 1)}});
      }
  for (int position = 0; position < 3; ++position) {
    std::vector<int32_t> clause;
    for (int block = 0; block < 3; ++block)
      clause.push_back(3 * block + position + 1);
    result.push_back({++occurrence, std::move(clause)});
  }
  for (int diagonal = 0; diagonal < 3; ++diagonal) {
    std::vector<int32_t> clause;
    for (int block = 0; block < 3; ++block)
      clause.push_back(3 * block + (block + diagonal) % 3 + 1);
    result.push_back({++occurrence, std::move(clause)});
  }
  return result;
}

static std::string dimacs(const std::vector<Premise> &source,
                          uint32_t variables = 9,
                          const std::vector<std::vector<int32_t>> &extra = {}) {
  std::ostringstream out;
  out << "p cnf " << variables << ' ' << source.size() + extra.size() << '\n';
  for (const Premise &premise : source) {
    for (int32_t literal : premise.literals) out << literal << ' ';
    out << "0\n";
  }
  for (const auto &clause : extra) {
    for (int32_t literal : clause) out << literal << ' ';
    out << "0\n";
  }
  return out.str();
}

static Kernel parse_source(const std::vector<Premise> &source) {
  const std::string text = dimacs(source);
  FILE *input = std::tmpfile();
  assert(input);
  assert(std::fwrite(text.data(), 1, text.size(), input) == text.size());
  std::rewind(input);
  Kernel kernel;
  assert(parse_dimacs(input, kernel) == nullptr);
  std::fclose(input);
  return kernel;
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
  std::rewind(records);
  std::string journal;
  char buffer[1024];
  while (const size_t count = std::fread(buffer, 1, sizeof buffer, records))
    journal.append(buffer, count);
  assert(journal.find("single-retained-source-image") != std::string::npos);
  assert(journal.find("block-occupancy") != std::string::npos);
  assert(journal.find("duplicate_key") != std::string::npos);
  assert(journal.find("omit-fresh-output") != std::string::npos);
  std::fclose(records);

  // Reusing a consumed plan declines before mutating either state or records.
  records = std::tmpfile();
  assert(records);
  const auto mapping = kernel.external_to_internal;
  const uint64_t id = kernel.next_clause_id;
  assert(publish_batch(kernel, plan, {700, 100000, records}).result ==
         ApplyResult::NoOp);
  assert(kernel.external_to_internal == mapping && kernel.next_clause_id == id);
  assert(std::ftell(records) == 0);
  std::fclose(records);
}

static void journal_limit_declines_before_mutation() {
  const BatchPlan plan = make_plan();
  Kernel kernel = parse_source(source_formula());
  FILE *records = std::tmpfile();
  assert(records);
  const auto mapping = kernel.external_to_internal;
  const uint64_t id = kernel.next_clause_id;
  const size_t arena = kernel.arena.size();
  const auto result = publish_batch(kernel, plan, {700, 1, records});
  assert(result.status == BatchPublicationStatus::ResourceExhausted);
  assert(result.result == ApplyResult::NoOp);
  assert(kernel.external_to_internal == mapping && kernel.next_clause_id == id);
  assert(kernel.arena.size() == arena && std::ftell(records) == 0);
  std::fclose(records);
}

static void publication_identity_preflight_is_pure() {
  const BatchPlan plan = make_plan();
  Kernel kernel = parse_source(source_formula());
  FILE *records = std::tmpfile();
  assert(records);
  const auto mapping = kernel.external_to_internal;
  const uint64_t id = kernel.next_clause_id;
  const size_t arena = kernel.arena.size();
  const auto result = publish_batch(kernel, plan, {701, 100000, records});
  assert(result.result == ApplyResult::NoOp);
  assert(result.status == BatchPublicationStatus::Invalid);
  assert(kernel.external_to_internal == mapping && kernel.next_clause_id == id);
  assert(kernel.arena.size() == arena && std::ftell(records) == 0);
  std::fclose(records);
}

static void stale_retained_source_declines() {
  const BatchPlan plan = make_plan();
  Kernel kernel = parse_source(source_formula());
  const Premise &lost = plan.source().front();
  assert(lost.literals.size() == 2);
  assert(kernel_delete_binary(kernel,
      kernel_internalize(kernel, lost.literals[0]),
      kernel_internalize(kernel, lost.literals[1])));
  FILE *records = std::tmpfile();
  assert(records);
  const auto mapping = kernel.external_to_internal;
  const uint64_t id = kernel.next_clause_id;
  const auto stale = publish_batch(kernel, plan, {700, 100000, records});
  assert(stale.result == ApplyResult::NoOp);
  assert(stale.status == BatchPublicationStatus::Stale);
  assert(kernel.external_to_internal == mapping && kernel.next_clause_id == id);
  assert(std::ftell(records) == 0);
  std::fclose(records);
}

static void changed_source_census_declines() {
  const BatchPlan plan = make_plan();
  auto extended = source_formula();
  extended.push_back({extended.size() + 1, {1, 2, 3}});
  Kernel kernel = parse_source(extended);
  FILE *records = std::tmpfile();
  assert(records);
  const auto mapping = kernel.external_to_internal;
  const uint64_t id = kernel.next_clause_id;
  const size_t arena = kernel.arena.size();
  const auto stale = publish_batch(kernel, plan, {700, 100000, records});
  assert(stale.result == ApplyResult::NoOp);
  assert(stale.status == BatchPublicationStatus::Stale);
  assert(kernel.external_to_internal == mapping && kernel.next_clause_id == id);
  assert(kernel.arena.size() == arena && std::ftell(records) == 0);
  std::fclose(records);
}

static void write_certificate(const char *cnf_path, const char *proof_path,
                              ProofFormat format) {
  const auto source = source_formula();
  const std::vector<std::vector<int32_t>> gadget = {
      {14, 15}, {-14, 15}, {14, -15}, {-14, -15}};
  const std::string checker_cnf = dimacs(source, 15, gadget);
  FILE *cnf = std::fopen(cnf_path, "w");
  assert(cnf);
  assert(std::fwrite(checker_cnf.data(), 1, checker_cnf.size(), cnf) ==
         checker_cnf.size());
  std::fclose(cnf);

  Kernel kernel = parse_source(source);
  kernel.next_clause_id = source.size() + gadget.size();
  Proof proof;
  assert(proof.open(proof_path, format));
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  kernel.proof = &proof;
  FILE *records = std::tmpfile();
  assert(records);
  const BatchPlan plan = make_plan(source);
  assert(publish_batch(kernel, plan, {700, 100000, records}).result ==
         ApplyResult::Ok);
  const int32_t unit = 14;
  assert(proof.add_external_rup(++kernel.next_clause_id, &unit, 1));
  assert(proof.add_external_rup(++kernel.next_clause_id, nullptr, 0));
  proof.flush();
  kernel.proof = nullptr;
  std::fclose(records);
}

int main(int argc, char **argv) {
  deterministic_complete_plan();
  no_partial_plan();
  atomic_publication_uses_retained_plan();
  journal_limit_declines_before_mutation();
  publication_identity_preflight_is_pure();
  stale_retained_source_declines();
  changed_source_census_declines();
  if (argc == 4) {
    write_certificate(argv[1], argv[2], ProofFormat::DsrAscii);
    write_certificate(argv[1], argv[3], ProofFormat::DratAscii);
  }
  std::puts("test_exact_cover_batch: passed");
}
