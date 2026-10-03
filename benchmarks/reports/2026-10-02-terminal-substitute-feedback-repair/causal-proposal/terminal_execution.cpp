#undef NDEBUG
#include "strategy/substitute_execution.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
using namespace tactix;
using namespace tactix::execution;

static void fixture(Kernel &k) {
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(1));
  const Lit a[] = {internalize(1), internalize(2), internalize(3)};
  const Lit b[] = {internalize(-1), internalize(-2), internalize(3)};
  kernel_add_large_clause(k, a, 3, false, 0);
  kernel_add_large_clause(k, b, 3, false, 0);
  kernel_assign_unit(k, internalize(-3));
}

int main(int argc, char **argv) {
  assert(argc == 3);
  const bool batch = std::atoi(argv[1]) != 0;
  const bool borrowed = std::atoi(argv[2]) != 0;
  Kernel k;
  fixture(k);
  if (borrowed) kernel_drop_large_watches(k);
  ApplyResult result;
  bool reconnected = false;
  unsigned published = 0, advised = 0;
  if (batch) {
    const auto out = execute_action(k, SubstituteBatchRequest{2}, [] { return 0; },
      [&](size_t) { ++published; assert(!k.large_watches_connected); },
      [&](int, ApplyResult r) { ++advised; assert(r == ApplyResult::Unsat); return true; });
    result = out.result;
    reconnected = out.reconnected;
    assert(out.rounds == 1 && published == 1 && advised == 1);
  } else {
    result = execute_action(k, SubstituteRoundRequest{});
  }
  std::printf("TERMINAL batch=%d borrowed=%d result=%d connected=%d reconnected=%d\n",
              batch, borrowed, int(result), k.large_watches_connected, reconnected);
  if (result != ApplyResult::Unsat || !k.inconsistent) return 4;
  if (k.large_watches_connected != !borrowed) return 3;
  if (batch && reconnected != !borrowed) return 5;
  return 0;
}
