// The proof backend (src/proof/proof.cpp): DSR and streaming-PBP
// serializers checked byte for byte. N2 rests on the encoding, which an
// end-to-end checker cannot distinguish from a wrong lemma.
#undef NDEBUG
#include "test_io.hpp"
#include "proof/proof.hpp"

#include <cassert>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#ifdef __linux__
#include <sys/prctl.h>
#endif
#include <sys/resource.h>
#include <sys/wait.h>

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

// Each case emits, closes the file, and compares the whole byte stream.
template <class Emit>
std::string emitted(const char *tag, ProofFormat fmt, Emit emit) {
  const std::string path = tmp_path(tag);
  {
    Proof p;
    assert(fmt == ProofFormat::PbpStreaming ? p.open(path.c_str(), fmt)
                                            : p.open_dsr_writer(path.c_str(), fmt));
    emit(p);
  }
  std::string bytes = read_file(path);
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

// --- binary line encoding ---------------------------------------------------
// A line tag, then per literal u = (|e|<<1)|(e<0) as low-first 7-bit chunks,
// high bit on all but the last, then a 0 terminator.
void test_binary_add() {
  const Lit lits[3] = {make_lit(0, false), make_lit(1, true),
                       make_lit(2, false)};
  const std::string got =
      emitted("bin-add", ProofFormat::DsrBinary,
              [&](Proof &p) { p.add_rup(7, lits, 3); });
  // +1 -> u=2; -2 -> u=5; +3 -> u=6.
  expect(got, std::string("\x01\x02\x05\x06\x00", 5), "binary add_rup");
}

void test_binary_varint_boundary() {
  // pin both sides of the `u >= 128` boundary: 63 fits one chunk, 64 needs two
  const Lit small[1] = {make_lit(62, false)}; // +63
  const Lit big[1] = {make_lit(63, false)};   // +64
  expect(emitted("bin-small", ProofFormat::DsrBinary,
                 [&](Proof &p) { p.add_rup(1, small, 1); }),
         std::string("\x01\x7e\x00", 3), "binary varint 1 byte");
  expect(emitted("bin-big", ProofFormat::DsrBinary,
                 [&](Proof &p) { p.add_rup(1, big, 1); }),
         std::string("\x01\x80\x01\x00", 4), "binary varint 2 bytes");
  // the sign bit rides in bit 0 of the same varint, so -64 differs from +64
  const Lit negbig[1] = {make_lit(63, true)}; // -64 -> u = 129
  expect(emitted("bin-negbig", ProofFormat::DsrBinary,
                 [&](Proof &p) { p.add_rup(1, negbig, 1); }),
         std::string("\x01\x81\x01\x00", 4), "binary negative varint");
}

void test_binary_delete() {
  const Lit lits[2] = {make_lit(0, true), make_lit(4, false)};
  expect(emitted("bin-del", ProofFormat::DsrBinary,
                 [&](Proof &p) { p.del(9, 3, lits, 2); }),
         std::string("\x02\x03\x0a\x00", 4), "binary del");
}

void test_binary_empty_clause() {
  expect(emitted("bin-empty", ProofFormat::DsrBinary,
                 [&](Proof &p) { p.add_empty(11); }),
         std::string("\x01\x00", 2), "binary add_empty");
}

// --- ascii lines ------------------------------------------------------------
void test_ascii_lines() {
  const Lit lits[2] = {make_lit(0, false), make_lit(2, true)};
  expect(emitted("asc-add", ProofFormat::DsrAscii,
                 [&](Proof &p) { p.add_rup(5, lits, 2); }),
         "1 -3 0\n", "ascii add_rup");
  expect(emitted("asc-del", ProofFormat::DsrAscii,
                 [&](Proof &p) { p.del(6, 5, lits, 2); }),
         "d 1 -3 0\n", "ascii del");
  expect(emitted("asc-empty", ProofFormat::DsrAscii,
                 [&](Proof &p) { p.add_empty(7); }),
         "0\n", "ascii add_empty");
}

// --- DSR -------------------------------------------------------------------
// Canonical binary DSR uses unprintable line tags 1/2 rather than the
// CaDiCaL-compatible DRAT tags 'a'/'d'. Witnessless additions otherwise share
// the literal encoding.
void test_dsr_binary() {
  const Lit lits[3] = {make_lit(0, false), make_lit(1, true),
                       make_lit(2, false)};
  expect(emitted("dsr-bin-add", ProofFormat::DsrBinary,
                 [&](Proof &p) { p.add_rup(7, lits, 3); }),
         std::string("\x01\x02\x05\x06\x00", 5), "dsr-binary add_rup");
  expect(emitted("dsr-bin-rat", ProofFormat::DsrBinary,
                 [&](Proof &p) { p.add_rat(8, lits, 3); }),
         std::string("\x01\x02\x05\x06\x00", 5), "dsr-binary add_rat");
  expect(emitted("dsr-bin-del", ProofFormat::DsrBinary,
                 [&](Proof &p) { p.del(9, 7, lits, 3); }),
         std::string("\x02\x02\x05\x06\x00", 5), "dsr-binary del");
  expect(emitted("dsr-bin-empty", ProofFormat::DsrBinary,
                 [&](Proof &p) { p.add_empty(10); }),
         std::string("\x01\x00", 2), "dsr-binary add_empty");
}

void test_dsr_ascii() {
  const Lit lits[2] = {make_lit(0, false), make_lit(2, true)};
  expect(emitted("dsr-asc-add", ProofFormat::DsrAscii,
                 [&](Proof &p) { p.add_rup(5, lits, 2); }),
         "1 -3 0\n", "dsr-ascii add_rup");
  expect(emitted("dsr-asc-del", ProofFormat::DsrAscii,
                 [&](Proof &p) { p.del(6, 5, lits, 2); }),
         "d 1 -3 0\n", "dsr-ascii del");
  expect(emitted("dsr-asc-root-unit", ProofFormat::DsrAscii,
                 [&](Proof &p) {
                   p.add_derived_unit(9, lits[0]);
                 }),
         "1 0\n", "dsr-ascii derived root unit");
}

void test_dsr_unit_deletion_transaction() {
  const auto lit = [](int e) { return make_lit(std::abs(e) - 1, e < 0); };
  const Lit unit = lit(1);
  const Lit support[2] = {lit(2), lit(-3)};
  const Lit support_reordered[2] = {lit(-3), lit(2)};
  const Lit unrelated[2] = {lit(4), lit(5)};
  const auto emit = [&](Proof &p) {
    p.add_rup(10, &unit, 1);
    p.del(11, 7, support_reordered, 2);
    p.del(12, 8, unrelated, 2);
    p.flush();
  };
  expect(emitted("dsr-transaction-ascii", ProofFormat::DsrAscii, emit),
         "1 6 0\n1 -6 0\nd -3 2 0\nd 4 5 0\n1 0\n"
         "d 1 6 0\nd 1 -6 0\n",
         "DSR split preserves a root unit across all following deletions");
  expect(emitted("dsr-transaction-binary", ProofFormat::DsrBinary, emit),
         std::string("\x01\x02\x0c\x00\x01\x02\x0d\x00"
                     "\x02\x07\x04\x00\x02\x08\x0a\x00"
                     "\x01\x02\x00\x02\x02\x0c\x00"
                     "\x02\x02\x0d\x00", 27),
         "binary DSR preserves the same split transaction");
  const int32_t external_support[2] = {2, 3};
  expect(emitted("dsr-transaction-external-delete", ProofFormat::DsrBinary,
                 [&](Proof &p) {
                   p.add_rup(10, &unit, 1);
                   assert(p.del_external(11, 7, external_support, 2));
                 }),
         std::string("\x01\x02\x08\x00\x01\x02\x09\x00"
                     "\x02\x04\x06\x00\x01\x02\x00"
                     "\x02\x02\x08\x00\x02\x02\x09\x00", 23),
         "external deletion uses the same bounded binary spool");
  const Lit second = lit(6);
  expect(emitted("dsr-transaction-unit-batch", ProofFormat::DsrAscii,
                 [&](Proof &p) {
                   p.add_rup(10, &unit, 1);
                   p.add_rup(11, &second, 1);
                   p.del(12, 7, support, 2);
                 }),
         "1 7 0\n1 -7 0\n6 7 0\n6 -7 0\nd 2 -3 0\n"
         "1 0\n6 0\nd 1 7 0\nd 1 -7 0\nd 6 7 0\nd 6 -7 0\n",
         "consecutive RUP units share one proof-only split before deletion");
  expect(emitted("dsr-transaction-earlier-unit-delete", ProofFormat::DsrAscii,
                 [&](Proof &p) {
                   p.add_rup(10, &unit, 1);
                   p.add_rup(11, &second, 1);
                   p.del(12, 10, &unit, 1);
                 }),
         "1 0\n6 0\nd 1 0\n",
         "a deletion of any staged unit first publishes the full batch");
  const std::vector<Var> map{5, 6, 7, 8, 9};
  expect(emitted("dsr-transaction-mapped", ProofFormat::DsrAscii,
                 [&](Proof &p) {
                   p.bind_variable_map(&map);
                   emit(p);
                 }),
         "6 11 0\n6 -11 0\nd -8 7 0\nd 9 10 0\n6 0\n"
         "d 6 11 0\nd 6 -11 0\n",
         "DSR split identity respects the external variable mapping");
  const Lit next = lit(6);
  expect(emitted("dsr-transaction-next-add", ProofFormat::DsrAscii,
                 [&](Proof &p) {
                   p.add_rup(10, &unit, 1);
                   p.del(11, 7, support, 2);
                   p.add_rup(12, &next, 1);
                 }),
         "1 4 0\n1 -4 0\nd 2 -3 0\n1 0\n"
         "d 1 4 0\nd 1 -4 0\n6 0\n",
         "next addition closes the pending unit transaction");
  expect(emitted("dsr-transaction-next-binary-unit", ProofFormat::DsrBinary,
                 [&](Proof &p) {
                   p.add_rup(10, &unit, 1);
                   p.del(11, 7, support, 2);
                   p.add_rup(12, &next, 1);
                 }),
         std::string("\x01\x02\x08\x00\x01\x02\x09\x00"
                     "\x02\x04\x07\x00\x01\x02\x00"
                     "\x02\x02\x08\x00\x02\x02\x09\x00"
                     "\x01\x0c\x00", 26),
         "binary deletion buffer closes before the next unit");
  expect(emitted("dsr-transaction-delete-unit", ProofFormat::DsrAscii,
                 [&](Proof &p) {
                   p.add_rup(10, &unit, 1);
                   p.del(11, 10, &unit, 1);
                 }),
         "1 0\nd 1 0\n",
         "a pending unit is published before its own deletion");
  const Lit open_pair[2] = {lit(1), lit(2)};
  const DsrLiteralMapping open_maps[2] = {
      {lit(1), lit(3)}, {lit(2), lit(4)}};
  expect(emitted("dsr-transaction-declined-sr", ProofFormat::DsrAscii,
                 [&](Proof &p) {
                   assert(std::string(p.verification_route()) == "dsr-cake_lpr");
                   p.add_rup(10, &unit, 1);
                   assert(!p.add_sr(11, open_pair, 2, nullptr, 0, open_maps, 2,
                                    INT32_MAX));
                   assert(std::string(p.verification_route()) == "dsr-cake_lpr");
                   p.del(12, 8, unrelated, 2);
                 }),
         "1 6 0\n1 -6 0\nd 4 5 0\n1 0\n"
         "d 1 6 0\nd 1 -6 0\n",
         "a declined SR addition does not close a pending unit transaction");
  const int32_t tautology[2] = {6, -6};
  expect(emitted("dsr-transaction-declined-external", ProofFormat::DsrAscii,
                 [&](Proof &p) {
                   p.add_rup(10, &unit, 1);
                   assert(!p.add_external_rup(11, tautology, 2));
                   p.del(12, 8, unrelated, 2);
                 }),
         "1 6 0\n1 -6 0\nd 4 5 0\n1 0\n"
         "d 1 6 0\nd 1 -6 0\n",
         "a declined external addition preserves the pending unit order");
  const DsrLiteralMapping rup_maps[2] = {
      {lit(1), lit(2)}, {lit(2), lit(1)}};
  expect(emitted("dsr-transaction-accepted-sr", ProofFormat::DsrAscii,
                 [&](Proof &p) {
                   p.add_rup(10, &unit, 1);
                   assert(p.add_sr(11, open_pair, 2, nullptr, 0, rup_maps, 2,
                                   6));
                   assert(std::string(p.verification_route()) == "dsr-cake_lpr");
                 }),
         "1 0\n1 2 0\n",
         "an accepted SR addition closes the pending unit transaction");
}

void test_dsr_deletion_spill() {
  // One large deletion block must cross the bounded memory spool without
  // changing the order or encoding of the split transaction.
  const Lit unit = make_lit(0, false);
  constexpr int deletions = 16000;
  const int auxiliary = deletions + 3;
  const auto line = [](unsigned char tag, std::initializer_list<int> lits) {
    std::string bytes(1, static_cast<char>(tag));
    for (int literal : lits) {
      uint32_t token = (static_cast<uint32_t>(std::abs(literal)) << 1) |
                       (literal < 0);
      while (token >= 128) {
        bytes.push_back(static_cast<char>(128 | (token & 127)));
        token >>= 7;
      }
      bytes.push_back(static_cast<char>(token));
    }
    bytes.push_back(0);
    return bytes;
  };
  const std::string got = emitted("dsr-deletion-spill", ProofFormat::DsrBinary,
                                  [&](Proof &p) {
    p.add_rup(1, &unit, 1);
    for (int i = 0; i < deletions; ++i) {
      const Lit clause[2] = {make_lit(1, false), make_lit(i + 2, false)};
      p.del(2 + i, 2 + i, clause, 2);
    }
  });
  std::string want = line(1, {1, auxiliary}) + line(1, {1, -auxiliary});
  for (int i = 0; i < deletions; ++i)
    want += line(2, {2, i + 3});
  want += line(1, {1}) + line(2, {1, auxiliary}) +
          line(2, {1, -auxiliary});
  assert(want.size() > 65536);
  expect(got, want, "large binary DSR deletion block spills intact");
}

void test_dsr_substitution() {
  const Lit clause[2] = {make_lit(0, false), make_lit(1, true)};
  const Lit true_lits[1] = {make_lit(2, false)};
  const DsrLiteralMapping mappings[1] = {
      {make_lit(3, false), make_lit(4, true)}};

  expect(emitted("dsr-asc-sr", ProofFormat::DsrAscii, [&](Proof &p) {
           assert(std::string(p.verification_route()) == "dsr-cake_lpr");
           assert(p.add_sr(11, clause, 2, true_lits, 1, mappings, 1, 5));
           assert(std::string(p.verification_route()) == "dsr-trestle");
         }),
         "1 -2 1 3 1 4 -5 0\n", "dsr-ascii add_sr");
  expect(emitted("dsr-bin-sr", ProofFormat::DsrBinary, [&](Proof &p) {
           assert(p.add_sr(11, clause, 2, true_lits, 1, mappings, 1, 5));
         }),
         std::string("\x01\x02\x05\x02\x06\x02\x08\x0b\x00", 9),
         "dsr-binary add_sr");

  Proof unopened;
  assert(unopened.add_sr(11, clause, 2, true_lits, 1, mappings, 1, 5));
}

// Pinned Trestle rejects the pivot literal as a mapping source and forces the
// pivot true after the substitution. An explicit pivot mapping is therefore
// re-expressed in a form both dsr-trim and Trestle read identically.
void test_dsr_pivot_mapping_forms() {
  const auto lit = [](int e) { return make_lit(std::abs(e) - 1, e < 0); };
  const auto mapping = [&](int from, int to) {
    return DsrLiteralMapping{lit(from), lit(to)};
  };
  const auto ascii = [](const char *name, const std::vector<Lit> &clause,
                        const std::vector<Lit> &true_lits,
                        const std::vector<DsrLiteralMapping> &mappings) {
    return emitted(name, ProofFormat::DsrAscii, [&](Proof &p) {
      assert(p.add_sr(1, clause.data(), clause.size(), true_lits.data(),
                      true_lits.size(), mappings.data(), mappings.size(), 6));
    });
  };

  // SST cut: the pivot image 2 is true under !C, so its mapping is dropped.
  expect(ascii("pivot-drop", {lit(1), lit(-2)}, {},
               {mapping(1, 2), mapping(2, 1)}),
         "1 -2 1 1 2 1 0\n", "pivot mapping true under !C is dropped");
  expect(ascii("negative-pivot-drop", {lit(-1), lit(2)}, {}, {mapping(1, 2)}),
         "-1 2 -1 0\n", "negative pivot mapping true under !C is dropped");
  // Another clause literal is mapped true, so it becomes the pivot.
  expect(ascii("repivot", {lit(1), lit(3)}, {lit(3)}, {mapping(1, 2)}),
         "3 1 3 3 1 2 0\n", "true-mapped literal becomes the pivot");
  // Every image is false under !C: the clause is RUP.
  expect(ascii("rup", {lit(1), lit(2)}, {}, {mapping(1, 2), mapping(2, 1)}),
         "1 2 0\n", "all-false image is a witnessless addition");
  // One open image m: RUP (C | m), then C with that pivot, then delete.
  expect(ascii("unit-lemma", {lit(1)}, {}, {mapping(1, 6)}),
         "1 6 0\n1 1 0\nd 1 6 0\n", "open unit pivot image uses a lemma");
  const Lit unit[1] = {lit(1)};
  const DsrLiteralMapping unit_map[1] = {mapping(1, 6)};
  expect(emitted("unit-lemma-bin", ProofFormat::DsrBinary,
                 [&](Proof &p) {
                   assert(p.add_sr(1, unit, 1, nullptr, 0, unit_map, 1, 6));
                 }),
         std::string("\x01\x02\x0c\x00\x01\x02\x02\x00\x02\x02\x0c\x00", 12),
         "dsr-binary pivot lemma");
  expect(ascii("lemma-pivot", {lit(2), lit(1)}, {},
               {mapping(2, 1), mapping(1, 6)}),
         "1 2 6 0\n1 2 1 1 2 1 0\nd 1 2 6 0\n",
         "open non-pivot image becomes the lemma pivot");

  // A clause that is unit at the root would propagate as soon as the checker
  // reads it, so it takes the general encoding: neither witnessed temporary is
  // unit, the default pivot image is stated, and the clause enters by RUP.
  const Lit root_unit[1] = {lit(1)};
  const Lit root_unit_true[1] = {lit(3)};
  expect(emitted("propagating-default-pivot", ProofFormat::DsrAscii,
                 [&](Proof &p) {
                   assert(p.add_sr(1, root_unit, 1, root_unit_true, 1, nullptr,
                                   0, 6, /*propagates=*/true));
                 }),
         "-7 1 0\n7 1 7 1 3 0\n1 0\nd 7 1 0\nd -7 1 0\n",
         "a propagating witnessed unit uses the general encoding");
  expect(emitted("propagating-mapped-pivot", ProofFormat::DsrAscii,
                 [&](Proof &p) {
                   assert(p.add_sr(1, unit, 1, nullptr, 0, unit_map, 1, 6,
                                   /*propagates=*/true));
                 }),
         "-7 1 0\n7 1 7 7 1 6 0\n1 0\nd 7 1 0\nd -7 1 0\n",
         "a propagating mapped pivot keeps its explicit image");
  // A witnessless addition cannot lose hint groups; it stays a plain line.
  expect(emitted("propagating-rup", ProofFormat::DsrAscii, [&](Proof &p) {
           const Lit pair[2] = {lit(1), lit(2)};
           const DsrLiteralMapping swap[2] = {mapping(1, 2), mapping(2, 1)};
           assert(p.add_sr(1, pair, 2, nullptr, 0, swap, 2, 6,
                           /*propagates=*/true));
         }),
         "1 2 0\n", "a propagating RUP form stays witnessless");

  const Lit open_pair[2] = {lit(1), lit(2)};
  const DsrLiteralMapping open_maps[2] = {mapping(1, 3), mapping(2, 4)};
  assert(plan_dsr_witness(open_pair, 2, nullptr, 0, open_maps, 2).form ==
         DsrWitnessForm::Extension);
  const DsrLiteralMapping unrelated[1] = {mapping(3, 4)};
  assert(plan_dsr_witness(open_pair, 2, nullptr, 0, unrelated, 1).form ==
         DsrWitnessForm::Direct);

  const std::string extension =
      "-5 1 2 0\n5 1 2 5 5 1 3 2 4 0\n1 2 0\n"
      "d 5 1 2 0\nd -5 1 2 0\n";
  expect(emitted("general-extension", ProofFormat::DsrAscii, [&](Proof &p) {
           assert(p.add_sr(1, open_pair, 2, nullptr, 0, open_maps, 2, 4));
           assert(p.add_sr(2, open_pair, 2, nullptr, 0, open_maps, 2, 4));
         }), extension + extension, "temporary external variable is reusable");
  expect(emitted("general-extension-mapped", ProofFormat::DsrAscii,
                 [&](Proof &p) {
                   const std::vector<Var> map{6, 1, 8, 3};
                   p.bind_variable_map(&map);
                   assert(p.add_sr(1, open_pair, 2, nullptr, 0, open_maps, 2, 9));
                 }),
         "-10 7 2 0\n10 7 2 10 10 7 9 2 4 0\n7 2 0\n"
         "d 10 7 2 0\nd -10 7 2 0\n",
         "temporary external identity bypasses internal renaming");
  expect(emitted("general-extension-exhausted", ProofFormat::DsrAscii,
                 [&](Proof &p) {
                   assert(!p.add_sr(1, open_pair, 2, nullptr, 0, open_maps, 2,
                                    INT32_MAX));
                 }), "", "external literal exhaustion emits no prefix");
}

void test_external_variable_map() {
  const Lit lits[2] = {make_lit(0, false), make_lit(1, true)};
  const std::vector<Var> map = {4, 1};
  expect(emitted("mapped", ProofFormat::DsrAscii, [&](Proof &p) {
           p.bind_variable_map(&map);
           p.add_rup(1, lits, 2);
         }),
         "5 -2 0\n", "stable external variable map");
}

void test_proof_only_external_clauses() {
  const int32_t gate[3] = {70001, -2, 4};
  const int32_t unit[1] = {-70001};
  expect(emitted("external-invalid-min", ProofFormat::DsrAscii, [&](Proof &p) {
           const int32_t min_second[2] = {1, INT32_MIN};
           const int32_t min_first[2] = {INT32_MIN, 1};
           assert(!p.add_external_rup(1, min_second, 2));
           assert(!p.add_external_rat(2, min_first, 2));
         }), "", "out-of-range external literals decline without signed overflow");
  expect(emitted("external-ascii", ProofFormat::DsrAscii, [&](Proof &p) {
           const std::vector<Var> map = {8, 9, 10, 11};
           p.bind_variable_map(&map);
           p.arm_population_shadow();
           assert(p.add_external_rat(1, gate, 3));
           assert(p.add_external_rup(2, unit, 1));
           assert(p.del_external(3, 1, gate, 3));
           assert(p.add_external_rup(4, nullptr, 0));
         }),
         "70001 -2 4 0\n-70001 0\nd 70001 -2 4 0\n0\n",
         "proof-only identities bypass the solver variable map");
  expect(emitted("external-binary", ProofFormat::DsrBinary, [&](Proof &p) {
           const int32_t clause[2] = {64, -65};
           p.arm_population_shadow();
           assert(p.add_external_rat(1, clause, 2));
           assert(p.del_external(2, 1, clause, 2));
         }),
         std::string("\x01\x80\x01\x83\x01\x00"
                     "\x02\x80\x01\x83\x01\x00", 12),
         "binary proof-only extension encoding");
}

void test_derived_unit() {
  expect(emitted("dsr-bin-unit", ProofFormat::DsrBinary,
                 [&](Proof &p) {
                   p.add_derived_unit(31, make_lit(3, true));
                 }),
         std::string("\x01\x09\x00", 3),
         "dsr-binary add_derived_unit is explicit");
  expect(emitted("dsr-asc-unit", ProofFormat::DsrAscii,
                 [&](Proof &p) {
                   p.add_derived_unit(31, make_lit(3, true));
                 }),
         "-4 0\n", "dsr-ascii add_derived_unit is explicit");
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
  p.add_rup(1, lits, 1);
  p.add_rat(2, lits, 1);
  p.add_empty(3);
  p.add_derived_unit(4, lits[0]);
  p.del(5, 1, lits, 1);
  p.flush();
}

#ifdef TACTIX_ENABLE_PROOF_SHADOW
void test_population_membership_uses_exact_drat_multiset() {
  Proof p;
  assert(p.population_shadow_available());
  assert(!p.population_shadow());
  const Lit a = make_lit(0, false), b = make_lit(1, true);
  const Lit raw[3] = {a, b, b};
  const Lit reordered[3] = {b, a, b};
  const Lit normalized[2] = {a, b};
  p.seed_population(1, raw, 3);
  assert(p.population_contains(99, reordered, 3));
  assert(!p.population_contains(1, normalized, 2));
  // Input import publishes replacements before the strict checker is armed.
  // Known deletions must still update the population during that interval.
  const std::string path = tmp_path("import-shadow");
  assert(p.open_dsr_writer(path.c_str(), ProofFormat::DsrAscii));
  p.add_rup(2, normalized, 2);
  p.del(2, 1, raw, 3);
  assert(!p.population_contains(1, raw, 3));
  assert(p.population_contains(2, normalized, 2));
  std::remove(path.c_str());
  p.arm_population_shadow();
  assert(p.population_shadow());
}

// The whole-run PBP stream records an external RAT addition, so deleting it
// is backed.
void test_population_backs_streaming_pbp_external_rat() {
  const std::string path = tmp_path("pbp-shadow-external-rat");
  {
    Proof p;
    assert(p.open(path.c_str(), ProofFormat::PbpStreaming));
    const Lit input = make_lit(0, false);
    p.seed_population(1, &input, 1);
    p.finish_input_transaction();
    p.arm_population_shadow();
    const int32_t gate[2] = {-9, 1};
    assert(p.add_external_rat(2, gate, 2));
    assert(p.del_external(3, 2, gate, 2));
    assert(p.unbacked_deletions() == 0);
  }
  std::remove(path.c_str());
}
#endif

} // namespace

void test_startup_pbp_certificate_boundary() {
  const Lit unit = internalize(1);
  const auto path = tmp_path("startup-pbp");
  const auto write_pb = [](FILE *out) {
    return std::fputs("pseudo-Boolean proof version 3.0\n", out) >= 0;
  };
  std::vector<int32_t> input;
  {
    Proof p;
    assert(p.open(path.c_str(), ProofFormat::PbpStreaming));
    assert(!p.startup_pb_available());
    p.add_rup(1, &unit, 1);
    assert(read_file(path).empty()); // input prefix is private
    p.finish_input_transaction();
    assert(p.startup_pb_available());
    assert(p.refute_by_counting(write_pb, input));
    assert(p.startup_pb_written() && !p.startup_pb_available());
    assert(!p.commit_clause_stream());
    assert(!p.refute_by_counting(write_pb, input));
  }
  expect(read_file(path), "pseudo-Boolean proof version 3.0\n", "atomic PB selection");
  std::remove(path.c_str());
  Proof strict;
  assert(strict.open_dsr_writer(path.c_str(), ProofFormat::DsrAscii));
  strict.finish_input_transaction();
  assert(!strict.startup_pb_available() && !strict.refute_by_counting(write_pb, input));
  std::remove(path.c_str());
  // A completed derivation is not a successful publication on a full device:
  // the run stops before any verdict.
  if (access("/dev/full", W_OK)) {
    std::puts("SKIP: full-device PB publication check (/dev/full unavailable)");
    return;
  }
  const pid_t pb_child = fork();
  assert(pb_child >= 0);
  if (!pb_child) {
    Proof failed_output;
    assert(failed_output.open("/dev/full", ProofFormat::PbpStreaming));
    failed_output.finish_input_transaction();
    failed_output.refute_by_counting(write_pb, input);
    _exit(0);
  }
  int pb_status = 0;
  assert(waitpid(pb_child, &pb_status, 0) == pb_child);
  assert(WIFSIGNALED(pb_status) && WTERMSIG(pb_status) == SIGABRT);
}

// A startup refutation with no proof takes the same path and formats nothing.
void test_startup_pb_without_a_proof() {
  bool wrote = false;
  const auto write_pb = [&](FILE *out) {
    wrote = true;
    return std::fputs("pseudo-Boolean proof version 3.0\n", out) >= 0;
  };
  std::vector<int32_t> input{1, 0, -1, 0};
  Proof nowhere;
  assert(nowhere.open_nowhere());
  nowhere.finish_input_transaction();
  assert(nowhere.refute_by_counting(write_pb, input));
  assert(!wrote && nowhere.startup_pb_written());
  const std::string path = tmp_path("startup-pb");
  {
    Proof p;
    assert(p.open(path.c_str(), ProofFormat::PbpStreaming));
    p.finish_input_transaction();
    assert(p.refute_by_counting(write_pb, input));
    assert(wrote && p.startup_pb_written());
    assert(!p.refute_by_counting(write_pb, input));
    assert(input.size() == 4 && !p.dsr_follow_on_pending());
  }
  std::remove(path.c_str());
}

void test_streaming_pbp() {
  const auto path = tmp_path("pbp-stream");
  {
    Proof p;
    assert(p.open(path.c_str(), ProofFormat::PbpStreaming));
    const Lit first_input = internalize(10);
    p.seed_population(1, &first_input, 1);
    const Lit input = internalize(1);
    p.seed_population(2, &input, 1);
    p.finish_input_transaction();
    const Lit unit = internalize(2);
    p.add_rup(3, &unit, 1);
    const Lit rat = internalize(-3);
    p.add_rat(4, &rat, 1);
    const Lit sr_clause = internalize(4);
    const DsrLiteralMapping mapping{internalize(5), internalize(-6)};
    assert(p.add_sr(5, &sr_clause, 1, nullptr, 0, &mapping, 1, 6));
    const int32_t external_rup[] = {7, -8};
    assert(p.add_external_rup(6, external_rup, 2));
    const int32_t external_rat[] = {-9};
    assert(p.add_external_rat(7, external_rat, 1));
    p.del(9, 3, &unit, 1);
    assert(p.del_external(10, 6, external_rup, 2));
    p.del(11, 1, &first_input, 1);
    p.add_empty(8);
    p.flush();
  }
  expect(read_file(path),
         "pseudo-Boolean proof version 3.0\n"
         "f 2;\n"
         "rup 1 x2 >= 1 ;\n"
         "red 1 ~x3 >= 1 : x3 -> 0 ;\n"
         "red 1 x4 >= 1 : x4 -> 1 x5 -> ~x6 ;\n"
         "rup 1 x7 1 ~x8 >= 1 ;\n"
         "red 1 ~x9 >= 1 : x9 -> 0 ;\n"
         "del id 3 ;\n"
         "del spec 1 x7 1 ~x8 >= 1 ;\n"
         "del id 1 ;\n"
         "rup >= 1 ;\n"
         "output NONE ;\n"
         "conclusion UNSAT : -1 ;\n"
         "end pseudo-Boolean proof ;\n",
         "whole-run PBP stream");
  std::remove(path.c_str());
}

void test_streaming_pbp_does_not_archive_divergent_binary_ids() {
  const auto path = tmp_path("pbp-stream-binary-divergence");
  {
    Proof proof;
    assert(proof.open(path.c_str(), ProofFormat::PbpStreaming));
    const Lit binary[] = {internalize(10), internalize(-11)};
    const Lit unit = internalize(12);
    // The deliberately non-consecutive internal identities force the input
    // suffix onto the explicit identity-map path.
    proof.seed_population(100, binary, 2);
    proof.seed_population(101, &unit, 1);
    proof.finish_input_transaction();
    proof.del(102, 100, binary, 2);
    // Reusing the retired binary's internal ID must not collide with a stale
    // binary entry in the proof-side identity map. Such a collision would
    // abandon exact deletion for both this derived unit and the input unit.
    proof.add_rup(100, &unit, 1);
    proof.del(103, 100, &unit, 1);
    proof.del(104, 101, &unit, 1);
    proof.add_empty(105);
    proof.flush();
  }
  expect(read_file(path),
         "pseudo-Boolean proof version 3.0\n"
         "f 2;\n"
         "del spec 1 x10 1 ~x11 >= 1 ;\n"
         "rup 1 x12 >= 1 ;\n"
         "del id 3 ;\n"
         "del id 2 ;\n"
         "rup >= 1 ;\n"
         "output NONE ;\n"
         "conclusion UNSAT : -1 ;\n"
         "end pseudo-Boolean proof ;\n",
         "divergent binary PBP identity");
  std::remove(path.c_str());
}

void test_streaming_pbp_parity_text_buffer_boundary() {
  const auto path = tmp_path("pbp-parity-buffer-boundary");
  {
    Proof proof;
    assert(proof.open(path.c_str(), ProofFormat::PbpStreaming));
    proof.finish_input_transaction();
    assert(proof.startup_pb_available());
    assert(proof.commit_clause_stream());
    assert(!proof.startup_pb_available());
    const PbpParityEquation equation{{1, 2}, false};
    const int32_t reason[] = {-1, 2};
    PbpParityBoundsCache cache;
    PbpParityReasonPlan first;
    assert(proof.plan_pbp_parity_reason({equation}, reason, 2, 2, cache,
                                        first));
    assert(proof.add_prevalidated_pbp_parity_reason(
        1, {equation}, reason, 2, cache, first));
    proof.flush();
    const std::string prefix = read_file(path);

    constexpr size_t repetitions = 10001;
    const std::vector<PbpParityEquation> equations(repetitions, equation);
    PbpParityReasonPlan repeated;
    assert(proof.plan_pbp_parity_reason(equations, reason, 2, 2, cache,
                                        repeated));
    assert(repeated.proof_constraints == 3 && !repeated.helper_variables);
    assert(proof.add_prevalidated_pbp_parity_reason(
        2, equations, reason, 2, cache, repeated));
    proof.flush();

    std::string expected = "pol 11";
    expected.reserve(2 * repetitions * 6 + 64);
    for (size_t i = 1; i < repetitions; ++i)
      expected += " 11 +";
    expected += " ~x1 + 2 d ;\npol 16";
    for (size_t i = 1; i < repetitions; ++i)
      expected += " 16 +";
    expected += " x2 + 2 d ;\npol 20 21 + ;\n";
    assert(expected.size() > 65536);
    const std::string complete = read_file(path);
    expect(complete.substr(prefix.size()), expected,
           "streaming PBP parity text crosses the bounded flush boundary");
  }
  std::remove(path.c_str());
}

static void test_all_formats_report_output_failure() {
  if (access("/dev/full", W_OK)) {
    std::puts("SKIP: full-device output failure checks (/dev/full unavailable)");
    return;
  }
  for (auto format : {ProofFormat::DsrBinary, ProofFormat::DsrAscii,
                      ProofFormat::PbpStreaming})
    for (bool explicit_flush : {false, true}) {
      const pid_t child = fork();
      assert(child >= 0);
      if (!child) {
        const struct rlimit no_core = {0, 0};
        assert(setrlimit(RLIMIT_CORE, &no_core) == 0);
#ifdef __linux__
        assert(prctl(PR_SET_DUMPABLE, 0) == 0);
#endif
        {
          Proof failed;
          assert(format == ProofFormat::PbpStreaming
                     ? failed.open("/dev/full", format)
                     : failed.open_dsr_writer("/dev/full", format));
          failed.finish_input_transaction();
          failed.add_empty(1);
          if (explicit_flush)
            failed.flush();
        }
        _exit(0); // neither explicit nor destructor publication may succeed
      }
      int status = 0;
      assert(waitpid(child, &status, 0) == child);
      assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
    }
}

int main() {
  test_all_formats_report_output_failure();
  test_startup_pbp_certificate_boundary();
  test_streaming_pbp();
  test_startup_pb_without_a_proof();
  test_streaming_pbp_does_not_archive_divergent_binary_ids();
  test_streaming_pbp_parity_text_buffer_boundary();
  test_binary_add();
  test_binary_varint_boundary();
  test_binary_delete();
  test_binary_empty_clause();
  test_ascii_lines();
  test_dsr_binary();
  test_dsr_ascii();
  test_dsr_unit_deletion_transaction();
  test_dsr_deletion_spill();
  test_dsr_substitution();
  test_dsr_pivot_mapping_forms();
  test_external_variable_map();
  test_proof_only_external_clauses();
  test_derived_unit();
  test_tautological();
  test_unopened_proof_is_silent();
#ifdef TACTIX_ENABLE_PROOF_SHADOW
  test_population_membership_uses_exact_drat_multiset();
  test_population_backs_streaming_pbp_external_rat();
#endif
  printf("test_proof: all proof-backend serializer cases passed\n");
  return 0;
}
