#undef NDEBUG
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix
#include "gf3_formula_support.hpp"
#include "temporary_file.hpp"
#include "dimacs_text.hpp"
#include "test_io.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "strategy/online_inprocess.hpp"

#include <cassert>
#include <fstream>
#include <algorithm>
#include <string>
#include <vector>

using namespace tactix;
using namespace tactix::strategy;

namespace {

Kernel install() { return gf3_parsed_kernel(gf3_row_fixture()); }

Gf3GeneralRowActionLimits limits() { return gf3_general_row_action_limits(); }

struct Bound {
  Kernel kernel = install();
  Gf3GeneralRowActionSession session;
  FILE *records = std::tmpfile();
  Bound() {
    assert(records);
    kernel.structural_provenance = records;
    kernel.gf3_general_row_action_session = &session;
  }
  ~Bound() { std::fclose(records); }
};

void pagination_replay_and_atomic_apply() {
  Bound b;
  Gf3GeneralRowActionOfferWindow window{1000, 1, 10000000, false, {}};
  const auto first =
      gf3_general_row_action_offer(b.kernel, b.session, limits(), window);
  assert(first.status == Gf3GeneralRowActionStatus::Applied);
  assert(first.offers.size() == 1 && first.have_resume_after);
  assert(!first.complete);
  window.have_after = true;
  window.after = first.resume_after;
  const auto second =
      gf3_general_row_action_offer(b.kernel, b.session, limits(), window);
  assert(second.offers.empty() || first.offers.front() < second.offers.front());

  const uint64_t before = b.kernel.next_clause_id;
  auto corrupt = first.offers.front();
  corrupt.identity.back() = corrupt.identity.back() == '0' ? '1' : '0';
  const auto rejected = gf3_general_row_action_apply(
      b.kernel, b.session, limits(), corrupt);
  assert(rejected.status == Gf3GeneralRowActionStatus::Stale);
  assert(b.kernel.next_clause_id == before && b.session.keys().empty());

  const auto applied = gf3_general_row_action_apply(
      b.kernel, b.session, limits(), first.offers.front());
  assert(applied.status == Gf3GeneralRowActionStatus::Applied);
  assert(applied.live.installed_clause_id &&
         applied.live.plan.projection.external_extent_unchanged &&
         applied.live.plan.projection.extension_variables.empty());
  assert(b.session.keys().size() == 1);
  record_gf3_general_row_action_feedback(b.kernel, applied);
  assert(b.kernel.stats.gf3_general_row_applied == 1);
}

void executor_requires_propagate_and_emits_dsr() {
  Bound source;
  const auto offered = gf3_general_row_action_offer(
      source.kernel, source.session, limits(), {1000, 1, 10000000, false, {}});
  assert(offered.offers.size() == 1);
  OnlineInvocation step;
  step.id = 32;
  step.rule = "Gf3GeneralRowConsequence";
  step.parameters = Gf3GeneralRowConsequenceParameters{};
  step.granularity = InvocationGranularity::OneInstance;
  step.gf3_general_row_instances = offered.offers;
  step.budget = WorkLimit::bounded(1);
  OnlineStrategyProgram program{{step}, {}};
  std::string error;
  assert(!validate_online_strategy_program(program, &error));
  step.post_application = PostApplication::Propagate;
  program.sequence = {step};

  Bound target;
  TemporaryFile file;
  Proof proof;
  assert(proof.open(file.path.c_str(), ProofFormat::DsrAscii));
  proof.bind_variable_map(&target.kernel.internal_to_external);
  proof.bind_external_variable_map(&target.kernel.external_to_internal);
  target.kernel.proof = &proof;
  OnlineStrategyExecutor executor;
  assert(executor.configure(program, 1, ReconfigureMode::ResetRuntime,
                            &error));
  const auto result = executor.advance(target.kernel);
  assert(result.status == InvocationStatus::Completed &&
         result.applications == 1);
  assert(target.kernel.stats.gf3_general_row_applied == 1);
  proof.flush();
  target.kernel.proof = nullptr;
  assert(!read_file(file.path).empty());
}

bool apply_search_implication(Kernel &kernel,
                              Gf3GeneralRowActionSession &session,
                              FILE *records) {
  const auto offered = gf3_general_row_action_offer(
      kernel, session, limits(), {10000, 128, 10000000, false, {}});
  assert(offered.status == Gf3GeneralRowActionStatus::Applied &&
         !offered.offers.empty());
  bool implied = false;
  for (const auto &selection : offered.offers) {
    Gf3GeneralRowLiveRequest request;
    request.limits = limits().live;
    request.selection = {selection.outcome, selection.tuple};
    request.selected_identity = selection.identity;
    request.action_id = kernel.structural_action_sequence + 1;
    request.semantic_id = request.action_id;
    request.records = records;
    request.retained_owner = session.owner();
    const auto planned = gf3_general_row_live_inspect(kernel, request);
    if (planned.status != Gf3GeneralRowLiveStatus::Ready ||
        planned.plan.consequence.size() != 2)
      continue;
    for (unsigned pivot = 0; pivot < 2 && !implied; ++pivot) {
      const Lit false_literal = kernel_internalize(
          kernel, -planned.plan.consequence[pivot]);
      const Lit implied_literal = kernel_internalize(
          kernel, planned.plan.consequence[1 - pivot]);
      if (false_literal == INVALID_LIT || implied_literal == INVALID_LIT ||
          kernel.value(false_literal) || kernel.value(implied_literal))
        continue;
      kernel_assign_decision(kernel, false_literal);
      const auto propagated = propagate_apply(kernel);
      if (propagated == ApplyResult::Ok && !kernel.value(implied_literal)) {
        const auto applied = gf3_general_row_action_apply(
            kernel, session, limits(), selection);
        assert(applied.status == Gf3GeneralRowActionStatus::Applied &&
               applied.live.propagation_pending);
        assert(propagate_apply(kernel) == ApplyResult::Ok);
        assert(kernel.value(implied_literal) > 0);
        implied = true;
      }
      kernel_backtrack(kernel, 0);
      assert(propagate_apply(kernel) == ApplyResult::Ok);
    }
    if (implied) break;
  }
  return implied;
}

void retained_owner_search_implication() {
  Bound b;
  const auto first = gf3_general_row_action_offer(
      b.kernel, b.session, limits(), {1000, 1, 10000000, false, {}});
  assert(first.offers.size() == 1);
  const auto initialized = gf3_general_row_action_apply(
      b.kernel, b.session, limits(), first.offers.front());
  assert(initialized.status == Gf3GeneralRowActionStatus::Applied);
  assert(propagate_apply(b.kernel) == ApplyResult::Ok);
  assert(apply_search_implication(b.kernel, b.session, b.records));
}

void c_v11_deep_copies_typed_selection() {
  Bound source;
  const auto offered = gf3_general_row_action_offer(
      source.kernel, source.session, limits(), {1000, 1, 10000000, false, {}});
  assert(offered.offers.size() == 1);
  std::string identity = offered.offers.front().identity;

  tactix_c_handle *solver = tactix_init();
  FILE *records = std::tmpfile();
  assert(solver && records &&
         tactix_bind_structural_provenance(solver, records) == 0);
  for (const auto &clause : gf3_row_fixture().clauses) {
    for (const int32_t literal : clause) tactix_add(solver, literal);
    tactix_add(solver, 0);
  }
  const Gf3GeneralRowConsequenceParameters p;
  tactix_online_step_v11 cstep{};
  auto &base = cstep.base.base.base.base.base.base.base.base.base.base;
  base.step_id = 32;
  base.rule_id = 43;
  base.parameters.kind =
      TACTIX_ONLINE_PARAMETERS_GF3_GENERAL_ROW_CONSEQUENCE;
  auto &c = base.parameters.value.gf3_general_row_consequence;
  c.gaussian = gf3_c_gaussian(p.gaussian);
  c.maximum_row_tuples = p.maximum_row_tuples;
  c.maximum_row_width = p.maximum_row_width;
  c.maximum_duplicate_scan_work = p.maximum_duplicate_scan_work;
  c.maximum_arena_words = p.maximum_arena_words;
  base.granularity = TACTIX_ONLINE_GRANULARITY_ONE;
  base.post_application = TACTIX_ONLINE_POST_PROPAGATE;
  base.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  base.repeat_limit = 1;
  base.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  base.budget = 1;
  const tactix_online_gf3_general_row_instance selected{
      offered.offers.front().outcome, offered.offers.front().tuple,
      identity.c_str()};
  cstep.gf3_general_row_instances = &selected;
  cstep.gf3_general_row_instance_count = 1;
  const tactix_online_program_v11 program{
      &cstep, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_configure_v11(
             solver, &program, 1, TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  identity.assign(identity.size(), '0');
  tactix_online_result result{};
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.rule_id == 43 && result.status == TACTIX_ONLINE_COMPLETED);
  tactix_release(solver);
  std::fclose(records);
}

void emit_public_fixture(const std::string &cnf_path,
                         const std::string &proof_path, bool search,
                         ProofFormat format = ProofFormat::DsrAscii) {
  Gf3Formula source = gf3_row_fixture();
  Kernel kernel = gf3_parsed_kernel(source);
  for (const auto &clause : std::vector<std::vector<int32_t>>{
           {13, 14}, {13, -14}, {-13, 14}, {-13, -14}})
    source.add(clause);
  std::ofstream cnf(cnf_path);
  assert(cnf);
  cnf << dimacs_text(14, source.clauses);
  cnf.close();

  kernel_reserve_vars(kernel, 14);
  // Reserve the IDs of the four checker-only closing clauses so additions
  // keep their IDs in the emitted CNF.
  kernel.next_clause_id += 4;
  Gf3GeneralRowActionSession session;
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  kernel.gf3_general_row_action_session = &session;
  const auto offered = gf3_general_row_action_offer(
      kernel, session, limits(), {1000, 1, 10000000, false, {}});
  assert(offered.status == Gf3GeneralRowActionStatus::Applied &&
         offered.offers.size() == 1);
  Proof proof;
  assert(proof.open(proof_path.c_str(), format));
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  uint64_t input_id = 0;
  for (const auto &external : source.clauses) {
    std::vector<Lit> clause;
    for (int32_t literal : external) clause.push_back(internalize(literal));
    proof.seed_population(++input_id, clause.data(), clause.size());
  }
  assert(input_id == kernel.next_clause_id);
  proof.finish_input_transaction();
  kernel.proof = &proof;
  const auto applied = gf3_general_row_action_apply(
      kernel, session, limits(), offered.offers.front());
  assert(applied.status == Gf3GeneralRowActionStatus::Applied);
  if (search) {
    assert(propagate_apply(kernel) == ApplyResult::Ok);
    assert(apply_search_implication(kernel, session, records));
  }
  const Lit positive = internalize(13);
  const Lit negative = internalize(-13);
  proof.add_rup(++kernel.next_clause_id, &positive, 1);
  proof.add_rup(++kernel.next_clause_id, &negative, 1);
  proof.add_rup(++kernel.next_clause_id, nullptr, 0);
  proof.flush();
  kernel.proof = nullptr;
  std::fclose(records);
}

} // namespace

int main(int argc, char **argv) {
  assert(std::string(tactix_rules()[43].name) == "Gf3GeneralRowConsequence");
  const auto reference = reference_inprocess_program();
  const auto step = std::find_if(reference.begin(), reference.end(),
                                 [](const auto &candidate) {
    return candidate.id == 32;
  });
  assert(step != reference.end() &&
         step->rule == "Gf3GeneralRowConsequence" && !step->enabled);
  pagination_replay_and_atomic_apply();
  executor_requires_propagate_and_emits_dsr();
  retained_owner_search_implication();
  c_v11_deep_copies_typed_selection();
  if (argc == 3) emit_public_fixture(argv[1], argv[2], false);
  else if (argc == 4 && std::string(argv[1]) == "--proof-search")
    emit_public_fixture(argv[2], argv[3], true);
  else if (argc == 4 && std::string(argv[1]) == "--proof-search-pbp")
    emit_public_fixture(argv[2], argv[3], true, ProofFormat::PbpStreaming);
  else assert(argc == 1);
  std::puts("test_gf3_general_row_public: passed");
}
