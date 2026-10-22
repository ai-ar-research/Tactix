#undef NDEBUG
#include "rules/semantic_extension/gf3_affine_live.hpp"

#include "frontend/parse.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"

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

std::vector<std::vector<int32_t>> formula(bool complete = true) {
  std::vector<std::vector<int32_t>> result;
  std::vector<std::vector<int32_t>> values;
  for (int32_t domain = 0; domain < 5; ++domain) {
    const int32_t first = 3 * domain + 1;
    values.push_back({first, first + 1, first + 2});
    result.push_back(values.back());
    for (uint8_t left = 0; left < 3; ++left)
      for (uint8_t right = left + 1; right < 3; ++right)
        result.push_back({-values.back()[left], -values.back()[right]});
  }
  const auto equation = [&](const std::vector<uint32_t> &domains,
                            const std::vector<uint8_t> &coefficients,
                            uint8_t rhs) {
    for (uint64_t tuple = 0; tuple < 27; ++tuple) {
      uint64_t rest = tuple;
      uint8_t sum = 0;
      std::vector<int32_t> clause;
      for (size_t index = 0; index < domains.size(); ++index) {
        const uint8_t value = static_cast<uint8_t>(rest % 3);
        rest /= 3;
        sum = static_cast<uint8_t>(
            (sum + coefficients[index] * value) % 3);
        clause.push_back(-values[domains[index]][value]);
      }
      if (sum != rhs) result.push_back(std::move(clause));
    }
  };
  equation({0, 1, 2}, {1, 1, 1}, 0);
  equation({2, 3, 4}, {1, 1, 1}, 0);
  if (!complete) result.pop_back();
  return result;
}

std::string dimacs(bool complete = true, bool checker_suffix = false) {
  const auto clauses = formula(complete);
  std::ostringstream output;
  output << "p cnf " << (checker_suffix ? 21 : 15) << ' '
         << clauses.size() + (checker_suffix ? 4 : 0) << '\n';
  for (const auto &clause : clauses) {
    for (int32_t literal : clause) output << literal << ' ';
    output << "0\n";
  }
  if (checker_suffix)
    output << "20 21 0\n-20 21 0\n20 -21 0\n-20 -21 0\n";
  return output.str();
}

Kernel parse(bool complete = true) {
  const std::string text = dimacs(complete);
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

Gf3AffineLiveRequest request(FILE *records) {
  Gf3AffineLiveRequest result;
  result.recognition = {1000, 10000, 100, 100, 6, 1000};
  result.elimination = {8, 10000, 1000, 10000};
  result.first_equation = 0;
  result.second_equation = 1;
  result.maximum_recognition_work = 1000000;
  result.maximum_planning_work = 1000000;
  result.maximum_duplicate_scan_work = 1000000;
  result.maximum_revalidation_work = 1000000;
  result.maximum_proof_work = 100000;
  result.maximum_proof_additions = 1000;
  result.maximum_proof_literal_occurrences = 10000;
  result.maximum_arena_words = 10000;
  result.maximum_journal_bytes = 1000000;
  result.maximum_external_extent = 100;
  result.action_id = 81;
  result.semantic_id = 93;
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

bool operator==(const Image &first, const Image &second) {
  return first.nvars == second.nvars && first.next_id == second.next_id &&
         first.arena == second.arena && first.extent == second.extent &&
         first.trail == second.trail &&
         first.generation == second.generation &&
         first.internal_to_external == second.internal_to_external &&
         first.external_to_internal == second.external_to_internal &&
         first.watches == second.watches;
}

long position(FILE *file) {
  const long result = std::ftell(file);
  assert(result >= 0);
  return result;
}

std::string read_all(FILE *file) {
  assert(!std::fflush(file));
  const long end = position(file);
  std::rewind(file);
  std::string result(static_cast<size_t>(end), '\0');
  assert(std::fread(result.data(), 1, result.size(), file) == result.size());
  assert(!std::fseek(file, end, SEEK_SET));
  return result;
}

Gf3AffineLiveResult decline_unchanged(
    Kernel &kernel, const Gf3AffineLiveRequest &candidate) {
  const Image before = image(kernel);
  const long journal = position(candidate.records);
  const auto result = gf3_affine_live_apply(kernel, candidate);
  assert(result.result == ApplyResult::NoOp);
  assert(result.status != Gf3AffineLiveStatus::Applied);
  assert(!result.emitted_additions && result.published_clause_ids.empty() &&
         !result.first_published_clause_id &&
         !result.last_published_clause_id && !result.journal_bytes &&
         result.semantic_variables.empty());
  assert(image(kernel) == before);
  assert(position(candidate.records) == journal);
  return result;
}

void capture_publish_continue_and_duplicate() {
  Kernel kernel = parse();
  FILE *records = std::tmpfile();
  assert(records);
  auto candidate = request(records);
  Gf3AffineOwnerSnapshot owner;
  const auto captured =
      gf3_affine_live_capture_owner(kernel, candidate, owner);
  assert(captured.status == Gf3AffineLiveStatus::Applied);
  assert(captured.source_clauses == formula().size());
  assert(captured.source_literals == owner.source_literals);
  assert(captured.equations == 2 && owner.theory.domains.size() == 5);
  assert(owner.theory.equations[0].domains ==
         std::vector<uint32_t>({0, 1, 2}));
  assert(owner.theory.equations[1].domains ==
         std::vector<uint32_t>({2, 3, 4}));

  candidate.retained_owner = &owner;
  const uint64_t first_id = kernel.next_clause_id + 1;
  const auto result = gf3_affine_live_apply(kernel, candidate);
  assert(result.status == Gf3AffineLiveStatus::Applied);
  assert(result.result == ApplyResult::Ok);
  assert(result.planned_additions == 54 && result.emitted_additions == 54);
  assert(!result.duplicate_additions && result.published_clause_ids.size() == 54);
  assert(result.first_published_clause_id == first_id);
  assert(result.last_published_clause_id == first_id + 53);
  assert(result.consequence.domains ==
         std::vector<uint32_t>({0, 1, 3, 4}));
  assert(result.consequence.coefficients ==
         std::vector<uint8_t>({1, 1, 2, 2}));
  assert(result.consequence.rhs == 0);
  assert(result.semantic_variables.size() == 12);
  assert(!result.reconstruction_witness_required);
  assert(!result.propagation_pending);
  assert(result.journal_bytes == static_cast<uint64_t>(position(records)));
  const std::string journal = read_all(records);
  assert(journal.find("\"kind\":\"gf3-affine-consequence\"") !=
         std::string::npos);
  assert(journal.find("\"reconstruction_witness_required\":false") !=
         std::string::npos);
  assert(journal.find("\"published_id\":" + std::to_string(first_id)) !=
         std::string::npos);

  // The published clauses are ordinary live constraints.  The controller can
  // later resume propagation/search without any private model constructor.
  for (int32_t selected : {1, 4, 10}) {
    kernel_assign_decision(kernel, kernel_internalize(kernel, selected));
    assert(propagate_service(kernel) == ApplyResult::Ok);
  }
  assert(kernel.value(kernel_internalize(kernel, 13)) > 0);

  // Repeat from a fresh root while retaining the original exact owner.
  while (kernel.level) kernel_backtrack(kernel, kernel.level - 1);
  assert(kernel.trail.empty() && kernel_propagation_fixpoint(kernel));
  FILE *repeat = std::tmpfile();
  assert(repeat);
  candidate.records = repeat;
  candidate.action_id++;
  const auto duplicate = decline_unchanged(kernel, candidate);
  assert(duplicate.status == Gf3AffineLiveStatus::Duplicate);
  assert(duplicate.duplicate_additions == 54);
  std::fclose(repeat);
  std::fclose(records);
}

void incomplete_and_stale_declines() {
  {
    Kernel kernel = parse(false);
    FILE *records = std::tmpfile();
    assert(records);
    const auto result = decline_unchanged(kernel, request(records));
    assert(result.status == Gf3AffineLiveStatus::NoStructure);
    std::fclose(records);
  }
  {
    Kernel kernel = parse();
    FILE *records = std::tmpfile();
    assert(records);
    auto candidate = request(records);
    Gf3AffineOwnerSnapshot owner;
    assert(gf3_affine_live_capture_owner(kernel, candidate, owner).status ==
           Gf3AffineLiveStatus::Applied);
    const auto binary = std::find_if(
        owner.sources.begin(), owner.sources.end(), [](const auto &source) {
          return source.kind == Gf3AffineSourceKind::Binary;
        });
    assert(binary != owner.sources.end());
    assert(kernel_delete_binary(kernel, binary->first, binary->second));
    candidate.retained_owner = &owner;
    const auto stale = decline_unchanged(kernel, candidate);
    assert(stale.status == Gf3AffineLiveStatus::Stale);
    std::fclose(records);
  }
  {
    Kernel kernel = parse();
    FILE *records = std::tmpfile();
    assert(records);
    auto candidate = request(records);
    Gf3AffineOwnerSnapshot owner;
    assert(gf3_affine_live_capture_owner(kernel, candidate, owner).status ==
           Gf3AffineLiveStatus::Applied);
    owner.theory.source.front().native_proof_id++;
    candidate.retained_owner = &owner;
    const auto invalid = decline_unchanged(kernel, candidate);
    assert(invalid.status == Gf3AffineLiveStatus::Invalid);
    std::fclose(records);
  }
}

void resource_and_interruption_declines() {
  for (int mode = 0; mode < 11; ++mode) {
    Kernel kernel = parse();
    FILE *records = std::tmpfile();
    assert(records);
    auto bounded = request(records);
    switch (mode) {
    case 0: bounded.maximum_recognition_work = 1; break;
    case 1: bounded.recognition.maximum_source_clauses = 1; break;
    case 2: bounded.recognition.maximum_source_literals = 1; break;
    case 3: bounded.maximum_planning_work = 1; break;
    case 4: bounded.maximum_duplicate_scan_work = 1; break;
    case 5: bounded.maximum_proof_work = 1; break;
    case 6: bounded.maximum_proof_additions = 1; break;
    case 7: bounded.maximum_proof_literal_occurrences = 1; break;
    case 8: bounded.maximum_arena_words = 1; break;
    case 9: bounded.maximum_journal_bytes = 1; break;
    case 10: bounded.maximum_revalidation_work = 1; break;
    }
    const auto result = decline_unchanged(kernel, bounded);
    assert(result.status == Gf3AffineLiveStatus::ResourceExhausted ||
           result.status == Gf3AffineLiveStatus::NoStructure);
    std::fclose(records);
  }
  {
    Kernel kernel = parse();
    FILE *records = std::tmpfile();
    assert(records);
    auto candidate = request(records);
    volatile sig_atomic_t stop = 1;
    kernel.stop = &stop;
    const auto interrupted = decline_unchanged(kernel, candidate);
    assert(interrupted.status == Gf3AffineLiveStatus::Interrupted);
    Gf3AffineOwnerSnapshot output;
    output.external_extent = 999;
    const auto capture =
        gf3_affine_live_capture_owner(kernel, candidate, output);
    assert(capture.status == Gf3AffineLiveStatus::Interrupted);
    assert(output.external_extent == 999 && output.sources.empty());
    std::fclose(records);
  }

  bool saw_after_plan = false;
  for (uint64_t seed = 1; seed <= 1024 && !saw_after_plan; ++seed) {
    Kernel kernel = parse();
    kernel.stop_fuzz = 127;
    kernel.stop_fuzz_state = seed;
    FILE *records = std::tmpfile();
    assert(records);
    const Image before = image(kernel);
    const auto result = gf3_affine_live_apply(kernel, request(records));
    if (result.status == Gf3AffineLiveStatus::Interrupted) {
      assert(result.result == ApplyResult::NoOp && image(kernel) == before &&
             position(records) == 0);
      saw_after_plan |= result.planned_additions != 0;
    } else {
      assert(result.status == Gf3AffineLiveStatus::Applied);
    }
    std::fclose(records);
  }
  assert(saw_after_plan);
}

void unsupported_proofs_decline_cleanly() {
  for (const auto &[format, path] :
       {std::pair{ProofFormat::Lrat, "/tmp/gf3-affine-decline.lrat"},
        std::pair{ProofFormat::Pbp, "/tmp/gf3-affine-decline.pbp"}}) {
    Kernel kernel = parse();
    Proof proof;
    std::remove(path);
    if (format == ProofFormat::Pbp) {
      assert(proof.open(path, ProofFormat::PbpStreaming));
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
    assert(result.status == Gf3AffineLiveStatus::UnsupportedProof);
    kernel.proof = nullptr;
    std::fclose(records);
  }
}

void write_certificate(const char *cnf_path, const char *proof_path,
                       ProofFormat format) {
  const std::string checker = dimacs(true, true);
  FILE *cnf = std::fopen(cnf_path, "w");
  assert(cnf);
  assert(std::fwrite(checker.data(), 1, checker.size(), cnf) == checker.size());
  std::fclose(cnf);

  Kernel kernel = parse();
  kernel.next_clause_id = formula().size() + 4;
  Proof proof;
  assert(proof.open(proof_path, format));
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  kernel.proof = &proof;
  FILE *records = std::tmpfile();
  assert(records);
  const auto result = gf3_affine_live_apply(kernel, request(records));
  assert(result.status == Gf3AffineLiveStatus::Applied &&
         result.emitted_additions == 54);
  const int32_t unit = 20;
  assert(proof.add_external_rup(++kernel.next_clause_id, &unit, 1));
  assert(proof.add_external_rup(++kernel.next_clause_id, nullptr, 0));
  proof.flush();
  kernel.proof = nullptr;
  std::fclose(records);
}

} // namespace

int main(int argc, char **argv) {
  capture_publish_continue_and_duplicate();
  incomplete_and_stale_declines();
  resource_and_interruption_declines();
  unsupported_proofs_decline_cleanly();
  if (argc == 4) {
    write_certificate(argv[1], argv[2], ProofFormat::DsrAscii);
    write_certificate(argv[1], argv[3], ProofFormat::DratAscii);
  }
  std::puts("test_gf3_affine_live: passed");
}
