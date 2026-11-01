#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace tactix::semantic_extension {

inline void write_journal_or_abort(FILE *records, const std::string &bytes) {
  if (std::fwrite(bytes.data(), 1, bytes.size(), records) != bytes.size() ||
      std::fflush(records) || std::ferror(records))
    std::abort();
}

inline bool append_bounded(std::string &bytes, uint64_t maximum,
                           const std::string &text) {
  if (bytes.size() > maximum || text.size() > maximum - bytes.size())
    return false;
  bytes += text;
  return true;
}

// Bounded stable-text construction shared by semantic publishers. Work is
// charged before capacity admission because failed rendering attempts are
// observable in publishers that report journal work.
template <bool TrackWork = false, bool SaturateWorkOverflow = false>
struct BoundedJournal {
  uint64_t maximum = 0;
  uint64_t work = 0;
  std::string bytes;

  explicit BoundedJournal(uint64_t maximum = 0) : maximum(maximum) {}

  bool append(const std::string &text) {
    if constexpr (TrackWork) {
      if (text.size() > std::numeric_limits<uint64_t>::max() - work) {
        if constexpr (SaturateWorkOverflow)
          work = std::numeric_limits<uint64_t>::max();
        return false;
      }
      work += text.size();
    }
    return append_bounded(bytes, maximum, text);
  }

  template <class Integer>
  bool integers(const std::vector<Integer> &values) {
    for (size_t index = 0; index < values.size(); ++index)
      if (!append((index ? "," : "") + std::to_string(values[index])))
        return false;
    return true;
  }
};

} // namespace tactix::semantic_extension
