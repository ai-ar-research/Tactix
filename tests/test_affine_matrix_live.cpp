#include "unit_test.hpp"
#include "affine_formula_support.hpp"
#include "dimacs_text.hpp"
#include "test_io.hpp"
#include "proof/proof.hpp"
#include "rules/semantic_extension/affine_matrix_live.hpp"

#include <unistd.h>

using namespace tactix::affine_semantic_extension;

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

static void decide(Kernel &kernel, int32_t external) {
  kernel_assign_decision(kernel, kernel_internalize(kernel, external));
  assert(!kernel_propagate(kernel));
  assert(kernel_propagation_fixpoint(kernel));
}

// The implication formula after decisions (x1 = x2 = 0 by default), with its
// owner captured, recognized and synchronized.
struct Scenario {
  Kernel kernel;
  FILE *journal = temporary_records();
  Fixture fixture;
  explicit Scenario(std::initializer_list<int32_t> decisions = {-1, -2}) {
    install_implication_formula(kernel);
    for (int32_t external : decisions) decide(kernel, external);
    recognize_and_synchronize(kernel, journal, fixture);
  }
  ~Scenario() { std::fclose(journal); }
  MatrixLiveRequest request() const {
    return live_request(journal, fixture.owner, fixture.matrix,
                        fixture.image.equations, fixture.event);
  }
};

// What a declined publication must leave unchanged.
struct Unchanged {
  const Kernel &kernel;
  FILE *journal;
  uint64_t id = kernel.next_clause_id;
  size_t extent = kernel.external_to_internal.size();
  size_t arena = kernel.arena.size();
  long journal_position = std::ftell(journal);
  void check() const {
    assert(kernel.next_clause_id == id &&
           kernel.external_to_internal.size() == extent &&
           kernel.arena.size() == arena && std::ftell(journal) == journal_position);
  }
};

TEST(implication_is_assigned_without_prefix_rescan) {
  Scenario scenario;
  Kernel &kernel = scenario.kernel;
  const auto &event = scenario.fixture.event;
  assert(event.kind == MatrixEventKind::Implication &&
         event.implied_variable == 5 && !event.implied_value);
  const auto request = scenario.request();
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
  const std::string provenance = written_bytes(scenario.journal);
  for (const char *field :
       {"\"event\":\"implication\"", "\"reason\":[1,2,-5]", "\"sources\":[",
        "\"occurrence\":", "\"clause_id\":", "\"proof_events\":[",
        "\"deleted_id\":", "\"helpers\":", "proof-only balanced-XOR identities",
        "\"planner_work\""})
    assert(provenance.find(field) != std::string::npos);
}

TEST(implication_watch_survives_backtrack) {
  Scenario scenario;
  Kernel &kernel = scenario.kernel;
  assert(apply_affine_matrix_live(kernel, scenario.request()).status ==
         MatrixLiveStatus::Applied);
  assert(!kernel_propagate(kernel));
  kernel_backtrack(kernel, 1);
  assert(!kernel.value(kernel_internalize(kernel, -5)));
  decide(kernel, -2);
  assert(kernel.value(kernel_internalize(kernel, -5)) > 0);
  kernel_check_invariants(kernel);
}

TEST(compact_provenance_keeps_authenticated_reason) {
  Scenario scenario;
  Kernel &kernel = scenario.kernel;
  auto request = scenario.request();
  request.compact_provenance = true;
  AffineMatrixLivePrepared prepared;
  const auto preflight = prepare_affine_matrix_live(kernel, request, prepared);
  assert(preflight.status == MatrixLiveStatus::Prepared);
  auto changed = request;
  changed.compact_provenance = false;
  const auto rejected = publish_affine_matrix_live(kernel, changed, prepared);
  assert(rejected.status == MatrixLiveStatus::Invalid &&
         written_bytes(scenario.journal).empty());
  const auto applied = publish_affine_matrix_live(kernel, request, prepared);
  assert(applied.status == MatrixLiveStatus::Applied && applied.implication &&
         applied.implication_consumed &&
         applied.preflight_additions > 0 && applied.journal_bytes > 0);
  assert(!kernel_propagate(kernel));
  assert(kernel.value(kernel_internalize(kernel, -5)) > 0);
  const std::string provenance = written_bytes(scenario.journal);
  for (const char *field : {"\"reason\":[1,2,-5]", "\"sources\":[", "\"occurrence\":",
                            "\"clause_id\":", "\"proof_events_omitted\":true"})
    assert(provenance.find(field) != std::string::npos);
  assert(provenance.find("\"proof_events\":[") == std::string::npos);
}

TEST(historical_implication_can_be_preflighted_after_tableau_advance) {
  Scenario scenario;
  Kernel &kernel = scenario.kernel;
  Fixture &fixture = scenario.fixture;
  assert(fixture.event.kind == MatrixEventKind::Implication &&
         fixture.event.implied_variable == 5 &&
         !fixture.event.implied_value);
  const MatrixEvent historical = fixture.event;

  // After consuming the implication and advancing the tableau, the old
  // event's source-derived reason stays independently authenticatable.
  decide(kernel, -5);
  MatrixEvent current;
  assert(fixture.matrix.synchronize(trail(kernel),
                                    [](uint64_t) { return true; }, current) ==
         MatrixStatus::Complete);
  assert(current.kind == MatrixEventKind::None);

  auto ordinary = live_request(scenario.journal, fixture.owner, fixture.matrix,
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
         kernel.arena.size() == arena_words &&
         written_bytes(scenario.journal).empty());

  // Preflight at a conflict boundary does not require a BCP fixpoint.
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
}

TEST(retained_implication_can_be_published_at_conflict) {
  Scenario scenario;
  Kernel &kernel = scenario.kernel;
  assert(scenario.fixture.event.kind == MatrixEventKind::Implication &&
         scenario.fixture.event.reason.size() == 3);
  auto request = scenario.request();
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
  assert(scenario.fixture.matrix.synchronize(
             trail(kernel), [](uint64_t) { return true; }, current) ==
         MatrixStatus::Complete);
  assert(current.kind == MatrixEventKind::None);
  kernel.conflict_valid = true;
  const uint64_t before = kernel.next_clause_id;
  const auto published =
      publish_affine_matrix_live(kernel, request, prepared);
  assert(published.status == MatrixLiveStatus::Applied &&
         !published.retained_authentication &&
         published.implication && !published.implication_consumed &&
         published.installed_reason_id > before &&
         published.installed_reason_ref != INVALID_REF &&
         clause_at(kernel.arena, published.installed_reason_ref)->id() ==
             published.installed_reason_id &&
         !written_bytes(scenario.journal).empty());
}

TEST(source_clause_order_is_not_identity) {
  Scenario scenario;
  for (AffineLiveSource &source : scenario.fixture.owner.sources)
    std::reverse(source.clause.begin(), source.clause.end());
  const auto applied = apply_affine_matrix_live(scenario.kernel, scenario.request());
  assert(applied.status == MatrixLiveStatus::Applied &&
         applied.implication && applied.implication_consumed);
  assert(!kernel_propagate(scenario.kernel));
}

TEST(conflict_is_consumed_by_ordinary_bcp) {
  Scenario scenario({-1, -2, 5});
  Kernel &kernel = scenario.kernel;
  assert(scenario.fixture.event.kind == MatrixEventKind::Conflict);
  const auto applied = apply_affine_matrix_live(kernel, scenario.request());
  assert(applied.status == MatrixLiveStatus::Applied && applied.conflict &&
         applied.propagation_pending && !kernel.conflict_valid &&
         !kernel.inconsistent);
  assert(kernel_propagate(kernel) && kernel.conflict_valid);
}

TEST(unit_above_root_declines) {
  Kernel kernel;
  kernel_reserve_vars(kernel, 6);
  add_equation(kernel, {3, 4}, false);
  add_equation(kernel, {3, 4, 5}, false);
  decide(kernel, 6);
  FILE *journal = temporary_records();
  Fixture fixture;
  recognize_and_synchronize(kernel, journal, fixture);
  assert(fixture.event.kind == MatrixEventKind::Implication &&
         fixture.event.reason.size() == 1);
  const auto request = live_request(journal, fixture.owner, fixture.matrix,
                                    fixture.image.equations, fixture.event);
  const Unchanged unchanged{kernel, journal};
  const auto result = apply_affine_matrix_live(kernel, request);
  assert(result.status == MatrixLiveStatus::UnsupportedOutcome &&
         result.result == ApplyResult::NoOp && std::ftell(journal) == 0);
  unchanged.check();
  std::fclose(journal);
}

enum class StaleKind { Trail, Source, Namespace, ProofId, Arena, ProofMode };

TEST(stale_publication_boundaries) {
  for (StaleKind kind : {StaleKind::Trail, StaleKind::Source,
                         StaleKind::Namespace, StaleKind::ProofId,
                         StaleKind::Arena, StaleKind::ProofMode}) {
    Scenario scenario;
    Kernel &kernel = scenario.kernel;
    const auto request = scenario.request();
    AffineMatrixLivePrepared prepared;
    assert(prepare_affine_matrix_live(kernel, request, prepared).status ==
           MatrixLiveStatus::Prepared);
    Proof proof;
    std::string proof_path;
    if (kind == StaleKind::Trail) {
      decide(kernel, 6);
    } else if (kind == StaleKind::Source) {
      const auto &sources = scenario.fixture.owner.sources;
      const auto source = std::find_if(
          sources.begin(), sources.end(), [](const AffineLiveSource &item) {
            return item.kind == AffineSourceKind::Large;
          });
      assert(source != sources.end());
      kernel_delete_clause(kernel, clause_at(kernel.arena, source->ref), false);
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
      bind_proof(proof, kernel, path);
    }
    const long before_records = std::ftell(scenario.journal);
    const auto result = publish_affine_matrix_live(kernel, request, prepared);
    assert(result.status == MatrixLiveStatus::Stale &&
           result.result == ApplyResult::NoOp &&
           std::ftell(scenario.journal) == before_records);
    kernel.proof = nullptr;
    if (!proof_path.empty()) std::remove(proof_path.c_str());
  }
}

TEST(resource_and_interruption_are_atomic) {
  for (unsigned mode = 0; mode < 4; ++mode) {
    Scenario scenario;
    auto request = scenario.request();
    volatile sig_atomic_t stop = 0;
    if (!mode) request.transaction.maximum_proof_additions = 1;
    else if (mode == 1) request.maximum_revalidation_work = 1;
    else if (mode == 2) request.maximum_journal_bytes = 1;
    else {
      stop = 1;
      scenario.kernel.stop = &stop;
    }
    const Unchanged unchanged{scenario.kernel, scenario.journal};
    const auto result = apply_affine_matrix_live(scenario.kernel, request);
    assert(result.status == (mode == 3 ? MatrixLiveStatus::Interrupted
                                      : MatrixLiveStatus::ResourceExhausted));
    assert(std::ftell(scenario.journal) == 0);
    unchanged.check();
  }

  // The immediate adapter's bounded publish replay revalidates against the
  // live owner and keeps the partial-work result.
  {
    Scenario scenario;
    auto request = scenario.request();
    AffineMatrixLivePrepared measured;
    const auto measurement =
        prepare_affine_matrix_live(scenario.kernel, request, measured);
    assert(measurement.status == MatrixLiveStatus::Prepared &&
           measurement.revalidation_work &&
           measured.transaction().authentication_work());
    request.maximum_revalidation_work =
        std::max(measurement.revalidation_work,
                 measured.transaction().authentication_work());
    const Unchanged unchanged{scenario.kernel, scenario.journal};
    const auto replayed = apply_affine_matrix_live(scenario.kernel, request);
    assert(replayed.status == MatrixLiveStatus::ResourceExhausted &&
           replayed.revalidation_work == request.maximum_revalidation_work &&
           std::ftell(scenario.journal) == 0);
    unchanged.check();
  }

  Scenario scenario;
  const auto request = scenario.request();
  AffineMatrixLivePrepared prepared;
  assert(prepare_affine_matrix_live(scenario.kernel, request, prepared).status ==
         MatrixLiveStatus::Prepared);
  volatile sig_atomic_t stop = 1;
  scenario.kernel.stop = &stop;
  const Unchanged unchanged{scenario.kernel, scenario.journal};
  assert(publish_affine_matrix_live(scenario.kernel, request, prepared).status ==
         MatrixLiveStatus::Interrupted);
  assert(std::ftell(scenario.journal) == 0);
  unchanged.check();
}

TEST(helper_namespace_is_monotone) {
  Scenario scenario;
  Kernel &kernel = scenario.kernel;
  const uint32_t before = kernel.external_to_internal.size();
  const auto result = apply_affine_matrix_live(kernel, scenario.request());
  assert(result.status == MatrixLiveStatus::Applied && result.helper_extent &&
         kernel.external_to_internal.size() == before + result.helper_extent);
  Var first = INVALID_VAR;
  assert(kernel_reserve_proof_variables(kernel, 2, first));
  assert(first == before + result.helper_extent);
}

using Rows = std::vector<std::pair<std::vector<int32_t>, bool>>;

// XOR rows (x1+x2+x3+x4+x9+x10 = 0 and x5+..+x10 = 1 by default) under root
// units (-1..-8): the owner is captured before the units, whose propagation
// leaves a conflict. The image keeps the rows' order; synchronized sets event
// at the root trail.
struct RootConflict {
  const Rows rows;
  const std::vector<int32_t> units;
  Kernel kernel;
  FILE *journal = temporary_records();
  AffineLiveSnapshot owner;
  Image image;
  AffineMatrix matrix;
  MatrixEvent event;
  explicit RootConflict(bool synchronized = true,
                        Rows xor_rows = {{{1, 2, 3, 4, 9, 10}, false},
                                         {{5, 6, 7, 8, 9, 10}, true}},
                        std::vector<int32_t> root_units = {-1, -2, -3, -4,
                                                           -5, -6, -7, -8})
      : rows(std::move(xor_rows)), units(std::move(root_units)) {
    int32_t extent = 0;
    for (const auto &row : rows)
      for (int32_t variable : row.first) extent = std::max(extent, variable);
    kernel_reserve_vars(kernel, extent);
    for (const auto &[variables, rhs] : rows) add_equation(kernel, variables, rhs);
    const auto capture = capture_request(journal);
    assert(capture_affine_live_snapshot(kernel, capture, owner));
    assert(recognize(owner.premises, capture.image,
                     [](uint64_t) { return true; }, image) == Status::Complete);
    const auto rank = [&](const Equation &equation) {
      const auto row = std::find(rows.begin(), rows.end(),
                                 std::make_pair(equation.variables, equation.rhs));
      assert(row != rows.end());
      return row - rows.begin();
    };
    assert(image.equations.size() == rows.size());
    std::sort(image.equations.begin(), image.equations.end(),
              [&](const Equation &a, const Equation &b) { return rank(a) < rank(b); });
    for (int32_t unit : units) add_clause(kernel, {unit});
    assert(!kernel_propagate(kernel));
    assert(build_affine_matrix(image.equations, 991, matrix_limits(),
                               [](uint64_t) { return true; }, matrix) ==
           MatrixStatus::Complete);
    if (synchronized)
      assert(matrix.synchronize(trail(kernel), [](uint64_t) { return true; },
                                event) == MatrixStatus::Complete);
  }
  ~RootConflict() { std::fclose(journal); }
  AffineCnf original() const {
    AffineCnf clauses;
    for (const auto &[variables, rhs] : rows)
      for (auto &clause : clausal_form(variables, rhs)) clauses.push_back(std::move(clause));
    for (int32_t unit : units) clauses.push_back({unit});
    return clauses;
  }
  // Seed the PBP input population with original().
  void seed_population(Proof &proof) {
    uint64_t id = 0;
    for (const auto &external : original()) {
      std::vector<Lit> clause;
      for (const int32_t literal : external)
        clause.push_back(kernel_internalize(kernel, literal));
      proof.seed_population(++id, clause.data(), clause.size());
    }
    assert(id == kernel.next_clause_id);
    proof.finish_input_transaction();
    assert(proof.commit_clause_stream());
  }
};

static int proof_fixture(const char *cnf_path, const char *proof_path,
                         bool pbp = false) {
  RootConflict fixture;
  Kernel &kernel = fixture.kernel;
  const MatrixEvent &event = fixture.event;
  assert(event.kind == MatrixEventKind::Conflict);
  Proof proof;
  bind_proof(proof, kernel, proof_path,
             pbp ? ProofFormat::PbpStreaming : ProofFormat::DsrAscii);
  if (pbp) fixture.seed_population(proof);
  auto request = live_request(fixture.journal, fixture.owner, fixture.matrix,
                              fixture.image.equations, event);
  PbpParityBoundsCache pbp_cache;
  if (pbp) {
    request.pbp_parity_cache = &pbp_cache;
    // Persistent search has already journaled this owner's retained sources.
    request.retained_source_provenance = true;
  }
  const auto result = apply_affine_matrix_live(kernel, request);
  assert(result.status == MatrixLiveStatus::Applied && result.conflict &&
         result.compact_pbp == pbp &&
         (!pbp || (result.deletions == 0 && pbp_cache.size() > 0)));
  if (pbp) {
    const std::string provenance = written_bytes(fixture.journal);
    assert(provenance.find("\"source_ordinals\":[") != std::string::npos &&
           provenance.find("\"sources_ref\":{\"retained_owner\":991}") !=
               std::string::npos &&
           provenance.find("\"sources\":[") == std::string::npos);
    std::vector<PbpParityEquation> equations;
    for (const size_t ordinal : event.row.source_equations)
      equations.push_back({fixture.image.equations[ordinal].variables,
                           fixture.image.equations[ordinal].rhs});
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
  write_file(cnf_path, dimacs_text(kernel.external_to_internal.size(),
                                   fixture.original()));
  return 0;
}

// Stream a cached plan for the event at prior_trail into the DSR proof and
// commit its cache entry; the live conflict publication at the root trail
// then resumes from that entry. Writes the refutation and its CNF.
static void resumed_publication(RootConflict &fixture,
                                const std::vector<MatrixAssignment> &prior_trail,
                                const std::vector<size_t> &prior_sources,
                                const std::vector<size_t> &sources,
                                const std::string &cnf_path,
                                const std::string &proof_path) {
  Kernel &kernel = fixture.kernel;
  MatrixEvent prior_event;
  assert(fixture.matrix.synchronize(prior_trail, [](uint64_t) { return true; },
                                    prior_event) == MatrixStatus::Complete);
  assert(prior_event.row.source_equations == prior_sources);
  Proof proof;
  bind_proof(proof, kernel, proof_path.c_str());
  AffineMatrixProofCache cache;
  AffineMatrixTransaction prior;
  assert(prepare_affine_matrix_transaction(
             fixture.matrix, 991, fixture.image.equations, prior_event,
             kernel.external_to_internal.size(), kernel.next_clause_id,
             {10000000, 1000000, 1000000, 100000, 1000000},
             [](uint64_t) { return true; }, prior, &cache,
             proof.persistent_external_epoch()) ==
         MatrixTransactionStatus::Prepared);
  assert(prior.proof_cache_insert());
  Var first_helper = INVALID_VAR;
  assert(kernel_reserve_proof_variables(kernel, prior.helper_extent(), first_helper));
  const auto &support = prior.proof_cache_entry().support_ids;
  for (const MatrixProofEvent &step : prior.proof_events()) {
    if (step.kind == MatrixProofEvent::Kind::Delete) {
      assert(proof.del_external(kernel.next_clause_id, step.deleted_id,
                                step.clause.data(), step.clause.size()));
      continue;
    }
    assert(++kernel.next_clause_id == step.id);
    assert(step.kind == MatrixProofEvent::Kind::Rat
               ? proof.add_external_rat(step.id, step.clause.data(), step.clause.size())
               : proof.add_external_rup(step.id, step.clause.data(), step.clause.size()));
    if (std::find(support.begin(), support.end(), step.id) != support.end())
      assert(proof.retain_external(step.id, step.clause.data(), step.clause.size()));
  }
  assert(cache.commit(MatrixProofCacheEntry(prior.proof_cache_entry()), 0));
  assert(fixture.matrix.synchronize(trail(kernel), [](uint64_t) { return true; },
                                    fixture.event) == MatrixStatus::Complete);
  assert(fixture.event.kind == MatrixEventKind::Conflict &&
         fixture.event.row.source_equations == sources);
  auto request = live_request(fixture.journal, fixture.owner, fixture.matrix,
                              fixture.image.equations, fixture.event);
  request.proof_cache = &cache;
  request.maximum_persistent_bytes = 10000000;
  const auto applied = apply_affine_matrix_live(kernel, request);
  assert(applied.status == MatrixLiveStatus::Applied &&
         applied.proof_cache_resume && applied.proof_cache_insert &&
         applied.conflict);
  assert(kernel_propagate(kernel) && kernel.conflict_valid);
  assert(proof.add_external_rup(++kernel.next_clause_id, nullptr, 0));
  proof.flush();
  kernel.proof = nullptr;
  write_file(cnf_path.c_str(), dimacs_text(kernel.external_to_internal.size(),
                                           fixture.original()));
}

// DIR/resume: the cached row is an implication whose cache entry the conflict
// extends. DIR/removed: the conflict row drops a cached source.
static int resume_proofs(const std::string &directory) {
  {
    RootConflict fixture(false);
    resumed_publication(fixture, {{1, false}, {2, false}, {3, false}, {4, false}, {9, false}},
                        {0}, {0, 1}, directory + "/resume.cnf", directory + "/resume.dsr");
  }
  RootConflict fixture(false,
                       {{{2, 3, 4, 7}, false}, {{5, 6, 7, 9}, true},
                        {{2, 3, 6, 8}, true}, {{2, 4, 7, 8}, false},
                        {{1, 2, 5, 9}, true}, {{1, 3, 6, 7}, false}},
                       {-4, 5, -8, -9});
  resumed_publication(fixture, {{1, true}, {2, true}, {5, false}, {6, true}, {8, true}, {9, true}},
                      {0, 2, 3}, {0, 1, 2}, directory + "/removed.cnf",
                      directory + "/removed.dsr");
  return 0;
}

// Advance the matrix proof cache as an unrelated owner's commit would.
static void commit_unrelated_entry(AffineMatrixProofCache &cache) {
  const uint64_t before = cache.generation();
  MatrixProofCacheEntry entry;
  entry.entry_id = before + 1;
  entry.owner_identity = 7; // never the requests' owner 991
  entry.support_ids = {1};
  assert(cache.commit(std::move(entry), before) &&
         cache.generation() == before + 1);
}

// A clausal plan reads the matrix proof cache and stays bound to it. A
// compact streaming-PBP plan never reads it, so a later commit there must
// not make the compact publication stale (it made Analyze abort on a lazy
// affine reason, Tier D linvrinv5).
TEST(compact_pbp_publication_ignores_unconsulted_matrix_cache) {
  {
    Scenario scenario;
    AffineMatrixProofCache cache;
    auto request = scenario.request();
    request.proof_cache = &cache;
    AffineMatrixLivePrepared prepared;
    assert(prepare_affine_matrix_live(scenario.kernel, request, prepared).status ==
           MatrixLiveStatus::Prepared);
    commit_unrelated_entry(cache);
    const auto stale = publish_affine_matrix_live(scenario.kernel, request, prepared);
    assert(stale.status == MatrixLiveStatus::Stale && !stale.compact_pbp &&
           stale.audit_stage == MatrixLiveResult::AuditStage::KernelSnapshot);
  }

  RootConflict fixture;
  char proof_path[] = "/tmp/tactix-affine-live-compact-cache-XXXXXX";
  const int fd = mkstemp(proof_path);
  assert(fd >= 0);
  close(fd);
  Proof proof;
  bind_proof(proof, fixture.kernel, proof_path, ProofFormat::PbpStreaming);
  fixture.seed_population(proof);

  AffineMatrixProofCache cache;
  commit_unrelated_entry(cache);
  PbpParityBoundsCache pbp_cache;
  auto request = live_request(fixture.journal, fixture.owner, fixture.matrix,
                              fixture.image.equations, fixture.event);
  request.proof_cache = &cache;
  request.pbp_parity_cache = &pbp_cache;
  request.retained_source_provenance = true;
  const auto result = apply_affine_matrix_live(fixture.kernel, request);
  assert(result.status == MatrixLiveStatus::Applied && result.compact_pbp &&
         result.conflict && cache.generation() == 1 && pbp_cache.size() > 0);
  fixture.kernel.proof = nullptr;
  unlink(proof_path);
}

int main(int argc, char **argv) {
  if (argc == 4 && std::strcmp(argv[1], "--proof") == 0)
    return proof_fixture(argv[2], argv[3]);
  if (argc == 4 && std::strcmp(argv[1], "--pbp-proof") == 0)
    return proof_fixture(argv[2], argv[3], true);
  if (argc == 3 && std::strcmp(argv[1], "--resume-proofs") == 0)
    return resume_proofs(argv[2]);
  run_tests();
  std::puts("test_affine_matrix_live: passed");
}
