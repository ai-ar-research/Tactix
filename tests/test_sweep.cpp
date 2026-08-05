// V5 Sweep · S-c: candidate selection + environment construction.
//
// Four things have to be true for S-c to be worth anything, and each has its
// own test here:
//
//  1. IT FINDS EQUIVALENCES RUP CANNOT (test_non_rup_equivalence). The whole
//     reason V5 exists is that a swept equivalence is entailed but not RUP
//     (design §4); the fixture proves its own premise by assuming the pair and
//     propagating in the MAIN kernel — no conflict, so the S-d fast path could
//     never get this one — and only then requires the detector to.
//  2. EVERY CLAIM IS TRUE (test_brute_force_oracle). 20k random formulas, and
//     every reported `a ≡ b` re-checked by enumerating all models of the
//     ORIGINAL formula. This is the oracle that would catch an environment that
//     is not a subset of F, a mis-mapped literal, or a vacuous "refutation".
//  3. THE ENVIRONMENT IS LITERALLY MADE OF F's CLAUSES (test_env_provenance).
//     Each environment clause is matched back to a kernel clause with the SAME
//     literals and the SAME proof id — S-e's core mapping is exactly this
//     lookup, and a truncated or strengthened clause would break it silently.
//  4. THE LIMITS BIND WITHOUT TRUNCATING (test_limits).
//
// Plus the two bookkeeping cases: an equivalence the formula already states is
// not re-reported (test_known_equivalence), and the seed order skips what
// cannot be swept (test_seeds).
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/sweep/sweep.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <random>
#include <vector>

using namespace tactix;

namespace {

// Build a kernel from DIMACS-style clauses, handing every clause a proof id in
// input order (what the parser does).
void build(Kernel &k, uint32_t nvars, const std::vector<std::vector<int>> &cls) {
  kernel_reserve_vars(k, nvars);
  for (const auto &c : cls) {
    const uint64_t id = ++k.next_clause_id;
    if (c.size() == 1) {
      k.unit_ids[var_of(internalize(c[0]))] = id;
      kernel_assign_unit(k, internalize(c[0]));
      continue;
    }
    if (c.size() == 2) {
      kernel_add_binary_clause(k, internalize(c[0]), internalize(c[1]), id);
      continue;
    }
    std::vector<Lit> lits;
    for (const int e : c)
      lits.push_back(internalize(e));
    kernel_add_large_clause(k, lits.data(), static_cast<uint32_t>(lits.size()),
                            false, static_cast<uint32_t>(lits.size()), id);
  }
}

bool satisfies(const std::vector<std::vector<int>> &cls, uint32_t bits) {
  for (const auto &c : cls) {
    bool ok = false;
    for (const int e : c) {
      const bool value = (bits >> (std::abs(e) - 1)) & 1u;
      ok = ok || (value == (e > 0));
    }
    if (!ok)
      return false;
  }
  return true;
}

// `a ≡ b` in every model of `cls` (vacuously true for an UNSAT formula, which
// is exactly the entailment the rule may use).
bool equivalent_in_all_models(const std::vector<std::vector<int>> &cls,
                              uint32_t nvars, Lit a, Lit b) {
  for (uint32_t bits = 0; bits < (1u << nvars); bits++) {
    if (!satisfies(cls, bits))
      continue;
    const auto value = [&](Lit l) {
      return (((bits >> var_of(l)) & 1u) != 0) != negated(l);
    };
    if (value(a) != value(b))
      return false;
  }
  return true;
}

// Is `(a ∨ b)` derivable by unit propagation alone from F (i.e. RUP)? Assume
// both negations at a decision level and propagate: a conflict means RUP.
bool is_rup_binary(Kernel &k, Lit a, Lit b) {
  kernel_assign_decision(k, negate(a));
  kernel_assign_decision(k, negate(b));
  const bool conflict = kernel_propagate(k);
  k.conflict_valid = false;
  kernel_backtrack(k, 0);
  return conflict;
}

// --- 1. the equivalence RUP cannot reach ------------------------------------
// (¬x ∨ y ∨ ±z ∨ ±w) over all four sign patterns says x → y with NO unit
// consequence: assuming x ∧ ¬y leaves (z ∨ w), (z ∨ ¬w), (¬z ∨ w), (¬z ∨ ¬w),
// which needs a DECISION to refute. The mirrored block gives y → x the same
// way, so F ⊨ x ≡ y and neither binary is RUP.
std::vector<std::vector<int>> non_rup_formula() {
  std::vector<std::vector<int>> cls;
  for (int zs = -1; zs <= 1; zs += 2)
    for (int ws = -1; ws <= 1; ws += 2) {
      cls.push_back({-1, 2, 3 * zs, 4 * ws});
      cls.push_back({1, -2, 3 * zs, 4 * ws});
    }
  return cls;
}

void test_non_rup_equivalence() {
  const std::vector<std::vector<int>> cls = non_rup_formula();
  Kernel k;
  build(k, 4, cls);
  const Lit x = internalize(1), y = internalize(2);
  // the fixture's own premise: neither direction is RUP
  assert(!is_rup_binary(k, negate(x), y));
  assert(!is_rup_binary(k, x, negate(y)));

  SweepRound r;
  SweepLimits lim;
  assert(sweep_guard(k));
  const uint32_t found = sweep_detect_round(k, lim, r);
  assert(found == 1);
  assert(r.found.size() == 1);
  assert(r.found[0].a == std::min(x, y) && r.found[0].b == std::max(x, y));
  // reachability: the detector really ran, and really solved candidate pairs
  assert(k.stats.sweep_rounds == 1);
  assert(k.stats.sweep_environments >= 1);
  assert(k.stats.sweep_env_clauses >= 8);
  assert(k.stats.sweep_solves >= 3); // one model + both directions
  assert(k.stats.sweep_candidates >= 2);
  assert(k.stats.sweep_equivalences == 1);
  assert(k.stats.sweep_ticks > 0);
  assert(k.stats.sweep_env_unsat == 0);
  // detection changes nothing: no clause, no unit, no witness
  assert(k.trail.empty() && k.reconstruction.empty());
  assert(k.next_clause_id == cls.size());
}

// The same equivalence, reached through the per-seed entry point (what S-d's
// `instances()` will drive), and from the OTHER seed — the answer must not
// depend on which end of the pair is swept.
void test_seed_symmetry() {
  const std::vector<std::vector<int>> cls = non_rup_formula();
  for (uint32_t seed = 0; seed < 2; seed++) {
    Kernel k;
    build(k, 4, cls);
    SweepRound r;
    SweepLimits lim;
    sweep_round_begin(k, r);
    assert(sweep_detect_seed(k, seed, lim, r) == 1);
    assert(r.found[0].a == internalize(1) && r.found[0].b == internalize(2));
  }
}

// --- 2. every claim is true --------------------------------------------------
void test_brute_force_oracle(uint32_t rounds) {
  std::mt19937 rng(20260805);
  uint64_t claims = 0, nontrivial = 0, envs = 0;
  for (uint32_t it = 0; it < rounds; it++) {
    const uint32_t nvars = 3 + rng() % 4; // 3..6
    const uint32_t nclauses = 2 + rng() % 10;
    std::vector<std::vector<int>> cls;
    for (uint32_t c = 0; c < nclauses; c++) {
      const uint32_t size = 2 + rng() % 3; // 2..4 (units are built separately)
      std::vector<int> lits;
      for (uint32_t i = 0; i < size; i++) {
        const int v = 1 + static_cast<int>(rng() % nvars);
        const int l = (rng() & 1) ? v : -v;
        if (std::find(lits.begin(), lits.end(), l) == lits.end() &&
            std::find(lits.begin(), lits.end(), -l) == lits.end())
          lits.push_back(l);
      }
      if (lits.size() >= 2)
        cls.push_back(lits);
    }
    if (cls.empty())
      continue;
    Kernel k;
    build(k, nvars, cls);
    SweepRound r;
    SweepLimits lim;
    lim.max_depth = 1 + rng() % 3;
    lim.max_env_vars = 2 + rng() % 6;
    lim.max_env_clauses = 2 + rng() % 10;
    lim.max_candidates = 1 + rng() % 4;
    sweep_detect_round(k, lim, r);
    envs += k.stats.sweep_environments;
    for (const SweepEquiv &e : r.found) {
      claims++;
      assert(e.a < e.b);
      assert(var_of(e.a) != var_of(e.b));
      assert(equivalent_in_all_models(cls, nvars, e.a, e.b));
      // was it something the formula did not already say? (the S-c question)
      Kernel probe;
      build(probe, nvars, cls);
      nontrivial += !is_rup_binary(probe, negate(e.a), e.b) ||
                    !is_rup_binary(probe, e.a, negate(e.b));
    }
    // the round leaves the formula alone
    assert(k.stats.sweep_equivalences == r.found.size());
  }
  // Reachability, at the source: a green oracle over ZERO environments or ZERO
  // claims would be vacuous ([[ab-arm-identical-artefacts]]).
  printf("  oracle: %llu environments, %llu claims (%llu not RUP)\n",
         (unsigned long long)envs, (unsigned long long)claims,
         (unsigned long long)nontrivial);
  assert(envs > 0);
  assert(claims > 0);
  // Non-RUP claims are RARE on formulas this small (~8 in 4,400 at the default
  // size) — unit propagation over the negated binary is strong. The default run
  // is large enough to require one; a shortened soak is not, and the property
  // has its own deterministic fixture above.
  assert(nontrivial > 0 || rounds < 20000);
}

// --- 3. the environment is made of F's clauses -------------------------------
// Every environment clause must BE a kernel clause: same literals (as a set),
// same proof id. That is the property `env ⊆ F` rests on, and the one S-e's
// core→id mapping consumes.
void test_env_provenance() {
  std::mt19937 rng(4242);
  for (uint32_t it = 0; it < 200; it++) {
    const uint32_t nvars = 4 + rng() % 5;
    std::vector<std::vector<int>> cls;
    for (uint32_t c = 0; c < 12; c++) {
      std::vector<int> lits;
      const uint32_t size = 2 + rng() % 3;
      for (uint32_t i = 0; i < size; i++) {
        const int v = 1 + static_cast<int>(rng() % nvars);
        const int l = (rng() & 1) ? v : -v;
        if (std::find(lits.begin(), lits.end(), l) == lits.end() &&
            std::find(lits.begin(), lits.end(), -l) == lits.end())
          lits.push_back(l);
      }
      if (lits.size() >= 2)
        cls.push_back(lits);
    }
    if (cls.empty())
      continue;
    Kernel k;
    build(k, nvars, cls);
    SweepRound r;
    SweepLimits lim;
    sweep_round_begin(k, r);
    for (Var seed = 0; seed < nvars; seed++) {
      if (!sweep_env_build(k, seed, lim, r))
        continue;
      const SweepEnv &e = r.env;
      for (uint32_t c = 0; c < e.nclauses(); c++) {
        std::vector<int> got;
        for (uint32_t i = e.clause_off[c]; i < e.clause_off[c + 1]; i++)
          got.push_back(externalize(e.clause_lits[i]));
        std::sort(got.begin(), got.end());
        assert(e.clause_ids[c] != 0 && "every environment clause is cited");
        // find the input clause with this id and compare literal sets
        assert(e.clause_ids[c] <= cls.size());
        std::vector<int> want = cls[e.clause_ids[c] - 1];
        std::sort(want.begin(), want.end());
        assert(got == want && "environment clause differs from F's clause");
        // and every variable of it is mapped into the sub-solver's space
        for (uint32_t i = e.clause_off[c]; i < e.clause_off[c + 1]; i++)
          assert(e.sub_of_var[var_of(e.clause_lits[i])] < e.vars.size());
      }
    }
  }
}

// --- 4. the limits bind, and bind by DECLINING whole clauses ------------------
void test_limits() {
  // a 6-variable clique of binaries: every seed can reach every variable
  std::vector<std::vector<int>> cls;
  for (int a = 1; a <= 6; a++)
    for (int b = a + 1; b <= 6; b++)
      cls.push_back({a, -b});
  Kernel k;
  build(k, 6, cls);
  SweepRound r;
  for (uint32_t cap = 1; cap <= 8; cap++) {
    SweepLimits lim;
    lim.max_env_clauses = cap;
    lim.max_env_vars = cap;
    sweep_round_begin(k, r);
    for (Var seed = 0; seed < 6; seed++) {
      if (!sweep_env_build(k, seed, lim, r))
        continue;
      assert(r.env.nclauses() <= cap);
      assert(r.env.vars.size() <= cap);
      for (uint32_t c = 0; c + 1 < r.env.clause_off.size(); c++)
        assert(r.env.clause_off[c + 1] - r.env.clause_off[c] == 2);
    }
  }
  // depth 0 keeps the environment to the seed's own clauses
  SweepLimits lim;
  lim.max_depth = 0;
  sweep_round_begin(k, r);
  assert(sweep_env_build(k, 0, lim, r));
  for (uint32_t c = 0; c < r.env.nclauses(); c++) {
    bool touches_seed = false;
    for (uint32_t i = r.env.clause_off[c]; i < r.env.clause_off[c + 1]; i++)
      touches_seed = touches_seed || var_of(r.env.clause_lits[i]) == 0;
    assert(touches_seed);
  }
}

// --- 5. an equivalence the formula already states ----------------------------
void test_known_equivalence() {
  // (¬1 ∨ 2), (1 ∨ ¬2) IS 1 ≡ 2 — nothing for Sweep to add. It must be counted
  // as known, not reported as a find (S-d would emit a duplicate binary).
  Kernel k;
  build(k, 3, {{-1, 2}, {1, -2}, {1, 3}, {-3, 2}});
  SweepRound r;
  SweepLimits lim;
  assert(sweep_detect_round(k, lim, r) == 0);
  assert(r.found.empty());
  assert(k.stats.sweep_known >= 1);
}

// --- 6. the seed surface -----------------------------------------------------
void test_seeds() {
  Kernel k;
  build(k, 5, {{1, 2}, {-1, 3}, {4, 5}});
  std::vector<Var> seeds;
  sweep_seeds(k, seeds);
  assert(seeds.size() == 5);
  // a root-fixed variable is not a seed (its equivalences are units, V1/V2's
  // delta), nor is a retired one
  k.unit_ids[0] = ++k.next_clause_id;
  kernel_assign_unit(k, internalize(1));
  k.var_status.resize(k.nvars, VarStatus::Active); // BVE would have sized it
  k.var_status[3] = VarStatus::Eliminated;
  sweep_seeds(k, seeds);
  assert(std::find(seeds.begin(), seeds.end(), 0u) == seeds.end());
  assert(std::find(seeds.begin(), seeds.end(), 3u) == seeds.end());
  assert(seeds.size() == 3);
  // ...and sweeping one anyway is a no-op, not an assertion (totality)
  SweepRound r;
  SweepLimits lim;
  sweep_round_begin(k, r);
  assert(sweep_detect_seed(k, 0, lim, r) == 0);
  assert(sweep_detect_seed(k, 3, lim, r) == 0);
}

// --- 7. an environment that refutes the formula outright ---------------------
void test_env_unsat() {
  // (1)(¬1) is caught before sweeping; use a refutation the environment finds:
  // the four clauses over {1,2} are UNSAT, and any seed's environment holds all
  // of them.
  Kernel k;
  build(k, 3, {{1, 2}, {1, -2}, {-1, 2}, {-1, -2}});
  SweepRound r;
  SweepLimits lim;
  assert(sweep_detect_round(k, lim, r) == 0);
  assert(k.stats.sweep_env_unsat >= 1);
  assert(r.found.empty()); // S-c never concludes from an UNSAT environment
  assert(!k.inconsistent); // and never touches the kernel
}

} // namespace

int main(int argc, char **argv) {
  const uint32_t rounds =
      argc > 1 ? static_cast<uint32_t>(strtoul(argv[1], nullptr, 10)) : 20000;
  test_non_rup_equivalence();
  test_seed_symmetry();
  test_env_provenance();
  test_limits();
  test_known_equivalence();
  test_seeds();
  test_env_unsat();
  test_brute_force_oracle(rounds);
  printf("test_sweep: all tests passed\n");
  return 0;
}
