// Sweep's embedded sub-solver and its refutation derivation. The solver is
// checked against brute force over all 2^n assignments, with and without the
// assumptions Sweep always uses. A derivation is checked three independent
// ways: `chain_replay` replays every chain, `check_target_is_entailed` checks
// entailment by enumeration, and the core may not under-report what the chains
// cite. `--emit <dir>` writes the cases out for `sweep_proofs.sh`.
#include "unit_test.hpp"
#include "engine/subsolver.hpp"

#include <map>
#include <random>

namespace {

struct Formula {
  uint32_t nvars = 0;
  std::vector<std::vector<SubLit>> clauses;
};

// Sequence the draws (argument evaluation order differs between compilers):
// sign before variable.
SubLit random_literal(std::mt19937 &rng, uint32_t nvars) {
  const bool negative = (rng() & 1u) != 0;
  const SubVar variable = rng() % nvars;
  return sub_lit(variable, negative);
}

// Whether `l` is true under `bits` (bit v = variable v).
bool lit_true(uint32_t bits, SubLit l) {
  return (((bits >> sub_var_of(l)) & 1u) != 0) != ((l & 1u) != 0);
}

bool all_true(uint32_t bits, const std::vector<SubLit> &lits) {
  return std::all_of(lits.begin(), lits.end(),
                     [&](SubLit l) { return lit_true(bits, l); });
}

bool satisfied_by(const Formula &f, uint32_t bits) {
  return std::all_of(f.clauses.begin(), f.clauses.end(), [&](const auto &c) {
    return std::any_of(c.begin(), c.end(),
                       [&](SubLit l) { return lit_true(bits, l); });
  });
}

// exhaustive: is `f` satisfiable with every assumption literal true?
bool brute_force(const Formula &f, const std::vector<SubLit> &assumptions) {
  for (uint32_t bits = 0; bits < (1u << f.nvars); bits++)
    if (all_true(bits, assumptions) && satisfied_by(f, bits))
      return true;
  return false;
}

// The reported model satisfies the formula and the assumptions.
void check_model(const SubSolver &s, const Formula &f,
                 const std::vector<SubLit> &assumptions) {
  uint32_t bits = 0;
  for (SubVar v = 0; v < f.nvars; v++)
    if (s.model()[v] > 0)
      bits |= 1u << v;
  assert(satisfied_by(f, bits) && "the reported model is not a model");
  assert(all_true(bits, assumptions) && "the model breaks an assumption");
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
      const SubLit l = random_literal(rng, nvars);
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

void load(SubSolver &s, const Formula &f,
          SubSearchMode mode = SubSearchMode::Fixed) {
  s.reset(f.nvars, mode);
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
        assumptions.push_back(random_literal(rng, nvars));
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
      check_model(s, f, assumptions);
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
TEST(test_assumption_paths) {
  SubSolver s;
  s.reset(3);
  s.add_clause({sub_lit(0, false), sub_lit(1, false)}); // (x0 v x1)
  assert(s.solve({sub_lit(0, true), sub_lit(1, true)}, 0) == SubResult::Unsat);
  // the same formula is satisfiable without the assumptions
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
TEST(test_degenerate) {
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
  // A late opposite-sign duplicate in a long clause is a dropped tautology in
  // linear time, and its touched-mark cleanup leaves the next unit intact.
  constexpr uint32_t long_size = 4096;
  s.reset(long_size);
  std::vector<SubLit> long_tautology;
  long_tautology.reserve(long_size + 1);
  for (SubVar v = 0; v < long_size; ++v)
    long_tautology.push_back(sub_lit(v, false));
  long_tautology.push_back(sub_lit(long_size - 1, true));
  s.add_clause(long_tautology);
  s.add_clause({sub_lit(0, true)});
  assert(s.solve({}, 0) == SubResult::Sat);
  assert(s.model()[0] < 0);
  s.reset(2);
  s.add_clause({sub_lit(0, false)});
  s.add_clause({sub_lit(0, true)}); // directly inconsistent
  assert(s.solve({}, 0) == SubResult::Unsat);
}

// The contract with the effort scheduler: exceeding the budget yields Budget,
// never a guessed verdict, and the tick count is deterministic.
TEST(test_budget_and_determinism) {
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

  // Kissat Kitten's currency charges one tick per loaded clause reference;
  // the entry counter does not, and the Kitten budget never uses `ticks()`.
  build();
  constexpr uint64_t original_clauses = pigeons + holes * pigeons * (pigeons - 1) / 2;
  assert(s.kitten_ticks() == original_clauses);
  assert(s.ticks() == 0);
  assert(s.solve({}, 0, SubTickMode::Kitten) == SubResult::Unsat);
  const uint64_t kitten_full = s.kitten_ticks();
  assert(kitten_full > original_clauses);
  build();
  assert(s.solve({}, kitten_full / 2, SubTickMode::Kitten) ==
         SubResult::Budget);

  // Kitten stops before another decision when its counter equals the limit.
  s.start_kitten_session();
  s.reset(3, SubSearchMode::Kitten);
  s.add_clause(
      {sub_lit(0, false), sub_lit(1, false), sub_lit(2, false)});
  s.randomize_phases();
  assert(s.kitten_ticks() == 1);
  assert(s.solve({}, 2, SubTickMode::Kitten) == SubResult::Budget);
  assert(s.kitten_ticks() == 2);
}

TEST(test_external_stop_is_budget_result) {
  SubSolver s;
  s.reset(3, SubSearchMode::Kitten);
  s.add_clause(
      {sub_lit(0, false), sub_lit(1, false), sub_lit(2, false)});
  volatile sig_atomic_t stop = 1;
  assert(s.solve_interruptible({}, 0, SubTickMode::Kitten, &stop) ==
         SubResult::Budget);
  stop = 0;
  assert(s.solve_interruptible({}, 0, SubTickMode::Kitten, &stop) ==
         SubResult::Sat);
}

TEST(test_model_flip) {
  SubSolver s;
  s.reset(2);
  s.add_clause({sub_lit(0, false), sub_lit(1, false)});
  assert(s.solve({}, 0) == SubResult::Sat);
  assert(s.flip_model_literal(sub_lit(0, false)));
  assert(s.model()[0] < 0 && s.model()[1] > 0);
  assert(!s.flip_model_literal(sub_lit(1, false)));
  assert(s.model()[1] > 0);

  // The Fixed-only occurrence image is rebuilt after a Kitten environment,
  // which does not maintain it.
  s.start_kitten_session();
  s.reset(3, SubSearchMode::Kitten);
  s.add_clause({sub_lit(0, false), sub_lit(1, false),
                sub_lit(2, false)});
  s.randomize_phases();
  assert(s.solve({}, 0, SubTickMode::Kitten) == SubResult::Sat);
  s.reset(2, SubSearchMode::Fixed);
  s.add_clause({sub_lit(0, false), sub_lit(1, false)});
  assert(s.solve({}, 0) == SubResult::Sat);
  assert(s.flip_model_literal(sub_lit(0, false)));
  assert(!s.flip_model_literal(sub_lit(1, false)));

  s.reset(2);
  s.add_clause({sub_lit(0, false)});
  assert(s.solve({}, 0) == SubResult::Sat);
  assert(s.has_model());
  assert(!s.flip_model_literal(sub_lit(0, false)));

  // An UNSAT-under-assumptions result invalidates the model; Kissat checks
  // `kitten_status == 10` before every Sweep flip.
  s.reset(2);
  s.add_clause({sub_lit(0, false), sub_lit(1, false)});
  assert(s.solve({}, 0) == SubResult::Sat);
  assert(s.flip_model_literal(sub_lit(0, false)));
  assert(s.solve({sub_lit(0, true), sub_lit(1, true)}, 0) ==
         SubResult::Unsat);
  assert(!s.has_model());
  assert(!s.flip_model_literal(sub_lit(1, false)));
  assert(s.solve({}, 0) == SubResult::Sat);
}

TEST(test_kitten_search_state) {
  SubSolver s;
  s.start_kitten_session();

  // Clause order imports variable 1 before 0; Kitten decides from the newest
  // end. A fresh embedded Kitten uses generator 0 for its first environment.
  s.reset(2, SubSearchMode::Kitten);
  assert(s.kitten_generator() == 0);
  s.add_clause({sub_lit(1, false), sub_lit(0, false)});
  s.randomize_phases();
  assert(s.solve({}, 0, SubTickMode::Kitten) == SubResult::Sat);
  assert(s.model()[0] < 0 && s.model()[1] > 0);
  assert(s.fixed_literal(sub_lit(0, false)) == 0);
  assert(s.fixed_literal(sub_lit(1, false)) == 0);

  // Clearing the same embedded Kitten advances the seed to 1.
  s.reset(2, SubSearchMode::Kitten);
  assert(s.kitten_generator() == 1);
  s.add_clause({sub_lit(1, false), sub_lit(0, false)});
  s.randomize_phases();
  assert(s.solve({}, 0, SubTickMode::Kitten) == SubResult::Sat);
  assert(s.model()[0] > 0 && s.model()[1] > 0);

  // A new Sweep invocation constructs another fresh generator-0 environment.
  s.start_kitten_session();
  s.reset(2, SubSearchMode::Kitten);
  assert(s.kitten_generator() == 0);
  s.add_clause({sub_lit(1, false), sub_lit(0, false)});
  s.randomize_phases();
  assert(s.solve({}, 0, SubTickMode::Kitten) == SubResult::Sat);
  assert(s.model()[0] < 0 && s.model()[1] > 0);

  // Eliminate clears before its first environment, which starts at seed 1.
  s.start_kitten_session(/*clear_before_first_environment=*/true);
  s.reset(2, SubSearchMode::Kitten);
  assert(s.kitten_generator() == 1);

  s.start_kitten_session();
  s.reset(2, SubSearchMode::Kitten);
  s.add_clause({sub_lit(1, true)});
  s.randomize_phases();
  assert(s.solve({}, 0, SubTickMode::Kitten) == SubResult::Sat);
  assert(s.fixed_literal(sub_lit(1, true)) > 0);
  assert(s.fixed_literal(sub_lit(1, false)) < 0);
  assert(s.fixed_literal(sub_lit(0, false)) == 0);

  // `kitten_clear` keeps the high-water phase capacity a later small
  // environment randomizes.
  s.start_kitten_session();
  s.reset(130, SubSearchMode::Kitten);
  for (SubVar v = 0; v < 130; v++)
    s.add_clause({sub_lit(v, false)});
  assert(s.kitten_phase_capacity() == 256);
  s.reset(3, SubSearchMode::Kitten);
  for (SubVar v = 0; v < 3; v++)
    s.add_clause({sub_lit(v, false)});
  assert(s.kitten_phase_capacity() == 256);
  s.start_kitten_session();
  s.reset(3, SubSearchMode::Kitten);
  for (SubVar v = 0; v < 3; v++)
    s.add_clause({sub_lit(v, false)});
  assert(s.kitten_phase_capacity() == 4);
}

TEST(test_compact_kitten_identity) {
  SubSolver mapped, compact;
  mapped.start_kitten_session();
  compact.start_kitten_session();
  mapped.reset(4, SubSearchMode::Kitten);
  compact.reset_compact_kitten(4);

  // First appearances 0,1,2,3 (BVE's compact-map contract): both entry points
  // build the same VMTF/search state.
  const Formula f{4, {
      {sub_lit(0, false), sub_lit(1, false)},
      {sub_lit(1, true), sub_lit(2, false)},
      {sub_lit(2, true), sub_lit(3, false)},
      {sub_lit(3, true), sub_lit(0, true)},
      {sub_lit(0, false), sub_lit(2, true)},
      {sub_lit(1, false), sub_lit(3, true)},
  }};
  for (const auto &clause : f.clauses) {
    mapped.add_clause(clause);
    compact.add_clause(clause);
  }
  mapped.randomize_phases();
  compact.randomize_phases();
  assert(mapped.kitten_generator() == compact.kitten_generator());
  const SubResult a = mapped.solve({}, 0, SubTickMode::Kitten);
  const SubResult b = compact.solve({}, 0, SubTickMode::Kitten);
  assert(a == b);
  assert(mapped.kitten_ticks() == compact.kitten_ticks());
  assert(mapped.conflicts() == compact.conflicts());
  if (a == SubResult::Sat)
    assert(mapped.model() == compact.model());
  else {
    SubDerivation da, db;
    assert(mapped.derivation(da) && compact.derivation(db));
    assert(da.core == db.core);
    assert(da.steps.size() == db.steps.size());
    for (size_t i = 0; i < da.steps.size(); ++i) {
      assert(da.steps[i].id == db.steps[i].id);
      assert(da.steps[i].lits == db.steps[i].lits);
      assert(da.steps[i].chain == db.steps[i].chain);
    }
  }
}

void test_kitten_against_brute_force(uint32_t iterations) {
  std::mt19937 rng(20260810);
  SubSolver s;
  s.start_kitten_session();
  for (uint32_t it = 0; it < iterations; it++) {
    const uint32_t nvars = 2 + rng() % 7;
    const Formula f =
        random_formula(rng, nvars, 1 + rng() % (4 * nvars), 3);
    std::vector<SubLit> assumptions;
    for (uint32_t i = 0, n = rng() % 3; i < n; i++)
      assumptions.push_back(random_literal(rng, nvars));
    load(s, f, SubSearchMode::Kitten);
    s.randomize_phases();
    const SubResult result = s.solve(assumptions, 0, SubTickMode::Kitten);
    assert(result != SubResult::Budget);
    assert((result == SubResult::Sat) == brute_force(f, assumptions));
    if (result == SubResult::Sat)
      check_model(s, f, assumptions);
  }
}

// --- the refutation derivation ---------------------------------------------

// A complete antecedent replay, so every derivation is checked by unit
// propagation in chain order. `clauses` maps id -> literals and grows as steps
// are accepted.
bool chain_replay(std::map<uint32_t, std::vector<SubLit>> &clauses,
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
  for (uint32_t bits = 0; bits < (1u << f.nvars); bits++)
    assert((!satisfied_by(f, bits) ||
            std::any_of(target.begin(), target.end(),
                        [&](SubLit t) { return lit_true(bits, t); })) &&
           "a model of the environment falsifies the derived clause");
}

void test_rup_admission_chains(uint32_t iterations) {
  {
    SubSolver s;
    s.reset(1);
    const std::vector<SubLit> positive{sub_lit(0,false)}, negative{sub_lit(0,true)};
    s.add_clause(positive); s.add_clause(negative);
    SubStep proof;
    assert(s.admit_rup(positive,UINT64_MAX,nullptr,&proof)==SubRupAdmission::Accepted);
    assert((proof.chain==std::vector<uint32_t>{1})); // stop at target's first root clash
    assert(s.admit_rup(negative,UINT64_MAX,nullptr,&proof)==SubRupAdmission::Accepted);
    assert((proof.chain==std::vector<uint32_t>{2})); // the opposite root is an assumption
    assert(s.admit_rup(positive,0,nullptr,&proof)==SubRupAdmission::Budget);
    assert(!proof.id && proof.lits.empty() && proof.chain.empty());
  }
  std::mt19937 rng(94153);
  uint32_t checked = 0, dependent = 0;
  for (uint32_t run = 0; run < iterations; ++run) {
    Formula f = random_formula(rng, 2 + rng() % 7, 1 + rng() % 12, 3);
    SubSolver s;
    s.reset(f.nvars);
    std::map<uint32_t, std::vector<SubLit>> clauses;
    std::vector<uint32_t> admitted;
    for (const auto &c : f.clauses) clauses.emplace(s.add_clause(c), c);
    for (unsigned query = 0; query < 16; ++query) {
      if (query % 4 == 3) {
        auto c = random_formula(rng, f.nvars, 1, 3).clauses[0];
        assert(s.append_input(f.nvars, c));
        clauses.emplace(s.original_clauses(), c);
        f.clauses.push_back(std::move(c));
      }
      // Admission captures this query's cone despite a previous query's
      // leftovers, including a directly falsified assumption.
      s.propagate_assumptions_only({random_literal(rng, f.nvars)}, UINT64_MAX);
      auto c = random_formula(rng, f.nvars, 1, 3).clauses[0];
      std::vector<SubLit> assumptions;
      for (auto l : c) assumptions.push_back(sub_negate(l));
      SubStep proof{UINT32_MAX, c, {UINT32_MAX}, {UINT32_MAX}};
      const auto result = s.admit_rup(c, UINT64_MAX, nullptr, &proof);
      if (result != SubRupAdmission::Accepted) {
        assert(result == SubRupAdmission::NotRup);
        assert(!proof.id && proof.lits.empty() && proof.chain.empty() && proof.antecedents.empty());
        continue;
      }
      assert(proof.id == s.original_clauses() && !clauses.count(proof.id));
      assert(chain_replay(clauses, proof));
      assert(!brute_force(f, assumptions));
      for (auto id : proof.chain) {
        assert(id < proof.id);
        if (std::find(admitted.begin(), admitted.end(), id) != admitted.end()) ++dependent;
      }
      clauses.emplace(proof.id, proof.lits);
      admitted.push_back(proof.id);
      ++checked;
    }
  }
  assert(checked && dependent);
  std::printf("  RUP admission: %u checked dependency chains\n", checked);
}

// One random environment of `regime`: 0 mixes units and binaries (refuted at
// level 0), 1 and 2 are 3-SAT that needs search (2 is too big to enumerate).
struct Case {
  Formula formula;
  std::vector<SubLit> assumptions;
};
Case random_case(std::mt19937 &rng, uint32_t regime, uint32_t small_base,
                 uint32_t small_span) {
  const uint32_t nvars = regime == 0   ? small_base + rng() % small_span
                         : regime == 1 ? 6 + rng() % 7
                                       : 20 + rng() % 12;
  const uint32_t nclauses = regime == 0 ? 1 + rng() % (5 * nvars)
                                        : (43 * nvars) / 10 + rng() % 4;
  Case c{regime == 0 ? random_formula(rng, nvars, nclauses, 3)
                     : random_formula(rng, nvars, nclauses, 3, 3),
         {}};
  const uint32_t na = rng() % 3;
  for (uint32_t i = 0; i < na; i++)
    c.assumptions.push_back(random_literal(rng, nvars));
  return c;
}

// The derivation oracle: random refutations, each replayed by propagation,
// checked for entailment by enumeration, and checked for core discipline.
void test_derivations(uint32_t iterations,
                      SubSearchMode mode = SubSearchMode::Fixed) {
  std::mt19937 rng(999);
  SubSolver s;
  if (mode == SubSearchMode::Kitten)
    s.start_kitten_session();
  uint32_t refutations = 0, assumption_targets = 0, empty_targets = 0;
  uint32_t steps_total = 0, core_total = 0, multi_step = 0, max_steps = 0;
  for (uint32_t it = 0; it < iterations; it++) {
    const uint32_t regime = it % 3;
    const auto [f, assumptions] = random_case(rng, regime, 2, 6);
    const bool check_entailment = regime != 2;
    load(s, f, mode);
    if (mode == SubSearchMode::Kitten)
      s.randomize_phases();
    const SubTickMode tick_mode = mode == SubSearchMode::Kitten
                                      ? SubTickMode::Kitten
                                      : SubTickMode::Entries;
    if (s.solve(assumptions, 0, tick_mode) != SubResult::Unsat)
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
      assert(chain_replay(clauses, step) && "a derivation step does not check");
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
  printf("  %s derivations: %u refutations checked (%u over assumptions, %u empty),"
         " %.1f steps / %.1f core clauses each, %u multi-step (max %u)\n",
         mode == SubSearchMode::Kitten ? "Kitten" : "fixed", refutations,
         assumption_targets, empty_targets,
         refutations ? double(steps_total) / refutations : 0.0,
         refutations ? double(core_total) / refutations : 0.0, multi_step,
         max_steps);
  assert(refutations > 100 && "the generator stopped producing refutations");
  assert(assumption_targets > 100 && empty_targets > 100 &&
         "both refutation paths must be exercised");
  // Chains must run through learned lemmas.
  assert(multi_step > refutations / 10 && max_steps > 20 &&
         "the derivations are not exercising lemma chains");
}

// Refutations reached without search, and the one case with nothing to give.
TEST(test_derivation_edge_cases) {
  SubSolver s;
  SubDerivation d;
  // a stated empty clause
  s.reset(2);
  const uint32_t e = s.add_clause({});
  assert(s.solve({}, 0) == SubResult::Unsat);
  assert(s.derivation(d) && d.steps.size() == 1 && d.steps[0].lits.empty());
  assert(s.refutation_target() && s.refutation_target()->lits.empty());
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
  assert(s.refutation_target() &&
         s.refutation_target()->lits == d.steps.back().lits);
  assert(d.core == std::vector<uint32_t>({u}));
  // directly contradictory assumptions: the formula proves nothing, so the
  // target would be the tautology x v ~x
  s.reset(2);
  s.add_clause({sub_lit(0, false), sub_lit(1, false)});
  assert(s.solve({sub_lit(0, false), sub_lit(0, true)}, 0) == SubResult::Unsat);
  assert(!s.derivation(d) && "contradictory assumptions derive nothing");
  assert(!s.refutation_target());

  // Kitten reuses the clause learned from a failed assumption solve; a
  // non-identity import order checks both the internal clause and the exported
  // literals. F = (x2 v x0) & (~x2 v x1) entails (x0 v x1).
  s.start_kitten_session();
  s.reset(3, SubSearchMode::Kitten);
  s.add_clause({sub_lit(2, false), sub_lit(0, false)});
  s.add_clause({sub_lit(2, true), sub_lit(1, false)});
  const std::vector<SubLit> assumptions = {sub_lit(0, true),
                                            sub_lit(1, true)};
  s.randomize_phases();
  assert(s.solve(assumptions, 0, SubTickMode::Kitten) == SubResult::Unsat);
  assert(s.derivation(d));
  assert(d.steps.back().lits ==
         std::vector<SubLit>({sub_lit(1, false), sub_lit(0, false)}));
  std::map<uint32_t, std::vector<SubLit>> clauses = {
      {1, {sub_lit(2, false), sub_lit(0, false)}},
      {2, {sub_lit(2, true), sub_lit(1, false)}}};
  for (const SubStep &step : d.steps) {
    assert(chain_replay(clauses, step));
    clauses[step.id] = step.lits;
  }
  s.randomize_phases();
  assert(s.solve({}, 0, SubTickMode::Kitten) == SubResult::Sat);
  s.randomize_phases();
  assert(s.solve(assumptions, 0, SubTickMode::Kitten) == SubResult::Unsat);
  assert(s.derivation(d));
}

// Write the cases the external-checker script feeds to drat-trim and the DSR
// pipeline: a DIMACS file (originals, then a unit per used assumption) plus a
// witness-free DSR proof.
void emit_cases(const char *dir) {
  std::mt19937 rng(4242);
  SubSolver s;
  uint32_t written = 0;
  uint32_t multi_step = 0, max_steps = 0;
  for (uint32_t it = 0; written < 40; it++) {
    assert(it < 100000);
    // All three regimes, so the checkers see real lemma chains.
    const auto [f, assumptions] = random_case(rng, it % 3, 3, 5);
    const uint32_t nvars = f.nvars;
    load(s, f);
    if (s.solve(assumptions, 0) != SubResult::Unsat)
      continue;
    SubDerivation d;
    if (!s.derivation(d))
      continue;
    const std::vector<SubLit> target = d.steps.back().lits;
    const uint32_t n = static_cast<uint32_t>(f.clauses.size());
    const uint32_t k = static_cast<uint32_t>(target.size()); // units to add
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

    snprintf(path, sizeof path, "%s/case%02u.dsr", dir, written);
    FILE *dsr = fopen(path, "w");
    assert(dsr);
    for (const SubStep &step : d.steps) {
      for (const SubLit l : step.lits)
        fprintf(dsr, "%d ", dimacs(l));
      fprintf(dsr, "0\n");
    }
    if (k) // close with the empty clause, using the assumption units
      fprintf(dsr, "0\n");
    fclose(dsr);
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

TEST(test_assumption_only_propagation) {
  SubSolver s;
  s.reset(3, SubSearchMode::Fixed);
  s.add_clause({sub_lit(0, false), sub_lit(1, false)});
  s.add_clause({sub_lit(0, true), sub_lit(2, false)});

  assert(s.propagate_assumptions_only({sub_lit(0, true)}, UINT64_MAX) ==
         SubPropagationResult::Consistent);
  assert(s.propagate_assumptions_only(
             {sub_lit(0, true), sub_lit(1, true)}, UINT64_MAX) ==
         SubPropagationResult::Conflict);
  // A new query discards the previous conflict's temporary levels.
  assert(s.propagate_assumptions_only(
             {sub_lit(0, false), sub_lit(2, true)}, UINT64_MAX) ==
         SubPropagationResult::Conflict);
  assert(s.propagate_assumptions_only({}, UINT64_MAX) ==
         SubPropagationResult::Consistent);
  assert(s.propagate_assumptions_only({}, 0) ==
         SubPropagationResult::Budget);

  volatile sig_atomic_t stop = 1;
  assert(s.propagate_assumptions_only({}, UINT64_MAX, &stop) ==
         SubPropagationResult::Interrupted);
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
  run_tests();
  test_rup_admission_chains(iterations / 100 + 1);
  test_kitten_against_brute_force(iterations / 5);
  test_against_brute_force(iterations);
  test_derivations(iterations / 4);
  test_derivations(iterations / 10, SubSearchMode::Kitten);
  printf("test_subsolver: all passed\n");
  return 0;
}
