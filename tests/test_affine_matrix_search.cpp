#undef NDEBUG
#include "kernel/clause.hpp"
#include "policies/random_controller_policy.hpp"
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

void add_clause(Kernel &kernel, const std::vector<int32_t> &external) {
  std::vector<Lit> clause;
  for (const int32_t literal : external) {
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

void add_equation(Kernel &kernel, const std::vector<int32_t> &variables,
                  bool rhs) {
  for (const auto &clause : clausal_form(variables, rhs))
    add_clause(kernel, clause);
}

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

void install_implication_formula(Kernel &kernel) {
  kernel_reserve_vars(kernel, 6);
  add_equation(kernel, {1, 3, 4}, false);
  add_equation(kernel, {2, 3, 4, 5}, false);
  assert(kernel_propagation_fixpoint(kernel));
}

void install_partial_formula(Kernel &kernel) {
  kernel_reserve_vars(kernel, 10);
  add_equation(kernel, {1, 3, 4}, false);
  add_equation(kernel, {2, 3, 4, 5}, false);
  add_equation(kernel, {6, 7, 8, 9, 10}, true);
  assert(kernel_propagation_fixpoint(kernel));
}

std::string journal_contents(FILE *journal) {
  assert(!std::fflush(journal));
  const long length = std::ftell(journal);
  assert(length >= 0);
  std::rewind(journal);
  std::string result(size_t(length), '\0');
  if (length)
    assert(std::fread(result.data(), 1, size_t(length), journal) ==
           size_t(length));
  assert(!std::fseek(journal, length, SEEK_SET));
  return result;
}

void unchanged_terminal_initialization_is_suppressed() {
  Kernel kernel;
  AffineMatrixSearchSession session;
  kernel.affine_matrix_search_session = &session;
  FILE *journal = std::tmpfile();
  assert(journal);
  kernel.structural_provenance = journal;
  install_implication_formula(kernel);

  auto exhausted = parameters();
  exhausted.maximum_snapshot_work = 1;
  kernel.affine_consequence_parameters = exhausted;
  assert(affine_matrix_initialize_guard(kernel));
  const auto first =
      affine_matrix_search_initialize(kernel, session, exhausted);
  assert(first.status == AffineMatrixInitStatus::ResourceExhausted);
  assert(session.initialization_retry_suppressed(kernel, exhausted));
  assert(affine_matrix_initialize_guard(kernel));
  const auto attempts = kernel.stats.affine_matrix_init_attempts;
  const auto retry =
      affine_matrix_search_initialize(kernel, session, exhausted);
  assert(retry.status == AffineMatrixInitStatus::ResourceExhausted &&
         !retry.snapshot_work && !retry.recognition_work && !retry.matrix_work &&
         kernel.stats.affine_matrix_init_attempts == attempts + 1);

  RandomControllerConfig config;
  config.parameters.rules = {"AffineMatrixInitialize"};
  config.parameters.affine_consequence_reference = exhausted;
  const auto unchanged = controller_action_availability(kernel, config);
  const auto unchanged_entry = std::find_if(
      unchanged.begin(), unchanged.end(), [](const auto &entry) {
        return entry.rule == "AffineMatrixInitialize";
      });
  assert(unchanged_entry != unchanged.end() && !unchanged_entry->eligible &&
         unchanged_entry->reason == "unchanged-terminal-initialization");

  auto reconfigured = exhausted;
  ++reconfigured.maximum_snapshot_work;
  assert(!session.initialization_retry_suppressed(kernel, reconfigured));
  config.parameters.affine_consequence_reference = reconfigured;
  const auto changed = controller_action_availability(kernel, config);
  const auto changed_entry = std::find_if(
      changed.begin(), changed.end(), [](const auto &entry) {
        return entry.rule == "AffineMatrixInitialize";
      });
  assert(changed_entry != changed.end() && changed_entry->eligible);
  std::fclose(journal);
}

void complete_snapshot_policy_admission() {
  {
    Kernel kernel;
    AffineMatrixSearchSession session;
    kernel.affine_matrix_search_session = &session;
    FILE *journal = std::tmpfile();
    assert(journal);
    kernel.structural_provenance = journal;
    install_implication_formula(kernel);
    const auto result = affine_matrix_search_initialize(
        kernel, session, parameters(),
        AffineMatrixAdmission::CompleteSnapshot);
    assert(result.status == AffineMatrixInitStatus::Applied &&
           result.covered_clauses == result.snapshot_clauses &&
           result.covered_clauses == kernel_irredundant_clauses(kernel) &&
           session.enabled() && session.complete_admission());
    std::fclose(journal);
  }
  {
    Kernel kernel;
    AffineMatrixSearchSession session;
    kernel.affine_matrix_search_session = &session;
    FILE *journal = std::tmpfile();
    assert(journal);
    kernel.structural_provenance = journal;
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
    std::fclose(journal);
  }
  {
    Kernel kernel;
    AffineMatrixSearchSession session;
    kernel.affine_matrix_search_session = &session;
    FILE *journal = std::tmpfile();
    assert(journal);
    kernel.structural_provenance = journal;
    install_implication_formula(kernel);
    add_clause(kernel, {1, 2, 6});
    const auto result = affine_matrix_search_initialize(
        kernel, session, parameters(),
        AffineMatrixAdmission::CompleteSnapshot);
    assert(result.status == AffineMatrixInitStatus::IncompleteImage &&
           result.covered_clauses + 1 == result.snapshot_clauses &&
           !session.enabled() &&
           kernel.stats.affine_matrix_init_incomplete_image == 1);
    std::fclose(journal);
  }
  {
    Kernel kernel;
    AffineMatrixSearchSession session;
    kernel.affine_matrix_search_session = &session;
    FILE *journal = std::tmpfile();
    assert(journal);
    kernel.structural_provenance = journal;
    install_partial_formula(kernel);
    auto limited = parameters();
    limited.basis_maximum_intermediate_width = 4;
    const auto result = affine_matrix_search_initialize(
        kernel, session, limited,
        AffineMatrixAdmission::CompleteSnapshot);
    assert(result.status == AffineMatrixInitStatus::IncompleteMatrix &&
           !session.enabled() &&
           kernel.stats.affine_matrix_init_incomplete_matrix == 1);
    std::fclose(journal);
  }
}

void deferred_reason_has_public_materialization_boundary() {
  Kernel kernel;
  AffineMatrixSearchSession session;
  kernel.affine_matrix_search_session = &session;
  FILE *journal = std::tmpfile();
  assert(journal);
  kernel.structural_provenance = journal;
  install_implication_formula(kernel);
  char proof_path[] = "/tmp/tactix-affine-deferred-XXXXXX";
  const int fd = mkstemp(proof_path);
  assert(fd >= 0);
  close(fd);
  Proof proof;
  assert(proof.open(proof_path, ProofFormat::DsrAscii));
  proof.arm_population_shadow();
  kernel.proof = &proof;
  const AffineMatrixPersistenceLimits persistence{64 * 1024 * 1024, 1000,
                                                  100000};
  assert(affine_matrix_search_initialize(
             kernel, session, parameters(),
             AffineMatrixAdmission::AuthenticatedSubimage, persistence)
             .status == AffineMatrixInitStatus::Applied);
  kernel_assign_decision(kernel, kernel_internalize(kernel, -1));
  assert(propagate_apply(kernel) == ApplyResult::Ok);
  kernel_assign_decision(kernel, kernel_internalize(kernel, -2));
  assert(propagate_apply(kernel) == ApplyResult::Ok);

  AffineMatrixInitializeParameters profile;
  profile.basis = parameters();
  profile.admission =
      AffineMatrixInitializeParameters::Admission::AuthenticatedSubimage;
  profile.operation =
      AffineMatrixInitializeParameters::Operation::DeferredSearchStep;
  profile.maximum_retained_bytes = persistence.maximum_retained_bytes;
  profile.maximum_deferred_deletions =
      persistence.maximum_deferred_deletions;
  profile.maximum_deferred_literal_work =
      persistence.maximum_deferred_literal_work;
  assert(profile.valid());
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
  kernel.proof = nullptr;
  unlink(proof_path);
  std::fclose(journal);
}

void detached_minimal_reason_is_public_and_unwatched() {
  Kernel kernel;
  AffineMatrixSearchSession session;
  kernel.affine_matrix_search_session = &session;
  FILE *journal = std::tmpfile();
  assert(journal);
  kernel.structural_provenance = journal;
  kernel_reserve_vars(kernel, 7);
  add_equation(kernel, {1, 2, 3, 4, 6, 7}, false);
  add_equation(kernel, {3, 4, 5, 6, 7}, true);
  char proof_path[] = "/tmp/tactix-affine-detached-XXXXXX";
  const int fd = mkstemp(proof_path);
  assert(fd >= 0);
  close(fd);
  Proof proof;
  assert(proof.open(proof_path, ProofFormat::DsrAscii));
  proof.arm_population_shadow();
  kernel.proof = &proof;
  const AffineMatrixPersistenceLimits persistence{64 * 1024 * 1024, 1000,
                                                  100000};
  assert(affine_matrix_search_initialize(
             kernel, session, parameters(),
             AffineMatrixAdmission::AuthenticatedSubimage, persistence)
             .status == AffineMatrixInitStatus::Applied);
  for (const int32_t decision : {-1, -2, -3, -4}) {
    kernel_assign_decision(kernel, kernel_internalize(kernel, decision));
    assert(propagate_apply(kernel) == ApplyResult::Ok);
  }

  AffineMatrixInitializeParameters profile;
  profile.basis = parameters();
  profile.admission =
      AffineMatrixInitializeParameters::Admission::AuthenticatedSubimage;
  profile.operation = AffineMatrixInitializeParameters::Operation::
      DetachedMinimalSearchStep;
  profile.maximum_retained_bytes = persistence.maximum_retained_bytes;
  profile.maximum_deferred_deletions =
      persistence.maximum_deferred_deletions;
  profile.maximum_deferred_literal_work =
      persistence.maximum_deferred_literal_work;
  assert(profile.valid());
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
  kernel.proof = nullptr;
  unlink(proof_path);
  std::fclose(journal);
}

void lazy_source_handles_are_multiple_public_and_consumed_on_demand() {
  Kernel kernel;
  AffineMatrixSearchSession session;
  kernel.affine_matrix_search_session = &session;
  FILE *journal = std::tmpfile();
  assert(journal);
  kernel.structural_provenance = journal;
  kernel_reserve_vars(kernel, 10);
  for (const auto &[variables, rhs] :
       std::vector<std::pair<std::vector<int32_t>, bool>>{
           {{1, 3, 4}, false}, {{2, 3, 4, 5}, false},
           {{6, 8, 9}, false}, {{7, 8, 9, 10}, false}})
    add_equation(kernel, variables, rhs);
  char proof_path[] = "/tmp/tactix-affine-lazy-multiple-XXXXXX";
  const int fd = mkstemp(proof_path);
  assert(fd >= 0);
  close(fd);
  Proof proof;
  assert(proof.open(proof_path, ProofFormat::DsrAscii));
  proof.arm_population_shadow();
  kernel.proof = &proof;
  const AffineMatrixPersistenceLimits persistence{64 * 1024 * 1024, 1000,
                                                  100000};
  assert(affine_matrix_search_initialize(
             kernel, session, parameters(),
             AffineMatrixAdmission::AuthenticatedSubimage, persistence)
             .status == AffineMatrixInitStatus::Applied);

  for (const int decision : {-1, -2}) {
    kernel_assign_decision(kernel, kernel_internalize(kernel, decision));
    assert(propagate_apply(kernel) == ApplyResult::Ok);
  }
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

  for (const int decision : {-6, -7}) {
    kernel_assign_decision(kernel, kernel_internalize(kernel, decision));
    assert(propagate_apply(kernel) == ApplyResult::Ok);
  }
  const auto second = affine_matrix_search_step_lazy_source(kernel, session);
  assert(second.status == AffineMatrixServiceStatus::Applied &&
         session.lazy_reason_count() == 2 &&
         kernel.value(kernel_internalize(kernel, -10)) > 0);

  // MaterializeReasons is the public eager boundary as well as Analyze's
  // automatic exact-consumer boundary. It discharges one live handle per
  // invocation and retains the certified source-support clause as watched.
  AffineMatrixInitializeParameters profile;
  profile.basis = parameters();
  profile.admission =
      AffineMatrixInitializeParameters::Admission::AuthenticatedSubimage;
  profile.operation =
      AffineMatrixInitializeParameters::Operation::MaterializeReasons;
  profile.maximum_retained_bytes = persistence.maximum_retained_bytes;
  profile.maximum_deferred_deletions =
      persistence.maximum_deferred_deletions;
  profile.maximum_deferred_literal_work =
      persistence.maximum_deferred_literal_work;
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
  kernel.proof = nullptr;
  unlink(proof_path);
  std::fclose(journal);
}

void lazy_minimal_handle_is_public_and_consumed_on_demand() {
  Kernel kernel;
  AffineMatrixSearchSession session;
  kernel.affine_matrix_search_session = &session;
  FILE *journal = std::tmpfile();
  assert(journal);
  kernel.structural_provenance = journal;
  install_implication_formula(kernel);
  char proof_path[] = "/tmp/tactix-affine-lazy-minimal-XXXXXX";
  const int fd = mkstemp(proof_path);
  assert(fd >= 0);
  close(fd);
  Proof proof;
  assert(proof.open(proof_path, ProofFormat::DsrAscii));
  proof.arm_population_shadow();
  kernel.proof = &proof;
  const AffineMatrixPersistenceLimits persistence{64 * 1024 * 1024, 1000,
                                                  100000};
  assert(affine_matrix_search_initialize(
             kernel, session, parameters(),
             AffineMatrixAdmission::AuthenticatedSubimage, persistence)
             .status == AffineMatrixInitStatus::Applied);
  for (const int decision : {-1, 2}) {
    kernel_assign_decision(kernel, kernel_internalize(kernel, decision));
    assert(propagate_apply(kernel) == ApplyResult::Ok);
  }
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
  kernel.proof = nullptr;
  unlink(proof_path);
  std::fclose(journal);
}

void lazy_minimal_materializes_only_analyze_ancestor() {
  Kernel kernel;
  AffineMatrixSearchSession session;
  kernel.affine_matrix_search_session = &session;
  FILE *journal = std::tmpfile();
  assert(journal);
  kernel.structural_provenance = journal;
  kernel_reserve_vars(kernel, 10);
  for (const auto &[variables, rhs] :
       std::vector<std::pair<std::vector<int32_t>, bool>>{
           {{1, 3, 4}, false}, {{2, 3, 4, 5}, false},
           {{6, 8, 9}, false}, {{7, 8, 9, 10}, false}})
    add_equation(kernel, variables, rhs);
  char proof_path[] = "/tmp/tactix-affine-lazy-analyze-XXXXXX";
  const int fd = mkstemp(proof_path);
  assert(fd >= 0);
  close(fd);
  Proof proof;
  assert(proof.open(proof_path, ProofFormat::DsrAscii));
  proof.arm_population_shadow();
  kernel.proof = &proof;
  const AffineMatrixPersistenceLimits persistence{64 * 1024 * 1024, 1000,
                                                  100000};
  assert(affine_matrix_search_initialize(
             kernel, session, parameters(),
             AffineMatrixAdmission::AuthenticatedSubimage, persistence)
             .status == AffineMatrixInitStatus::Applied);
  for (const int decision : {-1, -2}) {
    kernel_assign_decision(kernel, kernel_internalize(kernel, decision));
    assert(propagate_apply(kernel) == ApplyResult::Ok);
  }
  const auto first = affine_matrix_search_step_lazy_minimal(kernel, session);
  assert(first.status == AffineMatrixServiceStatus::Applied &&
         first.installed_reason.empty() && session.lazy_reason_count() == 1 &&
         !session.lazy_reason_vector_constructed(4));
  assert(kernel.assigned[4].affine &&
         kernel.assigned[4].reason == AFFINE_REASON);
  assert(propagate_apply(kernel) == ApplyResult::Ok);
  for (const int decision : {-6, -7}) {
    kernel_assign_decision(kernel, kernel_internalize(kernel, decision));
    assert(propagate_apply(kernel) == ApplyResult::Ok);
  }
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
  const Lit conflict[2] = {kernel_internalize(kernel, 7),
                           kernel_internalize(kernel, 10)};
  kernel_add_binary_clause(kernel, conflict[0], conflict[1],
                           ++kernel.next_clause_id);
  kernel.conflict_ref = INVALID_REF;
  kernel.conflict_bin[0] = conflict[0];
  kernel.conflict_bin[1] = conflict[1];
  kernel.conflict_valid = true;
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
  kernel.proof = nullptr;
  unlink(proof_path);
  std::fclose(journal);
}

void lazy_source_backtrack_discards_without_publication() {
  Kernel kernel;
  AffineMatrixSearchSession session;
  kernel.affine_matrix_search_session = &session;
  FILE *journal = std::tmpfile();
  assert(journal);
  kernel.structural_provenance = journal;
  install_implication_formula(kernel);
  char proof_path[] = "/tmp/tactix-affine-lazy-backtrack-XXXXXX";
  const int fd = mkstemp(proof_path);
  assert(fd >= 0);
  close(fd);
  Proof proof;
  assert(proof.open(proof_path, ProofFormat::DsrAscii));
  proof.arm_population_shadow();
  kernel.proof = &proof;
  const AffineMatrixPersistenceLimits persistence{64 * 1024 * 1024, 1000,
                                                  100000};
  assert(affine_matrix_search_initialize(
             kernel, session, parameters(),
             AffineMatrixAdmission::AuthenticatedSubimage, persistence)
             .status == AffineMatrixInitStatus::Applied);
  for (const int decision : {-1, -2}) {
    kernel_assign_decision(kernel, kernel_internalize(kernel, decision));
    assert(propagate_apply(kernel) == ApplyResult::Ok);
  }
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
  kernel.proof = nullptr;
  unlink(proof_path);
  std::fclose(journal);
}

void deferred_reason_materializes_at_later_conflict(bool pbp) {
  Kernel kernel;
  AffineMatrixSearchSession session;
  kernel.affine_matrix_search_session = &session;
  FILE *journal = std::tmpfile();
  assert(journal);
  kernel.structural_provenance = journal;
  kernel_reserve_vars(kernel, 7);
  std::vector<std::vector<int32_t>> original;
  for (const auto &[variables, rhs] :
       std::vector<std::pair<std::vector<int32_t>, bool>>{
           {{1, 3, 4}, false}, {{2, 3, 4, 5}, false}})
    for (const auto &clause : clausal_form(variables, rhs))
      original.push_back(clause);
  // Once affine reasoning assigns -5, deciding -6 makes the first clause
  // imply 7 and the second imply -7.  This reaches materialization with an
  // active Boolean conflict after an intervening ordinary decision/BCP step.
  original.push_back({5, 6, 7});
  original.push_back({6, -7});
  for (const auto &clause : original) add_clause(kernel, clause);
  char proof_path[] = "/tmp/tactix-affine-deferred-conflict-XXXXXX";
  const int fd = mkstemp(proof_path);
  assert(fd >= 0);
  close(fd);
  Proof proof;
  assert(proof.open(proof_path, pbp ? ProofFormat::PbpStreaming
                                    : ProofFormat::DsrAscii));
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  if (pbp) {
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
  } else {
    proof.arm_population_shadow();
  }
  kernel.proof = &proof;
  const AffineMatrixPersistenceLimits persistence{64 * 1024 * 1024, 1000,
                                                  100000};
  assert(affine_matrix_search_initialize(
             kernel, session, parameters(),
             AffineMatrixAdmission::AuthenticatedSubimage, persistence)
             .status == AffineMatrixInitStatus::Applied);
  kernel_assign_decision(kernel, kernel_internalize(kernel, -1));
  assert(propagate_apply(kernel) == ApplyResult::Ok);
  kernel_assign_decision(kernel, kernel_internalize(kernel, -2));
  assert(propagate_apply(kernel) == ApplyResult::Ok);
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
  kernel.proof = nullptr;
  unlink(proof_path);
  std::fclose(journal);
}

void repeated_deferred_pbp_reasons_preserve_cache_generation() {
  Kernel kernel;
  AffineMatrixSearchSession session;
  kernel.affine_matrix_search_session = &session;
  FILE *journal = std::tmpfile();
  assert(journal);
  kernel.structural_provenance = journal;
  kernel_reserve_vars(kernel, 10);
  std::vector<std::vector<int32_t>> original;
  for (const auto &[variables, rhs] :
       std::vector<std::pair<std::vector<int32_t>, bool>>{
           {{1, 3, 4}, false}, {{2, 3, 4, 5}, false},
           {{6, 8, 9}, false}, {{7, 8, 9, 10}, false}})
    for (const auto &clause : clausal_form(variables, rhs))
      original.push_back(clause);
  for (const auto &clause : original) add_clause(kernel, clause);
  char proof_path[] = "/tmp/tactix-affine-deferred-repeat-XXXXXX";
  const int fd = mkstemp(proof_path);
  assert(fd >= 0);
  close(fd);
  Proof proof;
  assert(proof.open(proof_path, ProofFormat::PbpStreaming));
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
  kernel.proof = &proof;
  const AffineMatrixPersistenceLimits persistence{64 * 1024 * 1024, 1000,
                                                  100000};
  assert(affine_matrix_search_initialize(
             kernel, session, parameters(),
             AffineMatrixAdmission::AuthenticatedSubimage, persistence)
             .status == AffineMatrixInitStatus::Applied);
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
  kernel.proof = nullptr;
  unlink(proof_path);
  std::fclose(journal);
}

void deferred_pbp_reason_survives_backtrack_and_cache_resume() {
  Kernel kernel;
  AffineMatrixSearchSession session;
  kernel.affine_matrix_search_session = &session;
  FILE *journal = std::tmpfile();
  assert(journal);
  kernel.structural_provenance = journal;
  kernel_reserve_vars(kernel, 5);
  std::vector<std::vector<int32_t>> original;
  for (const auto &[variables, rhs] :
       std::vector<std::pair<std::vector<int32_t>, bool>>{
           {{1, 3, 4}, false}, {{2, 3, 4, 5}, false}})
    for (const auto &clause : clausal_form(variables, rhs))
      original.push_back(clause);
  for (const auto &clause : original) add_clause(kernel, clause);
  char proof_path[] = "/tmp/tactix-affine-deferred-resume-XXXXXX";
  const int fd = mkstemp(proof_path);
  assert(fd >= 0);
  close(fd);
  Proof proof;
  assert(proof.open(proof_path, ProofFormat::PbpStreaming));
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
  kernel.proof = &proof;
  const AffineMatrixPersistenceLimits persistence{64 * 1024 * 1024, 1000,
                                                  100000};
  assert(affine_matrix_search_initialize(
             kernel, session, parameters(),
             AffineMatrixAdmission::AuthenticatedSubimage, persistence)
             .status == AffineMatrixInitStatus::Applied);
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
  kernel.proof = nullptr;
  unlink(proof_path);
  std::fclose(journal);
}

static std::string initialize_partial_fixture() {
  Kernel kernel;
  AffineMatrixSearchSession session;
  kernel.affine_matrix_search_session = &session;
  FILE *journal = std::tmpfile();
  assert(journal);
  kernel.structural_provenance = journal;
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
  const std::string journal_record = journal_contents(journal);
  assert(journal_record.find("\"admission\":\"partial\"") !=
         std::string::npos);
  assert(journal_record.find("\"sources\":[0]") != std::string::npos &&
         journal_record.find("\"sources\":[1]") != std::string::npos &&
         journal_record.find("\"sources\":[2]") == std::string::npos);

  RandomControllerConfig active_config;
  active_config.parameters.rules = {"AffineMatrixInitialize"};
  active_config.parameters.affine_consequence_reference = p;
  const auto availability =
      controller_action_availability(kernel, active_config);
  const auto active = std::find_if(
      availability.begin(), availability.end(), [](const auto &entry) {
        return entry.rule == "AffineMatrixInitialize";
      });
  assert(active != availability.end() && !active->eligible &&
         active->reason == "affine-matrix-session-active");

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
  const std::string deterministic_record = journal_contents(journal);
  assert(deterministic_record.find("\"admission\":\"partial\"") !=
             std::string::npos &&
         deterministic_record.find("\"source_ordinals\":[0,1]") !=
             std::string::npos);
  std::fclose(journal);
  return deterministic_record;
}

void partial_admission_feedback_provenance_and_replay() {
  const std::string first = initialize_partial_fixture();
  const std::string replay = initialize_partial_fixture();
  assert(first == replay);
}


void root_formula(Kernel &kernel) {
  kernel_reserve_vars(kernel, 7);
  add_equation(kernel,{1,2,3},false);
  add_equation(kernel,{1,2,4},false);
  add_equation(kernel,{3,4,5},true);
}
void decide(RestartController &controller,Kernel &kernel,int literal) {
  assert(controller.search_service("Decide",INSTANCE_CANONICAL,[&]{
    kernel_assign_decision(kernel,kernel_internalize(kernel,literal));return ApplyResult::Ok;
  })==ApplyResult::Ok);
  assert(controller.search_service("Propagate",INSTANCE_CANONICAL,[&]{return propagate_apply(kernel);})==ApplyResult::Ok);
}
void restart_only_replay() {
  char path[]="/tmp/tactix-affine-restart-only-XXXXXX";
  const int fd=mkstemp(path);assert(fd>=0);assert(!close(fd));
  for(bool replay:{false,true}) {
    Kernel kernel;AffineMatrixSearchSession session;
    kernel.affine_matrix_search_session=&session;
    FILE *journal=std::tmpfile();assert(journal);kernel.structural_provenance=journal;
    root_formula(kernel);Capture feedback;RestartController controller(kernel,{},&feedback);
    ControllerTrace trace;
    assert(replay?trace.replay(path,"restart-only-v1"):trace.write(path,17,"restart-only-v1"));
    const auto record=[&](const char *kind,uint64_t status){
      const auto state=state_version(kernel);
      assert(trace.event(std::string(kind)+" "+std::to_string(status)+" "+std::to_string(state.strategy_epoch)+" "+std::to_string(kernel.trail.size())));
    };
    const auto initialized=controller.apply_rule(initialize_request(controller,kernel,parameters()),{WorkUnit::Applications,1,{}});
    assert(initialized.invocation.status==InvocationStatus::Completed);record("initialize",unsigned(initialized.invocation.status));
    // Initial root setup is an explicit public action, not an implicit restart.
    assert(!controller.affine_matrix_root_service_due());
    assert(controller.resume_search(resume_request(controller,kernel)).resumed_search);record("resume",1);
    decide(controller,kernel,-6);decide(controller,kernel,-7);
    const auto calls=kernel.stats.affine_matrix_service_calls;
    const auto owner=session.owner_version(),id=kernel.next_clause_id;
    const auto declined=controller.affine_matrix_fixpoint_service();
    assert(declined.status==AffineMatrixServiceStatus::Inapplicable&&kernel.stats.affine_matrix_service_calls==calls&&session.owner_version()==owner&&kernel.next_clause_id==id&&!session.published_keys());
    record("mid-interval",unsigned(declined.status));
    assert(controller.restart(1)==ApplyResult::Ok&&kernel.level==1);
    assert(!controller.affine_matrix_root_service_due());
    assert(controller.affine_matrix_fixpoint_service().status==AffineMatrixServiceStatus::Inapplicable&&kernel.stats.affine_matrix_service_calls==calls);
    record("partial-restart",1);
    assert(controller.full_restart()==ApplyResult::Ok&&kernel.level==0&&controller.affine_matrix_root_service_due());record("full-restart",0);
    const auto used=controller.affine_matrix_fixpoint_service();
    assert(used.status==AffineMatrixServiceStatus::Applied&&used.installed_reason==std::vector<int32_t>{5}&&used.bcp_pending&&session.published_keys()==1);
    assert(trace.event(affine_matrix_service_trace_record(kernel,used)));
    assert(controller.resume_search(resume_request(controller,kernel)).invocation.status==InvocationStatus::Inapplicable);
    assert(session.enabled()&&session.published_keys()==1);
    assert(controller.search_service("Propagate",INSTANCE_CANONICAL,[&]{return propagate_apply(kernel);})==ApplyResult::Ok);
    assert(kernel.value(kernel_internalize(kernel,5))>0&&kernel_propagation_fixpoint(kernel));record("consume",1);
    const auto finished=controller.affine_matrix_fixpoint_service();
    assert(finished.status==AffineMatrixServiceStatus::NoEvent||finished.status==AffineMatrixServiceStatus::Duplicate);
    assert(!controller.affine_matrix_root_service_due());
    assert(controller.resume_search(resume_request(controller,kernel)).resumed_search);record("resume",2);
    const auto resumed_calls=kernel.stats.affine_matrix_service_calls;
    assert(controller.affine_matrix_fixpoint_service().status==AffineMatrixServiceStatus::Inapplicable&&kernel.stats.affine_matrix_service_calls==resumed_calls);
    decide(controller,kernel,-1);if(!kernel.value(kernel_internalize(kernel,2)))decide(controller,kernel,-2);
    if(!kernel.value(kernel_internalize(kernel,6)))decide(controller,kernel,-6);
    if(!kernel.value(kernel_internalize(kernel,7)))decide(controller,kernel,-7);
    assert(!kernel.unassigned_active);kernel_extend_model(kernel);
    for(const auto &pair:std::vector<std::pair<std::vector<int32_t>,bool>>{{{1,2,3},false},{{1,2,4},false},{{3,4,5},true}})
      for(const auto &clause:clausal_form(pair.first,pair.second)){bool sat=false;for(int x:clause)sat|=kernel.model_values[internalize(x)]>0;assert(sat);}
    assert(trace.finish(10));std::fclose(journal);
  }
  std::remove(path);
}
void root_decline_atomicity() {
  for(bool interrupted:{false,true}){
    Kernel kernel;AffineMatrixSearchSession session;kernel.affine_matrix_search_session=&session;
    FILE *journal=std::tmpfile();assert(journal);kernel.structural_provenance=journal;root_formula(kernel);
    auto p=parameters();if(!interrupted)p.maximum_session_work=1;
    RestartController controller(kernel);
    assert(controller.apply_rule(initialize_request(controller,kernel,p),{WorkUnit::Applications,1,{}}).invocation.status==InvocationStatus::Completed);
    assert(controller.resume_search(resume_request(controller,kernel)).resumed_search);
    assert(controller.full_restart()==ApplyResult::Ok);
    const auto id=kernel.next_clause_id,owner=session.owner_version();const auto arena=kernel.arena;const long position=std::ftell(journal);
    volatile sig_atomic_t stop=interrupted;kernel.stop=&stop;
    const auto result=controller.affine_matrix_fixpoint_service();
    assert(result.status==(interrupted?AffineMatrixServiceStatus::Interrupted:AffineMatrixServiceStatus::ResourceExhausted));
    assert(kernel.next_clause_id==id&&kernel.arena==arena&&session.owner_version()==owner&&!session.published_keys()&&std::ftell(journal)==position);
    kernel.stop=nullptr;std::fclose(journal);
  }
}
static int root_proof_fixture(const char *cnf_path, const char *proof_path,
                              bool unit, ProofFormat format = ProofFormat::DsrAscii) {
  Kernel kernel;
  AffineMatrixSearchSession session;
  kernel.affine_matrix_search_session = &session;
  FILE *journal = std::tmpfile();
  assert(journal);
  kernel.structural_provenance = journal;
  kernel_reserve_vars(kernel, unit ? 5 : 3);
  const std::vector<std::pair<std::vector<int32_t>, bool>> equations = unit
      ? std::vector<std::pair<std::vector<int32_t>, bool>>{
            {{1, 2, 3}, false}, {{1, 2, 4}, false}, {{3, 4, 5}, true}}
      : std::vector<std::pair<std::vector<int32_t>, bool>>{
            {{1, 2, 3}, false}, {{1, 2, 3}, true}};
  for (const auto &[variables, rhs] : equations)
    add_equation(kernel, variables, rhs);
  // The independently checked unit fixture has one extra original clause.
  if (unit) ++kernel.next_clause_id;
  Proof proof;
  assert(proof.open(proof_path, format));
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  uint64_t input_id = 0;
  for (const auto &[variables, rhs] : equations)
    for (const auto &external : clausal_form(variables, rhs)) {
      std::vector<Lit> clause;
      for (int32_t literal : external) clause.push_back(internalize(literal));
      proof.seed_population(++input_id, clause.data(), clause.size());
    }
  if (unit) {
    const Lit closure = internalize(-5);
    proof.seed_population(++input_id, &closure, 1);
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
    // The original CNF also contains -5.  Keeping it out of the captured
    // matrix isolates publication of the derived root unit before refutation.
    assert(proof.add_external_rup(++kernel.next_clause_id, nullptr, 0));
  } else {
    assert(service.result == ApplyResult::Unsat && !service.bcp_pending &&
           service.live.conflict && !service.live.implication &&
           service.reason_width == 0 && kernel.inconsistent);
  }
  proof.flush();
  kernel.proof = nullptr;
  FILE *cnf = std::fopen(cnf_path, "w");
  assert(cnf);
  std::fprintf(cnf, "p cnf %u %zu\n", unit ? 5u : 3u,
               equations.size() * 4 + (unit ? 1 : 0));
  for (const auto &[variables, rhs] : equations)
    for (const auto &clause : clausal_form(variables, rhs)) {
      for (int32_t literal : clause) std::fprintf(cnf, "%d ", literal);
      std::fprintf(cnf, "0\n");
    }
  if (unit) std::fprintf(cnf, "-5 0\n");
  std::fclose(cnf);
  std::fclose(journal);
  return 0;
}

static int search_proof_fixture(const char *cnf_path, const char *proof_path,
                                uint32_t seed = 0) {
  Kernel kernel;
  AffineMatrixSearchSession session;
  kernel.affine_matrix_search_session = &session;
  FILE *journal = std::tmpfile();
  assert(journal);
  kernel.structural_provenance = journal;
  kernel_reserve_vars(kernel, 4);
  std::vector<int32_t> variables = {1, 2, 3, 4};
  bool first_rhs = false, second_rhs = true, decision_positive = true;
  if (seed) {
    // Rename variables, change both row signs and branch in either phase.
    // The XOR sum still forces the fourth variable from the first decision.
    uint32_t state = seed;
    const auto next = [&]() {
      state ^= state << 13;
      state ^= state >> 17;
      state ^= state << 5;
      return state;
    };
    for (size_t i = variables.size() - 1; i > 0; --i)
      std::swap(variables[i], variables[next() % (i + 1)]);
    first_rhs = (next() & 1) != 0;
    second_rhs = (next() & 1) != 0;
    decision_positive = (next() & 1) != 0;
  }
  std::vector<std::pair<std::vector<int32_t>, bool>> equations = {
      {{variables[0], variables[1], variables[2]}, first_rhs},
      {{variables[1], variables[2], variables[3]}, second_rhs}};
  for (auto &[row, rhs] : equations) {
    (void)rhs;
    std::sort(row.begin(), row.end());
  }
  for (const auto &[variables, rhs] : equations)
    add_equation(kernel, variables, rhs);
  // The checker input has two additional unit clauses absent from this
  // isolated service fixture; reserve their input IDs before proof additions.
  kernel.next_clause_id += 2;
  Proof proof;
  assert(proof.open(proof_path, ProofFormat::DsrAscii));
  proof.arm_population_shadow();
  kernel.proof = &proof;
  const AffineMatrixPersistenceLimits persistence{64 * 1024 * 1024, 1000,
                                                  100000};
  const auto initialized = affine_matrix_search_initialize(
      kernel, session, parameters(),
      AffineMatrixAdmission::AuthenticatedSubimage, persistence);
  if (initialized.status != AffineMatrixInitStatus::Applied)
    std::fprintf(stderr, "seed=%u init=%u vars=%d,%d,%d,%d rhs=%d%d\n",
                 seed, static_cast<unsigned>(initialized.status), variables[0],
                 variables[1], variables[2], variables[3], first_rhs, second_rhs);
  assert(initialized.status == AffineMatrixInitStatus::Applied);
  assert(affine_matrix_search_step(kernel, session).status ==
         AffineMatrixServiceStatus::NoEvent);
  const int32_t decision = decision_positive ? variables[0] : -variables[0];
  const bool implied_positive = decision_positive ^ first_rhs ^ second_rhs;
  const int32_t implied = implied_positive ? variables[3] : -variables[3];
  kernel_assign_decision(kernel, kernel_internalize(kernel, decision));
  assert(propagate_apply(kernel) == ApplyResult::Ok);
  const auto service = affine_matrix_search_step(kernel, session);
  if (service.status != AffineMatrixServiceStatus::Applied ||
      service.reason_width != 2 || !service.bcp_pending)
    std::fprintf(stderr, "seed=%u status=%u reason_width=%zu bcp=%d vars=%d,%d,%d,%d rows=%d%d decision=%d before=%d\n",
                 seed, static_cast<unsigned>(service.status),
                 service.reason_width, service.bcp_pending,
                 variables[0], variables[1], variables[2], variables[3],
                 first_rhs, second_rhs, decision,
                 kernel_external_value(kernel, internalize(implied)));
  assert(service.status == AffineMatrixServiceStatus::Applied &&
         service.reason_width == 2 && service.bcp_pending);
  assert(propagate_apply(kernel) == ApplyResult::Ok &&
         kernel_external_value(kernel, internalize(implied)) > 0);
  // The extra two clauses in the independently checked input make a complete
  // refutation.  They are intentionally absent from the captured matrix.
  assert(proof.add_external_rup(++kernel.next_clause_id, nullptr, 0));
  proof.flush();
  kernel.proof = nullptr;
  FILE *cnf = std::fopen(cnf_path, "w");
  assert(cnf);
  std::fprintf(cnf, "p cnf 4 10\n");
  for (const auto &[variables, rhs] : equations)
    for (const auto &clause : clausal_form(variables, rhs)) {
      for (int32_t literal : clause) std::fprintf(cnf, "%d ", literal);
      std::fprintf(cnf, "0\n");
    }
  std::fprintf(cnf, "%d 0\n%d 0\n", decision, -implied);
  std::fclose(cnf);
  std::fclose(journal);
  return 0;
}

static int search_chain_proof_fixture(const char *cnf_path,
                                      const char *proof_path, uint32_t seed) {
  Kernel kernel;
  AffineMatrixSearchSession session;
  kernel.affine_matrix_search_session = &session;
  FILE *journal = std::tmpfile();
  assert(journal);
  kernel.structural_provenance = journal;
  kernel_reserve_vars(kernel, 7);
  std::vector<int32_t> variables = {1, 2, 3, 4, 5, 6, 7};
  std::array<bool, 4> rhs = {false, false, false, true};
  bool decision_positive = true;
  if (seed) {
    uint32_t state = seed;
    const auto next = [&]() {
      state ^= state << 13;
      state ^= state >> 17;
      state ^= state << 5;
      return state;
    };
    for (size_t i = variables.size() - 1; i > 0; --i)
      std::swap(variables[i], variables[next() % (i + 1)]);
    for (bool &sign : rhs) sign = (next() & 1) != 0;
    decision_positive = (next() & 1) != 0;
  }
  std::vector<std::pair<std::vector<int32_t>, bool>> equations = {
      {{variables[0], variables[1], variables[2]}, rhs[0]},
      {{variables[1], variables[3], variables[4]}, rhs[1]},
      {{variables[2], variables[3], variables[5]}, rhs[2]},
      {{variables[4], variables[5], variables[6]}, rhs[3]}};
  for (auto &[row, sign] : equations) {
    (void)sign;
    std::sort(row.begin(), row.end());
  }
  for (const auto &[row, sign] : equations) add_equation(kernel, row, sign);
  kernel.next_clause_id += 2; // Two checker-only units.
  Proof proof;
  assert(proof.open(proof_path, ProofFormat::DsrAscii));
  proof.arm_population_shadow();
  kernel.proof = &proof;
  const AffineMatrixPersistenceLimits persistence{64 * 1024 * 1024, 1000,
                                                  100000};
  assert(affine_matrix_search_initialize(
             kernel, session, parameters(),
             AffineMatrixAdmission::AuthenticatedSubimage, persistence).status ==
         AffineMatrixInitStatus::Applied);
  assert(affine_matrix_search_step(kernel, session).status ==
         AffineMatrixServiceStatus::NoEvent);
  const int32_t decision = decision_positive ? variables[0] : -variables[0];
  bool implied_positive = decision_positive;
  for (bool sign : rhs) implied_positive ^= sign;
  const int32_t implied = implied_positive ? variables[6] : -variables[6];
  kernel_assign_decision(kernel, kernel_internalize(kernel, decision));
  assert(propagate_apply(kernel) == ApplyResult::Ok);
  const auto service = affine_matrix_search_step(kernel, session);
  assert(service.status == AffineMatrixServiceStatus::Applied &&
         service.reason_width == 2 && service.bcp_pending &&
         service.live.selected_sources == 4);
  assert(propagate_apply(kernel) == ApplyResult::Ok &&
         kernel_external_value(kernel, internalize(implied)) > 0);
  assert(proof.add_external_rup(++kernel.next_clause_id, nullptr, 0));
  proof.flush();
  kernel.proof = nullptr;
  FILE *cnf = std::fopen(cnf_path, "w");
  assert(cnf);
  std::fprintf(cnf, "p cnf 7 18\n");
  for (const auto &[row, sign] : equations)
    for (const auto &clause : clausal_form(row, sign)) {
      for (int32_t literal : clause) std::fprintf(cnf, "%d ", literal);
      std::fprintf(cnf, "0\n");
    }
  std::fprintf(cnf, "%d 0\n%d 0\n", decision, -implied);
  std::fclose(cnf);
  std::fclose(journal);
  return 0;
}

static int deferred_reason_proof_fixture(const char *cnf_path,
                                         const char *proof_path,
                                         bool lazy_source = false,
                                         bool lazy_minimal = false) {
  Kernel kernel;
  AffineMatrixSearchSession session;
  kernel.affine_matrix_search_session = &session;
  FILE *journal = std::tmpfile();
  assert(journal);
  kernel.structural_provenance = journal;
  install_implication_formula(kernel);
  // The independently checked original formula additionally fixes the two
  // decisions and contradicts the affine implication. Reserve those input
  // identities before any derived certificate line is numbered.
  kernel.next_clause_id += 3;
  Proof proof;
  assert(proof.open(proof_path, ProofFormat::DsrAscii));
  proof.arm_population_shadow();
  kernel.proof = &proof;
  const AffineMatrixPersistenceLimits persistence{64 * 1024 * 1024, 1000,
                                                  100000};
  assert(affine_matrix_search_initialize(
             kernel, session, parameters(),
             AffineMatrixAdmission::AuthenticatedSubimage, persistence)
             .status == AffineMatrixInitStatus::Applied);
  kernel_assign_decision(kernel, kernel_internalize(kernel, -1));
  assert(propagate_apply(kernel) == ApplyResult::Ok);
  kernel_assign_decision(kernel, kernel_internalize(kernel, -2));
  assert(propagate_apply(kernel) == ApplyResult::Ok);
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
  kernel.proof = nullptr;

  FILE *cnf = std::fopen(cnf_path, "w");
  assert(cnf);
  std::fprintf(cnf, "p cnf 6 15\n");
  for (const auto &[variables, rhs] :
       std::vector<std::pair<std::vector<int32_t>, bool>>{
           {{1, 3, 4}, false}, {{2, 3, 4, 5}, false}})
    for (const auto &clause : clausal_form(variables, rhs)) {
      for (int32_t literal : clause) std::fprintf(cnf, "%d ", literal);
      std::fprintf(cnf, "0\n");
    }
  std::fprintf(cnf, "-1 0\n-2 0\n5 0\n");
  std::fclose(cnf);
  std::fclose(journal);
  return 0;
}

static int lazy_reason_ancestor_proof_fixture(const char *cnf_path,
                                              const char *proof_path) {
  Kernel kernel;
  AffineMatrixSearchSession session;
  kernel.affine_matrix_search_session = &session;
  FILE *journal = std::tmpfile();
  assert(journal);
  kernel.structural_provenance = journal;
  kernel_reserve_vars(kernel, 10);
  const std::vector<std::pair<std::vector<int32_t>, bool>> equations = {
      {{1, 3, 4}, false}, {{2, 3, 4, 5}, false},
      {{6, 8, 9}, false}, {{7, 8, 9, 10}, false}};
  std::vector<std::vector<int32_t>> original;
  for (const auto &[variables, rhs] : equations)
    for (const auto &clause : clausal_form(variables, rhs)) {
      original.push_back(clause);
      add_clause(kernel, clause);
    }

  // The untouched checker input additionally contains four decisions and the
  // binary conflict. Reserve their IDs before either lazy reason is certified.
  const uint64_t conflict_id = kernel.next_clause_id + 5;
  kernel.next_clause_id += 5;
  Proof proof;
  assert(proof.open(proof_path, ProofFormat::DsrAscii));
  proof.arm_population_shadow();
  kernel.proof = &proof;
  const AffineMatrixPersistenceLimits persistence{64 * 1024 * 1024, 1000,
                                                  100000};
  assert(affine_matrix_search_initialize(
             kernel, session, parameters(),
             AffineMatrixAdmission::AuthenticatedSubimage, persistence)
             .status == AffineMatrixInitStatus::Applied);
  for (const int decision : {-1, -2}) {
    kernel_assign_decision(kernel, kernel_internalize(kernel, decision));
    assert(propagate_apply(kernel) == ApplyResult::Ok);
  }
  assert(affine_matrix_search_step_lazy_source(kernel, session).status ==
         AffineMatrixServiceStatus::Applied);
  assert(propagate_apply(kernel) == ApplyResult::Ok);
  for (const int decision : {-6, -7}) {
    kernel_assign_decision(kernel, kernel_internalize(kernel, decision));
    assert(propagate_apply(kernel) == ApplyResult::Ok);
  }
  assert(affine_matrix_search_step_lazy_source(kernel, session).status ==
         AffineMatrixServiceStatus::Applied);
  assert(session.lazy_reason_count() == 2);

  const Lit conflict[2] = {kernel_internalize(kernel, 7),
                           kernel_internalize(kernel, 10)};
  kernel_add_binary_clause(kernel, conflict[0], conflict[1], conflict_id);
  kernel.conflict_ref = INVALID_REF;
  kernel.conflict_bin[0] = conflict[0];
  kernel.conflict_bin[1] = conflict[1];
  kernel.conflict_valid = true;
  const RuleVTable &analyze =
      tactix_rules()[tactix_rule_id("ConflictAnalysis")];
  assert(rule_guard(analyze, kernel));
  assert(apply_rule(analyze, kernel) == ApplyResult::Ok);
  assert(session.lazy_reason_count() == 1);
  assert(proof.add_external_rup(++kernel.next_clause_id, nullptr, 0));
  proof.flush();
  kernel.proof = nullptr;

  FILE *cnf = std::fopen(cnf_path, "w");
  assert(cnf);
  std::fprintf(cnf, "p cnf 10 %zu\n", original.size() + 5);
  for (const auto &clause : original) {
    for (const int32_t literal : clause) std::fprintf(cnf, "%d ", literal);
    std::fprintf(cnf, "0\n");
  }
  for (const int32_t unit : {-1, -2, -6, -7})
    std::fprintf(cnf, "%d 0\n", unit);
  std::fprintf(cnf, "7 10 0\n");
  std::fclose(cnf);
  std::fclose(journal);
  return 0;
}

static int detached_reason_proof_fixture(const char *cnf_path,
                                         const char *proof_path,
                                         bool minimal) {
  Kernel kernel;
  AffineMatrixSearchSession session;
  kernel.affine_matrix_search_session = &session;
  FILE *journal = std::tmpfile();
  assert(journal);
  kernel.structural_provenance = journal;
  kernel_reserve_vars(kernel, 7);
  std::vector<std::vector<int32_t>> original;
  for (const auto &[variables, rhs] :
       std::vector<std::pair<std::vector<int32_t>, bool>>{
           {{1, 2, 3, 4, 6, 7}, false}, {{3, 4, 5, 6, 7}, true}})
    for (const auto &clause : clausal_form(variables, rhs)) {
      original.push_back(clause);
      add_clause(kernel, clause);
    }
  // The checker input also contains four decisions and the negation of the
  // algebraic implication. Reserve those input identities before emitting the
  // exact-core reason and final empty clause.
  kernel.next_clause_id += 5;
  Proof proof;
  assert(proof.open(proof_path, ProofFormat::DsrAscii));
  proof.arm_population_shadow();
  kernel.proof = &proof;
  const AffineMatrixPersistenceLimits persistence{64 * 1024 * 1024, 1000,
                                                  100000};
  assert(affine_matrix_search_initialize(
             kernel, session, parameters(),
             AffineMatrixAdmission::AuthenticatedSubimage, persistence)
             .status == AffineMatrixInitStatus::Applied);
  for (const int32_t decision : {-1, -2, -3, -4}) {
    kernel_assign_decision(kernel, kernel_internalize(kernel, decision));
    assert(propagate_apply(kernel) == ApplyResult::Ok);
  }
  const auto detached = minimal
      ? affine_matrix_search_step_detached_minimal(kernel, session)
      : affine_matrix_search_step_detached_source(kernel, session);
  assert(detached.status == AffineMatrixServiceStatus::Applied &&
         detached.reason_width == (minimal ? 3u : 5u) &&
         detached.live.detached_reason &&
         kernel.value(kernel_internalize(kernel, 5)) > 0);
  assert(proof.add_external_rup(++kernel.next_clause_id, nullptr, 0));
  proof.flush();
  kernel.proof = nullptr;

  FILE *cnf = std::fopen(cnf_path, "w");
  assert(cnf);
  std::fprintf(cnf, "p cnf 7 %zu\n", original.size() + 5);
  for (const auto &clause : original) {
    for (const int32_t literal : clause) std::fprintf(cnf, "%d ", literal);
    std::fprintf(cnf, "0\n");
  }
  for (const int32_t unit : {-1, -2, -3, -4, -5})
    std::fprintf(cnf, "%d 0\n", unit);
  std::fclose(cnf);
  std::fclose(journal);
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
  if(argc==4&&!std::strcmp(argv[1],"--proof-drat"))return root_proof_fixture(argv[2],argv[3],true,ProofFormat::DratAscii);
  if(argc==4&&!std::strcmp(argv[1],"--proof-pbp"))return root_proof_fixture(argv[2],argv[3],true,ProofFormat::PbpStreaming);
  if(argc==4&&!std::strcmp(argv[1],"--proof-search"))return search_proof_fixture(argv[2],argv[3]);
  if(argc==5&&!std::strcmp(argv[1],"--proof-search-seed"))
    return search_proof_fixture(argv[3],argv[4],
                                static_cast<uint32_t>(std::stoul(argv[2])));
  if(argc==5&&!std::strcmp(argv[1],"--proof-search-chain-seed"))
    return search_chain_proof_fixture(argv[3],argv[4],
                                      static_cast<uint32_t>(std::stoul(argv[2])));
  unchanged_terminal_initialization_is_suppressed();
  complete_snapshot_policy_admission();
  deferred_reason_has_public_materialization_boundary();
  detached_minimal_reason_is_public_and_unwatched();
  lazy_source_handles_are_multiple_public_and_consumed_on_demand();
  lazy_minimal_handle_is_public_and_consumed_on_demand();
  lazy_minimal_materializes_only_analyze_ancestor();
  lazy_source_backtrack_discards_without_publication();
  deferred_reason_materializes_at_later_conflict(false);
  deferred_reason_materializes_at_later_conflict(true);
  repeated_deferred_pbp_reasons_preserve_cache_generation();
  deferred_pbp_reason_survives_backtrack_and_cache_resume();
  partial_admission_feedback_provenance_and_replay();
  restart_only_replay();root_decline_atomicity();std::puts("test_affine_matrix_search: passed");
}
