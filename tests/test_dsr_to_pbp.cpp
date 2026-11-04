// The DSR-to-PBP conversion of the automatic proof's switch (plan §5): each DSR
// line form, in the binary and ASCII spellings, and malformed streams.
#undef NDEBUG
#include "proof/dsr_to_pbp.hpp"

#include <cassert>
#include <cstdio>
#include <string>
#include <unistd.h>
#include <vector>

using namespace tactix;

namespace {

struct Converted {
  DsrToPbpResult result;
  std::string text;
};

Converted convert(const std::string &input, bool binary,
                  const std::vector<uint64_t> &rats) {
  FILE *in = std::tmpfile();
  assert(in);
  assert(std::fwrite(input.data(), 1, input.size(), in) == input.size());
  assert(!std::fflush(in));
  FILE *out = std::tmpfile();
  assert(out);
  Converted c;
  c.result = convert_dsr_to_pbp(fileno(in), binary, rats, out);
  assert(!std::fflush(out));
  std::rewind(out);
  char buffer[4096];
  size_t got;
  while ((got = std::fread(buffer, 1, sizeof buffer, out)))
    c.text.append(buffer, got);
  std::fclose(in);
  std::fclose(out);
  return c;
}

void put_varint(std::string &s, uint64_t v) {
  while (v >= 128) {
    s.push_back(static_cast<char>(v | 128));
    v >>= 7;
  }
  s.push_back(static_cast<char>(v));
}

std::string binary_line(int tag, const std::vector<int> &lits) {
  std::string s(1, static_cast<char>(tag));
  for (const int l : lits)
    put_varint(s, (static_cast<uint64_t>(l < 0 ? -l : l) << 1) | (l < 0));
  s.push_back(0);
  return s;
}

std::string ascii_line(bool deletion, const std::vector<int> &lits) {
  std::string s = deletion ? "d " : "";
  for (const int l : lits)
    s += std::to_string(l) + " ";
  return s + "0\n";
}

struct Line {
  bool deletion;
  std::vector<int> lits;
};

const std::vector<Line> kLines = {
    {false, {1, 2}},                      // 0: RUP
    {false, {3, -2}},                     // 1: marked RAT on its first literal
    {true, {1, 2}},                       // a deletion
    {true, {4}},                          // a unit deletion: dropped
    {true, {5, 5}},                       // canonically a unit: dropped
    {true, {6, -6, 7}},                   // a tautology: nothing to delete
    {true, {8, 9, 8}},                    // a repeated literal: canonical
    {false, {3, 1, 3, -4}},               // 2: witness, true literals only
    {false, {-5, 1, -5, 2, -5, 6, -7, -8, 9}}, // 3: true literal and mappings
    {false, {-6}},                        // 4: RUP unit
    {false, {}},                          // 5: the empty clause
};

const char *const kExpected =
    "rup 1 x1 1 x2 >= 1 ;\n"
    "red 1 x3 1 ~x2 >= 1 : x3 -> 1 ;\n"
    "del spec 1 x1 1 x2 >= 1 ;\n"
    "del spec 1 x8 1 x9 >= 1 ;\n"
    "red 1 x3 1 x1 >= 1 : x3 -> 1 x4 -> 0 ;\n"
    "red 1 ~x5 1 x1 >= 1 : x5 -> 0 x2 -> 1 x6 -> ~x7 x8 -> ~x9 ;\n"
    "rup 1 ~x6 >= 1 ;\n"
    "rup >= 1 ;\n";

} // namespace

int main() {
  std::string binary, ascii;
  for (const Line &line : kLines) {
    binary += binary_line(line.deletion ? 2 : 1, line.lits);
    ascii += ascii_line(line.deletion, line.lits);
  }
  for (const bool is_binary : {true, false}) {
    const Converted c = convert(is_binary ? binary : ascii, is_binary, {1});
    if (c.text != kExpected) {
      std::fprintf(stderr, "binary=%d got:\n%s\n", is_binary, c.text.c_str());
      return 1;
    }
    assert(c.result.ok);
    assert(c.result.derived == 6);
    assert(c.result.pbp_bytes == c.text.size());
    assert(c.result.dsr_bytes == (is_binary ? binary : ascii).size());
  }
  // An empty stage converts to nothing.
  assert(convert("", true, {}).result.ok && convert("", true, {}).text.empty());
  // A mark past the stream or a marked empty clause means the writer and the
  // stream disagree.
  assert(!convert(binary, true, {1, 99}).result.ok);
  assert(!convert(binary, true, {5}).result.ok);
  // Malformed streams: an unknown tag, a truncated line, an odd mapping list.
  assert(!convert(std::string(1, '\x03'), true, {}).result.ok);
  assert(!convert(binary.substr(0, binary.size() - 1), true, {}).result.ok);
  assert(!convert(binary_line(1, {1, 2, 1, 3, 1, 4}), true, {}).result.ok);
  assert(!convert("1 2", false, {}).result.ok);
  assert(!convert("1 x 0\n", false, {}).result.ok);
  // A long stream crosses the reader's and the writer's buffers.
  std::string many, expected;
  for (int i = 1; i <= 400000; ++i) {
    many += binary_line(1, {i, i + 1});
    expected += "rup 1 x" + std::to_string(i) + " 1 x" + std::to_string(i + 1) +
                " >= 1 ;\n";
  }
  const Converted c = convert(many, true, {});
  assert(c.result.ok && c.result.derived == 400000 && c.text == expected);
  std::puts("dsr to pbp: every line form, both spellings, malformed streams");
  return 0;
}
