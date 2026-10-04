#undef NDEBUG
#include "rules/semantic_extension/direct_csp_support_action.hpp"

#include "frontend/parse.hpp"
#include "proof/proof.hpp"

#include <algorithm>
#include <cassert>
#include <csignal>
#include <cstdio>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace tactix;

namespace {

std::vector<std::vector<int32_t>> clauses() {
  return {{1, 2, 3},  {-1, -2}, {-1, -3}, {-2, -3},
          {4, 5, 6, 7}, {-4, -5}, {-4, -6}, {-4, -7}, {-5, -6},
          {-5, -7}, {-6, -7}, {-1, -7}, {-2, -4}, {-2, -6},
          {-2, -7}, {-3, -5}};
}

std::string dimacs(bool checker_suffix = false) {
  const auto formula = clauses();
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

Kernel parse() {
  const std::string text = dimacs();
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

DirectCspSupportActionSelection selection(uint32_t row) {
  return {{1, 2, 3}, {4, 5, 6, 7}, row};
}

DirectCspSupportActionLimits limits(bool extension = false) {
  DirectCspSupportActionLimits result;
  result.maximum_source_clauses = 100;
  result.maximum_source_literals = 1000;
  result.maximum_recognition_work = 100000;
  result.maximum_domain_values = 16;
  result.maximum_direct_clause_literals = extension ? 2 : 8;
  result.maximum_definition_clause_literals = 16;
  result.maximum_proof_work = 1000;
  result.maximum_proof_additions = 20;
  result.maximum_proof_literal_occurrences = 100;
  result.maximum_journal_bytes = 100000;
  result.maximum_extent_growth = extension ? 1 : 0;
  result.maximum_external_extent = 100;
  result.maximum_session_keys = 16;
  result.maximum_session_work = 100;
  result.allow_extension = extension;
  assert(result.valid());
  return result;
}

void attach(Kernel &kernel, FILE *records) {
  kernel.structural_provenance = records;
  kernel.structural_action_sequence = 40;
}

struct Image {
  uint32_t nvars = 0;
  uint64_t next_clause_id = 0;
  uint64_t action_sequence = 0;
  size_t arena_words = 0;
  size_t external_extent = 0;
  size_t trail = 0;
  uint64_t arena_generation = 0;
  bool candidate_pending = false;
  std::vector<size_t> watches;
};

Image image(const Kernel &kernel) {
  Image result;
  result.nvars = kernel.nvars;
  result.next_clause_id = kernel.next_clause_id;
  result.action_sequence = kernel.structural_action_sequence;
  result.arena_words = kernel.arena.size();
  result.external_extent = kernel.external_to_internal.size();
  result.trail = kernel.trail.size();
  result.arena_generation = kernel.arena_generation;
  result.candidate_pending = kernel.semantic_variable_candidate_pending;
  for (Lit literal = 0; literal < 2 * kernel.nvars; ++literal)
    result.watches.push_back(kernel.watches.size(literal));
  return result;
}

bool operator==(const Image &first, const Image &second) {
  return first.nvars == second.nvars &&
         first.next_clause_id == second.next_clause_id &&
         first.action_sequence == second.action_sequence &&
         first.arena_words == second.arena_words &&
         first.external_extent == second.external_extent &&
         first.trail == second.trail &&
         first.arena_generation == second.arena_generation &&
         first.candidate_pending == second.candidate_pending &&
         first.watches == second.watches;
}

long position(FILE *file) {
  const long result = std::ftell(file);
  assert(result >= 0);
  return result;
}

void direct_then_extension_and_durable_replay() {
  Kernel kernel = parse();
  FILE *records = std::tmpfile();
  assert(records);
  attach(kernel, records);
  DirectCspSupportActionSession session;

  const auto first = direct_csp_support_action_apply(
      kernel, session, limits(), selection(1));
  assert(first.status == DirectCspSupportActionStatus::Applied &&
         first.result == ApplyResult::Ok);
  assert(first.proposed_action_id == 41 && first.committed_action_id == 41 &&
         first.semantic_id == 41 &&
         first.key == DirectCspSupportActionKey{1});
  assert(first.owner_retained && session.retained() &&
         session.belongs_to(kernel) && session.committed_keys() == 1 &&
         session.snapshot() &&
         session.snapshot()->source.size() == clauses().size());
  assert(first.effort.owner_capture_work && first.effort.recognition_work &&
         first.effort.emitted_additions == 1 &&
         first.live.published_support == std::vector<int32_t>({-2, 5}));
  assert(first.semantic_candidate_ready &&
         first.semantic_variables.size() == 2 &&
         !first.projection.extension_variable);
  for (const auto &variable : first.semantic_variables)
    assert(variable.role == strategy::SemanticVariableRole::ExistingTheory &&
           variable.priority == 1);
  assert(!kernel.semantic_variable_candidate_pending);

  const auto second = direct_csp_support_action_apply(
      kernel, session, limits(true), selection(0));
  assert(second.status == DirectCspSupportActionStatus::Applied &&
         second.result == ApplyResult::Ok &&
         second.proposed_action_id == 42 && second.committed_action_id == 42 &&
         second.effort.owner_capture_work == 0 &&
         second.effort.extent_growth == 1 && kernel.nvars == 8 &&
         kernel_num_output_vars(kernel) == 7);
  assert(session.committed_keys() == 2 &&
         session.keys()[0] == DirectCspSupportActionKey{0} &&
         session.keys()[1] == DirectCspSupportActionKey{1});
  assert(second.live.published_support == std::vector<int32_t>({-1, 8}) &&
         second.semantic_variables.size() == 5);
  assert(second.semantic_variables.back().external == 8 &&
         second.semantic_variables.back().role ==
             strategy::SemanticVariableRole::IntroducedTheory);
  assert(second.projection.extension_variable == 8 &&
         second.projection.definition_inputs ==
             std::vector<int32_t>({4, 5, 6}) &&
         second.projection.exact_or_definition &&
         second.projection.project_from_external_model &&
         !second.projection.reconstruction_witness_required);

  // The support clause can disappear without reopening its semantic action.
  assert(kernel_delete_binary(kernel, kernel_internalize(kernel, -1),
                              kernel_internalize(kernel, 8)));
  const Image before = image(kernel);
  const long journal = position(records);
  const auto repeated = direct_csp_support_action_apply(
      kernel, session, limits(true), selection(0));
  assert(repeated.status ==
             DirectCspSupportActionStatus::AlreadyPublished &&
         repeated.result == ApplyResult::NoOp &&
         repeated.key == DirectCspSupportActionKey{0} &&
         !repeated.proposed_action_id && !repeated.committed_action_id &&
         repeated.committed_keys == 2 && image(kernel) == before &&
         position(records) == journal);
  std::fclose(records);
}

void declines_are_atomic_and_sessions_are_kernel_local() {
  {
    Kernel kernel = parse();
    FILE *records = std::tmpfile();
    assert(records);
    attach(kernel, records);
    DirectCspSupportActionSession session;
    const Image before = image(kernel);
    auto invalid_selection = selection(3);
    const auto invalid = direct_csp_support_action_apply(
        kernel, session, limits(), invalid_selection);
    assert(invalid.status == DirectCspSupportActionStatus::Invalid &&
           invalid.result == ApplyResult::NoOp && !session.retained() &&
           image(kernel) == before && !position(records));

    auto tiny = limits();
    tiny.maximum_session_work = 1;
    const auto exhausted = direct_csp_support_action_apply(
        kernel, session, tiny, selection(1));
    assert(exhausted.status ==
               DirectCspSupportActionStatus::ResourceExhausted &&
           exhausted.result == ApplyResult::NoOp && !session.retained() &&
           image(kernel) == before && !position(records));
    std::fclose(records);
  }
  {
    Kernel first_kernel = parse();
    FILE *first_records = std::tmpfile();
    assert(first_records);
    attach(first_kernel, first_records);
    DirectCspSupportActionSession session;
    assert(direct_csp_support_action_apply(first_kernel, session, limits(),
                                           selection(1))
               .status == DirectCspSupportActionStatus::Applied);

    Kernel second_kernel = parse();
    FILE *second_records = std::tmpfile();
    assert(second_records);
    attach(second_kernel, second_records);
    const Image before = image(second_kernel);
    const auto stale = direct_csp_support_action_apply(
        second_kernel, session, limits(), selection(2));
    assert(stale.status == DirectCspSupportActionStatus::Stale &&
           stale.result == ApplyResult::NoOp && image(second_kernel) == before &&
           session.belongs_to(first_kernel) && session.committed_keys() == 1 &&
           !position(second_records));
    std::fclose(second_records);
    std::fclose(first_records);
  }
}

void mapped_live_declines_are_atomic() {
  {
    Kernel kernel = parse();
    assert(kernel_delete_binary(kernel, kernel_internalize(kernel, -1),
                                kernel_internalize(kernel, -2)));
    FILE *records = std::tmpfile();
    assert(records);
    attach(kernel, records);
    DirectCspSupportActionSession session;
    const Image before = image(kernel);
    const auto missing = direct_csp_support_action_apply(
        kernel, session, limits(), selection(1));
    assert(missing.status == DirectCspSupportActionStatus::NoStructure &&
           missing.result == ApplyResult::NoOp && !session.retained() &&
           image(kernel) == before && !position(records));
    std::fclose(records);
  }
  {
    Kernel kernel = parse();
    FILE *records = std::tmpfile();
    assert(records);
    attach(kernel, records);
    volatile sig_atomic_t stop = 1;
    kernel.stop = &stop;
    DirectCspSupportActionSession session;
    const Image before = image(kernel);
    const auto interrupted = direct_csp_support_action_apply(
        kernel, session, limits(), selection(1));
    assert(interrupted.status == DirectCspSupportActionStatus::Interrupted &&
           interrupted.result == ApplyResult::NoOp && !session.retained() &&
           image(kernel) == before && !position(records));
    std::fclose(records);
  }
  {
    Kernel kernel = parse();
    FILE *records = std::tmpfile();
    assert(records);
    attach(kernel, records);
    DirectCspSupportActionSession session;
    auto bounded = limits();
    bounded.maximum_recognition_work = 1;
    const Image before = image(kernel);
    const auto exhausted = direct_csp_support_action_apply(
        kernel, session, bounded, selection(1));
    assert(exhausted.status ==
               DirectCspSupportActionStatus::ResourceExhausted &&
           exhausted.result == ApplyResult::NoOp && !session.retained() &&
           image(kernel) == before && !position(records));
    std::fclose(records);
  }
}

void retained_payload_and_source_are_exact() {
  Kernel kernel = parse();
  FILE *records = std::tmpfile();
  assert(records);
  attach(kernel, records);
  DirectCspSupportActionSession session;
  assert(direct_csp_support_action_apply(kernel, session, limits(),
                                         selection(1))
             .status == DirectCspSupportActionStatus::Applied);

  auto changed = selection(2);
  std::swap(changed.right_values[0], changed.right_values[1]);
  const Image before_mismatch = image(kernel);
  const long before_journal = position(records);
  const auto mismatch = direct_csp_support_action_apply(
      kernel, session, limits(), changed);
  assert(mismatch.status == DirectCspSupportActionStatus::Invalid &&
         mismatch.result == ApplyResult::NoOp &&
         image(kernel) == before_mismatch &&
         position(records) == before_journal &&
         session.committed_keys() == 1);

  assert(kernel_delete_binary(kernel, kernel_internalize(kernel, -1),
                              kernel_internalize(kernel, -2)));
  const Image before_stale = image(kernel);
  const auto stale = direct_csp_support_action_apply(
      kernel, session, limits(), selection(2));
  assert(stale.status == DirectCspSupportActionStatus::Stale &&
         stale.result == ApplyResult::NoOp && image(kernel) == before_stale &&
         position(records) == before_journal &&
         session.committed_keys() == 1);
  std::fclose(records);
}

void proof_modes_and_guard() {
  for (const auto &[format, path] :
       {std::pair{ProofFormat::Lrat, "/tmp/direct-csp-action-decline.lrat"},
        std::pair{ProofFormat::Pbp, "/tmp/direct-csp-action-decline.pbp"}}) {
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
    attach(kernel, records);
    DirectCspSupportActionSession session;
    const Image before = image(kernel);
    const auto result = direct_csp_support_action_apply(
        kernel, session, limits(), selection(1));
    assert(result.status == DirectCspSupportActionStatus::Inapplicable &&
           result.result == ApplyResult::NoOp && image(kernel) == before &&
           !session.retained() && !position(records));
    kernel.proof = nullptr;
    std::fclose(records);
  }
}

void projection_preserves_an_original_model() {
  Kernel kernel = parse();
  FILE *records = std::tmpfile();
  assert(records);
  attach(kernel, records);
  DirectCspSupportActionSession session;
  const auto result = direct_csp_support_action_apply(
      kernel, session, limits(true), selection(0));
  assert(result.status == DirectCspSupportActionStatus::Applied &&
         result.projection.exact_or_definition);

  // left=1 and right=4 is an original-formula model.  The exact-OR extension
  // is true because right=4 is true, and forgetting it recovers that model.
  const std::vector<bool> value{false, true, false, false, true,
                                false, false, false, true};
  const auto satisfied = [&](const std::vector<int32_t> &clause) {
    return std::any_of(clause.begin(), clause.end(), [&](int32_t literal) {
      const bool assigned = value[static_cast<size_t>(
          literal < 0 ? -int64_t(literal) : int64_t(literal))];
      return literal > 0 ? assigned : !assigned;
    });
  };
  for (const auto &clause : clauses()) assert(satisfied(clause));
  assert(satisfied(result.live.published_support));
  assert(value[result.projection.extension_variable] ==
         std::any_of(result.projection.definition_inputs.begin(),
                     result.projection.definition_inputs.end(),
                     [&](int32_t literal) {
                       assert(literal > 0);
                       return value[static_cast<size_t>(literal)];
                     }));
  std::fclose(records);
}

void write_certificate(const char *cnf_path, const char *proof_path,
                       ProofFormat format) {
  const std::string checker = dimacs(true);
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
  attach(kernel, records);
  DirectCspSupportActionSession session;
  const auto result = direct_csp_support_action_apply(
      kernel, session, limits(true), selection(0));
  assert(result.status == DirectCspSupportActionStatus::Applied &&
         result.effort.emitted_additions == 5);
  const int32_t unit = 20;
  assert(proof.add_external_rup(++kernel.next_clause_id, &unit, 1));
  assert(proof.add_external_rup(++kernel.next_clause_id, nullptr, 0));
  proof.flush();
  kernel.proof = nullptr;
  std::fclose(records);
}

void write_model(const char *cnf_path, const char *model_path) {
  const std::string input = dimacs(false);
  FILE *cnf = std::fopen(cnf_path, "w");
  assert(cnf);
  assert(std::fwrite(input.data(), 1, input.size(), cnf) == input.size());
  std::fclose(cnf);
  const std::string model = "s SATISFIABLE\nv 1 -2 -3 4 -5 -6 -7 0\n";
  FILE *output = std::fopen(model_path, "w");
  assert(output);
  assert(std::fwrite(model.data(), 1, model.size(), output) == model.size());
  std::fclose(output);
}

} // namespace

int main(int argc, char **argv) {
  direct_then_extension_and_durable_replay();
  declines_are_atomic_and_sessions_are_kernel_local();
  mapped_live_declines_are_atomic();
  retained_payload_and_source_are_exact();
  proof_modes_and_guard();
  projection_preserves_an_original_model();
  if (argc == 6) {
    write_certificate(argv[1], argv[2], ProofFormat::DsrAscii);
    write_certificate(argv[1], argv[3], ProofFormat::DratAscii);
    write_model(argv[4], argv[5]);
  }
  std::puts("test_direct_csp_support_action: passed");
}
