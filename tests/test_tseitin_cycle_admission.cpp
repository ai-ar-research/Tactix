#undef NDEBUG
#include "rules/lucky/tseitin_cycle_admission.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <map>

using namespace tactix::tseitin_cycle;
using tactix::parity_certificate::xor_equation;

static AdmissionRequest triangle(const std::map<Edge, Edge> &rename = {}) {
  AdmissionRequest request;
  const auto edge = [&](Edge value) {
    const auto found = rename.find(value);
    return found == rename.end() ? value : found->second;
  };
  const std::vector<std::pair<bool, std::vector<Edge>>> equations = {
      {true, {edge(1), edge(3)}},
      {false, {edge(1), edge(2)}},
      {false, {edge(2), edge(3)}}};
  uint64_t occurrence = 100, proof = 1000, identity = 40;
  for (const auto &[rhs, unsorted] : equations) {
    std::vector<Edge> variables = unsorted;
    std::sort(variables.begin(), variables.end());
    CertifiedGroup group;
    group.identity = identity;
    identity -= 10; // Deliberately opposite discovery order.
    group.certified.equation.size = static_cast<uint8_t>(variables.size());
    group.certified.equation.rhs = rhs;
    for (size_t i = 0; i < variables.size(); ++i)
      group.certified.equation.variables[i] = variables[i];
    const auto clauses = xor_equation(variables.data(), variables.size(), rhs);
    for (auto clause : clauses) {
      LiveClause live;
      live.occurrence = occurrence++;
      live.proof = proof++;
      live.literals.assign(clause.lits.begin(), clause.lits.begin()+clause.size);
      std::reverse(live.literals.begin(), live.literals.end());
      group.certified.premises.push_back(live.occurrence);
      group.premise_proofs.push_back(live.proof);
      request.live_clauses.push_back(std::move(live));
    }
    request.groups.push_back(std::move(group));
  }
  // A completely unrelated live clause is allowed.
  request.live_clauses.push_back({occurrence, proof, {991}});
  return request;
}

static void exhaustive_encoding(const AdmissionRequest &request) {
  for (const CertifiedGroup &group : request.groups) {
    const auto &equation = group.certified.equation;
    for (uint32_t mask = 0; mask < (1u << equation.size); ++mask) {
      bool rhs = false, cnf = true;
      for (uint32_t i = 0; i < equation.size; ++i)
        rhs ^= bool(mask & (1u << i));
      for (uint64_t occurrence : group.certified.premises) {
        const auto found = std::find_if(
            request.live_clauses.begin(), request.live_clauses.end(),
            [&](const LiveClause &clause) {
              return clause.occurrence == occurrence;
            });
        assert(found != request.live_clauses.end());
        bool satisfied = false;
        for (int32_t literal : found->literals) {
          uint32_t at = 0;
          while (equation.variables[at] != std::abs(literal))
            ++at;
          const bool value = mask & (1u << at);
          satisfied |= literal > 0 ? value : !value;
        }
        cnf &= satisfied;
      }
      assert(cnf == (rhs == equation.rhs));
    }
  }
}

struct InterruptState { uint64_t calls = 0, stop = 0; };
static bool interrupt_after(void *opaque) {
  auto &state = *static_cast<InterruptState *>(opaque);
  return ++state.calls >= state.stop;
}

int main() {
  const AdmissionLimits ample{10000, nullptr, nullptr};
  AdmissionRequest request = triangle();
  exhaustive_encoding(request);
  const AdmissionResult accepted = admit(request, ample);
  assert(accepted.admitted());
  assert(accepted.input.occurrence_classification_complete);
  assert(accepted.input.equations.size() == 3);
  assert(accepted.provenance.size() == 3);
  assert(accepted.provenance[0].group_identity == 20);
  assert(accepted.provenance[1].group_identity == 30);
  assert(accepted.provenance[2].group_identity == 40);
  for (const EquationProvenance &source : accepted.provenance) {
    assert(source.vertex > 0);
    assert(source.premises.size() == 2);
    for (const StablePremise premise : source.premises)
      assert(premise.occurrence && premise.proof);
  }
  const Result plan = plan_one(accepted.input, {1000, 16, 1});
  assert(plan.planned());

  // Discovery order, clause order, and literal order do not change identities.
  AdmissionRequest reordered = request;
  std::reverse(reordered.groups.begin(), reordered.groups.end());
  std::reverse(reordered.live_clauses.begin(), reordered.live_clauses.end());
  for (LiveClause &clause : reordered.live_clauses)
    std::reverse(clause.literals.begin(), clause.literals.end());
  const AdmissionResult same = admit(reordered, ample);
  assert(same.admitted());
  assert(same.provenance.size() == accepted.provenance.size());
  for (size_t i = 0; i < same.provenance.size(); ++i) {
    assert(same.provenance[i].group_identity ==
           accepted.provenance[i].group_identity);
    assert(same.provenance[i].premises.size() ==
           accepted.provenance[i].premises.size());
  }

  // Arbitrary positive variable renaming preserves admission and graph shape.
  const AdmissionResult renamed =
      admit(triangle({{1, 17}, {2, 5}, {3, 29}}), ample);
  assert(renamed.admitted());
  assert(renamed.input.equations.size() == accepted.input.equations.size());
  assert(renamed.input.occurrences.size() == accepted.input.occurrences.size());
  assert(plan_one(renamed.input, {1000, 16, 1}).planned());

  AdmissionRequest residual = request;
  residual.live_clauses.push_back({999, 9999, {-1}});
  AdmissionResult rejected = admit(residual, ample);
  assert(rejected.reason == AdmissionReason::ResidualEdgeTouch);
  assert(rejected.input.equations.empty() && rejected.provenance.empty());

  AdmissionRequest partial = request;
  partial.live_clauses.erase(partial.live_clauses.begin());
  rejected = admit(partial, ample);
  assert(rejected.reason == AdmissionReason::PartialPremiseGroup);
  partial = request;
  partial.groups[0].certified.premises.pop_back();
  partial.groups[0].premise_proofs.pop_back();
  rejected = admit(partial, ample);
  assert(rejected.reason == AdmissionReason::PartialPremiseGroup);

  AdmissionRequest bad_clause = request;
  bad_clause.live_clauses[0].literals[0] *= -1;
  rejected = admit(bad_clause, ample);
  assert(rejected.reason == AdmissionReason::PremiseClauseMismatch);

  AdmissionRequest duplicate = request;
  duplicate.groups[1].identity = duplicate.groups[0].identity;
  rejected = admit(duplicate, ample);
  assert(rejected.reason == AdmissionReason::DuplicateGroupIdentity);
  duplicate = request;
  duplicate.live_clauses[1].occurrence =
      duplicate.live_clauses[0].occurrence;
  rejected = admit(duplicate, ample);
  assert(rejected.reason == AdmissionReason::DuplicateOccurrenceIdentity);
  duplicate = request;
  duplicate.live_clauses[1].proof = duplicate.live_clauses[0].proof;
  rejected = admit(duplicate, ample);
  assert(rejected.reason == AdmissionReason::DuplicateProofIdentity);
  duplicate = request;
  duplicate.groups[1].certified.premises[0] =
      duplicate.groups[2].certified.premises[0];
  duplicate.groups[1].premise_proofs[0] =
      duplicate.groups[2].premise_proofs[0];
  rejected = admit(duplicate, ample);
  assert(rejected.reason == AdmissionReason::PremiseReused);

  AdmissionRequest even = request;
  even.groups[0].certified.equation.rhs = false;
  // Rebuild the now-even group's clauses so authentication still succeeds.
  const auto &equation = even.groups[0].certified.equation;
  const auto clauses = xor_equation(equation.variables.data(), equation.size,
                                    equation.rhs);
  for (size_t i = 0; i < clauses.size(); ++i) {
    const uint64_t occurrence = even.groups[0].certified.premises[i];
    auto found = std::find_if(even.live_clauses.begin(), even.live_clauses.end(),
        [&](const LiveClause &clause) { return clause.occurrence == occurrence; });
    found->literals.assign(clauses[i].lits.begin(),
                           clauses[i].lits.begin()+clauses[i].size);
  }
  rejected = admit(even, ample);
  assert(rejected.reason == AdmissionReason::EvenCharge);

  assert(accepted.work > 0);
  rejected = admit(request, {accepted.work - 1, nullptr, nullptr});
  assert(rejected.status == AdmissionStatus::ResourceExhausted);
  assert(rejected.reason == AdmissionReason::WorkLimit);
  assert(rejected.input.equations.empty() && rejected.provenance.empty());

  InterruptState interruption{0, 5};
  rejected = admit(request, {10000, interrupt_after, &interruption});
  assert(rejected.status == AdmissionStatus::Interrupted);
  assert(rejected.reason == AdmissionReason::Interrupted);
  assert(rejected.input.equations.empty() && rejected.provenance.empty());

  std::printf("test_tseitin_cycle_admission: all passed; work %llu\n",
              static_cast<unsigned long long>(accepted.work));
}
