// Disabled-by-default semantic event tracing for DefaultPolicy diagnostics.
// The TSV format uses permanent external variable identities so traces remain
// meaningful across variable compaction and can be normalized against a
// reference solver.  No file is opened and no state is scanned unless
// TACTIX_EVENT_TRACE names an output path.
#pragma once

#include "kernel/kernel.hpp"

#include <cstdint>
#include <cstdio>

namespace tactix {

class EventTrace {
public:
  EventTrace() = default;
  ~EventTrace();

  bool open_from_env(const Kernel &);
  bool enabled() const { return out_ != nullptr; }
  void close();

  // Macro records are always retained until the global record cap is hit.
  void macro(const Kernel &, const char *event, const char *payload = "-");
  // Detailed records cover the deterministic prefix and 1024 records after
  // the first Reorder.  Later search is represented by conflict checkpoints.
  void detail(const Kernel &, const char *event, int64_t payload = 0);
  void checkpoint(const Kernel &);
  void reorder_begin(const Kernel &);
  void reorder_end(const Kernel &);

private:
  void emit(const Kernel &, const char *, const char *, bool snapshot);

  std::FILE *out_ = nullptr;
  uint64_t sequence_ = 0;
  uint64_t emitted_ = 0;
  uint64_t limit_ = 50000;
  uint64_t post_reorder_details_ = 0;
  uint64_t checkpoint_bucket_ = 0;
  bool saw_reorder_ = false;
  bool truncated_ = false;
};

} // namespace tactix
