#include "proof/dsr_to_pbp.hpp"

#include <algorithm>
#include <charconv>
#include <string>
#include <unistd.h>

namespace tactix {
namespace {

class Reader {
public:
  explicit Reader(int fd) : fd_(fd), buffer_(size_t{1} << 22) {}
  // False at the end of the stream or on a read failure (see failed()).
  bool next(unsigned char &byte) {
    if (at_ == length_ && !fill())
      return false;
    byte = buffer_[at_++];
    return true;
  }
  void unread() { --at_; }
  bool failed() const { return failed_; }
  uint64_t consumed() const { return offset_ - length_ + at_; }

private:
  bool fill() {
    const ssize_t got = pread(fd_, buffer_.data(), buffer_.size(),
                              static_cast<off_t>(offset_));
    failed_ = got < 0;
    if (got <= 0)
      return false;
    offset_ += static_cast<uint64_t>(got);
    length_ = static_cast<size_t>(got);
    at_ = 0;
    return true;
  }
  int fd_;
  std::vector<unsigned char> buffer_;
  uint64_t offset_ = 0;
  size_t at_ = 0, length_ = 0;
  bool failed_ = false;
};

class Writer {
public:
  Writer(FILE *out, const std::vector<DsrHelperNames> &helpers)
      : helpers_(helpers), out_(out) {
    text_.reserve(capacity + 256);
  }
  void put(const char *text) { text_ += text; }
  void put_number(uint64_t value) {
    char digits[24];
    const auto end = std::to_chars(digits, digits + sizeof digits, value).ptr;
    text_.append(digits, end);
  }
  // One clause term: `1 x7 ` or `1 ~x7 `.
  void term(uint64_t code) {
    put(code & 1 ? "1 ~x" : "1 x");
    put_number(pbp_variable(code >> 1));
    text_ += ' ';
  }
  void clause(const uint64_t *codes, size_t count) {
    for (size_t i = 0; i < count; ++i)
      term(codes[i]);
    put(">= 1");
  }
  // ` x7 -> 1`: the literal is mapped true.
  void constant(uint64_t code) {
    put(" x");
    put_number(pbp_variable(code >> 1));
    put(code & 1 ? " -> 0" : " -> 1");
  }
  bool end_line() {
    put(" ;\n");
    return text_.size() < capacity || flush();
  }
  void mapping(uint64_t from, uint64_t to) {
    if (from & 1) // a negated source: its variable maps to the negated image
      to ^= 1;
    put(" x");
    put_number(pbp_variable(from >> 1));
    put(to & 1 ? " -> ~x" : " -> x");
    put_number(pbp_variable(to >> 1));
  }
  bool flush() {
    written_ += text_.size();
    const bool ok = std::fwrite(text_.data(), 1, text_.size(), out_) == text_.size();
    text_.clear();
    return ok;
  }
  uint64_t written() const { return written_; }

private:
  static constexpr size_t capacity = size_t{1} << 20;
  // A DSR name back to the solver's variable, or a helper to a name above them.
  uint64_t pbp_variable(uint64_t name) const {
    const auto at = std::lower_bound(
        helpers_.begin(), helpers_.end(), name,
        [](const DsrHelperNames &entry, uint64_t value) {
          return uint64_t{entry.extent} + entry.helpers < value;
        });
    if (at == helpers_.end())
      return name - (helpers_.empty() ? 0 : helpers_.back().helpers);
    const uint64_t before = at == helpers_.begin() ? 0 : std::prev(at)->helpers;
    return name > uint64_t{at->extent} + before ? name + (uint64_t{1} << 31)
                                                : name - before;
  }
  const std::vector<DsrHelperNames> &helpers_;
  FILE *out_;
  std::string text_;
  uint64_t written_ = 0;
};

enum class Line { End, Addition, Deletion, Malformed };

// Literal codes are `variable << 1 | negated`, the binary format's own.
Line read_binary(Reader &in, std::vector<uint64_t> &codes) {
  unsigned char byte;
  if (!in.next(byte))
    return in.failed() ? Line::Malformed : Line::End;
  if (byte != 1 && byte != 2)
    return Line::Malformed;
  const Line kind = byte == 1 ? Line::Addition : Line::Deletion;
  for (;;) {
    uint64_t value = 0;
    for (int shift = 0;; shift += 7) {
      if (shift > 63 || !in.next(byte))
        return Line::Malformed;
      value |= static_cast<uint64_t>(byte & 127) << shift;
      if (!(byte & 128))
        break;
    }
    if (!value)
      return kind;
    codes.push_back(value);
  }
}

Line read_ascii(Reader &in, std::vector<uint64_t> &codes) {
  unsigned char byte;
  const auto skip_space = [&] {
    while (in.next(byte))
      if (byte != ' ' && byte != '\n' && byte != '\t' && byte != '\r')
        return true;
    return false;
  };
  if (!skip_space())
    return in.failed() ? Line::Malformed : Line::End;
  Line kind = Line::Addition;
  if (byte == 'd') {
    kind = Line::Deletion;
    if (!skip_space())
      return Line::Malformed;
  }
  for (;;) {
    const bool negated = byte == '-';
    if (negated && !in.next(byte))
      return Line::Malformed;
    if (byte < '0' || byte > '9')
      return Line::Malformed;
    uint64_t value = 0;
    do {
      if (value > (UINT64_MAX >> 2) / 10)
        return Line::Malformed;
      value = value * 10 + (byte - '0');
    } while (in.next(byte) && byte >= '0' && byte <= '9');
    if (in.failed())
      return Line::Malformed;
    if (!value)
      return kind;
    codes.push_back(value << 1 | uint64_t{negated});
    // `byte` ended the number; a separator is required before the next one.
    if (!skip_space())
      return Line::Malformed;
  }
}

// VeriPB's CNF reader drops tautologies and repeated literals: keeps the first
// occurrence of each literal, in order. False for a tautology.
bool canonical(std::vector<uint64_t> &codes, std::vector<uint64_t> &sorted) {
  sorted = codes;
  std::sort(sorted.begin(), sorted.end());
  sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
  for (size_t i = 1; i < sorted.size(); ++i)
    if (sorted[i] >> 1 == sorted[i - 1] >> 1)
      return false;
  if (sorted.size() == codes.size())
    return true;
  size_t kept = 0;
  for (size_t i = 0; i < codes.size(); ++i)
    if (std::find(codes.begin(), codes.begin() + kept, codes[i]) ==
        codes.begin() + kept)
      codes[kept++] = codes[i];
  codes.resize(kept);
  return true;
}

} // namespace

DsrToPbpResult convert_dsr_to_pbp(int fd, bool binary,
                                  const std::vector<uint64_t> &rat_additions,
                                  const std::vector<DsrHelperNames> &helpers,
                                  FILE *out) {
  DsrToPbpResult result;
  Reader in(fd);
  Writer text(out, helpers);
  std::vector<uint64_t> codes, scratch;
  size_t next_rat = 0;
  for (;;) {
    codes.clear();
    const Line kind = binary ? read_binary(in, codes) : read_ascii(in, codes);
    if (kind == Line::Malformed)
      return result;
    if (kind == Line::End)
      break;
    if (kind == Line::Deletion) {
      // The DSR checkers ignore a unit deletion, so the converted database
      // keeps the unit as they do.
      if (canonical(codes, scratch) && codes.size() > 1) {
        text.put("del spec ");
        text.clause(codes.data(), codes.size());
        if (!text.end_line())
          return result;
      }
      continue;
    }
    const uint64_t ordinal = result.derived++;
    const bool marked_rat =
        next_rat < rat_additions.size() && rat_additions[next_rat] == ordinal;
    next_rat += marked_rat;
    // A witness starts at the pivot's second occurrence.
    const auto first_end = codes.empty() ? codes.end() : codes.begin() + 1;
    const auto witness = std::find(first_end, codes.end(),
                                   codes.empty() ? 0 : codes.front());
    if (witness != codes.end()) {
      const size_t size = static_cast<size_t>(witness - codes.begin());
      text.put("red ");
      text.clause(codes.data(), size);
      text.put(" :");
      text.constant(codes.front());
      auto at = witness + 1;
      const auto separator = std::find(at, codes.end(), codes.front());
      for (; at != separator; ++at)
        text.constant(*at);
      if (separator != codes.end()) {
        at = separator + 1;
        if ((codes.end() - at) % 2)
          return result;
        for (; at != codes.end(); at += 2)
          text.mapping(at[0], at[1]);
      }
    } else if (marked_rat) {
      if (codes.empty())
        return result;
      text.put("red ");
      text.clause(codes.data(), codes.size());
      text.put(" :");
      text.constant(codes.front());
    } else {
      text.put("rup ");
      text.clause(codes.data(), codes.size());
    }
    if (!text.end_line())
      return result;
  }
  if (next_rat != rat_additions.size() || !text.flush())
    return result;
  result.dsr_bytes = in.consumed();
  result.pbp_bytes = text.written();
  result.ok = true;
  return result;
}

} // namespace tactix
