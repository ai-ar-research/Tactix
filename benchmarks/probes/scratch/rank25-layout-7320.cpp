#include "kernel/kernel.hpp"
#include <cstddef>
#include <cstdio>
#include <cstring>

int main() {
  using namespace tactix;
  OccIndex index;
  index.track_order = true;
  index.ordered.begin_exact_layout(2);
  index.ordered.count(0);
  index.ordered.finish_exact_layout();
  index.ordered.push(0, OccIndex::Entry::from_binary(1));
  // The Release realloc call saves its owner pointer at owner+0x70. Verify
  // the real compiled layout, not an assumed std::vector/FlatLists layout.
  const void *at112 = nullptr;
  static_assert(sizeof(index) >= 112 + sizeof(at112));
  std::memcpy(&at112, reinterpret_cast<const char *>(&index) + 112, sizeof(at112));
  if (at112 != index.ordered.arena_data()) return 1;
  if (sizeof(WatchHead) != 16 || sizeof(OccIndex::Entry) != 4) return 2;
  std::printf("kernel_bytes=%zu\nassigned_bytes=%zu\nwatch_head_bytes=%zu\n"
              "occ_index_bytes=%zu\nordered_offset=%zu\nordered_arena_pointer_offset=112\n",
              sizeof(Kernel), sizeof(Assigned), sizeof(WatchHead),
              sizeof(OccIndex), offsetof(OccIndex, ordered));
  // This is only a subset of root-retaining hot storage, not a peak estimate.
  std::printf("selected_bytes_per_internal_var=%zu\n",
              2 * sizeof(int8_t) + sizeof(Assigned) + 2 * sizeof(WatchHead) +
              sizeof(Lit) + sizeof(uint64_t) + 3 * sizeof(uint64_t));
}
