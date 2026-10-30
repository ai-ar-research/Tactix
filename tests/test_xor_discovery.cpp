#undef NDEBUG
#include "rules/semantic_extension/affine_image.hpp"
#include "rules/semantic_extension/affine_root_image.hpp"

#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <map>
#include <set>
#include <vector>

using namespace tactix::affine_semantic_extension;
using tactix::AffineLiveSource;
using tactix::AffineSourceKind;

namespace {

Limits discovery_limits() {
  return {1000, 100, 1000, 1000000, 8, 16, 32, 4};
}

std::vector<Premise> premises(const std::vector<External> &variables,
                              bool rhs, uint64_t &occurrence) {
  std::vector<Premise> result;
  for (auto clause : clausal_form(variables, rhs))
    result.push_back({occurrence++, std::move(clause)});
  return result;
}

Image discover(const std::vector<Premise> &input) {
  Image image;
  assert(recognize(input, discovery_limits(),
                   [](uint64_t) { return true; }, image) == Status::Complete);
  return image;
}

void order_and_sign_normalization() {
  uint64_t occurrence = 1;
  auto input = premises({1, 2, 3, 4}, false, occurrence);
  for (auto &premise : input) {
    for (External &literal : premise.literals)
      if (literal == 2 || literal == -2) literal = -literal;
    std::reverse(premise.literals.begin(), premise.literals.end());
  }
  std::reverse(input.begin(), input.end());

  const Image image = discover(input);
  assert(image.equations.size() == 1);
  assert(image.equations[0].variables ==
         std::vector<External>({1, 2, 3, 4}));
  // Replacing x2 by not-x2 flips the canonical right-hand side.
  assert(image.equations[0].rhs);
}

void duplicate_occurrences_do_not_fake_rank() {
  uint64_t occurrence = 100;
  auto input = premises({3, 7, 11}, true, occurrence);
  const size_t physical = input.size();
  for (size_t i = 0; i < physical; ++i) {
    Premise duplicate = input[i];
    duplicate.occurrence = occurrence++;
    std::rotate(duplicate.literals.begin(), duplicate.literals.begin() + 1,
                duplicate.literals.end());
    input.push_back(std::move(duplicate));
  }

  const Image image = discover(input);
  assert(image.equations.size() == 1);
  assert(image.equations[0].variables ==
         std::vector<External>({3, 7, 11}));
  assert(image.equations[0].rhs);
  assert(image.equations[0].premises.size() == physical);
}

void overlapping_equations_are_separate_and_composable() {
  uint64_t occurrence = 200;
  auto input = premises({1, 2, 3}, false, occurrence);
  auto second = premises({1, 2, 4}, true, occurrence);
  input.insert(input.end(), second.begin(), second.end());

  const Image image = discover(input);
  assert(image.equations.size() == 2);
  assert(image.equations[0].variables ==
         std::vector<External>({1, 2, 3}));
  assert(!image.equations[0].rhs);
  assert(image.equations[1].variables ==
         std::vector<External>({1, 2, 4}));
  assert(image.equations[1].rhs);
  assert(image.consequences.size() == 1);
  assert(image.consequences[0].variables ==
         std::vector<External>({3, 4}));
  assert(image.consequences[0].rhs);
}

void partial_and_malformed_populations_are_not_xors() {
  uint64_t occurrence = 300;
  const auto even = premises({1, 2, 3}, false, occurrence);
  const auto odd = premises({1, 2, 3}, true, occurrence);
  std::vector<Premise> mixed{even[0], even[1], odd[0], odd[1]};
  mixed.push_back({occurrence++, {5, -5, 6}}); // repeated variable
  mixed.push_back({occurrence++, {1, 2, 3, 4, 5, 6, 7, 8, 9}}); // arity 9

  Image sentinel;
  sentinel.clauses_seen = 777;
  assert(recognize(mixed, discovery_limits(),
                   [](uint64_t) { return true; }, sentinel) ==
         Status::NoStructure);
  assert(sentinel.clauses_seen == 777);
}

void root_guard_shortening_preserves_the_equation() {
  constexpr External guard = 9;
  uint64_t occurrence = 400;
  std::vector<AffineLiveSource> sources;
  for (auto clause : clausal_form({1, 4, 7}, true)) {
    clause.push_back(-guard);
    AffineLiveSource source;
    source.occurrence = occurrence;
    source.clause_id = occurrence;
    source.clause = std::move(clause);
    sources.push_back(std::move(source));
    ++occurrence;
  }
  AffineLiveSource unit;
  unit.occurrence = occurrence;
  unit.kind = AffineSourceKind::RootUnit;
  unit.clause_id = 0;
  unit.proof_id_exact = false;
  unit.clause = {guard};
  unit.recognition_candidate = false;
  sources.push_back(std::move(unit));

  RootImageLimits limits;
  limits.image = discovery_limits();
  limits.maximum_sources = 100;
  limits.maximum_literals = 1000;
  limits.maximum_units = 16;
  limits.maximum_work = 1000000;
  limits.maximum_bytes = 1000000;
  limits.maximum_proof_additions = 100;
  RootImagePlan plan;
  RootScopeEffort effort;
  assert(prepare_affine_root_image(
             sources, {guard}, limits, [](uint64_t) { return true; }, plan,
             nullptr, nullptr, &effort) == RootImageStatus::Prepared);
  assert(effort.recognized_groups == 1);
  assert(plan.image.equations.size() == 1);
  assert(plan.image.equations[0].variables ==
         std::vector<External>({1, 4, 7}));
  assert(plan.image.equations[0].rhs);
  assert(plan.residuals.size() == 4);
}

// ---------------------------------------------------------------------------
// Boundary and provenance coverage. These checks report every failure instead
// of stopping at the first one, so one production defect does not hide others.

int failures = 0;

void expect(bool ok, const char *condition, const char *function, int line) {
  if (ok) return;
  ++failures;
  std::fprintf(stderr, "%s:%d: %s: expectation failed: %s\n", __FILE__, line,
               function, condition);
}
#define EXPECT(condition) expect((condition), #condition, __func__, __LINE__)

const auto unlimited = [](uint64_t) { return true; };

Status run(const std::vector<Premise> &input, const Limits &limits,
           Image &image) {
  return recognize(input, limits, unlimited, image);
}

// Recognition only writes its output on Complete.
Image sentinel_image() {
  Image image;
  image.clauses_seen = 777;
  return image;
}

bool untouched(const Image &image) {
  return image.clauses_seen == 777 && image.equations.empty() &&
         image.consequences.empty() && image.patterns_seen == 0;
}

External variable_of(External literal) {
  return literal < 0 ? -literal : literal;
}

// Bit i is set when the clause negates its i-th smallest variable; this is
// the forbidden assignment the clause encodes.
uint32_t negation_mask(const std::vector<External> &clause) {
  std::vector<External> literals = clause;
  std::sort(literals.begin(), literals.end(),
            [](External a, External b) { return variable_of(a) < variable_of(b); });
  uint32_t mask = 0;
  for (size_t i = 0; i < literals.size(); ++i)
    if (literals[i] < 0) mask |= uint32_t{1} << i;
  return mask;
}

// An equation reports one witness per forbidden assignment, in ascending
// forbidden-assignment order, and premises[i] names the input clause whose
// exact physical body is premise_clauses[i].
void expect_witnesses(const Equation &equation, std::vector<Premise> expected,
                      int line) {
  std::sort(expected.begin(), expected.end(),
            [](const Premise &a, const Premise &b) {
              return negation_mask(a.literals) < negation_mask(b.literals);
            });
  std::vector<uint64_t> occurrences;
  std::vector<std::vector<External>> bodies;
  for (const Premise &premise : expected) {
    occurrences.push_back(premise.occurrence);
    bodies.push_back(premise.literals);
  }
  expect(equation.premises == occurrences, "equation.premises == occurrences",
         "expect_witnesses", line);
  expect(equation.premise_clauses == bodies,
         "equation.premise_clauses == bodies", "expect_witnesses", line);
  expect(authenticates_clausal_form(equation),
         "authenticates_clausal_form(equation)", "expect_witnesses", line);
}
#define EXPECT_WITNESSES(equation, expected)                                   \
  expect_witnesses((equation), (expected), __LINE__)

size_t find_equation(const Image &image, const std::vector<External> &variables,
                     bool rhs) {
  for (size_t i = 0; i < image.equations.size(); ++i)
    if (image.equations[i].variables == variables &&
        image.equations[i].rhs == rhs)
      return i;
  return SIZE_MAX;
}

// A consequence must be the XOR of its sorted source equations, and its
// premises must be their premises flattened in source order.
void expect_consequence_trace(const Image &image, const Consequence &fact,
                              int line) {
  bool ok = !fact.sources.empty() &&
            std::is_sorted(fact.sources.begin(), fact.sources.end()) &&
            std::adjacent_find(fact.sources.begin(), fact.sources.end()) ==
                fact.sources.end();
  std::vector<External> variables;
  bool rhs = false;
  std::vector<uint64_t> flattened;
  for (size_t source : fact.sources) {
    if (source >= image.equations.size()) {
      ok = false;
      break;
    }
    const Equation &equation = image.equations[source];
    std::vector<External> next;
    std::set_symmetric_difference(variables.begin(), variables.end(),
                                  equation.variables.begin(),
                                  equation.variables.end(),
                                  std::back_inserter(next));
    variables = std::move(next);
    rhs ^= equation.rhs;
    flattened.insert(flattened.end(), equation.premises.begin(),
                     equation.premises.end());
  }
  expect(ok, "consequence sources are sorted, distinct, and in range",
         "expect_consequence_trace", line);
  expect(variables == fact.variables && rhs == fact.rhs,
         "consequence == XOR of its sources", "expect_consequence_trace", line);
  expect(flattened == fact.premises,
         "consequence premises == source premises in source order",
         "expect_consequence_trace", line);
}
#define EXPECT_TRACE(image, fact) expect_consequence_trace((image), (fact), __LINE__)

std::vector<Premise> concatenated(std::vector<std::vector<Premise>> parts) {
  std::vector<Premise> result;
  for (auto &part : parts)
    result.insert(result.end(), part.begin(), part.end());
  return result;
}

// Reorders a clause's literals without changing the clause.
void permute_by_variable(std::vector<External> &literals, size_t steps) {
  const auto by_variable = [](External a, External b) {
    return variable_of(a) < variable_of(b);
  };
  std::sort(literals.begin(), literals.end(), by_variable);
  for (size_t i = 0; i < steps; ++i)
    std::next_permutation(literals.begin(), literals.end(), by_variable);
}

// A1: each clause may list its literals in its own order.
void per_clause_literal_order_is_irrelevant() {
  uint64_t occurrence = 500;
  auto input = premises({2, 4, 6, 8}, true, occurrence);
  std::set<std::vector<External>> orders;
  for (size_t i = 0; i < input.size(); ++i) {
    permute_by_variable(input[i].literals, 3 * i);
    std::vector<External> order;
    for (External literal : input[i].literals)
      order.push_back(variable_of(literal));
    orders.insert(order);
  }
  EXPECT(orders.size() == input.size()); // The fixture really is heterogeneous.
  std::swap(input[1], input[6]);

  Image image;
  EXPECT(run(input, discovery_limits(), image) == Status::Complete);
  EXPECT(image.equations.size() == 1);
  if (image.equations.size() != 1) return;
  EXPECT(image.equations[0].variables == std::vector<External>({2, 4, 6, 8}));
  EXPECT(image.equations[0].rhs);
  EXPECT_WITNESSES(image.equations[0], input);
}

// A2: the largest external variable is ordinary; INT32_MIN has no negation.
void extreme_external_literals() {
  constexpr External top = INT32_MAX;
  uint64_t occurrence = 520;
  auto input = premises({1, top - 1, top}, false, occurrence);
  for (Premise &premise : input)
    std::reverse(premise.literals.begin(), premise.literals.end());

  Image image;
  EXPECT(run(input, discovery_limits(), image) == Status::Complete);
  EXPECT(image.equations.size() == 1);
  if (image.equations.size() == 1) {
    EXPECT(image.equations[0].variables ==
           std::vector<External>({1, top - 1, top}));
    EXPECT(!image.equations[0].rhs);
    EXPECT_WITNESSES(image.equations[0], input);
  }

  const std::vector<std::vector<External>> malformed{
      {1, INT32_MIN, 3}, {INT32_MIN}, {0, 2}, {INT32_MIN, 5, -5}, {-top, 0}};
  for (const auto &body : malformed) {
    auto bad = input;
    bad.insert(bad.begin() + 1, Premise{occurrence++, body});
    Image sentinel = sentinel_image();
    EXPECT(run(bad, discovery_limits(), sentinel) == Status::Invalid);
    EXPECT(untouched(sentinel));
  }
}

// A3: the first occurrence of a repeated pattern is the witness, and its
// physical literal order is reported verbatim.
void first_duplicate_is_the_physical_witness() {
  uint64_t occurrence = 540;
  const auto originals = premises({3, 5, 9}, false, occurrence);
  std::vector<Premise> input, first;
  for (size_t i = 0; i < originals.size(); ++i) {
    Premise copy = originals[i];
    copy.occurrence = occurrence++;
    std::rotate(copy.literals.begin(), copy.literals.begin() + 1,
                copy.literals.end());
    Premise late = originals[i];
    late.occurrence = occurrence++;
    std::reverse(late.literals.begin(), late.literals.end());
    // Half of the patterns are first seen through their rotated duplicate.
    if (i % 2 == 0) {
      input.insert(input.end(), {copy, originals[i]});
      first.push_back(copy);
    } else {
      input.insert(input.end(), {originals[i], copy});
      first.push_back(originals[i]);
    }
    input.push_back(late);
  }

  Image image;
  EXPECT(run(input, discovery_limits(), image) == Status::Complete);
  EXPECT(image.clauses_seen == input.size());
  EXPECT(image.patterns_seen == originals.size());
  EXPECT(image.equations.size() == 1);
  if (image.equations.size() == 1) EXPECT_WITNESSES(image.equations[0], first);
}

// B1: duplicates consume clause budget but not pattern budget; both limits are
// inclusive.
void duplicate_budget_boundaries() {
  uint64_t occurrence = 560;
  const auto originals = premises({1, 2, 3}, true, occurrence);
  std::vector<Premise> input;
  for (const Premise &original : originals) {
    Premise duplicate = original;
    duplicate.occurrence = occurrence++;
    std::reverse(duplicate.literals.begin(), duplicate.literals.end());
    input.insert(input.end(), {original, duplicate});
  }
  // Skipped clauses still count as seen clauses.
  input.push_back({occurrence++, {1, 2, 3, 4, 5, 6, 7, 8, 9}});
  const uint64_t clauses = input.size();
  const uint64_t patterns = originals.size();

  Limits limits = discovery_limits();
  limits.maximum_clauses = clauses;
  limits.maximum_patterns = patterns;
  limits.maximum_groups = 1;
  Image image;
  EXPECT(run(input, limits, image) == Status::Complete);
  EXPECT(image.clauses_seen == clauses);
  EXPECT(image.patterns_seen == patterns);
  EXPECT(image.groups_seen == 1);
  EXPECT(image.equations.size() == 1);

  Limits fewer_clauses = limits;
  fewer_clauses.maximum_clauses = clauses - 1;
  Image sentinel = sentinel_image();
  EXPECT(run(input, fewer_clauses, sentinel) == Status::Exhausted);
  EXPECT(untouched(sentinel));

  Limits fewer_patterns = limits;
  fewer_patterns.maximum_patterns = patterns - 1;
  sentinel = sentinel_image();
  EXPECT(run(input, fewer_patterns, sentinel) == Status::Exhausted);
  EXPECT(untouched(sentinel));

  auto two_groups = premises({1, 2, 4}, false, occurrence);
  two_groups.insert(two_groups.end(), input.begin(), input.end());
  limits.maximum_clauses = two_groups.size();
  limits.maximum_patterns = 2 * patterns;
  limits.maximum_groups = 2;
  EXPECT(run(two_groups, limits, image) == Status::Complete);
  EXPECT(image.groups_seen == 2);
  limits.maximum_groups = 1;
  sentinel = sentinel_image();
  EXPECT(run(two_groups, limits, sentinel) == Status::Exhausted);
  EXPECT(untouched(sentinel));
}

// B2: occurrence identity is validated before any clause is skipped.
void skipped_clauses_still_own_unique_occurrences() {
  uint64_t occurrence = 600;
  const auto base = premises({1, 2, 3}, false, occurrence); // 600..603
  const std::vector<External> over_arity{1, 2, 3, 4, 5, 6, 7, 8, 9};
  const std::vector<External> repeated{5, -5, 6};
  const std::vector<std::vector<Premise>> cases{
      concatenated({base, {{601, over_arity}}}),
      concatenated({{{602, repeated}}, base}),
      concatenated({base, {{700, over_arity}, {700, repeated}}}),
      concatenated({base, {{600, {}}}}),
      concatenated({base, {{0, over_arity}}}),
      concatenated({{{0, repeated}}, base}),
  };
  for (const auto &input : cases) {
    Image sentinel = sentinel_image();
    EXPECT(run(input, discovery_limits(), sentinel) == Status::Invalid);
    EXPECT(untouched(sentinel));
  }
}

// B3: a malformed literal is invalid input regardless of configured arity.
void malformed_over_arity_clauses_are_invalid() {
  uint64_t occurrence = 640;
  const auto base = premises({1, 2, 3}, false, occurrence);
  const std::vector<std::vector<External>> malformed{
      {1, 2, 3, 4, 5, 6, 7, 8, 0},
      {INT32_MIN, 1, 2, 3, 4, 5, 6, 7, 8},
  };
  for (const auto &body : malformed) {
    Image sentinel = sentinel_image();
    EXPECT(run(concatenated({base, {{occurrence++, body}}}),
               discovery_limits(), sentinel) == Status::Invalid);
    EXPECT(untouched(sentinel));
  }
  Limits narrow = discovery_limits();
  narrow.maximum_arity = 3;
  for (const External bad : {External{0}, External{INT32_MIN}}) {
    Image sentinel = sentinel_image();
    EXPECT(run(concatenated({base, {{occurrence++, {1, 2, 3, bad}}}}), narrow,
               sentinel) == Status::Invalid);
    EXPECT(untouched(sentinel));
  }
}

// C1: a partial opposite parity in the same scope is ignored.
void complete_and_partial_parities_share_a_scope() {
  uint64_t occurrence = 660;
  const auto even = premises({1, 2, 3}, false, occurrence);
  const auto odd = premises({1, 2, 3}, true, occurrence);
  const std::vector<Premise> input{even[0], odd[0], even[1],
                                   even[2], odd[3], even[3]};

  Image image;
  EXPECT(run(input, discovery_limits(), image) == Status::Complete);
  EXPECT(image.patterns_seen == input.size());
  EXPECT(image.groups_seen == 1);
  EXPECT(image.equations.size() == 1);
  EXPECT(image.consequences.empty());
  if (image.equations.size() != 1) return;
  EXPECT(image.equations[0].variables == std::vector<External>({1, 2, 3}));
  EXPECT(!image.equations[0].rhs);
  EXPECT_WITNESSES(image.equations[0], even);
}

// C2: both complete parities give two disjointly supported equations whose
// sum is the empty contradiction.
void both_complete_parities_are_disjoint_and_contradict() {
  uint64_t occurrence = 680;
  const auto even = premises({1, 2, 3}, false, occurrence);
  const auto odd = premises({1, 2, 3}, true, occurrence);
  std::vector<Premise> input;
  for (size_t i = 0; i < even.size(); ++i)
    input.insert(input.end(), {odd[odd.size() - 1 - i], even[i]});

  Image image;
  EXPECT(run(input, discovery_limits(), image) == Status::Complete);
  EXPECT(image.equations.size() == 2);
  const size_t zero = find_equation(image, {1, 2, 3}, false);
  const size_t one = find_equation(image, {1, 2, 3}, true);
  EXPECT(zero != SIZE_MAX && one != SIZE_MAX);
  if (zero == SIZE_MAX || one == SIZE_MAX) return;
  EXPECT_WITNESSES(image.equations[zero], even);
  EXPECT_WITNESSES(image.equations[one], odd);
  std::set<uint64_t> support(image.equations[zero].premises.begin(),
                             image.equations[zero].premises.end());
  for (uint64_t premise : image.equations[one].premises)
    EXPECT(!support.contains(premise));

  EXPECT(image.consequences.size() == 1);
  if (image.consequences.size() != 1) return;
  const Consequence &contradiction = image.consequences[0];
  EXPECT(contradiction.variables.empty());
  EXPECT(contradiction.rhs);
  EXPECT(contradiction.sources ==
         std::vector<size_t>({std::min(zero, one), std::max(zero, one)}));
  EXPECT_TRACE(image, contradiction);
}

// x1+x2=0, x1+x3=1, x2+x3=0 over three distinct scopes; only their
// three-way sum is contradictory. Input order is deliberately not scope order.
std::vector<Premise> three_source_contradiction() {
  uint64_t occurrence = 720;
  auto x23 = premises({2, 3}, false, occurrence);
  auto x13 = premises({1, 3}, true, occurrence);
  auto x12 = premises({1, 2}, false, occurrence);
  std::reverse(x13.begin(), x13.end());
  return concatenated({x23, x13, x12});
}

// D1
void three_source_contradiction_is_traced() {
  const auto input = three_source_contradiction();
  Image image;
  EXPECT(run(input, discovery_limits(), image) == Status::Complete);
  EXPECT(image.equations.size() == 3);
  std::vector<size_t> sources{find_equation(image, {1, 2}, false),
                              find_equation(image, {1, 3}, true),
                              find_equation(image, {2, 3}, false)};
  EXPECT(std::find(sources.begin(), sources.end(), SIZE_MAX) == sources.end());
  std::sort(sources.begin(), sources.end());

  size_t contradictions = 0;
  for (const Consequence &fact : image.consequences) {
    EXPECT_TRACE(image, fact);
    if (!fact.variables.empty()) continue;
    ++contradictions;
    EXPECT(fact.rhs);
    EXPECT(fact.sources == sources);
    EXPECT(fact.premises.size() == input.size());
  }
  EXPECT(contradictions == 1);
}

// D3: the elimination-work limit admits exactly the work actually spent.
void elimination_work_boundary_is_inclusive() {
  const auto input = three_source_contradiction();
  Image measured;
  EXPECT(run(input, discovery_limits(), measured) == Status::Complete);
  const uint64_t work = measured.elimination_work;
  EXPECT(work > 0);

  Limits exact = discovery_limits();
  exact.maximum_elimination_work = work;
  Image image;
  EXPECT(run(input, exact, image) == Status::Complete);
  EXPECT(image.elimination_work == work);
  EXPECT(image.equations.size() == measured.equations.size());
  EXPECT(image.consequences.size() == measured.consequences.size());

  Limits short_by_one = exact;
  short_by_one.maximum_elimination_work = work - 1;
  Image sentinel = sentinel_image();
  EXPECT(run(input, short_by_one, sentinel) == Status::Exhausted);
  EXPECT(untouched(sentinel));
}

// D4: discovery charges once per scanned clause, then once per elimination
// step; refusing any elimination charge interrupts without publishing.
void interruption_during_elimination_leaves_output_untouched() {
  const auto input = three_source_contradiction();
  std::vector<uint64_t> charges;
  Image image;
  EXPECT(recognize(input, discovery_limits(),
                   [&](uint64_t amount) {
                     charges.push_back(amount);
                     return true;
                   },
                   image) == Status::Complete);

  const size_t scan_calls = input.size();
  EXPECT(charges.size() > scan_calls);
  if (charges.size() <= scan_calls) return;
  uint64_t scan = 0, expected_scan = 0, elimination = 0;
  for (size_t i = 0; i < scan_calls; ++i) {
    scan += charges[i];
    expected_scan += 1 + input[i].literals.size();
  }
  for (size_t i = scan_calls; i < charges.size(); ++i) elimination += charges[i];
  EXPECT(scan == expected_scan);
  EXPECT(elimination == image.elimination_work);

  for (size_t refused = scan_calls; refused < charges.size(); ++refused) {
    size_t calls = 0;
    Image sentinel = sentinel_image();
    EXPECT(recognize(input, discovery_limits(),
                     [&](uint64_t) { return calls++ != refused; },
                     sentinel) == Status::Interrupted);
    EXPECT(calls == refused + 1);
    EXPECT(untouched(sentinel));
  }
}

RootImageLimits root_limits() {
  RootImageLimits limits;
  limits.image = discovery_limits();
  limits.maximum_sources = 100;
  limits.maximum_literals = 1000;
  limits.maximum_units = 16;
  limits.maximum_work = 1000000;
  limits.maximum_bytes = 1000000;
  limits.maximum_proof_additions = 100;
  return limits;
}

AffineLiveSource large_source(uint64_t occurrence, uint64_t clause_id,
                              std::vector<External> clause) {
  AffineLiveSource source;
  source.occurrence = occurrence;
  source.clause_id = clause_id;
  source.clause = std::move(clause);
  return source;
}

AffineLiveSource root_unit(uint64_t occurrence, External literal) {
  AffineLiveSource unit;
  unit.occurrence = occurrence;
  unit.kind = AffineSourceKind::RootUnit;
  unit.clause_id = 0;
  unit.proof_id_exact = false;
  unit.clause = {literal};
  unit.recognition_candidate = false;
  return unit;
}

// E1: substituting a true root fact into an XOR's own scope flips its RHS; a
// false one preserves it.
void root_substitution_inside_scope() {
  for (const bool rhs : {false, true})
    for (const External root : {External{4}, External{-4}}) {
      std::vector<AffineLiveSource> sources;
      uint64_t occurrence = 800;
      for (auto clause : clausal_form({1, 2, 3, 4}, rhs)) {
        sources.push_back(large_source(occurrence, occurrence + 50, clause));
        ++occurrence;
      }
      sources.push_back(root_unit(occurrence, root));

      RootImagePlan plan;
      EXPECT(prepare_affine_root_image(sources, {root}, root_limits(),
                                       unlimited, plan) ==
             RootImageStatus::Prepared);
      EXPECT(plan.certified_root_facts == std::vector<External>({root}));
      EXPECT(plan.image.equations.size() == 1);
      if (plan.image.equations.size() != 1) continue;
      EXPECT(plan.image.equations[0].variables ==
             std::vector<External>({1, 2, 3}));
      EXPECT(plan.image.equations[0].rhs == (rhs != (root > 0)));
      EXPECT(plan.residuals.size() == 4);
      for (const RootResidual &residual : plan.residuals) {
        EXPECT(residual.removed_root_facts == std::vector<External>({root}));
        EXPECT(residual.clause.size() == 3);
      }
    }
}

// E3: after root shortening, dropped noise and renumbering, every equation
// premise names the residual whose body it reports, and that residual keeps
// the exact physical source it was shortened from.
void residual_alignment_survives_noise_and_renumbering() {
  constexpr External guard = 9;
  std::vector<std::vector<External>> guarded =
      clausal_form({2, 5, 8}, true);
  for (auto &clause : guarded) clause.push_back(-guard);
  const auto plain = clausal_form({3, 6}, false);

  const std::vector<std::vector<External>> bodies{
      {1, -4, 7},           // partial scope: residual, never selected
      guarded[0],
      {guard, 1, 2},        // satisfied by the root fact: dropped
      plain[0],
      guarded[1],
      {10, -10, 11},        // repeated variable: dropped
      {-guard, 4, -7},      // shortened partial scope: never selected
      guarded[2],
      plain[1],
      guarded[3],
  };
  // Sparse occurrence and clause IDs make any renumbering slip visible.
  std::vector<AffineLiveSource> sources;
  for (size_t k = 0; k < bodies.size(); ++k)
    sources.push_back(large_source(1000 + 7 * k, 50000 + 13 * k, bodies[k]));
  sources.insert(sources.begin() + 6, root_unit(2000, guard));
  std::map<uint64_t, size_t> position;
  for (size_t k = 0; k < sources.size(); ++k)
    position[sources[k].occurrence] = k;

  RootImagePlan plan;
  EXPECT(prepare_affine_root_image(sources, {guard}, root_limits(), unlimited,
                                   plan) == RootImageStatus::Prepared);
  EXPECT(plan.image.equations.size() == 2);
  EXPECT(find_equation(plan.image, {2, 5, 8}, true) != SIZE_MAX);
  EXPECT(find_equation(plan.image, {3, 6}, false) != SIZE_MAX);
  EXPECT(plan.residuals.size() == guarded.size() + plain.size());

  for (size_t i = 0; i < plan.residuals.size(); ++i) {
    const RootResidual &residual = plan.residuals[i];
    EXPECT(residual.occurrence == i + 1);
    const auto found = position.find(residual.physical_source.occurrence);
    EXPECT(found != position.end());
    if (found == position.end()) continue;
    // Residuals keep physical source order.
    if (i) EXPECT(position[plan.residuals[i - 1].physical_source.occurrence] <
                  found->second);
    const AffineLiveSource &physical = sources[found->second];
    EXPECT(residual.physical_source.clause_id == physical.clause_id);
    EXPECT(residual.physical_source.clause == physical.clause);
    std::vector<External> shortened;
    for (External literal : physical.clause)
      if (literal != -guard) shortened.push_back(literal);
    EXPECT(residual.clause == shortened);
    EXPECT(residual.removed_root_facts ==
           (shortened.size() < physical.clause.size()
                ? std::vector<External>({guard})
                : std::vector<External>()));
  }

  std::set<uint64_t> referenced;
  for (const Equation &equation : plan.image.equations) {
    EXPECT(equation.premises.size() == equation.premise_clauses.size());
    for (size_t i = 0; i < equation.premises.size(); ++i) {
      const uint64_t premise = equation.premises[i];
      EXPECT(premise >= 1 && premise <= plan.residuals.size());
      if (premise < 1 || premise > plan.residuals.size()) continue;
      EXPECT(referenced.insert(premise).second);
      EXPECT(plan.residuals[premise - 1].occurrence == premise);
      EXPECT(plan.residuals[premise - 1].clause == equation.premise_clauses[i]);
    }
  }
  EXPECT(referenced.size() == plan.residuals.size());
}

} // namespace

int main() {
  order_and_sign_normalization();
  duplicate_occurrences_do_not_fake_rank();
  overlapping_equations_are_separate_and_composable();
  partial_and_malformed_populations_are_not_xors();
  root_guard_shortening_preserves_the_equation();

  per_clause_literal_order_is_irrelevant();
  extreme_external_literals();
  first_duplicate_is_the_physical_witness();
  duplicate_budget_boundaries();
  skipped_clauses_still_own_unique_occurrences();
  malformed_over_arity_clauses_are_invalid();
  complete_and_partial_parities_share_a_scope();
  both_complete_parities_are_disjoint_and_contradict();
  three_source_contradiction_is_traced();
  elimination_work_boundary_is_inclusive();
  interruption_during_elimination_leaves_output_untouched();
  root_substitution_inside_scope();
  residual_alignment_survives_noise_and_renumbering();
  if (failures) std::fprintf(stderr, "%d expectation(s) failed\n", failures);
  return failures ? 1 : 0;
}
