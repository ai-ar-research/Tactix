#include "rules/semantic_extension/gf3_general_row.hpp"

#include "rules/semantic_extension/gf3_certificate_common.hpp"

#include <algorithm>
#include <climits>
#include <limits>
#include <set>
#include <utility>

namespace tactix::gf3_general_row {
namespace {

bool power3(uint32_t exponent, uint64_t &result) {
  result = 1;
  while (exponent--) {
    if (result > std::numeric_limits<uint64_t>::max() / 3) return false;
    result *= 3;
  }
  return true;
}

bool valid_limits(const Limits &limits) {
  return limits.maximum_work && limits.maximum_events &&
         limits.maximum_event_literals && limits.maximum_active_clauses &&
         limits.maximum_retained_source_references &&
         limits.maximum_row_tuples && limits.maximum_event_width &&
         limits.maximum_branch_domains && limits.maximum_row_width;
}

struct Selection {
  const gf3_gaussian::RowOutcome *outcome = nullptr;
  const gf3_gaussian::Derivation *derivation = nullptr;
  uint64_t tuples = 0;
  std::vector<uint8_t> values;
  std::vector<int32_t> clause;
};

Result select(const gf3_affine::Theory &theory,
              const gf3_gaussian_bridge::Plan &bridge,
              const Request &request, const Limits &limits,
              Selection *selection) {
  Result result;
  if (!valid_limits(limits) || request.outcome >= bridge.gaussian.outcomes.size())
    return result;
  const gf3_gaussian::RowOutcome &outcome =
      bridge.gaussian.outcomes[request.outcome];
  if (outcome.kind != gf3_gaussian::RowOutcomeKind::Derived) {
    result.status = Status::NoConsequence;
    return result;
  }
  if (!outcome.derivation ||
      outcome.derivation > bridge.gaussian.derivations.size()) {
    result.status = Status::InvalidBridge;
    return result;
  }
  const gf3_gaussian::Derivation &node =
      bridge.gaussian.derivations[outcome.derivation - 1];
  if (node.row.size() < 2 || node.rhs >= 3) {
    result.status = Status::InvalidBridge;
    return result;
  }
  if (node.row.size() > limits.maximum_row_width) {
    result.status = Status::LimitExceeded;
    result.limit = LimitKind::RowWidth;
    return result;
  }
  uint64_t tuples = 0;
  if (!power3(static_cast<uint32_t>(node.row.size()), tuples) ||
      tuples > limits.maximum_row_tuples) {
    result.status = Status::LimitExceeded;
    result.limit = LimitKind::RowTuples;
    return result;
  }
  result.consumed.row_tuples = tuples;
  if (request.tuple >= tuples) return result;

  uint64_t rest = request.tuple;
  uint8_t sum = 0;
  std::vector<uint8_t> values;
  std::vector<int32_t> clause;
  values.reserve(node.row.size());
  clause.reserve(node.row.size());
  uint32_t previous = 0;
  for (const gf3_gaussian::Term &term : node.row) {
    if (!term.variable || term.variable <= previous ||
        term.variable > theory.domains.size() ||
        (term.coefficient != 1 && term.coefficient != 2)) {
      result.status = Status::InvalidBridge;
      return result;
    }
    previous = term.variable;
    const gf3_affine::Domain &domain = theory.domains[term.variable - 1];
    if (domain.values.size() != 3) {
      result.status = Status::InvalidTheory;
      return result;
    }
    const uint8_t value = static_cast<uint8_t>(rest % 3);
    rest /= 3;
    const int32_t literal = domain.values[value];
    if (!literal || literal == INT32_MIN) {
      result.status = Status::InvalidTheory;
      return result;
    }
    values.push_back(value);
    clause.push_back(-literal);
    sum = static_cast<uint8_t>((sum + term.coefficient * value) % 3);
  }
  if (sum == node.rhs) {
    result.status = Status::NoConsequence;
    return result;
  }
  clause = gf3_certificate_common::canonical(std::move(clause));
  for (const gf3_affine::Premise &premise : theory.source) {
    const std::vector<int32_t> existing =
        gf3_certificate_common::canonical(premise.literals);
    if (std::includes(clause.begin(), clause.end(), existing.begin(),
                      existing.end())) {
      result.status = Status::Duplicate;
      return result;
    }
  }
  result.status = Status::Planned;
  if (selection)
    *selection = {&outcome, &node, tuples, std::move(values),
                  std::move(clause)};
  return result;
}

using Meter = gf3_certificate_common::Meter<Limits, Charge, Statistics,
                                              LimitKind>;
using Outcome = gf3_certificate_common::Outcome<Result>;

struct LocalProof
    : gf3_certificate_common::LocalProof<LocalProof, gf3_affine::Theory,
                                         Limits, Meter, std::set<uint32_t>> {
  using Core = gf3_certificate_common::LocalProof<
      LocalProof, gf3_affine::Theory, Limits, Meter, std::set<uint32_t>>;
  using Core::Core;

  std::vector<Event> events;

  static constexpr EventRole auxiliary_role() { return EventRole::Auxiliary; }
  static constexpr EventRole branch_role(EventRole root_role, bool root) {
    return root ? root_role : EventRole::Auxiliary;
  }

  bool active_capacity_available() const {
    return active.size() < limits.maximum_active_clauses;
  }

  void record_event(EventRole role, const std::vector<int32_t> &clause) {
    events.push_back({role, clause});
  }
};

bool collect_premises(const gf3_affine::Theory &theory,
                      const gf3_gaussian_bridge::Plan &bridge,
                      const std::vector<AuthenticatedSource> &sources,
                      LocalProof &proof) {
  if (!gf3_certificate_common::collect_authenticated_premises(
          theory, bridge, sources, proof, LimitKind::BranchDomains))
    return false;
  proof.meter.stats.branch_domains =
      static_cast<uint32_t>(proof.branch_domains.size());
  return true;
}

} // namespace

Result inspect(const gf3_affine::Theory &theory,
               const gf3_gaussian_bridge::Plan &bridge,
               const Request &request, const Limits &limits) {
  return select(theory, bridge, request, limits, nullptr);
}

Result build(const gf3_affine::Theory &theory,
             const gf3_gaussian_bridge::Plan &bridge,
             const Request &request, const Limits &limits, Charge charge) {
  if (!charge) return {};
  Selection selection;
  Result selected = select(theory, bridge, request, limits, &selection);
  if (selected.status != Status::Planned) return selected;

  gf3_gaussian_bridge::Result checked = gf3_gaussian_bridge::validate(
      theory, bridge, limits.bridge, charge);
  if (checked.status != gf3_gaussian_bridge::Status::Complete)
    return Outcome::bridge_failure(
        checked, Outcome::bridge_status(checked.status, Status::Planned));

  Meter meter(limits, charge);
  meter.stats.bridge_work = checked.consumed.work;
  meter.stats.row_tuples = selection.tuples;
  Plan plan;
  plan.request = request;
  plan.derivation = selection.outcome->derivation;
  plan.duplicate_key = {selection.derivation->row,
                        selection.derivation->rhs, request.tuple};
  plan.consequence = selection.clause;

  if (!gf3_certificate_common::retain_authenticated_sources(
          selection.derivation->provenance, bridge.authentication, meter,
          plan.sources))
    return Outcome::stopped(meter, Status::InvalidBridge);

  LocalProof proof{theory, limits, meter};
  if (!collect_premises(theory, bridge, plan.sources, proof))
    return Outcome::stopped(meter, Status::InvalidBridge);
  for (const gf3_gaussian::Term &term : selection.derivation->row)
    proof.fixed.insert(term.variable - 1);
  if (!proof.derive(selection.clause, 0, EventRole::Consequence, true))
    return Outcome::stopped(
        meter, proof.uncertifiable ? Status::Uncertifiable
                                  : Status::InvalidBridge);
  if (proof.events.empty() ||
      proof.events.back().role != EventRole::Consequence ||
      proof.events.back().clause != selection.clause)
    return Outcome::stopped(meter, Status::InvalidBridge);

  plan.events = std::move(proof.events);
  plan.projection = {{}, true, false};
  plan.statistics = meter.stats;
  Result result;
  result.status = Status::Planned;
  result.consumed = meter.stats;
  result.plan = std::move(plan);
  return result;
}

Result validate(const gf3_affine::Theory &theory,
                const gf3_gaussian_bridge::Plan &bridge,
                const Plan &recorded, const Limits &limits,
                Charge charge) {
  Result result = build(theory, bridge, recorded.request, limits,
                        std::move(charge));
  if (result.status != Status::Planned) return result;
  if (!(result.plan == recorded)) {
    result.status = Status::InvalidCertificate;
    result.plan = {};
  }
  return result;
}

} // namespace tactix::gf3_general_row
