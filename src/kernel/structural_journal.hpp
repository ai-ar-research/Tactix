#pragma once

#include <atomic>
#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace tactix {

// Where structural actions record: unbound (actions decline), a borrowed
// stream, or the discard journal, whose shared position counts non-empty
// writes as a stream position would.
class StructuralJournal {
public:
  StructuralJournal() = default;
  StructuralJournal(FILE *stream) : sink_(stream) {} // streams bind directly
  static StructuralJournal discard() {
    StructuralJournal journal;
    journal.sink_ = &discarded_;
    return journal;
  }
  explicit operator bool() const { return sink_ != nullptr; }
  bool operator==(const StructuralJournal &) const = default;

  // Whether records reach a stream, so their text must be spelled.
  bool spells() const { return stream() != nullptr; }
  long tell() const {
    if (FILE *out = stream()) return std::ftell(out);
    return sink_ ? discarded_.load() : -1;
  }
  bool failed() const {
    FILE *out = stream();
    return out && std::ferror(out);
  }
  void put(std::string_view text) const {
    if (FILE *out = stream()) std::fwrite(text.data(), 1, text.size(), out);
    else absorb(!text.empty());
  }
  // Each format spells at least one byte, as the discard count assumes.
  [[gnu::format(printf, 2, 3)]] void print(const char *format, ...) const {
    va_list arguments;
    va_start(arguments, format);
    if (FILE *out = stream()) {
      [[maybe_unused]] const int bytes = std::vfprintf(out, format, arguments);
      assert(bytes != 0);
    } else {
      absorb(true);
    }
    va_end(arguments);
  }
  void flush() const {
    FILE *out = stream();
    if (out && (std::fflush(out) || std::ferror(out))) std::abort();
  }
  // One finished record set of `size` bytes, spelled as `text` for a stream;
  // aborts unless the stream takes all of it.
  void write(std::string_view text, uint64_t size) const {
    if (FILE *out = stream()) {
      if (text.size() != size ||
          std::fwrite(text.data(), 1, text.size(), out) != text.size() ||
          std::fflush(out) || std::ferror(out))
        std::abort();
    } else {
      absorb(size != 0);
    }
  }

private:
  FILE *stream() const {
    return sink_ == &discarded_ ? nullptr : static_cast<FILE *>(sink_);
  }
  void absorb(bool written) const {
    if (sink_ != &discarded_) std::abort();
    discarded_ += written;
  }

  void *sink_ = nullptr;
  static inline std::atomic<long> discarded_{0};
};
static_assert(sizeof(StructuralJournal) == sizeof(FILE *) &&
              alignof(StructuralJournal) == alignof(FILE *));

} // namespace tactix
