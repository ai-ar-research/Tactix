#include "unit_test.hpp"
// The pure Tseitin-cycle stages: planning, admission and transaction preflight.
#include "tseitin_test_support.hpp"
#include "rules/lucky/tseitin_cycle_transaction.hpp"

#include <map>

using namespace tactix::tseitin_cycle;
using tseitin_test::InterruptState;
using tseitin_test::interrupt_after;
using tactix::parity_certificate::xor_equation;

namespace {

Input classified(std::vector<VertexEquation> equations) {
  Input input;
  input.equations = std::move(equations);
  input.occurrence_classification_complete = true;
  for (const VertexEquation &equation : input.equations)
    for (Edge edge : equation.edges)
      input.occurrences.push_back(
          {edge, OccurrenceKind::VertexParity, equation.vertex});
  return input;
}

bool valid(const Input &input, const Candidate &candidate) {
  uint64_t work = 0;
  bool exhausted = false, interrupted = false;
  return validate_candidate(input, candidate, UINT64_MAX, nullptr, nullptr,
                            work, exhausted, interrupted);
}

// Exhaustive X7 oracle: on every assignment falsifying the candidate unit
// (pivot true), the substitution keeps every vertex parity.
bool preserves(const Input &input, const Candidate &candidate) {
  std::map<Edge, ImageKind> images;
  for (const SubstitutionImage &image : candidate.substitution)
    images[image.source] = image.kind;
  Edge maximum = 0;
  for (const VertexEquation &equation : input.equations)
    for (Edge edge : equation.edges) maximum = std::max(maximum, edge);
  assert(maximum < 20);
  for (uint64_t mask = 0; mask < (uint64_t{1} << maximum); ++mask) {
    const auto value = [&](Edge edge) { return bool(mask >> (edge - 1) & 1); };
    if (!value(candidate.pivot)) continue;
    for (const VertexEquation &equation : input.equations) {
      bool before = false, after = false;
      for (Edge edge : equation.edges) {
        before ^= value(edge);
        const auto found = images.find(edge);
        // A constant image is false and contributes nothing.
        after ^= found == images.end()
                     ? value(edge)
                     : found->second == ImageKind::NegatedSelf && !value(edge);
      }
      if (before != after) return false;
    }
  }
  return true;
}

void planner() {
  const Limits ample{1000, 16, 1};
  // Charges XOR to one; each edge occurs at its two graph endpoints.
  const Input base = classified(
      {{1, true, {1, 3}}, {2, false, {1, 2}}, {3, false, {2, 3}}});
  const Result planned = plan_one(base, ample);
  assert(planned.planned() && planned.candidate.pivot == 3);
  assert((planned.candidate.cycle_edges == std::vector<Edge>{3, 1, 2}));
  assert(valid(base, planned.candidate) && preserves(base, planned.candidate));
  Candidate broken = planned.candidate;
  broken.substitution.back().kind = ImageKind::Constant;
  assert(!valid(base, broken) && !preserves(base, broken));

  // Input order does not affect the chosen chord, path, or substitution.
  Input shuffled = base;
  std::reverse(shuffled.equations.begin(), shuffled.equations.end());
  std::reverse(shuffled.occurrences.begin(), shuffled.occurrences.end());
  const Result same = plan_one(shuffled, ample);
  assert(same.planned() &&
         same.candidate.cycle_edges == planned.candidate.cycle_edges);

  Input incomplete = base, residual = base, missing = base, even = base;
  incomplete.occurrence_classification_complete = false;
  residual.occurrences.push_back({2, OccurrenceKind::Residual, 0});
  missing.occurrences.pop_back();
  even.equations[0].charge = false;
  const std::pair<Input, DeclineReason> declines[] = {
      {incomplete, DeclineReason::IncompleteOccurrenceClassification},
      {residual, DeclineReason::ResidualOccurrence},
      {missing, DeclineReason::OccurrenceMismatch},
      {even, DeclineReason::EvenCharge},
      {classified({{1, true, {1, 2}}, {2, false, {1, 2}},
                   {3, false, {3, 4}}, {4, false, {3, 4}}}),
       DeclineReason::Disconnected},
      {classified({{1, true, {1}}, {2, false, {1, 2}}, {3, false, {2}}}),
       DeclineReason::Acyclic}};
  for (const auto &[input, reason] : declines)
    assert(plan_one(input, ample).decline == reason);
  assert(!valid(residual, planned.candidate));
  assert(plan_one(base, {1000, 16, 0}).decline ==
         DeclineReason::NoAdditionAllowance);
  assert(plan_one(base, {1000, 2, 1}).decline ==
         DeclineReason::WitnessLengthLimit);
  assert(plan_one(base, {planned.work - 1, 16, 1}).decline ==
         DeclineReason::WorkLimit);
}

// The odd triangle as authenticated groups whose stable identities run
// opposite to discovery order, with reversed premise literals and one
// unrelated live clause.
AdmissionRequest triangle(const std::map<Edge, Edge> &rename = {}) {
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
    identity -= 10;
    group.certified.equation.size = static_cast<uint8_t>(variables.size());
    group.certified.equation.rhs = rhs;
    std::copy(variables.begin(), variables.end(),
              group.certified.equation.variables.begin());
    for (const auto &clause : xor_equation(variables.data(), variables.size(), rhs)) {
      LiveClause live{occurrence++, proof++,
                      {clause.lits.begin(), clause.lits.begin() + clause.size}};
      std::reverse(live.literals.begin(), live.literals.end());
      group.certified.premises.push_back(live.occurrence);
      group.premise_proofs.push_back(live.proof);
      request.live_clauses.push_back(std::move(live));
    }
    request.groups.push_back(std::move(group));
  }
  request.live_clauses.push_back({occurrence, proof, {991}});
  return request;
}

AdmissionRequest reversed(AdmissionRequest request) {
  std::reverse(request.groups.begin(), request.groups.end());
  std::reverse(request.live_clauses.begin(), request.live_clauses.end());
  for (LiveClause &clause : request.live_clauses)
    std::reverse(clause.literals.begin(), clause.literals.end());
  return request;
}

void admission() {
  const AdmissionLimits ample{10000, nullptr, nullptr};
  const AdmissionRequest request = triangle();
  const AdmissionResult accepted = admit(request, ample);
  assert(accepted.admitted() && accepted.input.occurrence_classification_complete);
  assert(accepted.provenance.size() == 3 &&
         accepted.provenance[0].group_identity == 20 &&
         accepted.provenance[2].group_identity == 40);
  assert(plan_one(accepted.input, {1000, 16, 1}).planned());

  // Discovery, clause and literal order and positive renaming keep the graph.
  assert(admit(reversed(request), ample).provenance == accepted.provenance);
  const AdmissionResult renamed =
      admit(triangle({{1, 17}, {2, 5}, {3, 29}}), ample);
  assert(renamed.admitted() &&
         renamed.input.occurrences.size() == accepted.input.occurrences.size());
  assert(plan_one(renamed.input, {1000, 16, 1}).planned());

  const auto rejects = [&](const AdmissionRequest &changed,
                           AdmissionReason reason) {
    const AdmissionResult rejected = admit(changed, ample);
    assert(rejected.reason == reason);
    assert(rejected.input.equations.empty() && rejected.provenance.empty());
  };
  AdmissionRequest changed = request;
  changed.live_clauses.push_back({999, 9999, {-1}});
  rejects(changed, AdmissionReason::ResidualEdgeTouch);
  changed = request;
  changed.live_clauses.erase(changed.live_clauses.begin());
  rejects(changed, AdmissionReason::PartialPremiseGroup);
  // A truncated certified group fails the premise-count guard.
  changed = request;
  changed.groups[0].certified.premises.pop_back();
  changed.groups[0].premise_proofs.pop_back();
  rejects(changed, AdmissionReason::PartialPremiseGroup);
  changed = request;
  changed.live_clauses[0].literals[0] *= -1;
  rejects(changed, AdmissionReason::PremiseClauseMismatch);
  changed = request;
  changed.groups[1].identity = changed.groups[0].identity;
  rejects(changed, AdmissionReason::DuplicateGroupIdentity);
  changed = request;
  changed.live_clauses[1].occurrence = changed.live_clauses[0].occurrence;
  rejects(changed, AdmissionReason::DuplicateOccurrenceIdentity);
  changed = request;
  changed.live_clauses[1].proof = changed.live_clauses[0].proof;
  rejects(changed, AdmissionReason::DuplicateProofIdentity);
  changed = request;
  changed.groups[1].certified.premises[0] = changed.groups[2].certified.premises[0];
  changed.groups[1].premise_proofs[0] = changed.groups[2].premise_proofs[0];
  rejects(changed, AdmissionReason::PremiseReused);
  changed = request;
  ++changed.groups[0].premise_proofs[0];
  rejects(changed, AdmissionReason::PremiseIdentityMismatch);
  // An even group with a correct encoding passes authentication, then the
  // whole system declines for even charge.
  changed = request;
  auto &even = changed.groups[0].certified;
  even.equation.rhs = false;
  const auto clauses = xor_equation(even.equation.variables.data(),
                                    even.equation.size, false);
  for (size_t i = 0; i < clauses.size(); ++i)
    for (LiveClause &clause : changed.live_clauses)
      if (clause.occurrence == even.premises[i])
        clause.literals.assign(clauses[i].lits.begin(),
                               clauses[i].lits.begin() + clauses[i].size);
  rejects(changed, AdmissionReason::EvenCharge);

  AdmissionResult stopped = admit(request, {accepted.work - 1, nullptr, nullptr});
  assert(stopped.status == AdmissionStatus::ResourceExhausted &&
         stopped.reason == AdmissionReason::WorkLimit && stopped.provenance.empty());
  InterruptState interruption{0, 5};
  stopped = admit(request, {10000, interrupt_after, &interruption});
  assert(stopped.status == AdmissionStatus::Interrupted &&
         stopped.reason == AdmissionReason::Interrupted && stopped.provenance.empty());
}

TransactionLimits ample() { return {1000, 3, 2, 2, 100, nullptr, nullptr}; }

void transaction() {
  const AdmissionRequest request = triangle();
  const AdmissionResult admission = admit(request, {1000, nullptr, nullptr});
  const Result plan = plan_one(admission.input, {1000, 16, 1});
  assert(admission.admitted() && plan.planned());
  const TransactionResult built = build_transaction(admission, plan, ample());
  assert(built.ready() && built.work > 0);
  const ProofTransaction &transaction = built.transaction;
  assert(transaction.identity.sources == admission.provenance);
  assert((transaction.x7.clause == std::vector<int32_t>{-3}));
  assert(transaction.x7.witness_true.empty() &&
         transaction.x7.substitution.size() == 2);
  for (const auto &[source, target] : transaction.x7.substitution)
    assert(target == -source);

  // Replay is independent of discovery and literal order while retaining IDs.
  const ReplayLimits replay{{1000, nullptr, nullptr}, {1000, 16, 1}, ample()};
  TransactionResult checked = replay_transaction(request, transaction, replay);
  assert(checked.ready() && checked.transaction == transaction);
  assert(replay_transaction(reversed(request), transaction, replay).ready());
  ProofTransaction corrupted = transaction;
  ++corrupted.identity.sources[0].premises[0].proof;
  checked = replay_transaction(request, corrupted, replay);
  assert(checked.status == TransactionStatus::InvalidInput &&
         checked.reason == TransactionReason::ReplayMismatch &&
         checked.transaction.version == 0);
  AdmissionRequest bad_source = request;
  bad_source.live_clauses[0].literals[0] *= -1;
  checked = replay_transaction(bad_source, transaction, replay);
  assert(checked.status == TransactionStatus::Declined &&
         checked.reason == TransactionReason::ReplayAdmissionFailed);

  const std::pair<void (*)(TransactionLimits &), TransactionReason> limits[] = {
      {[](TransactionLimits &l) { l.additions = 2; }, TransactionReason::AdditionLimit},
      {[](TransactionLimits &l) { l.deletions = 1; }, TransactionReason::DeletionLimit},
      {[](TransactionLimits &l) { l.helpers = 1; }, TransactionReason::HelperLimit},
      {[](TransactionLimits &l) { l.witness_literals = 4; },
       TransactionReason::WitnessLimit}};
  for (const auto &[limit, reason] : limits) {
    TransactionLimits limited = ample();
    limit(limited);
    const TransactionResult declined = build_transaction(admission, plan, limited);
    assert(declined.reason == reason && declined.transaction.version == 0);
  }
  TransactionLimits limited = ample();
  limited.work = built.work - 1;
  TransactionResult declined = build_transaction(admission, plan, limited);
  assert(declined.status == TransactionStatus::ResourceExhausted &&
         declined.reason == TransactionReason::WorkLimit &&
         declined.transaction.version == 0);
  InterruptState interrupted{0, 4};
  limited = ample();
  limited.interrupted = interrupt_after;
  limited.interrupt_state = &interrupted;
  declined = build_transaction(admission, plan, limited);
  assert(declined.status == TransactionStatus::Interrupted &&
         declined.reason == TransactionReason::Interrupted);

  AdmissionResult damaged_admission = admission;
  damaged_admission.provenance[0].premises[0].occurrence = 0;
  declined = build_transaction(damaged_admission, plan, ample());
  assert(declined.status == TransactionStatus::InvalidInput &&
         declined.reason == TransactionReason::InvalidSourceLedger);
  Result damaged_plan = plan;
  damaged_plan.candidate.substitution.back().kind = ImageKind::Constant;
  assert(build_transaction(admission, damaged_plan, ample()).reason ==
         TransactionReason::InvalidCandidate);
}

} // namespace

int main() {
  planner();
  admission();
  transaction();
  std::puts("test_tseitin_cycle_transaction: passed");
}
