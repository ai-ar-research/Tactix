#include "rules/lucky/tseitin_cycle_discovery.hpp"

#include "rules/lucky/parity_premises.hpp"

#include <algorithm>
#include <map>
#include <queue>
#include <set>

namespace tactix::tseitin_cycle_discovery {
namespace {

using External = parity_premises::External;

class Meter {
  const Limits &limits_;
  uint64_t used_ = 0;
  Status stopped_ = Status::Discovered;

public:
  explicit Meter(const Limits &limits) : limits_(limits) {}
  bool spend(uint64_t amount = 1) {
    if (limits_.interrupted && limits_.interrupted(limits_.interrupt_state)) {
      stopped_ = Status::Interrupted;
      return false;
    }
    if (amount > limits_.work - used_) {
      stopped_ = Status::ResourceExhausted;
      return false;
    }
    used_ += amount;
    return true;
  }
  uint64_t used() const { return used_; }
  Status stopped() const { return stopped_; }
};

Result stop(const Meter &meter, Status status, Reason reason,
            uint64_t clauses = 0, uint64_t groups = 0) {
  Result result;
  result.status = status;
  result.reason = reason;
  result.work = meter.used();
  result.clauses = clauses;
  result.complete_groups = groups;
  return result;
}

Result metered_stop(const Meter &meter, uint64_t clauses, uint64_t groups) {
  return stop(meter, meter.stopped(),
              meter.stopped() == Status::Interrupted ? Reason::Interrupted
                                                     : Reason::WorkLimit,
              clauses, groups);
}

bool valid(const Limits &limits) {
  return limits.work && limits.maximum_clauses && limits.maximum_groups &&
         limits.maximum_width >= 2 && limits.maximum_width <= 4;
}

bool valid_snapshot(const tseitin_cycle_live::Snapshot &snapshot) {
  if (snapshot.sources.size() != snapshot.clauses.size() ||
      snapshot.sources.empty())
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

Result discover(const tseitin_cycle_live::Snapshot &snapshot,
                const Limits &limits, Plan &output) {
  output = {};
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

  for (size_t i = 1; i < equations.size(); ++i) {
    if (!meter.spend())
      return metered_stop(meter, snapshot.clauses.size(), equations.size());
    if (same_scope(equations[i - 1], equations[i]) &&
        equations[i - 1].equation.rhs != equations[i].equation.rhs)
      return stop(meter, Status::Declined, Reason::AmbiguousEquation,
                  snapshot.clauses.size(), equations.size());
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

  // The cautious first front end accepts one completely authenticated XOR
  // population.  Multiple components would make selection depend on a hidden
  // opportunity policy, so they decline until the public action owns that
  // choice explicitly.
  std::vector<uint8_t> reached(groups.size(), 0);
  std::queue<size_t> pending;
  reached[0] = 1;
  pending.push(0);
  while (!pending.empty()) {
    if (!meter.spend())
      return metered_stop(meter, snapshot.clauses.size(), equations.size());
    const size_t current = pending.front();
    pending.pop();
    const auto &equation = groups[current].certified.equation;
    for (uint32_t i = 0; i < equation.size; ++i)
      for (size_t next : incident[equation.variables[i]])
        if (!reached[next]) {
          reached[next] = 1;
          pending.push(next);
        }
  }
  if (std::find(reached.begin(), reached.end(), uint8_t(0)) != reached.end())
    return stop(meter, Status::Declined, Reason::Disconnected,
                snapshot.clauses.size(), equations.size());

  bool odd_charge = false;
  std::set<External> edges;
  std::set<uint64_t> claimed;
  for (const auto &group : groups) {
    if (!meter.spend())
      return metered_stop(meter, snapshot.clauses.size(), equations.size());
    odd_charge ^= group.certified.equation.rhs;
    for (uint32_t i = 0; i < group.certified.equation.size; ++i)
      edges.insert(group.certified.equation.variables[i]);
    claimed.insert(group.certified.premises.begin(),
                   group.certified.premises.end());
  }
  for (const auto &[edge, vertices] : incident) {
    (void)edge;
    if (!meter.spend())
      return metered_stop(meter, snapshot.clauses.size(), equations.size());
    if (vertices.size() != 2 || vertices[0] == vertices[1])
      return stop(meter, Status::Declined,
                  Reason::EdgeNotInExactlyTwoEquations,
                  snapshot.clauses.size(), equations.size());
  }
  if (!odd_charge)
    return stop(meter, Status::Declined, Reason::EvenCharge,
                snapshot.clauses.size(), equations.size());

  for (const auto &clause : snapshot.clauses) {
    if (!meter.spend())
      return metered_stop(meter, snapshot.clauses.size(), equations.size());
    if (claimed.count(clause.occurrence))
      continue;
    for (External literal : clause.literals) {
      if (!meter.spend())
        return metered_stop(meter, snapshot.clauses.size(), equations.size());
      const External variable = literal < 0 ? -literal : literal;
      if (edges.count(variable))
        return stop(meter, Status::Declined, Reason::ResidualEdgeTouch,
                    snapshot.clauses.size(), equations.size());
    }
  }

  Plan plan;
  plan.ready_ = true;
  plan.snapshot_ = snapshot;
  plan.groups_ = std::move(groups);
  output = std::move(plan);
  Result result;
  result.status = Status::Discovered;
  result.reason = Reason::None;
  result.work = meter.used();
  result.clauses = snapshot.clauses.size();
  result.complete_groups = equations.size();
  result.selected_groups = output.groups().size();
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

const char *reason_name(Reason reason) {
  switch (reason) {
  case Reason::None: return "none";
  case Reason::InvalidLimits: return "invalid-limits";
  case Reason::InvalidSnapshot: return "invalid-snapshot";
  case Reason::ClauseLimit: return "clause-limit";
  case Reason::GroupLimit: return "group-limit";
  case Reason::WorkLimit: return "work-limit";
  case Reason::Interrupted: return "interrupted";
  case Reason::NoCompleteEquations: return "no-complete-equations";
  case Reason::AmbiguousEquation: return "ambiguous-equation";
  case Reason::Disconnected: return "disconnected";
  case Reason::EdgeNotInExactlyTwoEquations:
    return "edge-not-in-exactly-two-equations";
  case Reason::EvenCharge: return "even-charge";
  case Reason::ResidualEdgeTouch: return "residual-edge-touch";
  }
  return "unknown";
}

} // namespace tactix::tseitin_cycle_discovery
