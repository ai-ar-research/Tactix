#pragma once

// Diagnostics-only rule trace files, compiled only into semantic-trace builds
// (production binaries carry none of these symbols).
#ifdef TACTIX_ENABLE_EVENT_TRACE

#include "diagnostics/process_memory.hpp"
#include "kernel/kernel.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace tactix {

// One complete allocation-boundary memory line: the stage, process memory, the
// formula and watch arenas, then the caller's values (`fourth` only when given).
// The line is written by a single fprintf so concurrent solver threads sharing
// the process-wide trace stream cannot interleave inside it.
#define TACTIX_MEMORY_STAGE_FIELDS                                             \
  "%s\trss_kib=%ld\tvm_kib=%lu\tvars=%u\tarena_words=%zu\t"                    \
  "watch_words=%zu\twatch_capacity=%zu\tfirst=%zu\tsecond=%zu\tthird=%zu"
inline void memory_stage_line(FILE *out, const Kernel &k, const char *stage,
                              size_t first, size_t second, size_t third,
                              const size_t *fourth = nullptr) {
  const long peak_rss_kib = diagnostics::peak_resident_kib();
  const unsigned long virtual_kib = diagnostics::virtual_memory_kib();
  if (fourth)
    std::fprintf(out, TACTIX_MEMORY_STAGE_FIELDS "\tfourth=%zu\n", stage,
                 peak_rss_kib, virtual_kib, k.nvars, k.arena.size(),
                 k.watches.arena.size(), k.watches.arena.capacity(), first,
                 second, third, *fourth);
  else
    std::fprintf(out, TACTIX_MEMORY_STAGE_FIELDS "\n", stage, peak_rss_kib,
                 virtual_kib, k.nvars, k.arena.size(), k.watches.arena.size(),
                 k.watches.arena.capacity(), first, second, third);
}
#undef TACTIX_MEMORY_STAGE_FIELDS

// A ledger written only at one selected conflict: opened, line buffered and
// headed by `header` when both `path_environment` and `conflict_environment`
// are set; null otherwise. `selected` receives the parsed conflict.
inline FILE *open_conflict_selected_trace(const char *path_environment,
                                          const char *conflict_environment,
                                          const char *header,
                                          uint64_t &selected) {
  const char *path = std::getenv(path_environment);
  const char *conflict = std::getenv(conflict_environment);
  if (!path || !*path || !conflict || !*conflict)
    return nullptr;
  selected = std::strtoull(conflict, nullptr, 10);
  FILE *out = std::fopen(path, "w");
  if (out) {
    std::setvbuf(out, nullptr, _IOLBF, 0);
    std::fputs(header, out);
  }
  return out;
}

} // namespace tactix

#endif
