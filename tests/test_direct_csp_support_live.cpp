#include "unit_test.hpp"
#include "kernel_image.hpp"
#include "dimacs_text.hpp"
#include "rules/semantic_extension/direct_csp_support_live.hpp"

namespace {

std::vector<std::vector<int32_t>> clauses(bool unit_row = false) {
  std::vector<std::vector<int32_t>> result{
      {1, 2, 3}, {-1, -2}, {-1, -3}, {-2, -3},
      {4, 5, 6, 7}, {-4, -5}, {-4, -6}, {-4, -7}, {-5, -6}, {-5, -7},
      {-6, -7}};
  if (unit_row) {
    result.insert(result.end(), {{-1, -4}, {-1, -5}, {-1, -6}, {-1, -7},
                                 {-2, -4}, {-2, -6}, {-2, -7}, {-3, -5}});
  } else {
    result.insert(result.end(),
                  {{-1, -7}, {-2, -4}, {-2, -6}, {-2, -7}, {-3, -5}});
  }
  return result;
}

std::string dimacs(bool unit_row = false, bool checker_suffix = false) {
  auto formula = clauses(unit_row);
  if (checker_suffix)
    formula.insert(formula.end(), {{20, 21}, {-20, 21}, {20, -21}, {-20, -21}});
  return dimacs_text(checker_suffix ? 21 : 7, formula);
}

Kernel parse(bool unit_row = false) {
  return parse_dimacs_text(dimacs(unit_row), ParsedRoot::EmptyTrail);
}

DirectCspSupportLiveRequest request(FILE *records, uint32_t selected = 1) {
  DirectCspSupportLiveRequest result;
  result.left_values = {1, 2, 3};
  result.right_values = {4, 5, 6, 7};
  result.selected_left_value = selected;
  result.maximum_source_clauses = 100;
  result.maximum_source_literals = 1000;
  result.maximum_recognition_work = 100000;
  result.maximum_domain_values = 16;
  result.maximum_direct_clause_literals = 8;
  result.maximum_definition_clause_literals = 16;
  result.maximum_proof_work = 1000;
  result.maximum_proof_additions = 20;
  result.maximum_proof_literal_occurrences = 100;
  result.maximum_journal_bytes = 100000;
  result.maximum_extent_growth = 1;
  result.maximum_external_extent = 100;
  result.action_id = 71;
  result.semantic_id = 91;
  result.records = records;
  return result;
}

// A temporary records stream, closed on scope exit.
struct Records {
  FILE *file = std::tmpfile();
  Records() { assert(file); }
  ~Records() { std::fclose(file); }
  operator FILE *() const { return file; }
};

DirectCspSupportLiveResult decline_unchanged(
    Kernel &kernel, const DirectCspSupportLiveRequest &candidate) {
  const Image before = image(kernel);
  const long journal = candidate.records.tell();
  assert(journal >= 0);
  const auto result = direct_csp_support_live_apply(kernel, candidate);
  assert(result.result == ApplyResult::NoOp);
  assert(result.status != DirectCspSupportLiveStatus::Applied);
  assert(!result.emitted_additions && !result.published_clause_id &&
         result.published_support.empty() && result.semantic_variables.empty());
  assert(image(kernel) == before);
  assert(candidate.records.tell() == journal);
  return result;
}

TEST(capture_and_direct_publish) {
  Kernel kernel = parse();
  Records records;
  auto candidate = request(records);
  DirectCspSupportOwnerSnapshot owner;
  const auto capture =
      direct_csp_support_live_capture_owner(kernel, candidate, owner);
  assert(capture.status == DirectCspSupportLiveStatus::Applied &&
         capture.source_clauses == clauses().size() &&
         capture.source_literals == owner.source_literals);
  assert(owner.relation.left.values == std::vector<int32_t>({1, 2, 3}) &&
         owner.relation.right.values == std::vector<int32_t>({4, 5, 6, 7}));

  candidate.retained_owner = &owner;
  candidate.left_values.clear();
  candidate.right_values.clear();
  const auto result = direct_csp_support_live_apply(kernel, candidate);
  assert(result.status == DirectCspSupportLiveStatus::Applied &&
         result.result == ApplyResult::Ok && !result.propagation_pending);
  assert(result.published_support == std::vector<int32_t>({-2, 5}) &&
         result.emitted_additions == 1 && !result.extent_growth &&
         result.published_clause_id == kernel.next_clause_id);
  assert(result.semantic_variables.size() == 2 &&
         result.semantic_variables[0].literal == 2 &&
         result.semantic_variables[1].literal == 5);
  assert(std::ftell(records) > 0);
}

TEST(unit_support_records_pending_propagation) {
  Kernel kernel = parse(true);
  Records records;
  auto candidate = request(records, 0);
  const auto result = direct_csp_support_live_apply(kernel, candidate);
  assert(result.status == DirectCspSupportLiveStatus::Applied);
  assert(result.published_support == std::vector<int32_t>({-1}));
  assert(result.propagation_pending);
  assert(kernel.value(kernel_internalize(kernel, -1)) > 0);
}

TEST(stale_owner_and_clean_limits) {
  Kernel kernel = parse();
  Records records;
  auto candidate = request(records);
  DirectCspSupportOwnerSnapshot owner;
  assert(direct_csp_support_live_capture_owner(kernel, candidate, owner).status ==
         DirectCspSupportLiveStatus::Applied);
  const Lit new_clause[] = {kernel_internalize(kernel, -1),
                            kernel_internalize(kernel, -4)};
  kernel_add_binary_clause(kernel, new_clause[0], new_clause[1],
                           ++kernel.next_clause_id);
  candidate.retained_owner = &owner;
  candidate.left_values.clear();
  candidate.right_values.clear();
  const auto stale = decline_unchanged(kernel, candidate);
  assert(stale.status == DirectCspSupportLiveStatus::Stale);

  // Source caps and recognition work are covered by the action test.
  for (int mode = 3; mode < 8; ++mode) {
    Kernel bounded_kernel = parse();
    Records bounded_records;
    auto bounded = request(bounded_records, 0);
    bounded.maximum_direct_clause_literals = 2;
    bounded.allow_extension = true;
    switch (mode) {
    case 3: bounded.maximum_proof_work = 1; break;
    case 4: bounded.maximum_proof_additions = 1; break;
    case 5: bounded.maximum_proof_literal_occurrences = 1; break;
    case 6: bounded.maximum_journal_bytes = 1; break;
    case 7: bounded.maximum_external_extent = 7; break;
    }
    const auto result = decline_unchanged(bounded_kernel, bounded);
    assert(result.status == DirectCspSupportLiveStatus::ResourceExhausted);
  }
}

// A live duplicate scan is exact at its recognition-work boundary.
TEST(duplicate_scans_have_exact_work_boundaries) {
  for (uint32_t selected : {0u, 1u}) {
    Kernel kernel = parse();
    Records records;
    auto candidate = request(records, selected);
    DirectCspSupportOwnerSnapshot owner;
    assert(direct_csp_support_live_capture_owner(kernel, candidate, owner)
               .status == DirectCspSupportLiveStatus::Applied);
    candidate.retained_owner = &owner;
    candidate.left_values.clear();
    candidate.right_values.clear();
    assert(direct_csp_support_live_apply(kernel, candidate).status ==
           DirectCspSupportLiveStatus::Applied);

    Records duplicate_records;
    candidate.records = duplicate_records.file;
    ++candidate.action_id;
    const auto measured = decline_unchanged(kernel, candidate);
    assert(measured.status == DirectCspSupportLiveStatus::Duplicate &&
           measured.recognition_work);

    candidate.maximum_recognition_work = measured.recognition_work;
    const auto exact = decline_unchanged(kernel, candidate);
    assert(exact.status == DirectCspSupportLiveStatus::Duplicate &&
           exact.recognition_work == measured.recognition_work);

    --candidate.maximum_recognition_work;
    const auto short_by_one = decline_unchanged(kernel, candidate);
    assert(short_by_one.status ==
               DirectCspSupportLiveStatus::ResourceExhausted &&
           short_by_one.recognition_limit_exhausted);
  }
}

} // namespace
