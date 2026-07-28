// V5 Sweep · S-a: the embedded sub-solver, checked against BRUTE FORCE.
//
// The engine's whole job is to answer "is this environment refutable under
// these assumptions" correctly; there is no proof to check yet (S-b), so an
// exhaustive oracle is the only thing that can hold it honest. Every random
// formula is solved by enumerating all 2^n assignments and compared verdict for
// verdict, under randomly chosen assumption sets as well as without — the
// assumption path is where a CDCL engine most often goes wrong (a falsified
// assumption must end the solve, not be learned around), and it is the ONLY
// path Sweep will ever use it on.
//
// argv[1] overrides the iteration count (default 100000, ~0.2 s release /
// ~4 s debug; the pre-commit soak runs 1000000).
#undef NDEBUG
#include "rules/sweep/subsolver.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

using namespace tactix;

namespace {

struct Formula {
  uint32_t nvars = 0;
  std::vector<std::vector<SubLit>> clauses;
};

bool satisfied_by(const Formula &f, uint32_t bits) {
  for (const auto &c : f.clauses) {
    bool ok = false;
    for (const SubLit l : c) {
      const bool value = (bits >> sub_var_of(l)) & 1u;
      if (value != ((l & 1u) != 0)) { // positive literal true iff bit set
        ok = true;
        break;
      }
    }
    if (!ok)
      return false;
  }
  return true;
}

// exhaustive: is `f` satisfiable with every assumption literal true?
bool brute_force(const Formula &f, const std::vector<SubLit> &assumptions) {
  for (uint32_t bits = 0; bits < (1u << f.nvars); bits++) {
    bool ok = true;
    for (const SubLit a : assumptions) {
      const bool value = (bits >> sub_var_of(a)) & 1u;
      if (value == ((a & 1u) != 0)) {
        ok = false;
        break;
      }
    }
    if (ok && satisfied_by(f, bits))
      return true;
  }
  return false;
}

Formula random_formula(std::mt19937 &rng, uint32_t nvars, uint32_t nclauses,
                       uint32_t maxsize) {
  Formula f;
  f.nvars = nvars;
  for (uint32_t i = 0; i < nclauses; i++) {
    const uint32_t size = 1 + rng() % maxsize;
    std::vector<SubLit> c;
    for (uint32_t j = 0; j < size; j++)
      c.push_back(sub_lit(rng() % nvars, rng() & 1u));
    f.clauses.push_back(std::move(c));
  }
  return f;
}

void load(SubSolver &s, const Formula &f) {
  s.reset(f.nvars);
  for (const auto &c : f.clauses)
    s.add_clause(c);
}

// THE ORACLE. Random formulas at a density that straddles the phase
// transition, half of them with assumptions, every verdict checked against
// exhaustive enumeration and every reported model re-checked against the
// clauses (a wrong SAT with a bogus model is the failure fuzzing would miss).
void test_against_brute_force(uint32_t iterations) {
  std::mt19937 rng(12345);
  SubSolver s;
  uint32_t sat = 0, unsat = 0, with_assumptions = 0;
  for (uint32_t it = 0; it < iterations; it++) {
    const uint32_t nvars = 2 + rng() % 7;             // 2..8
    const uint32_t nclauses = 1 + rng() % (4 * nvars); // straddles the threshold
    const Formula f = random_formula(rng, nvars, nclauses, 3);
    std::vector<SubLit> assumptions;
    if (rng() & 1u) {
      const uint32_t n = rng() % 3;
      for (uint32_t i = 0; i < n; i++)
        assumptions.push_back(sub_lit(rng() % nvars, rng() & 1u));
      if (!assumptions.empty())
        with_assumptions++;
    }
    load(s, f);
    const SubResult r = s.solve(assumptions, /*unlimited=*/0);
    assert(r != SubResult::Budget); // unlimited: it must decide
    const bool expected = brute_force(f, assumptions);
    if (r == SubResult::Sat) {
      sat++;
      assert(expected && "reported SAT on an unsatisfiable environment");
      // the model must actually satisfy the formula AND the assumptions
      uint32_t bits = 0;
      for (SubVar v = 0; v < f.nvars; v++)
        if (s.model()[v] > 0)
          bits |= 1u << v;
      assert(satisfied_by(f, bits) && "the reported model is not a model");
      for (const SubLit a : assumptions) {
        const bool value = (bits >> sub_var_of(a)) & 1u;
        assert(value != ((a & 1u) != 0) && "the model breaks an assumption");
      }
    } else {
      unsat++;
      assert(!expected && "reported UNSAT on a satisfiable environment");
    }
  }
  printf("  brute force: %u iterations, %u SAT / %u UNSAT, %u with assumptions\n",
         iterations, sat, unsat, with_assumptions);
}

// Contradictory assumptions are refuted without touching the formula, and an
// assumption falsified by propagation ends the solve as Unsat rather than being
// learned around.
void test_assumption_paths() {
  SubSolver s;
  s.reset(3);
  s.add_clause({sub_lit(0, false), sub_lit(1, false)}); // (x0 v x1)
  assert(s.solve({sub_lit(0, true), sub_lit(1, true)}, 0) == SubResult::Unsat);
  // the same formula is satisfiable without the assumptions
  load(s, {});
  s.reset(3);
  s.add_clause({sub_lit(0, false), sub_lit(1, false)});
  assert(s.solve({}, 0) == SubResult::Sat);
  // directly contradictory assumptions
  s.reset(3);
  assert(s.solve({sub_lit(0, false), sub_lit(0, true)}, 0) == SubResult::Unsat);
  // an assumption that is already implied is not a contradiction
  s.reset(2);
  s.add_clause({sub_lit(0, false)}); // unit x0
  assert(s.solve({sub_lit(0, false)}, 0) == SubResult::Sat);
  assert(s.solve({sub_lit(0, true)}, 0) == SubResult::Unsat);
}

// Degenerate inputs the environment extraction (S-c) can hand it.
void test_degenerate() {
  SubSolver s;
  s.reset(0);
  assert(s.solve({}, 0) == SubResult::Sat); // no variables: trivially SAT
  s.reset(2);
  assert(s.solve({}, 0) == SubResult::Sat); // no clauses
  s.reset(2);
  s.add_clause({sub_lit(0, false), sub_lit(0, true)}); // tautology: dropped
  assert(s.solve({}, 0) == SubResult::Sat);
  s.reset(2);
  s.add_clause({sub_lit(0, false), sub_lit(0, false)}); // duplicate: a unit
  assert(s.solve({sub_lit(0, true)}, 0) == SubResult::Unsat);
  s.reset(2);
  s.add_clause({sub_lit(0, false)});
  s.add_clause({sub_lit(0, true)}); // directly inconsistent
  assert(s.solve({}, 0) == SubResult::Unsat);
}

// The budget is the contract with the effort scheduler: exceeding it yields
// Budget — never a guessed verdict — and the tick count is deterministic.
void test_budget_and_determinism() {
  // pigeonhole PHP(6,5): small to state, expensive to refute
  const uint32_t holes = 5, pigeons = 6;
  const auto var = [&](uint32_t p, uint32_t h) { return p * holes + h; };
  SubSolver s;
  const auto build = [&] {
    s.reset(pigeons * holes);
    for (uint32_t p = 0; p < pigeons; p++) { // every pigeon in some hole
      std::vector<SubLit> c;
      for (uint32_t h = 0; h < holes; h++)
        c.push_back(sub_lit(var(p, h), false));
      s.add_clause(c);
    }
    for (uint32_t h = 0; h < holes; h++) // no two pigeons share a hole
      for (uint32_t p = 0; p < pigeons; p++)
        for (uint32_t q = p + 1; q < pigeons; q++)
          s.add_clause({sub_lit(var(p, h), true), sub_lit(var(q, h), true)});
  };
  build();
  assert(s.solve({}, 0) == SubResult::Unsat);
  const uint64_t full = s.ticks();
  assert(full > 0);
  build(); // same formula, same budget => same answer and same tick count
  assert(s.solve({}, 0) == SubResult::Unsat);
  assert(s.ticks() == full && "the engine must be deterministic");
  build();
  const SubResult starved = s.solve({}, /*ticks=*/50);
  assert(starved == SubResult::Budget && "a starved solve must not conclude");
}

} // namespace

int main(int argc, char **argv) {
  const uint32_t iterations =
      argc > 1 ? static_cast<uint32_t>(std::atoi(argv[1])) : 100000;
  test_degenerate();
  test_assumption_paths();
  test_budget_and_determinism();
  test_against_brute_force(iterations);
  printf("test_subsolver: all passed\n");
  return 0;
}
