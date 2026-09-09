#undef NDEBUG
#include "frontend/parse.hpp"
#include "policies/heuristics.hpp"
#include <cassert>
#include <cstdio>
#include <memory>
using namespace tactix;
int main() {
  for (unsigned reverse : {0u, 1u})
    for (unsigned declared : {6u, 9u})
      for (unsigned attached : {0u, 1u}) {
        Kernel k;
        std::unique_ptr<ScopedHeuristics> owner;
        if (attached) owner = std::make_unique<ScopedHeuristics>(k);
        FILE *in = std::tmpfile(); assert(in);
        std::fprintf(in, "p cnf %u 3\n", declared);
        for (unsigned i = 0; i < 6; ++i)
          std::fprintf(in, "%u ", reverse ? 6-i : i+1);
        std::fputs("0\n2 0\n-4 0\n", in); std::rewind(in);
        assert(!parse_dimacs(in, k)); std::fclose(in);
        if (!attached) owner = std::make_unique<ScopedHeuristics>(k);
        heuristics_deactivate_inactive_vars(k);
        for (unsigned stage : {0u, 1u}) {
          if (stage) {
            assert(kernel_compact_variables(k));
            heuristics_compact_random_order_fixed(k);
          }
          const unsigned ext = reverse ? 4 : 2;
          const Var v = var_of(kernel_internalize(k, ext));
          assert(k.values[make_lit(v, false)] == (reverse ? -1 : 1));
          assert(!heuristics_var_active(k, v));
          kernel_check_invariants(k);
          std::printf("tactix\t%u\t%u\t%u\t%u\t%u\t%u\t%u\n",
              reverse, declared, attached, stage, ext, owner->h.links[v].stamp, owner->h.queue_stamp);
        }
      }
}
