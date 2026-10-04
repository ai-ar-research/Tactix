#undef NDEBUG
#include "frontend/parse.hpp"
#include "proof/proof.hpp"
#include "rules/semantic_extension/exact_cover_live.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <sstream>
#include <string>

using namespace tactix;

static std::string exact_cover_cnf(bool extra = false) {
  std::ostringstream out;
  const unsigned clauses = 15 + unsigned(extra);
  out << "p cnf 9 " << clauses << '\n';
  for (int block = 0; block < 3; ++block)
    for (int first = 0; first < 3; ++first)
      for (int second = first + 1; second < 3; ++second)
        out << -(3 * block + first + 1) << ' '
            << -(3 * block + second + 1) << " 0\n";
  for (int position = 0; position < 3; ++position) {
    for (int block = 0; block < 3; ++block)
      out << 3 * block + position + 1 << ' ';
    out << "0\n";
  }
  for (int diagonal = 0; diagonal < 3; ++diagonal) {
    for (int block = 0; block < 3; ++block)
      out << 3 * block + (block + diagonal) % 3 + 1 << ' ';
    out << "0\n";
  }
  if (extra) out << "1 2 3 0\n";
  return out.str();
}

static Kernel parse(bool extra = false) {
  const std::string text = exact_cover_cnf(extra);
  FILE *input = std::tmpfile();
  assert(input);
  assert(std::fwrite(text.data(), 1, text.size(), input) == text.size());
  std::rewind(input);
  Kernel kernel;
  assert(parse_dimacs(input, kernel) == nullptr);
  std::fclose(input);
  return kernel;
}

static ExactCoverLiveRequest request(FILE *records,
                                     ExactCoverLiveSession &session) {
  ExactCoverLiveRequest result;
  result.arity = 3;
  result.maximum_items = 100;
  result.maximum_source_clauses = 100;
  result.maximum_source_literals = 1000;
  result.maximum_recognition_work = 100000;
  result.maximum_premise_references = 1000;
  result.maximum_definitions = 10;
  result.maximum_additions = 100;
  result.maximum_literal_occurrences = 1000;
  result.maximum_journal_bytes = 100000;
  result.maximum_extent_growth = 10;
  result.maximum_external_extent = 1000;
  result.action_id = 77;
  result.records = records;
  result.session = &session;
  return result;
}

static void positive_and_repeat() {
  Kernel kernel = parse();
  ExactCoverLiveSession session;
  FILE *records = std::tmpfile();
  assert(records);
  const auto result = exact_cover_live_apply(kernel, request(records, session));
  assert(result.status == ExactCoverLiveStatus::Applied);
  assert(result.result == ApplyResult::Ok);
  assert(result.source_clauses == 15 && result.source_literals == 36);
  assert(result.definitions == 3 && result.additions == 12);
  assert(result.literal_occurrences == 30 && result.extent_growth == 3);
  assert(result.revalidation_work > 0);
  assert(result.journal_bytes > 0);
  assert(result.introduced_theory_variables ==
         std::vector<uint32_t>({10, 11, 12}));
  assert(kernel.nvars == 12 && kernel_num_output_vars(kernel) == 9);
  assert(session.committed_keys() == 3);
  assert(session.keys() == std::vector<std::vector<int32_t>>(
                               {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}));
  assert(std::ftell(records) > 0);
  std::rewind(records);
  std::string journal;
  char buffer[1024];
  while (const size_t count = std::fread(buffer, 1, sizeof buffer, records))
    journal.append(buffer, count);
  // Proofless/DRAT kernels name binaries by literals, while large source
  // clauses retain their native identities. Both cases remain explicit.
  assert(journal.find("\"native_proof_id\":0") != std::string::npos);
  assert(journal.find("\"native_proof_id\":10") != std::string::npos);

  FILE *repeat = std::tmpfile();
  assert(repeat);
  const auto before_map = kernel.external_to_internal;
  const uint64_t before_id = kernel.next_clause_id;
  auto repeated = request(repeat, session);
  repeated.action_id = 78;
  const auto repeated_result = exact_cover_live_apply(kernel, repeated);
  assert(repeated_result.result == ApplyResult::NoOp);
  assert(repeated_result.status == ExactCoverLiveStatus::NoStructure);
  assert(kernel.external_to_internal == before_map &&
         kernel.next_clause_id == before_id && std::ftell(repeat) == 0);
  std::fclose(repeat);
  std::fclose(records);
}

static void remove_published_definitions(Kernel &kernel) {
  uint32_t binaries = 0;
  for (int output : {10, 11, 12})
    for (int input = 1; input <= 9; ++input) {
      const Lit first = kernel_internalize(kernel, output);
      const Lit second = kernel_internalize(kernel, -input);
      if (kernel_binary_live(kernel, first, second)) {
        assert(kernel_delete_binary(kernel, first, second));
        ++binaries;
      }
    }
  uint32_t large = 0;
  for (Ref ref = 0; ref < kernel.arena.size();) {
    Clause *clause = clause_at(kernel.arena, ref);
    ref += clause_actual_words(clause);
    if (clause->garbage()) continue;
    const bool generated = std::any_of(
        clause->begin(), clause->end(), [&](Lit literal) {
          const int32_t external = kernel_externalize(kernel, literal);
          return external > 9 || external < -9;
        });
    if (generated) {
      kernel_delete_clause(kernel, clause);
      ++large;
    }
  }
  assert(binaries == 9 && large == 3);
}

static void durable_session_survives_clause_removal() {
  Kernel kernel = parse();
  ExactCoverLiveSession session;
  FILE *first_records = std::tmpfile();
  assert(first_records);
  const auto first =
      exact_cover_live_apply(kernel, request(first_records, session));
  assert(first.status == ExactCoverLiveStatus::Applied);
  assert(session.committed_keys() == 3);
  remove_published_definitions(kernel);

  FILE *second_records = std::tmpfile();
  assert(second_records);
  auto second_request = request(second_records, session);
  second_request.action_id = 78;
  const auto mapping = kernel.external_to_internal;
  const uint64_t id = kernel.next_clause_id;
  const size_t arena = kernel.arena.size();
  const auto second = exact_cover_live_apply(kernel, second_request);
  assert(second.status == ExactCoverLiveStatus::AlreadyPublished);
  assert(second.result == ApplyResult::NoOp);
  assert(second.recognition_work > 0 && second.revalidation_work == 0);
  assert(session.committed_keys() == 3);
  assert(kernel.external_to_internal == mapping && kernel.next_clause_id == id);
  assert(kernel.arena.size() == arena && std::ftell(second_records) == 0);
  std::fclose(second_records);
  std::fclose(first_records);
}

static void final_revalidation_obeys_recognition_limit() {
  uint64_t complete_work = 0;
  {
    Kernel kernel = parse();
    ExactCoverLiveSession session;
    FILE *records = std::tmpfile();
    assert(records);
    const auto result = exact_cover_live_apply(kernel, request(records, session));
    assert(result.status == ExactCoverLiveStatus::Applied);
    assert(result.revalidation_work > 0);
    complete_work = result.recognition_work;
    std::fclose(records);
  }
  assert(complete_work > 1);

  Kernel kernel = parse();
  ExactCoverLiveSession session;
  FILE *records = std::tmpfile();
  assert(records);
  auto limited = request(records, session);
  limited.maximum_recognition_work = complete_work - 1;
  const auto mapping = kernel.external_to_internal;
  const uint64_t id = kernel.next_clause_id;
  const size_t arena = kernel.arena.size();
  const auto result = exact_cover_live_apply(kernel, limited);
  assert(result.status == ExactCoverLiveStatus::ResourceExhausted);
  assert(result.result == ApplyResult::NoOp);
  assert(result.revalidation_work > 0);
  assert(session.committed_keys() == 0);
  assert(kernel.external_to_internal == mapping && kernel.next_clause_id == id);
  assert(kernel.arena.size() == arena && std::ftell(records) == 0);
  std::fclose(records);
}

static void clean_declines() {
  {
    Kernel kernel = parse(true);
    ExactCoverLiveSession session;
    FILE *records = std::tmpfile();
    assert(records);
    const auto mapping = kernel.external_to_internal;
    const uint64_t id = kernel.next_clause_id;
    const size_t arena = kernel.arena.size();
    const uint32_t nvars = kernel.nvars;
    const uint32_t outputs = kernel_num_output_vars(kernel);
    const auto result =
        exact_cover_live_apply(kernel, request(records, session));
    assert(result.result == ApplyResult::NoOp);
    assert(result.status == ExactCoverLiveStatus::NoStructure);
    assert(kernel.external_to_internal == mapping && kernel.next_clause_id == id);
    assert(kernel.arena.size() == arena && kernel.nvars == nvars);
    assert(kernel_num_output_vars(kernel) == outputs);
    assert(session.committed_keys() == 0);
    assert(std::ftell(records) == 0);
    std::fclose(records);
  }

  for (int mode = 0; mode < 11; ++mode) {
    Kernel kernel = parse();
    ExactCoverLiveSession session;
    FILE *records = std::tmpfile();
    assert(records);
    auto candidate = request(records, session);
    switch (mode) {
    case 0: candidate.maximum_recognition_work = 1; break;
    case 1: candidate.maximum_source_clauses = 14; break;
    case 2: candidate.maximum_source_literals = 35; break;
    case 3: candidate.maximum_items = 8; break;
    case 4: candidate.maximum_premise_references = 26; break;
    case 5: candidate.maximum_definitions = 2; break;
    case 6: candidate.maximum_additions = 11; break;
    case 7: candidate.maximum_literal_occurrences = 29; break;
    case 8: candidate.maximum_extent_growth = 2; break;
    case 9: candidate.maximum_external_extent = 11; break;
    case 10: candidate.maximum_journal_bytes = 1; break;
    }
    const auto mapping = kernel.external_to_internal;
    const uint64_t id = kernel.next_clause_id;
    const size_t arena = kernel.arena.size();
    const uint32_t nvars = kernel.nvars;
    const uint32_t outputs = kernel_num_output_vars(kernel);
    const auto result = exact_cover_live_apply(kernel, candidate);
    assert(result.result == ApplyResult::NoOp);
    assert(result.status == ExactCoverLiveStatus::ResourceExhausted);
    if (mode == 0) assert(result.recognition_work > 0);
    assert(kernel.external_to_internal == mapping && kernel.next_clause_id == id);
    assert(kernel.arena.size() == arena && kernel.nvars == nvars);
    assert(kernel_num_output_vars(kernel) == outputs);
    assert(session.committed_keys() == 0);
    assert(std::ftell(records) == 0);
    std::fclose(records);
  }
}

static void interrupted_decline_reports_work() {
  Kernel kernel = parse();
  ExactCoverLiveSession session;
  FILE *records = std::tmpfile();
  assert(records);
  volatile sig_atomic_t stop = 1;
  kernel.stop = &stop;
  const auto mapping = kernel.external_to_internal;
  const uint64_t id = kernel.next_clause_id;
  const auto result =
      exact_cover_live_apply(kernel, request(records, session));
  assert(result.status == ExactCoverLiveStatus::Interrupted);
  assert(result.result == ApplyResult::NoOp);
  assert(kernel.external_to_internal == mapping && kernel.next_clause_id == id);
  assert(session.committed_keys() == 0);
  assert(std::ftell(records) == 0);
  std::fclose(records);
}

static void write_certificate(const char *cnf_path, const char *proof_path,
                              ProofFormat format) {
  std::string checker = exact_cover_cnf();
  const std::string header = "p cnf 9 15\n";
  assert(checker.starts_with(header));
  checker.replace(0, header.size(), "p cnf 15 19\n");
  checker += "14 15 0\n-14 15 0\n14 -15 0\n-14 -15 0\n";
  FILE *cnf = std::fopen(cnf_path, "w");
  assert(cnf);
  assert(std::fwrite(checker.data(), 1, checker.size(), cnf) ==
         checker.size());
  std::fclose(cnf);

  Kernel kernel = parse();
  ExactCoverLiveSession session;
  kernel.next_clause_id = 19;
  Proof proof;
  assert(proof.open(proof_path, format));
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  kernel.proof = &proof;
  FILE *records = std::tmpfile();
  assert(records);
  const auto result =
      exact_cover_live_apply(kernel, request(records, session));
  assert(result.status == ExactCoverLiveStatus::Applied);
  const int32_t unit = 14;
  assert(proof.add_external_rup(++kernel.next_clause_id, &unit, 1));
  assert(proof.add_external_rup(++kernel.next_clause_id, nullptr, 0));
  proof.flush();
  kernel.proof = nullptr;
  std::fclose(records);
}

int main(int argc, char **argv) {
  positive_and_repeat();
  durable_session_survives_clause_removal();
  final_revalidation_obeys_recognition_limit();
  clean_declines();
  interrupted_decline_reports_work();
  if (argc == 4) {
    write_certificate(argv[1], argv[2], ProofFormat::DsrAscii);
    write_certificate(argv[1], argv[3], ProofFormat::DratAscii);
  }
  std::puts("test_exact_cover_live: passed");
}
