#undef NDEBUG
#include "query_test_support.hpp"
#include "policies/default_inprocess.hpp"
#include "rules/substitute/substitute.hpp"
#include "strategy/phase_execution.hpp"

using namespace tactix;
using namespace tactix::session;

int main() {
  QueryHistory history;
  for (const auto &clause : {std::vector<int32_t>{1, 2}, {1, -2}, {-1, 2}, {-1, -2}}) {
    for (int32_t literal : clause) assert(history.add(literal) == InputStatus::Ok);
    assert(history.add(0) == InputStatus::Ok);
  }
  assert(history.assume(3) == InputStatus::Ok);
  assert(history.begin_query() == InputStatus::Ok);
  QueryKernel query;
  assert(query.replay(history) == ReplayStatus::Ok);
  const auto ticket = query.ticket();
  auto &kernel = *query.kernel(ticket);
  assert(query.next_assumption(ticket).status == AssumptionStatus::Decide);
  execution::InprocessPhase phase;
  DefaultFactorMatch selection;
  phase.enter(kernel, execution::InprocessPhaseEntryRequest{false, selection, 0});
  assert(query.next_assumption(ticket).status == AssumptionStatus::Busy);
  assert(substitute_apply(kernel) == ApplyResult::Unsat);
  assert(query.next_assumption(ticket).status == AssumptionStatus::BaseUnsat);
  assert(query.finish_query(history, QueryResult::Unsat) == ResultCaptureStatus::NotReady);
  assert(history.state() == InputState::Solving);
  auto *projection = kernel.query_projection;
  assert(projection);
  kernel.query_projection = nullptr;
  assert(query.next_assumption(ticket).status == AssumptionStatus::Restore);
  kernel.query_projection = projection;
  phase.exit_terminal(kernel);
  assert(query.next_assumption(ticket).status == AssumptionStatus::BaseUnsat);
  assert(query.finish_query(history, QueryResult::Unsat) == ResultCaptureStatus::Ok);
}
