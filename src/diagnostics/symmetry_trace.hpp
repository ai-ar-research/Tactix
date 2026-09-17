#pragma once

#include <chrono>
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <utility>

namespace tactix {

// Optional diagnostics, never consulted by discovery or admission. Point IDs
// are local literal indices, not permanent external IDs. The runner can own
// stderr through an anonymous descriptor. Disabled tracing reads no clocks.
class SymmetryTrace {
  bool enabled_ = false;
  std::chrono::steady_clock::time_point started_{};

public:
  SymmetryTrace() noexcept {
    const char *setting = std::getenv("TACTIX_SR_TRACE");
    enabled_ = setting && setting[0] == '1' && setting[1] == '\0';
    if (enabled_)
      started_ = std::chrono::steady_clock::now();
  }

  bool enabled() const noexcept { return enabled_; }

  void permutation(const char *name, size_t ordinal, const uint32_t *image,
                   size_t domain) const noexcept {
    if (!enabled_)
      return;
    uint64_t fingerprint = 14695981039346656037ull;
    size_t support = 0;
    for (size_t point = 0; point < domain; ++point) {
      const uint32_t target = image[point];
      support += target != point;
      for (unsigned byte = 0; byte < 4; ++byte) {
        fingerprint ^= (target >> (byte * 8)) & 255u;
        fingerprint *= 1099511628211ull;
      }
    }
    // This preview bounds a diagnostic record, never discovery or admission.
    char preview[768] = {};
    size_t used = 0, shown = 0;
    for (size_t point = 0; point < domain && shown < 16; ++point) {
      if (image[point] == point)
        continue;
      const int written = std::snprintf(preview + used, sizeof preview - used,
                                        "%s%zu>%u", shown ? "," : "", point,
                                        image[point]);
      if (written < 0 || static_cast<size_t>(written) >= sizeof preview - used)
        break;
      used += static_cast<size_t>(written);
      ++shown;
    }
    event(name, "index\t%zu\tdomain\t%zu\tsupport\t%zu\tfnv1a64_le_u32\t%llu"
                "\tmoved_prefix16\t%s\tmoved_prefix_complete\t%u",
          ordinal, domain, support,
          static_cast<unsigned long long>(fingerprint), preview,
          static_cast<unsigned>(shown == support));
  }

  template <class Fixed>
  void permutation_support(const char *producer, const uint32_t *images,
                           size_t count, size_t domain, Fixed &&fixed) const {
    if (!enabled_)
      return;
    size_t fixed_only = 0, open = 0, identity = 0;
    for (size_t row = 0; row < count; ++row) {
      bool moved_fixed = false, moved_open = false;
      for (size_t point = 0; point < domain; ++point)
        if (images[row * domain + point] != point) {
          if (fixed(point))
            moved_fixed = true;
          else
            moved_open = true;
        }
      open += moved_open;
      fixed_only += moved_fixed && !moved_open;
      identity += !moved_fixed && !moved_open;
    }
    event("permutation_support",
          "producer\t%s\tmaps\t%zu\tfixed_only\t%zu\topen\t%zu\tidentity\t%zu",
          producer, count, fixed_only, open, identity);
  }

  void event(const char *name, const char *format, ...) const noexcept {
    if (!enabled_)
      return;
    char fields[1024]; // Diagnostic record bound, not a solver work limit.
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(fields, sizeof fields, format, arguments);
    va_end(arguments);
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - started_).count();
    std::fprintf(stderr, "c TACTIX_SR_TRACE\t%lld\t%s\t%s\n",
                 static_cast<long long>(micros), name, fields);
  }

  // Balanced on ordinary return, interruption returns, and C++ exceptions.
  // A hard-killed phase intentionally leaves its last begin unmatched.
  template <class Function>
  decltype(auto) measure(const char *name, Function &&function) const {
    struct End {
      const SymmetryTrace &trace;
      const char *name;
      ~End() { trace.event(name, "boundary\tend"); }
    } end{*this, name};
    event(name, "boundary\tbegin");
    return std::forward<Function>(function)();
  }
};

} // namespace tactix
