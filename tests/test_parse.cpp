// DIMACS ingestion regressions: stable clause storage and the decision seed
// both preserve the file-literal order used by Kissat's default `tumble=1`.
#undef NDEBUG
#include "frontend/parse.hpp"
#include "policies/heuristics.hpp"
#include "rules/decide/decide.hpp"

#include <cassert>
#include <cstdio>
#include <vector>

using namespace tactix;

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

  // Canonical Decide walks the same queue backwards, so this is the exact
  // initial external-variable prefix: 7,4,6,2,1,3,5.
  Lit candidates[7];
  assert(decide_candidates(k, candidates, 7) == 7);
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

int main() {
  test_tumble_preserves_file_literal_order();
  test_root_units_normalize_later_input_clauses();
  puts("test_parse OK");
  return 0;
}
