#include "rules/semantic_extension/gf3_gaussian_certificate.hpp"

#include "rules/semantic_extension/gf3_certificate_common.hpp"

#include <algorithm>
#include <climits>
#include <map>
#include <utility>

namespace tactix::gf3_gaussian_certificate {
namespace {

using Meter = gf3_certificate_common::Meter<Limits, Charge, Statistics,
                                              LimitKind>;

Result stopped(const Meter &meter, Status ordinary) {
  Result result;
  result.status = meter.interrupted
                      ? Status::Interrupted
                      : meter.limit != LimitKind::None
                            ? Status::LimitExceeded
                            : ordinary;
  result.limit = meter.limit;
  result.consumed = meter.stats;
  return result;
}

bool valid_limits(const Limits &limits) {
  return limits.maximum_work && limits.maximum_events &&
         limits.maximum_event_literals && limits.maximum_active_clauses &&
         limits.maximum_retained_source_references &&
         limits.maximum_event_width && limits.maximum_branch_domains &&
         limits.maximum_consequences;
}

Status bridge_status(gf3_gaussian_bridge::Status status) {
  switch (status) {
  case gf3_gaussian_bridge::Status::Complete: return Status::Complete;
  case gf3_gaussian_bridge::Status::InvalidTheory:
    return Status::InvalidTheory;
  case gf3_gaussian_bridge::Status::InvalidPlan:
    return Status::InvalidBridge;
  case gf3_gaussian_bridge::Status::LimitExceeded:
    return Status::LimitExceeded;
  case gf3_gaussian_bridge::Status::Interrupted:
    return Status::Interrupted;
  }
  return Status::InvalidBridge;
}

struct LocalProof
    : gf3_certificate_common::LocalProof<
          LocalProof, gf3_affine::Theory, Limits, Meter,
          std::map<uint32_t, uint8_t>> {
  using Core = gf3_certificate_common::LocalProof<
      LocalProof, gf3_affine::Theory, Limits, Meter,
      std::map<uint32_t, uint8_t>>;
  using Core::Core;

  uint32_t consequence = 0;
  std::vector<Event> events;

  static constexpr EventRole auxiliary_role() { return EventRole::Auxiliary; }
  static constexpr EventRole branch_role(EventRole root_role, bool) {
    return root_role;
  }

  bool active_capacity_available() const {
    return theory.source.size() <= limits.maximum_active_clauses &&
           meter.stats.events <
               limits.maximum_active_clauses - theory.source.size();
  }

  void record_event(EventRole role, const std::vector<int32_t> &clause) {
    events.push_back({role, consequence, clause});
  }
};

bool collect_premises(const gf3_affine::Theory &theory,
                      const gf3_gaussian_bridge::Plan &bridge,
                      const Consequence &consequence, LocalProof &proof) {
  return gf3_certificate_common::collect_authenticated_premises(
      theory, bridge, consequence.sources, proof, LimitKind::BranchDomains);
}

bool build_consequence(const gf3_affine::Theory &theory,
                       const gf3_gaussian_bridge::Plan &bridge,
                       const gf3_gaussian::RowOutcome &outcome,
                       ConsequenceKind kind, uint32_t index,
                       const Limits &limits, Meter &meter,
                       Consequence &consequence,
                       std::vector<Event> &events) {
  if (!outcome.derivation ||
      outcome.derivation > bridge.gaussian.derivations.size())
    return false;
  const gf3_gaussian::Derivation &node =
      bridge.gaussian.derivations[outcome.derivation - 1];
  consequence.kind = kind;
  consequence.derivation = outcome.derivation;
  consequence.row = node.row;
  consequence.rhs = node.rhs;
  consequence.first_event = events.size();

  for (const auto &combination : node.provenance) {
    if (!meter.spend(1) || !combination.source_id ||
        combination.source_id > bridge.authentication.size())
      return false;
    const uint64_t index = combination.source_id - 1;
    if (index > std::numeric_limits<uint32_t>::max() ||
        bridge.authentication[index].source_id != combination.source_id ||
        !meter.retain_source())
      return false;
    consequence.sources.push_back(
        {combination, static_cast<uint32_t>(index)});
  }

  LocalProof proof{theory, limits, meter};
  proof.consequence = index;
  if (!collect_premises(theory, bridge, consequence, proof)) return false;

  if (kind == ConsequenceKind::Conflict) {
    consequence.clauses.push_back({});
    if (!proof.derive({}, 0, EventRole::Conflict, true)) return false;
  } else {
    if (node.row.size() != 1 || node.row[0].variable == 0 ||
        node.row[0].variable > theory.domains.size() ||
        node.row[0].coefficient != 1 || node.rhs >= 3)
      return false;
    const uint32_t domain = node.row[0].variable - 1;
    proof.fixed[domain] = node.rhs;
    for (uint8_t value = 0; value < 3; ++value) {
      if (value == node.rhs) continue;
      std::vector<int32_t> clause = {-theory.domains[domain].values[value]};
      consequence.clauses.push_back(clause);
      proof.fixed[domain] = value;
      if (!proof.derive(std::move(clause), 0, EventRole::Consequence, true))
        return false;
    }
    proof.fixed[domain] = node.rhs;
    std::vector<int32_t> selected = {theory.domains[domain].values[node.rhs]};
    consequence.clauses.push_back(selected);
    if (!proof.derive(std::move(selected), 0, EventRole::Consequence, true))
      return false;
  }
  if (proof.uncertifiable) return false;
  consequence.event_count = proof.events.size();
  events.insert(events.end(), proof.events.begin(), proof.events.end());
  return true;
}

} // namespace

Result build(const gf3_affine::Theory &theory,
             const gf3_gaussian_bridge::Plan &bridge,
             const Limits &limits, Charge charge) {
  if (!charge || !valid_limits(limits)) return {};
  gf3_gaussian_bridge::Result checked = gf3_gaussian_bridge::validate(
      theory, bridge, limits.bridge, charge);
  if (checked.status != gf3_gaussian_bridge::Status::Complete) {
    Result result;
    result.status = bridge_status(checked.status);
    result.limit = checked.status == gf3_gaussian_bridge::Status::LimitExceeded
                       ? LimitKind::Bridge
                       : LimitKind::None;
    result.bridge_limit = checked.limit;
    result.consumed.bridge_work = checked.consumed.work;
    return result;
  }

  Meter meter(limits, charge);
  meter.stats.bridge_work = checked.consumed.work;
  if (theory.source.size() > limits.maximum_active_clauses) {
    meter.limit = LimitKind::ActiveClauses;
    return stopped(meter, Status::LimitExceeded);
  }
  Plan plan;
  for (const gf3_gaussian::RowOutcome &outcome : bridge.gaussian.outcomes) {
    if (outcome.kind == gf3_gaussian::RowOutcomeKind::Derived) {
      ++meter.stats.unsupported_outcomes;
      continue;
    }
    if (plan.consequences.size() >= limits.maximum_consequences) {
      meter.limit = LimitKind::Consequences;
      return stopped(meter, Status::LimitExceeded);
    }
    Consequence consequence;
    const ConsequenceKind kind =
        outcome.kind == gf3_gaussian::RowOutcomeKind::ZeroRowConflict
            ? ConsequenceKind::Conflict
            : ConsequenceKind::UnitEquation;
    if (!build_consequence(theory, bridge, outcome, kind,
                           static_cast<uint32_t>(plan.consequences.size()),
                           limits, meter, consequence, plan.events)) {
      return stopped(meter, Status::Uncertifiable);
    }
    ++meter.stats.supported_outcomes;
    plan.consequences.push_back(std::move(consequence));
  }
  if (plan.consequences.empty())
    return stopped(meter, Status::NoSupportedOutcome);
  plan.statistics = meter.stats;
  Result result;
  result.status = Status::Complete;
  result.consumed = meter.stats;
  result.plan = std::move(plan);
  return result;
}

Result validate(const gf3_affine::Theory &theory,
                const gf3_gaussian_bridge::Plan &bridge,
                const Plan &recorded, const Limits &limits,
                Charge charge) {
  Result result = build(theory, bridge, limits, std::move(charge));
  if (result.status != Status::Complete) return result;
  if (!(result.plan == recorded)) {
    result.status = Status::InvalidCertificate;
    result.plan = {};
  }
  return result;
}

} // namespace tactix::gf3_gaussian_certificate
