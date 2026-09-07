// Same entailed star-equivalence fixture as the native Kissat replay.
#include "kernel/kernel.hpp"
#include "rules/substitute/substitute.hpp"
#include <cstdio>
#include <cstring>

using namespace tactix;

int main(int argc, char **argv) {
  const bool sequential = argc > 1 && !std::strcmp(argv[1], "sequential");
  Kernel k;
  kernel_reserve_vars(k, 4);
  const auto add = [&](int a, int b) {
    kernel_add_binary_clause(k, internalize(a), internalize(b));
  };
  add(1, -4); add(-1, 4);
  add(4, -2); add(-4, 2);
  add(4, -3); add(-4, 3);
  OccIndex occ;
  kernel_dense_connect(k, occ);
  SweepSubstitutionSession session;
  session.begin(k);
  const std::vector<std::pair<Lit, Lit>> pairs{
      {internalize(1), internalize(4)},
      {internalize(1), internalize(2)},
      {internalize(1), internalize(3)}};
  for (const auto &pair : pairs) {
    session.prime_equivalence(k, pair.first, pair.second);
    kernel_add_binary_clause(k, pair.first, negate(pair.second));
    kernel_add_binary_clause(k, negate(pair.first), pair.second);
  }
  if (sequential) {
    for (const auto &pair : pairs)
      (void)substitute_apply_sweep_equivalences(k, {pair}, session);
  } else {
    (void)substitute_apply_sweep_equivalences(k, pairs, session);
  }
  const unsigned a = kernel_binary_copies(k, internalize(1), internalize(-2));
  const unsigned b = kernel_binary_copies(k, internalize(1), internalize(-3));
  std::printf("%s copies: %u %u\n", sequential ? "sequential" : "bulk", a, b);
  const unsigned expected = sequential ? 2 : 1;
  const bool ok = a == expected && b == expected;
  kernel_dense_disconnect(k);
  return ok ? 0 : 1;
}
