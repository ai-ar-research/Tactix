#undef NDEBUG
#include "kernel/clause.hpp"
#include "policies/random_controller_policy.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/semantic_extension/affine_matrix_search.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
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

void decide(RestartController &controller, Kernel &kernel, int32_t external) {
  const Lit literal = kernel_internalize(kernel, external);
  assert(controller.search_service("Decide", lit_instance(kernel, literal), [&] {
    return execution::execute_action(kernel,
                                     execution::DecisionRequest{literal});
  }) == ApplyResult::Ok);
  assert(!kernel_propagate(kernel));
  assert(kernel_propagation_fixpoint(kernel));
}

void initialize_resume_publish_and_bcp() {
  Kernel kernel;
  AffineMatrixSearchSession session;
  kernel.affine_matrix_search_session = &session;
  FILE *journal = std::tmpfile();
  assert(journal);
  kernel.structural_provenance = journal;
  install_implication_formula(kernel);
  // A non-affine binary and its assignments are outside the admitted matrix
  // support and must not be charged or retained by matrix synchronization.
  kernel_reserve_vars(kernel, 7);
  add_clause(kernel, {6, 7});
  Capture capture;
  RestartController controller(kernel, {}, &capture);
  const auto p = parameters();
  const auto initialized = controller.apply_rule(
      initialize_request(controller, kernel, p),
      {WorkUnit::Applications, 1, {}});
  assert(initialized.invocation.status == InvocationStatus::Completed &&
         initialized.invocation.made_rule_progress && session.enabled() &&
         session.belongs_to(kernel) && session.owner_version() == 1);
  assert(controller.resume_search(resume_request(controller, kernel))
             .resumed_search);
  decide(controller, kernel, -6);
  assert(kernel.value(kernel_internalize(kernel, 7)) > 0);
  decide(controller, kernel, -1);
  decide(controller, kernel, -2);
  const auto result = controller.affine_matrix_fixpoint_service();
  assert(result.status == AffineMatrixServiceStatus::Applied &&
         result.result == ApplyResult::Ok && result.bcp_pending &&
         result.trail_capture_work == 5 &&
         result.synchronization_incremental &&
         result.synchronization_prefix == 0 &&
         result.synchronization_conditioned == 2 &&
         result.event_identity.find("am1:o=1") == 0 &&
         !kernel.value(kernel_internalize(kernel, -5)) &&
         session.published_keys() == 1);
  assert(!capture.actions.empty());
  const auto &feedback = capture.actions.back();
  assert(feedback.identity == "AffineMatrixSearch" &&
         feedback.native_request &&
         feedback.native_request->have_affine_matrix_event &&
         feedback.native_request->affine_matrix_event_identity ==
             result.event_identity &&
         feedback.native_request->affine_matrix_bcp_pending);
  assert(controller.search_service("Propagate", INSTANCE_CANONICAL, [&] {
    return propagate_apply(kernel);
  }) == ApplyResult::Ok);
  assert(kernel.value(kernel_internalize(kernel, -5)) > 0 &&
         kernel_propagation_fixpoint(kernel));
  const size_t feedback_after_bcp = capture.actions.size();
  const auto no_event = controller.affine_matrix_fixpoint_service();
  assert(no_event.status == AffineMatrixServiceStatus::NoEvent &&
         no_event.synchronization_incremental &&
         no_event.synchronization_prefix == 2 &&
         no_event.synchronization_conditioned == 1 &&
         kernel.stats.affine_matrix_service_extension_hits == 2 &&
         kernel.stats.affine_matrix_service_rebuilds == 0 &&
         kernel.stats.affine_matrix_service_retained_assignments == 2 &&
         kernel.stats.affine_matrix_service_conditioned_assignments == 3 &&
         kernel.stats.affine_matrix_service_trail_capture_work == 10 &&
         capture.actions.size() == feedback_after_bcp);

  assert(controller.full_restart() == ApplyResult::Ok);
  const uint64_t version = session.owner_version();
  auto invalid = p;
  invalid.basis_maximum_work = 0;
  assert(affine_matrix_search_initialize(kernel, session, invalid).status ==
         AffineMatrixInitStatus::Invalid);
  assert(session.owner_version() == version && session.belongs_to(kernel));
  auto exhausted = p;
  exhausted.maximum_snapshot_work = 1;
  assert(affine_matrix_search_initialize(kernel, session, exhausted).status ==
         AffineMatrixInitStatus::ResourceExhausted);
  assert(session.owner_version() == version && session.belongs_to(kernel));

  const std::string line = affine_matrix_service_trace_record(kernel, result);
  char path[] = "/tmp/tactix-affine-search-replay-XXXXXX";
  const int descriptor = mkstemp(path);
  assert(descriptor >= 0);
  close(descriptor);
  {
    ControllerTrace trace;
    assert(trace.write(path, 17, "affine-search-test"));
    assert(trace.event(line));
    assert(trace.finish(10));
  }
  {
    ControllerTrace replay;
    assert(replay.replay(path, "affine-search-test"));
    assert(replay.event(line));
    assert(replay.finish(10));
  }
  std::remove(path);
  std::fclose(journal);
}

void interruption_is_atomic() {
  for (const bool interrupt : {false, true}) {
    Kernel kernel;
    AffineMatrixSearchSession session;
    kernel.affine_matrix_search_session = &session;
    FILE *journal = std::tmpfile();
    assert(journal);
    kernel.structural_provenance = journal;
    install_implication_formula(kernel);
    RestartController controller(kernel);
    auto p = parameters();
    if (!interrupt) p.maximum_session_work = 1;
    assert(controller.apply_rule(initialize_request(controller, kernel, p),
                                 {WorkUnit::Applications, 1, {}})
               .invocation.status == InvocationStatus::Completed);
    assert(controller.resume_search(resume_request(controller, kernel))
               .resumed_search);
    decide(controller, kernel, -1);
    decide(controller, kernel, -2);
    volatile sig_atomic_t stop = interrupt;
    kernel.stop = &stop;
    const auto id = kernel.next_clause_id;
    const auto arena = kernel.arena.size();
    const long journal_size = std::ftell(journal);
    const auto result = controller.affine_matrix_fixpoint_service();
    assert(result.status ==
           (interrupt ? AffineMatrixServiceStatus::Interrupted
                      : AffineMatrixServiceStatus::ResourceExhausted));
    assert(kernel.next_clause_id == id && kernel.arena.size() == arena &&
           std::ftell(journal) == journal_size && !session.published_keys());
    std::fclose(journal);
  }
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

void root_unit_and_empty_conflict_publish() {
  {
    Kernel kernel;
    AffineMatrixSearchSession session;
    kernel.affine_matrix_search_session = &session;
    FILE *journal = std::tmpfile();
    assert(journal);
    kernel.structural_provenance = journal;
    kernel_reserve_vars(kernel, 5);
    add_equation(kernel, {1, 2, 3}, false);
    add_equation(kernel, {1, 2, 4}, false);
    add_equation(kernel, {3, 4, 5}, true);
    assert(affine_matrix_search_initialize(kernel, session, parameters()).status ==
           AffineMatrixInitStatus::Applied);
    const auto service = affine_matrix_search_step(kernel, session);
    assert(service.status == AffineMatrixServiceStatus::Applied &&
           service.result == ApplyResult::Ok && service.bcp_pending &&
           service.live.implication && !service.live.conflict &&
           service.reason_width == 1 &&
           kernel.value(kernel_internalize(kernel, 5)) > 0);
    std::fclose(journal);
  }
  {
    Kernel kernel;
    AffineMatrixSearchSession session;
    kernel.affine_matrix_search_session = &session;
    FILE *journal = std::tmpfile();
    assert(journal);
    kernel.structural_provenance = journal;
    kernel_reserve_vars(kernel, 3);
    add_equation(kernel, {1, 2, 3}, false);
    add_equation(kernel, {1, 2, 3}, true);
    assert(affine_matrix_search_initialize(kernel, session, parameters()).status ==
           AffineMatrixInitStatus::Applied);
    const auto service = affine_matrix_search_step(kernel, session);
    assert(service.status == AffineMatrixServiceStatus::Applied &&
           service.result == ApplyResult::Unsat && !service.bcp_pending &&
           service.live.conflict && !service.live.implication &&
           service.reason_width == 0 && kernel.inconsistent);
    std::fclose(journal);
  }
}

void invalidated_session_retires_without_terminal_failure() {
  for (const bool retire_variable : {false, true}) {
    Kernel kernel;
    AffineMatrixSearchSession session;
    kernel.affine_matrix_search_session = &session;
    FILE *journal = std::tmpfile();
    assert(journal);
    kernel.structural_provenance = journal;
    install_implication_formula(kernel);
    assert(affine_matrix_search_initialize(kernel, session, parameters()).status ==
           AffineMatrixInitStatus::Applied);
    RestartController controller(kernel);
    assert(controller.resume_search(resume_request(controller, kernel))
               .resumed_search);
    decide(controller, kernel, -1);
    decide(controller, kernel, -2);
    if (retire_variable) {
      kernel.var_status.resize(kernel.nvars, VarStatus::Active);
      kernel.var_status[var_of(kernel_internalize(kernel, 1))] =
          VarStatus::Substituted;
    } else {
      clause_at(kernel.arena, 0)->flags |= Clause::GARBAGE;
    }
    const auto service = controller.affine_matrix_fixpoint_service();
    assert(service.status == (retire_variable
                                  ? AffineMatrixServiceStatus::Inapplicable
                                  : AffineMatrixServiceStatus::Stale));
    assert(!service.terminal_failure() && !session.enabled() &&
           service.result == ApplyResult::NoOp);
    std::fclose(journal);
  }
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

  // A zero-row bounded attempt does not replace the useful session or append
  // an initialization record.  It does become the terminal suppressed
  // outcome for this exact root/profile pair.
  const uint64_t owner = session.owner_version();
  const long before = std::ftell(journal);
  p.basis_maximum_intermediate_width = 1;
  const auto rejected = affine_matrix_search_initialize(kernel, session, p);
  assert(rejected.status == AffineMatrixInitStatus::ResourceExhausted &&
         session.owner_version() == owner && !session.complete_admission() &&
         session.initialization_retry_suppressed(kernel, p) &&
         std::ftell(journal) == before);

  RestartController controller(kernel);
  assert(controller.resume_search(resume_request(controller, kernel))
             .resumed_search);
  decide(controller, kernel, -1);
  decide(controller, kernel, -2);
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

int proof_fixture(const char *cnf_path, const char *proof_path,
                  ProofFormat format) {
  Kernel kernel;
  AffineMatrixSearchSession session;
  kernel.affine_matrix_search_session = &session;
  kernel_reserve_vars(kernel, 17);
  const std::vector<int32_t> first{1, 2, 3, 4, 9, 10};
  const std::vector<int32_t> second{5, 6, 7, 8, 9, 10};
  const std::vector<int32_t> skipped{11, 12, 13, 14, 15, 16, 17};
  add_equation(kernel, first, false);
  add_equation(kernel, second, true);
  add_equation(kernel, skipped, false);
  FILE *journal = std::tmpfile();
  assert(journal);
  kernel.structural_provenance = journal;
  Proof proof;
  assert(format == ProofFormat::DsrAscii || format == ProofFormat::DratAscii);
  assert(proof.open(proof_path, format));
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  kernel.proof = &proof;
  RestartController controller(kernel);
  auto partial = parameters();
  partial.basis_maximum_intermediate_width = 6;
  assert(controller.apply_rule(initialize_request(controller, kernel, partial),
                               {WorkUnit::Applications, 1, {}})
             .invocation.status == InvocationStatus::Completed);
  assert(session.enabled() && !session.complete_admission() &&
         kernel.stats.affine_matrix_init_applied_partial == 1);
  assert(controller.resume_search(resume_request(controller, kernel))
             .resumed_search);
  for (int32_t variable = 1; variable <= 8; ++variable)
    decide(controller, kernel, -variable);
  const auto first_service = controller.affine_matrix_fixpoint_service();
  assert(first_service.status == AffineMatrixServiceStatus::Applied &&
         first_service.live.conflict && first_service.bcp_pending &&
         first_service.live.proof_cache_insert &&
         !first_service.live.proof_cache_hit &&
         first_service.live.additions > 1 &&
         first_service.live.helper_extent);
  assert(kernel.stats.affine_matrix_service_proof_cache_inserts == 1 &&
         kernel.stats.affine_matrix_service_proof_cache_hits == 0);
  const std::string journal_after_insert = journal_contents(journal);
  const size_t insert_record_at =
      journal_after_insert.rfind("{\"kind\":\"affine-matrix-reason\"");
  assert(insert_record_at != std::string::npos);
  const std::string insert_record =
      journal_after_insert.substr(insert_record_at);
  assert(insert_record.find("\"sources\":[") != std::string::npos &&
         insert_record.find("\"occurrence\":") != std::string::npos &&
         insert_record.find("\"clause_id\":") != std::string::npos &&
         insert_record.find("\"proof_cache\":{\"status\":\"insert\","
                            "\"entry_id\":1") != std::string::npos &&
         insert_record.find("\"support_ids\":[") != std::string::npos &&
         insert_record.find("\"sources_ref\":") == std::string::npos);

  assert(controller.full_restart() == ApplyResult::Ok);
  assert(controller.resume_search(resume_request(controller, kernel))
             .resumed_search);
  decide(controller, kernel, 1);
  for (int32_t variable = 2; variable <= 7; ++variable)
    decide(controller, kernel, -variable);
  decide(controller, kernel, 8);
  const uint32_t cached_extent = kernel.external_to_internal.size();
  const auto second_service = controller.affine_matrix_fixpoint_service();
  assert(second_service.status == AffineMatrixServiceStatus::Applied &&
         second_service.live.conflict && second_service.bcp_pending &&
         second_service.live.proof_cache_hit &&
         !second_service.live.proof_cache_insert &&
         second_service.live.additions == 1 &&
         second_service.live.deletions == 0 &&
         second_service.live.helper_extent == 0 &&
         second_service.live.planner_work < first_service.live.planner_work &&
         second_service.live.journal_bytes < first_service.live.journal_bytes &&
         kernel.external_to_internal.size() == cached_extent);
  assert(kernel.stats.affine_matrix_service_proof_cache_inserts == 1 &&
         kernel.stats.affine_matrix_service_proof_cache_hits == 1);
  const std::string journal_after_hit = journal_contents(journal);
  assert(journal_after_hit.size() > journal_after_insert.size());
  const std::string hit_record =
      journal_after_hit.substr(journal_after_insert.size());
  assert(hit_record.find("\"reason\":[") != std::string::npos &&
         hit_record.find("\"sources_ref\":{\"cache_entry\":1,"
                         "\"proof_epoch\":0}") != std::string::npos &&
         hit_record.find("\"proof_cache\":{\"status\":\"hit\","
                         "\"entry_id\":1") != std::string::npos &&
         hit_record.find("\"sources\":[") == std::string::npos &&
         hit_record.find("\"source_ordinals\":") == std::string::npos &&
         hit_record.find("\"support_ids\":") == std::string::npos &&
         hit_record.size() < insert_record.size());
  const size_t first_reason = first_service.event_identity.find(";r=");
  const size_t second_reason = second_service.event_identity.find(";r=");
  assert(first_reason != std::string::npos &&
         second_reason != std::string::npos &&
         first_service.event_identity.substr(0, first_reason) ==
             second_service.event_identity.substr(0, second_reason) &&
         first_service.event_identity != second_service.event_identity);

  // A DSR witnessed entry first retires checker-only cached support.  The
  // epoch then forces this row to build fresh support instead of citing
  // clauses which the checker no longer owns.  DRAT has no witnessed entry;
  // a rejected add_sr leaves its legal retained support untouched.
  assert(proof.persistent_external_clauses() > 0);
  const uint64_t cache_epoch = proof.persistent_external_epoch();
  std::vector<Lit> witnessed_reason;
  for (int32_t variable = 1; variable <= 8; ++variable)
    witnessed_reason.push_back(kernel_internalize(kernel, variable));
  if (format == ProofFormat::DsrAscii) {
    assert(controller.full_restart() == ApplyResult::Ok);
    assert(controller.resume_search(resume_request(controller, kernel))
               .resumed_search);
    assert(proof.add_sr(++kernel.next_clause_id, witnessed_reason.data(),
                        witnessed_reason.size(), nullptr, 0, nullptr, 0,
                        kernel.external_to_internal.size()));
    assert(proof.persistent_external_clauses() == 0 &&
           proof.persistent_external_epoch() == cache_epoch + 1);

    decide(controller, kernel, 1);
    decide(controller, kernel, 2);
    for (int32_t variable = 3; variable <= 8; ++variable)
      decide(controller, kernel, -variable);
    const auto after_x7 = controller.affine_matrix_fixpoint_service();
    assert(after_x7.status == AffineMatrixServiceStatus::Applied &&
           after_x7.live.conflict && after_x7.bcp_pending &&
           after_x7.live.proof_cache_insert &&
           !after_x7.live.proof_cache_hit && after_x7.live.additions > 1 &&
           after_x7.live.helper_extent);
    assert(kernel.stats.affine_matrix_service_proof_cache_inserts == 2 &&
           kernel.stats.affine_matrix_service_proof_cache_hits == 1);
    const std::string journal_after_x7 = journal_contents(journal);
    assert(journal_after_x7.size() > journal_after_hit.size());
    const std::string epoch_insert =
        journal_after_x7.substr(journal_after_hit.size());
    assert(epoch_insert.find("\"sources\":[") != std::string::npos &&
           epoch_insert.find("\"proof_cache\":{\"status\":\"insert\","
                             "\"entry_id\":2,\"generation\":1,"
                             "\"proof_epoch\":1") != std::string::npos &&
           epoch_insert.find("\"sources_ref\":") == std::string::npos);
  } else {
    const size_t retained = proof.persistent_external_clauses();
    assert(!proof.add_sr(++kernel.next_clause_id, witnessed_reason.data(),
                         witnessed_reason.size(), nullptr, 0, nullptr, 0,
                         kernel.external_to_internal.size()));
    assert(proof.persistent_external_clauses() == retained &&
           proof.persistent_external_epoch() == cache_epoch);
  }
  assert(kernel_propagate(kernel) && kernel.conflict_valid);
  assert(proof.add_external_rup(++kernel.next_clause_id, nullptr, 0));
  proof.flush();
  kernel.proof = nullptr;

  FILE *cnf = std::fopen(cnf_path, "w");
  assert(cnf);
  std::fprintf(cnf, "p cnf %zu 136\n", kernel.external_to_internal.size());
  for (const auto &[variables, rhs] :
       std::vector<std::pair<std::vector<int32_t>, bool>>{{first, false},
                                                          {second, true},
                                                          {skipped, false}})
    for (const auto &clause : clausal_form(variables, rhs)) {
      for (const int32_t literal : clause) std::fprintf(cnf, "%d ", literal);
      std::fprintf(cnf, "0\n");
    }
  for (int32_t variable = 1; variable <= 8; ++variable)
    std::fprintf(cnf, "-%d 0\n", variable);
  std::fclose(cnf);
  std::fclose(journal);
  return 0;
}

static int root_proof_fixture(const char *cnf_path, const char *proof_path,
                              bool unit) {
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
  Proof proof;
  assert(proof.open(proof_path, ProofFormat::DsrAscii));
  kernel.proof = &proof;
  assert(affine_matrix_search_initialize(kernel, session, parameters()).status ==
         AffineMatrixInitStatus::Applied);
  const auto service = affine_matrix_search_step(kernel, session);
  assert(service.status == AffineMatrixServiceStatus::Applied);
  if (unit) {
    assert(service.result == ApplyResult::Ok && service.reason_width == 1);
    // The original CNF also contains -5.  Keeping it out of the captured
    // matrix isolates publication of the derived root unit before refutation.
    assert(proof.add_external_rup(++kernel.next_clause_id, nullptr, 0));
  } else {
    assert(service.result == ApplyResult::Unsat && kernel.inconsistent);
  }
  proof.flush();
  kernel.proof = nullptr;
  FILE *cnf = std::fopen(cnf_path, "w");
  assert(cnf);
  std::fprintf(cnf, "p cnf %zu %zu\n", kernel.external_to_internal.size(),
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

} // namespace

int main(int argc, char **argv) {
  if (argc == 4 && !std::strcmp(argv[1], "--proof-root-unit"))
    return root_proof_fixture(argv[2], argv[3], true);
  if (argc == 4 && !std::strcmp(argv[1], "--proof-root-empty"))
    return root_proof_fixture(argv[2], argv[3], false);
  if (argc == 4 && !std::strcmp(argv[1], "--proof"))
    return proof_fixture(argv[2], argv[3], ProofFormat::DsrAscii);
  if (argc == 4 && !std::strcmp(argv[1], "--proof-drat"))
    return proof_fixture(argv[2], argv[3], ProofFormat::DratAscii);
  initialize_resume_publish_and_bcp();
  interruption_is_atomic();
  unchanged_terminal_initialization_is_suppressed();
  complete_snapshot_policy_admission();
  root_unit_and_empty_conflict_publish();
  invalidated_session_retires_without_terminal_failure();
  partial_admission_feedback_provenance_and_replay();
  std::puts("test_affine_matrix_search: passed");
}
