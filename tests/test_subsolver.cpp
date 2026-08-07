// Sweep's embedded sub-solver and its refutation derivation. The solver is
// checked against brute force over all 2^n assignments, with and without the
// assumptions Sweep always uses. A derivation is checked three independent
// ways: `lrat_replay` replays every chain, `check_target_is_entailed` checks
// entailment by enumeration, and the core may not under-report what the chains
// cite. `--emit <dir>` writes the cases out for `sweep_proofs.sh`.
#undef NDEBUG
#include "engine/subsolver.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
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

// Clauses come out clean — no duplicate and no complementary literals — as the
// kernel guarantees for extracted environment clauses.
Formula random_formula(std::mt19937 &rng, uint32_t nvars, uint32_t nclauses,
                       uint32_t maxsize, uint32_t minsize = 1) {
  Formula f;
  f.nvars = nvars;
  for (uint32_t i = 0; i < nclauses; i++) {
    const uint32_t size =
        minsize + (maxsize > minsize ? rng() % (maxsize - minsize + 1) : 0);
    std::vector<SubLit> c;
    for (uint32_t j = 0; j < size; j++) {
      const SubLit l = sub_lit(rng() % nvars, rng() & 1u);
      bool clash = false;
      for (const SubLit o : c)
        if (o == l || o == sub_negate(l))
          clash = true;
      if (!clash)
        c.push_back(l);
    }
    f.clauses.push_back(std::move(c));
  }
  return f;
}

void load(SubSolver &s, const Formula &f) {
  s.reset(f.nvars);
  for (const auto &c : f.clauses)
    s.add_clause(c);
}

// The oracle: random formulas straddling the phase transition, half with
// assumptions, every verdict checked against enumeration and every reported
// model re-checked against the clauses.
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
      // the model must satisfy the formula and the assumptions
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

// Contradictory assumptions are refuted without touching the formula, and one
// falsified by propagation ends the solve as Unsat rather than being learned
// around.
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

// Degenerate inputs the environment extraction can hand it.
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

// The contract with the effort scheduler: exceeding the budget yields Budget,
// never a guessed verdict, and the tick count is deterministic.
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

// --- the refutation derivation ---------------------------------------------

// A complete LRAT replay, so every derivation is checked the way cake_lpr would
// check it. `clauses` maps id -> literals and grows as steps are accepted.
bool lrat_replay(std::map<uint32_t, std::vector<SubLit>> &clauses,
                 const SubStep &step) {
  std::map<SubLit, int8_t> value;
  const auto val = [&](SubLit l) {
    const auto it = value.find(l);
    return it == value.end() ? int8_t{0} : it->second;
  };
  const auto set_true = [&](SubLit l) {
    value[l] = 1;
    value[sub_negate(l)] = -1;
  };
  for (const SubLit l : step.lits)
    set_true(sub_negate(l)); // assume the lemma false
  for (size_t i = 0; i < step.chain.size(); i++) {
    const auto it = clauses.find(step.chain[i]);
    if (it == clauses.end())
      return false; // cites a clause that does not exist (or not yet)
    SubLit unassigned = 0;
    uint32_t open = 0;
    for (const SubLit l : it->second) {
      if (val(l) > 0)
        return false; // a satisfied hint propagates nothing: reject
      if (val(l) == 0) {
        unassigned = l;
        open++;
      }
    }
    if (open == 0)                          // conflicting: the chain ends here
      return i + 1 == step.chain.size();
    if (open > 1)
      return false; // not unit: the chain is out of order or incomplete
    set_true(unassigned);
  }
  return false; // ran out of chain without reaching a conflict
}

// Every literal of `target` must negate an assumption and every model of the
// environment must satisfy `target`: the semantic half, independent of replay.
void check_target_is_entailed(const Formula &f, const std::vector<SubLit> &assumptions,
                              const std::vector<SubLit> &target) {
  for (const SubLit t : target) {
    bool found = false;
    for (const SubLit a : assumptions)
      if (sub_negate(a) == t)
        found = true;
    assert(found && "the target names something that was not assumed");
  }
  for (uint32_t bits = 0; bits < (1u << f.nvars); bits++) {
    if (!satisfied_by(f, bits))
      continue;
    bool ok = false;
    for (const SubLit t : target) {
      const bool value = (bits >> sub_var_of(t)) & 1u;
      if (value != ((t & 1u) != 0))
        ok = true;
    }
    assert(ok && "a model of the environment falsifies the derived clause");
  }
}

// The derivation oracle: random refutations, each replayed as LRAT, checked for
// entailment by enumeration, and checked for core discipline — every chain
// entry is a core original or an earlier lemma, so the order replays.
void test_derivations(uint32_t iterations) {
  std::mt19937 rng(999);
  SubSolver s;
  uint32_t refutations = 0, assumption_targets = 0, empty_targets = 0;
  uint32_t steps_total = 0, core_total = 0, multi_step = 0, max_steps = 0;
  for (uint32_t it = 0; it < iterations; it++) {
    // Three regimes. The mixed one has units and binaries, so refutations land
    // at level 0 with a pure reason-cone chain; the two 3-SAT regimes have
    // neither, so refuting takes search and the chains run through learned
    // lemmas. The big regime only grows those chains, and skips entailment
    // because enumerating 2^22 assignments per iteration is unaffordable.
    const uint32_t regime = it % 3;
    const uint32_t nvars = regime == 0   ? 2 + rng() % 6
                           : regime == 1 ? 6 + rng() % 7
                                         : 20 + rng() % 12;
    const uint32_t nclauses = regime == 0 ? 1 + rng() % (5 * nvars)
                                          : (43 * nvars) / 10 + rng() % 4;
    const Formula f = regime == 0 ? random_formula(rng, nvars, nclauses, 3)
                                  : random_formula(rng, nvars, nclauses, 3, 3);
    const bool check_entailment = regime != 2;
    std::vector<SubLit> assumptions;
    const uint32_t na = rng() % 3;
    for (uint32_t i = 0; i < na; i++)
      assumptions.push_back(sub_lit(rng() % nvars, rng() & 1u));
    load(s, f);
    if (s.solve(assumptions, 0) != SubResult::Unsat)
      continue;
    SubDerivation d;
    if (!s.derivation(d))
      continue; // directly contradictory assumptions: nothing to derive
    refutations++;
    steps_total += static_cast<uint32_t>(d.steps.size());
    core_total += static_cast<uint32_t>(d.core.size());
    if (d.steps.size() > 1)
      multi_step++;
    if (d.steps.size() > max_steps)
      max_steps = static_cast<uint32_t>(d.steps.size());
    // (1) replay, starting from the originals as the checker would see them
    std::map<uint32_t, std::vector<SubLit>> clauses;
    for (uint32_t i = 0; i < f.clauses.size(); i++)
      clauses[i + 1] = f.clauses[i];
    for (const SubStep &step : d.steps) {
      // (3) core discipline, checked before the step is admitted
      for (const uint32_t a : step.chain) {
        const bool known = clauses.count(a) != 0;
        assert(known && "a chain cites a clause that does not exist yet");
        if (a <= s.original_clauses())
          assert(std::find(d.core.begin(), d.core.end(), a) != d.core.end() &&
                 "a chain cites an original outside the reported core");
      }
      assert(lrat_replay(clauses, step) && "a derivation step does not check");
      clauses[step.id] = step.lits;
    }
    // the last step is the target
    const std::vector<SubLit> &target = d.steps.back().lits;
    if (target.empty())
      empty_targets++;
    else
      assumption_targets++;
    // (2) semantics
    if (check_entailment)
      check_target_is_entailed(f, assumptions, target);
    for (const uint32_t id : d.core)
      assert(id >= 1 && id <= s.original_clauses());
  }
  printf("  derivations: %u refutations checked (%u over assumptions, %u empty),"
         " %.1f steps / %.1f core clauses each, %u multi-step (max %u)\n",
         refutations, assumption_targets, empty_targets,
         refutations ? double(steps_total) / refutations : 0.0,
         refutations ? double(core_total) / refutations : 0.0, multi_step,
         max_steps);
  assert(refutations > 100 && "the generator stopped producing refutations");
  assert(assumption_targets > 100 && empty_targets > 100 &&
         "both refutation paths must be exercised");
  // Without a chain through a learned lemma the derivation machinery is
  // untested.
  assert(multi_step > refutations / 10 && max_steps > 20 &&
         "the derivations are not exercising lemma chains");
}

// Refutations reached without search, and the one case with nothing to give.
void test_derivation_edge_cases() {
  SubSolver s;
  SubDerivation d;
  // a stated empty clause
  s.reset(2);
  const uint32_t e = s.add_clause({});
  assert(s.solve({}, 0) == SubResult::Unsat);
  assert(s.derivation(d) && d.steps.size() == 1 && d.steps[0].lits.empty());
  assert(d.core.size() == 1 && d.core[0] == e);
  // clashing units: the second one refutes at add time
  s.reset(2);
  const uint32_t u1 = s.add_clause({sub_lit(0, false)});
  const uint32_t u2 = s.add_clause({sub_lit(0, true)});
  assert(s.solve({}, 0) == SubResult::Unsat);
  assert(s.derivation(d) && d.steps.back().lits.empty());
  assert(d.core == std::vector<uint32_t>({u1, u2}));
  // a unit assumption refuted by a unit clause: the target is that assumption
  s.reset(2);
  const uint32_t u = s.add_clause({sub_lit(0, false)});
  assert(s.solve({sub_lit(0, true)}, 0) == SubResult::Unsat);
  assert(s.derivation(d));
  assert(d.steps.back().lits == std::vector<SubLit>({sub_lit(0, false)}));
  assert(d.core == std::vector<uint32_t>({u}));
  // directly contradictory assumptions: the formula proves nothing, so the
  // target would be the tautology x v ~x
  s.reset(2);
  s.add_clause({sub_lit(0, false), sub_lit(1, false)});
  assert(s.solve({sub_lit(0, false), sub_lit(0, true)}, 0) == SubResult::Unsat);
  assert(!s.derivation(d) && "contradictory assumptions derive nothing");
}

// Write the cases the external-checker script feeds to drat-trim and
// lrat-check: a DIMACS file (originals, then a unit per used assumption) plus
// matching LRAT and DRAT proofs, lemma ids shifted past the assumption units.
void emit_cases(const char *dir) {
  std::mt19937 rng(4242);
  SubSolver s;
  uint32_t written = 0;
  uint32_t multi_step = 0, max_steps = 0;
  for (uint32_t it = 0; written < 40; it++) {
    assert(it < 100000);
    // Same three regimes as test_derivations: easy cases alone leave the
    // external checkers verifying one-line proofs.
    const uint32_t regime = it % 3;
    const uint32_t nvars = regime == 0   ? 3 + rng() % 5
                           : regime == 1 ? 6 + rng() % 7
                                         : 20 + rng() % 12;
    const uint32_t nclauses = regime == 0 ? 1 + rng() % (5 * nvars)
                                          : (43 * nvars) / 10 + rng() % 4;
    const Formula f = regime == 0 ? random_formula(rng, nvars, nclauses, 3)
                                  : random_formula(rng, nvars, nclauses, 3, 3);
    std::vector<SubLit> assumptions;
    const uint32_t na = rng() % 3;
    for (uint32_t i = 0; i < na; i++)
      assumptions.push_back(sub_lit(rng() % nvars, rng() & 1u));
    load(s, f);
    if (s.solve(assumptions, 0) != SubResult::Unsat)
      continue;
    SubDerivation d;
    if (!s.derivation(d))
      continue;
    const std::vector<SubLit> target = d.steps.back().lits;
    const uint32_t n = static_cast<uint32_t>(f.clauses.size());
    const uint32_t k = static_cast<uint32_t>(target.size()); // units to add
    const auto shift = [&](uint32_t id) { return id <= n ? id : id + k; };
    const auto dimacs = [&](SubLit l) {
      return ((l & 1u) ? -1 : 1) * static_cast<int>(sub_var_of(l) + 1);
    };
    char path[512];
    snprintf(path, sizeof path, "%s/case%02u.cnf", dir, written);
    FILE *cnf = fopen(path, "w");
    assert(cnf);
    fprintf(cnf, "p cnf %u %u\n", nvars, n + k);
    for (const auto &c : f.clauses) {
      for (const SubLit l : c)
        fprintf(cnf, "%d ", dimacs(l));
      fprintf(cnf, "0\n");
    }
    for (const SubLit t : target) // the assumptions, as units
      fprintf(cnf, "%d 0\n", dimacs(sub_negate(t)));
    fclose(cnf);

    snprintf(path, sizeof path, "%s/case%02u.lrat", dir, written);
    FILE *lrat = fopen(path, "w");
    snprintf(path, sizeof path, "%s/case%02u.drat", dir, written);
    FILE *drat = fopen(path, "w");
    assert(lrat && drat);
    for (const SubStep &step : d.steps) {
      fprintf(lrat, "%u ", shift(step.id));
      for (const SubLit l : step.lits) {
        fprintf(lrat, "%d ", dimacs(l));
        fprintf(drat, "%d ", dimacs(l));
      }
      fprintf(lrat, "0 ");
      for (const uint32_t a : step.chain)
        fprintf(lrat, "%u ", shift(a));
      fprintf(lrat, "0\n");
      fprintf(drat, "0\n");
    }
    if (k) { // close with the empty clause, using the assumption units
      fprintf(lrat, "%u 0 ", shift(d.steps.back().id) + 1);
      for (uint32_t i = 0; i < k; i++)
        fprintf(lrat, "%u ", n + 1 + i);
      fprintf(lrat, "%u 0\n", shift(d.steps.back().id));
      fprintf(drat, "0\n");
    }
    fclose(lrat);
    fclose(drat);
    if (d.steps.size() > 1)
      multi_step++;
    if (d.steps.size() > max_steps)
      max_steps = static_cast<uint32_t>(d.steps.size());
    written++;
  }
  printf("emitted %u cases to %s (%u multi-step, longest %u steps)\n", written,
         dir, multi_step, max_steps);
  // the checkers must see real chains, not 40 one-line proofs
  assert(multi_step >= 15 && max_steps >= 10);
}

} // namespace

int main(int argc, char **argv) {
  if (argc > 2 && !strcmp(argv[1], "--emit")) {
    emit_cases(argv[2]);
    return 0;
  }
  // argv[1] overrides the iteration count.
  const uint32_t iterations =
      argc > 1 ? static_cast<uint32_t>(std::atoi(argv[1])) : 100000;
  test_degenerate();
  test_assumption_paths();
  test_budget_and_determinism();
  test_against_brute_force(iterations);
  test_derivation_edge_cases();
  test_derivations(iterations / 4);
  printf("test_subsolver: all passed\n");
  return 0;
}
