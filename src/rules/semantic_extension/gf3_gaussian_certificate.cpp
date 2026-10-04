#include "rules/semantic_extension/gf3_gaussian_certificate.hpp"

#include <algorithm>
#include <climits>
#include <limits>
#include <map>
#include <set>
#include <utility>

namespace tactix::gf3_gaussian_certificate {
namespace {

uint32_t variable(int32_t literal) {
  return static_cast<uint32_t>(literal < 0 ? -int64_t(literal) : literal);
}

bool add_checked(uint64_t first, uint64_t second, uint64_t &result) {
  if (second > std::numeric_limits<uint64_t>::max() - first) return false;
  result = first + second;
  return true;
}

std::vector<int32_t> canonical(std::vector<int32_t> clause) {
  std::sort(clause.begin(), clause.end());
  return clause;
}

class Meter {
  const Limits &limits_;
  Charge &charge_;

public:
  Statistics stats;
  LimitKind limit = LimitKind::None;
  bool interrupted = false;

  Meter(const Limits &limits, Charge &charge)
      : limits_(limits), charge_(charge) {}

  bool spend(uint64_t amount) {
    uint64_t next = 0;
    if (!add_checked(stats.work, amount, next) ||
        next > limits_.maximum_work) {
      limit = LimitKind::Work;
      return false;
    }
    if (!charge_(amount)) {
      interrupted = true;
      return false;
    }
    stats.work = next;
    return true;
  }

  bool retain_source_reference() {
    if (stats.retained_source_references >=
        limits_.maximum_retained_source_references) {
      limit = LimitKind::RetainedSourceReferences;
      return false;
    }
    // Charge the fixed-size compact record independently of the source lookup.
    if (!spend(1)) return false;
    ++stats.retained_source_references;
    return true;
  }
};

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

struct LocalProof {
  const gf3_affine::Theory &theory;
  const Limits &limits;
  Meter &meter;
  uint32_t consequence = 0;
  std::vector<std::vector<int32_t>> active;
  std::vector<uint32_t> branch_domains;
  std::map<uint32_t, uint8_t> fixed;
  std::vector<Event> events;
  bool uncertifiable = false;

  LocalProof(const gf3_affine::Theory &theory_, const Limits &limits_,
             Meter &meter_)
      : theory(theory_), limits(limits_), meter(meter_) {}

  bool available(const std::vector<int32_t> &candidate) {
    for (const std::vector<int32_t> &clause : active) {
      if (!meter.spend(uint64_t{1} + clause.size() + candidate.size()))
        return false;
      if (std::includes(candidate.begin(), candidate.end(),
                        clause.begin(), clause.end()))
        return true;
    }
    return false;
  }

  bool assign(std::map<uint32_t, int8_t> &values, int32_t literal) {
    const uint32_t atom = variable(literal);
    if (!atom || atom > theory.maximum_variable) return false;
    const int8_t value = literal > 0 ? 1 : -1;
    const auto found = values.find(atom);
    if (found != values.end() && found->second == -value) return false;
    if (found == values.end()) {
      values.emplace(atom, value);
      ++meter.stats.propagated_literals;
    }
    return true;
  }

  // Ordinary unit propagation over the exact selected source slice plus RUP
  // additions already produced for this consequence.
  bool rup(const std::vector<int32_t> &candidate) {
    ++meter.stats.rup_checks;
    std::map<uint32_t, int8_t> values;
    for (const int32_t literal : candidate) {
      if (!meter.spend(1)) return false;
      if (!assign(values, -literal)) return true;
    }
    bool changed = true;
    while (changed) {
      changed = false;
      for (const std::vector<int32_t> &clause : active) {
        if (!meter.spend(uint64_t{1} + clause.size())) return false;
        bool satisfied = false;
        uint32_t open = 0;
        int32_t last = 0;
        for (const int32_t literal : clause) {
          const auto found = values.find(variable(literal));
          const int8_t value = found == values.end() ? 0 : found->second;
          if (value && (literal > 0) == (value > 0)) {
            satisfied = true;
            break;
          }
          if (!value) {
            ++open;
            last = literal;
          }
        }
        if (satisfied) continue;
        if (!open) return true;
        if (open == 1) {
          const uint32_t atom = variable(last);
          const auto found = values.find(atom);
          const int8_t before = found == values.end() ? 0 : found->second;
          if (!assign(values, last)) return true;
          changed |= !before;
        }
      }
    }
    return false;
  }

  bool add(std::vector<int32_t> clause, EventRole role) {
    clause = canonical(std::move(clause));
    if (clause.size() > limits.maximum_event_width) {
      meter.limit = LimitKind::EventWidth;
      return false;
    }
    if (meter.stats.events >= limits.maximum_events) {
      meter.limit = LimitKind::Events;
      return false;
    }
    uint64_t literals = 0;
    if (!add_checked(meter.stats.event_literals, clause.size(), literals) ||
        literals > limits.maximum_event_literals) {
      meter.limit = LimitKind::EventLiterals;
      return false;
    }
    if (theory.source.size() > limits.maximum_active_clauses ||
        meter.stats.events >=
            limits.maximum_active_clauses - theory.source.size()) {
      meter.limit = LimitKind::ActiveClauses;
      return false;
    }
    if (!meter.spend(uint64_t{1} + clause.size())) return false;
    meter.stats.event_literals = literals;
    meter.stats.maximum_event_width = std::max<uint32_t>(
        meter.stats.maximum_event_width,
        static_cast<uint32_t>(clause.size()));
    events.push_back({role, consequence, clause});
    active.push_back(std::move(clause));
    ++meter.stats.events;
    return true;
  }

  bool derive(std::vector<int32_t> clause, size_t branch,
              EventRole root_role, bool root) {
    clause = canonical(std::move(clause));
    if (clause.size() > limits.maximum_event_width) {
      meter.limit = LimitKind::EventWidth;
      return false;
    }
    if (available(clause)) return meter.limit == LimitKind::None &&
                                  !meter.interrupted;
    if (meter.limit != LimitKind::None || meter.interrupted) return false;
    if (rup(clause))
      return add(std::move(clause), root ? root_role : EventRole::Auxiliary);
    if (meter.limit != LimitKind::None || meter.interrupted) return false;

    while (branch < branch_domains.size() &&
           fixed.count(branch_domains[branch]))
      ++branch;
    if (branch == branch_domains.size()) {
      uncertifiable = true;
      return false;
    }
    const uint32_t domain = branch_domains[branch];
    if (domain >= theory.domains.size()) {
      uncertifiable = true;
      return false;
    }
    for (uint8_t value = 0; value < 3; ++value) {
      std::vector<int32_t> child = clause;
      child.push_back(-theory.domains[domain].values[value]);
      if (!derive(std::move(child), branch + 1, root_role, false))
        return false;
    }
    if (!rup(clause)) {
      if (meter.limit == LimitKind::None && !meter.interrupted)
        uncertifiable = true;
      return false;
    }
    return add(std::move(clause), root_role);
  }
};

bool collect_premises(const gf3_affine::Theory &theory,
                      const gf3_gaussian_bridge::Plan &bridge,
                      const Consequence &consequence, LocalProof &proof) {
  std::set<uint64_t> occurrences;
  const auto take = [&](const gf3_affine::SourceReference &reference) {
    if (!reference.occurrence) return false;
    const gf3_affine::Premise *premise = nullptr;
    for (const auto &candidate : theory.source) {
      if (!proof.meter.spend(1)) return false;
      if (candidate.occurrence == reference.occurrence) {
        premise = &candidate;
        break;
      }
    }
    if (!premise || premise->native_proof_id != reference.native_proof_id)
      return false;
    if (!occurrences.insert(reference.occurrence).second) return true;
    if (proof.active.size() >= proof.limits.maximum_active_clauses) {
      proof.meter.limit = LimitKind::ActiveClauses;
      return false;
    }
    if (!proof.meter.spend(uint64_t{1} + premise->literals.size()))
      return false;
    proof.active.push_back(canonical(premise->literals));
    return true;
  };

  std::set<uint32_t> domains;
  for (const AuthenticatedSource &source : consequence.sources) {
    if (source.authentication_index >= bridge.authentication.size())
      return false;
    const gf3_gaussian_bridge::SourceAuthentication &authentication =
        bridge.authentication[source.authentication_index];
    if (authentication.source_id != source.combination.source_id)
      return false;
    for (const auto &domain : authentication.domains) {
      if (!take(domain.at_least_one)) return false;
      for (const auto &amo : domain.at_most_one)
        if (!take(amo)) return false;
      domains.insert(domain.domain);
    }
    for (const auto &tuple : authentication.forbidden_tuples)
      if (!take(tuple.premise)) return false;
  }
  if (domains.size() > proof.limits.maximum_branch_domains) {
    proof.meter.limit = LimitKind::BranchDomains;
    return false;
  }
  proof.branch_domains.assign(domains.begin(), domains.end());
  return true;
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
        !meter.retain_source_reference())
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
