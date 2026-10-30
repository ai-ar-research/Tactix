// Walk unit tests (plan §5.2 policy op). The walker repairs phases the
// defaults get wrong at root and is deterministic.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "policies/ops/walk/walk.hpp"
#include "rules/registry.hpp"
#include "strategy/random_online.hpp"

#include <cassert>
#include <bit>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace tactix;

static bool near(double a, double b) { return std::fabs(a - b) < 1e-12; }

static void test_configurable_score_curve() {
  strategy::WalkParameters reference;
  assert(reference.valid());
  assert(near(walk_break_base(reference, 3.0, 1), 2.5));
  assert(near(walk_break_base(reference, 3.5, 1), 2.675));
  assert(near(walk_break_base(reference, 3.5, 2), 2.0));
  const double old_curve[6][2] = {{0.0, 2.00}, {3.0, 2.50},
                                  {4.0, 2.85}, {5.0, 3.70},
                                  {6.0, 5.10}, {7.0, 7.40}};
  for (const double average : {0.01, 1.0, 3.5, 7.0, 11.25}) {
    size_t i = 0;
    while (i + 2 < 6 &&
           (old_curve[i][0] > average || old_curve[i + 1][0] < average))
      ++i;
    const double old =
        (old_curve[i + 1][1] - old_curve[i][1]) *
            (average - old_curve[i][0]) /
            (old_curve[i + 1][0] - old_curve[i][0]) +
        old_curve[i][1];
    assert(std::bit_cast<uint64_t>(walk_break_base(reference, average, 1)) ==
           std::bit_cast<uint64_t>(old));
  }

  strategy::WalkParameters custom = reference;
  custom.even_break_base_milli = 4000;
  custom.score_curve = {{0, 1000}, {4000, 5000}};
  assert(custom.valid());
  assert(near(walk_break_base(custom, 2.0, 1), 3.0));
  assert(near(walk_break_base(custom, 2.0, 2), 4.0));

  custom.score_curve[1].average_clause_size_milli = 0;
  assert(!custom.valid());
  custom = reference;
  custom.score_curve[0].break_base_milli = 0;
  assert(!custom.valid());
  custom = reference;
  custom.even_break_base_milli = 0;
  assert(!custom.valid());
  custom.even_break_base_milli = 1;
  assert(custom.valid());
  custom = reference;
  custom.score_curve.front().average_clause_size_milli = 1;
  assert(!custom.valid());
  custom = reference;
  custom.score_curve[2].break_base_milli = 1;
  assert(!custom.valid());
}

// Pairwise-conflicting variables plus one covering clause: a model sets
// exactly one variable true, so the all-positive default phase is worst-case.
static void build_conflict_cover(Kernel &k, int n) {
  kernel_reserve_vars(k, n);
  for (int i = 1; i <= n; i++)
    for (int j = i + 1; j <= n; j++)
      kernel_add_binary_clause(k, internalize(-i), internalize(-j));
  std::vector<Lit> cover;
  for (int i = 1; i <= n; i++)
    cover.push_back(internalize(i));
  kernel_add_large_clause(k, cover.data(), (uint32_t)cover.size(), false, 0);
}

static void add_nae3(Kernel &k, int a, int b, int c) {
  const Lit positive[] = {internalize(a), internalize(b), internalize(c)};
  const Lit negative[] = {internalize(-a), internalize(-b), internalize(-c)};
  kernel_add_large_clause(k, positive, 3, false, 0);
  kernel_add_large_clause(k, negative, 3, false, 0);
}

static void test_configuration_checking_profiles_and_structure() {
  strategy::WalkParameters invalid;
  invalid.initialization =
      strategy::WalkParameters::Initialization::Negative;
  assert(!invalid.valid());
  invalid.algorithm =
      strategy::WalkParameters::Algorithm::ConfigurationChecking;
  assert(invalid.valid());
  invalid.seed_candidate = 256;
  assert(!invalid.valid());

  constexpr int edges[][3] = {
      {1, 2, 3}, {2, 3, 4}, {3, 4, 5}, {1, 4, 5}};
  for (const auto initialization : {
           strategy::WalkParameters::Initialization::BestPhases,
           strategy::WalkParameters::Initialization::Negative,
           strategy::WalkParameters::Initialization::Random}) {
    Kernel k;
    ScopedHeuristics heuristics(k);
    kernel_reserve_vars(k, 5);
    for (const auto &edge : edges) add_nae3(k, edge[0], edge[1], edge[2]);
    assert(walk_complementary_ternary_fraction_permille(k) == 1000);
    k.h->saved.assign(k.nvars, 1);
    k.h->target.assign(k.nvars, 1);
    k.h->best.assign(k.nvars, -1);
    strategy::WalkParameters profile;
    profile.step_budget = 1000000;
    profile.random_seed = 17;
    profile.algorithm =
        strategy::WalkParameters::Algorithm::ConfigurationChecking;
    profile.initialization = initialization;
    profile.maximum_tries = 3;
    profile.flips_per_try = 10000;
    assert(profile.valid());
    strategy::ScopedWalkParameters selected(k, profile);
    assert(walk_apply(k) == ApplyResult::Ok);
    assert(k.stats.walk_minimum == 0 && k.stats.walk_improved == 1);
    for (const auto &edge : edges) {
      const bool a = k.h->saved[edge[0] - 1] > 0;
      const bool b = k.h->saved[edge[1] - 1] > 0;
      const bool c = k.h->saved[edge[2] - 1] > 0;
      assert(!(a == b && b == c));
    }
    kernel_check_invariants(k);
  }

  Kernel unpaired;
  ScopedHeuristics unpaired_heuristics(unpaired);
  kernel_reserve_vars(unpaired, 4);
  const Lit first[] = {internalize(1), internalize(2), internalize(3)};
  const Lit second[] = {internalize(2), internalize(3), internalize(4)};
  kernel_add_large_clause(unpaired, first, 3, false, 0);
  kernel_add_large_clause(unpaired, second, 3, false, 0);
  assert(walk_complementary_ternary_fraction_permille(unpaired) == 0);

  Kernel imbalanced;
  ScopedHeuristics imbalanced_heuristics(imbalanced);
  kernel_reserve_vars(imbalanced, 3);
  const Lit positive[] = {internalize(1), internalize(2), internalize(3)};
  const Lit negative[] = {internalize(-1), internalize(-2), internalize(-3)};
  for (unsigned copy = 0; copy != 9; ++copy)
    kernel_add_large_clause(imbalanced, positive, 3, false, 0);
  kernel_add_large_clause(imbalanced, negative, 3, false, 0);
  assert(walk_complementary_ternary_fraction_permille(imbalanced) == 200);

  Kernel uniform;
  ScopedHeuristics uniform_heuristics(uniform);
  kernel_reserve_vars(uniform, 4);
  kernel_add_binary_clause(uniform, internalize(1), internalize(2));
  kernel_add_binary_clause(uniform, internalize(-1), internalize(3));
  const Lit positive_four[] = {
      internalize(1), internalize(2), internalize(3), internalize(4)};
  const Lit mixed_four[] = {
      internalize(-1), internalize(2), internalize(3), internalize(4)};
  kernel_add_large_clause(uniform, positive_four, 4, false, 0);
  kernel_add_large_clause(uniform, mixed_four, 4, false, 0);
  kernel_add_large_clause(uniform, positive_four, 4, true, 0);
  const auto uniform_census = walk_uniform_negative_census(uniform);
  assert(uniform_census.clauses == 4);
  assert(uniform_census.unsatisfied == 2);

  strategy::RandomOnlineConfig random_config;
  random_config.rules = {"Walk"};
  random_config.maximum_sequence_length = 1;
  bool saw_probsat = false;
  bool saw_configuration_checking[3] = {false, false, false};
  for (uint64_t seed = 1; seed <= 256; ++seed) {
    strategy::OnlineStrategyProgram program;
    std::string error;
    assert(strategy::random_online_strategy_program(
        unpaired, seed, random_config, program, &error));
    assert(program.sequence.size() == 1);
    const auto &parameters = std::get<strategy::WalkParameters>(
        program.sequence.front().parameters);
    assert(parameters.valid());
    if (parameters.algorithm == strategy::WalkParameters::Algorithm::ProbSat) {
      saw_probsat = true;
    } else {
      const auto index = static_cast<uint32_t>(parameters.initialization);
      assert(index < 3);
      saw_configuration_checking[index] = true;
      const std::string encoded =
          strategy::serialize_online_strategy_program(program);
      assert(encoded.find(":walk4(") != std::string::npos);
      strategy::OnlineStrategyProgram decoded;
      const bool decoded_ok = strategy::deserialize_online_strategy_program(
          encoded, decoded, &error);
      if (!decoded_ok)
        std::fprintf(stderr, "walk4 decode failed: %s: %s\n",
                     error.c_str(), encoded.c_str());
      assert(decoded_ok);
      assert(strategy::serialize_online_strategy_program(decoded) == encoded);
    }
  }
  assert(saw_probsat && saw_configuration_checking[0] &&
         saw_configuration_checking[1] && saw_configuration_checking[2]);
}

static void test_break_base_domain_terminates() {
  for (const uint32_t base : {1u, 25u, 232u, 999u, 1000u, 1001u, 1500u,
                              1999u, 2000u}) {
    Kernel k;
    ScopedHeuristics heuristics(k);
    build_conflict_cover(k, 3);
    k.walk_even_break_base_milli = base;
    k.walk_score_curve = {{0, base}, {3000, base}};
    assert(walk_apply_steps(k, 1) != ApplyResult::Conflict);
    assert(k.stats.walks == 1 && k.stats.walk_steps > 0);
  }
}

// A valid curve interpolates to one ULP above 1 at the conflict-cover's
// average 9/4. Precomputing until underflow then takes an astronomical number
// of entries even though this three-variable formula has maximum break 2.
// Do not reject/round the curve or reduce the caller's requested work.
static void test_near_unit_interpolated_base() {
  strategy::WalkParameters p;
  p.score_curve = {{0, 136}, {3000, 1288}};
  assert(p.valid());
  const double cb = walk_break_base(p, 2.25, 1);
  assert(cb == std::nextafter(1.0, 2.0));
  const double base = 1 / cb;
  assert(base < 1 && base > 0.999999999);
  for (const uint64_t steps : {UINT64_C(0), UINT64_C(1), UINT64_C(1000000)}) {
    Kernel k;
    ScopedHeuristics heuristics(k);
    build_conflict_cover(k, 3);
    k.walk_score_curve = p.score_curve;
    assert(walk_guard(k));
    const auto result = walk_apply_steps(k, steps);
    assert(k.stats.walks == 1);
    if (!steps) {
      assert(result == ApplyResult::NoOp);
      assert(k.stats.walk_steps == 0 && k.stats.walk_flips == 0);
    } else {
      assert(result == ApplyResult::Ok);
      assert(k.stats.walk_steps > 0 && k.stats.walk_flips > 0);
      if (steps > 1) {
        assert(k.stats.walk_minimum == 0);
        unsigned trues = 0;
        for (const int8_t phase : k.h->saved)
          trues += phase > 0;
        assert(trues == 1); // independently satisfies the conflict cover
      }
    }
  }
}

// the walk finds the model and writes it into the saved phases
static void test_finds_model() {
  Kernel k;
  ScopedHeuristics _h(k);
  build_conflict_cover(k, 8);
  for (Var v = 0; v < k.nvars; v++) {
    assert(k.h->saved[v] == 0); // no assignment has established a saved phase
    assert(!negated(heuristics_decide_lit(k, v))); // initial phase is positive
  }

  assert(walk_guard(k));
  assert(walk_apply(k) == ApplyResult::Ok); // improved
  assert(k.stats.walks == 1 && k.stats.walk_improved == 1);
  assert(k.stats.walk_flips > 0);

  int trues = 0;
  for (Var v = 0; v < k.nvars; v++)
    trues += (k.h->saved[v] > 0);
  assert(trues == 1); // exactly one: every binary satisfied, cover satisfied
  for (Var v = 0; v < k.nvars; v++)
    assert(k.h->target[v] == k.h->saved[v]); // the rephase tail ran
  assert(k.h->target_assigned == 0);
  assert(k.stats.rephased == 1);
}

// `H`-only: no clause, trail, level or proof state may move
static void test_kernel_untouched() {
  Kernel k;
  ScopedHeuristics _h(k);
  build_conflict_cover(k, 6);
  const size_t arena = k.arena.size();
  const size_t trail = k.trail.size();
  const uint32_t level = k.level;
  const uint64_t confl = k.stats.conflicts, props = k.stats.propagations;
  walk_apply(k);
  assert(k.arena.size() == arena);
  assert(k.trail.size() == trail);
  assert(k.level == level);
  assert(k.stats.conflicts == confl && k.stats.propagations == props);
  assert(!k.inconsistent);
}

// Walk is part of the root phase. A caller in search must explicitly Restart.
static void test_rejects_non_root() {
  Kernel k;
  ScopedHeuristics _h(k);
  build_conflict_cover(k, 6);
  kernel_assign_decision(k, internalize(3)); // x3 true at level 1
  assert(k.level == 1);
  assert(!walk_guard(k));
}

// the walker is a pure function of kernel state: same formula, same result
static void test_deterministic() {
  int8_t first[10] = {0};
  for (int run = 0; run < 2; run++) {
    Kernel k;
    ScopedHeuristics _h(k);
    build_conflict_cover(k, 10);
    walk_apply(k);
    if (!run)
      for (Var v = 0; v < k.nvars; v++)
        first[v] = k.h->saved[v];
    else
      for (Var v = 0; v < k.nvars; v++)
        assert(k.h->saved[v] == first[v]);
  }
}

// a starved budget terminates, and nothing to improve reports NoOp, not Ok
static void test_budget_and_noop() {
  Kernel k;
  ScopedHeuristics _h(k);
  build_conflict_cover(k, 8);
  assert(walk_apply_steps(k, 1) != ApplyResult::Conflict); // terminates
  Kernel k2;
  ScopedHeuristics _h2(k2);
  kernel_reserve_vars(k2, 3);
  kernel_add_binary_clause(k2, internalize(1), internalize(2));
  assert(walk_apply(k2) == ApplyResult::NoOp); // default phase satisfies it
  assert(k2.stats.walk_improved == 0);
}

// Walk's non-null dense transition saves each binary from the lower compact
// literal and replays that stack on sparse resume. Stable kernel identities can
// disagree with compact/import order after retirement, so the shared endpoint
// below must receive var 1 before var 0 even though the clauses were inserted
// in the opposite order. No witness literals or formula profile are involved.
static void test_resume_uses_compact_literal_order() {
  Kernel k;
  ScopedHeuristics _h(k);
  kernel_reserve_vars(k, 3);
  kernel_record_import(k, 1);
  kernel_record_import(k, 0);
  kernel_record_import(k, 2);
  const Lit shared = internalize(3);
  kernel_add_binary_clause(k, internalize(1), shared);
  kernel_add_binary_clause(k, internalize(2), shared);

  const auto partners = [&] {
    std::vector<Lit> result;
    for (const WatchEntry watch : k.watches.entries(shared))
      if (watch.binary())
        result.push_back(watch.other());
    return result;
  };
  assert((partners() == std::vector<Lit>{internalize(1), internalize(2)}));
  assert(walk_apply_steps(k, 0) != ApplyResult::Conflict);
  assert((partners() == std::vector<Lit>{internalize(2), internalize(1)}));
}

// Kissat reconnects large clauses by scanning its physical source arena.
// Tactix can append a replacement after later clauses while preserving the
// replacement's inherited source position, so Walk resume has to replay the
// generation-local source position rather than Tactix's host-arena position.
static void test_resume_large_watches_uses_logical_clause_position() {
  Kernel k;
  ScopedHeuristics _h(k);
  kernel_reserve_vars(k, 7);
  const Lit shared = internalize(-1);
  const Lit a[3] = {shared, internalize(2), internalize(3)};
  const Lit b[3] = {shared, internalize(4), internalize(5)};
  const Lit replacement[3] = {shared, internalize(6), internalize(7)};
  const Ref source = kernel_add_large_clause(k, a, 3, false, 0, 301);
  kernel_add_large_clause(k, b, 3, false, 0, 302);
  kernel_inherit_clause_position(k, 303, source);
  kernel_delete_clause(k, clause_at(k.arena, source));
  kernel_add_large_clause(k, replacement, 3, false, 0, 303);

  assert(walk_apply_steps(k, 1) != ApplyResult::Conflict);
  std::vector<uint64_t> ids;
  for (const WatchEntry watch : k.watches.entries(shared))
    if (!watch.binary())
      ids.push_back(clause_at(k.arena, watch.ref())->id());
  assert((ids == std::vector<uint64_t>{303, 302}));
  kernel_check_invariants(k);
}

// Kissat's Walk dense entry retains only the first copy of a binary pair.
// Mirrored watch entries are not duplicates; two complete insertions are.
static void test_dense_entry_deduplicates_binary_pairs() {
  Kernel k;
  ScopedHeuristics _h(k);
  kernel_reserve_vars(k, 3);
  const Lit a = internalize(1), b = internalize(-2);
  kernel_add_binary_clause(k, a, b, ++k.next_clause_id);
  kernel_add_binary_clause(k, a, b, ++k.next_clause_id);
  kernel_add_binary_clause(k, internalize(2), internalize(3),
                           ++k.next_clause_id);
  assert(kernel_binary_copies(k, a, b) == 2);

  const uint64_t deleted_before = k.stats.binaries_deleted;
  assert(walk_apply_steps(k, 1) != ApplyResult::Conflict);
  assert(kernel_binary_copies(k, a, b) == 1);
  // the later copy is deleted as a clause (kissat_delete_binary), not merely
  // dropped from the watch lists
  assert(k.stats.binaries_deleted == deleted_before + 1);
  kernel_check_invariants(k);
}

// registered as a policy op with a working instance pair
static void test_registry() {
  const int id = tactix_rule_id("Walk");
  assert(id >= 0);
  const RuleVTable &vt = tactix_rules()[id];
  assert(vt.kind == ActionKind::PolicyOp);
  Kernel k;
  ScopedHeuristics _h(k);
  build_conflict_cover(k, 8);
  assert(vt.guard(k));
  uint64_t insts[4];
  assert(vt.instances(k, insts, 4) == 1);
  assert(insts[0] == INSTANCE_CANONICAL);
  assert(vt.apply_instance(k, INSTANCE_CANONICAL) == ApplyResult::Ok);
  assert(vt.apply_instance(k, 50000) != ApplyResult::Conflict); // any payload
}

// a kernel without `H` has no phases to improve, so the guard must reject
static void test_no_heuristics_guard() {
  Kernel k;
  build_conflict_cover(k, 4);
  assert(k.h == nullptr);
  assert(!walk_guard(k));
}

// Kissat's dense entry (dense.c:flush_large_watches) deletes root-satisfied
// binaries before init_walker reads BINIRR_CLAUSES, so such binaries are
// invisible to the walk: neither in its table nor in the score-table average.
// Two kernels that differ only by root-satisfied binaries must walk identically.
static void test_root_satisfied_binaries_do_not_reach_the_walk() {
  Kernel plain, extra;
  ScopedHeuristics _p(plain), _e(extra);
  build_conflict_cover(plain, 7);
  build_conflict_cover(extra, 7);
  kernel_reserve_vars(extra, 9);
  // a root unit satisfies six binaries that only `extra` carries
  kernel_add_binary_clause(extra, internalize(8), internalize(9));
  for (int i = 1; i <= 6; i++)
    kernel_add_binary_clause(extra, internalize(8), internalize(-i));
  kernel_assign_unit(extra, internalize(8));
  assert(!kernel_propagate(extra)); // no conflict
  assert(extra.level == 0 && extra.value(internalize(8)) > 0);
  assert(walk_apply(plain) == ApplyResult::Ok);
  assert(walk_apply(extra) == ApplyResult::Ok);
  assert(plain.stats.walk_flips == extra.stats.walk_flips);
  assert(plain.stats.walk_steps == extra.stats.walk_steps);
  assert(plain.stats.walk_minimum == extra.stats.walk_minimum);
  for (Var v = 0; v < plain.nvars; v++)
    assert(plain.h->saved[v] == extra.h->saved[v]);
}

static void test_private_seed_budget_and_export() {
  // Three binary exclusions and a ternary cover: nine evaluated literals.
  // Seed preparation costs 3 phase slots + 3 identity slots + 2*9 literals.
  constexpr uint64_t preparation = 24;
  for (uint64_t allowance = 0; allowance <= preparation; ++allowance) {
    Kernel k;
    ScopedHeuristics h(k);
    build_conflict_cover(k, 3);
    k.h->saved = {1, 1, 1};
    k.h->target = {-1, -1, -1};
    k.h->best = {-1, 1, -1};
    kernel_offer_local_search_hint(k, strategy::LocalSearchKind::DirectCsp, {1, 0, 0});
    strategy::WalkParameters p;
    p.seed_candidate = k.local_search_hints[0].id;
    p.step_budget = allowance;
    k.h->target_candidate.assign(3, p.seed_candidate);
    const auto saved = k.h->saved, target = k.h->target, best = k.h->best;
    {
      strategy::ScopedWalkParameters scope(k, p);
      const auto result = walk_apply(k);
      if (allowance < preparation) {
        assert(result == ApplyResult::NoOp);
        assert(k.h->saved == saved && k.h->target == target && k.h->best == best);
        assert((k.h->target_candidate ==
                std::vector<uint64_t>(3, p.seed_candidate)));
        assert(k.stats.walk_seed_budget_exhausted == 1);
      } else {
        assert(result == ApplyResult::Ok);
        assert((k.h->saved == std::vector<int8_t>{1, -1, -1}));
        assert(k.h->target == k.h->saved && k.h->best == best);
        assert(k.stats.walk_seed_ordinary_unsat == 3);
        assert(k.stats.walk_seed_candidate_unsat == 0);
        assert(k.stats.walk_seed_imports == 1);
        assert(k.stats.walk_seed_imported_phases == 3);
        assert(k.stats.walk_seed_improved == 0);
        assert(k.stats.walk_seed_exported == 1);
        assert((k.h->target_candidate == std::vector<uint64_t>(3, 0)));
        assert(k.stats.local_search_target_overwritten_walk == 3);
        assert(k.stats.walk_flips == 0); // an already-good seed must export
      }
    }
    assert(k.walk_seed_candidate == 0);
    assert(k.stats.walk_steps == allowance);
    assert(k.stats.walk_seed_steps == allowance);
    assert(k.level == 0 && k.trail.empty());
    kernel_check_invariants(k);
  }
  Kernel k;
  ScopedHeuristics h(k);
  build_conflict_cover(k, 3);
  k.h->saved = {1, -1, -1}; // ordinary phases are already a model
  k.h->best = {-1, -1, 1};
  const auto saved = k.h->saved, best = k.h->best;
  kernel_offer_local_search_hint(k, strategy::LocalSearchKind::DirectCsp, {1, 1, 1});
  k.walk_seed_candidate = k.local_search_hints[0].id;
  assert(walk_apply_steps(k, preparation) == ApplyResult::NoOp);
  assert(k.h->saved == saved && k.h->target == saved && k.h->best == best);
  assert((k.h->target_candidate == std::vector<uint64_t>(3, 0)));
  assert(!k.stats.local_search_target_install_attempts &&
         !k.stats.local_search_target_installs);
  assert(k.stats.walk_seed_ordinary_unsat == 0 && k.stats.walk_seed_candidate_unsat == 3);
  const auto walks = k.stats.walks;
  kernel_offer_local_search_hint(k, strategy::LocalSearchKind::DirectCsp, {0, 1, 0});
  assert(walk_apply_steps(k, 1000) == ApplyResult::NoOp); // exact old identity is stale
  assert(k.stats.walks == walks && k.stats.local_search_hints_stale == 1);
  assert(k.h->saved == saved && k.h->best == best);
  k.walk_seed_candidate = k.local_search_hints[0].id;
  volatile sig_atomic_t stop = 1;
  k.stop = &stop;
  const auto target = k.h->target;
  assert(walk_apply_steps(k, 1000) == ApplyResult::NoOp);
  assert(k.h->saved == saved && k.h->target == target && k.h->best == best);
  assert(k.stats.walk_seed_interrupted == 1);
}

static void test_partial_seed_and_empty_objective() {
  Kernel k;
  ScopedHeuristics h(k);
  build_conflict_cover(k, 3);
  k.h->saved = {-1, -1, -1};
  k.h->best = {1, 1, -1};
  k.h->target = {-1, 1, 1};
  kernel_offer_local_search_partial_hint(k, strategy::LocalSearchKind::DirectCsp,
                                        {1, 0, 0});
  k.walk_seed_candidate = k.local_search_hints[0].id;
  const auto best = k.h->best;
  assert(walk_apply_steps(k, 24) == ApplyResult::Ok);
  assert((k.h->saved == std::vector<int8_t>{1, -1, -1}));
  assert(k.h->best == best && k.stats.walk_flips == 0);

  Kernel empty;
  ScopedHeuristics empty_h(empty);
  kernel_reserve_vars(empty, 3);
  empty.h->saved = {1, -1, 1};
  empty.h->best = {-1, -1, 1};
  empty.h->target = {-1, 1, -1};
  kernel_offer_local_search_partial_hint(empty, strategy::LocalSearchKind::Affine,
                                        {-1, 0, 0});
  empty.walk_seed_candidate = empty.local_search_hints[1].id;
  const auto saved = empty.h->saved, target = empty.h->target;
  const auto empty_best = empty.h->best;
  assert(walk_apply_steps(empty, 100) == ApplyResult::NoOp);
  assert(empty.h->saved == saved && empty.h->target == target &&
         empty.h->best == empty_best);
  assert(empty.stats.rephased == 0 && empty.stats.walk_seed_imports == 0);
  kernel_check_invariants(empty);
}

int main() {
  test_configuration_checking_profiles_and_structure();
  test_partial_seed_and_empty_objective();
  test_private_seed_budget_and_export();
  test_configurable_score_curve();
  test_break_base_domain_terminates();
  test_near_unit_interpolated_base();
  test_finds_model();
  test_kernel_untouched();
  test_rejects_non_root();
  test_deterministic();
  test_budget_and_noop();
  test_resume_uses_compact_literal_order();
  test_resume_large_watches_uses_logical_clause_position();
  test_dense_entry_deduplicates_binary_pairs();
  test_root_satisfied_binaries_do_not_reach_the_walk();
  test_registry();
  test_no_heuristics_guard();
  printf("test_walk: all passed\n");
  return 0;
}
