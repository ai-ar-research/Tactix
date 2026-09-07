// Fixed differential check of the isolated 5316 ordered-core observer.
// Build against its SubSolver and the pristine Kissat library.
#include "engine/subsolver.hpp"
extern "C" {
#include "kissat.h"
#include "kitten.h"
}
#include <cassert>
#include <cstdio>
#include <vector>

static void print_clause(const char *kind, size_t size, const unsigned *lits) {
  std::printf("%s", kind);
  for (size_t i = 0; i < size; ++i)
    std::printf(" %d", (lits[i] & 1u) ? -int(lits[i] / 2 + 1) : int(lits[i] / 2 + 1));
  std::puts(" 0");
}

static void core_clause(void *state, bool learned, size_t size, const unsigned *lits) {
  auto &rows = *static_cast<std::vector<tactix::SubDerivation::CoreClause> *>(state);
  rows.push_back({0, learned, std::vector<unsigned>(lits, lits + size)});
  print_clause(learned ? "K learned" : "K original", size, lits);
}

int main() {
  // x, (!x or y), (!y or z): no search decision is needed in either solver.
  const std::vector<std::vector<unsigned>> formula{{0}, {1, 2}, {3, 4}};
  tactix::SubSolver sub;
  sub.start_kitten_session();
  sub.reset(3, tactix::SubSearchMode::Kitten);
  kissat *host = kissat_init();
  kitten *kit = kitten_embedded(host);
  kitten_track_antecedents(kit);
  for (auto clause : formula) {
    sub.add_clause(clause);
    kitten_clause(kit, clause.size(), clause.data());
  }
  sub.randomize_phases();
  kitten_randomize_phases(kit);
  assert(sub.solve({}, 0, tactix::SubTickMode::Kitten) == tactix::SubResult::Sat);
  assert(kitten_solve(kit) == 10);
  for (unsigned lit : {0u, 2u, 4u})
    assert(sub.fixed_literal(lit) == kitten_fixed(kit, lit));
  sub.randomize_phases();
  kitten_randomize_phases(kit);
  assert(sub.solve({5}, 0, tactix::SubTickMode::Kitten) == tactix::SubResult::Unsat);
  kitten_assume(kit, 5);
  assert(kitten_solve(kit) == 20);
  tactix::SubDerivation derivation;
  assert(sub.derivation(derivation, true));
  for (const auto &cc : derivation.core_clauses)
    print_clause(cc.learned ? "T learned" : "T original", cc.lits.size(), cc.lits.data());
  for (const auto &step : derivation.steps)
    print_clause("T proof-step", step.lits.size(), step.lits.data());
  uint64_t learned;
  kitten_compute_clausal_core(kit, &learned);
  std::vector<tactix::SubDerivation::CoreClause> expected;
  kitten_traverse_core_clauses(kit, &expected, core_clause);
  assert(expected.size() == derivation.core_clauses.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    assert(expected[i].learned == derivation.core_clauses[i].learned);
    assert(expected[i].lits == derivation.core_clauses[i].lits);
  }
  std::puts("all ordered core rows and learned/original tags match");
  kissat_release(host);
}
