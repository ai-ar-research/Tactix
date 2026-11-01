#include "rules/lucky/tseitin_cycle_discovery.hpp"

#include "kernel/types.hpp"
#include "rules/lucky/callback_work_meter.hpp"
#include "rules/lucky/parity_premises.hpp"

#include <algorithm>
#include <map>
#include <memory>
#include <queue>
#include <set>

namespace tactix::tseitin_cycle_discovery {
namespace {

using External = parity_premises::External;

using Meter = CallbackWorkMeter<Limits, Status, Status::Discovered,
                                Status::Interrupted,
                                Status::ResourceExhausted>;

Result stop(const Meter &meter, Status status, Reason reason,
            uint64_t clauses = 0, uint64_t groups = 0,
            uint64_t components = 0) {
  Result result;
  result.status = status;
  result.reason = reason;
  result.work = meter.used();
  result.clauses = clauses;
  result.complete_groups = groups;
  result.components = components;
  return result;
}

Result metered_stop(const Meter &meter, uint64_t clauses, uint64_t groups,
                    uint64_t components = 0) {
  return stop(meter, meter.stopped(),
              meter.stopped() == Status::Interrupted ? Reason::Interrupted
                                                     : Reason::WorkLimit,
              clauses, groups, components);
}

bool valid(const Limits &limits) {
  return limits.work && limits.maximum_clauses && limits.maximum_groups &&
         limits.maximum_width >= 2 && limits.maximum_width <= 4 &&
         limits.maximum_components && limits.maximum_retained_bytes;
}

bool add_bytes(uint64_t amount, uint64_t maximum, uint64_t &total) {
  if (total > maximum || amount > maximum - total) return false;
  total += amount;
  return true;
}

bool array_bytes(uint64_t count, uint64_t width, uint64_t maximum,
                 uint64_t &total) {
  if (width && count > maximum / width) return false;
  return add_bytes(count * width, maximum, total);
}

bool retained_snapshot_bytes(const tseitin_cycle_live::Snapshot &snapshot,
                             uint64_t maximum, uint64_t &bytes) {
  bytes = sizeof(snapshot);
  if (!array_bytes(snapshot.sources.size(), sizeof(snapshot.sources[0]),
                   maximum, bytes) ||
      !array_bytes(snapshot.clauses.size(), sizeof(snapshot.clauses[0]),
                   maximum, bytes) ||
      !array_bytes(snapshot.internal_to_external.size(), sizeof(Var), maximum,
                   bytes) ||
      !array_bytes(snapshot.external_to_internal.size(), sizeof(Var), maximum,
                   bytes) ||
      !array_bytes(snapshot.root_values.size(), sizeof(int8_t), maximum,
                   bytes))
    return false;
  for (const auto &source : snapshot.sources)
    if (!array_bytes(source.clause.size(), sizeof(source.clause[0]), maximum,
                     bytes))
      return false;
  for (const auto &clause : snapshot.clauses)
    if (!array_bytes(clause.literals.size(), sizeof(clause.literals[0]),
                     maximum, bytes))
      return false;
  return bytes <= maximum;
}

bool valid_snapshot(const tseitin_cycle_live::Snapshot &snapshot) {
  if (snapshot.sources.size() != snapshot.clauses.size() ||
      snapshot.root_values.size() != snapshot.internal_to_external.size())
    return false;
  std::set<uint64_t> occurrences, proofs;
  for (size_t i = 0; i < snapshot.sources.size(); ++i) {
    const auto &source = snapshot.sources[i];
    const auto &clause = snapshot.clauses[i];
    if (!source.occurrence || !source.native_proof_id ||
        source.occurrence != clause.occurrence ||
        source.native_proof_id != clause.proof ||
        source.clause != clause.literals || source.clause.empty() ||
        !occurrences.insert(source.occurrence).second ||
        !proofs.insert(source.native_proof_id).second)
      return false;
  }
  return true;
}

bool same_scope(const parity_premises::CertifiedEquation &left,
                const parity_premises::CertifiedEquation &right) {
  if (left.equation.size != right.equation.size)
    return false;
  for (uint32_t i = 0; i < left.equation.size; ++i)
    if (left.equation.variables[i] != right.equation.variables[i])
      return false;
  return true;
}

struct CapturePoll {
  tseitin_cycle::InterruptPoll first = nullptr;
  void *first_state = nullptr;
  tseitin_cycle::InterruptPoll second = nullptr;
  void *second_state = nullptr;
};

bool capture_interrupted(void *opaque) {
  auto &poll = *static_cast<CapturePoll *>(opaque);
  return (poll.first && poll.first(poll.first_state)) ||
         (poll.second && poll.second(poll.second_state));
}

} // namespace

Result enumerate_impl(const tseitin_cycle_live::Snapshot &snapshot,
                             const Limits &limits,
                             std::vector<Plan> *output,
                             ComponentSelector selector,
                             void *selector_state, Plan *selected) {
  if (output) output->clear();
  if (selected) *selected = {};
  Meter meter(limits);
  if (!valid(limits))
    return stop(meter, Status::InvalidInput, Reason::InvalidLimits);
  if (!valid_snapshot(snapshot))
    return stop(meter, Status::InvalidInput, Reason::InvalidSnapshot);
  if (snapshot.clauses.size() > limits.maximum_clauses)
    return stop(meter, Status::ResourceExhausted, Reason::ClauseLimit,
                snapshot.clauses.size());

  parity_premises::Collector collector(
      {limits.maximum_clauses, limits.maximum_groups});
  std::map<uint64_t, uint64_t> proof_by_occurrence;
  for (const auto &clause : snapshot.clauses) {
    if (!meter.spend(uint64_t(clause.literals.size()) + 1))
      return metered_stop(meter, proof_by_occurrence.size(), 0);
    if (!proof_by_occurrence.emplace(clause.occurrence, clause.proof).second)
      return stop(meter, Status::InvalidInput, Reason::InvalidSnapshot,
                  proof_by_occurrence.size());
    if (!collector.add(clause.occurrence, clause.literals.data(),
                       clause.literals.size())) {
      const bool exhausted =
          collector.status() == parity_premises::Status::Exhausted;
      return stop(meter,
                  exhausted ? Status::ResourceExhausted
                            : Status::InvalidInput,
                  exhausted ? Reason::GroupLimit : Reason::InvalidSnapshot,
                  proof_by_occurrence.size(), collector.groups_seen());
    }
  }

  std::vector<parity_premises::CertifiedEquation> equations;
  if (!collector.finish(equations))
    return stop(meter, Status::InvalidInput, Reason::InvalidSnapshot,
                snapshot.clauses.size(), collector.groups_seen());
  if (std::any_of(equations.begin(), equations.end(), [&](const auto &item) {
        return item.equation.size > limits.maximum_width &&
               item.equation.size <= 4;
      }))
    return stop(meter, Status::ResourceExhausted, Reason::GroupLimit,
                snapshot.clauses.size(), equations.size());
  equations.erase(
      std::remove_if(equations.begin(), equations.end(), [&](const auto &item) {
        return item.equation.size > limits.maximum_width;
      }),
      equations.end());
  if (equations.empty())
    return stop(meter, Status::Inapplicable, Reason::NoCompleteEquations,
                snapshot.clauses.size());
  if (equations.size() > limits.maximum_groups)
    return stop(meter, Status::ResourceExhausted, Reason::GroupLimit,
                snapshot.clauses.size(), equations.size());

  std::vector<uint8_t> ambiguous(equations.size(), 0);
  for (size_t i = 1; i < equations.size(); ++i) {
    if (!meter.spend())
      return metered_stop(meter, snapshot.clauses.size(), equations.size());
    if (same_scope(equations[i - 1], equations[i]) &&
        equations[i - 1].equation.rhs != equations[i].equation.rhs) {
      ambiguous[i - 1] = 1;
      ambiguous[i] = 1;
    }
  }

  std::vector<tseitin_cycle::CertifiedGroup> groups;
  groups.reserve(equations.size());
  std::map<External, std::vector<size_t>> incident;
  for (size_t index = 0; index < equations.size(); ++index) {
    if (!meter.spend())
      return metered_stop(meter, snapshot.clauses.size(), equations.size());
    const auto &equation = equations[index];
    tseitin_cycle::CertifiedGroup group;
    group.identity = index + 1;
    group.certified = equation;
    for (uint64_t occurrence : equation.premises) {
      if (!meter.spend())
        return metered_stop(meter, snapshot.clauses.size(), equations.size());
      const auto found = proof_by_occurrence.find(occurrence);
      if (found == proof_by_occurrence.end())
        return stop(meter, Status::InvalidInput, Reason::InvalidSnapshot,
                    snapshot.clauses.size(), equations.size());
      group.premise_proofs.push_back(found->second);
    }
    for (uint32_t i = 0; i < equation.equation.size; ++i)
      incident[equation.equation.variables[i]].push_back(index);
    groups.push_back(std::move(group));
  }

  // Connected components are themselves public opportunities.  Their order is
  // the order of the least canonical group they contain; no private ranking is
  // introduced here.
  std::vector<uint8_t> reached(groups.size(), 0);
  std::vector<Plan> local;
  uint64_t retained_bytes = 0;
  if (!retained_snapshot_bytes(snapshot, limits.maximum_retained_bytes,
                               retained_bytes))
    return stop(meter, Status::ResourceExhausted, Reason::WorkLimit,
                snapshot.clauses.size(), equations.size());
  auto shared_snapshot =
      std::make_shared<const tseitin_cycle_live::Snapshot>(snapshot);
  Reason first_decline = Reason::None;
  uint64_t legal_components = 0;
  for (size_t seed = 0; seed < groups.size(); ++seed) {
    if (reached[seed]) continue;
    std::vector<size_t> component;
    std::queue<size_t> pending;
    reached[seed] = 1;
    pending.push(seed);
    while (!pending.empty()) {
      if (!meter.spend())
        return metered_stop(meter, snapshot.clauses.size(), equations.size(),
                            legal_components);
      const size_t current = pending.front();
      pending.pop();
      component.push_back(current);
      const auto &equation = groups[current].certified.equation;
      for (uint32_t i = 0; i < equation.size; ++i)
        for (size_t next : incident[equation.variables[i]])
          if (!reached[next]) {
            reached[next] = 1;
            pending.push(next);
          }
    }
    std::sort(component.begin(), component.end());
    if (std::any_of(component.begin(), component.end(),
                    [&](size_t index) { return ambiguous[index] != 0; })) {
      if (first_decline == Reason::None)
        first_decline = Reason::AmbiguousEquation;
      continue;
    }
    bool odd_charge = false;
    std::set<External> edges;
    std::set<uint64_t> claimed;
    for (size_t index : component) {
      if (!meter.spend())
        return metered_stop(meter, snapshot.clauses.size(), equations.size(),
                            legal_components);
      const auto &group = groups[index];
      odd_charge ^= group.certified.equation.rhs;
      for (uint32_t i = 0; i < group.certified.equation.size; ++i)
        edges.insert(group.certified.equation.variables[i]);
      claimed.insert(group.certified.premises.begin(),
                     group.certified.premises.end());
    }
    bool degree_two = true;
    for (External edge : edges) {
      if (!meter.spend())
        return metered_stop(meter, snapshot.clauses.size(), equations.size(),
                            legal_components);
      const auto found = incident.find(edge);
      if (found == incident.end() || found->second.size() != 2 ||
          found->second[0] == found->second[1]) {
        degree_two = false;
        break;
      }
    }
    if (!degree_two) {
      if (first_decline == Reason::None)
        first_decline = Reason::EdgeNotInExactlyTwoEquations;
      continue;
    }
    if (!odd_charge) {
      if (first_decline == Reason::None) first_decline = Reason::EvenCharge;
      continue;
    }
    bool residual = false;
    for (const auto &clause : snapshot.clauses) {
      if (!meter.spend())
        return metered_stop(meter, snapshot.clauses.size(), equations.size(),
                            legal_components);
      if (claimed.count(clause.occurrence)) continue;
      for (External literal : clause.literals) {
        if (!meter.spend())
          return metered_stop(meter, snapshot.clauses.size(), equations.size(),
                              legal_components);
        const External variable =
            static_cast<External>(external_variable_of(literal));
        if (edges.count(variable)) {
          residual = true;
          break;
        }
      }
      if (residual) break;
    }
    if (residual) {
      if (first_decline == Reason::None)
        first_decline = Reason::ResidualEdgeTouch;
      continue;
    }
    ++legal_components;
    Plan plan;
    plan.ready_ = true;
    plan.snapshot_ = shared_snapshot;
    uint64_t component_bytes = 0;
    for (size_t index : component) {
      const auto &group = groups[index];
      uint64_t group_bytes = sizeof(group);
      if (!array_bytes(group.certified.premises.size(), sizeof(uint64_t),
                       limits.maximum_retained_bytes, group_bytes) ||
          !array_bytes(group.premise_proofs.size(), sizeof(uint64_t),
                       limits.maximum_retained_bytes, group_bytes) ||
          !add_bytes(group_bytes, limits.maximum_retained_bytes,
                     component_bytes))
        return stop(meter, Status::ResourceExhausted, Reason::WorkLimit,
                    snapshot.clauses.size(), equations.size(),
                    legal_components);
      plan.groups_.push_back(group);
    }
    if (legal_components > limits.maximum_components)
      return stop(meter, Status::ResourceExhausted, Reason::GroupLimit,
                  snapshot.clauses.size(), equations.size(),
                  legal_components);
    if (selector) {
      uint64_t peak = retained_bytes;
      if (!add_bytes(component_bytes, limits.maximum_retained_bytes, peak))
        return stop(meter, Status::ResourceExhausted, Reason::WorkLimit,
                    snapshot.clauses.size(), equations.size(),
                    legal_components);
      if (selector(plan, selector_state)) {
        *selected = std::move(plan);
        Result result;
        result.status = Status::Discovered;
        result.reason = Reason::None;
        result.work = meter.used();
        result.clauses = snapshot.clauses.size();
        result.complete_groups = equations.size();
        result.selected_groups = selected->groups().size();
        result.components = legal_components;
        result.retained_bytes = peak;
        return result;
      }
      continue;
    }
    if (!add_bytes(component_bytes, limits.maximum_retained_bytes,
                   retained_bytes))
      return stop(meter, Status::ResourceExhausted, Reason::WorkLimit,
                  snapshot.clauses.size(), equations.size(),
                  legal_components);
    local.push_back(std::move(plan));
  }
  if (local.empty())
    return stop(meter, Status::Declined,
                first_decline == Reason::None ? Reason::Disconnected
                                              : first_decline,
                snapshot.clauses.size(), equations.size());
  Result result;
  result.status = Status::Discovered;
  result.reason = Reason::None;
  result.work = meter.used();
  result.clauses = snapshot.clauses.size();
  result.complete_groups = equations.size();
  for (const Plan &plan : local) result.selected_groups += plan.groups().size();
  result.components = local.size();
  result.retained_bytes = retained_bytes;
  if (output) *output = std::move(local);
  return result;
}

Result enumerate(const tseitin_cycle_live::Snapshot &snapshot,
                 const Limits &limits, std::vector<Plan> &output) {
  return enumerate_impl(snapshot, limits, &output, nullptr, nullptr, nullptr);
}

Result select(const tseitin_cycle_live::Snapshot &snapshot,
              const Limits &limits, ComponentSelector selector,
              void *selector_state, Plan &output) {
  if (!selector) {
    output = {};
    Meter meter(limits);
    return stop(meter, Status::InvalidInput, Reason::InvalidLimits);
  }
  return enumerate_impl(snapshot, limits, nullptr, selector, selector_state,
                        &output);
}

Result discover(const tseitin_cycle_live::Snapshot &snapshot,
                const Limits &limits, Plan &output) {
  output = {};
  std::vector<Plan> components;
  Result result = enumerate(snapshot, limits, components);
  if (!result.discovered()) return result;
  if (components.size() != 1) {
    result.status = Status::Declined;
    result.reason = Reason::Disconnected;
    result.selected_groups = 0;
    return result;
  }
  output = std::move(components.front());
  return result;
}

Result capture(Kernel &kernel,
               const tseitin_cycle_live::SnapshotLimits &snapshot_limits,
               const Limits &limits, Plan &output) {
  output = {};
  CapturePoll poll{snapshot_limits.interrupted,
                   snapshot_limits.interrupt_state,
                   limits.interrupted,
                   limits.interrupt_state};
  auto bounded_snapshot = snapshot_limits;
  bounded_snapshot.interrupted = capture_interrupted;
  bounded_snapshot.interrupt_state = &poll;
  tseitin_cycle_live::Snapshot snapshot;
  const tseitin_cycle_live::Result captured =
      tseitin_cycle_live::capture(kernel, bounded_snapshot, snapshot);
  if (captured.status != tseitin_cycle_live::Status::Captured) {
    Result result;
    result.status = captured.status == tseitin_cycle_live::Status::Interrupted
                        ? Status::Interrupted
                    : captured.status ==
                              tseitin_cycle_live::Status::ResourceExhausted
                        ? Status::ResourceExhausted
                        : Status::InvalidInput;
    result.reason = captured.status == tseitin_cycle_live::Status::Interrupted
                        ? Reason::Interrupted
                    : captured.status ==
                              tseitin_cycle_live::Status::ResourceExhausted
                        ? Reason::WorkLimit
                        : Reason::InvalidSnapshot;
    result.clauses = captured.snapshot_clauses;
    return result;
  }
  return discover(snapshot, limits, output);
}

tseitin_cycle_live::Result
prepare(Kernel &kernel, const Plan &plan,
        const tseitin_cycle_live::Request &request,
        tseitin_cycle_live::Prepared &prepared) {
  if (!plan.ready()) {
    prepared = {};
    tseitin_cycle_live::Result result;
    result.status = tseitin_cycle_live::Status::Invalid;
    result.reason = tseitin_cycle_live::Reason::InvalidRequest;
    return result;
  }
  return tseitin_cycle_live::prepare(kernel, plan.snapshot(), plan.groups(),
                                     request, prepared);
}

} // namespace tactix::tseitin_cycle_discovery
