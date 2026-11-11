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

static void wrong_branch_is_not_unsat() {
  QueryHistory h;
  QueryKernel s;
  clause(h, {1, 2});
  assert(h.assume(3) == InputStatus::Ok); // an undecided prefix must not hide branch safety
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

static void root_refutation_looks_past_undecided_assumptions() {
  {
    QueryHistory h;
    QueryKernel s;
    clause(h,{1,2}); clause(h,{-1,2}); // F implies 2, initially undiscovered
    assert(h.assume(3)==InputStatus::Ok && h.assume(-2)==InputStatus::Ok);
    assert(h.begin_query()==InputStatus::Ok && s.replay(h)==ReplayStatus::Ok);
    const auto t=s.ticket();
    assert(s.next_assumption(t).status==AssumptionStatus::Decide);
    assert(s.next_assumption(t).status==AssumptionStatus::Decide); // cached absence
    auto &k=*s.kernel(t);
    kernel_assign_unit(k,kernel_internalize(k,2));
    assert(propagate_service(k)==ApplyResult::Ok);
    const auto next=s.next_assumption(t);
    assert(next.status==AssumptionStatus::QueryUnsat && next.index==1);
  }
  QueryHistory h;
  QueryKernel s;
  clause(h, {1});
  for (int literal=2;literal<=65;++literal)
    assert(h.assume(literal)==InputStatus::Ok);
  assert(h.assume(-1)==InputStatus::Ok);
  assert(h.begin_query()==InputStatus::Ok && s.replay(h)==ReplayStatus::Ok);
  auto t=s.ticket();
  auto check=[&] {
    const auto next=s.next_assumption(t);
    assert(next.status==AssumptionStatus::QueryUnsat && next.index==64 && next.literal==-1);
  };
  check();
  auto &k=*s.kernel(t);
  kernel_compact_variables(k,true); // root now lives in the fixed external table
  k.stats={};
  check();
  assert(s.publish_assumption_refutation(t) && !k.inconsistent);
  assert(h.finish_query(1,QueryResult::Unsat)==InputStatus::Ok);
  assert(h.begin_query()==InputStatus::Ok && s.replay(h)==ReplayStatus::Ok);
  assert(drive(s,h,true)==10); // the failed cube never refutes permanent F
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

static void unrelated_decision_cannot_refute_cube() {
  QueryHistory h; QueryKernel s;
  clause(h,{-5,-1});
  assert(h.assume(1)==InputStatus::Ok);
  assert(h.begin_query()==InputStatus::Ok && s.replay(h)==ReplayStatus::Ok);
  const auto t=s.ticket(); Kernel &k=*s.kernel(t);
  assert(decide_apply(k,kernel_internalize(k,5),false)==ApplyResult::Ok);
  assert(propagate_service(k)==ApplyResult::Ok);
  assert(kernel_external_value(k,internalize(1))<0);
  assert(s.next_assumption(t).status==AssumptionStatus::Backtrack);
  assert(!s.publish_assumption_refutation(t));
  assert(restart_apply_to(k,0)==ApplyResult::Ok);
  assert(drive(s,h,true)==10);
}

static void assigned_cube_with_foreign_branch() {
  QueryHistory h; QueryKernel s;
  clause(h,{1,3}); clause(h,{2,4}); clause(h,{5,6});
  assert(h.assume(1)==InputStatus::Ok && h.assume(2)==InputStatus::Ok);
  assert(h.begin_query()==InputStatus::Ok && s.replay(h)==ReplayStatus::Ok);
  const auto t=s.ticket(); Kernel &k=*s.kernel(t);
  assert(s.decide_assumption(t,0) && propagate_service(k)==ApplyResult::Ok);
  assert(s.decide_assumption(t,1) && propagate_service(k)==ApplyResult::Ok);
  assert(s.next_assumption(t).status==AssumptionStatus::Ready);
  assert(decide_apply(k,internalize(-5),false)==ApplyResult::Ok);
  assert(s.next_assumption(t).status==AssumptionStatus::Propagate);
  assert(propagate_service(k)==ApplyResult::Ok);
  k.stats={};
  assert(s.next_assumption(t).status==AssumptionStatus::Ready);
  assert(restart_apply_to(k,2)==ApplyResult::Ok);
  assert(s.next_assumption(t).status==AssumptionStatus::Ready);
  assert(restart_apply_to(k,0)==ApplyResult::Ok);
  assert(decide_apply(k,internalize(-2),false)==ApplyResult::Ok);
  assert(propagate_service(k)==ApplyResult::Ok);
  assert(s.next_assumption(t).status==AssumptionStatus::Backtrack);
  assert(!s.publish_assumption_refutation(t));
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

int main(int argc, char **) {
  assert(argc == 1);
  wrong_branch_is_not_unsat();
  root_refutation_looks_past_undecided_assumptions();
  assumption_cursor_after_public_backtracking();
  assigned_cube_with_foreign_branch();
  unrelated_decision_cannot_refute_cube();
  decision_on_skipped_retired_assumption();
  assumption_restarts_duplicates_and_compaction();
  tickets_boundaries_and_busy_replay();
  std::puts("query kernel: PASS");
}
