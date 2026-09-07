// Fixed three-clause diagnostic: proof derivation is not the clausal-core API.
// Build against the repository SubSolver and the unmodified reference library.
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

static void core_clause(void *, bool learned, size_t size, const unsigned *lits) {
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
  assert(sub.derivation(derivation));
  for (const auto &step : derivation.steps)
    print_clause("T proof-step", step.lits.size(), step.lits.data());
  uint64_t learned;
  kitten_compute_clausal_core(kit, &learned);
  kitten_traverse_core_clauses(kit, nullptr, core_clause);
  // This is an intentional API comparison, not an assertion of core equality.
  kissat_release(host);
}
