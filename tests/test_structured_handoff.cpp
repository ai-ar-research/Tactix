#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/substitution_redundancy/substitution_redundancy.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <numeric>

using namespace tactix;

// Three interchangeable rows, with no unique literal column: every clause
// occurs twice. The snapshot retains those physical copies.
static Kernel fixture() {
  Kernel k;
  kernel_reserve_vars(k, 9);
  for (unsigned copy = 0; copy < 2; ++copy) {
    Lit first[] = {internalize(1), internalize(2), internalize(3)};
    Lit second[] = {internalize(4), internalize(5), internalize(6)};
    kernel_add_large_clause(k, first, 3, false, 0, ++k.next_clause_id);
    kernel_add_large_clause(k, second, 3, false, 0, ++k.next_clause_id);
    for (int row = 0; row < 3; ++row) {
      kernel_add_binary_clause(k, internalize(-1-row), internalize(-4-row), ++k.next_clause_id);
      kernel_add_binary_clause(k, internalize(-1-row), internalize(7+row), ++k.next_clause_id);
    }
  }
  return k;
}

static SubstitutionRedundancyParameters profile() {
  SubstitutionRedundancyParameters p;
  p.producer = SubstitutionRedundancyProducer::Orbitopal;
  p.orbitopal_completion = OrbitopalCompletion::IndividualizedGraphCompletion;
  p.orbitopal_row_orbit_source = OrbitopalRowOrbitSource::RefinementCells;
  p.orbitopal_discovery_order = OrbitopalDiscoveryOrder::RefinementOnly;
  p.orbitopal_base_probe_filter = false;
  p.orbitopal_generator_fallback = false;
  p.graph_symmetry.clause_scope = SymmetryClauseScope::All;
  p.graph_symmetry.chain_mode = SymmetryChainMode::FixAndCut;
  p.graph_symmetry.maximum_generators = 64;
  p.graph_symmetry.maximum_group_ticks = 1000000;
  p.graph_symmetry.backend_search_budget = 64;
  p.graph_symmetry.preserve_detector_order = true;
  return p;
}

static void repeated_inexact_row_actions(int sign) {
  Kernel k;
  kernel_reserve_vars(k, 7);
  for (int row = 0; row < 3; ++row) {
    const int a = row + 1, b = row + 4, next = (row + 1) % 3 + 1;
    for (const auto pair : {std::pair{-a, b}, std::pair{-b, next},
                            std::pair{a, 7}, std::pair{a, -7}})
      kernel_add_binary_clause(k, internalize(sign * pair.first), internalize(sign * pair.second),
                               ++k.next_clause_id);
  }
  auto p = profile();
  SymmetryDiscoveryHints hints;
  const auto result = substitution_redundancy_discover_orbitopal_generators(k, p, hints);
  std::printf("inexact row control: status %u failures %llu reuse %llu matrices %llu\n",
      static_cast<unsigned>(result.status),
      static_cast<unsigned long long>(result.refinement_exact_failures),
      static_cast<unsigned long long>(result.reused_inexact_matrices),
      static_cast<unsigned long long>(result.matrices));
  assert(result.status == OrbitopalEnumerationStatus::Complete);
  assert(result.refinement_exact_failures > 0);
  assert(result.reused_inexact_matrices > 0);
  assert(hints.generators.empty() && hints.fixed_variables.empty());
  kernel_check_invariants(k);

  // A search failing within a selected generator subgroup is not an exact
  // impossibility certificate and must never populate this cache.
  auto fallback = p;
  fallback.orbitopal_generator_fallback = true;
  SymmetryDiscoveryHints searched;
  const auto search = substitution_redundancy_discover_orbitopal_generators(k, fallback, searched);
  assert(search.reused_inexact_matrices == 0);

  // Strengthening changes the symmetry group: complete the directed cycle's
  // other cross edges. The same kernel now admits the full row action.
  for (int row = 0; row < 3; ++row)
    kernel_add_binary_clause(k, internalize(-sign * (row + 4)),
        internalize(sign * ((row + 2) % 3 + 1)), ++k.next_clause_id);
  const auto clause_id = k.next_clause_id;
  const auto changed = substitution_redundancy_discover_orbitopal_generators(k, p, hints);
  assert(changed.status == OrbitopalEnumerationStatus::Complete);
  assert(!hints.generators.empty());
  assert(k.next_clause_id == clause_id);
  kernel_check_invariants(k);
}

static void product_anchor_scope(int sign) {
  Kernel k;
  kernel_reserve_vars(k, 9);
  for (unsigned row = 0; row < 3; ++row) {
    Lit clause[3];
    for (unsigned column = 0; column < 3; ++column)
      clause[column] = internalize(sign * static_cast<int>(3 * row + column + 1));
    kernel_add_large_clause(k, clause, 3, false, 0, ++k.next_clause_id);
  }
  for (unsigned column = 0; column < 3; ++column)
    for (unsigned first = 0; first < 3; ++first)
      for (unsigned second = first + 1; second < 3; ++second)
        kernel_add_binary_clause(k,
            internalize(-sign * static_cast<int>(3 * first + column + 1)),
            internalize(-sign * static_cast<int>(3 * second + column + 1)), ++k.next_clause_id);
  auto p = profile();
  p.orbitopal_orientation = OrbitopalOrientation::Both;
  p.maximum_orbitopal_matrices = 1;
  SymmetryDiscoveryHints whole, anchors;
  const auto full = substitution_redundancy_discover_orbitopal_generators(k, p, whole);
  p.graph_symmetry.structured_scope = SymmetryStructuredScope::AxisAnchors;
  const auto scoped = substitution_redundancy_discover_orbitopal_generators(k, p, anchors);
  assert(scoped.status == full.status && scoped.matrices == 1);
  assert(anchors.generators == whole.generators && anchors.generators.size() == 4);
  assert(whole.fixed_variables.size() == 9 && anchors.fixed_variables.size() == 5);
  assert(std::includes(whole.fixed_variables.begin(), whole.fixed_variables.end(),
                       anchors.fixed_variables.begin(), anchors.fixed_variables.end()));
  // A one-axis structured action can overlap with an independent action on
  // the other axis. Whole-matrix scope suppresses both; row anchors must leave
  // a nontrivial remaining subgroup, even when its stored basis is disabled.
  auto remaining = p;
  remaining.producer = SubstitutionRedundancyProducer::GraphSymmetry;
  remaining.orbitopal_orientation = OrbitopalOrientation::Rows;
  remaining.orbitopal_completion = OrbitopalCompletion::MatrixOnly;
  remaining.orbitopal_discovery_order = OrbitopalDiscoveryOrder::UniqueLiteralOnly;
  remaining.graph_symmetry.use_structured_generators = false;
  remaining.graph_symmetry.stabilize_structured_variables = true;
  remaining.graph_symmetry.fixing_mode = SymmetryFixingMode::StabilizerOnly;
  remaining.graph_symmetry.structured_scope = SymmetryStructuredScope::WholeMatrix;
  std::vector<SubstitutionRedundancyInstance> offered;
  const auto visit = [&](const auto &instance) { offered.push_back(instance); return true; };
  const auto suppressed = substitution_redundancy_enumerate_symmetry(k, remaining, visit);
  assert(suppressed.generators_validated == 0 && offered.empty());
  remaining.graph_symmetry.structured_scope = SymmetryStructuredScope::AxisAnchors;
  const auto exposed = substitution_redundancy_enumerate_symmetry(k, remaining, visit);
  assert(exposed.generators_validated > 0 && !offered.empty());
  assert(!exposed.complete_group && !suppressed.complete_group);
  const auto applied = substitution_redundancy_apply_detailed(k, offered.front(), remaining);
  assert(applied.status == SubstitutionRedundancyApplicationStatus::Applied);
  propagate_apply(k);

  volatile sig_atomic_t stop = 1;
  const auto interrupted = substitution_redundancy_discover_orbitopal_generators(k, p, anchors, &stop);
  assert(interrupted.status == OrbitopalEnumerationStatus::Interrupted);
  assert(anchors.generators.empty() && anchors.fixed_variables.empty());
  kernel_check_invariants(k);
}

// Unequal physical counts do not invalidate a complete semantic row action.
// Exercise both public advice and internal producer-to-consumer composition.
static void unequal_duplicate_handoff() {
  Kernel k = fixture();
  // Add a complete orbit, but with one source clause repeated again.
  for (int row = 0; row < 3; ++row) {
    Lit clause[] = {internalize(-1-row), internalize(7), internalize(8), internalize(9)};
    kernel_add_large_clause(k, clause, 4, false, 0, ++k.next_clause_id);
    if (!row)
      kernel_add_large_clause(k, clause, 4, false, 0, ++k.next_clause_id);
  }
  // The complete third column is invariant under every row permutation.
  auto p = profile();
  SymmetryDiscoveryHints hints;
  const auto discovery = substitution_redundancy_discover_orbitopal_generators(k, p, hints);
  assert(discovery.status == OrbitopalEnumerationStatus::Complete);
  assert(!hints.generators.empty());
  p.producer = SubstitutionRedundancyProducer::GraphSymmetry;
  std::vector<SubstitutionRedundancyInstance> offered;
  const auto visitor = [&](const auto &instance) { offered.push_back(instance); return true; };
  const auto supplied = substitution_redundancy_enumerate_symmetry(k, p, visitor, nullptr, &hints);
  assert(supplied.status == SymmetryEnumerationStatus::Complete);
  assert(!supplied.complete_group && !offered.empty());
  const auto expected = offered;
  p.graph_symmetry.use_structured_generators = true;
  p.graph_symmetry.stabilize_structured_variables = true;
  offered.clear();
  const auto automatic = substitution_redundancy_enumerate_symmetry(k, p, visitor);
  assert(automatic.status == SymmetryEnumerationStatus::Complete);
  assert(offered == expected);
  // Explicit applications still go through full X7 validation.
  for (const auto &instance : offered) {
    const auto applied = substitution_redundancy_apply_detailed(k, instance, p);
    assert(applied.status == SubstitutionRedundancyApplicationStatus::Applied ||
           applied.status == SubstitutionRedundancyApplicationStatus::NoChange);
    propagate_apply(k);
    kernel_check_invariants(k);
  }
}

static void impossible_graph_budget_precedes_formula_snapshot() {
  Kernel k = fixture();
  auto p = profile();
  p.producer = SubstitutionRedundancyProducer::GraphSymmetry;
  p.graph_symmetry.use_structured_generators = true;
  p.graph_symmetry.maximum_graph_vertices = 2 * k.nvars - 1;
  const auto id = k.next_clause_id;
  const auto result = substitution_redundancy_enumerate_symmetry(k, p,
      [](const auto &) { assert(false); return false; });
  assert(result.status == SymmetryEnumerationStatus::LimitReached && !result.ticks);
  assert(k.next_clause_id == id);
  // Refinement uses three vertices per variable, independently of the
  // generic producer's representation. A failed admission must leave the
  // kernel unchanged and recover when the caller increases the limit.
  p.producer = SubstitutionRedundancyProducer::Orbitopal;
  p.graph_symmetry.maximum_graph_vertices = 3 * k.nvars - 1;
  SymmetryDiscoveryHints rejected_hints;
  const auto refinement = substitution_redundancy_discover_orbitopal_generators(k, p, rejected_hints);
  assert(refinement.status == OrbitopalEnumerationStatus::LimitReached);
  assert(rejected_hints.generators.empty() && k.next_clause_id == id);
  // Matrix-only unique-literal AMO completion is independent of graph limits.
  Kernel unique;
  kernel_reserve_vars(unique, 3);
  const Lit clause[] = {0, 2, 4};
  kernel_add_large_clause(unique, clause, 3, false, 0, ++unique.next_clause_id);
  auto matrix = p;
  matrix.orbitopal_completion = OrbitopalCompletion::MatrixOnly;
  matrix.orbitopal_discovery_order = OrbitopalDiscoveryOrder::UniqueLiteralOnly;
  matrix.complete_unique_literal_amos = true;
  matrix.graph_symmetry.maximum_graph_vertices = 0;
  unsigned offered = 0;
  const auto amos = substitution_redundancy_enumerate_orbitopal(unique, matrix,
      [&](const auto &) { ++offered; return true; });
  assert(amos.status == OrbitopalEnumerationStatus::Complete && offered == 3);
  // The independent structured producer remains usable with its own budget.
  p.graph_symmetry.maximum_graph_vertices = 10000;
  SymmetryDiscoveryHints hints;
  const auto structured = substitution_redundancy_discover_orbitopal_generators(k, p, hints);
  assert(structured.status == OrbitopalEnumerationStatus::Complete && !hints.generators.empty());
}

int main() {
  impossible_graph_budget_precedes_formula_snapshot();
  unequal_duplicate_handoff();
  product_anchor_scope(1);
  product_anchor_scope(-1);
  repeated_inexact_row_actions(1);
  repeated_inexact_row_actions(-1);
  Kernel k = fixture();
  auto p = profile();
  const auto clause_id = k.next_clause_id;
  const auto empty = substitution_redundancy_enumerate_orbitopal(k, p,
      [](const auto &) { assert(false); return false; });
  assert(empty.instances == 0);
  SymmetryDiscoveryHints hints;
  const auto discovered = substitution_redundancy_discover_orbitopal_generators(k, p, hints);
  assert(discovered.status == OrbitopalEnumerationStatus::Complete);
  assert(discovered.matrices > 0 && !hints.generators.empty());
  assert(hints.fixed_variables.size() == 9);
  assert(k.next_clause_id == clause_id && !k.stats.sr_applications);
  kernel_check_invariants(k);

  // One complete row matrix keeps the same exact basis, but may expose a
  // smaller pointwise scope. No partial scope survives admission failure.
  auto anchored = p;
  anchored.maximum_orbitopal_matrices = 1;
  SymmetryDiscoveryHints whole, axes;
  const auto whole_result = substitution_redundancy_discover_orbitopal_generators(k, anchored, whole);
  anchored.graph_symmetry.structured_scope = SymmetryStructuredScope::AxisAnchors;
  const auto anchor_result = substitution_redundancy_discover_orbitopal_generators(k, anchored, axes);
  assert(axes.generators == whole.generators && !axes.generators.empty());
  assert(whole_result.status == anchor_result.status);
  assert(axes.fixed_variables.size() == 3 && whole.fixed_variables.size() == 9);
  assert(std::includes(whole.fixed_variables.begin(), whole.fixed_variables.end(),
                       axes.fixed_variables.begin(), axes.fixed_variables.end()));
  anchored.graph_symmetry.maximum_generators = 0;
  SymmetryDiscoveryHints absent;
  const auto zero = substitution_redundancy_discover_orbitopal_generators(k, anchored, absent);
  assert(zero.status == OrbitopalEnumerationStatus::LimitReached);
  assert(absent.generators.empty() && absent.fixed_variables.empty());
  anchored.graph_symmetry.structured_scope = static_cast<SymmetryStructuredScope>(2);
  assert(!anchored.graph_symmetry.valid());
  const auto invalid_scope = substitution_redundancy_discover_orbitopal_generators(k, anchored, absent);
  assert(invalid_scope.status == OrbitopalEnumerationStatus::Complete);
  assert(absent.generators.empty() && absent.fixed_variables.empty());

  // Collection does not depend on clause emission admission.
  p.maximum_orbitopal_instances = 0;
  SymmetryDiscoveryHints repeated;
  const auto again = substitution_redundancy_discover_orbitopal_generators(k, p, repeated);
  assert(again.status == discovered.status && repeated.generators == hints.generators);
  assert(repeated.fixed_variables == hints.fixed_variables);

  // Column ranking changes fixing priority, but not complete row generators.
  // Collection is independent even of an exhausted clique-ordering budget.
  for (const auto order : {OrbitopalColumnOrder::Canonical,
                          OrbitopalColumnOrder::CooccurrenceGreedyClique,
                          OrbitopalColumnOrder::CooccurrenceMaximumClique,
                          OrbitopalColumnOrder::CooccurrenceBoundedGreedyClique}) {
    for (const uint64_t ticks : {uint64_t{0}, uint64_t{1}, UINT64_MAX}) {
      auto reordered = p;
      reordered.orbitopal_column_order = order;
      reordered.minimum_orbitopal_clique_columns = 2;
      reordered.maximum_orbitopal_clique_ticks = ticks;
      SymmetryDiscoveryHints actual;
      const auto result = substitution_redundancy_discover_orbitopal_generators(k, reordered, actual);
      assert(result.status == discovered.status);
      assert(actual.generators == hints.generators);
      assert(actual.fixed_variables == hints.fixed_variables);
      assert(result.ticks == discovered.ticks);
    }
  }

  auto limited = p;
  limited.graph_symmetry.maximum_generators = hints.generators.size() - 1;
  SymmetryDiscoveryHints partial;
  const auto cap = substitution_redundancy_discover_orbitopal_generators(k, limited, partial);
  assert(cap.status == OrbitopalEnumerationStatus::LimitReached);
  assert(partial.generators.empty() && partial.fixed_variables.empty());
  limited = p;
  limited.maximum_detection_ticks = discovered.ticks - 1;
  const auto ticks = substitution_redundancy_discover_orbitopal_generators(k, limited, partial);
  assert(ticks.status == OrbitopalEnumerationStatus::LimitReached);
  assert(partial.generators.empty() && partial.fixed_variables.empty());
  volatile sig_atomic_t stop = 1;
  const auto stopped = substitution_redundancy_discover_orbitopal_generators(k, p, partial, &stop);
  assert(stopped.status == OrbitopalEnumerationStatus::Interrupted);
  assert(partial.generators.empty() && partial.fixed_variables.empty());

  p.producer = SubstitutionRedundancyProducer::GraphSymmetry;
  std::vector<SubstitutionRedundancyInstance> offered;
  const auto visit = [&](const auto &i) { offered.push_back(i); return true; };
  const auto supplied = substitution_redundancy_enumerate_symmetry(k, p, visit, nullptr, &hints);
  assert(supplied.generators_validated > 0 && !offered.empty());
  assert(!supplied.complete_group);
  const auto expected = offered;
  Kernel applied = fixture();
  unsigned applications = 0;
  for (const auto &instance : offered) {
    const auto result = substitution_redundancy_apply_detailed(applied, instance, p);
    assert(result.status == SubstitutionRedundancyApplicationStatus::Applied ||
           result.status == SubstitutionRedundancyApplicationStatus::NoChange);
    applications += result.status == SubstitutionRedundancyApplicationStatus::Applied;
    propagate_apply(applied);
    kernel_check_invariants(applied);
  }

  assert(applications > 0);

  // Same native basis/scope through configurable per-call discovery.
  p.graph_symmetry.use_structured_generators = true;
  p.graph_symmetry.stabilize_structured_variables = true;
  offered.clear();
  const auto automatic = substitution_redundancy_enumerate_symmetry(k, p, visit);
  assert(automatic.generators_validated == supplied.generators_validated);
  assert(offered == expected && !automatic.complete_group);

  // Scope without its structured basis loses these effective transformations:
  // all matrix variables are fixed and the remaining subgroup is trivial.
  p.graph_symmetry.use_structured_generators = false;
  offered.clear();
  const auto scope_only = substitution_redundancy_enumerate_symmetry(k, p, visit);
  assert(scope_only.generators_validated == 0 && offered.empty());
  assert(!scope_only.complete_group);
  p.graph_symmetry.stabilize_structured_variables = false;
  offered.clear();
  const auto plain = substitution_redundancy_enumerate_symmetry(k, p, visit);
  const auto plain_instances = offered;
  SymmetryDiscoveryHints no_hints;
  offered.clear();
  const auto no_handoff = substitution_redundancy_enumerate_symmetry(k, p, visit, nullptr, &no_hints);
  assert(offered == plain_instances && no_handoff.ticks == plain.ticks);
  assert(no_handoff.generators_validated == plain.generators_validated);

  // Invalid advice must fail before any candidate is published.
  p.graph_symmetry.use_structured_generators = false;
  p.graph_symmetry.stabilize_structured_variables = false;
  for (unsigned bad = 0; bad < 4; ++bad) {
    auto invalid = hints;
    if (bad == 0) invalid.generators.front().pop_back();
    if (bad == 1) invalid.generators.front()[0] = invalid.generators.front()[1];
    if (bad == 2) invalid.fixed_variables.push_back(k.nvars);
    if (bad == 3) { // Signed bijection, but not a symmetry of this formula.
      auto &image = invalid.generators.front();
      std::iota(image.begin(), image.end(), Lit{0});
      std::swap(image[0], image[6]); std::swap(image[1], image[7]);
    }
    offered.clear();
    const auto rejected = substitution_redundancy_enumerate_symmetry(k, p, visit, nullptr, &invalid);
    assert(rejected.status == SymmetryEnumerationStatus::Invalid && offered.empty());
  }
  // A once-valid map becomes invalid after asymmetric formula mutation.
  kernel_add_binary_clause(k, internalize(1), internalize(4), ++k.next_clause_id);
  offered.clear();
  const auto stale = substitution_redundancy_enumerate_symmetry(k, p, visit, nullptr, &hints);
  assert(stale.status == SymmetryEnumerationStatus::Invalid && offered.empty());
  std::puts("structured handoff: PASS");
}
