#define NDEBUG 1
#include "internal.hpp"
#include "testing.hpp"
#undef NDEBUG
#include <cassert>
#include <cstdio>
int main() {
  for (bool instantiate : {false, true}) {
    CaDiCaL::Solver s; s.set("quiet", 1); s.declare_more_variables(4);
    for (int x : {1, 2, 3, 0}) s.add(x);
    if (instantiate) { for (int x : {-3, 4, 0, -3, -4, 0}) s.add(x); }
    else { for (int x : {1, -2, 0}) s.add(x); }
    auto *in = CaDiCaL::Testing(s).internal();
    auto *c = in->clauses[0]; assert(c->size == 3 && !c->garbage);
    for (int v = 1; v != 4; ++v) in->flags(v).elim = false;
    in->clause = {1, instantiate ? 2 : 3};
    int64_t ticks = 0; in->vivify_strengthen(c, ticks);
    assert(c->garbage && !in->unsat);
    for (int v = 1; v != 4; ++v) assert(in->flags(v).elim);
  }
  std::puts("PASS: actual CaDiCaL Vivify binary strengthening marks removed source (2 cases)");
}
