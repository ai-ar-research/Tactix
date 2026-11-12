// Sweep: candidate selection, environment construction, emission and the
// scheduling contracts. Detection finds equivalences RUP cannot, every claim
// holds in every model, environments are F's own clauses, and limits never
// truncate a clause. A numeric argument sets the brute-force oracle's round
// count (default 20000); `--emit DIR` writes the cases for sweep_proofs.sh.
#include "unit_test.hpp"
#include "clause_oracle.hpp"
#include "dimacs_text.hpp"
#include "test_io.hpp"
#include "kernel/kernel.hpp"
#include "proof/proof.hpp"
#include "rule_test_utils.hpp"
#include "rules/congruence/congruence.hpp" // congruence_replay_rup
#include "rules/propagate/propagate.hpp"
#include "rules/sweep/sweep.hpp"
#include "rules/substitute/substitute.hpp"

#include <random>

// Internal emission boundary (the public action closes root propagation first).
namespace tactix {
void sweep_capture_core_units_for_test(Kernel &, SweepDirProof &, SweepRound &,
                                       const SweepLimits &);
ApplyResult sweep_emit_round_for_test(Kernel &, SweepRound &,
                                      const SweepLimits &);
}

// Whether `from` reaches `to` over the binary implication graph; edge p -> q is
// the clause (¬p ∨ q).
static bool implies_through_binaries(const Kernel &k, Lit from, Lit to) {
  std::vector<uint8_t> seen(2 * static_cast<size_t>(k.nvars), 0);
  std::vector<Lit> stack{from};
  seen[from] = 1;
  while (!stack.empty()) {
    const Lit p = stack.back();
    stack.pop_back();
    if (p == to)
      return true;
    for (const WatchEntry w : k.watches.entries(negate(p)))
      if (w.binary() && !seen[w.other()]) {
        seen[w.other()] = 1;
        stack.push_back(w.other());
      }
  }
  return false;
}

TEST(test_core_prefixes_share_one_immutable_derivation) {
  auto program = std::make_shared<SweepProofSteps>();
  const SubStep *first = program->retain(SubStep{3, {0}, {1}, {}});
  const SubStep *last = program->retain(SubStep{5, {2}, {3}, {}});
  SweepDirProof selected;
  selected.program = program;
  selected.program_steps = std::make_shared<const std::vector<const SubStep *>>(
      std::vector<const SubStep *>{first, last});
  selected.shared_step_count = 2;
  program->retain(SubStep{4, {4}, {3}, {}});
  for (uint32_t id = 6; id < 4096; ++id)
    program->retain(SubStep{id, {id}, {id - 1}, {}});
  assert(program->retain(SubStep{3, {0}, {1}, {}}) == first);
  const auto view = selected.steps_view();
  assert(view[0].id == 3 && view[1].id == 5 && view[1].chain[0] == 3);
  const auto found = std::ranges::lower_bound(view, 5u, {}, &SubStep::id);
  assert(found - view.begin() == 1);
  // Kitten's synthetic target does not consume its ID; a later clause may
  // reuse it with a different payload.
  const SubStep *chain_only = program->retain(SubStep{3, {0}, {2}, {}});
  assert(chain_only != first && chain_only->chain[0] == 2 && first->chain[0] == 1);
  const SubStep *reused = program->retain(SubStep{3, {8}, {2}, {}});
  assert(reused != chain_only && reused->lits[0] == 8 && chain_only->lits[0] == 0);
  const size_t retained = program->steps.size();
  assert(program->retain(SubStep{3, {8}, {2}, {}}) == reused);
  assert(program->steps.size() == retained && first->lits[0] == 0);
  SweepDirProof prefix;
  prefix.program = selected.program;
  prefix.program_steps = selected.program_steps;
  prefix.shared_step_count = 1;
  selected = {};
  // ID reuse in a fresh environment leaves an older captured prefix intact.
  program = std::make_shared<SweepProofSteps>();
  program->retain(SubStep{3, {8}, {2}, {}});
  assert(prefix.steps_view()[0].lits[0] == 0);
}

TEST(test_unbounded_clause_limit_sizes_from_formula) {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_large_clause(k, lits_of({1, 2, 3}).data(), 3, false, 0);
  SweepLimits limits;
  limits.max_env_clauses = UINT32_MAX;
  limits.max_env_clauses_ceiling = UINT32_MAX;
  limits.max_seeds = 1;
  limits.round_ticks = 1;
  SweepRound round;
  sweep_round_begin(k, round);
  assert(sweep_env_build(k, 0, limits, round));
  // An unbounded cap keeps the reference-sized initial table (never UINT32_MAX).
  assert(round.env.key_slots.size() == 4096);
  assert(sweep_apply_with_limits(k, limits) != ApplyResult::Unsat);
  kernel_check_invariants(k);
}

TEST(test_dense_entry_flushes_only_irredundant_satisfied_clauses) {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit root = internalize(1);
  k.unit_ids[var_of(root)] = ++k.next_clause_id;
  kernel_assign_unit(k, root);
  const Lit irred_lits[3] = {root, internalize(2), internalize(3)};
  const Lit red_lits[3] = {root, internalize(3), internalize(4)};
  const Ref irred = kernel_add_large_clause(
      k, irred_lits, 3, false, 0, ++k.next_clause_id);
  const Ref red = kernel_add_large_clause(k, red_lits, 3, true, 2,
                                          ++k.next_clause_id);

  SweepRound round;
  SweepLimits limits;
  limits.max_seeds = 1;
  (void)sweep_detect_round(k, limits, round);

  assert(clause_at(k.arena, irred)->garbage());
  assert(!clause_at(k.arena, red)->garbage());
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

// `a ≡ b` in every model of `cls` (vacuously true for an UNSAT formula, which
// is exactly the entailment the rule may use).
bool equivalent_in_all_models(const std::vector<std::vector<int>> &cls,
                              uint32_t nvars, Lit a, Lit b) {
  for (const uint64_t bits : models(cls, nvars)) {
    const auto value = [&](Lit l) {
      return (((bits >> var_of(l)) & 1u) != 0) != negated(l);
    };
    if (value(a) != value(b))
      return false;
  }
  return true;
}

// `count` random clauses of 2..4 distinct, non-complementary literals over
// 1..nvars (shorter draws are dropped).
std::vector<std::vector<int>> random_clauses(std::mt19937 &rng, uint32_t nvars,
                                             uint32_t count) {
  std::vector<std::vector<int>> cls;
  for (uint32_t c = 0; c < count; c++) {
    const uint32_t size = 2 + rng() % 3;
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
  return cls;
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

TEST(test_non_rup_equivalence) {
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
  const auto &pair = r.proofs.front();
  assert(pair.fwd.program && pair.fwd.program == pair.rev.program);
  const auto forward = pair.fwd.steps_view();
  const auto reverse = pair.rev.steps_view();
  // A captured clause keeps its creation pose across later queries.
  for (size_t i = 0; i + 1 < forward.size(); ++i)
    for (size_t j = 0; j + 1 < reverse.size(); ++j)
      if (forward[i].id == reverse[j].id)
        assert(&forward[i] == &reverse[j]);
  const uint32_t saved_id = forward[0].id;
  const auto saved_lits = forward[0].lits;
  r.env.proof_steps.reset();
  assert(forward[0].id == saved_id && forward[0].lits == saved_lits);
  // Kitten's randomized phases may state x=y as (x,y) or (-x,-y).
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

// The external stop bounds an unbounded model-flip pass, also after the
// embedded solver returned a model.
TEST(test_unbounded_flip_rounds_observe_stop) {
  Kernel k;
  build(k, 4, non_rup_formula());
  SweepLimits limits;
  limits.flip_rounds = UINT32_MAX;
  k.stop_fuzz = 1; // every kernel termination poll fires deterministically
  assert(sweep_try_with_limits(k, 0, limits) != ApplyResult::Unsat);
  assert(k.stats.rule_interrupted > 0);
  assert(k.level == 0 && !k.conflict_valid);
}

TEST(test_proof_capture_limit_is_caller_controlled) {
  const std::vector<std::vector<int>> cls = non_rup_formula();
  Kernel limited;
  build(limited, 4, cls);
  SweepRound limited_round;
  SweepLimits limit;
  limit.max_proof_steps = 1;
  assert(sweep_detect_round(limited, limit, limited_round) == 1);
  assert(limited_round.proofs.size() == 1);
  assert(!limited_round.proofs[0].fwd.ok ||
         !limited_round.proofs[0].rev.ok);

  Kernel unbounded;
  build(unbounded, 4, cls);
  SweepRound unbounded_round;
  limit.max_proof_steps = 0;
  assert(sweep_detect_round(unbounded, limit, unbounded_round) == 1);
  assert(unbounded_round.proofs.size() == 1);
  assert(unbounded_round.proofs[0].fwd.ok &&
         unbounded_round.proofs[0].rev.ok);
}

TEST(test_full_partition_finds_nonseed_pair) {
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
  SweepLimits emitted_limits;
  emitted_limits.full_partition = true;
  emitted_limits.flip_rounds = 0;
  emitted_limits.force_general_proof = true;
  assert(sweep_try_with_limits(emitted, 0, emitted_limits) == ApplyResult::Ok);
  assert(kernel_binary_id(emitted, negate(y), z));
  assert(kernel_binary_id(emitted, y, negate(z)));
  assert(emitted.stats.sweep_general >= 1);
}

// Sweep keeps a known class active and its bridge live; Substitute retires it.
TEST(test_known_equivalence_still_substitutes) {
  Kernel k;
  build(k, 3, {{-1, 2}, {-2, 1}});
  assert(sweep_apply(k) == ApplyResult::Ok);
  assert(k.stats.sweep_known >= 1);
  assert(!kernel_var_retired(k, 0) && !kernel_var_retired(k, 1));
  assert(substitute_apply(k) == ApplyResult::Ok);
  assert(kernel_var_retired(k, 0) || kernel_var_retired(k, 1));
}

// Kissat's `sweep_binary` declines a non-representative endpoint, so the live
// representative map keeps the preserved bridges out of later environments.
TEST(test_environment_skips_nonrepresentative_bridges) {
  Kernel k;
  build(k, 3, {{1, 3}, {-1, -3}});
  SweepRound round;
  SweepLimits limits;
  sweep_round_begin(k, round);

  const Lit one = internalize(1), two = internalize(2);
  kernel_add_binary_clause(k, negate(one), two, ++k.next_clause_id);
  kernel_add_binary_clause(k, one, negate(two), ++k.next_clause_id);
  const ApplyResult rewritten = substitute_apply_sweep_equivalences(
      k, {{one, two}}, round.substitution);
  assert(rewritten == ApplyResult::Ok || rewritten == ApplyResult::NoOp);
  round.repr[two] = one;
  round.repr[negate(two)] = negate(one);

  assert(sweep_env_build(k, var_of(one), limits, round));
  assert(round.env.vars.size() == 2);
  assert(round.env.nclauses() == 2);
  for (const Lit lit : round.env.clause_lits)
    assert(var_of(lit) != var_of(two));
}

// A proved class with a member fixed by the same batch forces the partner.
TEST(test_equivalence_with_new_unit_forces_partner) {
  Kernel k;
  build(k, 3, {{1, 3}, {1, -3}, {-1, 2}, {-2, 1}});
  // The core exposes unit 1, then (-1 v 2) simplified against it yields 2.
  assert(sweep_try(k, 0) == ApplyResult::Ok);
  assert(k.value(internalize(1)) > 0);
  assert(k.value(internalize(2)) > 0);
  assert(k.stats.sweep_units >= 2);
  assert(k.trail.size() >= 2);
  assert(k.trail[0] == internalize(1));
  assert(k.trail[1] == internalize(2));
}

// A saved-core original that becomes unit after earlier units of the same
// seed, posed directly (the builder omits clauses already unit).
static SweepDirProof original_core_unit_proof(const Kernel &k) {
  SweepDirProof proof;
  proof.ok = true;
  proof.vars = {0, 1, 2, 3};
  proof.orig_ids = {1};
  proof.formula_epoch = k.touch_epoch;
  proof.core_clauses.push_back(
      {1, false, {sub_lit(0, true), sub_lit(1, false),
                  sub_lit(2, false), sub_lit(3, false)}});
  return proof;
}

static void original_core_unit_env(SweepRound &round) {
  SweepEnv &env = round.env;
  env.vars = {0, 1, 2, 3};
  env.sub_of_var = {0, 1, 2, 3};
  env.clause_ids = {1};
  env.clause_lits = {internalize(-1), internalize(2), internalize(3),
                     internalize(4)};
  env.clause_off = {0, 4};
}

TEST(test_original_core_unit_capture) {
  Kernel not_unit;
  build(not_unit, 4, {{-1, 2, 3, 4}, {-2}, {-3}});
  SweepRound declined;
  SweepLimits limits;
  sweep_round_begin(not_unit, declined);
  original_core_unit_env(declined);
  SweepDirProof not_unit_proof = original_core_unit_proof(not_unit);
  sweep_capture_core_units_for_test(not_unit, not_unit_proof, declined, limits);
  assert(declined.units.empty()); // -1 and 4 are both still non-false

  Kernel k;
  build(k, 4, {{-1, 2, 3, 4}, {-2}, {-3}, {-4}});
  SweepRound round;
  sweep_round_begin(k, round);
  original_core_unit_env(round);
  SweepDirProof proof = original_core_unit_proof(k);
  sweep_capture_core_units_for_test(k, proof, round, limits);
  assert(!round.units.empty());
  const SweepUnitProof &unit = round.units.front();
  assert(unit.unit == internalize(-1));
  const auto steps = unit.proof.steps_view();
  assert(steps.size() == 1);
  assert(steps[0].chain.size() == 1 && steps[0].chain[0] == 1);
  assert(sweep_emit_round_for_test(k, round, limits) == ApplyResult::Ok);
  assert(k.value(internalize(-1)) > 0);
  assert(k.stats.sweep_units >= 1);
}

// --- 2. every claim is true --------------------------------------------------
void test_brute_force_oracle(uint32_t rounds) {
  std::mt19937 rng(20260805);
  uint64_t claims = 0, nontrivial = 0, envs = 0;
  for (uint32_t it = 0; it < rounds; it++) {
    const uint32_t nvars = 3 + rng() % 4; // 3..6
    const uint32_t nclauses = 2 + rng() % 10;
    const std::vector<std::vector<int>> cls = random_clauses(rng, nvars, nclauses);
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
  // An oracle over zero environments or claims is vacuous.
  printf("  oracle: %llu environments, %llu claims (%llu not RUP)\n",
         (unsigned long long)envs, (unsigned long long)claims,
         (unsigned long long)nontrivial);
  assert(envs > 0);
  assert(claims > 0);
  // Non-RUP claims are rare here; only the default-length run requires one.
  assert(nontrivial > 0 || rounds < 20000);
}

// --- 3. the environment is made of F's clauses -------------------------------
// Every environment clause must be a kernel clause: same literal set, same
// proof id. That is what `env ⊆ F` and the core→id mapping rest on.
TEST(test_env_provenance) {
  std::mt19937 rng(4242);
  for (uint32_t it = 0; it < 200; it++) {
    const uint32_t nvars = 4 + rng() % 5;
    const std::vector<std::vector<int>> cls = random_clauses(rng, nvars, 12);
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

TEST(test_equal_depth_binary_encounters) {
  Kernel k;
  build(k, 3, {{1, 2}, {1, 3}, {2, 3}});
  SweepRound r;
  SweepLimits lim;
  sweep_round_begin(k, r);
  assert(sweep_env_build(k, 0, lim, r));
  const SweepEnv &e = r.env;

  // Variables 2 and 3 share a BFS depth, so Kissat copies their binary from
  // both directed watch encounters: (2,3) and (3,2).
  assert(e.nclauses() == 4);
  assert((e.clause_ids == std::vector<uint64_t>{1, 2, 3, 3}));
  const uint32_t a = e.clause_off[2], b = e.clause_off[3];
  assert(externalize(e.clause_lits[a]) == 2);
  assert(externalize(e.clause_lits[a + 1]) == 3);
  assert(externalize(e.clause_lits[b]) == 3);
  assert(externalize(e.clause_lits[b + 1]) == 2);
}

// --- 4. structural limits bind between whole expansion steps -----------------
TEST(test_limits) {
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
  // The clause cap is checked before expanding a variable, so one expansion
  // lands whole and may overshoot.
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
TEST(test_known_equivalence) {
  // Detection runs Kissat's candidate pipeline even for an already stated
  // equivalence; duplicate suppression belongs to emission.
  Kernel k;
  build(k, 3, {{-1, 2}, {1, -2}, {1, 3}, {-3, 2}});
  SweepRound r;
  SweepLimits lim;
  assert(sweep_detect_round(k, lim, r) >= 1);
  assert(!r.found.empty());
  assert(k.stats.sweep_known == 0);
}

// --- 6. the seed surface -----------------------------------------------------
TEST(test_seeds) {
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

  // Dense scheduling counts each large occurrence once, not again through the
  // sparse watches.
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

// --- 7. an environment that refutes the formula outright ---------------------
TEST(test_env_unsat) {
  // Every seed sees the four UNSAT clauses over {1,2}.
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

// --- emission: a pair that is not RUP over F is declined and F is unchanged (N2).
TEST(test_sd_declines_non_rup) {
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
}

// RUP admission gives one verdict in every proof format (corpus 026).
TEST(test_rup_admission_is_format_independent) {
  // Neither direction of `1 == 2` is RUP; the control's unit 1 is.
  const std::vector<std::vector<int>> non_rup = non_rup_formula();
  const std::vector<std::vector<int>> rup = {{1, 2}, {1, -2}};

  for (int mode = 0; mode < 3; ++mode) { // no proof, DSR, PBP
    Proof proof;
    if (mode)
      assert(proof.open("/dev/null", mode == 1 ? ProofFormat::DsrAscii
                                               : ProofFormat::PbpStreaming));
    Kernel hard;
    build(hard, 4, non_rup);
    if (mode)
      hard.proof = &proof;
    const Lit x = internalize(1), y = internalize(2);
    const Lit entailed[2] = {negate(x), y};
    assert(!congruence_replay_rup(hard, entailed, 2) &&
           "an entailed but non-RUP lemma must be declined in every format");
    Kernel easy;
    build(easy, 2, rup);
    if (mode)
      easy.proof = &proof;
    const Lit unit = internalize(1);
    assert(congruence_replay_rup(easy, &unit, 1) &&
           "a genuinely RUP lemma must be admitted in every format");
  }
}

// RUP replay closes an inherited pending root trail before its speculative
// levels; otherwise a retracted assumption leaves a unit behind the cursor.
TEST(test_rup_replay_closes_inherited_root_debt) {
  Kernel k;
  kernel_reserve_vars(k, 2);
  const Lit root = internalize(1), implied = internalize(2);
  kernel_add_binary_clause(k, negate(root), implied, ++k.next_clause_id);
  k.unit_ids[var_of(root)] = ++k.next_clause_id;
  kernel_assign_unit(k, root);
  assert(!kernel_propagation_fixpoint(k));

  assert(congruence_replay_rup(k, &implied, 1));
  assert(k.value(implied) > 0);
  assert(kernel_propagation_fixpoint(k));
  kernel_check_invariants(k);
}

// Without a dense index, the seed after an inline substitution traverses a
// rebuilt occurrence snapshot, not stale Ref offsets (an ASan regression).
TEST(test_inline_bridge_refreshes_owned_occurrences) {
  std::vector<std::vector<int>> cls = non_rup_formula();
  cls.push_back({5, 6, 7});
  cls.push_back({-5, 6, 7});
  cls.push_back({5, -6, 7});
  Kernel k;
  build(k, 7, cls);
  assert(k.dense == nullptr);
  // The later-import environment runs first; Substitute retires the union in
  // Kissat's first-import order.
  k.sweep_tail = {k.internal_to_external[4], k.internal_to_external[0]};
  k.sweep_tail_next = 0;
  k.sweep_incomplete = true;
  k.sweep_marked.resize(k.external_to_internal.size(), 0);
  for (const Var external : k.sweep_tail)
    k.sweep_marked[external] = 1;

  SweepLimits limits;
  limits.force_general_proof = true;
  assert(sweep_apply_with_limits(k, limits) == ApplyResult::Ok);
  assert(k.stats.substitute_vars == 0);
  assert(k.stats.sweep_general >= 1);
  assert(substitute_apply(k) == ApplyResult::Ok);
  assert(k.stats.substitute_vars >= 1);
  for (size_t i = 1; i < k.retired_events.size(); i++)
    assert(kernel_import_var_order(k, k.retired_events[i - 1]) <
           kernel_import_var_order(k, k.retired_events[i]));
}

// The dense path rewrites in place: two productive environments cause no
// arena collection.
TEST(test_dense_call_collects_once_across_productive_environments) {
  std::vector<std::vector<int>> cls = non_rup_formula();
  for (const std::vector<int> &source : non_rup_formula()) {
    std::vector<int> shifted;
    for (const int lit : source)
      shifted.push_back(lit > 0 ? lit + 4 : lit - 4);
    cls.push_back(std::move(shifted));
  }
  Kernel k;
  build(k, 8, cls);
  OccIndex occ;
  kernel_dense_connect(k, occ);
  k.sweep_tail = {k.internal_to_external[4], k.internal_to_external[0]};
  k.sweep_tail_next = 0;
  k.sweep_incomplete = true;
  k.sweep_marked.resize(k.external_to_internal.size(), 0);
  for (const Var external : k.sweep_tail)
    k.sweep_marked[external] = 1;
  const uint64_t generation = k.arena_generation;
  SweepLimits limits;
  limits.force_general_proof = true;
  assert(sweep_apply_with_limits(k, limits) == ApplyResult::Ok);
  assert(k.stats.sweep_equivalences >= 2);
  assert(k.stats.substitute_vars == 0);
  assert(k.arena_generation == generation);
  assert(substitute_apply(k) == ApplyResult::Ok);
  assert(k.stats.substitute_vars >= 2);
  for (size_t i = 1; i < k.retired_events.size(); i++)
    assert(kernel_import_var_order(k, k.retired_events[i - 1]) <
           kernel_import_var_order(k, k.retired_events[i]));
  kernel_dense_disconnect(k);
}

// Without a dense ledger, the connected binary rewrite keeps every live pair
// mirrored and named by a proof id.
TEST(test_sessionless_substitution_keeps_binaries_whole) {
  std::vector<std::vector<int>> cls = non_rup_formula(); // 1 == 2, vars 1..4
  // A retiring endpoint against a stable one in each polarity, and a pair whose
  // image is a tautology.
  cls.push_back({2, 5});
  cls.push_back({-2, 6});
  cls.push_back({1, -2});
  Kernel k;
  build(k, 6, cls);
  assert(k.dense == nullptr); // the state this regression is about

  SweepLimits limits;
  limits.force_general_proof = true;
  const ApplyResult r = sweep_apply_with_limits(k, limits);
  assert(r == ApplyResult::Ok);
  assert(k.stats.sweep_equivalences >= 1 &&
         "the connected Sweep substitution under test never ran");

  size_t live_pairs = 0;
  for (Lit lit = 0; lit < 2 * k.nvars; lit++)
    for (const WatchEntry w : k.watches.entries(lit)) {
      if (!w.binary())
        continue;
      const Lit other = w.other();
      assert(kernel_binary_copies(k, other, lit, 1) == 1 &&
             "substitution left a half-watched binary");
      assert(kernel_binary_id(k, lit, other) != 0 &&
             "live binary has no proof identity");
      live_pairs++;
    }
  assert(live_pairs >= 2 && "no binary survived; nothing was checked");
  kernel_check_invariants(k);
  assert(substitute_apply(k) == ApplyResult::Ok);
  assert(k.stats.substitute_vars >= 1);
}

// --- the emitted proof, through a real checker ------------------------------
// Only an external checker sees a lemma decoded with inverted parity (N2), so
// each case writes DIR/<name>.cnf (the kernel clauses plus checker-only
// clauses) and a refutation DIR/<name>.dsr whose middle is Sweep's output,
// closed by the checker-only contradiction (tests/sweep_proofs.sh).
template <class Body>
static void emit_case(const char *dir, const char *name, uint32_t nvars,
                      const std::vector<std::vector<int>> &kernel_clauses,
                      const std::vector<std::vector<int>> &checker_only,
                      Body body) {
  std::vector<std::vector<int>> cnf = kernel_clauses;
  cnf.insert(cnf.end(), checker_only.begin(), checker_only.end());
  const std::string stem = std::string(dir) + "/" + name;
  write_file((stem + ".cnf").c_str(), dimacs_text(nvars, cnf));
  Proof proof;
  assert(proof.open((stem + ".dsr").c_str(), ProofFormat::DsrAscii));
  Kernel k;
  build(k, nvars, kernel_clauses);
  k.next_clause_id = cnf.size(); // no lemma reuses a checker-only id
  k.proof = &proof;
  body(k);
  proof.add_empty(++k.next_clause_id);
  proof.flush();
  printf("emitted the %s case to %s\n", name, dir);
}

static void emit_cases(const char *dir) {
  // The general path: the satisfiable environment 1..8 is F; units 9/10 close.
  emit_case(dir, "se", 4, non_rup_formula(), {{1}, {-1}}, [](Kernel &k) {
    const Lit x = internalize(1), y = internalize(2);
    assert(!is_rup_binary(k, negate(x), y)); // premise: only S-e can do this
    assert(!is_rup_binary(k, x, negate(y)));
    assert(sweep_apply(k) == ApplyResult::Ok && k.stats.sweep_general >= 1 &&
           "S-e must have emitted, or this case checks nothing");
  });
  emit_case(dir, "unit", 2, {{1, 2}, {1, -2}}, {{-1}}, [](Kernel &k) {
    assert(sweep_apply(k) == ApplyResult::Ok && k.stats.sweep_units == 1);
    const uint64_t unit_id = k.unit_ids[var_of(internalize(1))];
    assert(unit_id > 3 && k.value(internalize(1)) > 0);
  });
  // The exact original-core path: root units 2--4 justify simplifying the
  // saved original clause.
  emit_case(dir, "original-core-unit", 4, {{-1, 2, 3, 4}, {-2}, {-3}, {-4}},
            {{1}}, [](Kernel &k) {
    SweepRound round;
    SweepLimits limits;
    sweep_round_begin(k, round);
    original_core_unit_env(round);
    SweepDirProof captured = original_core_unit_proof(k);
    sweep_capture_core_units_for_test(k, captured, round, limits);
    assert(!round.units.empty() && round.units.front().unit == internalize(-1));
    assert(sweep_emit_round_for_test(k, round, limits) == ApplyResult::Ok);
    const uint64_t unit_id = k.unit_ids[var_of(internalize(-1))];
    assert(unit_id > 5 && k.value(internalize(-1)) > 0);
  });
  // A chained batch whose middle winner no later pair names: the first pair's
  // bridge carries the intermediate winner through checker-validated mirror
  // lemmas, and Substitute owns the retirement.
  emit_case(dir, "bridge-chain", 6,
            {{1, 5}, {2, 5}, {3, 6}, {4, 6}, // one occurrence per member
             {-3, 4}, {3, -4},               // pair x3 = x4
             {-3, 2}, {3, -2},               // pair x3 = x2
             {-4, 1}, {4, -1}},              // pair x4 = x1
            {{5}, {-5}}, [](Kernel &k) {
    OccIndex occ;
    kernel_dense_connect(k, occ);
    SweepSubstitutionSession session;
    session.begin(k);
    const Lit one = internalize(1), two = internalize(2),
              three = internalize(3), four = internalize(4);
    assert(substitute_apply_sweep_equivalences(
               k, {{three, four}, {three, two}, {four, one}}, session) ==
           ApplyResult::Ok);
    assert(k.retired_events.empty());
    assert(substitute_apply(k) == ApplyResult::Ok);
    assert(kernel_var_retired(k, var_of(two)) &&
           kernel_var_retired(k, var_of(three)) &&
           kernel_var_retired(k, var_of(four)) &&
           "the bridge chain must let Substitute rediscover the whole class");
    (void)session.collect(k);
    kernel_dense_disconnect(k);
  });
  // Renaming an absorbed literal maps both endpoints of an early bridge to
  // intermediate members ({-6, 7} becomes {-4, 7}); its RUP check needs both
  // mirror lemmas.
  emit_case(dir, "bridge-two-hint", 8,
            {{3, 8}, {4, 8}, {6, 8}, {7, 8}, // one occurrence per member
             {-6, 7}, {6, -7},               // pair x6 = x7
             {-3, 4}, {3, -4},               // pair x3 = x4
             {-4, 6}, {4, -6}},              // pair x4 = x6
            {{8}, {-8}}, [](Kernel &k) {
    OccIndex occ;
    kernel_dense_connect(k, occ);
    SweepSubstitutionSession session;
    session.begin(k);
    const Lit three = internalize(3), four = internalize(4),
              six = internalize(6), seven = internalize(7);
    assert(substitute_apply_sweep_equivalences(
               k, {{six, seven}, {three, four}, {four, six}}, session) ==
           ApplyResult::Ok);
    assert(k.retired_events.empty());
    assert(kernel_binary_live(k, negate(four), seven) &&
           kernel_binary_live(k, four, negate(seven)) &&
           "the early bridge must carry both intermediate members");
    assert(substitute_apply(k) == ApplyResult::Ok);
    assert(kernel_var_retired(k, var_of(four)) &&
           kernel_var_retired(k, var_of(six)) &&
           kernel_var_retired(k, var_of(seven)) &&
           !kernel_var_retired(k, var_of(three)));
    (void)session.collect(k);
    kernel_dense_disconnect(k);
  });
}
// All three variables remain active across Sweep's connected rewrite.
// Reusing a pre-rewrite endpoint in a later pair must preserve every original
// equivalence, even before the caller chooses to run Substitute.
TEST(test_sequential_equivalence_images) {
  uint64_t productive = 0;
  for (bool dense : {false, true})
    for (bool partition : {false, true})
      for (unsigned signs = 0; signs != 8; ++signs) {
        std::vector<Var> order{0, 1, 2};
        do {
          Kernel k;
          kernel_reserve_vars(k, 3);
          for (const Var v : order) kernel_record_import(k, v);
          const Lit a = make_lit(0, signs & 1);
          const Lit b = make_lit(1, signs & 2);
          const Lit c = make_lit(2, signs & 4);
          for (const auto &[x, y] :
               std::vector<std::pair<Lit, Lit>>{{a, negate(b)},
                                               {b, negate(c)},
                                               {c, negate(a)}})
            kernel_add_binary_clause(k, x, y, ++k.next_clause_id);
          OccIndex occ;
          if (dense) kernel_dense_connect(k, occ);
          SweepLimits lim;
          lim.full_partition = partition;
          // Repeating individual applications crosses fresh round/session
          // boundaries as well as the within-environment merge boundary.
          for (const Var seed : {2u, 1u, 0u, 2u}) {
            assert(sweep_try_with_limits(k, seed, lim) != ApplyResult::Unsat);
            for (const Lit x : {a, b, c})
              for (const Lit y : {a, b, c})
                if (x != y && !implies_through_binaries(k, x, y)) {
                  fprintf(stderr, "bridge lost: dense=%d partition=%d signs=%u order=%u,%u,%u seed=%u from=%u to=%u\n", dense, partition, signs, order[0], order[1], order[2], seed, x, y);
                  assert(false && "Sweep must preserve the complete equivalence");
                }
            kernel_check_invariants(k);
          }
          productive += k.stats.sweep_binaries;
          if (dense) kernel_dense_disconnect(k);
        } while (std::next_permutation(order.begin(), order.end()));
      }
  assert(productive > 0);
}

int main(int argc, char **argv) {
  if (argc > 2 && !strcmp(argv[1], "--emit")) {
    emit_cases(argv[2]);
    return 0;
  }
  run_tests();
  const uint32_t rounds =
      argc > 1 ? static_cast<uint32_t>(strtoul(argv[1], nullptr, 10)) : 20000;
  test_brute_force_oracle(rounds);
  printf("test_sweep: all tests passed\n");
  return 0;
}
