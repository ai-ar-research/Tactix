// Shared round execution must respect a caller-owned disconnected watch
// session, and publish retirements before another round observes the state.
#undef NDEBUG
#include "policies/heuristics.hpp"
#include "strategy/substitute_execution.hpp"

#include <cassert>
#include <vector>

using namespace tactix;
using namespace tactix::execution;

static void exposed_equivalence(Kernel &k) {
  kernel_reserve_vars(k, 5);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(1));
  const Lit a[] = {internalize(1), internalize(-3), internalize(2)};
  const Lit b[] = {internalize(-1), internalize(3), internalize(-2)};
  kernel_add_large_clause(k, a, 3, false, 0);
  kernel_add_large_clause(k, b, 3, false, 0);
}

static void test_session(bool borrowed, uint32_t cap, bool continue_rounds) {
  Kernel k;
  exposed_equivalence(k);
  ScopedHeuristics h(k);
  if (borrowed)
    kernel_drop_large_watches(k);
  const uint64_t ticks = k.stats.substitute_ticks;
  std::vector<uint64_t> retired_at_snapshot;
  uint32_t published = 0, advised = 0;
  const auto out = execute_action(
      k, SubstituteBatchRequest{cap},
      [&] {
        assert(k.retired_events.empty());
        retired_at_snapshot.push_back(k.stats.substitute_vars);
        return kernel_active_variables(k);
      },
      [&](size_t trail_begin) {
        assert(!k.large_watches_connected);
        heuristics_consume_root_assignments(k, trail_begin);
        heuristics_consume_retirements(k);
        ++published;
      },
      [&](uint32_t before, ApplyResult current) {
        assert(published == advised + 1);
        assert(k.retired_events.empty());
        ++advised;
        return continue_rounds && current != ApplyResult::Unsat &&
               kernel_active_variables(k) < before;
      });
  const uint32_t rounds = cap ? (continue_rounds ? cap : 1) : 0;
  assert(out.rounds == rounds && published == rounds && advised == rounds);
  assert(out.result == (cap ? ApplyResult::Ok : ApplyResult::NoOp));
  assert(k.large_watches_connected == !borrowed);
  assert(out.reconnected == (!borrowed && cap));
  if (!cap) {
    assert(k.stats.substitute_ticks == ticks);
    assert(!kernel_var_retired(k, 1));
  } else {
    assert(kernel_var_retired(k, 1));
    assert(!heuristics_var_active(k, 1));
    assert(kernel_var_retired(k, 2) == (rounds == 2));
    if (rounds == 2)
      assert(retired_at_snapshot == std::vector<uint64_t>({0, 1}));
  }
  if (borrowed)
    assert(execute_action(k, SubstituteReconnectRequest{}) != ApplyResult::Unsat);
  kernel_check_invariants(k);
}

static void test_refutation_before_disconnection(bool borrowed) {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(1));
  kernel_add_binary_clause(k, internalize(-1), internalize(-2));
  const Lit a[] = {internalize(1), internalize(2), internalize(-3)};
  kernel_add_large_clause(k, a, 3, false, 0);
  kernel_assign_unit(k, internalize(3));
  if (borrowed)
    kernel_drop_large_watches(k);
  uint32_t published = 0, advised = 0;
  bool connected_at_publication = false;
  const auto out = execute_action(
      k, SubstituteBatchRequest{2}, [] { return 0; },
      [&](size_t) {
        ++published;
        connected_at_publication = k.large_watches_connected;
      },
      [&](int, ApplyResult current) {
        assert(published == 1 && current == ApplyResult::Unsat);
        ++advised;
        return true; // A terminal result overrides even permissive advice.
      });
  assert(out.result == ApplyResult::Unsat && k.inconsistent);
  assert(out.rounds == 1 && published == 1 && advised == 1);
  assert(!out.reconnected);
  assert(k.large_watches_connected == connected_at_publication);
}


// A rewrite can refute after dropping large watches: equivalence collapses
// the two large clauses to opposite units. Owned single-round and batch calls
// restore their watch session; an enclosing disconnected caller retains it.
static void test_rewrite_refutation_restores_owned_session(bool batch, bool borrowed) {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(1));
  const Lit a[] = {internalize(1), internalize(2), internalize(3)};
  const Lit b[] = {internalize(-1), internalize(-2), internalize(3)};
  kernel_add_large_clause(k, a, 3, false, 0);
  kernel_add_large_clause(k, b, 3, false, 0);
  kernel_assign_unit(k, internalize(-3));
  if (borrowed) kernel_drop_large_watches(k);
  if (batch) {
    unsigned published = 0, advised = 0;
    const auto out = execute_action(k, SubstituteBatchRequest{2}, [] { return 0; },
        [&](size_t) { ++published; assert(!k.large_watches_connected); },
        [&](int, ApplyResult result) {
          ++advised;
          assert(result == ApplyResult::Unsat);
          return true; // Terminal cleanup overrides permissive continuation.
        });
    assert(out.result == ApplyResult::Unsat && out.rounds == 1);
    assert(published == 1 && advised == 1);
    assert(out.reconnected == !borrowed);
  } else {
    assert(execute_action(k, SubstituteRoundRequest{}) == ApplyResult::Unsat);
  }
  assert(k.inconsistent && k.large_watches_connected == !borrowed);
}

int main() {
  for (bool borrowed : {false, true})
    for (uint32_t cap : {0u, 1u, 2u})
      for (bool more : {false, true})
        test_session(borrowed, cap, more);
  for (bool borrowed : {false, true})
    test_refutation_before_disconnection(borrowed);
  for (bool batch : {false, true})
    for (bool borrowed : {false, true})
      test_rewrite_refutation_restores_owned_session(batch, borrowed);
}
