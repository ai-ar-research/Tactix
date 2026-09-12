#undef NDEBUG
#include "kernel/kernel.hpp"
#include "strategy/forward_parameters.hpp"

#include <cassert>
#include <cstdio>

using namespace tactix;

int main() {
  Kernel k;
  const strategy::SubsumeForwardParameters original_c1 =
      k.subsume_forward_parameters;
  const strategy::StrengthenParameters original_c2 = k.strengthen_parameters;

  strategy::SubsumeForwardParameters c1{7, 11, 13};
  strategy::StrengthenParameters c2{17, 19};
  {
    strategy::ScopedSubsumeForwardParameters pose_c1(k, c1);
    assert(k.subsume_forward_parameters.clause_size_limit == 7);
    assert(k.subsume_forward_parameters.large_occurrence_limit == 11);
    assert(k.subsume_forward_parameters.binary_occurrence_limit == 13);
    {
      strategy::ScopedStrengthenParameters pose_c2(k, c2);
      assert(k.strengthen_parameters.clause_size_limit == 17);
      assert(k.strengthen_parameters.occurrence_limit == 19);
    }
    assert(k.strengthen_parameters.clause_size_limit ==
           original_c2.clause_size_limit);
  }
  assert(k.subsume_forward_parameters.clause_size_limit ==
         original_c1.clause_size_limit);
  assert(k.subsume_forward_parameters.large_occurrence_limit ==
         original_c1.large_occurrence_limit);
  assert(k.subsume_forward_parameters.binary_occurrence_limit ==
         original_c1.binary_occurrence_limit);

  std::puts("Scoped per-occurrence C1/C2 parameter restoration PASS");
}
