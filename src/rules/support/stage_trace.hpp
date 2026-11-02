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

// The shared fields of a rule's allocation-boundary memory line: the stage,
// process memory, the formula and watch arenas, then the caller's first three
// values. The caller finishes the line.
inline void memory_stage_fields(FILE *out, const Kernel &k, const char *stage,
                                size_t first, size_t second, size_t third) {
  const long peak_rss_kib = diagnostics::peak_resident_kib();
  const unsigned long virtual_kib = diagnostics::virtual_memory_kib();
  std::fprintf(out,
               "%s\trss_kib=%ld\tvm_kib=%lu\tvars=%u\tarena_words=%zu\t"
               "watch_words=%zu\twatch_capacity=%zu\tfirst=%zu\tsecond=%zu\t"
               "third=%zu",
               stage, peak_rss_kib, virtual_kib, k.nvars, k.arena.size(),
               k.watches.arena.size(), k.watches.arena.capacity(), first,
               second, third);
}

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
