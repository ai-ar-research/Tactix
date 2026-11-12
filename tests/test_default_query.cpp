#undef NDEBUG
#include "policies/default_policy.hpp"
#include "session/query_kernel.hpp"
#include "query_test_support.hpp"
#include "proof/proof.hpp"

#include <string>

using namespace tactix;
using namespace tactix::session;
using query_test::clause;

static void guarded_unsat(QueryHistory &h) {
  clause(h, {-1, 2, 3}); clause(h, {-1, 2, -3});
  clause(h, {-1, -2, 3}); clause(h, {-1, -2, -3});
}

static void repeated_default_queries() {
  QueryHistory h;
  clause(h, {1, 2});
  QueryKernel session;
  DefaultPolicy policy;
  for (unsigned q = 1; q <= 4; ++q) {
    if (q <= 2) assert(h.assume(-1) == InputStatus::Ok);
    if (q == 2) clause(h, {-2});
    if (q == 4) clause(h, {-1});
    assert(h.begin_query() == InputStatus::Ok);
    assert(session.replay(h) == ReplayStatus::Ok);
    const auto solved = policy.solve_query(session);
    if (solved.status != QuerySolveStatus::Complete)
      std::fprintf(stderr, "query=%u status=%u result=%d next=%u restore=%d witnesses=%llu aliases=%llu\n",
          q, unsigned(solved.status), solved.result,
          unsigned(session.next_assumption(session.ticket()).status),
          session.projection_state(session.ticket()).restore,
          (unsigned long long)session.projection_state(session.ticket()).reconstruction_literals,
          (unsigned long long)session.projection_state(session.ticket()).translated_assumptions);
    assert(solved.status == QuerySolveStatus::Complete);
    assert(solved.result == (q == 2 || q == 4 ? 20 : 10));
    if (solved.result == 10)
      assert(session.check_model(session.ticket()).status == ModelStatus::Valid);
    if (q == 1) {
      const Kernel &k = *session.kernel(session.ticket());
      assert(k.stats.lucky_early_calls);
      assert(session.projection_state(session.ticket()).rejected_models);
      assert(k.model_values[internalize(-1)] > 0);
    }
    if (q == 2) assert(!session.kernel(session.ticket())->inconsistent);
    assert(session.flush_proof(session.ticket()));
    assert(h.finish_query(q, static_cast<QueryResult>(solved.result)) == InputStatus::Ok);
  }
}

static void interrupted_and_invalid_entry() {
  DefaultPolicy policy;
  QueryKernel session;
  assert(policy.solve_query(session).status == QuerySolveStatus::InvalidState);
  QueryHistory h;
  guarded_unsat(h);
  assert(h.assume(1) == InputStatus::Ok);
  assert(h.begin_query() == InputStatus::Ok);
  assert(session.replay(h) == ReplayStatus::Ok);
  volatile sig_atomic_t stop = 1;
  const auto interrupted = policy.solve_query(session, &stop);
  assert(interrupted.status == QuerySolveStatus::Interrupted && !interrupted.result);
  assert(h.finish_query(1, QueryResult::Unknown) == InputStatus::Ok);
  assert(h.pending_assumptions().empty());
  assert(h.begin_query() == InputStatus::Ok);
  assert(session.replay(h) == ReplayStatus::Ok);
  const auto next = policy.solve_query(session);
  assert(next.status == QuerySolveStatus::Complete && next.result == 10);
}

static void contradictory_assumptions() {
  QueryHistory h;
  assert(h.assume(1) == InputStatus::Ok);
  assert(h.assume(-1) == InputStatus::Ok);
  assert(h.begin_query() == InputStatus::Ok);
  QueryKernel session;
  assert(session.replay(h) == ReplayStatus::Ok);
  DefaultPolicy policy;
  const auto solved = policy.solve_query(session);
  assert(solved.status == QuerySolveStatus::Complete && solved.result == 20);
  assert(!session.kernel(session.ticket())->inconsistent);
  assert(h.finish_query(1, QueryResult::Unsat) == InputStatus::Ok);
  assert(h.begin_query() == InputStatus::Ok);
  assert(session.replay(h) == ReplayStatus::Ok);
  const auto next = policy.solve_query(session);
  assert(next.status == QuerySolveStatus::Complete && next.result == 10);
}

static void callback_interrupts_search() {
  QueryHistory h;guarded_unsat(h);
  assert(h.assume(1)==InputStatus::Ok&&h.begin_query()==InputStatus::Ok);
  QueryKernel owner;assert(owner.replay(h)==ReplayStatus::Ok);
  struct State {Kernel *kernel;unsigned calls=0;bool nonroot=false;} state{owner.kernel(owner.ticket())};
  owner.set_terminate(&state,[](void *p) {
    auto &s=*static_cast<State *>(p);++s.calls;s.nonroot|=s.kernel->level>0;
    return s.kernel->stats.conflicts>0?1:0;
  });
  DefaultStrategyConfig config;
  config.inprocess.structured_root_stages.clear();
  config.inprocess.structured_root_bounded_backtracking_stages.clear();
  config.inprocess.structured_root_large_domain_stages.clear();
  config.inprocess.program.clear();config.inprocess.append_unlisted_rules=false;
  config.inprocess.initial_fast_elimination=false;config.rephase.late_lucky=false;
  DefaultPolicy policy(config);
  const auto answer=policy.solve_query(owner);
  assert(answer.status==QuerySolveStatus::Interrupted&&!answer.result);
  assert(state.calls>2&&state.nonroot&&state.kernel->stats.conflicts>0);
  assert(owner.finish_query(h,QueryResult::Unknown)==ResultCaptureStatus::Ok);
  assert(h.pending_assumptions().empty());
  const auto calls=state.calls;
  assert(policy.solve_query(owner).status==QuerySolveStatus::InvalidState);
  assert(state.calls==calls); // invalid entry does not invoke client code
  owner.set_terminate(nullptr,nullptr);owner.reset_termination();
  assert(h.begin_query()==InputStatus::Ok&&owner.replay(h)==ReplayStatus::Ok);
  const auto resumed=policy.solve_query(owner);
  assert(resumed.status==QuerySolveStatus::Complete&&resumed.result==10);
  assert(owner.check_model(owner.ticket()).status==ModelStatus::Valid);
}

static void disconnected_query_components() {
  QueryHistory h;
  clause(h, {8, -8});
  clause(h, {1, 2});
  clause(h, {-7, -6, -8});
  clause(h, {6, -8, -5, -6});
  clause(h, {-5});
  clause(h, {-4});
  clause(h, {5, 4, 4, -8});
  clause(h, {-4});
  clause(h, {4, 7, 6});
  assert(h.assume(-1) == InputStatus::Ok);
  assert(h.assume(9) == InputStatus::Ok);
  assert(h.begin_query() == InputStatus::Ok);
  QueryKernel session;
  assert(session.replay(h) == ReplayStatus::Ok);
  DefaultPolicy policy;
  const auto solved = policy.solve_query(session);
  assert(solved.status == QuerySolveStatus::Complete && solved.result == 10);
  assert(!session.projection_state(session.ticket()).restore);
  assert(session.check_model(session.ticket()).status == ModelStatus::Valid);
}

static void assumptions_preclude_amo_completion() {
  QueryHistory h;
  clause(h, {1, 2});
  assert(h.assume(1) == InputStatus::Ok);
  assert(h.assume(2) == InputStatus::Ok);
  assert(h.begin_query() == InputStatus::Ok);
  QueryKernel session;
  assert(session.replay(h) == ReplayStatus::Ok);
  DefaultPolicy policy;
  const auto solved = policy.solve_query(session);
  assert(solved.status == QuerySolveStatus::Complete && solved.result == 10);
  assert(session.check_model(session.ticket()).status == ModelStatus::Valid);
}

static void root_fixed_amo_pivot_query() {
  for (bool permanent_one : {false, true}) {
    QueryHistory h;
    clause(h, {6, -6});
    clause(h, {1, 2, 6});
    clause(h, {-4, 2});
    clause(h, {2});
    clause(h, {-5, 4, 3, -2});
    clause(h, {2});
    clause(h, {-4, -3, 6, -6});
    clause(h, {-5});
    if (permanent_one) clause(h, {1});
    else assert(h.assume(1) == InputStatus::Ok);
    assert(h.assume(-6) == InputStatus::Ok);
    assert(h.assume(-7) == InputStatus::Ok);
    assert(h.begin_query() == InputStatus::Ok);
    QueryKernel session;
    assert(session.replay(h) == ReplayStatus::Ok);
    DefaultPolicy policy;
    const auto solved = policy.solve_query(session);
    if (solved.status != QuerySolveStatus::Complete)
      std::fprintf(stderr, "root-fixed-pivot permanent=%d status=%u witnesses=%llu restrictions=%llu\n",
          permanent_one, unsigned(solved.status),
          (unsigned long long)session.projection_state(session.ticket()).reconstruction_literals,
          (unsigned long long)session.projection_state(session.ticket()).preserved_restrictions);
    assert(solved.status == QuerySolveStatus::Complete && solved.result == 10);
    const Kernel &k = *session.kernel(session.ticket());
    assert(k.model_values[internalize(-6)] > 0);
    assert(k.model_values[internalize(-7)] > 0);
  }
}

static void fixture(const std::string &prefix, const std::string &mode) {
  QueryHistory h;
  guarded_unsat(h);
  assert(h.assume(1) == InputStatus::Ok);
  assert(h.begin_query() == InputStatus::Ok);
  query_test::export_query(h, prefix);
  auto writer = query_test::writer(prefix, mode);
  QueryKernel session;
  assert(session.replay(h, std::move(writer)) == ReplayStatus::Ok);
  unsigned polls=0;
  session.set_terminate(&polls,[](void *p) {++*static_cast<unsigned *>(p);return 0;});
  DefaultPolicy policy;
  const auto solved = policy.solve_query(session);
  if (solved.status != QuerySolveStatus::Complete || solved.result != 20)
    std::fprintf(stderr, "fixture status=%u result=%d next=%u restore=%d witnesses=%llu aliases=%llu\n",
        unsigned(solved.status), solved.result,
        unsigned(session.next_assumption(session.ticket()).status),
        session.projection_state(session.ticket()).restore,
        (unsigned long long)session.projection_state(session.ticket()).reconstruction_literals,
        (unsigned long long)session.projection_state(session.ticket()).translated_assumptions);
  assert(solved.status == QuerySolveStatus::Complete && solved.result == 20);
  Kernel &k = *session.kernel(session.ticket());
  assert(!k.inconsistent);
  assert(session.flush_proof(session.ticket()));
  assert(polls>0);
  std::printf("conflicts=%llu decisions=%llu roots=%llu bve=%llu aliases=%llu polls=%u\n",
      (unsigned long long)k.stats.conflicts, (unsigned long long)k.stats.decisions,
      (unsigned long long)k.stats.root_assignments, (unsigned long long)k.stats.bve_eliminated,
      (unsigned long long)session.projection_state(session.ticket()).translated_assumptions,polls);
}

static void orbitopal_prefix_preserves_assumptions() {
  QueryHistory h;
  // Retained seed 50128, fresh query 12: refinement-derived prefixes used to
  // undo the cube and cause an endless adapter repair/replay/startup cycle.
  for (const int l : {2,7,0,3,17,41,-41,0,-7,-1,-7,3,0,-7,41,0,7,-17,0,
                     -41,-3,-41,0,-17,17,0,17,3,0})
    assert(h.add(l) == InputStatus::Ok);
  for (const int l : {-41,-7,3,-7}) assert(h.assume(l) == InputStatus::Ok);
  assert(h.begin_query() == InputStatus::Ok);
  QueryKernel session;
  assert(session.replay(h) == ReplayStatus::Ok);
  DefaultPolicy policy;
  const auto solved = policy.solve_query(session);
  assert(solved.status == QuerySolveStatus::Complete && solved.result == 10);
  assert(!session.projection_state(session.ticket()).restore);
  assert(session.check_model(session.ticket()).status == ModelStatus::Valid);
}

int main(int argc, char **argv) {
  if (argc == 3) { fixture(argv[1], argv[2]); return 0; }
  assert(argc == 1);
  repeated_default_queries();
  interrupted_and_invalid_entry();
  contradictory_assumptions();
  callback_interrupts_search();
  disconnected_query_components();
  assumptions_preclude_amo_completion();
  root_fixed_amo_pivot_query();
  orbitopal_prefix_preserves_assumptions();
}
