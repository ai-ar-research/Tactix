#undef NDEBUG
#include "kernel/kernel.hpp"
#include "strategy/failed_literal_parameters.hpp"
#include <cassert>
#include <cstdio>

using namespace tactix;
using namespace tactix::strategy;

int main() {
  Kernel kernel;
  assert(kernel.backbone_parameters.initial_rounds ==
         BackboneParameters::INITIAL_ROUNDS_DEFAULT);
  {
    ScopedBackboneParameters scoped(kernel, {7, 11});
    assert(kernel.backbone_parameters.initial_rounds == 7);
    assert(kernel.backbone_parameters.maximum_rounds == 11);
  }
  assert(kernel.backbone_parameters.initial_rounds ==
         BackboneParameters::INITIAL_ROUNDS_DEFAULT);

  std::puts("Backbone strategy parameters PASS");
}
