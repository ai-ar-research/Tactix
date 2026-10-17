// Progress lines for `tactix -v` (setting TACTIX_VERBOSE).
//
// The default strategy reports at restarts and reductions, at most about once
// per second, plus a first and a last line. Reporting only reads counters and
// the clock, so it cannot change what the solver does.
#pragma once

#include <cstdint>

namespace tactix {

struct Kernel;

class ProgressReport {
public:
  // Reads TACTIX_VERBOSE; prints the column header when it is set.
  void start(const Kernel &k);
  // `event`: 'r' restart, 'R' reduction. Prints if a second has passed.
  void event(const Kernel &k, char event) {
    if (verbosity_) maybe_print(k, event, false);
  }
  void finish(const Kernel &k, int result);

private:
  void maybe_print(const Kernel &k, char event, bool force);
  int verbosity_ = 0;
  uint64_t start_nanos_ = 0;
  uint64_t last_nanos_ = 0;
};

} // namespace tactix
