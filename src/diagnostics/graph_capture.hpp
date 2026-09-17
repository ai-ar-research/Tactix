#pragma once

#include <atomic>
#include <climits>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>

namespace tactix::diagnostics {

// Opt-in diagnostic stream owned by the caller. No pathname is opened and no
// solver setting/state is changed. The record count is a capture bound, not a
// discovery limit. A killed/incomplete record has no terminating 'z' line.
template <class Color, class Edges>
void capture_graph(uint64_t vertices, uint64_t edges, Color color,
                   Edges visit_edges) noexcept {
  const char *fd_text = std::getenv("TACTIX_GRAPH_CAPTURE_FD");
  const char *limit_text = std::getenv("TACTIX_GRAPH_CAPTURE_COUNT");
  if (!fd_text || !limit_text || *fd_text < '0' || *fd_text > '9' ||
      *limit_text < '0' || *limit_text > '9')
    return;
  char *end = nullptr;
  errno = 0;
  const auto descriptor = std::strtoull(fd_text, &end, 10);
  if (errno || *end || descriptor > INT_MAX) return;
  errno = 0;
  const auto limit = std::strtoull(limit_text, &end, 10);
  if (errno || *end || !limit) return;
  struct stat st {};
  if (fstat(static_cast<int>(descriptor), &st) || !S_ISREG(st.st_mode))
    return;
  static std::atomic<uint64_t> records{0};
  if (records.fetch_add(1, std::memory_order_relaxed) >= limit) return;
  const int copied = dup(static_cast<int>(descriptor));
  if (copied < 0) return;
  FILE *stream = fdopen(copied, "a");
  if (!stream) { close(copied); return; }
  bool good = std::fprintf(stream, "g %llu %llu\n",
                           static_cast<unsigned long long>(vertices),
                           static_cast<unsigned long long>(edges)) >= 0;
  for (uint64_t vertex = 0; vertex < vertices && good; ++vertex)
    good = std::fprintf(stream, "v %llu %llu\n",
                        static_cast<unsigned long long>(vertex),
                        static_cast<unsigned long long>(color(vertex))) >= 0;
  visit_edges([&](uint64_t first, uint64_t second) {
    if (good)
      good = std::fprintf(stream, "e %llu %llu\n",
                          static_cast<unsigned long long>(first),
                          static_cast<unsigned long long>(second)) >= 0;
  });
  if (good) std::fputs("z\n", stream);
  std::fclose(stream);
}

} // namespace tactix::diagnostics
