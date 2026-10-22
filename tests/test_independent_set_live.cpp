#undef NDEBUG
#include "rules/semantic_extension/independent_set_live.hpp"

#include "frontend/parse.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"

#include <algorithm>
#include <cassert>
#include <climits>
#include <csignal>
#include <cstdio>
#include <initializer_list>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace tactix;

namespace {

struct Encoding {
  uint32_t vertices = 0;
  uint32_t target = 0;
  uint32_t variables = 0;
  std::vector<std::vector<int32_t>> layers;
  std::vector<std::pair<uint32_t, uint32_t>> edges;
  std::vector<std::vector<int32_t>> clauses;
};

Encoding make_encoding(uint32_t vertices, uint32_t target,
                       std::vector<std::pair<uint32_t, uint32_t>> edges) {
  Encoding result;
  result.vertices = vertices;
  result.target = target;
  result.edges = std::move(edges);
  result.layers.resize(vertices + 1);
  int32_t next = static_cast<int32_t>(vertices + 1);
  for (uint32_t layer = 1; layer <= vertices; ++layer)
    for (uint32_t state = 0; state <= std::min(layer, target); ++state)
      result.layers[layer].push_back(next++);
  result.variables = static_cast<uint32_t>(next - 1);
  const auto add = [&](std::initializer_list<int32_t> literals) {
    result.clauses.emplace_back(literals);
  };
  for (auto [first, second] : result.edges)
    add({-static_cast<int32_t>(first), -static_cast<int32_t>(second)});
  add({1, result.layers[1][0]});
  add({-1, result.layers[1][1]});
  for (uint32_t layer = 2; layer <= vertices; ++layer) {
    const auto &previous = result.layers[layer - 1];
    const auto &current = result.layers[layer];
    for (uint32_t state = 0; state < previous.size(); ++state) {
      add({-previous[state], static_cast<int32_t>(layer), current[state]});
      add({-previous[state], -static_cast<int32_t>(layer),
           current[std::min(state + 1, target)]});
    }
    for (uint32_t first = 0; first < current.size(); ++first)
      for (uint32_t second = first + 1; second < current.size(); ++second)
        add({-current[first], -current[second]});
  }
  const auto &terminal = result.layers[vertices];
  for (uint32_t state = 0; state < target; ++state)
    add({-terminal[state], terminal[target]});
  result.clauses.push_back(terminal);
  return result;
}

std::string dimacs(const Encoding &encoding) {
  std::ostringstream output;
  output << "p cnf " << encoding.variables << ' ' << encoding.clauses.size()
         << '\n';
  for (const auto &clause : encoding.clauses) {
    for (int32_t literal : clause)
      output << literal << ' ';
    output << "0\n";
  }
  return output.str();
}

void load(Kernel &kernel, const Encoding &encoding) {
  const std::string text = dimacs(encoding);
  FILE *input = std::tmpfile();
  assert(input);
  assert(std::fwrite(text.data(), 1, text.size(), input) == text.size());
  std::rewind(input);
  assert(parse_dimacs(input, kernel) == nullptr);
  std::fclose(input);
  assert(kernel.trail.empty() && kernel_propagation_fixpoint(kernel));
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

bool operator==(const KernelImage &left, const KernelImage &right) {
  return left.nvars == right.nvars &&
         left.next_clause_id == right.next_clause_id &&
         left.arena_words == right.arena_words &&
         left.external_extent == right.external_extent &&
         left.trail == right.trail &&
         left.startup_open == right.startup_open &&
         left.arena_generation == right.arena_generation &&
         left.internal_to_external == right.internal_to_external &&
         left.external_to_internal == right.external_to_internal &&
         left.watch_sizes == right.watch_sizes;
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
  const long record_position = std::ftell(candidate.records);
  assert(record_position >= 0);
  const IndependentSetLiveResult result =
      independent_set_live_apply(kernel, candidate);
  assert_declined(result);
  assert(image(kernel) == before);
  assert(std::ftell(candidate.records) == record_position);
  return result;
}

std::string read_all(FILE *file) {
  assert(!std::fflush(file));
  const long end = std::ftell(file);
  assert(end >= 0);
  std::rewind(file);
  std::string result(static_cast<size_t>(end), '\0');
  assert(std::fread(result.data(), 1, result.size(), file) == result.size());
  std::fseek(file, end, SEEK_SET);
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
  const std::string journal = read_all(records);
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

  // A capture and its first cached apply share one hard recognition ceiling.
  // With no room after capture, application declines without retaining any
  // formula, proof or journal effect and reports exactly the spent capture.
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

void negative_boundaries() {
  const Encoding encoding = make_encoding(4, 2, {{1, 2}});
  {
    Kernel kernel;
    load(kernel, encoding);
    FILE *records = std::tmpfile();
    assert(records);
    auto capture_request = request(records);
    capture_request.maximum_recognition_work = 2;
    IndependentSetOwnerSnapshot owner;
    owner.external_variables = 77;
    const auto capture = independent_set_live_capture_owner_detailed(
        kernel, capture_request, owner);
    assert(capture.status == IndependentSetLiveStatus::ResourceExhausted &&
           capture.recognition_work <=
               capture_request.maximum_recognition_work &&
           capture.source_scan_work == capture.recognition_work &&
           !capture.theory_recognition_work &&
           owner.external_variables == 77 && owner.clauses.empty());
    std::fclose(records);
  }
  {
    const Encoding unit_encoding = make_encoding(3, 3, {{1, 2}});
    Kernel kernel;
    load(kernel, unit_encoding);
    FILE *records = std::tmpfile();
    assert(records);
    auto apply = request(records, {1});
    const auto unit = assert_failed_unchanged(kernel, apply);
    assert(unit.status == IndependentSetLiveStatus::Inapplicable);
    std::fclose(records);
  }
  {
    Kernel kernel;
    load(kernel, encoding);
    FILE *records = std::tmpfile();
    assert(records);
    auto apply = request(records);
    volatile sig_atomic_t stop = 1;
    kernel.stop = &stop;
    const auto interrupted = assert_failed_unchanged(kernel, apply);
    assert(interrupted.status == IndependentSetLiveStatus::Interrupted);
    kernel.stop = nullptr;
    std::fclose(records);
  }
  {
    Kernel kernel;
    load(kernel, encoding);
    FILE *records = std::tmpfile();
    assert(records);
    auto apply = request(records);
    kernel.next_clause_id = uint64_t(INT64_MAX) + 1;
    assert_failed_unchanged(kernel, apply);
    std::fclose(records);
  }
  {
    Kernel kernel;
    load(kernel, encoding);
    FILE *records = std::tmpfile();
    assert(records);
    auto apply = request(records);
    IndependentSetOwnerSnapshot owner;
    assert(independent_set_live_capture_owner(kernel, apply, owner));
    --owner.literals;
    apply.owner_snapshot = &owner;
    assert_failed_unchanged(kernel, apply);
    std::fclose(records);
  }
  {
    // The first guard poll and first snapshot charge pass; the next charge is
    // interrupted.  This checks the zero-effect transaction after work began,
    // rather than only the entry guard.
    Kernel kernel;
    load(kernel, encoding);
    kernel.stop_fuzz = 2;
    kernel.stop_fuzz_state = UINT64_C(0xaa80754d1a1a8d4f);
    FILE *records = std::tmpfile();
    assert(records);
    const auto interrupted =
        assert_failed_unchanged(kernel, request(records));
    assert(interrupted.status == IndependentSetLiveStatus::Interrupted);
    assert(kernel.stats.stop_fuzz_fires == 1);
    kernel.stop_fuzz = 0;
    std::fclose(records);
  }
  {
    Kernel kernel;
    load(kernel, encoding);
    FILE *records = std::tmpfile();
    assert(records);
    auto apply = request(records);
    assert(kernel_delete_binary(kernel, kernel_internalize(kernel, -1),
                                kernel_internalize(kernel, -2)));
    assert_failed_unchanged(kernel, apply);
    std::fclose(records);
  }
  {
    Kernel kernel;
    load(kernel, encoding);
    const Lit extra[] = {kernel_internalize(kernel, 1),
                         kernel_internalize(kernel, 4)};
    kernel_add_binary_clause(kernel, extra[0], extra[1],
                             ++kernel.next_clause_id);
    FILE *records = std::tmpfile();
    assert(records);
    const auto contaminated =
        assert_failed_unchanged(kernel, request(records));
    assert(contaminated.status == IndependentSetLiveStatus::NoStructure);
    std::fclose(records);
  }
  {
    Kernel kernel;
    load(kernel, encoding);
    FILE *records = std::tmpfile();
    assert(records);
    assert_failed_unchanged(kernel, request(records, {1, 1, 3}));
    assert_failed_unchanged(kernel, request(records, {1, 2}));
    assert_failed_unchanged(kernel, request(records, {1, 2, 99}));
    kernel_assign_decision(kernel, kernel_internalize(kernel, 1));
    assert(propagate_service(kernel) == ApplyResult::Ok);
    const auto assigned =
        assert_failed_unchanged(kernel, request(records));
    assert(assigned.status == IndependentSetLiveStatus::Inapplicable);
    std::fclose(records);
  }
  {
    Kernel kernel;
    load(kernel, encoding);
    Proof proof;
    assert(proof.open("/tmp/independent-set-live-decline.lrat",
                      ProofFormat::Lrat));
    proof.bind_variable_map(&kernel.internal_to_external);
    proof.bind_external_variable_map(&kernel.external_to_internal);
    kernel.proof = &proof;
    FILE *records = std::tmpfile();
    assert(records);
    const auto unsupported =
        assert_failed_unchanged(kernel, request(records));
    assert(unsupported.status == IndependentSetLiveStatus::UnsupportedProof);
    kernel.proof = nullptr;
    proof.flush();
    std::fclose(records);
  }
  {
    Kernel kernel;
    load(kernel, encoding);
    Proof proof;
    assert(proof.open("/tmp/independent-set-live-decline.pbp",
                      ProofFormat::PbpStreaming));
    proof.finish_input_transaction();
    assert(proof.begin_pb());
    kernel.proof = &proof;
    FILE *records = std::tmpfile();
    assert(records);
    const auto unsupported =
        assert_failed_unchanged(kernel, request(records));
    assert(unsupported.status == IndependentSetLiveStatus::UnsupportedProof);
    kernel.proof = nullptr;
    assert(proof.finish_pb());
    std::fclose(records);
  }
}

void add_rup(Proof &proof, Kernel &kernel, std::vector<int32_t> clause) {
  assert(proof.add_external_rup(++kernel.next_clause_id,
                                clause.empty() ? nullptr : clause.data(),
                                clause.size()));
}

void emit_checked_refutation(const char *cnf_path, const char *proof_path,
                             const char *records_path) {
  const Encoding encoding =
      make_encoding(3, 2, {{1, 2}, {1, 3}, {2, 3}});
  const std::string text = dimacs(encoding);
  FILE *cnf = std::fopen(cnf_path, "w");
  assert(cnf);
  assert(std::fwrite(text.data(), 1, text.size(), cnf) == text.size());
  assert(!std::fclose(cnf));

  Kernel kernel;
  load(kernel, encoding);
  Proof proof;
  assert(proof.open(proof_path, ProofFormat::DsrAscii));
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  kernel.proof = &proof;
  FILE *records = std::fopen(records_path, "w");
  assert(records);
  IndependentSetLiveRequest apply = request(records, {1, 2});
  IndependentSetOwnerSnapshot owner;
  assert(independent_set_live_capture_owner(kernel, apply, owner));
  const IndependentSetLiveResult result =
      independent_set_live_apply(kernel, apply);
  assert(result.result == ApplyResult::Ok && result.published_cover ==
                                                std::vector<int32_t>({1, 2}));

  apply.owner_snapshot = &owner;
  apply.cover_vertices = {1, 3};
  ++apply.action_id;
  ++apply.semantic_id;
  assert(independent_set_live_apply(kernel, apply).result == ApplyResult::Ok);
  apply.cover_vertices = {2, 3};
  ++apply.action_id;
  ++apply.semantic_id;
  assert(independent_set_live_apply(kernel, apply).result == ApplyResult::Ok);
  add_rup(proof, kernel, {2, -3});
  add_rup(proof, kernel, {-3});
  add_rup(proof, kernel, {1});
  add_rup(proof, kernel, {2});
  add_rup(proof, kernel, {});
  proof.flush();
  kernel.proof = nullptr;
  assert(!std::fclose(records));
}

} // namespace

int main(int argc, char **argv) {
  if (argc == 4) {
    emit_checked_refutation(argv[1], argv[2], argv[3]);
    return 0;
  }
  assert(argc == 1);
  positive_and_continuing(nullptr, ProofFormat::DsrAscii);
  positive_and_continuing("/tmp/independent-set-live-test.dsr",
                          ProofFormat::DsrAscii);
  positive_and_continuing("/tmp/independent-set-live-test.drat",
                          ProofFormat::DratAscii);
  retained_owner_repeated_covers();
  negative_boundaries();
  std::puts("test_independent_set_live OK");
  return 0;
}
