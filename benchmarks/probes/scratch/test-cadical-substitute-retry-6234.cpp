#define NDEBUG 1
#include "internal.hpp"
#include "testing.hpp"
#undef NDEBUG
#include <cassert>
#include <cstdio>
int main() {
  for (unsigned copies : {1u, 2u, 3u}) {
    CaDiCaL::Solver solver;
    solver.set("quiet", 1);
    for (unsigned i = 0; i < copies; ++i) {
      for (int lit : {-1, 2, 0, 1, -2, 0}) solver.add(lit);
    }
    for (int lit : {2, 3, 0, 3, 4, 0}) solver.add(lit);
    auto *internal = CaDiCaL::Testing(solver).internal();
    for (int i = 1; i <= internal->max_var; ++i) internal->flags(i).elim = false;
    internal->decompose_round();
    assert(!internal->unsat);
    assert(internal->flags(1).elim && internal->flags(2).elim);
    // CaDiCaL garbage-marks replaced clauses too, unlike Kissat's surviving
    // binary rewrite. Do not force equality on the extra x3 removal mark.
    assert(!internal->flags(4).elim);
  }
  std::puts("PASS: actual CaDiCaL decomposition publishes disappeared binary removals (3 duplicate cases)");
}
