#include "rules/semantic_extension/gf3_gaussian_planner.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <set>
#include <tuple>
#include <vector>

using namespace tactix::gf3_gaussian;

namespace {

uint8_t plus3(uint8_t a, uint8_t b) {
  return static_cast<uint8_t>((a + b) % 3);
}

uint8_t times3(uint8_t a, uint8_t b) {
  return static_cast<uint8_t>((a * b) % 3);
}

Limits generous_limits() {
  return {1000000, 1000000, 32, 128, 1024, 10000000};
}

std::vector<Term> independent_scale(const std::vector<Term> &row,
                                    uint8_t factor) {
  std::vector<Term> result = row;
  for (Term &term : result)
    term.coefficient = times3(term.coefficient, factor);
  return result;
}

std::vector<Term> independent_add(const std::vector<Term> &a,
                                  const std::vector<Term> &b) {
  std::map<uint32_t, uint8_t> coefficients;
  for (const Term &term : a)
    coefficients[term.variable] =
        plus3(coefficients[term.variable], term.coefficient);
  for (const Term &term : b)
    coefficients[term.variable] =
        plus3(coefficients[term.variable], term.coefficient);
  std::vector<Term> result;
  for (const auto &[variable, coefficient] : coefficients)
    if (coefficient) result.push_back({variable, coefficient});
  return result;
}

using Provenance = std::map<uint64_t, uint8_t>;

Provenance independent_scale(const Provenance &input, uint8_t factor) {
  Provenance result;
  for (const auto &[source, coefficient] : input) {
    const uint8_t product = times3(coefficient, factor);
    if (product) result.emplace(source, product);
  }
  return result;
}

Provenance independent_add(const Provenance &a, const Provenance &b) {
  Provenance result = a;
  for (const auto &[source, coefficient] : b) {
    const uint8_t sum = plus3(result[source], coefficient);
    if (sum)
      result[source] = sum;
    else
      result.erase(source);
  }
  return result;
}

Provenance as_map(const std::vector<ProvenanceTerm> &terms) {
  Provenance result;
  uint64_t previous = 0;
  for (const ProvenanceTerm &term : terms) {
    assert(term.source_id > previous);
    assert(term.coefficient == 1 || term.coefficient == 2);
    const bool inserted = result.emplace(term.source_id, term.coefficient).second;
    assert(inserted);
    (void)inserted;
    previous = term.source_id;
  }
  return result;
}

uint8_t evaluate(const std::vector<Term> &row,
                 const std::vector<uint8_t> &assignment) {
  uint8_t result = 0;
  for (const Term &term : row) {
    assert(term.variable < assignment.size());
    result = plus3(result,
                   times3(term.coefficient, assignment[term.variable]));
  }
  return result;
}

uint64_t assignments(uint32_t variables) {
  uint64_t count = 1;
  while (variables--) count *= 3;
  return count;
}

std::vector<uint8_t> decode(uint64_t value, uint32_t variables) {
  std::vector<uint8_t> result(variables + 1, 0);
  for (uint32_t variable = 1; variable <= variables; ++variable) {
    result[variable] = static_cast<uint8_t>(value % 3);
    value /= 3;
  }
  return result;
}

void assert_empty_decline(const Result &result, PlanStatus status,
                          LimitKind limit = LimitKind::None) {
  assert(result.status == status);
  assert(result.limit == limit);
  assert(result.plan.derivations.empty());
  assert(result.plan.outcomes.empty());
}

// Replay uses only the public source equations and operation records.  It does
// not trust stored rows or stored provenance, and additionally reconstructs
// each node directly from its claimed original-source linear combination.
void replay_every_derivation(const std::vector<SourceRow> &source,
                             const Result &result) {
  assert(result.status == PlanStatus::Complete);
  assert(result.consumed.work == result.plan.statistics.work);
  assert(result.consumed.fill_entries ==
         result.plan.statistics.fill_entries);
  assert(result.consumed.retained_plan_bytes ==
         result.plan.statistics.retained_plan_bytes);
  uint64_t independently_counted_bytes = 16 * result.plan.outcomes.size();
  for (const Derivation &node : result.plan.derivations)
    independently_counted_bytes +=
        48 + 8 * node.row.size() + 16 * node.provenance.size();
  assert(result.consumed.retained_plan_bytes ==
         independently_counted_bytes);
  std::map<uint64_t, SourceRow> originals;
  for (const SourceRow &row : source) {
    const bool inserted = originals.emplace(row.source_id, row).second;
    assert(inserted);
    (void)inserted;
  }

  struct Replayed {
    std::vector<Term> row;
    uint8_t rhs = 0;
    Provenance provenance;
  };
  std::vector<Replayed> replayed(1);
  for (size_t index = 0; index < result.plan.derivations.size(); ++index) {
    const Derivation &node = result.plan.derivations[index];
    Replayed current;
    if (node.kind == DerivationKind::Source) {
      assert(!node.first_parent && !node.second_parent);
      assert(node.factor == 1);
      const SourceRow &original = originals.at(node.source_id);
      current = {original.terms, original.rhs, {{node.source_id, 1}}};
    } else if (node.kind == DerivationKind::Scale) {
      assert(!node.source_id);
      assert(node.first_parent && node.first_parent <= index);
      assert(!node.second_parent);
      assert(node.factor == 2);
      const Replayed &parent = replayed[node.first_parent];
      current.row = independent_scale(parent.row, node.factor);
      current.rhs = times3(parent.rhs, node.factor);
      current.provenance =
          independent_scale(parent.provenance, node.factor);
    } else {
      assert(node.kind == DerivationKind::Add);
      assert(!node.source_id);
      assert(node.first_parent && node.first_parent <= index);
      assert(node.second_parent && node.second_parent <= index);
      assert(node.factor == 1);
      const Replayed &first = replayed[node.first_parent];
      const Replayed &second = replayed[node.second_parent];
      current.row = independent_add(first.row, second.row);
      current.rhs = plus3(first.rhs, second.rhs);
      current.provenance =
          independent_add(first.provenance, second.provenance);
    }
    assert(current.row == node.row);
    assert(current.rhs == node.rhs);
    assert(current.provenance == as_map(node.provenance));

    std::vector<Term> from_sources;
    uint8_t source_rhs = 0;
    for (const auto &[source_id, factor] : current.provenance) {
      const SourceRow &original = originals.at(source_id);
      from_sources = independent_add(
          from_sources, independent_scale(original.terms, factor));
      source_rhs = plus3(source_rhs, times3(original.rhs, factor));
    }
    assert(from_sources == current.row);
    assert(source_rhs == current.rhs);
    replayed.push_back(std::move(current));
  }

  std::set<uint32_t> pivots;
  bool conflict = false;
  for (const RowOutcome &outcome : result.plan.outcomes) {
    assert(outcome.derivation &&
           outcome.derivation <= result.plan.derivations.size());
    const Replayed &row = replayed[outcome.derivation];
    if (outcome.kind == RowOutcomeKind::ZeroRowConflict) {
      assert(row.row.empty());
      assert(row.rhs == 1 || row.rhs == 2);
      conflict = true;
    } else if (outcome.kind == RowOutcomeKind::Unit) {
      assert(row.row.size() == 1);
      assert(row.row.front().coefficient == 1);
      const bool inserted = pivots.insert(row.row.front().variable).second;
      assert(inserted);
      (void)inserted;
    } else {
      assert(outcome.kind == RowOutcomeKind::Derived);
      assert(row.row.size() >= 2);
      assert(row.row.front().coefficient == 1);
      const bool inserted = pivots.insert(row.row.front().variable).second;
      assert(inserted);
      (void)inserted;
    }
  }
  if (conflict) assert(result.plan.outcomes.size() == 1);

  if (!conflict) {
    for (const RowOutcome &first : result.plan.outcomes) {
      const auto &pivot_row = replayed[first.derivation].row;
      const uint32_t pivot = pivot_row.front().variable;
      for (const RowOutcome &second : result.plan.outcomes) {
        if (&first == &second) continue;
        const auto &other = replayed[second.derivation].row;
        assert(std::none_of(other.begin(), other.end(), [&](const Term &term) {
          return term.variable == pivot;
        }));
      }
    }
  }
}

void check_semantics(const std::vector<SourceRow> &source,
                     const Result &result, uint32_t variables) {
  replay_every_derivation(source, result);
  bool has_source_model = false;
  bool has_conflict = false;
  for (const RowOutcome &outcome : result.plan.outcomes)
    has_conflict |= outcome.kind == RowOutcomeKind::ZeroRowConflict;
  for (uint64_t encoded = 0; encoded < assignments(variables); ++encoded) {
    const std::vector<uint8_t> assignment = decode(encoded, variables);
    bool source_model = true;
    for (const SourceRow &row : source)
      source_model &= evaluate(row.terms, assignment) == row.rhs;
    has_source_model |= source_model;
    bool outcome_model = true;
    for (const RowOutcome &outcome : result.plan.outcomes) {
      const Derivation &row =
          result.plan.derivations[outcome.derivation - 1];
      outcome_model &= evaluate(row.row, assignment) == row.rhs;
    }
    // The final reduced basis must preserve the complete solution set, not
    // merely consist of individually entailed equations.
    assert(outcome_model == source_model);
  }
  assert(has_conflict != has_source_model);
}

std::vector<SourceRow> all_nonzero_rows(uint32_t variables) {
  std::vector<SourceRow> result;
  const uint64_t count = assignments(variables);
  for (uint64_t encoded = 1; encoded < count; ++encoded) {
    std::vector<uint8_t> coefficients = decode(encoded, variables);
    std::vector<Term> terms;
    for (uint32_t variable = 1; variable <= variables; ++variable)
      if (coefficients[variable])
        terms.push_back({variable, coefficients[variable]});
    for (uint8_t rhs = 0; rhs < 3; ++rhs)
      result.push_back({0, terms, rhs});
  }
  return result;
}

void test_units_reduced_basis_and_determinism() {
  const std::vector<SourceRow> source = {
      {20, {{1, 1}, {2, 2}}, 1},
      {10, {{1, 1}, {2, 1}}, 0},
  };
  Result first = build_plan(source, generous_limits(),
                            [](uint64_t) { return true; });
  assert(first.status == PlanStatus::Complete);
  assert(first.plan.outcomes.size() == 2);
  assert(first.plan.outcomes[0].kind == RowOutcomeKind::Unit);
  assert(first.plan.outcomes[1].kind == RowOutcomeKind::Unit);
  check_semantics(source, first, 2);

  std::vector<SourceRow> reversed = source;
  std::reverse(reversed.begin(), reversed.end());
  Result second = build_plan(reversed, generous_limits(),
                             [](uint64_t) { return true; });
  assert(second.status == PlanStatus::Complete);
  assert(second.plan.derivations.size() == first.plan.derivations.size());
  assert(second.plan.outcomes.size() == first.plan.outcomes.size());
  for (size_t i = 0; i < first.plan.derivations.size(); ++i) {
    const Derivation &a = first.plan.derivations[i];
    const Derivation &b = second.plan.derivations[i];
    assert(std::tie(a.kind, a.source_id, a.first_parent, a.second_parent,
                    a.factor, a.row, a.rhs, a.provenance) ==
           std::tie(b.kind, b.source_id, b.first_parent, b.second_parent,
                    b.factor, b.row, b.rhs, b.provenance));
  }
  for (size_t i = 0; i < first.plan.outcomes.size(); ++i) {
    assert(first.plan.outcomes[i].kind == second.plan.outcomes[i].kind);
    assert(first.plan.outcomes[i].derivation ==
           second.plan.outcomes[i].derivation);
  }
}

void test_zero_rows() {
  Result tautology = build_plan({{1, {}, 0}}, generous_limits(),
                                [](uint64_t) { return true; });
  assert_empty_decline(tautology, PlanStatus::NoStructure);

  const std::vector<SourceRow> contradiction = {{9, {}, 2}};
  Result conflict = build_plan(contradiction, generous_limits(),
                               [](uint64_t) { return true; });
  assert(conflict.status == PlanStatus::Complete);
  assert(conflict.plan.outcomes.size() == 1);
  assert(conflict.plan.outcomes.front().kind ==
         RowOutcomeKind::ZeroRowConflict);
  check_semantics(contradiction, conflict, 0);
}

void test_exhaustive_small_systems() {
  const std::vector<SourceRow> rows = all_nonzero_rows(2);
  uint64_t systems = 0;
  for (size_t first = 0; first < rows.size(); ++first)
    for (size_t second = 0; second < rows.size(); ++second)
      for (size_t third = 0; third < rows.size(); ++third) {
        std::vector<SourceRow> source = {rows[first], rows[second],
                                         rows[third]};
        source[0].source_id = 101;
        source[1].source_id = 102;
        source[2].source_id = 103;
        Result result = build_plan(source, generous_limits(),
                                   [](uint64_t) { return true; });
        assert(result.status == PlanStatus::Complete);
        check_semantics(source, result, 2);
        ++systems;
      }
  assert(systems == 13824);

  const std::vector<SourceRow> ternary = all_nonzero_rows(3);
  for (size_t i = 0; i < ternary.size(); ++i) {
    SourceRow source = ternary[i];
    source.source_id = 1000 + i;
    Result result = build_plan({source}, generous_limits(),
                               [](uint64_t) { return true; });
    assert(result.status == PlanStatus::Complete);
    check_semantics({source}, result, 3);
  }
}

void test_invalid_inputs() {
  const std::vector<std::vector<SourceRow>> invalid = {
      {{0, {{1, 1}}, 0}},
      {{1, {{2, 1}, {1, 1}}, 0}},
      {{1, {{1, 1}, {1, 2}}, 0}},
      {{1, {{0, 1}}, 0}},
      {{1, {{1, 0}}, 0}},
      {{1, {{1, 3}}, 0}},
      {{1, {{1, 1}}, 3}},
      {{7, {{1, 1}}, 0}, {7, {{2, 1}}, 0}},
  };
  for (const auto &source : invalid)
    assert_empty_decline(
        build_plan(source, generous_limits(), [](uint64_t) { return true; }),
        PlanStatus::InvalidInput);
  assert_empty_decline(build_plan({{1, {{1, 1}}, 0}}, generous_limits(), {}),
                       PlanStatus::InvalidInput);
}

void test_every_resource_limit_and_interruption() {
  const std::vector<SourceRow> source = {
      {10, {{1, 1}, {2, 1}}, 0},
      {20, {{1, 1}, {2, 2}}, 1},
  };
  const Result baseline = build_plan(source, generous_limits(),
                                     [](uint64_t) { return true; });
  assert(baseline.status == PlanStatus::Complete);

  Limits limits = generous_limits();
  limits.maximum_work = 0;
  assert_empty_decline(
      build_plan(source, limits, [](uint64_t) { return true; }),
      PlanStatus::LimitExceeded, LimitKind::Work);

  limits = generous_limits();
  limits.maximum_fill_entries = 0;
  assert_empty_decline(
      build_plan(source, limits, [](uint64_t) { return true; }),
      PlanStatus::LimitExceeded, LimitKind::Fill);

  limits = generous_limits();
  limits.maximum_intermediate_width = 1;
  assert_empty_decline(
      build_plan(source, limits, [](uint64_t) { return true; }),
      PlanStatus::LimitExceeded, LimitKind::IntermediateWidth);

  limits = generous_limits();
  limits.maximum_provenance_terms = 1;
  assert_empty_decline(
      build_plan(source, limits, [](uint64_t) { return true; }),
      PlanStatus::LimitExceeded, LimitKind::Provenance);

  limits = generous_limits();
  limits.maximum_derivations = 2;
  assert_empty_decline(
      build_plan(source, limits, [](uint64_t) { return true; }),
      PlanStatus::LimitExceeded, LimitKind::Derivations);

  assert(baseline.consumed.retained_plan_bytes > 0);
  limits = generous_limits();
  limits.maximum_retained_plan_bytes =
      baseline.consumed.retained_plan_bytes;
  Result exact_retained_limit =
      build_plan(source, limits, [](uint64_t) { return true; });
  assert(exact_retained_limit.status == PlanStatus::Complete);
  assert(exact_retained_limit.consumed.retained_plan_bytes ==
         baseline.consumed.retained_plan_bytes);

  limits.maximum_retained_plan_bytes =
      baseline.consumed.retained_plan_bytes - 1;
  assert_empty_decline(
      build_plan(source, limits, [](uint64_t) { return true; }),
      PlanStatus::LimitExceeded, LimitKind::RetainedPlan);

  uint32_t calls = 0;
  Result interrupted = build_plan(source, generous_limits(),
                                  [&](uint64_t) { return calls++ == 0; });
  assert_empty_decline(interrupted, PlanStatus::Interrupted);
  assert(interrupted.consumed.work > 0);
}

} // namespace

int main() {
  test_units_reduced_basis_and_determinism();
  test_zero_rows();
  test_exhaustive_small_systems();
  test_invalid_inputs();
  test_every_resource_limit_and_interruption();
  std::cout << "gf3 gaussian planner tests passed\n";
  return 0;
}
