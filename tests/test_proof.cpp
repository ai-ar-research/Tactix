// The proof backend itself (src/proof/proof.cpp): the two DRAT serializers and
// the native-LRAT writer, checked BYTE FOR BYTE.
//
// N2 rests on this file and it had no direct test: every other test and every
// fuzz leg reaches it end-to-end through a checker, which means a wrong byte is
// only ever caught when a checker happens to reject — and a checker cannot see
// the difference between "the rule chose the wrong lemma" and "the writer
// mis-encoded a correct one". These assertions pin the ENCODING (varint
// boundaries, the sign bit, the id/chain/hint layout, and which primitives are
// deliberately silent in which format), so a serializer regression fails here in
// milliseconds instead of somewhere inside a gigabyte-scale tier proof.
#undef NDEBUG
#include "proof/proof.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <unistd.h> // getpid, for a collision-free scratch name

using namespace tactix;

namespace {

std::string tmp_path(const char *tag) {
  const char *dir = getenv("TMPDIR");
  std::string p = (dir && *dir) ? dir : "/tmp";
  p += "/tactix-test-proof-";
  p += tag;
  p += "-" + std::to_string(static_cast<long>(getpid())) + ".proof";
  return p;
}

std::string read_all(const std::string &path) {
  FILE *f = fopen(path.c_str(), "rb");
  assert(f);
  std::string out;
  char buf[4096];
  size_t n;
  while ((n = fread(buf, 1, sizeof buf, f)) > 0)
    out.append(buf, n);
  fclose(f);
  return out;
}

// Every case runs the emitter, closes the file (Proof's destructor flushes) and
// compares the whole byte stream, so a stray extra byte fails as loudly as a
// wrong one.
template <class Emit>
std::string emitted(const char *tag, ProofFormat fmt, Emit emit) {
  const std::string path = tmp_path(tag);
  {
    Proof p;
    assert(p.open(path.c_str(), fmt));
    emit(p);
  }
  std::string bytes = read_all(path);
  remove(path.c_str());
  return bytes;
}

std::string hex(const std::string &s) {
  static const char *d = "0123456789abcdef";
  std::string out;
  for (unsigned char c : s) {
    out += d[c >> 4];
    out += d[c & 15];
    out += ' ';
  }
  return out;
}

void expect(const std::string &got, const std::string &want,
            const char *what) {
  if (got != want) {
    fprintf(stderr, "FAIL %s\n  got  [%s] = %s\n  want [%s] = %s\n", what,
            got.c_str(), hex(got).c_str(), want.c_str(), hex(want).c_str());
    abort();
  }
}

// --- binary DRAT ------------------------------------------------------------
// Encoding under test (drat-trim's): 'a'/'d' tag, then per literal the unsigned
// u = (|e| << 1) | (e < 0) as 7-bit varint chunks, low chunk first, high bit set
// on every chunk but the last, then a 0 terminator byte.
void test_drat_binary_add() {
  const Lit lits[3] = {make_lit(0, false), make_lit(1, true),
                       make_lit(2, false)};
  const std::string got =
      emitted("bin-add", ProofFormat::DratBinary,
              [&](Proof &p) { p.add_rup(7, lits, 3, nullptr, 0); });
  // +1 -> u=2; -2 -> u=5; +3 -> u=6. Ids and chains are LRAT-only.
  expect(got, std::string("a\x02\x05\x06\x00", 5), "drat-binary add_rup");
}

void test_drat_binary_varint_boundary() {
  // |e| = 64 is the first literal that does not fit in one 7-bit chunk:
  // u = 128 = 0x80 -> chunks 0x80, 0x01. |e| = 63 (u = 126) still fits, and
  // together they pin the boundary from both sides — an off-by-one in the
  // `u >= 128` test corrupts every proof with more than 63 variables.
  const Lit small[1] = {make_lit(62, false)}; // +63
  const Lit big[1] = {make_lit(63, false)};   // +64
  expect(emitted("bin-small", ProofFormat::DratBinary,
                 [&](Proof &p) { p.add_rup(1, small, 1, nullptr, 0); }),
         std::string("a\x7e\x00", 3), "drat-binary varint 1 byte");
  expect(emitted("bin-big", ProofFormat::DratBinary,
                 [&](Proof &p) { p.add_rup(1, big, 1, nullptr, 0); }),
         std::string("a\x80\x01\x00", 4), "drat-binary varint 2 bytes");
  // and the sign bit rides in bit 0 of the SAME varint, so -64 differs from +64
  const Lit negbig[1] = {make_lit(63, true)}; // -64 -> u = 129
  expect(emitted("bin-negbig", ProofFormat::DratBinary,
                 [&](Proof &p) { p.add_rup(1, negbig, 1, nullptr, 0); }),
         std::string("a\x81\x01\x00", 4), "drat-binary negative varint");
}

void test_drat_binary_delete() {
  const Lit lits[2] = {make_lit(0, true), make_lit(4, false)};
  // no space after 'd' in binary; the literals and terminator are as for add
  expect(emitted("bin-del", ProofFormat::DratBinary,
                 [&](Proof &p) { p.del(9, 3, lits, 2); }),
         std::string("d\x03\x0a\x00", 4), "drat-binary del");
}

void test_drat_binary_empty_clause() {
  // the refutation itself: tag + terminator, no literals
  expect(emitted("bin-empty", ProofFormat::DratBinary,
                 [&](Proof &p) { p.add_empty(11, nullptr, 0); }),
         std::string("a\x00", 2), "drat-binary add_empty");
}

// --- ascii DRAT -------------------------------------------------------------
void test_drat_ascii() {
  const Lit lits[2] = {make_lit(0, false), make_lit(2, true)};
  expect(emitted("asc-add", ProofFormat::DratAscii,
                 [&](Proof &p) { p.add_rup(5, lits, 2, nullptr, 0); }),
         "1 -3 0\n", "drat-ascii add_rup");
  // 'd' is followed by a SPACE in ascii (drat-trim needs the separator) and by
  // nothing in binary — the one place the two DRAT paths diverge.
  expect(emitted("asc-del", ProofFormat::DratAscii,
                 [&](Proof &p) { p.del(6, 5, lits, 2); }),
         "d 1 -3 0\n", "drat-ascii del");
  expect(emitted("asc-empty", ProofFormat::DratAscii,
                 [&](Proof &p) { p.add_empty(7, nullptr, 0); }),
         "0\n", "drat-ascii add_empty");
}

// --- native LRAT ------------------------------------------------------------
void test_lrat_add_rup() {
  const Lit lits[2] = {make_lit(0, false), make_lit(1, true)};
  const uint64_t chain[3] = {4, 2, 9};
  // `<id> <lits> 0 <antecedents> 0`, ids first, chain in checker-propagation
  // order — a reordered chain is a rejected proof, so the order is the test.
  expect(emitted("lrat-add", ProofFormat::Lrat,
                 [&](Proof &p) { p.add_rup(12, lits, 2, chain, 3); }),
         "12 1 -2 0 4 2 9 0\n", "lrat add_rup");
}

void test_lrat_add_rat() {
  const Lit lits[2] = {make_lit(2, false), make_lit(0, true)};
  // pivot-first literals; hints are the flat SIGNED stream (negative = a
  // resolution partner, positive = a RUP antecedent), and the sign must survive
  // verbatim
  const int64_t hints[4] = {7, -5, 3, -8};
  expect(emitted("lrat-rat", ProofFormat::Lrat,
                 [&](Proof &p) { p.add_rat(21, lits, 2, hints, 4); }),
         "21 3 -1 0 7 -5 3 -8 0\n", "lrat add_rat");
}

void test_lrat_empty_and_unit() {
  const uint64_t chain[2] = {1, 2};
  // the empty clause keeps its id and chain: `<id> 0 <chain> 0`
  expect(emitted("lrat-empty", ProofFormat::Lrat,
                 [&](Proof &p) { p.add_empty(30, chain, 2); }),
         "30 0 1 2 0\n", "lrat add_empty");
  // a root-level derived unit is EXPLICIT in LRAT (later chains cite its id)
  expect(emitted("lrat-unit", ProofFormat::Lrat,
                 [&](Proof &p) {
                   p.add_derived_unit(31, make_lit(3, true), chain, 2);
                 }),
         "31 -4 0 1 2 0\n", "lrat add_derived_unit");
  // ... and IMPLIED in DRAT: writing it would be a wrong lemma line, so both
  // DRAT paths must emit nothing at all for it.
  expect(emitted("bin-unit", ProofFormat::DratBinary,
                 [&](Proof &p) {
                   p.add_derived_unit(31, make_lit(3, true), chain, 2);
                 }),
         "", "drat-binary add_derived_unit is silent");
  expect(emitted("asc-unit", ProofFormat::DratAscii,
                 [&](Proof &p) {
                   p.add_derived_unit(31, make_lit(3, true), chain, 2);
                 }),
         "", "drat-ascii add_derived_unit is silent");
}

void test_lrat_delete() {
  const Lit lits[2] = {make_lit(0, false), make_lit(1, false)};
  // LRAT deletes BY ID and needs its own line id; the literals are ignored,
  // which is exactly the asymmetry with DRAT that a shared call site must get
  // right — pass literals and require they do not appear.
  expect(emitted("lrat-del", ProofFormat::Lrat,
                 [&](Proof &p) { p.del(40, 17, lits, 2); }),
         "40 d 17 0\n", "lrat del");
}

void test_lrat_multi_line_stream() {
  const Lit a[1] = {make_lit(0, false)};
  const Lit b[2] = {make_lit(1, false), make_lit(0, true)};
  const uint64_t c1[1] = {1};
  const uint64_t c2[2] = {2, 3};
  // a whole small stream: lines are newline-terminated and appended in call
  // order, with no separators of their own
  expect(emitted("lrat-stream", ProofFormat::Lrat,
                 [&](Proof &p) {
                   p.add_rup(50, a, 1, c1, 1);
                   p.add_rup(51, b, 2, c2, 2);
                   p.del(52, 50, a, 1);
                   p.add_empty(53, c2, 2);
                 }),
         "50 1 0 1 0\n51 2 -1 0 2 3 0\n52 d 50 0\n53 0 2 3 0\n",
         "lrat stream");
}

// --- the tautology predicate + the closed-file contract ---------------------
void test_tautological() {
  const Lit x = make_lit(3, false);
  const Lit y = make_lit(4, false);
  const Lit ok[2] = {x, y};
  const Lit dup[3] = {x, y, x};
  const Lit comp[2] = {x, negate(x)};
  assert(!Proof::tautological(nullptr, 0));
  assert(!Proof::tautological(ok, 2));
  assert(Proof::tautological(dup, 3));  // duplicate literal
  assert(Proof::tautological(comp, 2)); // complementary pair
}

void test_unopened_proof_is_silent() {
  // proof-off runs call the same primitives; they must be no-ops, not crashes.
  Proof p;
  const Lit lits[1] = {make_lit(0, false)};
  const uint64_t chain[1] = {1};
  p.add_rup(1, lits, 1, chain, 1);
  p.add_rat(2, lits, 1, nullptr, 0);
  p.add_empty(3, chain, 1);
  p.add_derived_unit(4, lits[0], chain, 1);
  p.del(5, 1, lits, 1);
  p.flush();
}

} // namespace

int main() {
  test_drat_binary_add();
  test_drat_binary_varint_boundary();
  test_drat_binary_delete();
  test_drat_binary_empty_clause();
  test_drat_ascii();
  test_lrat_add_rup();
  test_lrat_add_rat();
  test_lrat_empty_and_unit();
  test_lrat_delete();
  test_lrat_multi_line_stream();
  test_tautological();
  test_unopened_proof_is_silent();
  printf("test_proof: all proof-backend serializer cases passed\n");
  return 0;
}
