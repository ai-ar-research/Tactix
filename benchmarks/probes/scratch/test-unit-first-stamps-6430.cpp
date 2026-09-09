#undef NDEBUG
#include "frontend/parse.hpp"
#include "policies/heuristics.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>
using namespace tactix;
int main(int argc, char **argv) {
  const bool old = argc == 2 && !std::strcmp(argv[1], "--expect-old-missing");
  assert(argc == 1 || old);
  unsigned observations = 0, missing = 0;
  for (bool reverse : {false, true})
    for (unsigned declared : {6u, 9u})
      for (bool attached : {false, true}) {
        Kernel k; std::unique_ptr<ScopedHeuristics> owner;
        if (attached) owner = std::make_unique<ScopedHeuristics>(k);
        FILE *in = std::tmpfile(); assert(in);
        std::fprintf(in, "p cnf %u 3\n-1 0\n-4 0\n", declared);
        for (unsigned i = 0; i != 6; ++i) std::fprintf(in, "%u ", reverse ? 6-i : i+1);
        std::fputs("0\n", in); std::rewind(in);
        assert(!parse_dimacs(in, k)); std::fclose(in);
        if (!attached) owner = std::make_unique<ScopedHeuristics>(k);
        heuristics_deactivate_inactive_vars(k);
        for (unsigned stage = 0; stage != 2; ++stage) {
          if (stage) { assert(kernel_compact_variables(k)); heuristics_compact_random_order_fixed(k); }
          const Var first = var_of(kernel_internalize(k, 1));
          assert(k.value(kernel_internalize(k, -1)) == 1 && k.value(kernel_internalize(k, -4)) == 1);
          assert(!heuristics_var_active(k, first));
          ++observations; missing += owner->h.links[first].stamp != 1;
          kernel_check_invariants(k);
        }
      }
  std::printf("unit-first observations=%u missing=%u\n", observations, missing);
  assert(observations == 16);
  return old ? missing != observations : missing != 0;
}
