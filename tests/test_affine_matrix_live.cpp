#undef NDEBUG
#include "proof/proof.hpp"
#include "rules/semantic_extension/affine_matrix_live.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <unistd.h>

using namespace tactix;
using namespace tactix::affine_semantic_extension;

static void add_clause(Kernel &kernel, const std::vector<int32_t> &external) {
  std::vector<Lit> clause;
  for (int32_t literal : external) {
    const Lit internal = kernel_internalize(kernel, literal);
    assert(internal != INVALID_LIT);
    clause.push_back(internal);
  }
  const uint64_t id = ++kernel.next_clause_id;
  if (clause.size() == 1) {
    assert(!kernel.value(clause[0]));
    kernel.unit_ids[var_of(clause[0])] = id;
    kernel_assign_unit(kernel, clause[0]);
  } else if (clause.size() == 2) {
    kernel_add_binary_clause(kernel, clause[0], clause[1], id);
  } else {
    kernel_add_large_clause(kernel, clause.data(), clause.size(), false, 0, id);
  }
}

static void add_equation(Kernel &kernel, const std::vector<int32_t> &variables,
                         bool rhs) {
  for (const auto &clause : clausal_form(variables, rhs))
    add_clause(kernel, clause);
}

static AffineLiveRequest capture_request(FILE *records) {
  AffineLiveRequest request;
  request.image = {100000, 10000, 100000, 1000000, 8, 64, 64, 4};
  request.maximum_snapshot_literals = 1000000;
  request.maximum_snapshot_work = 1000000;
  request.maximum_proof_additions = 1000000;
  request.maximum_proof_deletions = 1000000;
  request.maximum_journal_bytes = 1000000;
  request.action_id = 81;
  request.semantic_id = 82;
  request.records = records;
  return request;
}

static MatrixLimits matrix_limits() {
  return {10000000, 256, 256, 256, 256, 256};
}

static MatrixLiveRequest live_request(FILE *records,
                                      const AffineLiveSnapshot &owner,
                                      const AffineMatrix &matrix,
                                      const std::vector<Equation> &sources,
                                      const MatrixEvent &event) {
  MatrixLiveRequest request;
  request.owner = &owner;
  request.matrix = &matrix;
  request.exact_sources = &sources;
  request.event = &event;
  request.owner_identity = 991;
  request.transaction = {10000000, 1000000, 1000000, 100000, 1000000};
  request.maximum_revalidation_work = 10000000;
  request.maximum_journal_bytes = 1000000;
  request.maximum_external_extent = 1000000;
  request.action_id = 91;
  request.semantic_id = 92;
  request.records = records;
  return request;
}

static std::vector<MatrixAssignment> trail(const Kernel &kernel) {
  std::vector<MatrixAssignment> result;
  for (Lit literal : kernel.trail) {
    const int32_t external = kernel_externalize(kernel, literal);
    result.push_back({external < 0 ? -external : external, external > 0});
  }
  return result;
}

struct Fixture {
  AffineLiveSnapshot owner;
  Image image;
  AffineMatrix matrix;
  MatrixEvent event;
};

static void recognize_and_synchronize(Kernel &kernel, FILE *records,
                                      Fixture &fixture) {
  const auto capture = capture_request(records);
  assert(capture_affine_live_snapshot(kernel, capture, fixture.owner));
  assert(recognize(fixture.owner.premises, capture.image,
                   [](uint64_t) { return true; }, fixture.image) ==
         Status::Complete);
  assert(build_affine_matrix(fixture.image.equations, 991, matrix_limits(),
                             [](uint64_t) { return true; }, fixture.matrix) ==
         MatrixStatus::Complete);
  assert(fixture.matrix.synchronize(trail(kernel),
                                    [](uint64_t) { return true; },
                                    fixture.event) == MatrixStatus::Complete);
}

static void install_implication_formula(Kernel &kernel) {
  kernel_reserve_vars(kernel, 6);
  add_equation(kernel, {1, 3, 4}, false);
  add_equation(kernel, {2, 3, 4, 5}, false);
  assert(kernel_propagation_fixpoint(kernel));
}

static void decide(Kernel &kernel, int32_t external) {
  kernel_assign_decision(kernel, kernel_internalize(kernel, external));
  assert(!kernel_propagate(kernel));
  assert(kernel_propagation_fixpoint(kernel));
}

static std::string records(FILE *file) {
  assert(!std::fflush(file));
  const long end = std::ftell(file);
  assert(end >= 0);
  std::rewind(file);
  std::string result(static_cast<size_t>(end), '\0');
  assert(std::fread(result.data(), 1, result.size(), file) == result.size());
  assert(!std::fseek(file, end, SEEK_SET));
  return result;
}

static void implication_is_assigned_without_prefix_rescan() {
  Kernel kernel;
  install_implication_formula(kernel);
  decide(kernel, -1);
  decide(kernel, -2);
  FILE *journal = std::tmpfile();
  assert(journal);
  Fixture fixture;
  recognize_and_synchronize(kernel, journal, fixture);
  assert(fixture.event.kind == MatrixEventKind::Implication &&
         fixture.event.implied_variable == 5 &&
         !fixture.event.implied_value);
  const auto request = live_request(journal, fixture.owner, fixture.matrix,
                                    fixture.image.equations, fixture.event);
  AffineMatrixLivePrepared prepared;
  const auto preflight = prepare_affine_matrix_live(kernel, request, prepared);
  assert(preflight.status == MatrixLiveStatus::Prepared && prepared.ready());
  const uint32_t old_head = kernel.propagate_head;
  const auto applied = publish_affine_matrix_live(kernel, request, prepared);
  assert(applied.status == MatrixLiveStatus::Applied &&
         applied.result == ApplyResult::Ok && applied.implication &&
         applied.implication_consumed && applied.propagation_pending &&
         !kernel.conflict_valid);
  assert(kernel.value(kernel_internalize(kernel, -5)) > 0 &&
         kernel.propagate_head == old_head &&
         kernel.propagate_head < kernel.trail.size());
  assert(!kernel_propagate(kernel));
  assert(kernel.value(kernel_internalize(kernel, -5)) > 0 &&
         kernel_propagation_fixpoint(kernel));
  const std::string provenance = records(journal);
  assert(provenance.find("\"event\":\"implication\"") != std::string::npos &&
         provenance.find("\"reason\":[1,2,-5]") != std::string::npos &&
         provenance.find("\"sources\":[") != std::string::npos &&
         provenance.find("\"occurrence\":") != std::string::npos &&
         provenance.find("\"clause_id\":") != std::string::npos &&
         provenance.find("\"proof_events\":[") != std::string::npos &&
         provenance.find("\"deleted_id\":") != std::string::npos &&
         provenance.find("\"helpers\":") != std::string::npos &&
         provenance.find("proof-only balanced-XOR identities") !=
             std::string::npos &&
         provenance.find("\"planner_work\"") != std::string::npos);
  std::fclose(journal);
}

static void compact_provenance_keeps_authenticated_reason() {
  Kernel kernel;
  install_implication_formula(kernel);
  decide(kernel, -1);
  decide(kernel, -2);
  FILE *journal = std::tmpfile();
  assert(journal);
  Fixture fixture;
  recognize_and_synchronize(kernel, journal, fixture);
  auto request = live_request(journal, fixture.owner, fixture.matrix,
                              fixture.image.equations, fixture.event);
  request.compact_provenance = true;
  AffineMatrixLivePrepared prepared;
  const auto preflight = prepare_affine_matrix_live(kernel, request, prepared);
  assert(preflight.status == MatrixLiveStatus::Prepared);
  auto changed = request;
  changed.compact_provenance = false;
  const auto rejected = publish_affine_matrix_live(kernel, changed, prepared);
  assert(rejected.status == MatrixLiveStatus::Invalid &&
         records(journal).empty());
  const auto applied = publish_affine_matrix_live(kernel, request, prepared);
  assert(applied.status == MatrixLiveStatus::Applied && applied.implication &&
         applied.implication_consumed &&
         applied.preflight_additions > 0 && applied.journal_bytes > 0);
  assert(!kernel_propagate(kernel));
  assert(kernel.value(kernel_internalize(kernel, -5)) > 0);
  const std::string provenance = records(journal);
  assert(provenance.find("\"reason\":[1,2,-5]") != std::string::npos &&
         provenance.find("\"sources\":[") != std::string::npos &&
         provenance.find("\"occurrence\":") != std::string::npos &&
         provenance.find("\"clause_id\":") != std::string::npos &&
         provenance.find("\"proof_events_omitted\":true") != std::string::npos &&
         provenance.find("\"proof_events\":[") == std::string::npos);
  std::fclose(journal);
}

static void historical_implication_can_be_preflighted_after_tableau_advance() {
  Kernel kernel;
  install_implication_formula(kernel);
  decide(kernel, -1);
  decide(kernel, -2);
  FILE *journal = std::tmpfile();
  assert(journal);
  Fixture fixture;
  recognize_and_synchronize(kernel, journal, fixture);
  assert(fixture.event.kind == MatrixEventKind::Implication &&
         fixture.event.implied_variable == 5 &&
         !fixture.event.implied_value);
  const MatrixEvent historical = fixture.event;

  // Consume the implication and advance the retained tableau.  The old event
  // is no longer the matrix's current event, but its complete source-derived
  // reason remains independently authenticatable.
  decide(kernel, -5);
  MatrixEvent current;
  assert(fixture.matrix.synchronize(trail(kernel),
                                    [](uint64_t) { return true; }, current) ==
         MatrixStatus::Complete);
  assert(current.kind == MatrixEventKind::None);

  auto ordinary = live_request(journal, fixture.owner, fixture.matrix,
                               fixture.image.equations, historical);
  AffineMatrixLivePrepared rejected;
  assert(prepare_affine_matrix_live(kernel, ordinary, rejected).status !=
         MatrixLiveStatus::Prepared);

  auto retained = ordinary;
  retained.historical_event = true;
  AffineMatrixLivePrepared prepared;
  const uint64_t clause_id = kernel.next_clause_id;
  const size_t arena_words = kernel.arena.size();
  const auto planned = prepare_affine_matrix_live(kernel, retained, prepared);
  assert(planned.status == MatrixLiveStatus::Prepared && planned.implication &&
         prepared.ready() && kernel.next_clause_id == clause_id &&
         kernel.arena.size() == arena_words && records(journal).empty());

  // A conflict boundary is exactly where the future public materialization
  // action needs this pure plan; preflight must not require a BCP fixpoint.
  kernel.conflict_valid = true;
  AffineMatrixLivePrepared at_conflict;
  assert(prepare_affine_matrix_live(kernel, retained, at_conflict).status ==
         MatrixLiveStatus::Prepared);

  MatrixEvent forged = historical;
  forged.reason.front() = -forged.reason.front();
  auto bad = retained;
  bad.event = &forged;
  AffineMatrixLivePrepared forged_plan;
  assert(prepare_affine_matrix_live(kernel, bad, forged_plan).status !=
         MatrixLiveStatus::Prepared);
  std::fclose(journal);
}

static void retained_implication_can_be_published_at_conflict() {
  Kernel kernel;
  install_implication_formula(kernel);
  decide(kernel, -1);
  decide(kernel, -2);
  FILE *journal = std::tmpfile();
  assert(journal);
  Fixture fixture;
  recognize_and_synchronize(kernel, journal, fixture);
  assert(fixture.event.kind == MatrixEventKind::Implication &&
         fixture.event.reason.size() == 3);
  auto request = live_request(journal, fixture.owner, fixture.matrix,
                              fixture.image.equations, fixture.event);
  request.retained_publication = true;
  AffineMatrixLivePrepared prepared;
  const auto planned = prepare_affine_matrix_live(kernel, request, prepared);
  assert(planned.status == MatrixLiveStatus::Prepared && prepared.ready() &&
         prepared.physical_reason().size() == 3);
  assert(prepared.retained_bytes() > 1);
  auto bounded = request;
  bounded.maximum_persistent_bytes = prepared.retained_bytes() - 1;
  AffineMatrixLivePrepared rejected;
  assert(prepare_affine_matrix_live(kernel, bounded, rejected).status ==
         MatrixLiveStatus::ResourceExhausted);

  decide(kernel, -5);
  MatrixEvent current;
  assert(fixture.matrix.synchronize(trail(kernel),
                                    [](uint64_t) { return true; }, current) ==
         MatrixStatus::Complete);
  assert(current.kind == MatrixEventKind::None);
  kernel.conflict_valid = true;
  const uint64_t before = kernel.next_clause_id;
  const auto published =
      publish_affine_matrix_live(kernel, request, prepared);
  assert(published.status == MatrixLiveStatus::Applied &&
         published.implication && !published.implication_consumed &&
         published.installed_reason_id > before &&
         published.installed_reason_ref != INVALID_REF &&
         clause_at(kernel.arena, published.installed_reason_ref)->id() ==
             published.installed_reason_id &&
         !records(journal).empty());
  std::fclose(journal);
}

static void source_clause_order_is_not_identity() {
  Kernel kernel;
  install_implication_formula(kernel);
  decide(kernel, -1);
  decide(kernel, -2);
  FILE *journal = std::tmpfile();
  assert(journal);
  Fixture fixture;
  recognize_and_synchronize(kernel, journal, fixture);
  for (AffineLiveSource &source : fixture.owner.sources)
    std::reverse(source.clause.begin(), source.clause.end());
  const auto request = live_request(journal, fixture.owner, fixture.matrix,
                                    fixture.image.equations, fixture.event);
  const auto applied = apply_affine_matrix_live(kernel, request);
  assert(applied.status == MatrixLiveStatus::Applied &&
         applied.implication && applied.implication_consumed);
  assert(!kernel_propagate(kernel));
  std::fclose(journal);
}

static void conflict_is_consumed_by_ordinary_bcp() {
  Kernel kernel;
  install_implication_formula(kernel);
  decide(kernel, -1);
  decide(kernel, -2);
  decide(kernel, 5);
  FILE *journal = std::tmpfile();
  assert(journal);
  Fixture fixture;
  recognize_and_synchronize(kernel, journal, fixture);
  assert(fixture.event.kind == MatrixEventKind::Conflict);
  const auto request = live_request(journal, fixture.owner, fixture.matrix,
                                    fixture.image.equations, fixture.event);
  const auto applied = apply_affine_matrix_live(kernel, request);
  assert(applied.status == MatrixLiveStatus::Applied && applied.conflict &&
         applied.propagation_pending && !kernel.conflict_valid &&
         !kernel.inconsistent);
  assert(kernel_propagate(kernel) && kernel.conflict_valid);
  std::fclose(journal);
}

static void unit_above_root_declines() {
  Kernel kernel;
  kernel_reserve_vars(kernel, 6);
  add_equation(kernel, {3, 4}, false);
  add_equation(kernel, {3, 4, 5}, false);
  decide(kernel, 6);
  FILE *journal = std::tmpfile();
  assert(journal);
  Fixture fixture;
  recognize_and_synchronize(kernel, journal, fixture);
  assert(fixture.event.kind == MatrixEventKind::Implication &&
         fixture.event.reason.size() == 1);
  const auto request = live_request(journal, fixture.owner, fixture.matrix,
                                    fixture.image.equations, fixture.event);
  const uint64_t id = kernel.next_clause_id;
  const auto extent = kernel.external_to_internal.size();
  const auto result = apply_affine_matrix_live(kernel, request);
  assert(result.status == MatrixLiveStatus::UnsupportedOutcome &&
         result.result == ApplyResult::NoOp && kernel.next_clause_id == id &&
         kernel.external_to_internal.size() == extent &&
         std::ftell(journal) == 0);
  std::fclose(journal);
}

enum class StaleKind { Trail, Source, Namespace, ProofId, Arena, ProofMode };

static void stale_publication_boundaries() {
  for (StaleKind kind : {StaleKind::Trail, StaleKind::Source,
                         StaleKind::Namespace, StaleKind::ProofId,
                         StaleKind::Arena, StaleKind::ProofMode}) {
    Kernel kernel;
    install_implication_formula(kernel);
    decide(kernel, -1);
    decide(kernel, -2);
    FILE *journal = std::tmpfile();
    assert(journal);
    Fixture fixture;
    recognize_and_synchronize(kernel, journal, fixture);
    auto request = live_request(journal, fixture.owner, fixture.matrix,
                                fixture.image.equations, fixture.event);
    AffineMatrixLivePrepared prepared;
    assert(prepare_affine_matrix_live(kernel, request, prepared).status ==
           MatrixLiveStatus::Prepared);
    Proof proof;
    std::string proof_path;
    if (kind == StaleKind::Trail) {
      decide(kernel, 6);
    } else if (kind == StaleKind::Source) {
      const auto source = std::find_if(
          fixture.owner.sources.begin(), fixture.owner.sources.end(),
          [](const AffineLiveSource &item) {
            return item.kind == AffineSourceKind::Large;
          });
      assert(source != fixture.owner.sources.end());
      Clause *clause = clause_at(kernel.arena, source->ref);
      kernel_delete_clause(kernel, clause, false);
    } else if (kind == StaleKind::Namespace) {
      Var first = INVALID_VAR;
      assert(kernel_reserve_proof_variables(kernel, 1, first));
    } else if (kind == StaleKind::ProofId) {
      ++kernel.next_clause_id;
    } else if (kind == StaleKind::Arena) {
      kernel_collect_garbage(kernel);
    } else {
      char path[] = "/tmp/tactix-affine-live-stale-XXXXXX";
      const int descriptor = mkstemp(path);
      assert(descriptor >= 0);
      close(descriptor);
      proof_path = path;
      assert(proof.open(proof_path.c_str(), ProofFormat::DsrAscii));
      proof.bind_variable_map(&kernel.internal_to_external);
      proof.bind_external_variable_map(&kernel.external_to_internal);
      kernel.proof = &proof;
    }
    const long before_records = std::ftell(journal);
    const auto result = publish_affine_matrix_live(kernel, request, prepared);
    assert(result.status == MatrixLiveStatus::Stale &&
           result.result == ApplyResult::NoOp &&
           std::ftell(journal) == before_records);
    kernel.proof = nullptr;
    if (!proof_path.empty()) std::remove(proof_path.c_str());
    std::fclose(journal);
  }
}

static void resource_and_interruption_are_atomic() {
  for (unsigned mode = 0; mode < 4; ++mode) {
    Kernel kernel;
    install_implication_formula(kernel);
    decide(kernel, -1);
    decide(kernel, -2);
    FILE *journal = std::tmpfile();
    assert(journal);
    Fixture fixture;
    recognize_and_synchronize(kernel, journal, fixture);
    auto request = live_request(journal, fixture.owner, fixture.matrix,
                                fixture.image.equations, fixture.event);
    volatile sig_atomic_t stop = 0;
    if (!mode) request.transaction.maximum_proof_additions = 1;
    else if (mode == 1) request.maximum_revalidation_work = 1;
    else if (mode == 2) request.maximum_journal_bytes = 1;
    else {
      stop = 1;
      kernel.stop = &stop;
    }
    const uint64_t id = kernel.next_clause_id;
    const auto extent = kernel.external_to_internal.size();
    const auto arena = kernel.arena.size();
    const auto result = apply_affine_matrix_live(kernel, request);
    assert(result.status == (mode == 3 ? MatrixLiveStatus::Interrupted
                                      : MatrixLiveStatus::ResourceExhausted));
    assert(kernel.next_clause_id == id &&
           kernel.external_to_internal.size() == extent &&
           kernel.arena.size() == arena && std::ftell(journal) == 0);
    std::fclose(journal);
  }

  // The immediate adapter does not retain the owner snapshot, but its bounded
  // publish replay must still validate against the live owner on the same
  // stack and preserve the historical partial-work result.
  {
    Kernel replay_kernel;
    install_implication_formula(replay_kernel);
    decide(replay_kernel, -1);
    decide(replay_kernel, -2);
    FILE *replay_journal = std::tmpfile();
    assert(replay_journal);
    Fixture replay_fixture;
    recognize_and_synchronize(replay_kernel, replay_journal, replay_fixture);
    auto replay_request = live_request(
        replay_journal, replay_fixture.owner, replay_fixture.matrix,
        replay_fixture.image.equations, replay_fixture.event);
    AffineMatrixLivePrepared measured;
    const auto measurement =
        prepare_affine_matrix_live(replay_kernel, replay_request, measured);
    assert(measurement.status == MatrixLiveStatus::Prepared &&
           measurement.revalidation_work &&
           measured.transaction().authentication_work());
    replay_request.maximum_revalidation_work =
        std::max(measurement.revalidation_work,
                 measured.transaction().authentication_work());
    const uint64_t replay_id = replay_kernel.next_clause_id;
    const auto replay_extent = replay_kernel.external_to_internal.size();
    const auto replay_arena = replay_kernel.arena.size();
    const auto replayed =
        apply_affine_matrix_live(replay_kernel, replay_request);
    assert(replayed.status == MatrixLiveStatus::ResourceExhausted &&
           replayed.revalidation_work ==
               replay_request.maximum_revalidation_work &&
           replay_kernel.next_clause_id == replay_id &&
           replay_kernel.external_to_internal.size() == replay_extent &&
           replay_kernel.arena.size() == replay_arena &&
           std::ftell(replay_journal) == 0);
    std::fclose(replay_journal);
  }

  Kernel kernel;
  install_implication_formula(kernel);
  decide(kernel, -1);
  decide(kernel, -2);
  FILE *journal = std::tmpfile();
  assert(journal);
  Fixture fixture;
  recognize_and_synchronize(kernel, journal, fixture);
  auto request = live_request(journal, fixture.owner, fixture.matrix,
                              fixture.image.equations, fixture.event);
  AffineMatrixLivePrepared prepared;
  assert(prepare_affine_matrix_live(kernel, request, prepared).status ==
         MatrixLiveStatus::Prepared);
  volatile sig_atomic_t stop = 1;
  kernel.stop = &stop;
  const uint64_t id = kernel.next_clause_id;
  const auto extent = kernel.external_to_internal.size();
  assert(publish_affine_matrix_live(kernel, request, prepared).status ==
         MatrixLiveStatus::Interrupted);
  assert(kernel.next_clause_id == id &&
         kernel.external_to_internal.size() == extent &&
         std::ftell(journal) == 0);
  std::fclose(journal);
}

static void helper_namespace_is_monotone() {
  Kernel kernel;
  install_implication_formula(kernel);
  decide(kernel, -1);
  decide(kernel, -2);
  FILE *journal = std::tmpfile();
  assert(journal);
  Fixture fixture;
  recognize_and_synchronize(kernel, journal, fixture);
  const auto request = live_request(journal, fixture.owner, fixture.matrix,
                                    fixture.image.equations, fixture.event);
  const uint32_t before = kernel.external_to_internal.size();
  const auto result = apply_affine_matrix_live(kernel, request);
  assert(result.status == MatrixLiveStatus::Applied && result.helper_extent &&
         kernel.external_to_internal.size() == before + result.helper_extent);
  Var first = INVALID_VAR;
  assert(kernel_reserve_proof_variables(kernel, 2, first));
  assert(first == before + result.helper_extent);
  std::fclose(journal);
}

static int proof_fixture(const char *cnf_path, const char *proof_path,
                         bool pbp = false) {
  Kernel kernel;
  kernel_reserve_vars(kernel, 10);
  const std::vector<int32_t> first{1, 2, 3, 4, 9, 10};
  const std::vector<int32_t> second{5, 6, 7, 8, 9, 10};
  add_equation(kernel, first, false);
  add_equation(kernel, second, true);
  FILE *journal = std::tmpfile();
  assert(journal);
  AffineLiveSnapshot owner;
  const auto capture = capture_request(journal);
  assert(capture_affine_live_snapshot(kernel, capture, owner));
  Image image;
  assert(recognize(owner.premises, capture.image,
                   [](uint64_t) { return true; }, image) == Status::Complete);
  for (int32_t variable = 1; variable <= 8; ++variable)
    add_clause(kernel, {-variable});
  assert(!kernel_propagate(kernel));
  AffineMatrix matrix;
  assert(build_affine_matrix(image.equations, 991, matrix_limits(),
                             [](uint64_t) { return true; }, matrix) ==
         MatrixStatus::Complete);
  MatrixEvent event;
  assert(matrix.synchronize(trail(kernel), [](uint64_t) { return true; },
                            event) == MatrixStatus::Complete);
  assert(event.kind == MatrixEventKind::Conflict);

  Proof proof;
  assert(proof.open(proof_path, pbp ? ProofFormat::PbpStreaming
                                    : ProofFormat::DsrAscii));
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  if (pbp) {
    uint64_t id = 0;
    for (const auto &[variables, rhs] :
         std::vector<std::pair<std::vector<int32_t>, bool>>{{first, false},
                                                            {second, true}})
      for (const auto &external : clausal_form(variables, rhs)) {
        std::vector<Lit> clause;
        for (const int32_t literal : external)
          clause.push_back(kernel_internalize(kernel, literal));
        proof.seed_population(++id, clause.data(), clause.size());
      }
    for (int32_t variable = 1; variable <= 8; ++variable) {
      const Lit unit = kernel_internalize(kernel, -variable);
      proof.seed_population(++id, &unit, 1);
    }
    assert(id == kernel.next_clause_id);
    proof.finish_input_transaction();
    assert(proof.commit_clause_stream());
  }
  kernel.proof = &proof;
  auto request = live_request(journal, owner, matrix, image.equations, event);
  PbpParityBoundsCache pbp_cache;
  if (pbp) {
    request.pbp_parity_cache = &pbp_cache;
    // Persistent search promises that this owner already has a complete
    // affine-matrix-retained-sources record in the same journal.
    request.retained_source_provenance = true;
  }
  const auto result = apply_affine_matrix_live(kernel, request);
  assert(result.status == MatrixLiveStatus::Applied && result.conflict &&
         result.compact_pbp == pbp &&
         (!pbp || (result.deletions == 0 && pbp_cache.size() > 0)));
  if (pbp) {
    const std::string provenance = records(journal);
    assert(provenance.find("\"source_ordinals\":[") != std::string::npos &&
           provenance.find("\"sources_ref\":{\"retained_owner\":991}") !=
               std::string::npos &&
           provenance.find("\"sources\":[") == std::string::npos);
    std::vector<PbpParityEquation> equations;
    for (const size_t ordinal : event.row.source_equations)
      equations.push_back({image.equations[ordinal].variables,
                           image.equations[ordinal].rhs});
    PbpParityReasonPlan cached;
    assert(proof.plan_pbp_parity_reason(
               equations, event.reason.empty() ? nullptr : event.reason.data(),
               event.reason.size(), kernel.external_to_internal.size(),
               pbp_cache, cached) &&
           cached.proof_constraints == 3 && !cached.helper_variables);
    if (!event.reason.empty()) {
      auto wrong = event.reason;
      wrong.front() = -wrong.front();
      PbpParityReasonPlan rejected;
      assert(!proof.plan_pbp_parity_reason(
          equations, wrong.data(), wrong.size(),
          kernel.external_to_internal.size(), pbp_cache, rejected));
    }
  }
  assert(kernel_propagate(kernel) && kernel.conflict_valid);
  assert(proof.add_external_rup(++kernel.next_clause_id, nullptr, 0));
  proof.flush();
  kernel.proof = nullptr;

  FILE *cnf = std::fopen(cnf_path, "w");
  assert(cnf);
  std::fprintf(cnf, "p cnf %zu 72\n", kernel.external_to_internal.size());
  for (const auto &[variables, rhs] :
       std::vector<std::pair<std::vector<int32_t>, bool>>{{first, false},
                                                          {second, true}})
    for (const auto &clause : clausal_form(variables, rhs)) {
      for (int32_t literal : clause) std::fprintf(cnf, "%d ", literal);
      std::fprintf(cnf, "0\n");
    }
  for (int32_t variable = 1; variable <= 8; ++variable)
    std::fprintf(cnf, "-%d 0\n", variable);
  std::fclose(cnf);
  std::fclose(journal);
  return 0;
}

static int resumed_proof_fixture(const char *cnf_path,
                                 const char *proof_path) {
  Kernel kernel;
  kernel_reserve_vars(kernel, 10);
  const std::vector<int32_t> first{1,2,3,4,9,10};
  const std::vector<int32_t> second{5,6,7,8,9,10};
  add_equation(kernel, first, false);
  add_equation(kernel, second, true);
  FILE *journal = std::tmpfile();
  assert(journal);
  AffineLiveSnapshot owner;
  const auto capture = capture_request(journal);
  assert(capture_affine_live_snapshot(kernel, capture, owner));
  Image image;
  assert(recognize(owner.premises, capture.image,
                   [](uint64_t) { return true; }, image) == Status::Complete);
  for (int32_t variable=1;variable<=8;++variable)
    add_clause(kernel, {-variable});
  assert(!kernel_propagate(kernel));
  AffineMatrix matrix;
  assert(build_affine_matrix(image.equations, 991, matrix_limits(),
                             [](uint64_t) { return true; }, matrix) ==
         MatrixStatus::Complete);
  MatrixEvent prior_event;
  assert(matrix.synchronize({{1,false},{2,false},{3,false},{4,false},{9,false}},
                            [](uint64_t) { return true; }, prior_event) ==
         MatrixStatus::Complete);
  assert(prior_event.kind == MatrixEventKind::Implication &&
         prior_event.row.source_equations.size() == 1);
  Proof proof;
  assert(proof.open(proof_path, ProofFormat::DsrAscii));
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  kernel.proof = &proof;
  AffineMatrixProofCache cache;
  MatrixTransactionLimits limits{10000000,1000000,1000000,100000,1000000};
  AffineMatrixTransaction prior;
  assert(prepare_affine_matrix_transaction(
             matrix,991,image.equations,prior_event,
             kernel.external_to_internal.size(),kernel.next_clause_id,limits,
             [](uint64_t) { return true; },prior,&cache,
             proof.persistent_external_epoch()) ==
         MatrixTransactionStatus::Prepared);
  assert(prior.proof_cache_insert());
  Var first_helper = INVALID_VAR;
  assert(kernel_reserve_proof_variables(kernel,prior.helper_extent(),first_helper));
  std::set<uint64_t> support(prior.proof_cache_entry().support_ids.begin(),
                             prior.proof_cache_entry().support_ids.end());
  for (const MatrixProofEvent &step : prior.proof_events()) {
    if (step.kind == MatrixProofEvent::Kind::Delete) {
      assert(proof.del_external(kernel.next_clause_id,step.deleted_id,
                                step.clause.data(),step.clause.size()));
      continue;
    }
    assert(++kernel.next_clause_id == step.id);
    const bool emitted = step.kind == MatrixProofEvent::Kind::Rat
        ? proof.add_external_rat(step.id,step.clause.data(),step.clause.size())
        : proof.add_external_rup(step.id,step.clause.data(),step.clause.size());
    assert(emitted);
    if (support.contains(step.id))
      assert(proof.retain_external(step.id,step.clause.data(),
                                   step.clause.size()));
  }
  assert(cache.commit(MatrixProofCacheEntry(prior.proof_cache_entry()),0));
  MatrixEvent event;
  assert(matrix.synchronize(trail(kernel),[](uint64_t) { return true; },event) ==
         MatrixStatus::Complete);
  assert(event.kind == MatrixEventKind::Conflict &&
         event.row.source_equations.size() == 2);
  auto request = live_request(journal,owner,matrix,image.equations,event);
  request.proof_cache = &cache;
  request.maximum_persistent_bytes = 10000000;
  const auto applied = apply_affine_matrix_live(kernel,request);
  assert(applied.status == MatrixLiveStatus::Applied &&
         applied.proof_cache_resume && applied.proof_cache_insert &&
         applied.conflict);
  assert(kernel_propagate(kernel) && kernel.conflict_valid);
  assert(proof.add_external_rup(++kernel.next_clause_id,nullptr,0));
  proof.flush();
  kernel.proof = nullptr;
  FILE *cnf=std::fopen(cnf_path,"w");
  assert(cnf);
  std::fprintf(cnf,"p cnf %zu 72\n",kernel.external_to_internal.size());
  for (const auto &[variables,rhs] :
       std::vector<std::pair<std::vector<int32_t>,bool>>{{first,false},
                                                         {second,true}})
    for (const auto &clause : clausal_form(variables,rhs)) {
      for (int32_t literal:clause) std::fprintf(cnf,"%d ",literal);
      std::fprintf(cnf,"0\n");
    }
  for (int32_t variable=1;variable<=8;++variable)
    std::fprintf(cnf,"-%d 0\n",variable);
  std::fclose(cnf);
  std::fclose(journal);
  return 0;
}

static int removed_source_proof_fixture(const char *cnf_path,
                                        const char *proof_path) {
  const std::vector<std::pair<std::vector<int32_t>,bool>> rows{
      {{2,3,4,7},false},{{5,6,7,9},true},{{2,3,6,8},true},
      {{2,4,7,8},false},{{1,2,5,9},true},{{1,3,6,7},false}};
  Kernel kernel;
  kernel_reserve_vars(kernel,9);
  for(const auto &[variables,rhs]:rows)add_equation(kernel,variables,rhs);
  FILE *journal=std::tmpfile();assert(journal);
  AffineLiveSnapshot owner;
  const auto capture=capture_request(journal);
  assert(capture_affine_live_snapshot(kernel,capture,owner));
  Image image;
  assert(recognize(owner.premises,capture.image,[](uint64_t){return true;},
                   image)==Status::Complete);
  const auto rank = [&](const Equation &equation) {
    for (size_t i=0;i<rows.size();++i)
      if (equation.variables==rows[i].first && equation.rhs==rows[i].second)
        return i;
    return rows.size();
  };
  assert(image.equations.size()==rows.size());
  std::sort(image.equations.begin(),image.equations.end(),
            [&](const Equation &a,const Equation &b){return rank(a)<rank(b);});
  for(size_t i=0;i<rows.size();++i)assert(rank(image.equations[i])==i);
  for(int32_t literal:{-4,5,-8,-9})add_clause(kernel,{literal});
  assert(!kernel_propagate(kernel));
  AffineMatrix matrix;
  assert(build_affine_matrix(image.equations,991,matrix_limits(),
                             [](uint64_t){return true;},matrix)==
         MatrixStatus::Complete);
  MatrixEvent old_event;
  assert(matrix.synchronize({{1,true},{2,true},{5,false},{6,true},
                             {8,true},{9,true}},[](uint64_t){return true;},
                            old_event)==MatrixStatus::Complete);
  assert((old_event.row.source_equations==std::vector<size_t>{0,2,3}));
  Proof proof;
  assert(proof.open(proof_path,ProofFormat::DsrAscii));
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  kernel.proof=&proof;
  AffineMatrixProofCache cache;
  MatrixTransactionLimits limits{10000000,1000000,1000000,100000,1000000};
  AffineMatrixTransaction old_tx;
  assert(prepare_affine_matrix_transaction(
             matrix,991,image.equations,old_event,
             kernel.external_to_internal.size(),kernel.next_clause_id,limits,
             [](uint64_t){return true;},old_tx,&cache,
             proof.persistent_external_epoch())==
         MatrixTransactionStatus::Prepared);
  assert(old_tx.proof_cache_insert());
  Var first_helper=INVALID_VAR;
  assert(kernel_reserve_proof_variables(kernel,old_tx.helper_extent(),
                                       first_helper));
  std::set<uint64_t> support(old_tx.proof_cache_entry().support_ids.begin(),
                             old_tx.proof_cache_entry().support_ids.end());
  for(const MatrixProofEvent &step:old_tx.proof_events()) {
    if(step.kind==MatrixProofEvent::Kind::Delete) {
      assert(proof.del_external(kernel.next_clause_id,step.deleted_id,
                                step.clause.data(),step.clause.size()));
      continue;
    }
    assert(++kernel.next_clause_id==step.id);
    const bool emitted=step.kind==MatrixProofEvent::Kind::Rat
        ?proof.add_external_rat(step.id,step.clause.data(),step.clause.size())
        :proof.add_external_rup(step.id,step.clause.data(),step.clause.size());
    assert(emitted);
    if(support.contains(step.id))
      assert(proof.retain_external(step.id,step.clause.data(),step.clause.size()));
  }
  assert(cache.commit(MatrixProofCacheEntry(old_tx.proof_cache_entry()),0));
  MatrixEvent event;
  assert(matrix.synchronize(trail(kernel),[](uint64_t){return true;},event)==
         MatrixStatus::Complete);
  std::fprintf(stderr,"removed live event kind=%u sources",unsigned(event.kind));
  for(auto id:event.row.source_equations)std::fprintf(stderr," %zu",id);
  std::fprintf(stderr,"\n");
  assert((event.row.source_equations==std::vector<size_t>{0,1,2}));
  auto request=live_request(journal,owner,matrix,image.equations,event);
  request.proof_cache=&cache;
  request.maximum_persistent_bytes=10000000;
  const auto applied=apply_affine_matrix_live(kernel,request);
  assert(applied.status==MatrixLiveStatus::Applied &&
         applied.proof_cache_resume && applied.conflict);
  assert(kernel_propagate(kernel)&&kernel.conflict_valid);
  assert(proof.add_external_rup(++kernel.next_clause_id,nullptr,0));
  proof.flush();kernel.proof=nullptr;
  FILE *cnf=std::fopen(cnf_path,"w");assert(cnf);
  std::fprintf(cnf,"p cnf %zu 52\n",kernel.external_to_internal.size());
  for(const auto &[variables,rhs]:rows)
    for(const auto &clause:clausal_form(variables,rhs)) {
      for(int32_t literal:clause)std::fprintf(cnf,"%d ",literal);
      std::fprintf(cnf,"0\n");
    }
  for(int32_t literal:{-4,5,-8,-9})std::fprintf(cnf,"%d 0\n",literal);
  std::fclose(cnf);std::fclose(journal);
  return 0;
}

int main(int argc, char **argv) {
  if (argc == 4 && std::strcmp(argv[1], "--proof") == 0)
    return proof_fixture(argv[2], argv[3]);
  if (argc == 4 && std::strcmp(argv[1], "--pbp-proof") == 0)
    return proof_fixture(argv[2], argv[3], true);
  if (argc == 4 && std::strcmp(argv[1], "--resume-proof") == 0)
    return resumed_proof_fixture(argv[2], argv[3]);
  if (argc == 4 && std::strcmp(argv[1], "--removed-proof") == 0)
    return removed_source_proof_fixture(argv[2], argv[3]);
  implication_is_assigned_without_prefix_rescan();
  compact_provenance_keeps_authenticated_reason();
  historical_implication_can_be_preflighted_after_tableau_advance();
  retained_implication_can_be_published_at_conflict();
  source_clause_order_is_not_identity();
  conflict_is_consumed_by_ordinary_bcp();
  unit_above_root_declines();
  stale_publication_boundaries();
  resource_and_interruption_are_atomic();
  helper_namespace_is_monotone();
  std::puts("test_affine_matrix_live: passed");
}
