#pragma once

#include "kernel/structural_journal.hpp"

#include <charconv>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace tactix {

// Integers up to 64 bits spell as std::to_string spells them. Character types
// spell differently under to_string, streams and +=, so they are not pieces.
template <class I>
concept JournalInteger =
    std::integral<I> && sizeof(I) <= sizeof(uint64_t) &&
    !std::is_same_v<I, char> && !std::is_same_v<I, signed char> &&
    !std::is_same_v<I, unsigned char> && !std::is_same_v<I, wchar_t> &&
    !std::is_same_v<I, char8_t> && !std::is_same_v<I, char16_t> &&
    !std::is_same_v<I, char32_t>;

// Spells journal pieces into `text`, or only measures them when `text` is
// null; `size` counts their bytes either way. Pieces are string literals,
// const char *, std::string_view, std::string lvalues, integers, and
// callables f(out) that emit further pieces through out(pieces...).
struct JournalPieces {
  explicit JournalPieces(std::string *spelling) : text(spelling) {}
  JournalPieces(const JournalPieces &) = delete;
  JournalPieces &operator=(const JournalPieces &) = delete;

  std::string *const text;
  uint64_t size = 0;

  template <class... P> void operator()(P &&...pieces) {
    (piece(std::forward<P>(pieces)), ...);
  }

private:
  void piece(std::string_view spelling) {
    size += spelling.size();
    if (text) text->append(spelling);
  }
  void piece(const char *spelling) { piece(std::string_view(spelling)); }
  void piece(const std::string &spelling) {
    piece(std::string_view(spelling));
  }
  // A temporary string is text formatted whether or not it is spelled.
  void piece(std::string &&) = delete;
  void piece(const std::string &&) = delete;
  template <JournalInteger I> void piece(I value) {
    if constexpr (std::is_same_v<I, bool>) {
      piece(int(value));
    } else if (text) {
      char digits[24];
      const auto end = std::to_chars(digits, digits + sizeof digits, value);
      piece(std::string_view(digits, size_t(end.ptr - digits)));
    } else {
      using Magnitude = std::make_unsigned_t<I>;
      Magnitude magnitude = Magnitude(value);
      if constexpr (std::is_signed_v<I>) {
        if (value < 0) {
          magnitude = Magnitude(Magnitude(0) - magnitude);
          ++size;
        }
      }
      for (++size; magnitude >= 10; magnitude /= 10) ++size;
    }
  }
  template <class F>
    requires std::invocable<F &, JournalPieces &> &&
             std::is_void_v<std::invoke_result_t<F &, JournalPieces &>>
  void piece(F &&emit) { emit(*this); }
};

// The comma-separated spelling of `items` as one piece; each(out, item)
// emits one item, the item itself by default.
template <class Items, class Each>
auto journal_list(const Items &items, Each each) {
  return [&items, each](auto &out) {
    for (size_t i = 0; i < items.size(); ++i) {
      if (i) out(",");
      each(out, items[i]);
    }
  };
}
template <class Items> auto journal_list(const Items &items) {
  return journal_list(items, [](auto &out, const auto &item) { out(item); });
}

// Journal record text, spelled for a journal that reaches a stream and
// otherwise only measured. Each append passes the size of its pieces to
// admit(size) and keeps them whole if the text stays within `maximum`; a
// refused append leaves the text unchanged.
class JournalText {
public:
  JournalText() = default; // measures
  explicit JournalText(const StructuralJournal &records)
      : spelled_(records.spells()) {}

  uint64_t size() const { return size_; }
  std::string_view text() const { return bytes_; }
  std::string take() && { return std::move(bytes_); }
  // Commits the text to the journal it was made for.
  void write(const StructuralJournal &records) const {
    records.write(bytes_, size_);
  }

  template <class Admit, class... P>
  bool append_if(uint64_t maximum, Admit &&admit, P &&...pieces) {
    const size_t mark = bytes_.size();
    JournalPieces out(spelled_ ? &bytes_ : nullptr);
    out(std::forward<P>(pieces)...);
    if (!admit(out.size) || size_ > maximum || out.size > maximum - size_) {
      bytes_.resize(mark);
      return false;
    }
    size_ += out.size;
    return true;
  }
  template <class... P> bool append(uint64_t maximum, P &&...pieces) {
    return append_if(maximum, [](uint64_t) { return true; },
                     std::forward<P>(pieces)...);
  }
  template <class... P> void put(P &&...pieces) {
    append(UINT64_MAX, std::forward<P>(pieces)...);
  }

private:
  bool spelled_ = false;
  uint64_t size_ = 0;
  std::string bytes_;
};

} // namespace tactix
