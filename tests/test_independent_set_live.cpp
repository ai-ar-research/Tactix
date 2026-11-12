#undef NDEBUG
#include "independent_set_test_support.hpp"
#include "dimacs_text.hpp"
#include "temporary_file.hpp"
#include "test_io.hpp"
#include "rules/semantic_extension/independent_set_live.hpp"

#include "frontend/parse.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"

#include <algorithm>
#include <cassert>
#include <climits>
#include <csignal>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

using namespace tactix;

namespace {

using Encoding = IndependentSetEncoding;

Encoding make_encoding(uint32_t vertices, uint32_t target,
                       std::vector<std::pair<uint32_t, uint32_t>> edges) {
  return independent_set_encoding(vertices, target, std::move(edges));
}

std::string dimacs(const Encoding &encoding) {
  return dimacs_text(encoding.variables, encoding.clauses);
}

void load(Kernel &kernel, const Encoding &encoding) {
  parse_dimacs_text(dimacs(encoding), kernel, ParsedRoot::EmptyTrail);
}

IndependentSetLiveRequest request(FILE *records,
                                  std::vector<uint32_t> cover = {1, 2, 3}) {
  IndependentSetLiveRequest result;
  result.cover_vertices = std::move(cover);
  result.maximum_clauses = 10000;
  result.maximum_literals = 100000;
  result.maximum_recognition_work = 10000000;
  result.maximum_proof_work = 10000000;
  result.maximum_proof_additions = 100000;
  result.maximum_proof_deletions = 100000;
  result.maximum_proof_literal_occurrences = 10000000;
  result.maximum_journal_bytes = 10000000;
  result.maximum_vertices = 64;
  result.maximum_target = 64;
  result.action_id = 17;
  result.semantic_id = 29;
  result.records = records;
  return result;
}

struct KernelImage {
  uint32_t nvars = 0;
  uint64_t next_clause_id = 0;
  size_t arena_words = 0;
  size_t external_extent = 0;
  size_t trail = 0;
  bool startup_open = false;
  uint64_t arena_generation = 0;
  std::vector<Var> internal_to_external;
  std::vector<Var> external_to_internal;
  std::vector<size_t> watch_sizes;
  bool operator==(const KernelImage &) const = default;
};

KernelImage image(const Kernel &kernel) {
  KernelImage result;
  result.nvars = kernel.nvars;
  result.next_clause_id = kernel.next_clause_id;
  result.arena_words = kernel.arena.size();
  result.external_extent = kernel.external_to_internal.size();
  result.trail = kernel.trail.size();
  result.startup_open = kernel.startup_certificate_open;
  result.arena_generation = kernel.arena_generation;
  result.internal_to_external = kernel.internal_to_external;
  result.external_to_internal = kernel.external_to_internal;
  for (Lit literal = 0; literal < 2 * kernel.nvars; ++literal)
    result.watch_sizes.push_back(kernel.watches.size(literal));
  return result;
}

void assert_declined(const IndependentSetLiveResult &result) {
  assert(result.status != IndependentSetLiveStatus::Applied);
  assert(result.result == ApplyResult::NoOp);
  assert(!result.emitted_proof_additions && !result.emitted_proof_deletions &&
         !result.published_clause_id &&
         result.advice_vertex_literals.empty() &&
         result.published_cover.empty());
  assert(result.recognition_work ==
         result.source_scan_work + result.theory_recognition_work);
}

IndependentSetLiveResult
assert_failed_unchanged(Kernel &kernel,
                        const IndependentSetLiveRequest &candidate) {
  const KernelImage before = image(kernel);
  const long record_position = candidate.records.tell();
  assert(record_position >= 0);
  const IndependentSetLiveResult result =
      independent_set_live_apply(kernel, candidate);
  assert_declined(result);
  assert(image(kernel) == before);
  assert(candidate.records.tell() == record_position);
  return result;
}

bool all_live_clauses_satisfied(const Kernel &kernel) {
  for (Ref reference = 0; reference < kernel.arena.size();) {
    const Clause *clause = clause_at(kernel.arena, reference);
    reference += clause_actual_words(clause);
    if (clause->garbage())
      continue;
    bool satisfied = false;
    for (const Lit literal : *clause)
      satisfied |= kernel.value(literal) > 0;
    if (!satisfied)
      return false;
  }
  for (Lit first = 0; first < 2 * kernel.nvars; ++first)
    for (const WatchEntry watch : kernel.watches.entries(first))
      if (watch.binary() && first < watch.other() &&
          kernel.value(first) <= 0 && kernel.value(watch.other()) <= 0)
        return false;
  return true;
}

std::vector<uint8_t> satisfying_assignment(const Encoding &encoding,
                                           std::vector<uint8_t> vertices) {
  std::vector<uint8_t> values(encoding.variables + 1, 0);
  uint32_t count = 0;
  for (uint32_t vertex = 1; vertex <= encoding.vertices; ++vertex) {
    values[vertex] = vertices[vertex - 1];
    count = std::min(encoding.target, count + vertices[vertex - 1]);
    values[encoding.layers[vertex][count]] = 1;
  }
  return values;
}

void complete_model(Kernel &kernel, const std::vector<uint8_t> &values) {
  for (uint32_t external = 1; external < values.size(); ++external) {
    const Lit positive = kernel_internalize(kernel, external);
    if (kernel.value(positive))
      continue;
    kernel_assign_decision(kernel,
                           values[external] ? positive : negate(positive));
    assert(propagate_service(kernel) == ApplyResult::Ok);
  }
  assert(kernel.trail.size() == kernel.nvars);
  assert(all_live_clauses_satisfied(kernel));
}

void positive_and_continuing(const char *proof_path, ProofFormat format) {
  const Encoding encoding = make_encoding(4, 2, {{1, 2}});
  Kernel kernel;
  load(kernel, encoding);
  const auto initial_map = kernel.external_to_internal;
  Proof proof;
  if (proof_path) {
    assert(proof.open(proof_path, format));
    proof.bind_variable_map(&kernel.internal_to_external);
    proof.bind_external_variable_map(&kernel.external_to_internal);
    kernel.proof = &proof;
  }
  FILE *records = std::tmpfile();
  assert(records);
  IndependentSetLiveRequest apply = request(records);

  auto low = apply;
  low.maximum_proof_additions = 2;
  auto declined = assert_failed_unchanged(kernel, low);
  assert(declined.status == IndependentSetLiveStatus::ResourceExhausted);
  low = apply;
  low.maximum_proof_deletions = 1;
  declined = assert_failed_unchanged(kernel, low);
  assert(declined.status == IndependentSetLiveStatus::ResourceExhausted &&
         declined.planned_proof_additions == 3);
  low = apply;
  low.maximum_recognition_work = 3;
  declined = assert_failed_unchanged(kernel, low);
  assert(declined.status == IndependentSetLiveStatus::ResourceExhausted &&
         declined.recognition_work == 3 && declined.source_scan_work == 3);
  low = apply;
  low.maximum_proof_work = 1;
  declined = assert_failed_unchanged(kernel, low);
  assert(declined.status == IndependentSetLiveStatus::ResourceExhausted &&
         declined.proof_work <= low.maximum_proof_work);
  low = apply;
  low.maximum_journal_bytes = 1;
  declined = assert_failed_unchanged(kernel, low);
  assert(declined.status == IndependentSetLiveStatus::ResourceExhausted &&
         declined.journal_work > low.maximum_journal_bytes);

  const uint64_t prior_id = kernel.next_clause_id;
  const IndependentSetLiveResult result =
      independent_set_live_apply(kernel, apply);
  assert(result.status == IndependentSetLiveStatus::Applied &&
         result.result == ApplyResult::Ok && result.vertices == 4 &&
         result.target == 2 && result.source_clauses == encoding.clauses.size());
  assert(result.recognition_work ==
             result.source_scan_work + result.theory_recognition_work &&
         result.source_scan_work > 0 &&
         result.theory_recognition_work > 0 && result.proof_work > 0 &&
         result.journal_work > 0);
  assert(result.planned_proof_additions == 3 &&
         result.proof_literal_occurrences == 11);
  assert(result.emitted_proof_additions == (proof_path ? 3 : 0));
  assert(result.emitted_proof_deletions == (proof_path ? 2 : 0));
  assert(result.published_clause_id ==
         prior_id + (proof_path ? result.planned_proof_additions : 1));
  assert(result.published_cover == std::vector<int32_t>({1, 2, 3}));
  assert(result.advice_vertex_literals ==
         std::vector<int32_t>({1, 2, 3, 4}));
  assert(kernel.nvars == encoding.variables &&
         kernel.external_to_internal == initial_map);
  assert(!kernel.startup_certificate_open);
  const std::string journal = written_bytes(records);
  assert(result.journal_work == journal.size());
  assert(journal.find("independent-set-live-source") != std::string::npos);
  assert(journal.find("forbidden-assignment") != std::string::npos);
  assert(journal.find("resolution") != std::string::npos);
  assert(journal.find("advice_vertices\":[1,2,3,4]") != std::string::npos);
  assert(journal.find("independent-set-publish-cover") != std::string::npos);
  assert(journal.find("independent-set-conclusion-binding") !=
         std::string::npos);
  if (!proof_path) {
    assert(journal.find("\"proof_mode\":\"none\"") != std::string::npos);
    assert(journal.find("\"conclusion_proof_id\":0,"
                        "\"published_clause_id\":" +
                        std::to_string(result.published_clause_id)) !=
           std::string::npos);
  }

  const KernelImage duplicate_before = image(kernel);
  const long journal_before = std::ftell(records);
  const auto duplicate = independent_set_live_apply(kernel, apply);
  assert_declined(duplicate);
  assert(duplicate.status == IndependentSetLiveStatus::Duplicate &&
         duplicate.source_scan_work > 0);
  assert(image(kernel) == duplicate_before &&
         std::ftell(records) == journal_before);

  kernel_assign_decision(kernel, kernel_internalize(kernel, -1));
  assert(propagate_service(kernel) == ApplyResult::Ok);
  kernel_assign_decision(kernel, kernel_internalize(kernel, -2));
  assert(propagate_service(kernel) == ApplyResult::Ok);
  const Lit third = kernel_internalize(kernel, 3);
  assert(kernel.value(third) > 0);
  const Assigned &reason = kernel.assigned[var_of(third)];
  assert(!reason.binary && reason.reason != DECISION_REASON &&
         reason.reason != UNIT_REASON);
  assert(clause_at(kernel.arena, reason.reason)->id() ==
         result.published_clause_id);
  complete_model(kernel,
                 satisfying_assignment(encoding, {0, 0, 1, 1}));
  if (proof_path) {
    proof.flush();
    kernel.proof = nullptr;
  }
  std::fclose(records);
}

void retained_owner_repeated_covers() {
  const Encoding encoding = make_encoding(4, 2, {{1, 2}});
  Kernel kernel;
  load(kernel, encoding);
  FILE *records = std::tmpfile();
  assert(records);
  auto apply = request(records);
  IndependentSetOwnerSnapshot owner;
  const auto capture =
      independent_set_live_capture_owner_detailed(kernel, apply, owner);
  assert(capture.captured() && capture.source_scan_work > 0 &&
         capture.theory_recognition_work > 0 &&
         capture.recognition_work ==
             capture.source_scan_work + capture.theory_recognition_work);
  assert(owner.external_variables == encoding.variables &&
         owner.vertices == encoding.vertices && owner.target == encoding.target &&
         owner.clauses.size() == encoding.clauses.size() &&
         owner.capture_source_scan_work == capture.source_scan_work &&
         owner.capture_theory_recognition_work ==
             capture.theory_recognition_work);

  // Capture and the first cached apply share one recognition ceiling; with no
  // room left the apply declines without effect and reports the capture work.
  apply.owner_snapshot = &owner;
  apply.include_owner_capture_effort = true;
  apply.maximum_recognition_work = capture.recognition_work;
  const auto no_apply_room = assert_failed_unchanged(kernel, apply);
  assert(no_apply_room.status == IndependentSetLiveStatus::ResourceExhausted &&
         no_apply_room.recognition_work == capture.recognition_work &&
         no_apply_room.source_scan_work == capture.source_scan_work &&
         no_apply_room.theory_recognition_work ==
             capture.theory_recognition_work);

  apply.maximum_recognition_work = 10000000;
  const auto first = independent_set_live_apply(kernel, apply);
  assert(first.status == IndependentSetLiveStatus::Applied &&
         first.result == ApplyResult::Ok &&
         first.recognition_work > capture.recognition_work &&
         first.source_scan_work >= capture.source_scan_work &&
         first.theory_recognition_work >= capture.theory_recognition_work &&
         first.published_cover == std::vector<int32_t>({1, 2, 3}));

  // An unrelated later semantic consequence must not contaminate the retained
  // owner image used for a second, distinct cover.
  const std::vector<int32_t> unrelated_external = {1, 2, 3, 4};
  std::vector<Lit> unrelated;
  for (int32_t external : unrelated_external)
    unrelated.push_back(kernel_internalize(kernel, external));
  kernel_add_large_clause(kernel, unrelated.data(), unrelated.size(), true,
                          unrelated.size(), ++kernel.next_clause_id);

  apply.cover_vertices = {1, 2, 4};
  apply.action_id = 18;
  apply.semantic_id = 30;
  apply.include_owner_capture_effort = false;
  const auto second = independent_set_live_apply(kernel, apply);
  assert(second.status == IndependentSetLiveStatus::Applied &&
         second.result == ApplyResult::Ok &&
         second.recognition_work < first.recognition_work &&
         second.source_clauses == encoding.clauses.size() &&
         second.published_cover == std::vector<int32_t>({1, 2, 4}));

  // Every retained source is checked again immediately before commit.
  assert(kernel_delete_binary(kernel, kernel_internalize(kernel, -1),
                              kernel_internalize(kernel, -2)));
  apply.cover_vertices = {1, 3, 4};
  apply.action_id = 19;
  apply.semantic_id = 31;
  const auto stale = assert_failed_unchanged(kernel, apply);
  assert(stale.status == IndependentSetLiveStatus::Stale &&
         stale.source_scan_work > 0);
  std::fclose(records);
}

// A loaded encoding with a fresh journal.
struct Loaded {
  Kernel kernel;
  FILE *records = std::tmpfile();
  explicit Loaded(const Encoding &encoding) {
    load(kernel, encoding);
    assert(records);
  }
  ~Loaded() { std::fclose(records); }
  IndependentSetLiveResult declines(const IndependentSetLiveRequest &apply) {
    return assert_failed_unchanged(kernel, apply);
  }
  IndependentSetLiveResult
  declines_cover(std::vector<uint32_t> cover = {1, 2, 3}) {
    return declines(request(records, std::move(cover)));
  }
};

void declines_under_pb_proof(bool bound_maps) {
  Loaded f(make_encoding(4, 2, {{1, 2}}));
  TemporaryFile file;
  Proof proof;
  assert(proof.open(file.path.c_str(), ProofFormat::PbpStreaming));
  proof.finish_input_transaction();
  assert(proof.begin_pb());
  if (bound_maps) {
    proof.bind_variable_map(&f.kernel.internal_to_external);
    proof.bind_external_variable_map(&f.kernel.external_to_internal);
  }
  f.kernel.proof = &proof;
  assert(f.declines_cover().status ==
         IndependentSetLiveStatus::UnsupportedProof);
  f.kernel.proof = nullptr;
  if (bound_maps)
    proof.flush();
  else
    assert(proof.finish_pb());
}

void negative_boundaries() {
  const Encoding encoding = make_encoding(4, 2, {{1, 2}});
  {
    Loaded f(encoding);
    auto capture_request = request(f.records);
    capture_request.maximum_recognition_work = 2;
    IndependentSetOwnerSnapshot owner;
    owner.external_variables = 77;
    const auto capture = independent_set_live_capture_owner_detailed(
        f.kernel, capture_request, owner);
    assert(capture.status == IndependentSetLiveStatus::ResourceExhausted &&
           capture.recognition_work <=
               capture_request.maximum_recognition_work &&
           capture.source_scan_work == capture.recognition_work &&
           !capture.theory_recognition_work &&
           owner.external_variables == 77 && owner.clauses.empty());
  }
  {
    Loaded f(make_encoding(3, 3, {{1, 2}}));
    assert(f.declines_cover({1}).status ==
           IndependentSetLiveStatus::Inapplicable);
  }
  {
    Loaded f(encoding);
    volatile sig_atomic_t stop = 1;
    f.kernel.stop = &stop;
    assert(f.declines_cover().status == IndependentSetLiveStatus::Interrupted);
    f.kernel.stop = nullptr;
  }
  {
    Loaded f(encoding);
    f.kernel.next_clause_id = uint64_t(INT64_MAX) + 1;
    f.declines_cover();
  }
  {
    Loaded f(encoding);
    auto apply = request(f.records);
    IndependentSetOwnerSnapshot owner;
    assert(independent_set_live_capture_owner(f.kernel, apply, owner));
    --owner.literals;
    apply.owner_snapshot = &owner;
    f.declines(apply);
  }
  {
    // The first guard poll and snapshot charge pass and the next charge is
    // interrupted: the transaction has no effect after work began.
    Loaded f(encoding);
    f.kernel.stop_fuzz = 2;
    f.kernel.stop_fuzz_state = UINT64_C(0xaa80754d1a1a8d4f);
    assert(f.declines_cover().status == IndependentSetLiveStatus::Interrupted);
    assert(f.kernel.stats.stop_fuzz_fires == 1);
    f.kernel.stop_fuzz = 0;
  }
  {
    Loaded f(encoding);
    assert(kernel_delete_binary(f.kernel, kernel_internalize(f.kernel, -1),
                                kernel_internalize(f.kernel, -2)));
    f.declines_cover();
  }
  {
    Loaded f(encoding);
    kernel_add_binary_clause(f.kernel, kernel_internalize(f.kernel, 1),
                             kernel_internalize(f.kernel, 4),
                             ++f.kernel.next_clause_id);
    assert(f.declines_cover().status == IndependentSetLiveStatus::NoStructure);
  }
  {
    Loaded f(encoding);
    f.declines_cover({1, 1, 3});
    f.declines_cover({1, 2});
    f.declines_cover({1, 2, 99});
    kernel_assign_decision(f.kernel, kernel_internalize(f.kernel, 1));
    assert(propagate_service(f.kernel) == ApplyResult::Ok);
    assert(f.declines_cover().status == IndependentSetLiveStatus::Inapplicable);
  }
  declines_under_pb_proof(true);
  declines_under_pb_proof(false);
}

} // namespace

int main() {
  positive_and_continuing(nullptr, ProofFormat::DsrAscii);
  positive_and_continuing("/tmp/independent-set-live-test.dsr",
                          ProofFormat::DsrAscii);
  retained_owner_repeated_covers();
  negative_boundaries();
  std::puts("test_independent_set_live OK");
  return 0;
}
