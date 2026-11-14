#include "unit_test.hpp"
#include "affine_formula_support.hpp"
#include "kernel_image.hpp"
#include "test_io.hpp"
#include "rules/semantic_extension/affine_live.hpp"

#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"

using namespace tactix::affine_semantic_extension;

static AffineLiveRequest request(FILE *records) {
  AffineLiveRequest result;
  result.image = {1000, 100, 1000, 1000000, 8, 16, 16, 4};
  result.maximum_snapshot_literals = 10000;
  result.maximum_snapshot_work = 100000;
  result.maximum_proof_additions = 100000;
  result.maximum_proof_deletions = 100000;
  result.maximum_journal_bytes = 1000000;
  result.action_id = 1;
  result.semantic_id = 7;
  result.records = records;
  return result;
}

// Run a request that must decline with status, leaving the kernel image and
// the journal untouched.
static AffineLiveResult decline(Kernel &k, const AffineLiveRequest &r,
                                AffineLiveStatus status) {
  const ::Image before = image(k);
  const auto result = derive_affine_live(k, r);
  assert(result.status == status && result.result == ApplyResult::NoOp &&
         !result.journal_bytes);
  assert(image(k) == before && r.records.tell() == 0);
  return result;
}

static void assert_model(const Kernel &k) {
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    ref += clause_actual_words(clause);
    if (clause->garbage()) continue;
    bool satisfied = false;
    for (Lit literal : *clause) satisfied |= k.value(literal) > 0;
    assert(satisfied);
  }
  for (Lit first = 0; first < 2 * k.nvars; ++first)
    for (const WatchEntry watch : k.watches.entries(first))
      if (watch.binary() && first < watch.other())
        assert(k.value(first) > 0 || k.value(watch.other()) > 0);
}

TEST(positive_and_continuing) {
  Kernel k;
  install(k, affine_formula());
  FILE *records = temporary_records();
  auto r = request(records);
  const auto result = derive_affine_live(k, r);
  assert(result.status == AffineLiveStatus::Applied &&
         result.result == ApplyResult::Ok);
  assert(result.equations == 2 && result.consequences == 1);
  assert(result.selected_sources == 2 && result.selected_premises == 32);
  assert((result.semantic_variables == std::vector<int32_t>{5, 6}));
  assert(result.rhs && !result.contradiction);
  assert(result.published == 2 && result.additions > 2 &&
         result.proof_addition_work == result.preflight_additions);
  assert(!result.propagation_pending);
  assert(kernel_binary_live(k, kernel_internalize(k, 5),
                            kernel_internalize(k, 6)));
  assert(kernel_binary_live(k, kernel_internalize(k, -5),
                            kernel_internalize(k, -6)));
  assert(k.nvars == 6 && kernel_num_output_vars(k) == 6);
  assert(position(records) > 0 &&
         result.journal_bytes == static_cast<uint64_t>(position(records)) &&
         result.journal_work == result.journal_bytes);

  FILE *repeat = temporary_records();
  r.records = repeat;
  decline(k, r, AffineLiveStatus::NoStructure);

  for (int32_t external : {-1, -2, -3, -4}) {
    if (!k.value(kernel_internalize(k, external)))
      kernel_assign_decision(k, kernel_internalize(k, external));
    assert(propagate_service(k) == ApplyResult::Ok);
  }
  assert(k.value(kernel_internalize(k, -5)) > 0);
  assert(k.value(kernel_internalize(k, 6)) > 0);
  kernel_extend_model(k);
  assert_model(k);
  std::fclose(repeat);
  std::fclose(records);
}

// Each resource bound declines at its own boundary.
TEST(resource_declines) {
  for (int mode : {0, 1, 2}) {
    Kernel k;
    install(k, affine_formula());
    FILE *records = temporary_records();
    auto r = request(records);
    if (!mode) r.maximum_proof_additions = 1;
    else if (mode == 1) r.maximum_journal_bytes = 1;
    else r.image.maximum_elimination_work = 1;
    const auto result = decline(k, r, AffineLiveStatus::ResourceExhausted);
    if (mode == 1)
      assert(result.journal_work > r.maximum_journal_bytes);
    if (mode == 0)
      assert(result.proof_addition_work == r.maximum_proof_additions);
    if (mode == 2)
      assert(result.snapshot_work && result.recognition_work <= 1);
    std::fclose(records);
  }
}

TEST(interruption_boundaries) {
  bool saw_after_snapshot = false, saw_after_preflight = false;
  for (uint64_t seed = 1; seed <= 512 && !saw_after_preflight; ++seed) {
    Kernel k;
    install(k, affine_formula());
    k.stop_fuzz = 127;
    k.stop_fuzz_state = seed;
    FILE *records = temporary_records();
    const ::Image before = image(k);
    const auto result = derive_affine_live(k, request(records));
    if (result.result == ApplyResult::NoOp) {
      assert(result.status == AffineLiveStatus::Interrupted &&
             !result.journal_bytes);
      assert(image(k) == before && position(records) == 0);
      saw_after_snapshot |= result.snapshot_clauses != 0;
      saw_after_preflight |= result.preflight_additions != 0;
    } else {
      assert(result.status == AffineLiveStatus::Applied &&
             result.result == ApplyResult::Ok && result.published == 2);
    }
    std::fclose(records);
  }
  assert(saw_after_snapshot && saw_after_preflight);
}

TEST(stale_snapshots) {
  {
    // A deleted binary source.
    Kernel k;
    install(k, xor_cnf({{{1, 2}, false}, {{1, 3}, true}}), 3);
    FILE *records = temporary_records();
    auto r = request(records);
    AffineLiveSnapshot snapshot;
    assert(capture_affine_live_snapshot(k, r, snapshot));
    assert(snapshot.sources.size() == 4);
    const auto &lost = snapshot.sources[0];
    assert(lost.kind == AffineSourceKind::Binary);
    assert(kernel_delete_binary(k, lost.first, lost.second));
    assert(!affine_live_sources_valid(k, snapshot, {lost.occurrence}));
    r.snapshot = &snapshot;
    decline(k, r, AffineLiveStatus::Stale);
    std::fclose(records);
  }
  {
    // A large source moved by garbage collection, or a reference into one.
    Kernel k;
    install(k, affine_formula());
    FILE *records = temporary_records();
    auto r = request(records);
    AffineLiveSnapshot snapshot;
    assert(capture_affine_live_snapshot(k, r, snapshot));
    const auto large = std::find_if(
        snapshot.sources.begin(), snapshot.sources.end(), [](const auto &source) {
          return source.kind == AffineSourceKind::Large;
        });
    assert(large != snapshot.sources.end());
    const size_t index = static_cast<size_t>(large - snapshot.sources.begin());

    AffineLiveSnapshot interior = snapshot;
    ++interior.sources[index].ref;
    assert(!affine_live_sources_valid(
        k, interior, {interior.sources[index].occurrence}));

    const uint64_t generation = k.arena_generation;
    kernel_collect_garbage(k);
    assert(k.arena_generation == generation + 1);
    assert(!affine_live_sources_valid(
        k, snapshot, {snapshot.sources[index].occurrence}));
    r.snapshot = &snapshot;
    decline(k, r, AffineLiveStatus::Stale);
    std::fclose(records);
  }
}

// A retained snapshot's literal and work counters are recomputed from its
// premises, both for accounting and for the snapshot-literal bound.
TEST(cached_snapshot_counters) {
  for (bool bounded : {false, true}) {
    Kernel k;
    install(k, affine_formula());
    FILE *records = temporary_records();
    auto r = request(records);
    AffineLiveSnapshot snapshot;
    assert(capture_affine_live_snapshot(k, r, snapshot));
    uint64_t literals = 0, payload_work = 0;
    for (const auto &premise : snapshot.premises) {
      literals += premise.literals.size();
      payload_work += premise.literals.size() + 1;
    }
    snapshot.literals = 0;
    snapshot.work = 0;
    r.snapshot = &snapshot;
    if (bounded) {
      r.maximum_snapshot_literals = literals - 1;
      decline(k, r, AffineLiveStatus::ResourceExhausted);
    } else {
      const auto result = derive_affine_live(k, r);
      assert(result.status == AffineLiveStatus::Applied &&
             result.result == ApplyResult::Ok);
      assert(result.snapshot_literals == literals &&
             result.snapshot_work == payload_work);
    }
    std::fclose(records);
  }
}

TEST(uninstalled_target_cleanup) {
  Kernel k;
  install(k, affine_formula());
  add_clause(k, {5, 6});
  FILE *records = temporary_records();
  const auto result = derive_affine_live(k, request(records));
  assert(result.status == AffineLiveStatus::Applied &&
         result.result == ApplyResult::Ok && result.published == 1);
  assert(result.preflight_deletions == result.deletions);
  assert(written_bytes(records).find("affine-proof-delete-uninstalled") !=
         std::string::npos);
  assert(kernel_binary_live(k, kernel_internalize(k, 5),
                            kernel_internalize(k, 6)));
  assert(kernel_binary_live(k, kernel_internalize(k, -5),
                            kernel_internalize(k, -6)));
  std::fclose(records);
}

TEST(false_unit) {
  Kernel k;
  install(k, xor_cnf({{{1, 2}, false}, {{1, 2, 3}, true}}), 3);
  kernel_assign_unit(k, kernel_internalize(k, -3));
  assert(propagate_service(k) == ApplyResult::Ok);
  FILE *records = temporary_records();
  const auto result = derive_affine_live(k, request(records));
  assert(result.status == AffineLiveStatus::Applied &&
         result.result == ApplyResult::Unsat && !result.contradiction);
  assert((result.semantic_variables == std::vector<int32_t>{3}));
  assert(result.rhs && result.published == 0 && k.inconsistent);
  assert(result.preflight_additions == result.additions);
  std::fclose(records);
}

TEST(status_and_decline_effort) {
  for (int mode = 0; mode < 3; ++mode) {
    Kernel k;
    install(k, affine_formula());
    FILE *records = temporary_records();
    auto r = request(records);
    AffineLiveSnapshot snapshot;
    if (mode == 0) {
      r.image.maximum_arity = 9;
      const auto result = decline(k, r, AffineLiveStatus::Invalid);
      assert(!result.snapshot_work && !result.recognition_work &&
             !result.journal_work);
    } else if (mode == 1) {
      r.maximum_snapshot_work = 1;
      const auto result = decline(k, r, AffineLiveStatus::ResourceExhausted);
      assert(result.snapshot_work == 1 && !result.recognition_work &&
             !result.journal_work);
    } else {
      assert(capture_affine_live_snapshot(k, r, snapshot));
      snapshot.premises.front().occurrence = 99;
      r.snapshot = &snapshot;
      assert(decline(k, r, AffineLiveStatus::Invalid).snapshot_clauses ==
             snapshot.premises.size());
    }
    std::fclose(records);
  }
}
