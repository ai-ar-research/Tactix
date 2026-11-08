#undef NDEBUG
#include "session/query_kernel.hpp"
#include "frontend/parse.hpp"
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "proof/proof.hpp"
#include "rules/decide/decide.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/restart/restart.hpp"
#include "rules/substitution_redundancy/substitution_redundancy.hpp"
#include "strategy/conflict_analysis_execution.hpp"
#include "temporary_file.hpp"
#include "query_test_support.hpp"

#include <cassert>
#include <string>
#include <vector>

using namespace tactix;
using namespace tactix::session;

using query_test::clause;
using query_test::drive;

static void repeated_queries(bool dynamic) {
  QueryHistory h;
  QueryKernel session;
  clause(h, {1, 2});
  clause(h, {-1, 2});
  assert(h.assume(-2) == InputStatus::Ok);
  assert(h.begin_query() == InputStatus::Ok);
  assert(session.replay(h) == ReplayStatus::Ok);
  const auto first = session.ticket();
  Kernel &k = *session.kernel(first);
  assert(k.level == 0 && !k.values[internalize(2)]);
  assert(session.assumption_input_ids(first).size() == 1);
  assert(drive(session, h, dynamic) == 20);
  assert(!k.inconsistent); // a failed assumption does not refute the base
  assert(h.finish_query(1, QueryResult::Unsat) == InputStatus::Ok);
  assert(!session.kernel(first));
  assert(h.begin_query() == InputStatus::Ok);
  assert(session.replay(h) == ReplayStatus::Ok);
  assert(!session.kernel(first));
  assert(drive(session, h, dynamic) == 10);
  assert(h.finish_query(2, QueryResult::Sat) == InputStatus::Ok);

  // Previously unmentioned assumption variable; no hidden root assignment.
  assert(h.assume(-9) == InputStatus::Ok);
  assert(h.begin_query() == InputStatus::Ok);
  assert(session.replay(h) == ReplayStatus::Ok);
  const auto third = session.ticket();
  assert(!kernel_var_retired(*session.kernel(third), 8));
  assert(session.kernel(third)->values[internalize(-9)] == 0);
  assert(drive(session, h, dynamic) == 10);
  assert(h.finish_query(3, QueryResult::Sat) == InputStatus::Ok);
  clause(h, {-2});
  assert(h.begin_query() == InputStatus::Ok);
  assert(session.replay(h) == ReplayStatus::Ok);
  assert(drive(session, h, dynamic) == 20);
  assert(session.kernel(session.ticket())->inconsistent);
}

static void replay_restores_representation_and_namespaces() {
  QueryHistory h;
  QueryKernel s;
  clause(h, {1, 2});
  assert(h.begin_query() == InputStatus::Ok);
  assert(s.replay(h) == ReplayStatus::Ok);
  const auto old = s.ticket();
  Kernel &k = *s.kernel(old);
  const auto &bve = tactix_rules()[tactix_rule_id("BVE")];
  assert(rule_guard(bve, k));
  assert(bve.apply_instance(k, lit_instance(internalize(1))) == ApplyResult::Ok);
  assert(k.projection_changed && !k.reconstruction.empty());
  kernel_compact_variables(k);
  assert(s.replay(h) == ReplayStatus::Ok);
  assert(!s.kernel(old));
  Kernel &restored = *s.kernel(s.ticket());
  assert(!restored.projection_changed && restored.reconstruction.empty());
  assert(restored.nvars == 2 && restored.unassigned_active == 2);
  assert(kernel_internalize(restored, 1) != INVALID_LIT);
  assert(!kernel_var_retired(restored, 0));
  const Var extension = kernel_new_extension_vars(restored, 1);
  assert(kernel_externalize(restored, make_lit(extension, false)) == 3);
  assert(decide_apply(restored, make_lit(extension, false), false) == ApplyResult::Ok);
  assert(h.finish_query(1, QueryResult::Unknown) == InputStatus::Ok);
  clause(h, {-3}); // a new client variable may reuse the OLD epoch's helper ID
  assert(h.begin_query() == InputStatus::Ok);
  assert(s.replay(h) == ReplayStatus::Ok);
  const auto current = s.ticket();
  const Kernel &new_input = *s.kernel(current);
  assert(new_input.values[internalize(-3)] > 0);
  assert(!kernel_is_solver_extension(new_input, 2));
  assert(!s.kernel(old));
}

static void wrong_branch_is_not_unsat() {
  QueryHistory h;
  QueryKernel s;
  clause(h, {1, 2});
  assert(h.assume(1) == InputStatus::Ok);
  assert(h.begin_query() == InputStatus::Ok);
  assert(s.replay(h) == ReplayStatus::Ok);
  const auto t = s.ticket();
  Kernel &k = *s.kernel(t);
  assert(decide_apply(k, internalize(-1), true) == ApplyResult::Ok);
  assert(propagate_service(k) == ApplyResult::Ok);
  assert(s.next_assumption(t).status == AssumptionStatus::Backtrack);
  assert(!s.publish_assumption_refutation(t));
  assert(restart_apply_to(k, 0) == ApplyResult::Ok);
  assert(s.next_assumption(t).status == AssumptionStatus::Propagate ||
         s.next_assumption(t).status == AssumptionStatus::Decide);
  assert(drive(s, h, true) == 10);
}

static void assumption_cursor_after_public_backtracking() {
  QueryHistory h;QueryKernel s;
  clause(h,{1,3});clause(h,{2,4});
  assert(h.assume(1)==InputStatus::Ok&&h.assume(2)==InputStatus::Ok);
  assert(h.begin_query()==InputStatus::Ok&&s.replay(h)==ReplayStatus::Ok);
  const auto t=s.ticket();Kernel &k=*s.kernel(t);
  assert(s.next_assumption(t).index==0&&s.decide_assumption(t,0));
  assert(propagate_service(k)==ApplyResult::Ok);
  assert(s.next_assumption(t).status==AssumptionStatus::Decide);
  assert(s.next_assumption(t).index==1); // a satisfied prefix was observed
  assert(restart_apply_to(k,0)==ApplyResult::Ok);
  assert(decide_apply(k,internalize(-1),false)==ApplyResult::Ok);
  assert(propagate_service(k)==ApplyResult::Ok);
  k.stats={}; // semantic invalidation survives telemetry reset
  assert(s.next_assumption(t).status==AssumptionStatus::Backtrack);
  assert(!s.publish_assumption_refutation(t));
  assert(restart_apply_to(k,0)==ApplyResult::Ok);
  assert(propagate_service(k)==ApplyResult::Ok);
  assert(s.decide_assumption(t,0)&&propagate_service(k)==ApplyResult::Ok);
  assert(s.decide_assumption(t,1)&&propagate_service(k)==ApplyResult::Ok);
  assert(s.next_assumption(t).status==AssumptionStatus::Ready);
  assert(restart_apply_to(k,0)==ApplyResult::Ok);
  assert(decide_apply(k,internalize(-2),false)==ApplyResult::Ok);
  assert(propagate_service(k)==ApplyResult::Ok);
  assert(s.next_assumption(t).status==AssumptionStatus::Backtrack);
  assert(restart_apply_to(k,0)==ApplyResult::Ok);
  k.affine_capture_epochs_trusted=false; // direct-mutation clients opt out
  assert(drive(s,h,true)==10);
}

static void decision_on_skipped_retired_assumption() {
  QueryHistory h;QueryKernel s;
  clause(h,{1,2});assert(h.assume(1)==InputStatus::Ok);
  assert(h.begin_query()==InputStatus::Ok&&s.replay(h)==ReplayStatus::Ok);
  const auto t=s.ticket();Kernel &k=*s.kernel(t);
  const auto &bve=tactix_rules()[tactix_rule_id("BVE")];
  assert(bve.apply_instance(k,lit_instance(k,internalize(1)))==ApplyResult::Ok);
  assert(kernel_var_retired(k,0)&&!s.projection_state(t).restore);
  assert(s.next_assumption(t).status==AssumptionStatus::Ready);
  assert(decide_apply(k,internalize(-1),true)==ApplyResult::Ok);
  assert(propagate_service(k)==ApplyResult::Ok);
  assert(s.next_assumption(t).status==AssumptionStatus::Backtrack);
  assert(!s.publish_assumption_refutation(t));
  assert(restart_apply_to(k,0)==ApplyResult::Ok);
  assert(propagate_service(k)==ApplyResult::Ok);
  assert(s.next_assumption(t).status==AssumptionStatus::Ready);
  assert(decide_apply(k,internalize(1),true)==ApplyResult::Ok);
  assert(drive(s,h,true)==10);
}

static void assumption_restarts_duplicates_and_compaction() {
  QueryHistory h;
  QueryKernel s;
  clause(h, {2});
  assert(h.assume(2) == InputStatus::Ok); // already a proved root
  assert(h.assume(-5) == InputStatus::Ok);
  assert(h.assume(-5) == InputStatus::Ok);
  assert(h.begin_query() == InputStatus::Ok);
  assert(s.replay(h) == ReplayStatus::Ok);
  const auto t = s.ticket();
  Kernel &k = *s.kernel(t);
  kernel_compact_variables(k, true);
  const auto step = s.next_assumption(t);
  assert(step.status == AssumptionStatus::Decide && step.index == 1);
  assert(!s.decide_assumption(t, 0));
  assert(s.decide_assumption(t, 1));
  assert(k.level == 1);
  assert(propagate_service(k) == ApplyResult::Ok);
  assert(s.next_assumption(t).status == AssumptionStatus::Ready);
  assert(restart_apply_to(k, 0) == ApplyResult::Ok);
  assert(propagate_service(k) == ApplyResult::Ok);
  assert(s.next_assumption(t).status == AssumptionStatus::Decide);
  assert(drive(s, h, true) == 10);
  assert(h.finish_query(1, QueryResult::Sat) == InputStatus::Ok);
  assert(h.assume(1) == InputStatus::Ok);
  assert(h.assume(1) == InputStatus::Ok);
  assert(h.assume(-1) == InputStatus::Ok);
  assert(h.begin_query() == InputStatus::Ok);
  assert(s.replay(h) == ReplayStatus::Ok);
  assert(drive(s, h, false) == 20);
  assert(!s.kernel(s.ticket())->inconsistent);
}

static void projection_changes_require_restoration() {
  QueryHistory h;
  QueryKernel s;
  clause(h, {1, 2});
  assert(h.assume(-1) == InputStatus::Ok);
  assert(h.begin_query() == InputStatus::Ok);
  assert(s.replay(h) == ReplayStatus::Ok);
  const auto t = s.ticket();
  Kernel &k = *s.kernel(t);
  SubstitutionRedundancyInstance sr;
  sr.clause = {internalize(1)};
  sr.witness = {internalize(1)};
  const auto result = substitution_redundancy_apply_detailed(k, sr);
  assert(result.rule_result == ApplyResult::Ok && k.projection_changed);
  k.stats = {}; // correctness may not depend on telemetry surviving a reset
  assert(s.next_assumption(t).status == AssumptionStatus::Restore);
  assert(!s.publish_assumption_refutation(t));
  assert(s.replay(h) == ReplayStatus::Ok);
  assert(drive(s, h, false) == 10);
}

static void tickets_boundaries_and_busy_replay() {
  QueryHistory h, other;
  QueryKernel s, foreign;
  assert(s.replay(h) == ReplayStatus::NoActiveQuery);
  assert(!s.kernel({}) && !s.flush_proof({}));
  assert(!s.publish_assumption_refutation({}));
  assert(!s.decide_assumption({}, 0));
  assert(h.begin_query() == InputStatus::Ok);
  assert(other.begin_query() == InputStatus::Ok);
  assert(s.replay(h) == ReplayStatus::Ok);
  assert(foreign.replay(other) == ReplayStatus::Ok);
  const auto t = s.ticket();
  assert(s.replay(other) == ReplayStatus::WrongHistory && s.kernel(t));
  assert(s.replay(h, std::make_unique<Proof>()) == ReplayStatus::ProofUnavailable);
  assert(s.kernel(t));
  assert(!foreign.kernel(t));
  Kernel &k = *s.kernel(t);
  k.inprocessing = true;
  assert(s.replay(h) == ReplayStatus::Busy && s.kernel(t) == &k);
  assert(s.next_assumption(t).status == AssumptionStatus::Busy);
  k.inprocessing = false;
  assert(s.replay(h) == ReplayStatus::Ok);
  assert(!s.kernel(t));
  assert(h.finish_query(1, QueryResult::Unknown) == InputStatus::Ok);
  assert(s.replay(h) == ReplayStatus::NoActiveQuery);
  assert(s.next_assumption(s.ticket()).status == AssumptionStatus::Stale);
}

static void certificate_fixture(const char *cnf, const char *path, const std::string &mode,
                                const std::string &scenario) {
  QueryHistory h;
  if (scenario == "opposite") {
    assert(h.assume(1) == InputStatus::Ok);
    assert(h.assume(1) == InputStatus::Ok);
    assert(h.assume(-1) == InputStatus::Ok);
  } else if (scenario == "root") {
    clause(h, {1});
    clause(h, {-1, 2}); // derive a root before the later checker assumption
    clause(h, {-2, 3});
    assert(h.assume(-3) == InputStatus::Ok);
  } else {
    clause(h, {1, 2});
    clause(h, {-1, 2});
    assert(h.assume(-2) == InputStatus::Ok);
  }
  assert(h.begin_query() == InputStatus::Ok);
  FILE *input = std::fopen(cnf, "w");
  assert(input && h.write_query_dimacs(1, input));
  assert(std::fclose(input) == 0);
  std::unique_ptr<Proof> proof;
  if (mode != "none") {
    proof = std::make_unique<Proof>();
    assert(proof->open(path, mode == "pbp" ? ProofFormat::PbpStreaming : ProofFormat::DsrAscii));
  }
  QueryKernel s;
  assert(s.replay(h, std::move(proof)) == ReplayStatus::Ok);
  assert(drive(s, h, false) == 20);
  assert(s.flush_proof(s.ticket()));
  const auto &k = *s.kernel(s.ticket());
  std::printf("query result=20 conflicts=%llu decisions=%llu propagations=%llu ticks=%llu roots=%llu\n",
              (unsigned long long)k.stats.conflicts,
              (unsigned long long)k.stats.decisions,
              (unsigned long long)k.stats.propagations,
              (unsigned long long)k.stats.ticks,
              (unsigned long long)k.stats.root_assignments);
}

int main(int argc, char **argv) {
  if (argc == 5) {
    certificate_fixture(argv[1], argv[2], argv[3], argv[4]);
    return 0;
  }
  assert(argc == 1);
  repeated_queries(false);
  repeated_queries(true);
  replay_restores_representation_and_namespaces();
  wrong_branch_is_not_unsat();
  assumption_cursor_after_public_backtracking();
  decision_on_skipped_retired_assumption();
  assumption_restarts_duplicates_and_compaction();
  projection_changes_require_restoration();
  tickets_boundaries_and_busy_replay();
  std::puts("query kernel: PASS");
}
