#include "rules/lucky/tseitin_cycle_admission.hpp"

#include "kernel/types.hpp"

#include <algorithm>
#include <map>
#include <queue>
#include <set>

namespace tactix::tseitin_cycle {
namespace {

using External = parity_premises::External;

class Meter {
  AdmissionLimits limits_;
  uint64_t used_ = 0;
  AdmissionStatus stopped_ = AdmissionStatus::Admitted;

public:
  explicit Meter(AdmissionLimits limits) : limits_(limits) {}
  bool spend(uint64_t amount = 1) {
    if (limits_.interrupted && limits_.interrupted(limits_.interrupt_state)) {
      stopped_ = AdmissionStatus::Interrupted;
      return false;
    }
    if (amount > limits_.work - used_) {
      stopped_ = AdmissionStatus::ResourceExhausted;
      return false;
    }
    used_ += amount;
    return true;
  }
  uint64_t used() const { return used_; }
  AdmissionStatus stopped() const { return stopped_; }
};

AdmissionResult stop(const Meter &meter, AdmissionStatus status,
                     AdmissionReason reason) {
  AdmissionResult result;
  result.status = status;
  result.reason = reason;
  result.work = meter.used();
  return result;
}

AdmissionResult metered_stop(const Meter &meter) {
  if (meter.stopped() == AdmissionStatus::Interrupted)
    return stop(meter, AdmissionStatus::Interrupted,
                AdmissionReason::Interrupted);
  return stop(meter, AdmissionStatus::ResourceExhausted,
              AdmissionReason::WorkLimit);
}

bool canonical_clause(const std::vector<External> &input,
                      std::vector<External> &output) {
  output = input;
  for (External literal : output)
    if (!literal || literal == INT32_MIN)
      return false;
  std::sort(output.begin(), output.end(), [](External left, External right) {
    const External a = left < 0 ? -left : left;
    const External b = right < 0 ? -right : right;
    if (a != b)
      return a < b;
    return left < right;
  });
  for (size_t i = 1; i < output.size(); ++i) {
    const External previous = output[i - 1] < 0 ? -output[i - 1]
                                                 : output[i - 1];
    const External current = output[i] < 0 ? -output[i] : output[i];
    if (previous == current)
      return false;
  }
  return true;
}

} // namespace

AdmissionResult admit(const AdmissionRequest &request,
                      AdmissionLimits limits) {
  Meter meter(limits);
  if (request.groups.empty())
    return stop(meter, AdmissionStatus::Declined,
                AdmissionReason::EmptySystem);

  // Validate and index the complete live snapshot before trusting a premise.
  std::map<uint64_t, const LiveClause *> by_occurrence;
  std::set<uint64_t> proof_identities;
  for (const LiveClause &clause : request.live_clauses) {
    if (!meter.spend())
      return metered_stop(meter);
    if (!clause.occurrence || !clause.proof || clause.literals.empty())
      return stop(meter, AdmissionStatus::InvalidInput,
                  AdmissionReason::InvalidLiveClause);
    if (!by_occurrence.emplace(clause.occurrence, &clause).second)
      return stop(meter, AdmissionStatus::InvalidInput,
                  AdmissionReason::DuplicateOccurrenceIdentity);
    if (!proof_identities.insert(clause.proof).second)
      return stop(meter, AdmissionStatus::InvalidInput,
                  AdmissionReason::DuplicateProofIdentity);
    for (External literal : clause.literals) {
      if (!meter.spend())
        return metered_stop(meter);
      if (!literal || literal == INT32_MIN)
        return stop(meter, AdmissionStatus::InvalidInput,
                    AdmissionReason::InvalidLiveClause);
    }
  }

  std::vector<const CertifiedGroup *> ordered;
  ordered.reserve(request.groups.size());
  std::set<uint64_t> group_identities;
  for (const CertifiedGroup &group : request.groups) {
    if (!meter.spend())
      return metered_stop(meter);
    if (!group.identity)
      return stop(meter, AdmissionStatus::InvalidInput,
                  AdmissionReason::InvalidStableIdentity);
    if (!group_identities.insert(group.identity).second)
      return stop(meter, AdmissionStatus::InvalidInput,
                  AdmissionReason::DuplicateGroupIdentity);
    ordered.push_back(&group);
  }
  std::sort(ordered.begin(), ordered.end(),
            [](const CertifiedGroup *left, const CertifiedGroup *right) {
              return left->identity < right->identity;
            });

  Input admitted;
  std::vector<EquationProvenance> provenance;
  std::set<uint64_t> claimed_occurrences;
  std::map<Edge, std::vector<Vertex>> endpoints;
  bool odd_charge = false;
  Vertex vertex = 0;
  for (const CertifiedGroup *group : ordered) {
    if (!meter.spend())
      return metered_stop(meter);
    const auto &equation = group->certified.equation;
    if (equation.size < 2 || equation.size > equation.variables.size())
      return stop(meter, AdmissionStatus::InvalidInput,
                  AdmissionReason::InvalidEquation);
    std::vector<Edge> edges;
    edges.reserve(equation.size);
    for (uint32_t i = 0; i < equation.size; ++i) {
      if (!meter.spend())
        return metered_stop(meter);
      const Edge edge = equation.variables[i];
      if (edge <= 0)
        return stop(meter, AdmissionStatus::InvalidInput,
                    AdmissionReason::InvalidEquation);
      if (i && edge <= equation.variables[i - 1])
        return stop(meter, AdmissionStatus::InvalidInput,
                    edge == equation.variables[i - 1]
                        ? AdmissionReason::DuplicateEdgeInEquation
                        : AdmissionReason::InvalidEquation);
      edges.push_back(edge);
    }

    const auto expected = parity_certificate::xor_equation(
        equation.variables.data(), equation.size, equation.rhs);
    if (group->certified.premises.size() != expected.size() ||
        group->premise_proofs.size() != expected.size())
      return stop(meter, AdmissionStatus::Declined,
                  AdmissionReason::PartialPremiseGroup);

    EquationProvenance source;
    source.group_identity = group->identity;
    source.vertex = ++vertex;
    for (size_t i = 0; i < expected.size(); ++i) {
      if (!meter.spend())
        return metered_stop(meter);
      const uint64_t occurrence = group->certified.premises[i];
      const uint64_t proof = group->premise_proofs[i];
      if (!occurrence || !proof)
        return stop(meter, AdmissionStatus::InvalidInput,
                    AdmissionReason::InvalidStableIdentity);
      if (!claimed_occurrences.insert(occurrence).second)
        return stop(meter, AdmissionStatus::InvalidInput,
                    AdmissionReason::PremiseReused);
      const auto found = by_occurrence.find(occurrence);
      if (found == by_occurrence.end())
        return stop(meter, AdmissionStatus::Declined,
                    AdmissionReason::PartialPremiseGroup);
      if (found->second->proof != proof)
        return stop(meter, AdmissionStatus::InvalidInput,
                    AdmissionReason::PremiseIdentityMismatch);
      std::vector<External> actual, wanted(
          expected[i].lits.begin(),
          expected[i].lits.begin() + expected[i].size);
      std::vector<External> normalized_wanted;
      if (!canonical_clause(found->second->literals, actual) ||
          !canonical_clause(wanted, normalized_wanted) ||
          actual != normalized_wanted)
        return stop(meter, AdmissionStatus::Declined,
                    AdmissionReason::PremiseClauseMismatch);
      source.premises.push_back({occurrence, proof});
    }

    admitted.equations.push_back({vertex, equation.rhs, edges});
    provenance.push_back(std::move(source));
    odd_charge ^= equation.rhs;
    for (Edge edge : edges) {
      endpoints[edge].push_back(vertex);
      admitted.occurrences.push_back(
          {edge, OccurrenceKind::VertexParity, vertex});
    }
  }

  for (const auto &[edge, incident] : endpoints) {
    if (!meter.spend())
      return metered_stop(meter);
    if (incident.size() != 2 || incident[0] == incident[1])
      return stop(meter, AdmissionStatus::Declined,
                  AdmissionReason::EdgeNotInExactlyTwoEquations);
    if (request.edge_unavailable &&
        request.edge_unavailable(request.edge_unavailable_state, edge))
      admitted.unavailable_edges.push_back(edge);
  }
  if (!odd_charge)
    return stop(meter, AdmissionStatus::Declined,
                AdmissionReason::EvenCharge);

  // Every stored nonunit clause outside the authenticated groups is residual.
  // Any touch of a graph edge invalidates this deliberately conservative
  // action domain, even when that clause is root-satisfied.  Root units are
  // authenticated separately by the live snapshot; they may fix off-cycle
  // edges and are therefore absent from this stored-clause census.
  for (const LiveClause &clause : request.live_clauses) {
    if (!meter.spend())
      return metered_stop(meter);
    if (claimed_occurrences.count(clause.occurrence))
      continue;
    for (External literal : clause.literals) {
      if (!meter.spend())
        return metered_stop(meter);
      const Edge edge = static_cast<Edge>(external_variable_of(literal));
      if (endpoints.count(edge))
        return stop(meter, AdmissionStatus::Declined,
                    AdmissionReason::ResidualEdgeTouch);
    }
  }

  std::map<Vertex, std::vector<Vertex>> adjacent;
  for (const VertexEquation &equation : admitted.equations)
    adjacent[equation.vertex];
  for (const auto &[edge, incident] : endpoints) {
    (void)edge;
    adjacent[incident[0]].push_back(incident[1]);
    adjacent[incident[1]].push_back(incident[0]);
  }
  std::set<Vertex> reached;
  std::queue<Vertex> pending;
  pending.push(1);
  reached.insert(1);
  while (!pending.empty()) {
    if (!meter.spend())
      return metered_stop(meter);
    const Vertex current = pending.front();
    pending.pop();
    for (Vertex next : adjacent[current])
      if (reached.insert(next).second)
        pending.push(next);
  }
  if (reached.size() != admitted.equations.size())
    return stop(meter, AdmissionStatus::Declined,
                AdmissionReason::Disconnected);

  admitted.occurrence_classification_complete = true;
  AdmissionResult result;
  result.status = AdmissionStatus::Admitted;
  result.reason = AdmissionReason::None;
  result.work = meter.used();
  result.input = std::move(admitted);
  result.provenance = std::move(provenance);
  return result;
}

const char *admission_reason_name(AdmissionReason reason) {
  switch (reason) {
  case AdmissionReason::None: return "none";
  case AdmissionReason::EmptySystem: return "empty-system";
  case AdmissionReason::InvalidStableIdentity: return "invalid-stable-identity";
  case AdmissionReason::DuplicateGroupIdentity: return "duplicate-group-identity";
  case AdmissionReason::InvalidEquation: return "invalid-equation";
  case AdmissionReason::DuplicateEdgeInEquation: return "duplicate-edge-in-equation";
  case AdmissionReason::PartialPremiseGroup: return "partial-premise-group";
  case AdmissionReason::InvalidLiveClause: return "invalid-live-clause";
  case AdmissionReason::DuplicateOccurrenceIdentity: return "duplicate-occurrence-identity";
  case AdmissionReason::DuplicateProofIdentity: return "duplicate-proof-identity";
  case AdmissionReason::PremiseIdentityMismatch: return "premise-identity-mismatch";
  case AdmissionReason::PremiseClauseMismatch: return "premise-clause-mismatch";
  case AdmissionReason::PremiseReused: return "premise-reused";
  case AdmissionReason::ResidualEdgeTouch: return "residual-edge-touch";
  case AdmissionReason::EdgeNotInExactlyTwoEquations: return "edge-not-in-exactly-two-equations";
  case AdmissionReason::EvenCharge: return "even-charge";
  case AdmissionReason::Disconnected: return "disconnected";
  case AdmissionReason::WorkLimit: return "work-limit";
  case AdmissionReason::Interrupted: return "interrupted";
  }
  return "unknown";
}

} // namespace tactix::tseitin_cycle
