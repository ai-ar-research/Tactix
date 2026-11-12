#undef NDEBUG
#include "affine_formula_support.hpp"
#include "dimacs_text.hpp"
#include "test_io.hpp"
#include "kernel/clause.hpp"
#include "strategy/online_inprocess.hpp"
#include "policies/restart_controller.hpp"
#include "rules/semantic_extension/direct_csp_support_action.hpp"
#include "rules/semantic_extension/nary_csp_equality_action.hpp"
#include "rules/semantic_extension/gf3_affine_action.hpp"
#include "rules/semantic_extension/gf3_gaussian_action.hpp"
#include "rules/semantic_extension/gf3_general_row_action.hpp"
#include "rules/lucky/tseitin_cycle_action.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/reduce/reduce.hpp"
#include "rules/semantic_extension/affine_matrix_search.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <unistd.h>

using namespace tactix;
using namespace tactix::affine_semantic_extension;
using namespace tactix::strategy;
using namespace tactix::strategy::controller;

namespace {

struct Capture final : FeedbackReceiver {
  std::vector<ActionFeedback> actions;
  void after_action(const ActionFeedback &record) override {
    actions.push_back(record);
  }
  void search_interval(const SearchIntervalFeedback &) override {}
};

AffineConsequenceParameters parameters() {
  AffineConsequenceParameters result;
  result.maximum_clauses = 10000;
  result.maximum_snapshot_literals = 100000;
  result.maximum_snapshot_work = 1000000;
  result.maximum_groups = 1000;
  result.maximum_patterns = 10000;
  result.maximum_elimination_work = 10000000;
  result.maximum_arity = 8;
  result.maximum_intermediate_width = 32;
  result.maximum_sources = 32;
  result.maximum_output_width = 4;
  result.maximum_proof_additions = 1000000;
  result.maximum_proof_deletions = 1000000;
  result.maximum_journal_bytes = 10000000;
  result.semantics = AffineConsequenceSemantics::CertifiedBasisV2;
  result.basis_maximum_work = 10000000;
  result.basis_maximum_intermediate_width = 32;
  result.basis_maximum_sources = 32;
  result.basis_maximum_premises = 10000;
  result.basis_maximum_nodes = 10000;
  result.basis_maximum_direct_width = 3;
  result.maximum_external_extent = 100000;
  result.maximum_session_keys = 10000;
  result.maximum_session_work = 1000000;
  assert(result.valid());
  return result;
}

const AffineMatrixPersistenceLimits persistence{64 * 1024 * 1024, 1000, 100000};

// The persistent-matrix initialization the scenarios start from.
void initialize_persistent(Kernel &kernel, AffineMatrixSearchSession &session) {
  assert(affine_matrix_search_initialize(
             kernel, session, parameters(),
             AffineMatrixAdmission::AuthenticatedSubimage, persistence)
             .status == AffineMatrixInitStatus::Applied);
}

// The AffineMatrixInitialize profile of one persistent-session operation.
AffineMatrixInitializeParameters persistent_profile(
    AffineMatrixInitializeParameters::Operation operation) {
  AffineMatrixInitializeParameters profile;
  profile.basis = parameters();
  profile.admission =
      AffineMatrixInitializeParameters::Admission::AuthenticatedSubimage;
  profile.operation = operation;
  profile.maximum_retained_bytes = persistence.maximum_retained_bytes;
  profile.maximum_deferred_deletions = persistence.maximum_deferred_deletions;
  profile.maximum_deferred_literal_work = persistence.maximum_deferred_literal_work;
  assert(profile.valid());
  return profile;
}

// Add the binary (first second) with id and make it the current conflict.
void binary_conflict(Kernel &kernel, int32_t first, int32_t second, uint64_t id) {
  const Lit conflict[2] = {kernel_internalize(kernel, first),
                           kernel_internalize(kernel, second)};
  kernel_add_binary_clause(kernel, conflict[0], conflict[1], id);
  kernel.conflict_ref = INVALID_REF;
  kernel.conflict_bin[0] = conflict[0];
  kernel.conflict_bin[1] = conflict[1];
  kernel.conflict_valid = true;
}

// Two independent implication pairs: x1 = x2 = 0 implies x5 = 0 and
// x6 = x7 = 0 implies x10 = 0.
AffineCnf paired_implications() {
  return xor_cnf({{{1, 3, 4}, false}, {{2, 3, 4, 5}, false},
                  {{6, 8, 9}, false}, {{7, 8, 9, 10}, false}});
}

// Bind the variable maps and seed the PBP input population with original.
void seed_pbp_population(Proof &proof, Kernel &kernel,
                         const std::vector<std::vector<int32_t>> &original) {
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  uint64_t id = 0;
  for (const auto &external : original) {
    std::vector<Lit> clause;
    for (int32_t literal : external)
      clause.push_back(kernel_internalize(kernel, literal));
    proof.seed_population(++id, clause.data(), clause.size());
  }
  assert(id == kernel.next_clause_id);
  proof.finish_input_transaction();
  assert(proof.commit_clause_stream());
}

// Create the mkstemp file a proof is then opened on.
void reserve_path(char *path) {
  const int fd = mkstemp(path);
  assert(fd >= 0);
  close(fd);
}

// A kernel with an attached matrix session and journal. open_dsr binds a DSR
// proof (on a fresh temporary file unless a path is given) over an empty
// armed population; open_pbp seeds a streaming-PBP proof with original.
struct SearchFixture {
  Kernel kernel;
  AffineMatrixSearchSession session;
  FILE *journal = temporary_records();
  char temporary[64] = "/tmp/tactix-affine-search-XXXXXX";
  bool owns_path = false;
  Proof proof;
  SearchFixture() {
    kernel.affine_matrix_search_session = &session;
    kernel.structural_provenance = journal;
  }
  ~SearchFixture() {
    kernel.proof = nullptr;
    if (owns_path) unlink(temporary);
    std::fclose(journal);
  }
  const char *reserve() {
    reserve_path(temporary);
    owns_path = true;
    return temporary;
  }
  Proof &open_dsr(const char *path = nullptr, bool bind_maps = false) {
    assert(proof.open(path ? path : reserve(), ProofFormat::DsrAscii));
    if (bind_maps) {
      proof.bind_variable_map(&kernel.internal_to_external);
      proof.bind_external_variable_map(&kernel.external_to_internal);
    }
    proof.arm_population_shadow();
    proof.finish_input_transaction();
    kernel.proof = &proof;
    return proof;
  }
  Proof &open_pbp(const AffineCnf &original) {
    assert(proof.open(reserve(), ProofFormat::PbpStreaming));
    seed_pbp_population(proof, kernel, original);
    kernel.proof = &proof;
    return proof;
  }
};

ActionRequest initialize_request(RestartController &controller, Kernel &kernel,
                                 const AffineConsequenceParameters &p) {
  ActionRequest request;
  request.sequence = controller.observe().sequence + 1;
  request.expected = state_version(kernel);
  request.invocation.rule = "AffineMatrixInitialize";
  request.invocation.parameters = p;
  request.invocation.granularity = InvocationGranularity::CanonicalApplication;
  request.invocation.repeat = {RepeatMode::Once, 1};
  request.invocation.work_unit = WorkUnit::Applications;
  request.invocation.budget = WorkLimit::bounded(1);
  return request;
}

ActionRequest resume_request(RestartController &controller, Kernel &kernel) {
  ActionRequest request;
  request.type = ActionType::ResumeSearch;
  request.sequence = controller.observe().sequence + 1;
  request.expected = state_version(kernel);
  request.search = {Branching::Vmtf, Restart::Reluctant, Phase::Saved};
  return request;
}

void install_partial_formula(Kernel &kernel) {
  kernel_reserve_vars(kernel, 10);
  add_equation(kernel, {1, 3, 4}, false);
  add_equation(kernel, {2, 3, 4, 5}, false);
  add_equation(kernel, {6, 7, 8, 9, 10}, true);
  assert(kernel_propagation_fixpoint(kernel));
}

void complete_snapshot_policy_admission() {
  {
    SearchFixture fixture;
    Kernel &kernel = fixture.kernel;
    AffineMatrixSearchSession &session = fixture.session;
    install_implication_formula(kernel);
    auto &profile=kernel.affine_matrix_initialize_parameters;
    profile.basis=parameters();
    profile.admission=AffineMatrixInitializeParameters::Admission::CompleteSnapshot;
    profile.operation=AffineMatrixInitializeParameters::Operation::Initialize;
    kernel.affine_matrix_initialize_parameters_active=true;
    const auto &rule=tactix_rules()[tactix_rule_id("AffineMatrixInitialize")];
    assert(!kernel.proof&&rule_guard(rule,kernel));
    const auto result = affine_matrix_search_initialize(
        kernel, session, parameters(),
        AffineMatrixAdmission::CompleteSnapshot);
    assert(result.status == AffineMatrixInitStatus::Applied &&
           result.covered_clauses == result.snapshot_clauses &&
           result.covered_clauses == kernel_irredundant_clauses(kernel) &&
           session.enabled() && session.complete_admission());
    profile.operation=AffineMatrixInitializeParameters::Operation::SearchStep;
    kernel.model_extended=true;
    assert(rule_guard(rule,kernel)); // non-persistent, no-proof root service
    kernel.model_extended=false;
  }
  {
    SearchFixture fixture;
    Kernel &kernel = fixture.kernel;
    AffineMatrixSearchSession &session = fixture.session;
    kernel_reserve_vars(kernel, 7);
    add_equation(kernel, {1, 2, 3}, false);
    add_clause(kernel, {4, 5, 6, 7});
    auto ternary = parameters();
    ternary.maximum_arity = 3;
    const auto result = affine_matrix_search_initialize(
        kernel, session, ternary,
        AffineMatrixAdmission::CompleteSnapshot);
    assert(result.status == AffineMatrixInitStatus::IncompleteImage &&
           result.snapshot_clauses + 1 ==
               kernel_irredundant_clauses(kernel) &&
           result.covered_clauses == result.snapshot_clauses &&
           !session.enabled());
  }
  {
    SearchFixture fixture;
    Kernel &kernel = fixture.kernel;
    AffineMatrixSearchSession &session = fixture.session;
    install_implication_formula(kernel);
    add_clause(kernel, {1, 2, 6});
    const auto result = affine_matrix_search_initialize(
        kernel, session, parameters(),
        AffineMatrixAdmission::CompleteSnapshot);
    assert(result.status == AffineMatrixInitStatus::IncompleteImage &&
           result.covered_clauses + 1 == result.snapshot_clauses &&
           !session.enabled() &&
           kernel.stats.affine_matrix_init_incomplete_image == 1);
  }
  {
    SearchFixture fixture;
    Kernel &kernel = fixture.kernel;
    AffineMatrixSearchSession &session = fixture.session;
    install_partial_formula(kernel);
    auto limited = parameters();
    limited.basis_maximum_intermediate_width = 4;
    const auto result = affine_matrix_search_initialize(
        kernel, session, limited,
        AffineMatrixAdmission::CompleteSnapshot);
    assert(result.status == AffineMatrixInitStatus::IncompleteMatrix &&
           !session.enabled() &&
           kernel.stats.affine_matrix_init_incomplete_matrix == 1);
  }
}

void deferred_reason_has_public_materialization_boundary() {
  SearchFixture fixture;
  Kernel &kernel = fixture.kernel;
  AffineMatrixSearchSession &session = fixture.session;
  install_implication_formula(kernel);
  Proof &proof = fixture.open_dsr();
  initialize_persistent(kernel, session);
  decide_all(kernel, {-1, -2});

  auto profile = persistent_profile(
      AffineMatrixInitializeParameters::Operation::DeferredSearchStep);
  kernel.affine_matrix_initialize_parameters = profile;
  kernel.affine_matrix_initialize_parameters_active = true;
  const RuleVTable &materialize =
      tactix_rules()[tactix_rule_id("AffineMatrixInitialize")];
  assert(rule_guard(materialize, kernel));
  assert(apply_rule(materialize, kernel) == ApplyResult::Ok);
  assert(session.last_service());
  const auto deferred = *session.last_service();
  assert(deferred.status == AffineMatrixServiceStatus::Applied &&
         deferred.reason_deferred && deferred.reason_width == 3 &&
         deferred.bcp_pending && session.has_deferred_reason() &&
         kernel.value(kernel_internalize(kernel, -5)) > 0);
  const Ref provisional = kernel.assigned[4].reason;
  assert(!kernel.assigned[4].binary &&
         clause_at(kernel.arena, provisional)->id() == 0);

  profile.operation =
      AffineMatrixInitializeParameters::Operation::MaterializeReasons;
  assert(profile.valid());
  kernel.affine_matrix_initialize_parameters = profile;
  const RuleVTable &analyze =
      tactix_rules()[tactix_rule_id("ConflictAnalysis")];
  assert(rule_guard(materialize, kernel));
  assert(!rule_guard(analyze, kernel));

  assert(apply_rule(materialize, kernel) == ApplyResult::Ok);
  assert(session.last_service());
  const auto materialized = *session.last_service();
  assert(materialized.status == AffineMatrixServiceStatus::Applied &&
         materialized.reason_materialized && !session.has_deferred_reason() &&
         kernel.assigned[4].reason != provisional &&
         clause_at(kernel.arena, kernel.assigned[4].reason)->id() > 0 &&
         clause_at(kernel.arena, provisional)->garbage());
  proof.flush();
}

void detached_minimal_reason_is_public_and_unwatched() {
  SearchFixture fixture;
  Kernel &kernel = fixture.kernel;
  AffineMatrixSearchSession &session = fixture.session;
  kernel_reserve_vars(kernel, 7);
  add_equation(kernel, {1, 2, 3, 4, 6, 7}, false);
  add_equation(kernel, {3, 4, 5, 6, 7}, true);
  Proof &proof = fixture.open_dsr();
  initialize_persistent(kernel, session);
  decide_all(kernel, {-1, -2, -3, -4});

  auto profile = persistent_profile(
      AffineMatrixInitializeParameters::Operation::DetachedMinimalSearchStep);
  kernel.affine_matrix_initialize_parameters = profile;
  kernel.affine_matrix_initialize_parameters_active = true;
  const RuleVTable &rule =
      tactix_rules()[tactix_rule_id("AffineMatrixInitialize")];
  assert(rule_guard(rule, kernel));
  assert(apply_rule(rule, kernel) == ApplyResult::Ok);
  assert(session.last_service());
  const auto service = *session.last_service();
  assert(service.status == AffineMatrixServiceStatus::Applied &&
         service.reason_width == 3 && service.live.detached_reason &&
         service.live.implication_consumed &&
         kernel.value(kernel_internalize(kernel, 5)) > 0);
  const Ref ref = kernel.assigned[var_of(kernel_internalize(kernel, 5))].reason;
  const Clause *reason = clause_at(kernel.arena, ref);
  assert(reason->garbage() && reason->reason() && reason->detached_reason() &&
         reason->id());
  assert(reason->size == 3);
  for (Lit lit = 0; lit < 2 * kernel.nvars; ++lit)
    for (const WatchEntry watch : kernel.watches.entries(lit))
      assert(watch.binary() || watch.ref() != ref);

  proof.flush();
}

void lazy_source_handles_are_multiple_public_and_consumed_on_demand() {
  SearchFixture fixture;
  Kernel &kernel = fixture.kernel;
  AffineMatrixSearchSession &session = fixture.session;
  kernel_reserve_vars(kernel, 10);
  for (const auto &clause : paired_implications()) add_clause(kernel, clause);
  Proof &proof = fixture.open_dsr();
  initialize_persistent(kernel, session);

  decide_all(kernel, {-1, -2});
  const uint64_t before_publication = kernel.next_clause_id;
  const size_t arena_before_publication = kernel.arena.size();
  const auto first = affine_matrix_search_step_lazy_source(kernel, session);
  assert(first.status == AffineMatrixServiceStatus::Applied &&
         first.reason_deferred && session.lazy_reason_count() == 1 &&
         kernel.value(kernel_internalize(kernel, -5)) > 0 &&
         kernel.next_clause_id == before_publication &&
         kernel.arena.size() == arena_before_publication);
  assert(kernel.assigned[4].affine &&
         kernel.assigned[4].reason == AFFINE_REASON &&
         session.has_lazy_reason(4));
  assert(propagate_apply(kernel) == ApplyResult::Ok);
  // Reduce is publicly legal during search. Its reason locking and an arena
  // collection must treat the native algebraic handle as session-owned, not
  // as a clause Ref.
  assert(reduce_apply(kernel) == ApplyResult::NoOp);
  kernel_collect_garbage(kernel);
  assert(kernel.assigned[4].affine &&
         kernel.assigned[4].reason == AFFINE_REASON);
  kernel_check_invariants(kernel);

  decide_all(kernel, {-6, -7});
  const auto second = affine_matrix_search_step_lazy_source(kernel, session);
  assert(second.status == AffineMatrixServiceStatus::Applied &&
         session.lazy_reason_count() == 2 &&
         kernel.value(kernel_internalize(kernel, -10)) > 0);

  // MaterializeReasons is the public eager boundary as well as Analyze's
  // automatic exact-consumer boundary. It discharges one live handle per
  // invocation and retains the certified source-support clause as watched.
  auto profile = persistent_profile(
      AffineMatrixInitializeParameters::Operation::MaterializeReasons);
  kernel.affine_matrix_initialize_parameters = profile;
  kernel.affine_matrix_initialize_parameters_active = true;
  const RuleVTable &materialize =
      tactix_rules()[tactix_rule_id("AffineMatrixInitialize")];
  assert(rule_guard(materialize, kernel));
  assert(apply_rule(materialize, kernel) == ApplyResult::Ok);
  assert(session.last_service());
  const auto materialized_first = *session.last_service();
  assert(materialized_first.status == AffineMatrixServiceStatus::Applied &&
         materialized_first.reason_materialized &&
         session.lazy_reason_count() == 1 &&
         !kernel.assigned[4].affine &&
         kernel.assigned[4].reason != AFFINE_REASON);
  const Ref watched_ref = kernel.assigned[4].reason;
  const Clause *watched = clause_at(kernel.arena, watched_ref);
  assert(!watched->garbage() && watched->id());
  uint32_t watches = 0;
  for (Lit lit = 0; lit < 2 * kernel.nvars; ++lit)
    for (const WatchEntry watch : kernel.watches.entries(lit))
      watches += !watch.binary() && watch.ref() == watched_ref;
  assert(watches == 2);
  assert(rule_guard(materialize, kernel));
  assert(apply_rule(materialize, kernel) == ApplyResult::Ok);
  assert(session.last_service()->reason_materialized &&
         session.lazy_reason_count() == 0);

  proof.flush();
}

void lazy_minimal_handle_is_public_and_consumed_on_demand() {
  SearchFixture fixture;
  Kernel &kernel = fixture.kernel;
  AffineMatrixSearchSession &session = fixture.session;
  install_implication_formula(kernel);
  Proof &proof = fixture.open_dsr();
  initialize_persistent(kernel, session);
  decide_all(kernel, {-1, 2});
  const auto lazy = affine_matrix_search_step_lazy_minimal(kernel, session);
  assert(lazy.status == AffineMatrixServiceStatus::Applied &&
         lazy.reason_deferred && lazy.reason_width == 3 &&
         lazy.installed_reason.empty() && session.lazy_reason_count() == 1 &&
         !session.lazy_reason_vector_constructed(4) &&
         lazy.event_identity.find(";r=1,-2,5") != std::string::npos);
  std::vector<Var> antecedents;
  assert(session.inspect_lazy_reason(kernel, 4, antecedents));
  assert((antecedents == std::vector<Var>{0, 1}) &&
         !session.lazy_reason_vector_constructed(4));
  volatile sig_atomic_t stop = 1;
  kernel.stop = &stop;
  const auto interrupted = affine_matrix_materialize_reason(kernel, session);
  assert(interrupted.status == AffineMatrixServiceStatus::Interrupted &&
         session.lazy_reason_count() == 1 && kernel.assigned[4].affine &&
         session.lazy_reason_vector_constructed(4));
  kernel.stop = nullptr;
  const auto materialized = affine_matrix_materialize_reason(kernel, session);
  assert(materialized.status == AffineMatrixServiceStatus::Applied &&
         materialized.reason_materialized &&
         !materialized.reason_vector_constructed &&
         materialized.reason_width == 3 &&
         (materialized.installed_reason ==
          std::vector<int32_t>{1, -2, 5}) &&
         materialized.event_identity == lazy.event_identity &&
         session.lazy_reason_count() == 0);
  proof.flush();
}

void lazy_minimal_materializes_only_analyze_ancestor() {
  SearchFixture fixture;
  Kernel &kernel = fixture.kernel;
  AffineMatrixSearchSession &session = fixture.session;
  kernel_reserve_vars(kernel, 10);
  for (const auto &clause : paired_implications()) add_clause(kernel, clause);
  Proof &proof = fixture.open_dsr();
  initialize_persistent(kernel, session);
  decide_all(kernel, {-1, -2});
  const auto first = affine_matrix_search_step_lazy_minimal(kernel, session);
  assert(first.status == AffineMatrixServiceStatus::Applied &&
         first.installed_reason.empty() && session.lazy_reason_count() == 1 &&
         !session.lazy_reason_vector_constructed(4));
  assert(kernel.assigned[4].affine &&
         kernel.assigned[4].reason == AFFINE_REASON);
  assert(propagate_apply(kernel) == ApplyResult::Ok);
  decide_all(kernel, {-6, -7});
  const auto second = affine_matrix_search_step_lazy_minimal(kernel, session);
  assert(second.status == AffineMatrixServiceStatus::Applied &&
         second.installed_reason.empty() && session.lazy_reason_count() == 2 &&
         !session.lazy_reason_vector_constructed(4) &&
         !session.lazy_reason_vector_constructed(9));
  assert(kernel.assigned[9].affine &&
         kernel.assigned[9].reason == AFFINE_REASON);

  // Compaction cannot remap an opaque historical algebraic event. It remains
  // publicly inapplicable until the handles have been discharged.
  assert(!kernel_compact_variables(kernel));

  // The independent first implication is not in this conflict's reason
  // ancestry. ConflictAnalysis certifies the second before capturing any arena
  // pointer or proof chain, without eagerly publishing the unrelated handle.
  binary_conflict(kernel, 7, 10, ++kernel.next_clause_id);
  const RuleVTable &analyze =
      tactix_rules()[tactix_rule_id("ConflictAnalysis")];
  assert(rule_guard(analyze, kernel));
  // ConflictAnalysis has already crossed its atomic rule boundary. A stop
  // observed while making an opaque affine ancestor proof-visible is honored
  // after the learned transition, not by abandoning its required reason.
  volatile sig_atomic_t stop = 1;
  kernel.stop = &stop;
  assert(apply_rule(analyze, kernel) == ApplyResult::Ok);
  kernel.stop = nullptr;
  assert(session.lazy_reason_count() == 1 && kernel.assigned[4].affine &&
         kernel.assigned[4].reason == AFFINE_REASON &&
         !session.lazy_reason_vector_constructed(4) &&
         kernel.stats.affine_matrix_analyze_handles == 1 &&
         kernel.stats.affine_matrix_analyze_materializations == 1 &&
         kernel.stats.affine_matrix_analyze_vectors_constructed == 1 &&
         kernel.stats.affine_matrix_analyze_literals_constructed == 3);
  proof.flush();
}

// When no budget remains for a handle's clauses, ConflictAnalysis withdraws
// every handle and ends parity reasoning instead of aborting the run. Nothing
// is learned and the search continues from below the lowest handle.
void unaffordable_lazy_reasons_are_withdrawn() {
  SearchFixture fixture;
  Kernel &kernel = fixture.kernel;
  AffineMatrixSearchSession &session = fixture.session;
  kernel_reserve_vars(kernel, 10);
  for (const auto &clause : paired_implications()) add_clause(kernel, clause);
  Proof &proof = fixture.open_dsr();
  initialize_persistent(kernel, session);
  decide_all(kernel, {-1, -2});
  assert(affine_matrix_search_step_lazy_minimal(kernel, session).reason_deferred);
  assert(propagate_apply(kernel) == ApplyResult::Ok);
  const uint32_t lowest = kernel.assigned[4].level;
  decide_all(kernel, {-6, -7});
  assert(affine_matrix_search_step_lazy_minimal(kernel, session).reason_deferred);
  assert(session.lazy_reason_count() == 2 && lowest == 2);

  binary_conflict(kernel, 7, 10, ++kernel.next_clause_id);
  const uint64_t action_sequence = kernel.structural_action_sequence;
  kernel.structural_action_sequence = UINT64_MAX; // materialization is exhausted
  const RuleVTable &analyze =
      tactix_rules()[tactix_rule_id("ConflictAnalysis")];
  assert(rule_guard(analyze, kernel));
  assert(apply_rule(analyze, kernel) == ApplyResult::Ok);
  kernel.structural_action_sequence = action_sequence;
  assert(kernel.stats.affine_matrix_analyze_retractions == 1 &&
         kernel.affine_reasons_unaffordable && !session.enabled() &&
         !session.lazy_reason_count() && kernel.level == lowest - 1 &&
         !kernel.conflict_valid && !kernel.pending.valid &&
         !kernel.value(kernel_internalize(kernel, 4)) &&
         !kernel.value(kernel_internalize(kernel, 9)) &&
         kernel.stats.learned == 0);
  assert(propagate_apply(kernel) == ApplyResult::Ok);
  proof.flush();
}

// Streaming PBP plans a compact reason without the matrix proof cache, and
// its encoder accepts only source equations of 2..8 variables. Root
// maintenance gives an archived root fact a 1-variable source equation, so
// the next reason citing it takes the clausal bridge and commits a matrix
// proof cache entry. An outstanding compact lazy handle must stay
// publishable when ConflictAnalysis needs it (it was misjudged Stale and the
// solver aborted on Tier D linvrinv5).
void lazy_compact_pbp_reason_survives_matrix_cache_commit() {
  SearchFixture fixture;
  Kernel &kernel = fixture.kernel;
  AffineMatrixSearchSession &session = fixture.session;
  kernel_reserve_vars(kernel, 11);
  const AffineCnf original =
      xor_cnf({{{2, 4, 5}, false}, {{3, 4, 5, 6}, false},
               {{7, 9, 10}, false}, {{8, 9, 10, 11}, false}});
  for (const auto &clause : original) add_clause(kernel, clause);
  Proof &proof = fixture.open_pbp(original);
  const auto p = parameters();
  assert(affine_matrix_search_initialize(
             kernel, session, p,
             AffineMatrixAdmission::AuthenticatedSubimage, persistence)
             .status == AffineMatrixInitStatus::Applied);

  // Late root facts: 1 stays the fixed representative, 2 is archived and
  // root maintenance names it by the 1-variable source equation 2 = 0.
  add_clause(kernel, {-1});
  add_clause(kernel, {-2});
  assert(!kernel_propagate(kernel) && kernel_propagation_fixpoint(kernel));
  assert(kernel_sparse_collect_root(kernel, 0) !=
         RootCollectOutcome::NoChange);
  kernel_collect_garbage(kernel);
  assert(kernel_compact_variables(kernel, true) &&
         kernel_internalize(kernel, 2) == INVALID_LIT &&
         kernel_internalize(kernel, 1) != INVALID_LIT);
  const auto maintained =
      affine_matrix_search_refresh(kernel, session, p, persistence, false);
  assert(maintained.status == AffineMatrixInitStatus::AppliedPartial &&
         maintained.result == ApplyResult::Ok);
  assert(propagate_apply(kernel) == ApplyResult::Ok);

  kernel_assign_decision(kernel, kernel_internalize(kernel, -3));
  assert(propagate_apply(kernel) == ApplyResult::Ok);
  // The reason cites the root-unit source, so the compact encoder declines.
  const auto clausal = affine_matrix_search_step(kernel, session);
  assert(clausal.status == AffineMatrixServiceStatus::Applied &&
         !clausal.live.compact_pbp && clausal.live.proof_cache_insert);
  assert(propagate_apply(kernel) == ApplyResult::Ok &&
         kernel.value(kernel_internalize(kernel, -6)) > 0);

  decide_all(kernel, {-7, -8});
  const auto lazy = affine_matrix_search_step_lazy_minimal(kernel, session);
  const Var implied = var_of(kernel_internalize(kernel, 11));
  assert(lazy.status == AffineMatrixServiceStatus::Applied &&
         lazy.reason_deferred && session.lazy_reason_count() == 1 &&
         kernel.assigned[implied].affine &&
         kernel.assigned[implied].reason == AFFINE_REASON);

  binary_conflict(kernel, 8, 11, ++kernel.next_clause_id);
  const RuleVTable &analyze =
      tactix_rules()[tactix_rule_id("ConflictAnalysis")];
  assert(rule_guard(analyze, kernel));
  assert(apply_rule(analyze, kernel) == ApplyResult::Ok);
  assert(session.lazy_reason_count() == 0 &&
         kernel.stats.affine_matrix_analyze_handles == 1 &&
         kernel.stats.affine_matrix_analyze_materializations == 1);
  proof.flush();
}

void lazy_source_backtrack_discards_without_publication() {
  SearchFixture fixture;
  Kernel &kernel = fixture.kernel;
  AffineMatrixSearchSession &session = fixture.session;
  install_implication_formula(kernel);
  Proof &proof = fixture.open_dsr();
  initialize_persistent(kernel, session);
  decide_all(kernel, {-1, -2});
  const uint64_t before = kernel.next_clause_id;
  const uint64_t retained_before = session.retained_bytes();
  const auto lazy = affine_matrix_search_step_lazy_source(kernel, session);
  assert(lazy.status == AffineMatrixServiceStatus::Applied &&
         session.lazy_reason_count() == 1 && kernel.next_clause_id == before &&
         session.retained_bytes() > retained_before);
  const uint64_t retained_with_reason = session.retained_bytes();
  assert(kernel.assigned[4].affine &&
         kernel.assigned[4].reason == AFFINE_REASON);
  kernel_backtrack(kernel, 0);
  assert(!kernel.values[make_lit(4, false)] &&
         session.lazy_reason_count() == 0 &&
         session.retained_bytes() < retained_with_reason);
  const auto root = affine_matrix_search_step_lazy_source(kernel, session);
  assert(root.status == AffineMatrixServiceStatus::NoEvent &&
         session.lazy_reason_count() == 0 && kernel.next_clause_id == before);
  session.disable(kernel);
  assert(!session.enabled());
  proof.flush();
}

void deferred_reason_materializes_at_later_conflict(bool pbp) {
  SearchFixture fixture;
  Kernel &kernel = fixture.kernel;
  AffineMatrixSearchSession &session = fixture.session;
  kernel_reserve_vars(kernel, 7);
  AffineCnf original = xor_cnf({{{1, 3, 4}, false}, {{2, 3, 4, 5}, false}});
  // Once affine reasoning assigns -5, deciding -6 makes the first clause
  // imply 7 and the second imply -7: materialization meets an active Boolean
  // conflict after an intervening decision and BCP.
  original.push_back({5, 6, 7});
  original.push_back({6, -7});
  for (const auto &clause : original) add_clause(kernel, clause);
  if (pbp) fixture.open_pbp(original);
  else fixture.open_dsr(nullptr, true);
  initialize_persistent(kernel, session);
  decide_all(kernel, {-1, -2});
  const auto deferred = affine_matrix_search_step_deferred(kernel, session);
  assert(deferred.status == AffineMatrixServiceStatus::Applied &&
         deferred.reason_deferred && session.has_deferred_reason());
  assert(propagate_apply(kernel) == ApplyResult::Ok);
  kernel_assign_decision(kernel, kernel_internalize(kernel, -6));
  assert(propagate_apply(kernel) == ApplyResult::Conflict &&
         kernel.conflict_valid);
  const auto materialized = affine_matrix_materialize_reason(kernel, session);
  assert(materialized.status == AffineMatrixServiceStatus::Applied &&
         materialized.reason_materialized && !session.has_deferred_reason());
}

void repeated_deferred_pbp_reasons_preserve_cache_generation() {
  SearchFixture fixture;
  Kernel &kernel = fixture.kernel;
  AffineMatrixSearchSession &session = fixture.session;
  kernel_reserve_vars(kernel, 10);
  const AffineCnf original = paired_implications();
  for (const auto &clause : original) add_clause(kernel, clause);
  fixture.open_pbp(original);
  initialize_persistent(kernel, session);
  for (const auto &[first, second, implied] :
       std::vector<std::tuple<int32_t, int32_t, int32_t>>{
           {-1, -2, -5}, {-6, -7, -10}}) {
    kernel_assign_decision(kernel, kernel_internalize(kernel, first));
    assert(propagate_apply(kernel) == ApplyResult::Ok);
    kernel_assign_decision(kernel, kernel_internalize(kernel, second));
    assert(propagate_apply(kernel) == ApplyResult::Ok);
    const auto deferred = affine_matrix_search_step_deferred(kernel, session);
    assert(deferred.status == AffineMatrixServiceStatus::Applied &&
           deferred.reason_deferred && session.has_deferred_reason());
    assert(propagate_apply(kernel) == ApplyResult::Ok &&
           kernel.value(kernel_internalize(kernel, implied)) > 0);
    const auto materialized = affine_matrix_materialize_reason(kernel, session);
    assert(materialized.status == AffineMatrixServiceStatus::Applied &&
           materialized.reason_materialized && !session.has_deferred_reason());
  }
}

void deferred_pbp_reason_survives_backtrack_and_cache_resume() {
  SearchFixture fixture;
  Kernel &kernel = fixture.kernel;
  AffineMatrixSearchSession &session = fixture.session;
  kernel_reserve_vars(kernel, 5);
  const AffineCnf original = xor_cnf({{{1, 3, 4}, false}, {{2, 3, 4, 5}, false}});
  for (const auto &clause : original) add_clause(kernel, clause);
  fixture.open_pbp(original);
  initialize_persistent(kernel, session);
  for (const auto &[first, implied] :
       std::vector<std::pair<int32_t, int32_t>>{{-1, -5}, {1, 5}}) {
    kernel_assign_decision(kernel, kernel_internalize(kernel, first));
    assert(propagate_apply(kernel) == ApplyResult::Ok);
    kernel_assign_decision(kernel, kernel_internalize(kernel, -2));
    assert(propagate_apply(kernel) == ApplyResult::Ok);
    const auto deferred = affine_matrix_search_step_deferred(kernel, session);
    assert(deferred.status == AffineMatrixServiceStatus::Applied &&
           deferred.reason_deferred && session.has_deferred_reason());
    assert(propagate_apply(kernel) == ApplyResult::Ok &&
           kernel.value(kernel_internalize(kernel, implied)) > 0);
    const auto materialized = affine_matrix_materialize_reason(kernel, session);
    assert(materialized.status == AffineMatrixServiceStatus::Applied &&
           materialized.reason_materialized && !session.has_deferred_reason());
    kernel_backtrack(kernel, 0);
    assert(propagate_apply(kernel) == ApplyResult::Ok);
  }
}

static std::string initialize_partial_fixture() {
  SearchFixture fixture;
  Kernel &kernel = fixture.kernel;
  AffineMatrixSearchSession &session = fixture.session;
  FILE *journal = fixture.journal;
  install_partial_formula(kernel);
  auto p = parameters();
  p.basis_maximum_intermediate_width = 4;
  const auto initialized = affine_matrix_search_initialize(kernel, session, p);
  assert(initialized.status == AffineMatrixInitStatus::AppliedPartial &&
         initialized.result == ApplyResult::Ok && initialized.rows == 2 &&
         initialized.admitted_sources == 2 && initialized.journal_bytes &&
         session.enabled() && !session.complete_admission() &&
         kernel.stats.affine_matrix_init_applied == 1 &&
         kernel.stats.affine_matrix_init_applied_partial == 1);
  const std::string journal_record = written_bytes(journal);
  assert(journal_record.find("\"admission\":\"partial\"") !=
         std::string::npos);
  assert(journal_record.find("\"sources\":[0]") != std::string::npos &&
         journal_record.find("\"sources\":[1]") != std::string::npos &&
         journal_record.find("\"sources\":[2]") == std::string::npos);

  const uint64_t owner = session.owner_version();
  const long before = std::ftell(journal);
  p.basis_maximum_intermediate_width = 1;
  const auto rejected = affine_matrix_search_initialize(kernel, session, p);
  assert(rejected.status == AffineMatrixInitStatus::ResourceExhausted &&
         session.owner_version() == owner && !session.complete_admission() &&
         session.initialization_retry_suppressed(kernel, p) &&
         std::ftell(journal) == before);

  // Make the old conditional event a root event.  The initialized partial
  // owner remains unchanged, while ordinary root units supply its antecedents.
  add_clause(kernel, {-1});
  add_clause(kernel, {-2});
  assert(!kernel_propagate(kernel) && kernel_propagation_fixpoint(kernel));
  RestartController controller(kernel);
  assert(controller.resume_search(resume_request(controller, kernel))
             .resumed_search);
  assert(controller.full_restart() == ApplyResult::Ok &&
         controller.affine_matrix_root_service_due());
  const auto service = controller.affine_matrix_fixpoint_service();
  assert(service.status == AffineMatrixServiceStatus::Applied &&
         service.bcp_pending && service.event_identity.find(";s=0,1;") !=
                                    std::string::npos);
  assert(controller.search_service("Propagate", INSTANCE_CANONICAL, [&] {
    return propagate_apply(kernel);
  }) == ApplyResult::Ok);
  assert(kernel.value(kernel_internalize(kernel, -5)) > 0);
  const std::string deterministic_record = written_bytes(journal);
  assert(deterministic_record.find("\"admission\":\"partial\"") !=
             std::string::npos &&
         deterministic_record.find("\"source_ordinals\":[0,1]") !=
             std::string::npos);
  return deterministic_record;
}

void partial_admission_feedback_provenance_and_replay() {
  const std::string first = initialize_partial_fixture();
  const std::string replay = initialize_partial_fixture();
  assert(first == replay);
}


void root_decline_atomicity() {
  for (bool interrupted : {false, true}) {
    SearchFixture fixture;
    Kernel &kernel = fixture.kernel;
    kernel_reserve_vars(kernel, 7);
    add_equation(kernel, {1, 2, 3}, false);
    add_equation(kernel, {1, 2, 4}, false);
    add_equation(kernel, {3, 4, 5}, true);
    auto p = parameters();
    if (!interrupted) p.maximum_session_work = 1;
    RestartController controller(kernel);
    assert(controller.apply_rule(initialize_request(controller, kernel, p),
                                 {WorkUnit::Applications, 1, {}})
               .invocation.status == InvocationStatus::Completed);
    assert(controller.resume_search(resume_request(controller, kernel)).resumed_search);
    assert(controller.full_restart() == ApplyResult::Ok);
    const auto id = kernel.next_clause_id, owner = fixture.session.owner_version();
    const auto arena = kernel.arena;
    const long position = std::ftell(fixture.journal);
    volatile sig_atomic_t stop = interrupted;
    kernel.stop = &stop;
    const auto result = controller.affine_matrix_fixpoint_service();
    assert(result.status == (interrupted ? AffineMatrixServiceStatus::Interrupted
                                         : AffineMatrixServiceStatus::ResourceExhausted));
    assert(kernel.next_clause_id == id && kernel.arena == arena &&
           fixture.session.owner_version() == owner &&
           !fixture.session.published_keys() &&
           std::ftell(fixture.journal) == position);
    kernel.stop = nullptr;
  }
}

static int root_proof_fixture(const char *cnf_path, const char *proof_path,
                              bool unit, ProofFormat format = ProofFormat::DsrAscii) {
  SearchFixture fixture;
  Kernel &kernel = fixture.kernel;
  AffineMatrixSearchSession &session = fixture.session;
  kernel_reserve_vars(kernel, unit ? 5 : 3);
  AffineCnf original =
      unit ? xor_cnf({{{1, 2, 3}, false}, {{1, 2, 4}, false}, {{3, 4, 5}, true}})
           : xor_cnf({{{1, 2, 3}, false}, {{1, 2, 3}, true}});
  for (const auto &clause : original) add_clause(kernel, clause);
  // The checked unit fixture's extra original clause -5 stays out of the
  // captured matrix, isolating publication of the derived root unit.
  if (unit) {
    ++kernel.next_clause_id;
    original.push_back({-5});
  }
  Proof &proof = fixture.proof;
  assert(proof.open(proof_path, format));
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  uint64_t input_id = 0;
  for (const auto &external : original) {
    std::vector<Lit> clause;
    for (int32_t literal : external) clause.push_back(internalize(literal));
    proof.seed_population(++input_id, clause.data(), clause.size());
  }
  assert(input_id == kernel.next_clause_id);
  proof.finish_input_transaction();
  kernel.proof = &proof;
  assert(affine_matrix_search_initialize(kernel, session, parameters()).status ==
         AffineMatrixInitStatus::Applied);
  const auto service = affine_matrix_search_step(kernel, session);
  assert(service.status == AffineMatrixServiceStatus::Applied);
  if (unit) {
    assert(service.result == ApplyResult::Ok && service.bcp_pending &&
           service.live.implication && !service.live.conflict &&
           service.reason_width == 1 &&
           kernel.value(kernel_internalize(kernel, 5)) > 0);
    assert(proof.add_external_rup(++kernel.next_clause_id, nullptr, 0));
  } else {
    assert(service.result == ApplyResult::Unsat && !service.bcp_pending &&
           service.live.conflict && !service.live.implication &&
           service.reason_width == 0 && kernel.inconsistent);
  }
  proof.flush();
  write_file(cnf_path, dimacs_text(unit ? 5 : 3, original));
  return 0;
}

// The xorshift stream of a fixture seed.
struct XorShift {
  uint32_t state;
  uint32_t operator()() {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
  }
};

// Sorted-row XORs from (variables, rhs) pairs.
AffineCnf sorted_rows(std::vector<std::pair<std::vector<int32_t>, bool>> rows) {
  AffineCnf result;
  for (auto &[row, rhs] : rows) {
    std::sort(row.begin(), row.end());
    for (auto &clause : clausal_form(row, rhs)) result.push_back(std::move(clause));
  }
  return result;
}

// Install the XOR rows (reserving the checker-only input ids), decide, and
// certify the matrix implication of implied through the persistent service;
// the checker CNF adds the decision and the negated implication as units.
static int implication_proof(const char *cnf_path, const char *proof_path,
                             uint32_t variables, AffineCnf original,
                             int32_t decision, int32_t implied,
                             size_t sources = 0) {
  SearchFixture fixture;
  Kernel &kernel = fixture.kernel;
  AffineMatrixSearchSession &session = fixture.session;
  kernel_reserve_vars(kernel, variables);
  for (const auto &clause : original) add_clause(kernel, clause);
  kernel.next_clause_id += 2;
  Proof &proof = fixture.open_dsr(proof_path);
  initialize_persistent(kernel, session);
  assert(affine_matrix_search_step(kernel, session).status ==
         AffineMatrixServiceStatus::NoEvent);
  decide_all(kernel, {decision});
  const auto service = affine_matrix_search_step(kernel, session);
  assert(service.status == AffineMatrixServiceStatus::Applied &&
         service.reason_width == 2 && service.bcp_pending &&
         (!sources || service.live.selected_sources == sources));
  assert(propagate_apply(kernel) == ApplyResult::Ok &&
         kernel_external_value(kernel, internalize(implied)) > 0);
  assert(proof.add_external_rup(++kernel.next_clause_id, nullptr, 0));
  proof.flush();
  original.push_back({decision});
  original.push_back({-implied});
  write_file(cnf_path, dimacs_text(variables, original));
  return 0;
}

// Two rows over four variables; a seed renames the variables, flips both row
// signs and the decision phase. The XOR sum forces the fourth variable.
static int search_proof_fixture(const char *cnf_path, const char *proof_path,
                                uint32_t seed = 0) {
  std::vector<int32_t> v = {1, 2, 3, 4};
  bool first_rhs = false, second_rhs = true, decision_positive = true;
  if (seed) {
    XorShift next{seed};
    for (size_t i = v.size() - 1; i > 0; --i) std::swap(v[i], v[next() % (i + 1)]);
    first_rhs = (next() & 1) != 0;
    second_rhs = (next() & 1) != 0;
    decision_positive = (next() & 1) != 0;
  }
  const bool implied_positive = decision_positive ^ first_rhs ^ second_rhs;
  return implication_proof(
      cnf_path, proof_path, 4,
      sorted_rows({{{v[0], v[1], v[2]}, first_rhs}, {{v[1], v[2], v[3]}, second_rhs}}),
      decision_positive ? v[0] : -v[0], implied_positive ? v[3] : -v[3]);
}

// Four rows cancel five interior variables, leaving only the decision and the
// implied variable: a four-source certificate.
static int search_chain_proof_fixture(const char *cnf_path,
                                      const char *proof_path, uint32_t seed) {
  std::vector<int32_t> v = {1, 2, 3, 4, 5, 6, 7};
  std::array<bool, 4> rhs = {false, false, false, true};
  bool decision_positive = true;
  if (seed) {
    XorShift next{seed};
    for (size_t i = v.size() - 1; i > 0; --i) std::swap(v[i], v[next() % (i + 1)]);
    for (bool &sign : rhs) sign = (next() & 1) != 0;
    decision_positive = (next() & 1) != 0;
  }
  bool implied_positive = decision_positive;
  for (bool sign : rhs) implied_positive ^= sign;
  return implication_proof(
      cnf_path, proof_path, 7,
      sorted_rows({{{v[0], v[1], v[2]}, rhs[0]}, {{v[1], v[3], v[4]}, rhs[1]},
                   {{v[2], v[3], v[5]}, rhs[2]}, {{v[4], v[5], v[6]}, rhs[3]}}),
      decision_positive ? v[0] : -v[0], implied_positive ? v[6] : -v[6], 4);
}

// A deferred (or lazy source / lazy minimal) reason of x5 = 0 after x1 = x2 = 0,
// materialized and refuted with the checker units -1, -2 and 5.
static int deferred_reason_proof_fixture(const char *cnf_path,
                                         const char *proof_path,
                                         bool lazy_source = false,
                                         bool lazy_minimal = false) {
  SearchFixture fixture;
  Kernel &kernel = fixture.kernel;
  AffineMatrixSearchSession &session = fixture.session;
  install_implication_formula(kernel);
  kernel.next_clause_id += 3;
  Proof &proof = fixture.open_dsr(proof_path);
  initialize_persistent(kernel, session);
  decide_all(kernel, {-1, -2});
  const auto deferred = lazy_minimal
      ? affine_matrix_search_step_lazy_minimal(kernel, session)
      : lazy_source ? affine_matrix_search_step_lazy_source(kernel, session)
                    : affine_matrix_search_step_deferred(kernel, session);
  assert(deferred.status == AffineMatrixServiceStatus::Applied &&
         deferred.reason_deferred &&
         ((lazy_source || lazy_minimal) ? session.lazy_reason_count() == 1
                                       : session.has_deferred_reason()));
  assert(propagate_apply(kernel) == ApplyResult::Ok &&
         kernel.value(kernel_internalize(kernel, -5)) > 0);
  const auto materialized = affine_matrix_materialize_reason(kernel, session);
  assert(materialized.status == AffineMatrixServiceStatus::Applied &&
         materialized.reason_materialized && !session.has_deferred_reason() &&
         session.lazy_reason_count() == 0);
  assert(proof.add_external_rup(++kernel.next_clause_id, nullptr, 0));
  proof.flush();
  auto original = xor_cnf({{{1, 3, 4}, false}, {{2, 3, 4, 5}, false}});
  for (int32_t unit : {-1, -2, 5}) original.push_back({unit});
  write_file(cnf_path, dimacs_text(6, original));
  return 0;
}

// Two lazy source handles; ConflictAnalysis on (7 10) certifies only its
// ancestor. The checker CNF adds the four decisions and the conflict binary.
static int lazy_reason_ancestor_proof_fixture(const char *cnf_path,
                                              const char *proof_path) {
  SearchFixture fixture;
  Kernel &kernel = fixture.kernel;
  AffineMatrixSearchSession &session = fixture.session;
  kernel_reserve_vars(kernel, 10);
  AffineCnf original = paired_implications();
  for (const auto &clause : original) add_clause(kernel, clause);
  const uint64_t conflict_id = kernel.next_clause_id + 5;
  kernel.next_clause_id += 5;
  Proof &proof = fixture.open_dsr(proof_path);
  initialize_persistent(kernel, session);
  decide_all(kernel, {-1, -2});
  assert(affine_matrix_search_step_lazy_source(kernel, session).status ==
         AffineMatrixServiceStatus::Applied);
  assert(propagate_apply(kernel) == ApplyResult::Ok);
  decide_all(kernel, {-6, -7});
  assert(affine_matrix_search_step_lazy_source(kernel, session).status ==
         AffineMatrixServiceStatus::Applied);
  assert(session.lazy_reason_count() == 2);
  binary_conflict(kernel, 7, 10, conflict_id);
  const RuleVTable &analyze =
      tactix_rules()[tactix_rule_id("ConflictAnalysis")];
  assert(rule_guard(analyze, kernel));
  assert(apply_rule(analyze, kernel) == ApplyResult::Ok);
  assert(session.lazy_reason_count() == 1);
  assert(proof.add_external_rup(++kernel.next_clause_id, nullptr, 0));
  proof.flush();
  for (const int32_t unit : {-1, -2, -6, -7}) original.push_back({unit});
  original.push_back({7, 10});
  write_file(cnf_path, dimacs_text(10, original));
  return 0;
}

// The exact-core (minimal) or source-support reason of x5 after four
// decisions, published detached; the checker CNF adds the decisions and -5.
static int detached_reason_proof_fixture(const char *cnf_path,
                                         const char *proof_path,
                                         bool minimal) {
  SearchFixture fixture;
  Kernel &kernel = fixture.kernel;
  AffineMatrixSearchSession &session = fixture.session;
  kernel_reserve_vars(kernel, 7);
  AffineCnf original = xor_cnf({{{1, 2, 3, 4, 6, 7}, false}, {{3, 4, 5, 6, 7}, true}});
  for (const auto &clause : original) add_clause(kernel, clause);
  kernel.next_clause_id += 5;
  Proof &proof = fixture.open_dsr(proof_path);
  initialize_persistent(kernel, session);
  decide_all(kernel, {-1, -2, -3, -4});
  const auto detached = minimal
      ? affine_matrix_search_step_detached_minimal(kernel, session)
      : affine_matrix_search_step_detached_source(kernel, session);
  assert(detached.status == AffineMatrixServiceStatus::Applied &&
         detached.reason_width == (minimal ? 3u : 5u) &&
         detached.live.detached_reason &&
         kernel.value(kernel_internalize(kernel, 5)) > 0);
  assert(proof.add_external_rup(++kernel.next_clause_id, nullptr, 0));
  proof.flush();
  for (const int32_t unit : {-1, -2, -3, -4, -5}) original.push_back({unit});
  write_file(cnf_path, dimacs_text(7, original));
  return 0;
}

} // namespace
int main(int argc,char **argv){
  if(argc==4&&!std::strcmp(argv[1],"--proof-deferred"))
    return deferred_reason_proof_fixture(argv[2],argv[3]);
  if(argc==4&&!std::strcmp(argv[1],"--proof-lazy-source"))
    return deferred_reason_proof_fixture(argv[2],argv[3],true);
  if(argc==4&&!std::strcmp(argv[1],"--proof-lazy-minimal"))
    return deferred_reason_proof_fixture(argv[2],argv[3],false,true);
  if(argc==4&&!std::strcmp(argv[1],"--proof-lazy-ancestor"))
    return lazy_reason_ancestor_proof_fixture(argv[2],argv[3]);
  if(argc==4&&!std::strcmp(argv[1],"--proof-detached-minimal"))
    return detached_reason_proof_fixture(argv[2],argv[3],true);
  if(argc==4&&!std::strcmp(argv[1],"--proof-detached-source"))
    return detached_reason_proof_fixture(argv[2],argv[3],false);
  if(argc==4&&!std::strcmp(argv[1],"--proof-root-unit"))return root_proof_fixture(argv[2],argv[3],true);
  if(argc==4&&!std::strcmp(argv[1],"--proof-root-empty"))return root_proof_fixture(argv[2],argv[3],false);
  if(argc==4&&!std::strcmp(argv[1],"--proof"))return root_proof_fixture(argv[2],argv[3],true);
  if(argc==4&&!std::strcmp(argv[1],"--proof-pbp"))return root_proof_fixture(argv[2],argv[3],true,ProofFormat::PbpStreaming);
  if(argc==4&&!std::strcmp(argv[1],"--proof-search"))return search_proof_fixture(argv[2],argv[3]);
  if(argc==5&&!std::strcmp(argv[1],"--proof-search-seed"))
    return search_proof_fixture(argv[3],argv[4],
                                static_cast<uint32_t>(std::stoul(argv[2])));
  if(argc==5&&!std::strcmp(argv[1],"--proof-search-chain-seed"))
    return search_chain_proof_fixture(argv[3],argv[4],
                                      static_cast<uint32_t>(std::stoul(argv[2])));
  complete_snapshot_policy_admission();
  deferred_reason_has_public_materialization_boundary();
  detached_minimal_reason_is_public_and_unwatched();
  lazy_source_handles_are_multiple_public_and_consumed_on_demand();
  lazy_minimal_handle_is_public_and_consumed_on_demand();
  lazy_minimal_materializes_only_analyze_ancestor();
  unaffordable_lazy_reasons_are_withdrawn();
  lazy_compact_pbp_reason_survives_matrix_cache_commit();
  lazy_source_backtrack_discards_without_publication();
  deferred_reason_materializes_at_later_conflict(false);
  deferred_reason_materializes_at_later_conflict(true);
  repeated_deferred_pbp_reasons_preserve_cache_generation();
  deferred_pbp_reason_survives_backtrack_and_cache_resume();
  partial_admission_feedback_provenance_and_replay();
  root_decline_atomicity();std::puts("test_affine_matrix_search: passed");
}
