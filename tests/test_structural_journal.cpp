#undef NDEBUG
#include "rules/structural_text.hpp"

#include <cassert>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

using namespace tactix;

namespace {

FILE *const file = std::tmpfile();
const StructuralJournal stream = file;

// Spells `pieces` as one append, checking that the measured text has the same
// size and spells nothing.
template <class... P> std::string spell(const P &...pieces) {
  JournalText spelled(stream), measured;
  spelled.put(pieces...);
  measured.put(pieces...);
  assert(measured.text().empty() && spelled.size() == spelled.text().size() &&
         measured.size() == spelled.size());
  return std::string(spelled.text());
}

template <class I> void check_widths() {
  using Limits = std::numeric_limits<I>;
  std::vector<I> values{I(0), Limits::min(), Limits::max()};
  for (I power = 1;; power *= 10) {
    for (I value : {I(power - 1), power, I(power + 1)}) {
      values.push_back(value);
      if constexpr (Limits::is_signed) values.push_back(I(-value));
    }
    if (power > Limits::max() / 10) break;
  }
  for (I value : values) assert(spell(value) == std::to_string(value));
}

} // namespace

int main() {
  assert(file);
  static_assert(JournalInteger<bool> && JournalInteger<short> &&
                JournalInteger<unsigned long long> && !JournalInteger<char> &&
                !JournalInteger<signed char> &&
                !JournalInteger<unsigned char> && !JournalInteger<char32_t> &&
                !JournalInteger<double>);
#ifdef __SIZEOF_INT128__
  static_assert(!JournalInteger<__int128> &&
                !JournalInteger<unsigned __int128>);
#endif

  check_widths<short>();
  check_widths<unsigned short>();
  check_widths<int>();
  check_widths<unsigned>();
  check_widths<long>();
  check_widths<unsigned long>();
  check_widths<long long>();
  check_widths<unsigned long long>();
  assert(spell(true, false) == "10");

  const std::string word = "word";
  const char *pointer = "pointer";
  const std::vector<int> values{-1, 0, 17};
  const std::vector<std::string> names{"a", "b"};
  assert(spell("{", pointer, std::string_view("view"), word, -5, 7u,
               [&](auto &out) { out("[", journal_list(values), "]"); },
               journal_list(names, [](auto &out, const std::string &name) {
                 out("\"", name, "\"");
               }),
               journal_list(std::vector<int>{}), "}") ==
         "{pointerviewword-57[-1,0,17]\"a\",\"b\"}");

  // Admission charges first, then refuses an append past the cap whole; a
  // later shorter append still fits.
  for (const bool spelled : {true, false}) {
    JournalText text = spelled ? JournalText(stream) : JournalText();
    uint64_t charged = 0;
    const auto admit = [&](uint64_t size) {
      charged += size;
      return true;
    };
    assert(text.append_if(5, admit, "ab", 1));
    assert(!text.append_if(5, admit, "abc"));
    assert(!text.append_if(9, [](uint64_t) { return false; }, "x"));
    assert(text.size() == 3 && charged == 6);
    assert(text.append(5, "de") && !text.append(5, "f") && text.size() == 5);
    assert(text.text() == (spelled ? "ab1de" : ""));
  }

  const StructuralJournal unbound, discard = StructuralJournal::discard();
  assert(!unbound && unbound == nullptr && unbound.tell() == -1);
  assert(discard && !discard.spells() && !discard.failed() &&
         discard == StructuralJournal::discard() && discard != unbound);
  assert(!unbound.spells());
  const long start = discard.tell();
  assert(start >= 0);
  discard.put("");
  discard.write("", 0);
  discard.flush();
  assert(discard.tell() == start);
  discard.put("x");
  discard.print("%d", 5);
  discard.write("", 7);
  assert(discard.tell() == start + 3);

  assert(stream && stream.spells() && stream != discard && stream == file);
  const long origin = stream.tell();
  stream.put("ab");
  stream.print("%d", 12);
  stream.write("xyz", 3);
  stream.flush();
  assert(stream.tell() == origin + 7 && !stream.failed());
  std::fseek(file, origin, SEEK_SET);
  char bytes[8] = {};
  assert(std::fread(bytes, 1, sizeof bytes, file) == 7 &&
         std::string(bytes) == "ab12xyz");
  std::fclose(file);
  return 0;
}
