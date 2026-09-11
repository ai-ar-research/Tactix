#include "kernel/kernel.hpp"
#include <cstddef>
#include <cstdio>
#include <cstring>

using namespace tactix;
int main(int argc, char **) {
  if (argc > 1) {
    Kernel k;
    kernel_reserve_vars(k, 8);
    k.var_status.assign(8, VarStatus::Active);
    k.values[0] = 1; k.values[1] = -1;
    k.values[2] = -1; k.values[3] = 1;
    k.values[4] = 1; k.values[5] = -1;
    k.assigned[0].level = k.assigned[1].level = 0;
    k.assigned[2].level = 3;
    k.var_status[3] = VarStatus::Eliminated;
    k.stats.conflicts = 17; k.stats.decisions = 29;
    k.reconstruction.assign(16, 0);
    const Lit rest[] = {2, 4, 6};
    kernel_push_witness(k, 0, rest, 3);
    if (k.reconstruction.size() != 21 || k.reconstruction[16] != 2 ||
        k.reconstruction[17] != 4 || k.reconstruction[18] != 6 ||
        k.reconstruction[19] != 0 || k.reconstruction[20] != 4) return 1;
    std::puts("fixed witness fixture PASS");
    return 0;
  }
  static_assert(sizeof(void *) == 8 && sizeof(Assigned) == 16);
  static_assert(sizeof(std::vector<Lit>) == 24);
  std::printf("{\"kernel_bytes\":%zu,\"assigned_bytes\":%zu,\"level_offset\":%zu,",
              sizeof(Kernel), sizeof(Assigned), offsetof(Assigned, level));
  std::printf("\"nvars_offset\":%zu,\"conflicts_offset\":%zu,\"decisions_offset\":%zu,",
              offsetof(Kernel, nvars), offsetof(Kernel, stats)+offsetof(Stats, conflicts),
              offsetof(Kernel, stats)+offsetof(Stats, decisions));
  std::printf("\"vectors\":{");
#define VECTOR(member, bytes) std::printf("\"" #member "\":[%zu,%zu],", offsetof(Kernel, member), size_t(bytes))
  VECTOR(values, 1); VECTOR(assigned, sizeof(Assigned)); VECTOR(jumped_over, 4);
  VECTOR(unit_ids, 8); VECTOR(touched, 8); VECTOR(removed, 8); VECTOR(touched_kept, 8);
  VECTOR(var_status, 1); VECTOR(reconstruction, 4);
#undef VECTOR
  std::printf("\"watch_heads\":[%zu,%zu]}}\n",
              offsetof(Kernel, watches)+offsetof(WatchLists, heads), sizeof(WatchHead));
}
