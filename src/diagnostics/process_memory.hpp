#pragma once

#include <cstdio>
#include <cstdlib>

#include <sys/resource.h>
#include <unistd.h>

namespace tactix::diagnostics {

class EnvironmentLineFile {
  bool initialized_ = false;
  FILE *out_ = nullptr;

public:
  // A stream that opens successfully starts with `header`, if one is given.
  FILE *stream(const char *environment, const char *header = nullptr) {
    if (!initialized_) {
      initialized_ = true;
      const char *path = std::getenv(environment);
      if (path && *path) {
        out_ = std::fopen(path, "w");
        if (out_) {
          std::setvbuf(out_, nullptr, _IOLBF, 0);
          if (header)
            std::fputs(header, out_);
        }
      }
    }
    return out_;
  }
};

inline long peak_resident_kib() {
  struct rusage usage {};
  getrusage(RUSAGE_SELF, &usage);
  return usage.ru_maxrss;
}

inline unsigned long virtual_memory_kib() {
  unsigned long pages = 0;
  if (FILE *statm = std::fopen("/proc/self/statm", "r")) {
    (void)std::fscanf(statm, "%lu", &pages);
    std::fclose(statm);
  }
  return pages * static_cast<unsigned long>(sysconf(_SC_PAGESIZE) / 1024);
}

} // namespace tactix::diagnostics
