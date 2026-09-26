// The proof backend (src/proof/proof.cpp): DSR, DRAT, and native-LRAT
// serializers checked byte for byte. N2 rests on the encoding, which an
// end-to-end checker cannot distinguish from a wrong lemma.
#undef NDEBUG
#include "proof/proof.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <sys/prctl.h>
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

// Each case emits, closes the file, and compares the whole byte stream.
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
// Encoding (drat-trim's): 'a'/'d' tag, then per literal u = (|e|<<1)|(e<0) as
// low-first 7-bit chunks, high bit on all but the last, then a 0 terminator.
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
  // pin both sides of the `u >= 128` boundary: 63 fits one chunk, 64 needs two
  const Lit small[1] = {make_lit(62, false)}; // +63
  const Lit big[1] = {make_lit(63, false)};   // +64
  expect(emitted("bin-small", ProofFormat::DratBinary,
                 [&](Proof &p) { p.add_rup(1, small, 1, nullptr, 0); }),
         std::string("a\x7e\x00", 3), "drat-binary varint 1 byte");
  expect(emitted("bin-big", ProofFormat::DratBinary,
                 [&](Proof &p) { p.add_rup(1, big, 1, nullptr, 0); }),
         std::string("a\x80\x01\x00", 4), "drat-binary varint 2 bytes");
  // the sign bit rides in bit 0 of the same varint, so -64 differs from +64
  const Lit negbig[1] = {make_lit(63, true)}; // -64 -> u = 129
  expect(emitted("bin-negbig", ProofFormat::DratBinary,
                 [&](Proof &p) { p.add_rup(1, negbig, 1, nullptr, 0); }),
         std::string("a\x81\x01\x00", 4), "drat-binary negative varint");
}

void test_drat_binary_delete() {
  const Lit lits[2] = {make_lit(0, true), make_lit(4, false)};
  expect(emitted("bin-del", ProofFormat::DratBinary,
                 [&](Proof &p) { p.del(9, 3, lits, 2); }),
         std::string("d\x03\x0a\x00", 4), "drat-binary del");
}

void test_drat_binary_empty_clause() {
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
  // 'd' takes a separating space in ascii and none in binary: the one place the
  // two DRAT paths diverge
  expect(emitted("asc-del", ProofFormat::DratAscii,
                 [&](Proof &p) { p.del(6, 5, lits, 2); }),
         "d 1 -3 0\n", "drat-ascii del");
  expect(emitted("asc-empty", ProofFormat::DratAscii,
                 [&](Proof &p) { p.add_empty(7, nullptr, 0); }),
         "0\n", "drat-ascii add_empty");
}

// --- DSR -------------------------------------------------------------------
// Canonical binary DSR uses unprintable line tags 1/2 rather than the
// CaDiCaL-compatible DRAT tags 'a'/'d'. Witnessless additions otherwise share
// the literal encoding.
void test_dsr_binary() {
  const Lit lits[3] = {make_lit(0, false), make_lit(1, true),
                       make_lit(2, false)};
  expect(emitted("dsr-bin-add", ProofFormat::DsrBinary,
                 [&](Proof &p) { p.add_rup(7, lits, 3, nullptr, 0); }),
         std::string("\x01\x02\x05\x06\x00", 5), "dsr-binary add_rup");
  expect(emitted("dsr-bin-rat", ProofFormat::DsrBinary,
                 [&](Proof &p) { p.add_rat(8, lits, 3, nullptr, 0); }),
         std::string("\x01\x02\x05\x06\x00", 5), "dsr-binary add_rat");
  expect(emitted("dsr-bin-del", ProofFormat::DsrBinary,
                 [&](Proof &p) { p.del(9, 7, lits, 3); }),
         std::string("\x02\x02\x05\x06\x00", 5), "dsr-binary del");
  expect(emitted("dsr-bin-empty", ProofFormat::DsrBinary,
                 [&](Proof &p) { p.add_empty(10, nullptr, 0); }),
         std::string("\x01\x00", 2), "dsr-binary add_empty");
}

void test_dsr_ascii() {
  const Lit lits[2] = {make_lit(0, false), make_lit(2, true)};
  expect(emitted("dsr-asc-add", ProofFormat::DsrAscii,
                 [&](Proof &p) { p.add_rup(5, lits, 2, nullptr, 0); }),
         "1 -3 0\n", "dsr-ascii add_rup");
  expect(emitted("dsr-asc-del", ProofFormat::DsrAscii,
                 [&](Proof &p) { p.del(6, 5, lits, 2); }),
         "d 1 -3 0\n", "dsr-ascii del");
  const uint64_t ignored_chain[2] = {7, 8};
  expect(emitted("dsr-asc-root-unit", ProofFormat::DsrAscii,
                 [&](Proof &p) {
                   p.add_derived_unit(9, lits[0], ignored_chain, 2);
                 }),
         "1 0\n", "dsr-ascii derived root unit");
}

void test_dsr_unit_deletion_transaction() {
  const auto lit = [](int e) { return make_lit(std::abs(e) - 1, e < 0); };
  const Lit unit = lit(1);
  const Lit support[2] = {lit(2), lit(-3)};
  const Lit support_reordered[2] = {lit(-3), lit(2)};
  const Lit unrelated[2] = {lit(4), lit(5)};
  const std::vector<std::vector<Lit>> cone{{support[0], support[1]}};
  const auto emit = [&](Proof &p) {
    p.add_rup(10, &unit, 1, nullptr, 0, &cone);
    p.del(11, 7, support_reordered, 2);
    p.del(12, 8, unrelated, 2);
    p.flush();
  };
  expect(emitted("dsr-transaction-ascii", ProofFormat::DsrAscii, emit),
         "d 4 5 0\n1 0\nd -3 2 0\n",
         "DSR preserves a root unit's exact RUP support across deletions");
  expect(emitted("dsr-transaction-binary", ProofFormat::DsrBinary, emit),
         std::string("\x02\x08\x0a\x00\x01\x02\x00\x02\x07\x04\x00", 11),
         "binary DSR preserves the same support order");
  const std::vector<Var> map{5, 6, 7, 8, 9};
  expect(emitted("dsr-transaction-mapped", ProofFormat::DsrAscii,
                 [&](Proof &p) {
                   p.bind_variable_map(&map);
                   emit(p);
                 }),
         "d 9 10 0\n6 0\nd -8 7 0\n",
         "DSR support identity survives external variable mapping");
  const Lit next = lit(6);
  expect(emitted("dsr-transaction-next-add", ProofFormat::DsrAscii,
                 [&](Proof &p) {
                   p.add_rup(10, &unit, 1, nullptr, 0, &cone);
                   p.del(11, 7, support, 2);
                   p.add_rup(12, &next, 1, nullptr, 0);
                 }),
         "1 0\nd 2 -3 0\n6 0\n",
         "next addition closes the pending unit transaction");
  expect(emitted("dsr-transaction-delete-unit", ProofFormat::DsrAscii,
                 [&](Proof &p) {
                   p.add_rup(10, &unit, 1, nullptr, 0, &cone);
                   p.del(11, 10, &unit, 1);
                 }),
         "1 0\nd 1 0\n",
         "a pending unit is published before its own deletion");
  const Lit open_pair[2] = {lit(1), lit(2)};
  const DsrLiteralMapping open_maps[2] = {
      {lit(1), lit(3)}, {lit(2), lit(4)}};
  expect(emitted("dsr-transaction-declined-sr", ProofFormat::DsrAscii,
                 [&](Proof &p) {
                   p.add_rup(10, &unit, 1, nullptr, 0, &cone);
                   assert(!p.add_sr(11, open_pair, 2, nullptr, 0, open_maps, 2,
                                    INT32_MAX));
                   p.del(12, 8, unrelated, 2);
                 }),
         "d 4 5 0\n1 0\n",
         "a declined SR addition does not close a pending unit transaction");
  const int32_t tautology[2] = {6, -6};
  expect(emitted("dsr-transaction-declined-external", ProofFormat::DsrAscii,
                 [&](Proof &p) {
                   p.add_rup(10, &unit, 1, nullptr, 0, &cone);
                   assert(!p.add_external_rup(11, tautology, 2));
                   p.del(12, 8, unrelated, 2);
                 }),
         "d 4 5 0\n1 0\n",
         "a declined external addition preserves the pending unit order");
  const DsrLiteralMapping rup_maps[2] = {
      {lit(1), lit(2)}, {lit(2), lit(1)}};
  expect(emitted("dsr-transaction-accepted-sr", ProofFormat::DsrAscii,
                 [&](Proof &p) {
                   p.add_rup(10, &unit, 1, nullptr, 0, &cone);
                   assert(p.add_sr(11, open_pair, 2, nullptr, 0, rup_maps, 2,
                                   6));
                 }),
         "1 0\n1 2 0\n",
         "an accepted SR addition closes the pending unit transaction");
}

void test_dsr_substitution() {
  const Lit clause[2] = {make_lit(0, false), make_lit(1, true)};
  const Lit true_lits[1] = {make_lit(2, false)};
  const DsrLiteralMapping mappings[1] = {
      {make_lit(3, false), make_lit(4, true)}};

  expect(emitted("dsr-asc-sr", ProofFormat::DsrAscii, [&](Proof &p) {
           assert(p.add_sr(11, clause, 2, true_lits, 1, mappings, 1, 5));
         }),
         "1 -2 1 3 1 4 -5 0\n", "dsr-ascii add_sr");
  expect(emitted("dsr-bin-sr", ProofFormat::DsrBinary, [&](Proof &p) {
           assert(p.add_sr(11, clause, 2, true_lits, 1, mappings, 1, 5));
         }),
         std::string("\x01\x02\x05\x02\x06\x02\x08\x0b\x00", 9),
         "dsr-binary add_sr");

  for (const ProofFormat format : {ProofFormat::DratBinary,
                                   ProofFormat::DratAscii,
                                   ProofFormat::Lrat})
    expect(emitted("incompatible-sr", format, [&](Proof &p) {
             assert(!p.add_sr(11, clause, 2, true_lits, 1, mappings, 1, 5));
           }),
           "", "add_sr rejects compatibility format");

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
  expect(emitted("mapped", ProofFormat::DratAscii, [&](Proof &p) {
           p.bind_variable_map(&map);
           p.add_rup(1, lits, 2, nullptr, 0);
         }),
         "5 -2 0\n", "stable external variable map");
}

void test_proof_only_external_clauses() {
  const int32_t gate[3] = {70001, -2, 4};
  const int32_t unit[1] = {-70001};
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
  expect(emitted("external-lrat", ProofFormat::Lrat, [&](Proof &p) {
           assert(!p.add_external_rat(1, gate, 3));
           assert(!p.add_external_rup(2, unit, 1));
           assert(!p.del_external(3, 1, gate, 3));
         }), "", "LRAT declines proof-only extension clauses atomically");
}

// --- native LRAT ------------------------------------------------------------
void test_lrat_add_rup() {
  const Lit lits[2] = {make_lit(0, false), make_lit(1, true)};
  const uint64_t chain[3] = {4, 2, 9};
  // `<id> <lits> 0 <chain> 0`; a reordered chain is a rejected proof
  expect(emitted("lrat-add", ProofFormat::Lrat,
                 [&](Proof &p) { p.add_rup(12, lits, 2, chain, 3); }),
         "12 1 -2 0 4 2 9 0\n", "lrat add_rup");
}

void test_lrat_add_rat() {
  const Lit lits[2] = {make_lit(2, false), make_lit(0, true)};
  // pivot first; hints are one flat signed stream (negative = resolution
  // partner) and the sign is verbatim
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
  // a root-level derived unit is explicit in LRAT (later chains cite its id)
  expect(emitted("lrat-unit", ProofFormat::Lrat,
                 [&](Proof &p) {
                   p.add_derived_unit(31, make_lit(3, true), chain, 2);
                 }),
         "31 -4 0 1 2 0\n", "lrat add_derived_unit");
  // ... and implied in DRAT: emitting it would be a wrong lemma line
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
  expect(emitted("dsr-bin-unit", ProofFormat::DsrBinary,
                 [&](Proof &p) {
                   p.add_derived_unit(31, make_lit(3, true), chain, 2);
                 }),
         std::string("\x01\x09\x00", 3),
         "dsr-binary add_derived_unit is explicit");
  expect(emitted("dsr-asc-unit", ProofFormat::DsrAscii,
                 [&](Proof &p) {
                   p.add_derived_unit(31, make_lit(3, true), chain, 2);
                 }),
         "-4 0\n", "dsr-ascii add_derived_unit is explicit");
}

void test_lrat_delete() {
  const Lit lits[2] = {make_lit(0, false), make_lit(1, false)};
  // LRAT deletes by id and needs its own line id; the literals are ignored
  expect(emitted("lrat-del", ProofFormat::Lrat,
                 [&](Proof &p) { p.del(40, 17, lits, 2); }),
         "40 d 17 0\n", "lrat del");
}

void test_lrat_multi_line_stream() {
  const Lit a[1] = {make_lit(0, false)};
  const Lit b[2] = {make_lit(1, false), make_lit(0, true)};
  const uint64_t c1[1] = {1};
  const uint64_t c2[2] = {2, 3};
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
  assert(p.open(path.c_str(), ProofFormat::DsrAscii));
  p.add_rup(2, normalized, 2, nullptr, 0);
  p.del(2, 1, raw, 3);
  assert(!p.population_contains(1, raw, 3));
  assert(p.population_contains(2, normalized, 2));
  std::remove(path.c_str());
  p.arm_population_shadow();
  assert(p.population_shadow());
}
#endif

} // namespace

void test_automatic_certificate_boundary() {
  const Lit unit = internalize(1);
  const auto path = tmp_path("auto");
  {
    Proof p;
    assert(p.open(path.c_str(), ProofFormat::Automatic));
    assert(!p.startup_pb_available());
    p.add_rup(1, &unit, 1, nullptr, 0);
    assert(read_all(path).empty()); // input prefix is private
    p.finish_input_transaction();
    assert(p.startup_pb_available());
    FILE *out = p.begin_pb();
    assert(out && p.pb() && !p.startup_pb_available());
    std::fputs("pseudo-Boolean proof version 3.0\n", out);
    assert(p.finish_pb());
    assert(!p.commit_dsr());
    assert(!p.begin_pb());
  }
  expect(read_all(path), "pseudo-Boolean proof version 3.0\n", "atomic PB selection");
  {
    Proof p;
    assert(p.open(path.c_str(), ProofFormat::Automatic));
    p.add_rup(1, &unit, 1, nullptr, 0);
    p.finish_input_transaction();
    p.add_empty(2, nullptr, 0);
    assert(!p.startup_pb_available() && !p.begin_pb());
    p.flush();
  }
  expect(read_all(path), std::string("\1\2\0\1\0", 5), "atomic DSR fallback");
  std::remove(path.c_str());
  Proof strict;
  assert(strict.open(path.c_str(), ProofFormat::DsrAscii));
  strict.finish_input_transaction();
  assert(!strict.startup_pb_available() && !strict.begin_pb());
  std::remove(path.c_str());
  // A completed derivation is not a successful publication on a full device.
  const pid_t pb_child = fork();
  assert(pb_child >= 0);
  if (!pb_child) {
    Proof failed_output;
    assert(failed_output.open("/dev/full", ProofFormat::Automatic));
    failed_output.finish_input_transaction();
    FILE *full = failed_output.begin_pb();
    assert(full);
    std::fputs("pseudo-Boolean proof version 3.0\n", full);
    assert(!failed_output.finish_pb());
    _exit(0); // failure was reported; do not retry publication in the destructor
  }
  int pb_status = 0;
  assert(waitpid(pb_child, &pb_status, 0) == pb_child);
  assert(WIFEXITED(pb_status) && !WEXITSTATUS(pb_status));
  const pid_t child = fork();
  assert(child >= 0);
  if (!child) {
    const struct rlimit no_core = {0, 0};
    assert(setrlimit(RLIMIT_CORE, &no_core) == 0);
    assert(prctl(PR_SET_DUMPABLE, 0) == 0);
    Proof failed_dsr;
    assert(failed_dsr.open("/dev/full", ProofFormat::Automatic));
    failed_dsr.add_empty(1, nullptr, 0);
    failed_dsr.finish_input_transaction();
    failed_dsr.commit_dsr();
    failed_dsr.flush();
    _exit(0); // the regression used to report successful publication
  }
  int status = 0;
  assert(waitpid(child, &status, 0) == child);
  assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
}

static void test_all_formats_report_output_failure() {
  for (auto format : {ProofFormat::DsrBinary, ProofFormat::DsrAscii,
                      ProofFormat::DratBinary, ProofFormat::DratAscii,
                      ProofFormat::Lrat, ProofFormat::Automatic})
    for (bool explicit_flush : {false, true}) {
      const pid_t child = fork();
      assert(child >= 0);
      if (!child) {
        const struct rlimit no_core = {0, 0};
        assert(setrlimit(RLIMIT_CORE, &no_core) == 0);
        assert(prctl(PR_SET_DUMPABLE, 0) == 0);
        {
          Proof failed;
          assert(failed.open("/dev/full", format));
          failed.finish_input_transaction();
          failed.add_empty(1, nullptr, 0);
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
  test_automatic_certificate_boundary();
  test_drat_binary_add();
  test_drat_binary_varint_boundary();
  test_drat_binary_delete();
  test_drat_binary_empty_clause();
  test_drat_ascii();
  test_dsr_binary();
  test_dsr_ascii();
  test_dsr_unit_deletion_transaction();
  test_dsr_substitution();
  test_dsr_pivot_mapping_forms();
  test_external_variable_map();
  test_proof_only_external_clauses();
  test_lrat_add_rup();
  test_lrat_add_rat();
  test_lrat_empty_and_unit();
  test_lrat_delete();
  test_lrat_multi_line_stream();
  test_tautological();
  test_unopened_proof_is_silent();
#ifdef TACTIX_ENABLE_PROOF_SHADOW
  test_population_membership_uses_exact_drat_multiset();
#endif
  printf("test_proof: all proof-backend serializer cases passed\n");
  return 0;
}
