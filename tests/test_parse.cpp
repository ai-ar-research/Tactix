// DIMACS ingestion regressions: canonical clause storage must not erase the
// file-literal order used by Kissat's default `tumble=1` decision seed.
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
  // occurrence of 2 is already active.  Storage is still sorted separately.
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

  // Tumble is an ordering side effect only.  The first stored large clause
  // remains sorted/deduplicated for the kernel's set-based representation.
  const Clause *clause = clause_at(k.arena, 0);
  assert(clause->size == 3);
  assert(clause->lits[0] == internalize(1));
  assert(clause->lits[1] == internalize(2));
  assert(clause->lits[2] == internalize(3));
  kernel_check_invariants(k);
}

int main() {
  test_tumble_preserves_file_literal_order();
  puts("test_parse OK");
  return 0;
}
