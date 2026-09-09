// The diagnostic must emit capacity transitions, not every candidate callback.
#undef NDEBUG
#include "diagnostics/event_trace.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>
using namespace tactix;
int main() {
#ifdef TACTIX_ENABLE_EVENT_TRACE
  char path[] = "bve-memory-test-XXXXXX";
  const int fd = mkstemp(path);
  assert(fd >= 0); close(fd);
  assert(setenv("TACTIX_SLOT_MEMORY_TRACE", path, 1) == 0);
  Kernel k;
  kernel_reserve_vars(k, 3);
  event_trace_bve_candidate(k, 0, "begin", 0, 1, false);
  event_trace_bve_candidate(k, 0, "end", 0, 1, false); // no allocation
  k.reconstruction.reserve(k.reconstruction.capacity() + 1);
  event_trace_bve_candidate(k, 1, "begin", 0, 1, false);
  k.arena.reserve(k.arena.capacity() + 1);
  event_trace_bve_candidate(k, 1, "end", 0, 1, false);
  event_trace_bve_candidate(k, 1, "postflush", 0, 1, false); // unchanged
  std::ifstream input(path);
  std::vector<std::string> rows;
  std::string line;
  while (std::getline(input, line)) rows.push_back(line);
  assert(rows.size() == 3);
  assert(rows[0].find("BVECapacity\tbegin\t") == 0);
  assert(rows[1].find("BVECapacity\tbegin\t") == 0);
  assert(rows[2].find("BVECapacity\tend\t") == 0);
  assert(rows[0].find("memory_subject_internal=1") != std::string::npos);
  for (size_t i = 1; i < rows.size(); ++i)
    assert(rows[i].find("memory_subject_internal=2") != std::string::npos);
  for (const auto &row : rows) {
    assert(row.find("dense_large_arena_capacity_bytes=0") != std::string::npos);
    assert(row.find("dense_ordered_arena_capacity_bytes=0") != std::string::npos);
  }
  input.close(); unsetenv("TACTIX_SLOT_MEMORY_TRACE");
  assert(std::remove(path) == 0);
#endif
}
