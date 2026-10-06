#undef NDEBUG
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
  decide(controller, kernel, -1);
  decide(controller, kernel, -2);
  const auto result = controller.affine_matrix_fixpoint_service();
  assert(result.status == AffineMatrixServiceStatus::Applied &&
         result.result == ApplyResult::Ok && result.bcp_pending &&
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

int proof_fixture(const char *cnf_path, const char *proof_path) {
  Kernel kernel;
  AffineMatrixSearchSession session;
  kernel.affine_matrix_search_session = &session;
  kernel_reserve_vars(kernel, 10);
  const std::vector<int32_t> first{1, 2, 3, 4, 9, 10};
  const std::vector<int32_t> second{5, 6, 7, 8, 9, 10};
  add_equation(kernel, first, false);
  add_equation(kernel, second, true);
  FILE *journal = std::tmpfile();
  assert(journal);
  kernel.structural_provenance = journal;
  Proof proof;
  assert(proof.open(proof_path, ProofFormat::DsrAscii));
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  kernel.proof = &proof;
  RestartController controller(kernel);
  assert(controller.apply_rule(initialize_request(controller, kernel, parameters()),
                               {WorkUnit::Applications, 1, {}})
             .invocation.status == InvocationStatus::Completed);
  for (int32_t variable = 1; variable <= 8; ++variable)
    add_clause(kernel, {-variable});
  assert(!kernel_propagate(kernel));
  assert(controller.resume_search(resume_request(controller, kernel))
             .resumed_search);
  const auto service = controller.affine_matrix_fixpoint_service();
  assert(service.status == AffineMatrixServiceStatus::Applied &&
         service.live.conflict && service.bcp_pending);
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
      for (const int32_t literal : clause) std::fprintf(cnf, "%d ", literal);
      std::fprintf(cnf, "0\n");
    }
  for (int32_t variable = 1; variable <= 8; ++variable)
    std::fprintf(cnf, "-%d 0\n", variable);
  std::fclose(cnf);
  std::fclose(journal);
  return 0;
}

} // namespace

int main(int argc, char **argv) {
  if (argc == 4 && !std::strcmp(argv[1], "--proof"))
    return proof_fixture(argv[2], argv[3]);
  initialize_resume_publish_and_bcp();
  interruption_is_atomic();
  unchanged_terminal_initialization_is_suppressed();
  std::puts("test_affine_matrix_search: passed");
}
