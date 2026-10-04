#undef NDEBUG
#include "rules/semantic_extension/direct_csp_support_live.hpp"

#include "frontend/parse.hpp"
#include "proof/proof.hpp"

#include <cassert>
#include <csignal>
#include <cstdio>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace tactix;

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
  std::ostringstream output;
  output << "p cnf " << (checker_suffix ? 21 : 7) << ' '
         << formula.size() + (checker_suffix ? 4 : 0) << '\n';
  for (const auto &clause : formula) {
    for (int32_t literal : clause) output << literal << ' ';
    output << "0\n";
  }
  if (checker_suffix)
    output << "20 21 0\n-20 21 0\n20 -21 0\n-20 -21 0\n";
  return output.str();
}

Kernel parse(bool unit_row = false) {
  const std::string text = dimacs(unit_row);
  FILE *input = std::tmpfile();
  assert(input);
  assert(std::fwrite(text.data(), 1, text.size(), input) == text.size());
  std::rewind(input);
  Kernel kernel;
  assert(parse_dimacs(input, kernel) == nullptr);
  std::fclose(input);
  assert(kernel.trail.empty() && kernel_propagation_fixpoint(kernel));
  return kernel;
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

struct Image {
  uint32_t nvars = 0;
  uint64_t next_id = 0;
  size_t arena = 0;
  size_t extent = 0;
  size_t trail = 0;
  uint64_t generation = 0;
  std::vector<Var> internal_to_external;
  std::vector<Var> external_to_internal;
  std::vector<size_t> watches;
};

Image image(const Kernel &kernel) {
  Image result;
  result.nvars = kernel.nvars;
  result.next_id = kernel.next_clause_id;
  result.arena = kernel.arena.size();
  result.extent = kernel.external_to_internal.size();
  result.trail = kernel.trail.size();
  result.generation = kernel.arena_generation;
  result.internal_to_external = kernel.internal_to_external;
  result.external_to_internal = kernel.external_to_internal;
  for (Lit literal = 0; literal < 2 * kernel.nvars; ++literal)
    result.watches.push_back(kernel.watches.size(literal));
  return result;
}

bool operator==(const Image &a, const Image &b) {
  return a.nvars == b.nvars && a.next_id == b.next_id &&
         a.arena == b.arena && a.extent == b.extent &&
         a.trail == b.trail && a.generation == b.generation &&
         a.internal_to_external == b.internal_to_external &&
         a.external_to_internal == b.external_to_internal &&
         a.watches == b.watches;
}

DirectCspSupportLiveResult decline_unchanged(
    Kernel &kernel, const DirectCspSupportLiveRequest &candidate) {
  const Image before = image(kernel);
  const long journal = std::ftell(candidate.records);
  assert(journal >= 0);
  const auto result = direct_csp_support_live_apply(kernel, candidate);
  assert(result.result == ApplyResult::NoOp);
  assert(result.status != DirectCspSupportLiveStatus::Applied);
  assert(!result.emitted_additions && !result.published_clause_id &&
         result.published_support.empty() && result.semantic_variables.empty());
  assert(image(kernel) == before);
  assert(std::ftell(candidate.records) == journal);
  return result;
}

void capture_and_direct_publish() {
  Kernel kernel = parse();
  FILE *records = std::tmpfile();
  assert(records);
  auto candidate = request(records);
  DirectCspSupportOwnerSnapshot owner;
  const auto capture =
      direct_csp_support_live_capture_owner(kernel, candidate, owner);
  assert(capture.status == DirectCspSupportLiveStatus::Applied);
  assert(capture.source_clauses == clauses().size());
  assert(capture.source_literals == owner.source_literals);
  assert(owner.relation.left.values == std::vector<int32_t>({1, 2, 3}));
  assert(owner.relation.right.values ==
         std::vector<int32_t>({4, 5, 6, 7}));

  candidate.retained_owner = &owner;
  candidate.left_values.clear();
  candidate.right_values.clear();
  const auto result = direct_csp_support_live_apply(kernel, candidate);
  assert(result.status == DirectCspSupportLiveStatus::Applied);
  assert(result.result == ApplyResult::Ok);
  assert(result.published_support == std::vector<int32_t>({-2, 5}));
  assert(result.emitted_additions == 1 && !result.extent_growth);
  assert(result.published_clause_id == kernel.next_clause_id);
  assert(result.semantic_variables.size() == 2);
  assert(result.semantic_variables[0].literal == 2);
  assert(result.semantic_variables[0].role ==
         DirectCspSemanticRole::SelectedDomainValue);
  assert(result.semantic_variables[1].literal == 5);
  assert(!result.propagation_pending);
  assert(std::ftell(records) > 0);

  FILE *repeat_records = std::tmpfile();
  assert(repeat_records);
  candidate.records = repeat_records;
  candidate.action_id++;
  const auto repeated = decline_unchanged(kernel, candidate);
  assert(repeated.status == DirectCspSupportLiveStatus::Duplicate);
  std::fclose(repeat_records);
  std::fclose(records);
}

void extension_and_retained_owner_reuse() {
  Kernel kernel = parse();
  FILE *capture_records = std::tmpfile();
  assert(capture_records);
  auto initial = request(capture_records, 0);
  initial.maximum_direct_clause_literals = 2;
  initial.allow_extension = true;
  DirectCspSupportOwnerSnapshot owner;
  assert(direct_csp_support_live_capture_owner(kernel, initial, owner).status ==
         DirectCspSupportLiveStatus::Applied);

  initial.retained_owner = &owner;
  initial.left_values.clear();
  initial.right_values.clear();
  const auto first = direct_csp_support_live_apply(kernel, initial);
  assert(first.status == DirectCspSupportLiveStatus::Applied);
  assert(first.extent_growth == 1 && kernel.nvars == 8);
  assert(first.emitted_additions == 5);
  assert(first.published_support == std::vector<int32_t>({-1, 8}));
  assert(first.semantic_variables.back().literal == 8);
  assert(first.semantic_variables.back().fresh_extension);
  assert(first.semantic_variables.back().definition_inputs ==
         std::vector<int32_t>({4, 5, 6}));
  assert(kernel_num_output_vars(kernel) == 7);

  FILE *second_records = std::tmpfile();
  assert(second_records);
  auto second = request(second_records, 1);
  second.retained_owner = &owner;
  second.left_values.clear();
  second.right_values.clear();
  const auto next = direct_csp_support_live_apply(kernel, second);
  assert(next.status == DirectCspSupportLiveStatus::Applied);
  assert(next.published_support == std::vector<int32_t>({-2, 5}));
  assert(kernel.nvars == 8);
  std::fclose(second_records);
  std::fclose(capture_records);
}

void unit_support_records_pending_propagation() {
  Kernel kernel = parse(true);
  FILE *records = std::tmpfile();
  assert(records);
  auto candidate = request(records, 0);
  const auto result = direct_csp_support_live_apply(kernel, candidate);
  assert(result.status == DirectCspSupportLiveStatus::Applied);
  assert(result.published_support == std::vector<int32_t>({-1}));
  assert(result.propagation_pending);
  assert(kernel.value(kernel_internalize(kernel, -1)) > 0);
  std::fclose(records);
}

void stale_owner_and_clean_limits() {
  Kernel kernel = parse();
  FILE *records = std::tmpfile();
  assert(records);
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
  std::fclose(records);

  for (int mode = 0; mode < 8; ++mode) {
    Kernel bounded_kernel = parse();
    FILE *bounded_records = std::tmpfile();
    assert(bounded_records);
    auto bounded = request(bounded_records, 0);
    bounded.maximum_direct_clause_literals = 2;
    bounded.allow_extension = true;
    switch (mode) {
    case 0: bounded.maximum_recognition_work = 1; break;
    case 1: bounded.maximum_source_clauses = 1; break;
    case 2: bounded.maximum_source_literals = 1; break;
    case 3: bounded.maximum_proof_work = 1; break;
    case 4: bounded.maximum_proof_additions = 1; break;
    case 5: bounded.maximum_proof_literal_occurrences = 1; break;
    case 6: bounded.maximum_journal_bytes = 1; break;
    case 7: bounded.maximum_external_extent = 7; break;
    }
    const auto result = decline_unchanged(bounded_kernel, bounded);
    assert(result.status == DirectCspSupportLiveStatus::ResourceExhausted ||
           result.status == DirectCspSupportLiveStatus::NoStructure);
    std::fclose(bounded_records);
  }
}

void interruption_and_capture_output_stability() {
  Kernel kernel = parse();
  FILE *records = std::tmpfile();
  assert(records);
  auto candidate = request(records);
  volatile sig_atomic_t stop = 1;
  kernel.stop = &stop;
  const auto interrupted = decline_unchanged(kernel, candidate);
  assert(interrupted.status == DirectCspSupportLiveStatus::Interrupted);

  DirectCspSupportOwnerSnapshot output;
  output.external_extent = 99;
  const auto captured =
      direct_csp_support_live_capture_owner(kernel, candidate, output);
  assert(captured.status == DirectCspSupportLiveStatus::Interrupted);
  assert(output.external_extent == 99 && output.source.empty());
  std::fclose(records);
}

void unsupported_proofs_decline_before_mutation() {
  for (const auto &[format, path] :
       {std::pair{ProofFormat::Lrat, "/tmp/direct-csp-support-decline.lrat"},
        std::pair{ProofFormat::Pbp, "/tmp/direct-csp-support-decline.pbp"}}) {
    Kernel kernel = parse();
    Proof proof;
    std::remove(path);
    if (format == ProofFormat::Pbp) {
      assert(proof.open(path, ProofFormat::Automatic));
      proof.finish_input_transaction();
      assert(proof.begin_pb());
    } else {
      assert(proof.open(path, format));
    }
    proof.bind_variable_map(&kernel.internal_to_external);
    proof.bind_external_variable_map(&kernel.external_to_internal);
    kernel.proof = &proof;
    FILE *records = std::tmpfile();
    assert(records);
    const auto result = decline_unchanged(kernel, request(records));
    assert(result.status == DirectCspSupportLiveStatus::UnsupportedProof);
    kernel.proof = nullptr;
    std::fclose(records);
  }
}

void write_certificate(const char *cnf_path, const char *proof_path,
                       ProofFormat format) {
  const std::string checker = dimacs(false, true);
  FILE *cnf = std::fopen(cnf_path, "w");
  assert(cnf);
  assert(std::fwrite(checker.data(), 1, checker.size(), cnf) == checker.size());
  std::fclose(cnf);

  Kernel kernel = parse();
  kernel.next_clause_id = clauses().size() + 4;
  Proof proof;
  assert(proof.open(proof_path, format));
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  kernel.proof = &proof;
  FILE *records = std::tmpfile();
  assert(records);
  auto candidate = request(records, 0);
  candidate.maximum_direct_clause_literals = 2;
  candidate.allow_extension = true;
  const auto result = direct_csp_support_live_apply(kernel, candidate);
  assert(result.status == DirectCspSupportLiveStatus::Applied);
  const int32_t unit = 20;
  assert(proof.add_external_rup(++kernel.next_clause_id, &unit, 1));
  assert(proof.add_external_rup(++kernel.next_clause_id, nullptr, 0));
  proof.flush();
  kernel.proof = nullptr;
  std::fclose(records);
}

} // namespace

int main(int argc, char **argv) {
  capture_and_direct_publish();
  extension_and_retained_owner_reuse();
  unit_support_records_pending_propagation();
  stale_owner_and_clean_limits();
  interruption_and_capture_output_stability();
  unsupported_proofs_decline_before_mutation();
  if (argc == 4) {
    write_certificate(argv[1], argv[2], ProofFormat::DsrAscii);
    write_certificate(argv[1], argv[3], ProofFormat::DratAscii);
  }
  std::puts("test_direct_csp_support_live: passed");
}
