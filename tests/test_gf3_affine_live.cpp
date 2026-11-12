#include "unit_test.hpp"
#include "gf3_formula_support.hpp"
#include "kernel_image.hpp"
#include "test_io.hpp"
#include "rules/semantic_extension/gf3_affine_live.hpp"

#include "rules/propagate/propagate.hpp"

#include <csignal>

namespace {

// Five domains; x0 + x1 + x2 = 0 and x2 + x3 + x4 = 0.
Gf3Clauses formula(bool complete = true) {
  return gf3_formula(5, {{0, 1, 2}, {2, 3, 4}}, complete);
}

Kernel parse(bool complete = true) { return gf3_parsed_kernel(formula(complete)); }

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

Gf3AffineLiveResult decline_unchanged(
    Kernel &kernel, const Gf3AffineLiveRequest &candidate) {
  const Image before = image(kernel);
  const long journal = candidate.records.tell();
  const auto result = gf3_affine_live_apply(kernel, candidate);
  assert(result.result == ApplyResult::NoOp);
  assert(result.status != Gf3AffineLiveStatus::Applied);
  assert(!result.emitted_additions && result.published_clause_ids.empty() &&
         !result.first_published_clause_id &&
         !result.last_published_clause_id && !result.journal_bytes &&
         result.semantic_variables.empty());
  assert(image(kernel) == before);
  assert(candidate.records.tell() == journal);
  return result;
}

TEST(capture_publish_continue_and_duplicate) {
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
  const std::string journal = written_bytes(records);
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

// A captured owner corrupted by `corrupt` declines with `expected`.
template <class Corrupt>
void retained_owner_declines(Gf3AffineLiveStatus expected, Corrupt corrupt) {
  Kernel kernel = parse();
  FILE *records = std::tmpfile();
  assert(records);
  auto candidate = request(records);
  Gf3AffineOwnerSnapshot owner;
  assert(gf3_affine_live_capture_owner(kernel, candidate, owner).status ==
         Gf3AffineLiveStatus::Applied);
  corrupt(kernel, owner);
  candidate.retained_owner = &owner;
  assert(decline_unchanged(kernel, candidate).status == expected);
  std::fclose(records);
}

TEST(incomplete_and_stale_declines) {
  {
    Kernel kernel = parse(false);
    FILE *records = std::tmpfile();
    assert(records);
    const auto result = decline_unchanged(kernel, request(records));
    assert(result.status == Gf3AffineLiveStatus::NoStructure);
    std::fclose(records);
  }
  retained_owner_declines(Gf3AffineLiveStatus::Stale,
                          [](Kernel &kernel, Gf3AffineOwnerSnapshot &owner) {
    const auto binary = std::find_if(
        owner.sources.begin(), owner.sources.end(), [](const auto &source) {
          return source.kind == Gf3AffineSourceKind::Binary;
        });
    assert(binary != owner.sources.end());
    assert(kernel_delete_binary(kernel, binary->first, binary->second));
  });
  retained_owner_declines(Gf3AffineLiveStatus::Invalid,
                          [](Kernel &, Gf3AffineOwnerSnapshot &owner) {
    owner.theory.source.front().native_proof_id++;
  });
}

TEST(resource_and_interruption_declines) {
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

} // namespace
