#include "policies/default_inprocess.hpp"
extern "C" int foundation_phase(tactix::DefaultInprocessScheduler *, tactix::Kernel &,
    tactix::SlotGroup, volatile sig_atomic_t *)
    asm("_ZN6tactix25DefaultInprocessScheduler9run_phaseERNS_6KernelENS_9SlotGroupEPVi");
namespace tactix {
int DefaultInprocessScheduler::run_phase(Kernel &k, SlotGroup group,
    volatile sig_atomic_t *stop, execution::PolicyCompletion *) {
  return foundation_phase(this, k, group, stop);
}
}
