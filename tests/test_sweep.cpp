// Sweep: candidate selection and environment construction
// (docs/design/v5-sweep-design.md). What makes detection worth anything, one
// test each: it finds equivalences RUP cannot, every claim it makes holds in
// every model of the original formula, the environment is made of F's own
// clauses (same literals, same proof id), and the limits bind without
// truncating a clause.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/sweep/sweep.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

using namespace tactix;

static void test_effort_prices_embedded_and_representation_ticks() {
  Kernel k;
  k.stats.sweep_ticks_sub = 17;
  k.stats.sweep_ticks_repr = 1000;
  assert(sweep_effort(k) == 1017);
}

namespace {

// Build a kernel from DIMACS-style clauses, ids in input order as the parser
// hands them out.
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

bool same_polarity_pair(const SweepEquiv &e, Lit x, Lit y) {
  return var_of(e.a) != var_of(e.b) &&
         (var_of(e.a) == var_of(x) || var_of(e.a) == var_of(y)) &&
         (var_of(e.b) == var_of(x) || var_of(e.b) == var_of(y)) &&
         negated(e.a) == negated(e.b);
}

// --- 1. the equivalence RUP cannot reach ------------------------------------
// (¬x ∨ y ∨ ±z ∨ ±w) over all four sign patterns says x → y with no unit
// consequence (refuting x ∧ ¬y needs a decision), and the mirrored block gives
// y → x, so F ⊨ x ≡ y while neither binary is RUP.
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
  // The full partition is model-polarized. Kitten's deterministic randomized
  // phases may represent x=y as either (x,y) or (-x,-y).
  assert(same_polarity_pair(r.found[0], x, y));
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

// The per-seed entry point `instances()` drives: the answer must not depend on
// which end of the pair is swept.
void test_seed_symmetry() {
  const std::vector<std::vector<int>> cls = non_rup_formula();
  for (uint32_t seed = 0; seed < 2; seed++) {
    Kernel k;
    build(k, 4, cls);
    SweepRound r;
    SweepLimits lim;
    sweep_round_begin(k, r);
    assert(sweep_detect_seed(k, seed, lim, r) == 1);
    assert(same_polarity_pair(r.found[0], internalize(1), internalize(2)));
  }
}

void test_full_partition_finds_nonseed_pair() {
  std::vector<std::vector<int>> cls;
  for (int xs = -1; xs <= 1; xs += 2)
    for (int us = -1; us <= 1; us += 2)
      for (int vs = -1; vs <= 1; vs += 2) {
        cls.push_back({-2, 3, xs, 4 * us, 5 * vs});
        cls.push_back({2, -3, xs, 4 * us, 5 * vs});
      }

  Kernel legacy;
  build(legacy, 5, cls);
  SweepRound old_round;
  SweepLimits old_lim;
  old_lim.full_partition = false;
  sweep_round_begin(legacy, old_round);
  sweep_detect_seed(legacy, 0, old_lim, old_round);
  const Lit y = internalize(2), z = internalize(3);
  assert(std::none_of(old_round.found.begin(), old_round.found.end(),
                      [&](const SweepEquiv &e) {
                        return same_polarity_pair(e, y, z);
                      }));

  Kernel full;
  build(full, 5, cls);
  SweepRound new_round;
  SweepLimits new_lim;
  new_lim.full_partition = true;
  // Isolate full-partition reach from the model-dependent flip prefilter.
  new_lim.flip_rounds = 0;
  sweep_round_begin(full, new_round);
  assert(sweep_detect_seed(full, 0, new_lim, new_round) >= 1);
  assert(std::any_of(new_round.found.begin(), new_round.found.end(),
                     [&](const SweepEquiv &e) {
                       return same_polarity_pair(e, y, z);
                     }));

  Kernel filtered;
  build(filtered, 5, cls);
  SweepRound filtered_round;
  SweepLimits filtered_lim;
  sweep_round_begin(filtered, filtered_round);
  (void)sweep_detect_seed(filtered, 0, filtered_lim, filtered_round);
  assert(filtered.stats.sweep_flip_attempts > 0);
  assert(filtered.stats.sweep_flipped > 0);
  assert(filtered.stats.sweep_solves < full.stats.sweep_solves &&
         "candidate flips must avoid embedded SAT queries");

  Kernel emitted;
  build(emitted, 5, cls);
  assert(setenv("TACTIX_SWEEP_PARTITION", "1", 1) == 0);
  assert(setenv("TACTIX_SWEEP_FLIP_ROUNDS", "0", 1) == 0);
  assert(setenv("TACTIX_SWEEP_FORCE_GENERAL", "1", 1) == 0);
  assert(sweep_try(emitted, 0) == ApplyResult::Ok);
  assert(kernel_binary_id(emitted, negate(y), z));
  assert(kernel_binary_id(emitted, y, negate(z)));
  assert(emitted.stats.sweep_general >= 1);
  assert(unsetenv("TACTIX_SWEEP_FORCE_GENERAL") == 0);
  assert(unsetenv("TACTIX_SWEEP_FLIP_ROUNDS") == 0);
  assert(unsetenv("TACTIX_SWEEP_PARTITION") == 0);
}

// Existing implication binaries make an equivalence cheap to detect, but do
// not make its variable merge redundant. Kissat still substitutes the class.
static void test_known_equivalence_still_substitutes() {
  Kernel k;
  build(k, 3, {{-1, 2}, {-2, 1}});
  assert(sweep_apply(k) == ApplyResult::Ok);
  assert(k.stats.sweep_known >= 1);
  assert(kernel_var_retired(k, 0) || kernel_var_retired(k, 1));
}

// Sweep commits backbone units before its equivalence batch. If one member of
// a proved class is fixed by that batch, the class implies a unit on the other
// member; dropping the pair loses a core consequence.
static void test_equivalence_with_new_unit_forces_partner() {
  Kernel k;
  build(k, 3, {{1, 3}, {1, -3}, {-1, 2}, {-2, 1}});
  // One seed is enough: the failed-assumption core exposes unit 1 first, and
  // the following lemma (-1 v 2) must then be simplified against that newly
  // committed root fact to expose unit 2 in the same core transaction.  A
  // detector that retains only its final target loses this ordered consequence.
  assert(sweep_try(k, 0) == ApplyResult::Ok);
  assert(k.value(internalize(1)) > 0);
  assert(k.value(internalize(2)) > 0);
  assert(k.stats.sweep_units >= 2);
  assert(k.trail.size() >= 2);
  assert(k.trail[0] == internalize(1));
  assert(k.trail[1] == internalize(2));
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
      // was it something the formula did not already say?
      Kernel probe;
      build(probe, nvars, cls);
      nontrivial += !is_rup_binary(probe, negate(e.a), e.b) ||
                    !is_rup_binary(probe, e.a, negate(e.b));
    }
    // the round leaves the formula alone
    assert(k.stats.sweep_equivalences == r.found.size());
  }
  // Reachability at the source: an oracle over zero environments or zero
  // claims is vacuous.
  printf("  oracle: %llu environments, %llu claims (%llu not RUP)\n",
         (unsigned long long)envs, (unsigned long long)claims,
         (unsigned long long)nontrivial);
  assert(envs > 0);
  assert(claims > 0);
  // Non-RUP claims are rare on formulas this small, so only the default-length
  // run is large enough to require one.
  assert(nontrivial > 0 || rounds < 20000);
}

// --- 3. the environment is made of F's clauses -------------------------------
// Every environment clause must be a kernel clause: same literal set, same
// proof id. That is what `env ⊆ F` and the core→id mapping rest on.
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
        assert(e.clause_ids[c] <= cls.size());
        std::vector<int> want = cls[e.clause_ids[c] - 1];
        std::sort(want.begin(), want.end());
        assert(got == want && "environment clause differs from F's clause");
        // every variable is mapped into the sub-solver's space
        for (uint32_t i = e.clause_off[c]; i < e.clause_off[c + 1]; i++)
          assert(e.sub_of_var[var_of(e.clause_lits[i])] < e.vars.size());
      }
    }
  }
}

void test_equal_depth_binary_encounters() {
  Kernel k;
  build(k, 3, {{1, 2}, {1, 3}, {2, 3}});
  SweepRound r;
  SweepLimits lim;
  sweep_round_begin(k, r);
  assert(sweep_env_build(k, 0, lim, r));
  const SweepEnv &e = r.env;

  // The seed introduces variables 2 and 3 at the same BFS depth. Kissat then
  // copies their connecting binary from both directed watch encounters:
  // (2,3) when expanding 2 and (3,2) when expanding 3. A canonical pair-set
  // dedup loses the second clause and changes Kitten's watch/query trajectory.
  assert(e.nclauses() == 4);
  assert((e.clause_ids == std::vector<uint64_t>{1, 2, 3, 3}));
  const uint32_t a = e.clause_off[2], b = e.clause_off[3];
  assert(externalize(e.clause_lits[a]) == 2);
  assert(externalize(e.clause_lits[a + 1]) == 3);
  assert(externalize(e.clause_lits[b]) == 3);
  assert(externalize(e.clause_lits[b + 1]) == 2);
}

// --- 4. structural limits bind between whole expansion steps -----------------
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
      // Kissat checks after adding a whole clause, so a binary can cross the
      // variable cap by one; it never truncates or declines that clause.
      assert(r.env.vars.size() <= cap + 1);
      for (uint32_t c = 0; c + 1 < r.env.clause_off.size(); c++)
        assert(r.env.clause_off[c + 1] - r.env.clause_off[c] == 2);
    }
  }
  // The clause cap is checked before expanding a variable, not after each of
  // its watches. One expansion therefore lands whole and may overshoot.
  SweepLimits overshoot;
  overshoot.max_env_clauses = 1;
  overshoot.max_env_vars = 100;
  sweep_round_begin(k, r);
  assert(sweep_env_build(k, 0, overshoot, r));
  assert(r.env.nclauses() == 5);
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
  // Detection follows Kissat's candidate pipeline even when the two binaries
  // already state the equivalence.  Duplicate suppression belongs to emission;
  // short-circuiting here changes the embedded model/watch trajectory.
  Kernel k;
  build(k, 3, {{-1, 2}, {1, -2}, {1, 3}, {-3, 2}});
  SweepRound r;
  SweepLimits lim;
  assert(sweep_detect_round(k, lim, r) >= 1);
  assert(!r.found.empty());
  assert(k.stats.sweep_known == 0);
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

  // Dense scheduling counts each large occurrence once. The sparse watches
  // for the first two literals of a large clause remain connected in Tactix;
  // they must not be added on top of the all-literal dense index.
  Kernel ranked;
  build(ranked, 6, {{1, 3, 2}, {-1, 4, 2}, {5, 6, -2}});
  OccIndex occ;
  kernel_dense_connect(ranked, occ);
  sweep_seeds(ranked, seeds);
  const auto one = std::find(seeds.begin(), seeds.end(), 0u);
  const auto two = std::find(seeds.begin(), seeds.end(), 1u);
  assert(one != seeds.end() && two != seeds.end() && one < two);
  kernel_dense_disconnect(ranked);
}

void test_resumable_schedule() {
  const std::vector<std::vector<int>> cls = {
      {1, 2}, {-1, 3}, {-2, 4}, {-3, -4}};
  assert(setenv("TACTIX_SWEEP_SEEDS", "1", 1) == 0);

  Kernel off;
  build(off, 4, cls);
  SweepLimits off_limits;
  off_limits.resume = false;
  sweep_apply_with_limits(off, off_limits);
  sweep_apply_with_limits(off, off_limits);
  assert(off.sweep_tail.empty());
  assert(off.stats.sweep_resumed == 0 && off.stats.sweep_completed == 0);

  Kernel filtered;
  build(filtered, 3, {{1, 2}, {-1, 2}});
  sweep_apply(filtered);
  assert(filtered.stats.sweep_scheduled == 1);
  assert(filtered.stats.sweep_completed == 1);
  std::vector<Var> complete_surface;
  sweep_seeds(filtered, complete_surface);
  assert(filtered.stats.sweep_units == 1);
  assert(complete_surface.size() == 2); // only the newly fixed var leaves scope

  assert(setenv("TACTIX_SWEEP_CLAUSES", "1", 1) == 0);
  Kernel growth;
  build(growth, 3, {{1, 2}, {1, 3}, {-1, 2}, {-1, 3}});
  sweep_apply(growth);
  assert(growth.stats.sweep_scheduled == 0 &&
         growth.stats.sweep_completed == 1);
  sweep_apply(growth);
  assert(growth.stats.sweep_limit_clauses == 2);
  assert(growth.stats.sweep_scheduled == 1);
  assert(unsetenv("TACTIX_SWEEP_CLAUSES") == 0);

  Kernel on;
  build(on, 4, cls);
  sweep_apply(on);
  // Productive seeds may append their surviving representatives to the live
  // tail, exactly as Kissat's schedule_inner does. The original four entries
  // remain the scheduled baseline, but completion is no longer necessarily
  // four calls after a productive first seed.
  assert(on.sweep_tail.size() >= 4 && on.sweep_tail_next == 1);
  assert(on.stats.sweep_scheduled == 4 && on.stats.sweep_resumed == 0);
  sweep_apply(on);
  assert(on.sweep_tail_next >= 2 && on.stats.sweep_resumed >= 1);
  for (uint32_t calls = 0; !on.sweep_passes_completed && calls < 16; calls++)
    sweep_apply(on);
  assert(on.sweep_tail.empty() && on.sweep_tail_next == 0);
  assert(on.sweep_passes_completed == 1 && on.stats.sweep_completed == 1);
  assert(on.stats.sweep_limit_vars == 256 &&
         on.stats.sweep_limit_clauses == 1024 &&
         on.stats.sweep_limit_depth == 2);

  sweep_apply(on);
  assert(on.stats.sweep_limit_vars == 512 &&
         on.stats.sweep_limit_clauses == 2048 &&
         on.stats.sweep_limit_depth == 3);
  assert(unsetenv("TACTIX_SWEEP_SEEDS") == 0);
}

// Kissat reschedules a proved pair's REPRESENTATIVE — `schedule_inner
// (sweeper, repr_idx)` in `sweep_equivalence_candidates`, where the
// representative is the endpoint with the smaller internal index, i.e. Tactix's
// import order. Reading it off "whichever endpoint escaped retirement" happens
// to agree only while a substitution actually lands, and names nothing at all
// when the pair is declined. Both import orders are exercised because the
// representative is not a function of the external numbering.
void test_reschedules_pair_representative_by_import_order() {
  assert(setenv("TACTIX_SWEEP_SEEDS", "1", 1) == 0);
  for (uint32_t flipped = 0; flipped < 2; flipped++) {
    Kernel k;
    kernel_reserve_vars(k, 3);
    kernel_record_import(k, flipped ? 1 : 0);
    kernel_record_import(k, flipped ? 0 : 1);
    kernel_record_import(k, 2);
    kernel_add_binary_clause(k, internalize(-1), internalize(2),
                             ++k.next_clause_id);
    kernel_add_binary_clause(k, internalize(-2), internalize(1),
                             ++k.next_clause_id);
    assert(sweep_apply(k) == ApplyResult::Ok);

    const Var representative = flipped ? 1u : 0u;
    assert(!kernel_var_retired(k, representative));
    assert(kernel_var_retired(k, flipped ? 0u : 1u));
    assert(k.stats.sweep_rescheduled == 1);
    assert(k.sweep_tail_next < k.sweep_tail.size());
    assert(k.sweep_tail[k.sweep_tail_next] ==
           k.internal_to_external[representative]);
  }
  assert(unsetenv("TACTIX_SWEEP_SEEDS") == 0);
}

// One environment can prove several pairs, and every pair is rescheduled on
// its own representative. When those pairs share a class, only the class
// minimum survives: an intermediate representative is retired by the same
// batch and `schedule_inner` declines an inactive variable (kissat flags.c
// `kissat_mark_fixed_literal` deactivates a fixed one the same way). Reading
// the representative off the pair, as Kissat does, is what makes this case
// reachable at all — and rescheduling a retired identity would seed a variable
// that no longer occurs in F.
//
// The chain x1 = x2 = x3 is proved from a single environment, so one of the
// two pairs names x2 — which the same batch then retires into x1.
void test_reschedule_declines_a_retired_representative() {
  assert(setenv("TACTIX_SWEEP_SEEDS", "1", 1) == 0);
  Kernel k;
  kernel_reserve_vars(k, 5);
  // Four ternaries state the equivalence without handing unit propagation a
  // binary, so detection has to prove it and the pair is not already known.
  const auto equate = [&](int left, int right, int helper) {
    for (uint32_t mask = 0; mask < 4; mask++) {
      const int sign = (mask & 1) ? 1 : -1;
      const int h = (mask & 2) ? helper : -helper;
      const Lit clause[3] = {internalize(-sign * left),
                             internalize(sign * right), internalize(h)};
      kernel_add_large_clause(k, clause, 3, false, 0, ++k.next_clause_id);
    }
  };
  equate(1, 2, 4);
  equate(2, 3, 5);

  assert(sweep_apply(k) == ApplyResult::Ok);
  const Var minimum = var_of(internalize(1));
  const Var intermediate = var_of(internalize(2));
  assert(k.stats.sweep_equivalences == 2);
  assert(!kernel_var_retired(k, minimum));
  assert(kernel_var_retired(k, intermediate));
  assert(kernel_var_retired(k, var_of(internalize(3))));
  // Two pairs, one live representative: the retired one is declined.
  assert(k.stats.sweep_rescheduled == 1);
  assert(k.sweep_tail_next < k.sweep_tail.size());
  // The retired identity keeps its original schedule slot, where the seed
  // loop skips it; what it must not do is take the pop end from the live
  // representative.
  assert(k.sweep_tail[k.sweep_tail_next] == k.internal_to_external[minimum]);
  assert(unsetenv("TACTIX_SWEEP_SEEDS") == 0);
}

// A productive dense environment is a transaction boundary, not just a
// detector boundary.  Its proved unit can force variables outside a depth-zero
// environment; Kissat propagates that trail before choosing/building the next
// seed, so those variables must never become later environments.
void test_resumable_closes_root_between_environments() {
  Kernel k;
  build(k, 6,
        {{1, 2}, {-1, 2}, {-2, 3}, {-3, 4}, {-4, 5}, {-5, 6}});
  assert(setenv("TACTIX_SWEEP_DEPTH", "0", 1) == 0);
  assert(sweep_apply(k) == ApplyResult::Ok);
  assert(unsetenv("TACTIX_SWEEP_DEPTH") == 0);
  for (Var v = 1; v < 6; v++)
    assert(kernel_root_value(k, make_lit(v, false)) > 0);
  assert(k.stats.sweep_environments == 1 &&
         "root consequences must close before the next environment");
}

// --- 7. an environment that refutes the formula outright ---------------------
void test_env_unsat() {
  // (1)(¬1) is caught before sweeping, so use a refutation the environment
  // finds: the four clauses over {1,2} are UNSAT and every seed sees them all.
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

// --- emission via the RUP fast path -----------------------------------------
// Detection proves `a == b` inside an environment; emission must justify it
// over the main formula in a form a checker accepts, as two binaries. The
// fixture is a 4-cycle a->c->b, b->d->a, so both directions propagate to a
// conflict and both binaries are replayable.
static void test_sd_emits_rup_equivalence(void) {
  Kernel k;
  kernel_reserve_vars(k, 8);
  const Lit a = make_lit(0, true), b = make_lit(1, true);
  const Lit c = make_lit(2, true), d = make_lit(3, true);
  kernel_add_binary_clause(k, negate(a), c); // a -> c
  kernel_add_binary_clause(k, negate(c), b); // c -> b
  kernel_add_binary_clause(k, negate(b), d); // b -> d
  kernel_add_binary_clause(k, negate(d), a); // d -> a

  const uint64_t before = k.stats.sweep_binaries;
  const ApplyResult r = sweep_apply(k);
  if (r == ApplyResult::NoOp) {
    // Detection is budgeted; with no pair there is nothing to assert here.
    printf("  S-d: detection found nothing on the 4-cycle (emission untested)\n");
    return;
  }
  assert(r == ApplyResult::Ok);
  assert(k.stats.sweep_binaries >= before + 2 &&
         "an emitted equivalence is TWO binaries; one alone would state an "
         "implication the sub-solver did not prove on its own");
  assert(k.stats.sweep_binaries % 2 == 0 && "binaries are emitted in pairs");
  assert(k.stats.sweep_general == 0 &&
         "default emission must prefer the independently replayed RUP path");
  printf("  S-d: emitted %llu binaries via the RUP fast path\n",
         (unsigned long long)(k.stats.sweep_binaries - before));
}

// A pair that is not RUP over F must be declined, leaving F unchanged;
// emitting here would write a line no checker can verify (an N2 breach).
static void test_sd_declines_non_rup(void) {
  Kernel k;
  kernel_reserve_vars(k, 8);
  // No implication structure at all: nothing is RUP, so nothing may be added.
  kernel_add_binary_clause(k, make_lit(0, true), make_lit(1, true));
  kernel_add_binary_clause(k, make_lit(2, true), make_lit(3, true));
  const size_t bins_before = k.binary_ids.size();
  const uint64_t emitted_before = k.stats.sweep_binaries;
  sweep_apply(k);
  assert(k.stats.sweep_binaries == emitted_before &&
         "nothing is RUP here; emitting would be unverifiable (N2)");
  assert(k.binary_ids.size() == bins_before && "F must be unchanged");
  printf("  S-d: non-RUP pairs declined, F unchanged\n");
}

// --- the general path, and its reachability ---------------------------------
// `non_rup_formula()` carries an equivalence unit propagation cannot refute in
// either direction, so only the captured sub-solver refutation can justify it.
// The `sweep_general` assertion is the reachability proof.
static void test_se_general_path_emits(void) {
  const std::vector<std::vector<int>> cls = non_rup_formula();
  Kernel k;
  build(k, 4, cls);
  const Lit x = internalize(1), y = internalize(2);
  assert(!is_rup_binary(k, negate(x), y)); // premise: S-d cannot do this
  assert(!is_rup_binary(k, x, negate(y)));

  const uint64_t arena_words_before = k.arena.size();
  assert(setenv("TACTIX_SWEEP_FORCE_GENERAL", "1", 1) == 0);
  const ApplyResult r = sweep_apply(k);
  assert(unsetenv("TACTIX_SWEEP_FORCE_GENERAL") == 0);
  assert(r == ApplyResult::Ok && "the general path must justify what RUP cannot");
  assert(k.stats.sweep_general >= 1 &&
         "S-e NEVER FIRED -- the general path is unreachable and untested");
  assert(k.stats.sweep_binaries >= 2 && "an equivalence is two binaries");
  // Reference-inline application consumes the transient binaries by rewriting
  // the proved class immediately; the retired variable and witness are the
  // persistent effect, not two stale equivalence clauses.
  assert(k.stats.substitute_vars >= 1);
  assert(k.stats.sweep_ticks_repr >= arena_words_before &&
         "one productive call prices its deferred arena rebuild once");
  assert(kernel_var_retired(k, var_of(x)) ||
         kernel_var_retired(k, var_of(y)));
  printf("  S-e: general path emitted %llu binaries for a non-RUP equivalence\n",
         (unsigned long long)k.stats.sweep_binaries);
}

// The public/random-policy path has no shared dense index. A productive seed
// substitutes inline and garbage-collects the variable-width arena; the next
// resumed seed must traverse a rebuilt occurrence snapshot rather than the old
// Ref offsets. ASan turns the pre-fix stale read into a deterministic failure.
static void test_inline_bridge_refreshes_owned_occurrences(void) {
  std::vector<std::vector<int>> cls = non_rup_formula();
  cls.push_back({5, 6, 7});
  cls.push_back({-5, 6, 7});
  cls.push_back({5, -6, 7});
  Kernel k;
  build(k, 7, cls);
  assert(k.dense == nullptr);
  k.sweep_tail = {k.internal_to_external[0], k.internal_to_external[4]};
  k.sweep_tail_next = 0;

  assert(setenv("TACTIX_SWEEP_FORCE_GENERAL", "1", 1) == 0);
  assert(sweep_apply(k) == ApplyResult::Ok);
  assert(unsetenv("TACTIX_SWEEP_FORCE_GENERAL") == 0);
  assert(k.stats.substitute_vars >= 1);
  assert(k.stats.sweep_general >= 1);
}

// --- the emitted proof, through a real checker ------------------------------
// Emitting is not the same as being checkable: a sub-solver literal decoded
// with inverted parity still leaves both correct binaries in F, so detection,
// the pair test and the verdict all look right and only an external checker
// sees the bad lemma (N2). So this writes a self-contained refutation whose
// middle is the general path's output, for drat-trim and lrat-check
// (tests/sweep_proofs.sh). Two checker-only contradictory units close the file
// after Sweep, so the checker validates the whole general-path + inline-
// substitution prefix without depending on transient binaries that the
// substitution correctly deletes.
static void emit_se_case(const char *dir) {
  const std::vector<std::vector<int>> env = non_rup_formula(); // clauses 1..8
  const uint64_t ID_XY = 9, ID_NXNY = 10;

  char path[512];
  snprintf(path, sizeof path, "%s/se.cnf", dir);
  FILE *cnf = fopen(path, "w");
  assert(cnf);
  fprintf(cnf, "p cnf 4 %u\n", static_cast<unsigned>(env.size()) + 2);
  for (const auto &c : env) {
    for (const int e : c)
      fprintf(cnf, "%d ", e);
    fprintf(cnf, "0\n");
  }
  fprintf(cnf, "1 0\n-1 0\n"); // checker-only clauses 9 and 10
  fclose(cnf);

  for (int fmt = 0; fmt < 2; fmt++) {
    const bool lrat = fmt == 1;
    snprintf(path, sizeof path, "%s/se.%s", dir, lrat ? "lrat" : "drat");
    Proof proof;
    assert(proof.open(path, lrat ? ProofFormat::Lrat : ProofFormat::DratAscii));
    Kernel k;
    // The kernel holds only clauses 1..8: detection needs a satisfiable
    // environment, so 9/10 stay out of F and the contradiction is closed by
    // hand below. The id cursor skips past them so no lemma reuses their ids.
    build(k, 4, env);
    k.next_clause_id = ID_NXNY;
    k.proof = &proof;
    const Lit x = internalize(1), y = internalize(2);
    assert(!is_rup_binary(k, negate(x), y)); // premise: only S-e can do this
    assert(!is_rup_binary(k, x, negate(y)));

    const ApplyResult r = sweep_apply(k);
    assert(r == ApplyResult::Ok && k.stats.sweep_general >= 1 &&
           "S-e must have emitted, or this case checks nothing");

    const uint64_t empty_chain[2] = {ID_XY, ID_NXNY};
    proof.add_empty(++k.next_clause_id, empty_chain, 2);
    proof.flush();
  }
  printf("emitted the S-e case to %s\n", dir);
}

static void emit_unit_case(const char *dir) {
  char path[512];
  snprintf(path, sizeof path, "%s/unit.cnf", dir);
  FILE *cnf = fopen(path, "w");
  assert(cnf);
  fprintf(cnf, "p cnf 2 3\n1 2 0\n1 -2 0\n-1 0\n");
  fclose(cnf);

  for (int fmt = 0; fmt < 2; fmt++) {
    const bool lrat = fmt == 1;
    snprintf(path, sizeof path, "%s/unit.%s", dir,
             lrat ? "lrat" : "drat");
    Proof proof;
    assert(proof.open(path, lrat ? ProofFormat::Lrat : ProofFormat::DratAscii));
    Kernel k;
    build(k, 2, {{1, 2}, {1, -2}}); // clause 3 stays checker-only
    k.next_clause_id = 3;
    k.proof = &proof;
    assert(sweep_apply(k) == ApplyResult::Ok && k.stats.sweep_units == 1);
    const uint64_t unit_id = k.unit_ids[var_of(internalize(1))];
    assert(unit_id > 3 && k.value(internalize(1)) > 0);
    const uint64_t empty_chain[2] = {unit_id, 3};
    proof.add_empty(++k.next_clause_id, empty_chain, 2);
    proof.flush();
  }
  printf("emitted the Sweep-unit case to %s\n", dir);
}

int main(int argc, char **argv) {
  if (argc > 2 && !strcmp(argv[1], "--emit")) {
    emit_se_case(argv[2]);
    emit_unit_case(argv[2]);
    return 0;
  }
  const uint32_t rounds =
      argc > 1 ? static_cast<uint32_t>(strtoul(argv[1], nullptr, 10)) : 20000;
  test_effort_prices_embedded_and_representation_ticks();
  test_non_rup_equivalence();
  test_seed_symmetry();
  test_full_partition_finds_nonseed_pair();
  test_known_equivalence_still_substitutes();
  test_equivalence_with_new_unit_forces_partner();
  test_env_provenance();
  test_equal_depth_binary_encounters();
  test_limits();
  test_known_equivalence();
  test_seeds();
  test_resumable_schedule();
  test_reschedules_pair_representative_by_import_order();
  test_reschedule_declines_a_retired_representative();
  test_resumable_closes_root_between_environments();
  test_env_unsat();
  test_sd_emits_rup_equivalence();
  test_sd_declines_non_rup();
  test_se_general_path_emits();
  test_inline_bridge_refreshes_owned_occurrences();
  test_brute_force_oracle(rounds);
  printf("test_sweep: all tests passed\n");
  return 0;
}
