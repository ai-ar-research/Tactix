#define NDEBUG 1
#include "internal.hpp"
#include "testing.hpp"
#undef NDEBUG
#include <cassert>
#include <cstdio>
int main() {
  for(unsigned copies:{1u,2u,5u}) {
    CaDiCaL::Solver solver; solver.set("quiet",1); solver.declare_more_variables(4);
    for(int x:{-1,2,0,1,-2,0})solver.add(x);
    for(unsigned i=0;i<copies;++i)for(int x:{1,2,3,0})solver.add(x);
    auto *internal=CaDiCaL::Testing(solver).internal();
    for(int i=1;i<=internal->max_var;++i)internal->flags(i).elim=false;
    internal->decompose_round();
    assert(!internal->unsat && internal->flags(3).elim && !internal->flags(4).elim);
  }
  puts("PASS: actual CaDiCaL binary collapse retries retained endpoint in3 cases");
}
