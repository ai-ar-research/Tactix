#undef NDEBUG
#include "frontend/parse.hpp"
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "policies/default_policy.hpp"
#include "proof/proof.hpp"
#include "rules/bce/bce.hpp"
#include "rules/bve/bve.hpp"
#include "rules/propagate/propagate.hpp"
#include "session/weakening_history.hpp"
#include "session/query_history.hpp"
#include "session/query_kernel.hpp"

#include <algorithm>
#include <cassert>
#include <set>
#include <memory>
#include <string>
#include <type_traits>

using namespace tactix;
using namespace tactix::session;
static_assert(!std::is_copy_constructible_v<WeakeningHistory>);
static_assert(!std::is_move_constructible_v<WeakeningHistory>);

static constexpr WeakeningLimits bounds{256, 4096, 1024};

static void input(Kernel &k, std::initializer_list<int32_t> raw) {
  std::vector<Lit> clause;
  for (int32_t lit : raw) {
    if (lit) clause.push_back(internalize(lit));
    else { ingest_clause(k, clause); clause.clear(); }
  }
  assert(clause.empty());
  finish_input(k);
}

static bool equal(std::vector<int32_t> actual, std::initializer_list<int32_t> expected) {
  std::vector<int32_t> sorted(expected);
  std::sort(actual.begin(), actual.end());
  std::sort(sorted.begin(), sorted.end());
  return actual == sorted;
}

static void eliminate(Kernel &k, int32_t lit) {
  const auto &rule = tactix_rules()[tactix_rule_id("BVE")];
  assert(rule_guard(rule, k));
  assert(rule.apply_instance(k, lit_instance(k, kernel_internalize(k, lit))) == ApplyResult::Ok);
}

static void full_large_sources_survive_compaction() {
  Kernel k;
  WeakeningHistory history(k, bounds);
  ScopedPopulationObserverBinding binding(k, history);
  input(k, {1, 2, 4, 0, -1, 3, 5, 0, -4, 0});
  assert(history.capture_complete());
  eliminate(k, 1);
  assert(history.capture_complete() && history.pending() == 0);
  assert(history.records().size() == 2);
  const auto &positive = history.records()[0];
  const auto &negative = history.records()[1];
  assert(equal(positive.clause, {1, 2, 4}) && equal(positive.witness, {1}));
  assert(equal(negative.clause, {-1, 3, 5}) && equal(negative.witness, {-1}));
  assert(positive.sources.size() == 1 && positive.sources[0].proof_id == 1);
  assert(negative.sources.size() == 1 && negative.sources[0].proof_id == 2);
  assert(positive.family == WeakeningFamily::Bve && positive.order < negative.order);
  // The saved first clause has three literals, but its rho residual has two.
  assert(k.reconstruction.size() == 7);
  assert(kernel_compact_variables(k, true));
  assert(history.capture_complete());
  assert(equal(history.records()[0].clause, {1, 2, 4}));
  assert(history.records()[0].sources[0].proof_id == 1);
  kernel_check_invariants(k);
}

static void binary_occurrence_ids_ignore_sentinel() {
  Kernel k;
  WeakeningHistory history(k, bounds);
  ScopedPopulationObserverBinding binding(k, history);
  input(k, {1, 2, 0, -1, 3, 0});
  const uint64_t extra_id = ++k.next_clause_id;
  kernel_add_binary_clause(k, internalize(1), internalize(2), extra_id);
  assert(kernel_binary_id(k, internalize(-1), internalize(3)) == 1);
  eliminate(k, 1);
  assert(history.capture_complete() && history.records().size() == 3);
  std::set<uint64_t> ids, occurrences;
  for (const auto &record : history.records()) {
    assert(record.sources.size() == 1);
    ids.insert(record.sources[0].proof_id);
    occurrences.insert(record.sources[0].occurrence);
  }
  assert(ids == std::set<uint64_t>({1, 2, extra_id}));
  assert(occurrences.size() == 3);
  kernel_check_invariants(k);
}

static void bce_batch_and_duplicate_sources() {
  Kernel k;
  WeakeningHistory history(k, bounds);
  ScopedPopulationObserverBinding binding(k, history);
  input(k, {1, 2, 3, 0, 1, 4, 0});
  const uint64_t extra_id = ++k.next_clause_id;
  kernel_add_binary_clause(k, internalize(1), internalize(4), extra_id);
  assert(bce_try(k, internalize(1)) == ApplyResult::Ok);
  assert(history.capture_complete() && history.records().size() == 2);
  std::set<uint64_t> ids;
  size_t source_count = 0;
  for (const auto &record : history.records()) {
    assert(record.family == WeakeningFamily::Bce && equal(record.witness, {1}));
    source_count += record.sources.size();
    for (const auto &source : record.sources) ids.insert(source.proof_id);
  }
  assert(source_count == 3 && ids == std::set<uint64_t>({1, 2, extra_id}));
  assert(history.live_binary_copies() == 0 && history.pending() == 0);
  assert(bce_try(k, internalize(1)) == ApplyResult::NoOp);
  assert(history.records().size() == 2 && history.capture_complete());
  kernel_check_invariants(k);
}

static void many_reserved_copies_survive_storage_compaction() {
  Kernel k;
  WeakeningHistory history(k, bounds);
  ScopedPopulationObserverBinding binding(k, history);
  input(k, {1, 2, 0, -1, 3, 0});
  for (unsigned copy = 0; copy < 100; ++copy)
    kernel_add_binary_clause(k, internalize(1), internalize(2), ++k.next_clause_id);
  eliminate(k, 1);
  assert(history.capture_complete() && history.records().size() == 102);
  std::set<uint64_t> ids;
  for (const auto &record : history.records()) {
    assert(record.sources.size() == 1);
    ids.insert(record.sources[0].proof_id);
  }
  assert(ids.size() == 102 && *ids.begin() == 1 && *ids.rbegin() == 102);
}

static void cancelled_preparation_and_uncommitted_delete() {
  Kernel k;
  WeakeningHistory history(k, bounds);
  ScopedPopulationObserverBinding binding(k, history);
  input(k, {1, 2, 0});
  const Lit clause[2] = {internalize(1), internalize(2)};
  {
    ScopedWeakeningCapture capture(k, WeakeningFamily::Bce, clause, 2,
        internalize(1), 0, true, true);
    assert(history.pending() == 1 && history.records().empty());
  }
  assert(history.capture_complete() && history.pending() == 0 && history.records().empty());
  assert(bce_try(k, internalize(1)) == ApplyResult::Ok && history.capture_complete());
  assert(history.records().size() == 1);

  Kernel other;
  WeakeningHistory missing(other, bounds);
  ScopedPopulationObserverBinding other_binding(other, missing);
  input(other, {1, 2, 0});
  {
    ScopedWeakeningCapture capture(other, WeakeningFamily::Bce, clause, 2,
        internalize(1), 0, true, true);
    // A dishonest/missing deletion cannot turn staging into a durable record.
    capture.commit();
  }
  assert(!missing.capture_complete() && missing.records().empty());
}

static void bce_semantic_choice() {
  Kernel k;
  WeakeningHistory history(k, bounds);
  ScopedPopulationObserverBinding binding(k, history);
  input(k, {1, 2, 0, 1, 3, 0});
  kernel_add_binary_clause(k, internalize(1), internalize(2), ++k.next_clause_id);
  const auto &rule = tactix_rules()[tactix_rule_id("BCE")];
  uint64_t choices[8];
  assert(rule.semantic_instances(k, choices, 8) == 4);
  assert(rule.apply_semantic_instance(k, choices[0]) == ApplyResult::Ok);
  assert(history.capture_complete() && history.records().size() == 1);
  assert(history.records()[0].sources.size() == 2);
  assert(kernel_binary_live(k, internalize(1), internalize(3)));
}

static void staged_binary_replacement() {
  Kernel k;
  WeakeningHistory history(k, bounds);
  ScopedPopulationObserverBinding binding(k, history);
  input(k, {1, 2, 3, 0, -3, 0});
  Ref source = INVALID_REF;
  for (const auto [ref, clause] : arena_records(k.arena))
    if (!clause->garbage()) source = ref;
  assert(source != INVALID_REF);
  const Lit replacement[2] = {internalize(1), internalize(2)};
  // Root -3 licenses this shortening. Exercise the already audited native
  // replacement transaction used by the rules, then weaken its binary image.
  const uint64_t replacement_id = ++k.next_clause_id;
  assert(kernel_replace_clause(k, clause_at(k.arena, source), replacement_id,
      replacement, 2, false, 0, nullptr) == ReplaceOutcome::Binary);
  assert(history.capture_complete() && history.live_binary_copies() == 1);
  assert(bce_try(k, internalize(1)) == ApplyResult::Ok);
  assert(history.capture_complete() && history.records().size() == 1);
  assert(history.records()[0].sources[0].proof_id == replacement_id);
}

static void full_width_id_and_capture_gaps() {
  for (uint64_t id : {uint64_t(INT32_MAX), uint64_t(UINT32_MAX),
                      uint64_t(UINT32_MAX) + 1, uint64_t(INT64_MAX)}) {
    Kernel k;
    WeakeningHistory history(k, bounds);
    ScopedPopulationObserverBinding binding(k, history);
    k.next_clause_id = id - 1;
    input(k, {1, 2, 3, 0});
    assert(bce_try(k, internalize(1)) == ApplyResult::Ok);
    assert(history.capture_complete() && history.records().size() == 1);
    assert(history.records()[0].sources[0].proof_id == id);
  }
  Kernel k;
  WeakeningHistory bounded(k, {1, 8, 8});
  ScopedPopulationObserverBinding binding(k, bounded);
  input(k, {1, 2, 0, 1, 3, 0});
  assert(bce_try(k, internalize(1)) == ApplyResult::Ok);
  assert(!bounded.capture_complete()); // exhaustion never narrows the legal rule
  assert(k.stats.bce_deleted == 2 && !k.reconstruction.empty());

  Kernel unsupported;
  WeakeningHistory history(unsupported, bounds);
  ScopedPopulationObserverBinding unsupported_binding(unsupported, history);
  input(unsupported, {1, 2, 0});
  const Lit rest = internalize(2);
  kernel_push_witness(unsupported, internalize(1), &rest, 1);
  assert(!history.capture_complete());
  assert(bce_try(unsupported, internalize(1)) == ApplyResult::Ok);
  assert(!history.capture_complete() && history.records().empty());
}

static void mismatched_reconstruction_is_a_gap() {
  Kernel k;
  WeakeningHistory history(k, bounds);
  ScopedPopulationObserverBinding binding(k, history);
  input(k, {1, 2, 0});
  const Lit clause[2] = {internalize(1), internalize(2)};
  ScopedWeakeningCapture capture(k, WeakeningFamily::Bce, clause, 2, clause[0], 0, true, true);
  assert(kernel_delete_binary_all_copies(k, clause[0], clause[1]));
  kernel_push_witness(k, clause[1], clause, 1); // different repair, same word count
  capture.commit();
  assert(!history.capture_complete() && history.records().empty());
}

static void lost_binding_is_a_permanent_gap() {
  Kernel k;
  WeakeningHistory history(k, bounds);
  ScopedPopulationObserverBinding binding(k, history);
  input(k, {1, 2, 0});
  assert(history.capture_complete());
  kernel_bind_population_observer(k, nullptr);
  kernel_add_binary_clause(k, internalize(1), internalize(2), ++k.next_clause_id);
  kernel_bind_population_observer(k, &history);
  assert(!history.capture_complete());
}

static void proof_fixture(const std::string &prefix, const std::string &mode,
                          bool capture, bool unsat) {
  QueryHistory input_history;
  const std::vector<int32_t> raw = {1,2,0,-1,3,0,4,5,0,6,7,0,-6,7,0,6,-7,0};
  for (auto literal : raw) assert(input_history.add(literal) == InputStatus::Ok);
  if (unsat) for (int literal : {-6,-7,0}) assert(input_history.add(literal) == InputStatus::Ok);
  assert(input_history.begin_query() == InputStatus::Ok);
  FILE *cnf = std::fopen((prefix + ".cnf").c_str(), "w");
  assert(cnf && input_history.write_query_dimacs(1, cnf) && std::fclose(cnf) == 0);
  Kernel k;
  ScopedHeuristics guidance(k);
  WeakeningHistory history(k, bounds);
  Proof writer;
  assert(mode == "none" ? writer.open_nowhere() : writer.open((prefix+".proof").c_str(),
      mode == "pbp" ? ProofFormat::PbpStreaming : ProofFormat::DsrAscii));
  k.proof = &writer;
  writer.bind_variable_map(&k.internal_to_external);
  writer.bind_external_variable_map(&k.external_to_internal);
  std::unique_ptr<ScopedPopulationObserverBinding> binding;
  if (capture) binding = std::make_unique<ScopedPopulationObserverBinding>(k, history);
  declare_input(k, 7, input_history.clauses());
  std::vector<Lit> clause;
  for (int32_t literal : input_history.input()) {
    if (literal) clause.push_back(internalize(literal));
    else { ingest_clause(k, clause); clause.clear(); }
  }
  finish_input(k);
  eliminate(k, 1);
  assert(bce_try(k, internalize(4)) == ApplyResult::Ok);
  if (capture) assert(history.capture_complete() && history.records().size() == 3);
  const int result = default_solve(k);
  assert(result == (unsat ? 20 : 10));
  if (!unsat) {
    kernel_extend_model(k);
    assert(input_history.check_model(1, [&](int32_t variable) {
      return k.model_values[internalize(variable)];
    }).status == ModelStatus::Valid);
    FILE *model = std::fopen((prefix+".model").c_str(), "w");
    assert(model);
    for (int variable = 1; variable <= 7; ++variable)
      assert(std::fprintf(model, "%d ", k.model_values[internalize(variable)] > 0
          ? variable : -variable) > 0);
    assert(std::fprintf(model, "0\n") > 0 && std::fclose(model) == 0);
  }
  writer.flush();
  std::printf("result=%d conflicts=%llu decisions=%llu bve=%llu bce=%llu vars=%u rho=%zu\n",
      result, (unsigned long long)k.stats.conflicts, (unsigned long long)k.stats.decisions,
      (unsigned long long)k.stats.bve_eliminated, (unsigned long long)k.stats.bce_deleted,
      k.nvars, k.reconstruction.size());
  binding.reset();
  k.proof = nullptr;
}

int main(int argc, char **argv) {
  if (argc == 5) {
    proof_fixture(argv[1], argv[2], std::string(argv[3]) == "capture",
                  std::string(argv[4]) == "unsat");
    return 0;
  }
  assert(argc == 1);
  full_large_sources_survive_compaction();
  binary_occurrence_ids_ignore_sentinel();
  bce_batch_and_duplicate_sources();
  many_reserved_copies_survive_storage_compaction();
  cancelled_preparation_and_uncommitted_delete();
  bce_semantic_choice();
  staged_binary_replacement();
  full_width_id_and_capture_gaps();
  mismatched_reconstruction_is_a_gap();
  lost_binding_is_a_permanent_gap();
}
