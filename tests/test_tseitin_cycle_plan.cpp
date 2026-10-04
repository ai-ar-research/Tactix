#undef NDEBUG
#include "rules/lucky/tseitin_cycle_plan.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>

using namespace tactix::tseitin_cycle;

static Input classified(std::vector<VertexEquation> equations) {
  Input input;
  input.equations = std::move(equations);
  input.occurrence_classification_complete = true;
  for (const VertexEquation &equation : input.equations)
    for (Edge edge : equation.edges)
      input.occurrences.push_back(
          {edge, OccurrenceKind::VertexParity, equation.vertex});
  return input;
}

static Input triangle() {
  // Charges XOR to one.  Each edge occurs at its two graph endpoints.
  return classified({{1, true, {1, 3}},
                     {2, false, {1, 2}},
                     {3, false, {2, 3}}});
}

static void exhaustive_semantics(const Input &input,
                                 const Candidate &candidate) {
  Edge maximum = 0;
  for (const VertexEquation &equation : input.equations)
    for (Edge edge : equation.edges)
      maximum = std::max(maximum, edge);
  assert(maximum < 20);
  for (uint64_t mask = 0; mask < (uint64_t{1} << maximum); ++mask) {
    std::vector<bool> assignment(static_cast<size_t>(maximum));
    for (Edge edge = 1; edge <= maximum; ++edge)
      assignment[static_cast<size_t>(edge) - 1] =
          mask & (uint64_t{1} << (edge - 1));
    if (!assignment[static_cast<size_t>(candidate.pivot) - 1])
      continue;
    assert(preserves_relations_on_falsifying_assignment(input, candidate,
                                                        assignment));
  }
}

int main() {
  const Limits ample{1000, 16, 1};
  const Input base = triangle();
  const Result planned = plan_one(base, ample);
  assert(planned.planned());
  assert(planned.requires_live_revalidation);
  assert(planned.candidate.pivot == 3);
  assert(!planned.candidate.satisfying_value);
  assert((planned.candidate.cycle_edges == std::vector<Edge>{3, 1, 2}));
  assert(planned.candidate.substitution.size() == 3);
  assert(planned.candidate.substitution[0].kind == ImageKind::Constant);
  assert(!planned.candidate.substitution[0].constant);
  assert(planned.candidate.substitution[1].kind == ImageKind::NegatedSelf);
  assert(planned.candidate.substitution[2].kind == ImageKind::NegatedSelf);
  assert(validate_candidate(base, planned.candidate));
  exhaustive_semantics(base, planned.candidate);

  // Input order does not affect the chosen chord, path, or substitution.
  Input shuffled = base;
  std::reverse(shuffled.equations.begin(), shuffled.equations.end());
  std::reverse(shuffled.occurrences.begin(), shuffled.occurrences.end());
  const Result same = plan_one(shuffled, ample);
  assert(same.planned());
  assert(same.candidate.pivot == planned.candidate.pivot);
  assert(same.candidate.cycle_edges == planned.candidate.cycle_edges);

  Input incomplete = base;
  incomplete.occurrence_classification_complete = false;
  assert(plan_one(incomplete, ample).decline ==
         DeclineReason::IncompleteOccurrenceClassification);

  Input residual = base;
  residual.occurrences.push_back({2, OccurrenceKind::Residual, 0});
  assert(plan_one(residual, ample).decline ==
         DeclineReason::ResidualOccurrence);
  assert(!validate_candidate(residual, planned.candidate));

  Input missing = base;
  missing.occurrences.pop_back();
  assert(plan_one(missing, ample).decline ==
         DeclineReason::OccurrenceMismatch);

  Input malformed = base;
  malformed.equations[0].edges.push_back(1);
  malformed.occurrences.push_back({1, OccurrenceKind::VertexParity, 1});
  assert(plan_one(malformed, ample).decline ==
         DeclineReason::DuplicateEdgeInEquation);

  Input one_endpoint = classified({{1, true, {1}}, {2, false, {}}});
  assert(plan_one(one_endpoint, ample).decline ==
         DeclineReason::EdgeNotInExactlyTwoVertices);

  Input disconnected = classified({{1, true, {1, 2}},
                                    {2, false, {1, 2}},
                                    {3, false, {3, 4}},
                                    {4, false, {3, 4}}});
  assert(plan_one(disconnected, ample).decline ==
         DeclineReason::Disconnected);

  Input even_charge = base;
  even_charge.equations[0].charge = false;
  assert(plan_one(even_charge, ample).decline == DeclineReason::EvenCharge);

  Input acyclic = classified({{1, true, {1}}, {2, false, {1, 2}},
                              {3, false, {2}}});
  assert(plan_one(acyclic, ample).decline == DeclineReason::Acyclic);

  assert(plan_one(base, {1000, 16, 0}).decline ==
         DeclineReason::NoAdditionAllowance);
  assert(plan_one(base, {1000, 2, 1}).decline ==
         DeclineReason::WitnessLengthLimit);
  assert(planned.work > 0);
  assert(plan_one(base, {planned.work - 1, 16, 1}).decline ==
         DeclineReason::WorkLimit);

  Candidate broken = planned.candidate;
  broken.substitution.back().kind = ImageKind::Constant;
  assert(!validate_candidate(base, broken));
  std::vector<bool> falsifying(3, false);
  assert(!preserves_relations_on_falsifying_assignment(base, broken,
                                                       falsifying));
  assert(!preserves_relations_on_falsifying_assignment(
      base, planned.candidate, falsifying));

  std::printf("test_tseitin_cycle_plan: all passed; work %llu\n",
              static_cast<unsigned long long>(planned.work));
}
