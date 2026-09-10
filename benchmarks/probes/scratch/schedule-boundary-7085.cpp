// Posed-state diagnostic against the actual immutable 7030 policy objects.
// No CNF solving, and no copied invitation implementation.
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include <cstdio>
using namespace tactix;
int main() {
  for (unsigned conflicts : {99u, 100u, 101u})
    for (unsigned same_reduce : {0u, 1u})
      for (unsigned material : {0u, 1u})
        for (unsigned reduced : {0u, 1u}) {
          Kernel k;
          DefaultPolicy p;
          auto &s = p.inprocess();
          s.pose_initialized();
          s.probe_clock.rounds = s.elim_clock.rounds = 1;
          s.probe_clock.next_conflicts = s.elim_clock.next_conflicts = 100;
          s.probe_clock.reductions_mark = 0;
          s.elim_clock.marks_mark = k.stats.bve_marks;
          s.elim_clock.fixed_mark = kernel_root_fixed(k);
          s.elim_clock.material_mark = k.subsume_material_count;
          k.stats.conflicts = conflicts;
          k.stats.reductions = reduced;
          k.stats.bve_marks += material;
          // No candidate field exists for 'same_reduce'. Its missing state is
          // precisely the difference under test; do not fake one in Kernel.
          std::printf("%u\t%u\t%u\t%u\t%d\t%d\n", conflicts,
                      same_reduce, material, reduced,
                      s.want_phase(k, SlotGroup::Probe),
                      s.want_phase(k, SlotGroup::Eliminate));
        }
}
