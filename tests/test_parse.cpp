// DIMACS ingestion regressions: stable clause storage and the decision seed
// both preserve the file-literal order used by Kissat's default `tumble=1`.
#undef NDEBUG
#include "frontend/parse.hpp"
#include "policies/heuristics.hpp"
#include "proof/proof.hpp"
#include "rules/decide/decide.hpp"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <map>
#include <sstream>
#include <cstdio>
#include <string>
#include <vector>

#include <unistd.h>

using namespace tactix;

static FILE *dimacs_file(const char *text) {
  FILE *file = std::tmpfile();
  assert(file);
  assert(std::fputs(text, file) >= 0);
  std::rewind(file);
  return file;
}

static void test_compact_input_filter_grows_and_deduplicates_exactly() {
  Kernel k;
  FILE *file = std::tmpfile();
  assert(file);
  // The advisory clause count deliberately underpredicts the 150 distinct
  // binaries so this crosses the filter's initial one-block (128-entry)
  // capacity. Every pair is then repeated: the filter may answer "maybe", but
  // the watch scan must reject each duplicate exactly.
  constexpr int vars = 20, distinct = 150;
  assert(std::fputs("p cnf 20 1\n", file) >= 0);
  for (int round = 0; round < 2; round++)
    for (int a = 1, emitted = 0; a <= vars && emitted < distinct; a++)
      for (int b = a + 1; b <= vars && emitted < distinct; b++, emitted++)
        assert(std::fprintf(file, "%d %d 0\n", a, b) > 0);
  std::rewind(file);
  assert(parse_dimacs(file, k) == nullptr);
  std::fclose(file);
  assert(!k.binary_ids.tracks_keys());
  assert(k.binary_ids.size() == distinct);
  for (int a = 1, emitted = 0; a <= vars && emitted < distinct; a++)
    for (int b = a + 1; b <= vars && emitted < distinct; b++, emitted++)
      assert(kernel_binary_copies(k, internalize(a), internalize(b)) == 1);
  kernel_check_invariants(k);
}

// One string per imported formula: variable population, root values, every
// binary (from the mirrored watch lists), every large clause in arena order,
// the activation seed, and the normalization count.
static std::string import_fingerprint(Kernel &k) {
  std::string out = "n=" + std::to_string(k.nvars) + ";";
  for (Var v = 0; v < k.nvars; v++)
    out += std::to_string(static_cast<int>(k.values[make_lit(v, false)]));
  out += ";b:";
  for (Lit l = 0; l < static_cast<Lit>(2 * k.nvars); l++)
    for (const WatchEntry w : k.watches.entries(l))
      if (w.binary() && l < w.other())
        out += std::to_string(l) + "," + std::to_string(w.other()) + " ";
  out += ";c:";
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    for (const Lit lit : *c)
      out += std::to_string(lit) + " ";
    out += "|";
    ref += clause_actual_words(c);
  }
  out += ";a:";
  for (const Var v : k.input_activation_order)
    out += std::to_string(v) + " ";
  out += ";r=" + std::to_string(k.stats.input_restated);
  return out;
}

static std::string parse_with_block(const char *text, size_t block,
                                    const char **error = nullptr) {
  Kernel k;
  k.tumble = true;
  FILE *file = dimacs_file(text);
  const char *err = parse_dimacs_with_block(file, k, block);
  std::fclose(file);
  if (error)
    *error = err;
  return err ? std::string("error:") + err : import_fingerprint(k);
}

static void test_advisory_headers_preserve_binary_liveness() {
  const char *canonical =
      "p cnf 3 6\n1 2 0\n2 3 0\n1 2 0\n2 3 0\n-1 3 0\n-1 3 0\n";
  // The parser deliberately accepts advisory/late headers. The temporary
  // exact index must not outlive its literal universe, and re-arming must
  // retain all previous binaries, including the pairs repeated afterwards.
  const char *variants[] = {
      "p cnf 2 6\n1 2 0\n2 3 0\n1 2 0\n2 3 0\n-1 3 0\n-1 3 0\n",
      "p cnf 2 6\n1 2 0\np cnf 3 6\n2 3 0\n1 2 0\n2 3 0\n-1 3 0\n-1 3 0\n",
      "p cnf 3 6\n1 2 0\np cnf 3 6\n2 3 0\n1 2 0\n2 3 0\n-1 3 0\n-1 3 0\n",
      "1 2 0\np cnf 3 6\n2 3 0\n1 2 0\n2 3 0\n-1 3 0\n-1 3 0\n",
  };
  const std::string expected = parse_with_block(canonical, size_t{1} << 20);
  for (const char *text : variants) {
    assert(parse_with_block(text, size_t{1} << 20) == expected);
    for (size_t block = 1; block <= 40; ++block)
      assert(parse_with_block(text, block) == expected);
    Kernel k;
    FILE *file = dimacs_file(text);
    assert(!parse_dimacs(file, k));
    std::fclose(file);
    assert(!k.binary_liveness); // the import scope must release its index
    assert(k.binary_ids.size() == 3);
    assert(kernel_binary_copies(k, internalize(1), internalize(2)) == 1);
    assert(kernel_binary_copies(k, internalize(2), internalize(3)) == 1);
    assert(kernel_binary_copies(k, internalize(-1), internalize(3)) == 1);
    kernel_check_invariants(k);
  }
}

// The block scanner must not care where a token, a line, or a comment ends
// relative to a block boundary: every block size imports the same formula
// as the production block, and the same malformed input yields the same
// error text.
static void test_block_boundaries_do_not_change_the_import() {
  const char *text =
      "c leading comment\r\n"
      "p cnf 12 9\r\n"
      "1 -2\t3 0\n"
      "c mid comment with digits 4 5 6 0\n"
      "  -4   5\r\n"
      "6 0\n"
      "7 7 -8 0\n"
      "-9 9 10 0\n"
      "11 0\n"
      "-11 12 0\n"
      "\n\n"
      "1 2 3 4 5 6 7 8 9 10 11 12 0\n"
      "10 -12"; // missing final zero and newline
  const std::string reference = parse_with_block(text, size_t{1} << 20);
  assert(reference.find("n=12;") == 0);
  assert(reference.find(";r=") != std::string::npos);
  for (size_t block = 1; block <= 40; block++)
    assert(parse_with_block(text, block) == reference);
  for (size_t block : {41u, 64u, 100u, 255u, 256u, 1000u, 4096u})
    assert(parse_with_block(text, block) == reference);

  struct Bad {
    const char *text;
    const char *error;
  };
  const Bad bad[] = {
      {"p cnf 2 1\n1 x 0\n", "expected literal"},
      {"p cnf 2 1\n1 -\n", "expected literal"},
      {"p cnf 2 1\n1 -", "expected literal"},
      {"p cnf 2 1\nabc 0\n", "expected literal"},
      {"p cnf 2 1\n1 999999999999 0\n", "literal out of range"},
      {"p cnf 999999999999 1\n1 0\n", "variable count out of range"},
      {"p cnf 2 1\n1 2z\n", "unexpected character after literal"},
  };
  for (const Bad &b : bad) {
    const char *err = nullptr;
    parse_with_block(b.text, size_t{1} << 20, &err);
    assert(err && std::string(err) == b.error);
    for (size_t block = 1; block <= 24; block++) {
      err = nullptr;
      parse_with_block(b.text, block, &err);
      assert(err && std::string(err) == b.error);
    }
  }
  // A header without digits, a '-' followed by zero, and a comment ending at
  // end of file are all accepted, at every block size.
  const char *odd = "p\n1 -0\n2 -3 0\nc trailing";
  const std::string odd_reference = parse_with_block(odd, size_t{1} << 20);
  assert(odd_reference.find("error:") == std::string::npos);
  for (size_t block = 1; block <= 30; block++)
    assert(parse_with_block(odd, block) == odd_reference);
}

static void test_native_lrat_retains_binary_ids() {
  Kernel k;
  const std::string path = "/tmp/tactix-test-parse-lrat-" +
                           std::to_string(static_cast<long>(getpid())) +
                           ".proof";
  {
    Proof proof;
    assert(proof.open(path.c_str(), ProofFormat::Lrat));
    k.proof = &proof;
    FILE *file = dimacs_file("p cnf 2 2\n1 -2 0\n1 -2 0\n");
    assert(parse_dimacs(file, k) == nullptr);
    std::fclose(file);
    assert(k.binary_ids.tracks_keys());
    assert(k.binary_ids.tracks_values());
    assert(k.binary_ids.size() == 1);
    assert(k.binary_ids.at(
               kernel_binary_key(internalize(1), internalize(-2))) == 1);
    k.proof = nullptr;
  }
  assert(std::remove(path.c_str()) == 0);
}

// Independently replay the literal-addressed import delta over the raw input.
// After shortening and binary deduplication, later SR must see exactly the
// represented clause population, including when a duplicate itself shrinks.
static void test_literal_proof_import_retires_raw_replacements() {
  const std::vector<std::vector<int>> raw = {
      {1}, {-1, 2, 2, 3}, {3, 2}, {2, -1, 3},
      {4, 5, 4, -1, 6}, {7, -1, 7}, {1, 8}, {9, -9}};
  std::ostringstream input;
  input << "p cnf 9 " << raw.size() << '\n';
  std::map<std::vector<int>, unsigned> initial_population;
  for (auto clause : raw) {
    for (int lit : clause)
      input << lit << ' ';
    input << "0\n";
    std::sort(clause.begin(), clause.end());
    ++initial_population[clause];
  }
  for (const ProofFormat format : {ProofFormat::DsrAscii,
                                   ProofFormat::DratAscii}) {
    const std::string path = "test-parse-population-" +
                             std::to_string(getpid()) +
                             (format == ProofFormat::DsrAscii ? ".dsr" : ".drat");
    std::string proved_state;
    {
      Kernel k;
      Proof proof;
      assert(proof.open(path.c_str(), format));
      k.proof = &proof;
      FILE *file = dimacs_file(input.str().c_str());
      assert(parse_dimacs(file, k) == nullptr);
      std::fclose(file);
      proved_state = import_fingerprint(k);
      kernel_check_invariants(k);
      k.proof = nullptr;
    }
    {
      Kernel k;
      FILE *file = dimacs_file(input.str().c_str());
      assert(parse_dimacs(file, k) == nullptr);
      std::fclose(file);
      assert(import_fingerprint(k) == proved_state);
    }
    std::map<std::vector<int>, unsigned> population = initial_population;
    std::ifstream proof(path);
    std::string line;
    while (std::getline(proof, line)) {
      std::istringstream record(line);
      const bool deletion = record.peek() == 'd';
      if (deletion)
        record.get();
      std::vector<int> clause;
      for (int lit; record >> lit && lit;)
        clause.push_back(lit);
      std::sort(clause.begin(), clause.end());
      if (deletion) {
        auto found = population.find(clause);
        assert(found != population.end() && found->second);
        if (!--found->second)
          population.erase(found);
      } else {
        ++population[clause];
      }
    }
    const std::map<std::vector<int>, unsigned> expected = {
        {{1}, 1}, {{2, 3}, 1}, {{4, 5, 6}, 1}, {{7}, 1}};
    assert(population == expected);
    assert(std::remove(path.c_str()) == 0);
  }
}

static void test_tumble_preserves_file_literal_order() {
  Kernel k;
  ScopedHeuristics hg(k);
  FILE *file = std::tmpfile();
  assert(file);
  // First surviving occurrences: 3,1,2,6,4,7.  The repeated 3 is inert,
  // the tautological second clause activates neither 5 nor 4, and the final
  // occurrence of 2 is already active.  Storage stably removes the repeated 3.
  const char *dimacs =
      "p cnf 7 4\n"
      "3 1 3 2 0\n"
      "5 -5 4 0\n"
      "6 4 0\n"
      "2 7 0\n";
  assert(std::fputs(dimacs, file) >= 0);
  std::rewind(file);
  assert(parse_dimacs(file, k) == nullptr);
  std::fclose(file);
  assert(!k.binary_ids.tracks_keys());
  assert(!k.binary_ids.tracks_values() &&
         "proofless/DRAT must release the import-only binary archive");
  assert(k.binary_ids.size() == 2 &&
         "dropping storage must preserve the exact material watermark");

  // Kissat imports variables before it recognizes a tautology, so 5 and 4
  // still occupy their first-occurrence positions even though that clause is
  // discarded.  Header-only variables would follow this imported prefix.
  const std::vector<Var> expected_import_order = {2, 0, 1, 4, 3, 5, 6};
  for (uint32_t rank = 0; rank < expected_import_order.size(); rank++)
    assert(kernel_import_var_order(k, expected_import_order[rank]) == rank);
  assert(kernel_import_lit_order(k, internalize(5)) + 1 ==
         kernel_import_lit_order(k, internalize(-5)));

  assert(k.stats.tumble_seeded == 6);
  const std::vector<Var> expected_queue = {2, 0, 1, 5, 3, 6};
  std::vector<Var> queue;
  for (Var v = k.h->queue_first; v != INVALID_VAR; v = k.h->links[v].next)
    queue.push_back(v);
  assert(queue == expected_queue);

  // The policy-ranked candidate surface walks the same queue backwards, so
  // this is the exact initial external-variable prefix: 7,4,6,2,1,3,5.
  Lit candidates[7];
  assert(heuristics_decide_candidates(k, candidates, 7) == 7);
  const int expected_candidates[7] = {7, 4, 6, 2, 1, 3, 5};
  for (unsigned i = 0; i < 7; i++)
    assert(externalize(candidates[i]) == expected_candidates[i]);

  // The first stored large clause matches Kissat's stable first-occurrence
  // order while retaining the kernel's set semantics.
  const Clause *clause = clause_at(k.arena, 0);
  assert(clause->size == 3);
  assert(clause->lits[0] == internalize(3));
  assert(clause->lits[1] == internalize(1));
  assert(clause->lits[2] == internalize(2));
  kernel_check_invariants(k);
}

static void test_root_units_normalize_later_input_clauses() {
  Kernel k;
  ScopedHeuristics hg(k);
  FILE *file = std::tmpfile();
  assert(file);
  const char *dimacs =
      "p cnf 7 5\n"
      "1 0\n"
      "-1 2 3 0\n"     // root-false -1: stored as binary 2 3
      "-1 4 5 6 0\n"   // root-false -1: stored as large 4 5 6
      "1 7 0\n"        // root-true 1: satisfied, activates no 7
      "2 4 0\n";
  assert(std::fputs(dimacs, file) >= 0);
  std::rewind(file);
  assert(parse_dimacs(file, k) == nullptr);
  std::fclose(file);

  assert(k.stats.input_restated == 2);
  assert(kernel_binary_id(k, internalize(2), internalize(3)) != 0);
  const Clause *clause = clause_at(k.arena, 0);
  assert(clause->size == 3);
  assert(clause->lits[0] == internalize(4));
  assert(clause->lits[1] == internalize(5));
  assert(clause->lits[2] == internalize(6));
  assert(k.stats.tumble_seeded == 6 &&
         "a root-satisfied clause must not activate its other variable");
  kernel_check_invariants(k);
}

static void test_input_units_propagate_before_later_clauses() {
  Kernel k;
  ScopedHeuristics hg(k);
  FILE *file = std::tmpfile();
  assert(file);
  const char *dimacs =
      "p cnf 6 6\n"
      "-1 2 0\n"       // stored before its forcing unit
      "-2 3 0\n"       // transitive binary consequence
      "1 0\n"          // must propagate x2 and x3 during ingestion
      "3 4 0\n"        // now root-satisfied and omitted
      "-3 5 6 0\n"     // now shrinks to the binary 5 6
      "4 5 0\n";
  assert(std::fputs(dimacs, file) >= 0);
  std::rewind(file);
  assert(parse_dimacs(file, k) == nullptr);
  std::fclose(file);

  assert(k.values[internalize(1)] > 0);
  assert(k.values[internalize(2)] > 0);
  assert(k.values[internalize(3)] > 0);
  assert(k.propagate_head == k.trail.size());
  assert(kernel_binary_live(k, internalize(5), internalize(6)));
  assert(k.stats.input_restated == 1);
  // Kissat routes original-unit closure through its search propagator: scan
  // 1 assigns 2, scan 2 assigns 3 (three ticks each), then scan 3 is empty.
  assert(k.stats.search_ticks == 7);
  // x4 first appears only in the satisfied clause and must not be activated
  // there, matching Kissat's add-at-root behavior.
  assert(!k.input_activated.size()); // finish_input has released the scratch
  kernel_check_invariants(k);
}

static void test_header_only_variables_are_not_search_variables() {
  Kernel k;
  ScopedHeuristics hg(k);
  FILE *file = std::tmpfile();
  assert(file);
  const char *dimacs =
      "p cnf 8 2\n"
      "1 3 0\n"
      "-3 1 0\n";
  assert(std::fputs(dimacs, file) >= 0);
  std::rewind(file);
  assert(parse_dimacs(file, k) == nullptr);
  std::fclose(file);

  assert(k.nvars == 8); // stable DIMACS identities remain available
  assert(k.unassigned == 8);
  assert(k.unassigned_active == 2);
  assert(k.var_status.size() == 8);
  for (const Var v : {Var{0}, Var{2}})
    assert(k.var_status[v] == VarStatus::Active);
  for (const Var v : {Var{1}, Var{3}, Var{4}, Var{5}, Var{6}, Var{7}}) {
    assert(k.var_status[v] == VarStatus::Eliminated);
    assert(!heuristics_var_active(k, v));
  }
  assert(kernel_num_output_vars(k) == 8);
  kernel_check_invariants(k);
}

static void test_discarded_only_variables_are_not_search_variables() {
  const char *dimacs =
      "p cnf 7 5\n"
      "1 -1 3 0\n" // x1/x3 occur only in a tautology
      "4 0\n"
      "4 5 0\n"    // x5 occurs only in a root-satisfied clause
      "2 6 0\n"
      "-2 6 0\n";
  for (const bool tumble : {true, false}) {
    Kernel k;
    k.tumble = tumble;
    ScopedHeuristics hg(k);
    FILE *file = dimacs_file(dimacs);
    assert(parse_dimacs(file, k) == nullptr);
    std::fclose(file);
    assert(k.values[internalize(4)] > 0);
    assert(k.unassigned_active == 2);
    for (const Var v : {Var{0}, Var{2}, Var{4}, Var{6}}) {
      assert(k.values[make_lit(v, false)] == 0);
      assert(kernel_var_retired(k, v));
      assert(!heuristics_var_active(k, v));
    }
    for (const Var v : {Var{1}, Var{5}})
      assert(!kernel_var_retired(k, v));
    if (tumble)
      assert(k.compact_import_order == std::vector<Var>({3, 1, 5}));
    assert(k.compact_import_order.size() >= k.unassigned_active);
    k.h->stable = true;
    heuristics_update_target_phases(k);
    Lit candidates[7] = {};
    const uint32_t count = decide_candidates(k, candidates, 7);
    assert(count == 6); // explicit N1 Decide still exposes retired variables
    assert(std::find(candidates, candidates + count, internalize(1)) !=
           candidates + count);
    kernel_check_invariants(k);
  }
}

int main() {
  test_compact_input_filter_grows_and_deduplicates_exactly();
  test_native_lrat_retains_binary_ids();
  test_literal_proof_import_retires_raw_replacements();
  test_tumble_preserves_file_literal_order();
  test_root_units_normalize_later_input_clauses();
  test_input_units_propagate_before_later_clauses();
  test_header_only_variables_are_not_search_variables();
  test_discarded_only_variables_are_not_search_variables();
  test_block_boundaries_do_not_change_the_import();
  test_advisory_headers_preserve_binary_liveness();
  puts("test_parse OK");
  return 0;
}
