#include "unit_test.hpp"
#include "query_test_support.hpp"
#include "policies/default_inprocess.hpp"
#include "rules/substitute/substitute.hpp"
#include "strategy/phase_execution.hpp"

using namespace tactix::session;

int main() {
  auto [history, query] = query_test::Query({{1, 2}, {1, -2}, {-1, 2}, {-1, -2}}, {3});
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
