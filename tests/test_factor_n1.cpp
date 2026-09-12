// N1 regression: Factor admission lives wholly in the calling policy. A rule
// that guards true, advertises an instance and then NoOps on a strategy
// predicate removes a capability from every external policy.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/factor/factor.hpp"
#include "rules/registry.hpp"

#include <cassert>
#include <algorithm>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <set>

using namespace tactix;

// A 2x3 grid with delta = m*n - (m+n) = 1 > 0, so the factoring is worth
// applying, plus padding that holds the binary density far below the shipped
// admission threshold of 6 binaries per variable.
static void build_sparse_factorable(Kernel &k, uint32_t pad_vars) {
  kernel_reserve_vars(k, 5 + pad_vars);
  const Lit a = make_lit(0, true), b = make_lit(1, true);
  const Lit x = make_lit(2, true), y = make_lit(3, true), z = make_lit(4, true);
  for (const Lit f : {a, b})
    for (const Lit r : {x, y, z})
      kernel_add_binary_clause(k, f, r);
}

static void test_exact_subgrid_selection() {
  Kernel k;
  kernel_reserve_vars(k, 6);
  const Lit a = internalize(-1), b = internalize(-2), c = internalize(-3);
  const Lit p = internalize(-4), q = internalize(-5), r = internalize(-6);
  for (const Lit factor : {a, b, c})
    for (const Lit remainder : {p, q, r})
      kernel_add_binary_clause(k, factor, remainder);

  FactorGrid selected;
  selected.factors = {b, a}; // caller order need not be canonical
  selected.remainders = {{r}, {p}, {q}};
  assert(factor_apply_grid(k, selected) == ApplyResult::Ok);
  assert(k.stats.factorings == 1);
  for (const Lit factor : {a, b})
    for (const Lit remainder : {p, q, r})
      assert(!kernel_binary_live(k, factor, remainder));
  for (const Lit remainder : {p, q, r})
    assert(kernel_binary_live(k, c, remainder) &&
           "the unselected third row must remain live");
}

static void test_exact_large_grid_selection() {
  Kernel k;
  kernel_reserve_vars(k, 8);
  const Lit a = internalize(1), b = internalize(2);
  const std::vector<std::vector<Lit>> columns = {
      {internalize(3), internalize(4)},
      {internalize(5), internalize(6)},
      {internalize(7), internalize(8)}};
  FactorGrid selected;
  selected.factors = {b, a};
  selected.remainders = columns;
  for (const Lit factor : selected.factors)
    for (const std::vector<Lit> &remainder : columns) {
      const Lit clause[3] = {remainder[1], factor, remainder[0]};
      selected.members.push_back(
          kernel_add_large_clause(k, clause, 3, false, 0));
    }
  assert(factor_apply_grid(k, selected) == ApplyResult::Ok);
  assert(k.stats.factorings == 1 && k.nvars == 9);
  for (const Ref ref : selected.members)
    assert(clause_at(k.arena, ref)->garbage());
}

static std::set<Lit> literal_set(const std::vector<Lit> &literals) {
  return {literals.begin(), literals.end()};
}

static void test_complete_grid_enumeration_includes_subgrids() {
  Kernel k;
  kernel_reserve_vars(k, 6);
  const Lit a = internalize(1), b = internalize(2);
  const Lit p = internalize(3), q = internalize(4);
  const Lit r = internalize(5), s = internalize(6);
  for (const Lit factor : {a, b})
    for (const Lit remainder : {p, q, r, s})
      kernel_add_binary_clause(k, factor, remainder);

  const std::set<Lit> wanted_factors = {a, b};
  std::set<std::set<Lit>> selected_columns;
  uint64_t matching = 0;
  const FactorEnumerationResult result = factor_enumerate_grids(
      k,
      [&](const FactorGrid &grid) {
        if (literal_set(grid.factors) != wanted_factors)
          return true;
        assert(grid.members.size() ==
               grid.factors.size() * grid.remainders.size());
        std::set<Lit> columns;
        for (const std::vector<Lit> &remainder : grid.remainders) {
          assert(remainder.size() == 1);
          columns.insert(remainder.front());
        }
        selected_columns.insert(std::move(columns));
        ++matching;
        return true;
      });
  assert(result.status == FactorEnumerationStatus::Complete);
  assert(matching == 5 &&
         "a 2x4 grid has all four profitable 2x3 subgrids plus itself");
  assert(selected_columns.count({p, q, r}));
  assert(selected_columns.count({p, q, s}));
  assert(selected_columns.count({p, r, s}));
  assert(selected_columns.count({q, r, s}));
  assert(selected_columns.count({p, q, r, s}));
}

static void test_complete_grid_enumeration_controls() {
  Kernel square;
  kernel_reserve_vars(square, 4);
  const Lit a = internalize(1), b = internalize(2);
  const Lit p = internalize(3), q = internalize(4);
  for (const Lit factor : {a, b})
    for (const Lit remainder : {p, q})
      kernel_add_binary_clause(square, factor, remainder);
  const FactorEnumerationResult none =
      factor_enumerate_grids(square, [](const FactorGrid &) { return true; });
  assert(none.status == FactorEnumerationStatus::Complete && none.grids == 0 &&
         "zero-gain 2x2 rectangles are not legal E2 actions");

  Kernel larger;
  kernel_reserve_vars(larger, 6);
  for (const Lit factor : {a, b})
    for (const Lit remainder : {p, q, internalize(5), internalize(6)})
      kernel_add_binary_clause(larger, factor, remainder);
  const FactorEnumerationResult limited = factor_enumerate_grids(
      larger, [](const FactorGrid &) { return true; },
      FactorEnumerationOptions{2, nullptr});
  assert(limited.status == FactorEnumerationStatus::LimitReached &&
         limited.grids == 2);

  volatile sig_atomic_t stop = 1;
  const FactorEnumerationResult stopped = factor_enumerate_grids(
      larger, [](const FactorGrid &) { return true; },
      FactorEnumerationOptions{std::numeric_limits<uint64_t>::max(), &stop});
  assert(stopped.status == FactorEnumerationStatus::Stopped &&
         stopped.grids == 0);
}

static void test_complete_grid_enumeration_names_large_occurrences() {
  Kernel k;
  kernel_reserve_vars(k, 7);
  const std::vector<Lit> factors = {internalize(1), internalize(2),
                                    internalize(3)};
  const std::vector<std::vector<Lit>> remainders = {
      {internalize(4), internalize(5)},
      {internalize(6), internalize(7)}};
  std::set<Ref> expected;
  for (const Lit factor : factors)
    for (const std::vector<Lit> &remainder : remainders) {
      const Lit literals[] = {factor, remainder[0], remainder[1]};
      expected.insert(kernel_add_large_clause(k, literals, 3, false, 0));
    }

  bool found = false;
  const FactorEnumerationResult result = factor_enumerate_grids(
      k,
      [&](const FactorGrid &grid) {
        if (literal_set(grid.factors) != literal_set(factors) ||
            grid.remainders != remainders)
          return true;
        assert(std::set<Ref>(grid.members.begin(), grid.members.end()) ==
               expected);
        found = true;
        return true;
      });
  assert(result.status == FactorEnumerationStatus::Complete && found);
}

int main() {
  test_exact_subgrid_selection();
  test_exact_large_grid_selection();
  test_complete_grid_enumeration_includes_subgrids();
  test_complete_grid_enumeration_controls();
  test_complete_grid_enumeration_names_large_occurrences();
  Kernel k; // orders of magnitude under the gate: not a lucky pass
  build_sparse_factorable(k, 200);
  assert(k.binary_ids.size() < 6ull * kernel_num_output_vars(k) &&
         "test is vacuous unless the formula is genuinely below the threshold");

  // Drive the registry surface, the path an external policy takes; calling
  // `factor_apply` directly would not exercise the advertised instance.
  const std::vector<RuleVTable> &rules = tactix_rules();
  const RuleVTable *factor = nullptr;
  for (const RuleVTable &rv : rules)
    if (!std::strcmp(rv.name, "Factor"))
      factor = &rv;
  assert(factor && "Factor must be registered");
  assert(factor->guard(k) && "guard must hold at level 0 on a consistent formula");

  uint64_t inst[4];
  const uint32_t n = factor->instances ? factor->instances(k, inst, 4) : 0;
  assert(n >= 1 && "N1: instances() must offer at least the canonical instance");

  const uint64_t before = k.stats.factorings;
  const ApplyResult r = factor->apply_instance(k, inst[0]);

  // An advertised instance must apply: NoOp here is the N1 breach.
  assert(r == ApplyResult::Ok &&
         "N1 BREACH: a legal factoring is not applicable through the rule API "
         "on a binary-sparse formula");
  assert(k.stats.factorings > before && "apply reported Ok but factored nothing");

  // `N1-FACTOR-INSTANCES`: the sweep is not the only addressable application.
  // Every unassigned, unretired literal is a legal seed, and seeding at one
  // applies at most that one factoring.
  {
    Kernel s;
    build_sparse_factorable(s, 4);
    uint64_t seeds[512];
    const uint32_t m = factor->instances(s, seeds, 512);
    assert(m == 1 + 2 * s.nvars &&
           "N1: canonical, then every unassigned literal as a seed");
    assert(seeds[0] == INSTANCE_CANONICAL);

    // DefaultPolicy's per-literal touch filter is a schedule narrowing, so it
    // must not reach the API: clear every factor mark (the state in which the
    // canonical sweep declines to schedule ANY seed) and seed explicitly.
    std::fill(s.factor_marks.begin(), s.factor_marks.end(), 0);
    s.touch_filter = true;
    const uint64_t canonical_before = s.stats.factorings;
    assert(factor->apply_instance(s, INSTANCE_CANONICAL) == ApplyResult::NoOp &&
           "the filtered canonical schedule must be empty for this to be "
           "an adversarial test of the instance surface");
    assert(s.stats.factorings == canonical_before);

    const Lit seed = make_lit(0, true); // divider `a` of the 2x3 grid
    assert(factor->apply_instance(s, lit_instance(s, seed)) ==
               ApplyResult::Ok &&
           "N1 BREACH: a legal seed is unreachable once DefaultPolicy's touch "
           "filter declines to schedule it");
    assert(s.stats.factorings == canonical_before + 1 &&
           "a literal instance applies exactly one factoring");
  }

  std::printf("test_factor_n1: all passed (%llu factorings on a formula at "
              "%.3f binaries/var)\n",
              (unsigned long long)(k.stats.factorings - before),
              (double)k.binary_ids.size() / kernel_num_output_vars(k));
  return 0;
}
