// Streaming-PBP exact deletion: the proof-side identity of every non-binary
// constraint survives anonymous (id-0) additions, input restatement and
// lazily published root units, so deletions stay compact `del id` lines.
// Proof bytes are checked exactly; kernel cases also pin solver identities.
#undef NDEBUG
#include "temporary_file.hpp"
#include "frontend/parse.hpp"
#include "kernel/kernel.hpp"
#include "proof/proof.hpp"
#include "rules/bve/bve.hpp"
#include "rules/lucky/refutation_proof_sink.hpp"
#include "rules/registry.hpp"

#include <cassert>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace tactix;

namespace {

std::string read_all(const std::string &path) {
  FILE *f = std::fopen(path.c_str(), "rb");
  assert(f);
  std::string out;
  char buf[4096];
  size_t n;
  while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
    out.append(buf, n);
  std::fclose(f);
  return out;
}

void expect(const std::string &got, const std::string &want,
            const char *what) {
  if (got == want)
    return;
  std::fprintf(stderr, "FAIL %s\n--- got ---\n%s--- want ---\n%s", what,
               got.c_str(), want.c_str());
  std::abort();
}

constexpr const char *UNSAT_TAIL = "rup >= 1 ;\n"
                                   "output NONE ;\n"
                                   "conclusion UNSAT : -1 ;\n"
                                   "end pseudo-Boolean proof ;\n";

std::vector<Lit> clause(std::initializer_list<int> external) {
  std::vector<Lit> lits;
  for (const int literal : external)
    lits.push_back(internalize(literal));
  return lits;
}

size_t count_lines(const std::string &text, const char *prefix) {
  size_t count = 0, at = 0;
  const size_t width = std::strlen(prefix);
  while (at < text.size()) {
    const size_t end = text.find('\n', at);
    if (!text.compare(at, width, prefix))
      ++count;
    if (end == std::string::npos)
      break;
    at = end + 1;
  }
  return count;
}

// `del spec` lines naming a constraint of width >= 3 (a lost exact identity).
size_t wide_specifications(const std::string &text) {
  size_t count = 0, at = 0;
  while (at < text.size()) {
    size_t end = text.find('\n', at);
    if (end == std::string::npos)
      end = text.size();
    if (!text.compare(at, 9, "del spec ")) {
      size_t literals = 0;
      for (size_t i = at; i < end; ++i)
        literals += text[i] == 'x';
      count += literals >= 3;
    }
    at = end + 1;
  }
  return count;
}

// (b) Anonymous constraints: exact literal-set registry, newest copy first.
void test_anonymous_registry() {
  TemporaryFile file;
  {
    Proof p;
    assert(p.open(file.path.c_str(), ProofFormat::PbpStreaming));
    const auto input = clause({9, 10, 11});
    p.seed_population(1, input.data(), 3);
    p.finish_input_transaction();
    const auto a = clause({1, 2, 3});
    const auto a_permuted = clause({3, 1, 2});
    const auto unit = clause({4});
    const auto tracked = clause({5, 6, 7});
    p.add_rup(0, a.data(), 3, nullptr, 0);         // 2
    p.add_rup(0, a_permuted.data(), 3, nullptr, 0); // 3
    p.add_rup(0, unit.data(), 1, nullptr, 0);      // 4
    p.add_rup(40, tracked.data(), 3, nullptr, 0);  // 5
    p.del(50, 0, a.data(), 3);                     // newest copy: 3
    p.del(51, 77, a_permuted.data(), 3);           // untracked id: 2
    p.del(52, 0, unit.data(), 1);                  // 4
    p.del(53, 40, tracked.data(), 3);              // tracked: 5
    p.del(54, 1, input.data(), 3);                 // input prefix: 1
    const auto &c = p.pbp_deletion_counters();
    assert(c.by_id == 5 && c.by_anonymous == 3 && !c.abandon_reason);
    assert(c.anonymous_registered == 3);
    p.add_empty(0, nullptr, 0);
  }
  expect(read_all(file.path),
         std::string("pseudo-Boolean proof version 3.0\n"
                     "f 1;\n"
                     "rup 1 x1 1 x2 1 x3 >= 1 ;\n"
                     "rup 1 x3 1 x1 1 x2 >= 1 ;\n"
                     "rup 1 x4 >= 1 ;\n"
                     "rup 1 x5 1 x6 1 x7 >= 1 ;\n"
                     "del id 3 ;\n"
                     "del id 2 ;\n"
                     "del id 4 ;\n"
                     "del id 5 ;\n"
                     "del id 1 ;\n") +
             UNSAT_TAIL,
         "anonymous registry");
}

// (b) A miss still abandons: a later tracked deletion falls back to the
// specification, and the registry never offers a different literal set.
void test_anonymous_registry_miss_abandons() {
  TemporaryFile file;
  {
    Proof p;
    assert(p.open(file.path.c_str(), ProofFormat::PbpStreaming));
    p.finish_input_transaction();
    const auto a = clause({1, 2, 3});
    const auto b = clause({1, 2, 4});
    const auto tracked = clause({5, 6, 7});
    p.add_rup(0, a.data(), 3, nullptr, 0);        // 1
    p.add_rup(30, tracked.data(), 3, nullptr, 0); // 2
    p.del(40, 0, b.data(), 3);                    // never added: spec
    p.del(41, 30, tracked.data(), 3);             // abandoned: spec
    p.del(42, 0, a.data(), 3);
    const auto &c = p.pbp_deletion_counters();
    assert(c.by_id == 0 && c.spec_wide == 3 && c.abandon_reason == 1 &&
           c.abandon_width == 3);
    p.add_empty(0, nullptr, 0);
  }
  expect(read_all(file.path),
         std::string("pseudo-Boolean proof version 3.0\n"
                     "f 0;\n"
                     "rup 1 x1 1 x2 1 x3 >= 1 ;\n"
                     "rup 1 x5 1 x6 1 x7 >= 1 ;\n"
                     "del spec 1 x1 1 x2 1 x4 >= 1 ;\n"
                     "del spec 1 x5 1 x6 1 x7 >= 1 ;\n"
                     "del spec 1 x1 1 x2 1 x3 >= 1 ;\n") +
             UNSAT_TAIL,
         "anonymous registry miss");
}

// (c) Input restatement reuses the raw clause's id. The raw original is
// deleted by its exact input constraint and the id then names the
// replacement; ID deletion stays enabled. Covers a wide replacement, a unit
// replacing a repeated raw binary, and an identity prefix that continues.
void test_restatement_rebinds_identity() {
  TemporaryFile file;
  {
    Proof p;
    assert(p.open(file.path.c_str(), ProofFormat::PbpStreaming));
    const auto raw_wide = clause({1, 2, 2, 3});
    const auto wide = clause({1, 2, 3});
    const auto raw_binary = clause({4, 4});
    const auto unit = clause({4});
    const auto later = clause({5, 6, 7});
    p.seed_population(1, raw_wide.data(), 4);
    p.add_rup(1, wide.data(), 3, nullptr, 0);
    p.del(2, 1, raw_wide.data(), 4);
    p.seed_population(2, raw_binary.data(), 2);
    p.add_rup(2, unit.data(), 1, nullptr, 0);
    p.del(3, 2, raw_binary.data(), 2);
    p.seed_population(3, later.data(), 3);
    p.finish_input_transaction();
    p.del(10, 1, wide.data(), 3);   // the replacement: f + 1
    p.del(11, 3, later.data(), 3);  // the prefix continues: 3
    p.del(12, 2, unit.data(), 1);   // f + 2
    const auto &c = p.pbp_deletion_counters();
    assert(c.rebinds_recorded == 2 && c.by_rebind == 2 && c.by_id == 5 &&
           !c.abandon_reason && !c.spec_unit && !c.spec_wide);
    p.add_empty(0, nullptr, 0);
  }
  expect(read_all(file.path),
         std::string("pseudo-Boolean proof version 3.0\n"
                     "f 3;\n"
                     "rup 1 x1 1 x2 1 x3 >= 1 ;\n"
                     "del id 1 ;\n"
                     "rup 1 x4 >= 1 ;\n"
                     "del id 2 ;\n"
                     "del id 4 ;\n"
                     "del id 3 ;\n"
                     "del id 5 ;\n") +
             UNSAT_TAIL,
         "restatement rebind");
}

// (c) Divergent input ids (an earlier derived unit consumed one) use the
// explicit input map; the rebind names the raw clause's input ordinal.
void test_restatement_rebinds_divergent_identity() {
  TemporaryFile file;
  {
    Proof p;
    assert(p.open(file.path.c_str(), ProofFormat::PbpStreaming));
    const auto first = clause({9});
    const auto derived = clause({8});
    const auto raw = clause({1, -2, 3, 3});
    const auto replacement = clause({1, -2, 3});
    p.seed_population(1, first.data(), 1);
    p.add_derived_unit(2, derived[0], nullptr, 0);
    p.seed_population(3, raw.data(), 4);
    p.add_rup(3, replacement.data(), 3, nullptr, 0);
    p.del(4, 3, raw.data(), 4);
    p.finish_input_transaction();
    p.del(10, 3, replacement.data(), 3); // f + 2
    p.del(11, 2, derived.data(), 1);     // f + 1
    const auto &c = p.pbp_deletion_counters();
    assert(c.by_rebind == 1 && c.by_id == 3 && !c.abandon_reason);
    p.add_empty(0, nullptr, 0);
  }
  expect(read_all(file.path),
         std::string("pseudo-Boolean proof version 3.0\n"
                     "f 2;\n"
                     "rup 1 x8 >= 1 ;\n"
                     "rup 1 x1 1 ~x2 1 x3 >= 1 ;\n"
                     "del id 2 ;\n"
                     "del id 4 ;\n"
                     "del id 3 ;\n") +
             UNSAT_TAIL,
         "divergent restatement rebind");
}

// (c) An unresolved restatement at the end of input abandons exact deletion.
void test_unresolved_restatement_abandons() {
  TemporaryFile file;
  {
    Proof p;
    assert(p.open(file.path.c_str(), ProofFormat::PbpStreaming));
    const auto raw = clause({1, 1, 2, 3});
    const auto replacement = clause({1, 2, 3});
    p.seed_population(1, raw.data(), 4);
    p.add_rup(1, replacement.data(), 3, nullptr, 0);
    p.finish_input_transaction();
    assert(p.pbp_deletion_counters().abandon_reason == 5);
    p.del(5, 1, replacement.data(), 3);
    p.add_empty(0, nullptr, 0);
  }
  expect(read_all(file.path),
         std::string("pseudo-Boolean proof version 3.0\n"
                     "f 1;\n"
                     "rup 1 x1 1 x2 1 x3 >= 1 ;\n"
                     "del spec 1 x1 1 x2 1 x3 >= 1 ;\n") +
             UNSAT_TAIL,
         "unresolved restatement");
}

// (d) Staged root units: dropped before the empty clause, written in order
// before any other event, and written when the stream ends unfinished.
void test_deferred_root_units() {
  {
    TemporaryFile file;
    {
      Proof p;
      assert(p.open(file.path.c_str(), ProofFormat::PbpStreaming));
      p.finish_input_transaction();
      p.begin_deferred_root_units();
      p.add_derived_unit(1, internalize(2), nullptr, 0);
      p.add_derived_unit(2, internalize(-3), nullptr, 0);
      p.end_deferred_root_units();
      p.add_empty(3, nullptr, 0);
      const auto &c = p.pbp_deletion_counters();
      assert(c.deferred_units == 2 && c.dropped_units == 2 &&
             c.dropped_batches == 1);
    }
    expect(read_all(file.path),
           std::string("pseudo-Boolean proof version 3.0\nf 0;\n") +
               UNSAT_TAIL,
           "dropped root units");
  }
  {
    TemporaryFile file;
    {
      Proof p;
      assert(p.open(file.path.c_str(), ProofFormat::PbpStreaming));
      p.finish_input_transaction();
      const auto lemma = clause({4, 5, 6});
      p.add_rup(7, lemma.data(), 3, nullptr, 0);
      p.begin_deferred_root_units();
      p.add_derived_unit(8, internalize(2), nullptr, 0);
      p.add_derived_unit(9, internalize(-3), nullptr, 0);
      p.end_deferred_root_units();
      p.del(10, 7, lemma.data(), 3);
      p.del(11, 9, clause({-3}).data(), 1);
      p.add_empty(12, nullptr, 0);
      assert(!p.pbp_deletion_counters().dropped_units);
    }
    expect(read_all(file.path),
           std::string("pseudo-Boolean proof version 3.0\nf 0;\n"
                       "rup 1 x4 1 x5 1 x6 >= 1 ;\n"
                       "rup 1 x2 >= 1 ;\n"
                       "rup 1 ~x3 >= 1 ;\n"
                       "del id 1 ;\n"
                       "del id 3 ;\n") +
               UNSAT_TAIL,
           "written root units");
  }
  {
    TemporaryFile file;
    {
      Proof p;
      assert(p.open(file.path.c_str(), ProofFormat::PbpStreaming));
      p.finish_input_transaction();
      p.begin_deferred_root_units();
      p.add_derived_unit(1, internalize(2), nullptr, 0);
      p.end_deferred_root_units();
    }
    expect(read_all(file.path),
           "pseudo-Boolean proof version 3.0\nf 0;\nrup 1 x2 >= 1 ;\n",
           "unfinished stream keeps root units");
  }
  // Other formats ignore the staging calls byte for byte.
  for (const ProofFormat format :
       {ProofFormat::DsrAscii, ProofFormat::DratAscii, ProofFormat::Lrat}) {
    std::string bytes[2];
    for (int staged = 0; staged < 2; ++staged) {
      TemporaryFile file;
      {
        Proof p;
        assert(p.open(file.path.c_str(), format));
        p.finish_input_transaction();
        const uint64_t hint = 1;
        if (staged)
          p.begin_deferred_root_units();
        p.add_derived_unit(2, internalize(2), &hint, 1);
        if (staged)
          p.end_deferred_root_units();
        const uint64_t chain[] = {2, 1};
        p.add_empty(3, chain, 2);
      }
      bytes[staged] = read_all(file.path);
    }
    expect(bytes[1], bytes[0], "non-PBP root units unaffected");
    assert(!bytes[0].empty());
  }
}

// A repeated binary canonicalizes to a unit, so it is never a binary
// specification that could match a tracked unit constraint.
void test_repeated_binary_is_exact() {
  TemporaryFile file;
  {
    Proof p;
    assert(p.open(file.path.c_str(), ProofFormat::PbpStreaming));
    const auto raw = clause({3, 3});
    p.seed_population(1, raw.data(), 2);
    p.finish_input_transaction();
    p.del(2, 1, raw.data(), 2);
    assert(p.pbp_deletion_counters().by_id == 1);
    p.add_empty(0, nullptr, 0);
  }
  expect(read_all(file.path),
         std::string("pseudo-Boolean proof version 3.0\nf 1;\n"
                     "del id 1 ;\n") +
             UNSAT_TAIL,
         "repeated binary");
}

// After the input identities diverge, a raw repeated binary is still tracked:
// it canonicalizes to a unit, so its deletion is exact and keeps ID deletion.
void test_repeated_binary_after_divergence() {
  TemporaryFile file;
  {
    Proof p;
    assert(p.open(file.path.c_str(), ProofFormat::PbpStreaming));
    const auto first = clause({9});
    const auto derived = clause({8});
    const auto raw = clause({3, 3});
    p.seed_population(1, first.data(), 1);
    p.add_derived_unit(2, derived[0], nullptr, 0); // splits the namespaces
    p.seed_population(3, raw.data(), 2);
    p.finish_input_transaction();
    p.del(4, 3, raw.data(), 2);
    const auto &c = p.pbp_deletion_counters();
    assert(c.by_id == 1 && !c.spec_unit && !c.abandon_reason);
    p.add_empty(0, nullptr, 0);
  }
  expect(read_all(file.path),
         std::string("pseudo-Boolean proof version 3.0\n"
                     "f 2;\n"
                     "rup 1 x8 >= 1 ;\n"
                     "del id 2 ;\n") +
             UNSAT_TAIL,
         "repeated binary after divergence");
}

// A stream whose ID deletion is disabled (a proof attached after replayed
// input) deletes even tracked and input identities by specification.
void test_disabled_id_deletions() {
  TemporaryFile file;
  {
    Proof p;
    assert(p.open(file.path.c_str(), ProofFormat::PbpStreaming));
    const auto input = clause({1, 2, 3});
    p.seed_population(1, input.data(), 3);
    p.finish_input_transaction();
    p.disable_pbp_id_deletions();
    const auto lemma = clause({4, 5, 6});
    p.add_rup(5, lemma.data(), 3, nullptr, 0);
    p.del(6, 5, lemma.data(), 3);
    p.del(7, 1, input.data(), 3);
    const auto &c = p.pbp_deletion_counters();
    assert(!c.by_id && c.spec_wide == 2 && c.abandon_reason == 7);
    p.add_empty(0, nullptr, 0);
  }
  expect(read_all(file.path),
         std::string("pseudo-Boolean proof version 3.0\n"
                     "f 1;\n"
                     "rup 1 x4 1 x5 1 x6 >= 1 ;\n"
                     "del spec 1 x4 1 x5 1 x6 >= 1 ;\n"
                     "del spec 1 x1 1 x2 1 x3 >= 1 ;\n") +
             UNSAT_TAIL,
         "disabled id deletions");
}

// (a) A Lucky refutation sink erases its own additions by the ids they were
// added with, so a terminal certificate keeps ID deletion.
void test_refutation_sink_erases_by_id() {
  TemporaryFile file;
  {
    Kernel k;
    Proof p;
    assert(p.open(file.path.c_str(), ProofFormat::PbpStreaming));
    p.finish_input_transaction();
    k.proof = &p;
    uint64_t interrupted = 0;
    RefutationProofSink<std::vector<int32_t>> sink(k, true, 3, 100,
                                                   interrupted);
    assert(sink.add({1, 2, 3}));
    assert(sink.add({-1, 2, 3}));
    assert(sink.erase({3, 1, 2}));
    assert(sink.erase({-1, 2, 3}));
    const auto &c = p.pbp_deletion_counters();
    assert(c.by_id == 2 && !c.spec_wide && !c.abandon_reason);
    p.add_empty(0, nullptr, 0);
    k.proof = nullptr;
  }
  expect(read_all(file.path),
         std::string("pseudo-Boolean proof version 3.0\n"
                     "f 0;\n"
                     "rup 1 x1 1 x2 1 x3 >= 1 ;\n"
                     "rup 1 ~x1 1 x2 1 x3 >= 1 ;\n"
                     "del id 1 ;\n"
                     "del id 2 ;\n") +
             UNSAT_TAIL,
         "refutation sink");
}

// (d) Root units held back by a root conflict are written out first when an
// event other than the empty clause follows. pbp_deletion_identity.sh checks
// this CNF and proof with VeriPB and CakePB.
void write_written_back_units(const std::string &directory) {
  FILE *cnf = std::fopen((directory + "/writeback.cnf").c_str(), "w");
  assert(cnf);
  std::fputs("p cnf 3 4\n1 2 0\n1 -2 0\n-1 3 0\n-1 -3 0\n", cnf);
  assert(!std::fclose(cnf));
  const std::vector<std::vector<Lit>> input = {
      clause({1, 2}), clause({1, -2}), clause({-1, 3}), clause({-1, -3})};
  {
    Proof p;
    assert(p.open((directory + "/writeback.pbp").c_str(),
                  ProofFormat::PbpStreaming));
    for (size_t i = 0; i < input.size(); ++i)
      p.seed_population(i + 1, input[i].data(), 2);
    p.finish_input_transaction();
    p.begin_deferred_root_units();
    p.add_derived_unit(5, internalize(1), nullptr, 0);
    p.add_derived_unit(6, internalize(3), nullptr, 0);
    p.end_deferred_root_units();
    p.del(7, 2, input[1].data(), 2); // writes the held units first
    p.add_empty(8, nullptr, 0);
    assert(!p.pbp_deletion_counters().dropped_units);
  }
  expect(read_all(directory + "/writeback.pbp"),
         std::string("pseudo-Boolean proof version 3.0\n"
                     "f 4;\n"
                     "rup 1 x1 >= 1 ;\n"
                     "rup 1 x3 >= 1 ;\n"
                     "del spec 1 x1 1 ~x2 >= 1 ;\n") +
             UNSAT_TAIL,
         "written-back root units");
}

// Kernel: parse with streaming PBP and the same formula under DSR.
struct Parsed {
  std::string proof;
  uint64_t next_clause_id = 0;
  bool inconsistent = false;
  Proof::PbpDeletionCounters counters;
};

Parsed parse_with(const char *cnf, ProofFormat format) {
  TemporaryFile proof_file;
  FILE *input = std::tmpfile();
  assert(input);
  std::fputs(cnf, input);
  std::rewind(input);
  Parsed result;
  {
    Kernel k;
    Proof proof;
    assert(proof.open(proof_file.path.c_str(), format));
    proof.bind_variable_map(&k.internal_to_external);
    k.proof = &proof;
    assert(!parse_dimacs(input, k));
    result.next_clause_id = k.next_clause_id;
    result.inconsistent = k.inconsistent;
    result.counters = proof.pbp_deletion_counters();
    proof.flush();
    k.proof = nullptr;
  }
  std::fclose(input);
  result.proof = read_all(proof_file.path);
  return result;
}

// (d) A conflicting root propagation during input: no root unit is written
// before the empty clause, while solver ids advance exactly as on DSR.
void test_root_conflict_publishes_no_units() {
  const char *cnf = "p cnf 4 5\n-1 2 0\n-2 3 0\n-3 4 0\n-4 -1 0\n1 0\n";
  const Parsed pbp = parse_with(cnf, ProofFormat::PbpStreaming);
  const Parsed dsr = parse_with(cnf, ProofFormat::DsrAscii);
  assert(pbp.inconsistent && dsr.inconsistent);
  assert(pbp.next_clause_id == dsr.next_clause_id &&
         "staging must not change solver clause ids");
  assert(pbp.counters.deferred_units >= 2 &&
         pbp.counters.dropped_units == pbp.counters.deferred_units);
  expect(pbp.proof,
         std::string("pseudo-Boolean proof version 3.0\nf 5;\n") + UNSAT_TAIL,
         "root conflict proof");
  assert(count_lines(dsr.proof, "0") == 1 && dsr.proof.size() > 4 &&
         "DSR still publishes the root units");
}

// (c) Real input restatement through the parser: repeated literals and a
// root-false literal. Both raw originals are deleted by exact input id and ID
// deletion is never abandoned.
void test_parser_restatement_keeps_ids() {
  const char *cnf = "p cnf 6 4\n1 2 2 3 0\n4 0\n-4 5 6 1 0\n2 3 5 0\n";
  const Parsed pbp = parse_with(cnf, ProofFormat::PbpStreaming);
  assert(!pbp.inconsistent);
  assert(pbp.counters.rebinds_recorded == 2 && pbp.counters.by_rebind == 2 &&
         !pbp.counters.abandon_reason);
  expect(pbp.proof,
         "pseudo-Boolean proof version 3.0\nf 4;\n"
         "rup 1 x1 1 x2 1 x3 >= 1 ;\n"
         "del id 1 ;\n"
         "rup 1 x5 1 x6 1 x1 >= 1 ;\n"
         "del id 3 ;\n",
         "parser restatement");
}

// (a) BVE definition: the lifted one-sided refutation's transient lemmas
// (width >= 3 here) are retired by their tracked ids.
void test_bve_definition_transients_by_id() {
  TemporaryFile proof_file;
  Kernel k;
  Proof proof;
  assert(proof.open(proof_file.path.c_str(), ProofFormat::PbpStreaming));
  k.proof = &proof;
  proof.finish_input_transaction();
  kernel_reserve_vars(k, 5); // x=1, a,b,c=2..4, opposite antecedent 5
  for (int mask = 0; mask < 8; ++mask) {
    Lit lits[4] = {internalize(1), internalize(mask & 1 ? -2 : 2),
                   internalize(mask & 2 ? -3 : 3),
                   internalize(mask & 4 ? -4 : 4)};
    kernel_add_large_clause(k, lits, 4, false, 0, ++k.next_clause_id);
  }
  kernel_add_binary_clause(k, internalize(-1), internalize(5),
                           ++k.next_clause_id);
  k.bve_gate_mask = BVE_GATE_DEFINITION;
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  assert(vt.apply_instance(k, lit_instance(internalize(1))) ==
         ApplyResult::Ok);
  assert(k.stats.bve_definition_units == 1);
  proof.flush();
  const std::string text = read_all(proof_file.path);
  const auto &c = proof.pbp_deletion_counters();
  const size_t lemmas = count_lines(text, "rup ");
  const size_t deletions = count_lines(text, "del ");
  std::printf("bve definition: %zu lemmas, %zu deletions, %" PRIu64
              " by id\n", lemmas, deletions, c.by_id);
  assert(lemmas >= 2 && deletions >= lemmas - 1 &&
         "every lemma but the unit is a retired transient");
  assert(!wide_specifications(text) && !c.abandon_reason && c.by_id > 0 &&
         c.by_id + c.spec_binary + c.spec_unit == deletions);
  k.proof = nullptr;
}

} // namespace

int main(int argc, char **argv) {
  if (argc == 2) {
    write_written_back_units(argv[1]);
    return 0;
  }
  test_anonymous_registry();
  test_anonymous_registry_miss_abandons();
  test_restatement_rebinds_identity();
  test_restatement_rebinds_divergent_identity();
  test_unresolved_restatement_abandons();
  test_deferred_root_units();
  test_repeated_binary_is_exact();
  test_repeated_binary_after_divergence();
  test_disabled_id_deletions();
  test_refutation_sink_erases_by_id();
  test_root_conflict_publishes_no_units();
  test_parser_restatement_keeps_ids();
  test_bve_definition_transients_by_id();
  std::printf("test_pbp_deletion_identity: all cases passed\n");
  return 0;
}
