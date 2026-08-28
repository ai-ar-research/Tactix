// DIMACS ingestion regressions: stable clause storage and the decision seed
// both preserve the file-literal order used by Kissat's default `tumble=1`.
#undef NDEBUG
#include "frontend/parse.hpp"
#include "policies/heuristics.hpp"
#include "proof/proof.hpp"
#include "rules/decide/decide.hpp"

#include <cassert>
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
  // The advisory clause count deliberately underpredicts the 20 distinct
  // binaries so this crosses the filter's initial 16-entry capacity. Every
  // pair is then repeated: the filter may answer "maybe", but the watch scan
  // must reject each duplicate exactly.
  assert(std::fputs("p cnf 10 1\n", file) >= 0);
  for (int a = 1, emitted = 0; a <= 10 && emitted < 20; a++)
    for (int b = a + 1; b <= 10 && emitted < 20; b++, emitted++)
      assert(std::fprintf(file, "%d %d 0\n", a, b) > 0);
  for (int a = 1, emitted = 0; a <= 10 && emitted < 20; a++)
    for (int b = a + 1; b <= 10 && emitted < 20; b++, emitted++)
      assert(std::fprintf(file, "%d %d 0\n", a, b) > 0);
  std::rewind(file);
  assert(parse_dimacs(file, k) == nullptr);
  std::fclose(file);
  assert(!k.binary_ids.tracks_keys());
  assert(k.binary_ids.size() == 20);
  for (int a = 1, emitted = 0; a <= 10 && emitted < 20; a++)
    for (int b = a + 1; b <= 10 && emitted < 20; b++, emitted++)
      assert(kernel_binary_copies(k, internalize(a), internalize(b)) == 1);
  kernel_check_invariants(k);
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
  const std::vector<Var> expected_queue = {4, 2, 0, 1, 5, 3, 6};
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

int main() {
  test_compact_input_filter_grows_and_deduplicates_exactly();
  test_native_lrat_retains_binary_ids();
  test_tumble_preserves_file_literal_order();
  test_root_units_normalize_later_input_clauses();
  test_input_units_propagate_before_later_clauses();
  test_header_only_variables_are_not_search_variables();
  puts("test_parse OK");
  return 0;
}
