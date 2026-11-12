#include "unit_test.hpp"
#include "frontend/parse.hpp"
#include "kernel/kernel.hpp"
#include "rules/bce/bce.hpp"
#include "session/weakening_history.hpp"

extern "C" void tactix_test_fail_allocations(bool);

using namespace tactix::session;

int main() {
  Kernel k;
  WeakeningHistory history(k, {4, 32, 4});
  ScopedPopulationObserverBinding binding(k, history);
  const Lit literals[2] = {internalize(1), internalize(2)};
  std::vector<Lit> input(literals, literals+2);
  ingest_clause(k, input);
  finish_input(k);
  const uint64_t id = k.next_clause_id;
  const uint64_t epoch = k.strategy_epoch;
  assert(history.capture_complete());
  tactix_test_fail_allocations(true);
  const auto token = kernel_prepare_weakening(k, WeakeningFamily::Bce,
      kernel_population_clause(k, literals, 2), kernel_population_clause(k, literals, 1),
      0, true, true);
  tactix_test_fail_allocations(false);
  kernel_finish_weakening(k, token, false);
  assert(!history.capture_complete() && history.records().empty());
  assert(kernel_binary_live(k, literals[0], literals[1]) && k.reconstruction.empty());
  assert(k.next_clause_id == id && k.strategy_epoch == epoch && !k.inconsistent);
  // Optional capture exhaustion cannot remove the legal rule or mutate its
  // pre-state; the ordinary rule can still perform the selected deletion.
  assert(bce_try(k, literals[0]) == ApplyResult::Ok && k.stats.bce_deleted == 1);
  assert(!history.capture_complete() && history.records().empty());
}
