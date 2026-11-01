#include "rules/lucky/tseitin_cycle_action.hpp"

#include "rules/structural_action_guard.hpp"

#include "proof/proof.hpp"
#include "strategy/action_status_mapping.hpp"
#include "strategy/action_status_statistics.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <initializer_list>
#include <limits>
#include <map>
#include <sstream>
#include <string_view>

namespace tactix {

struct TseitinCycleBreakingSessionAccess {
  static void commit(TseitinCycleBreakingSession &session,
                     const Kernel &kernel, std::vector<std::string> keys) {
    session.owner_ = &kernel;
    session.keys_.swap(keys);
  }
};

namespace {

struct Offered {
  tseitin_cycle_discovery::Plan plan;
  tseitin_cycle::ProofTransaction transaction;
  std::string component;
  std::string identity;
  std::string duplicate_key;
};

using tseitin_cycle_live::KernelPoll;
using tseitin_cycle_live::kernel_poll;
using tseitin_cycle_live::with_kernel_poll;

bool add(uint64_t amount, uint64_t maximum, uint64_t &value) {
  if (value > maximum || amount > maximum - value) return false;
  value += amount;
  return true;
}

// Identity, session-key and journal work share one meter: spend() polls for an
// interrupt before charging; charge() only adds to the bounded work total.
struct WorkMeter {
  uint64_t maximum = UINT64_MAX;
  uint64_t &work;
  KernelPoll *poll = nullptr;
  bool exhausted = false;
  bool interrupted = false;
  bool charge(uint64_t amount) {
    if (add(amount, maximum, work)) return true;
    exhausted = true;
    return false;
  }
  bool spend(uint64_t amount) {
    if (!kernel_poll(poll)) return charge(amount);
    interrupted = true;
    return false;
  }
  // The status of a failed metered step: `otherwise` when the meter itself
  // neither observed an interrupt nor ran out of budget.
  TseitinCycleBreakingStatus
  failure(TseitinCycleBreakingStatus otherwise) const {
    return interrupted ? TseitinCycleBreakingStatus::Interrupted
           : exhausted ? TseitinCycleBreakingStatus::ResourceExhausted
                       : otherwise;
  }
};

class Encoder {
  std::string bytes_;
  uint64_t maximum_ = 0;
  WorkMeter &meter_;

public:
  Encoder(uint64_t maximum, WorkMeter &meter)
      : maximum_(maximum), meter_(meter) {}
  bool spend(uint64_t amount) { return meter_.spend(amount); }
  bool u64(uint64_t value) {
    if (!spend(8)) return false;
    if (bytes_.size() > maximum_ || 8 > maximum_ - bytes_.size())
      return false;
    for (unsigned shift = 0; shift != 64; shift += 8)
      bytes_.push_back(static_cast<char>((value >> shift) & 0xff));
    return true;
  }
  bool i32(int32_t value) { return u64(static_cast<uint32_t>(value)); }
  bool i32s(const std::vector<int32_t> &values) {
    for (int32_t value : values)
      if (!i32(value)) return false;
    return true;
  }
  const std::string &bytes() const { return bytes_; }
};

// The planned proof accounting in identity and journal order; without
// `emits` only the committed solver clauses are emitted.
std::array<uint64_t, 6> accounting_fields(
    const tseitin_cycle::ProofTransaction &transaction, bool emits) {
  const auto &planned = transaction.accounting;
  return {emits ? planned.preflight_additions : 0,
          emits ? planned.preflight_deletions : 0,
          emits ? planned.temporary_helpers : 0,
          emits ? planned.proof_only_variables : 0,
          planned.committed_solver_clauses,
          emits ? planned.witness_literals : 0};
}

std::string hex(const char *prefix, const std::string &bytes) {
  static constexpr char digits[] = "0123456789abcdef";
  std::string result = prefix;
  result.reserve(result.size() + 2 * bytes.size());
  for (unsigned char byte : bytes) {
    result.push_back(digits[byte >> 4]);
    result.push_back(digits[byte & 15]);
  }
  return result;
}

bool parse_identity(const std::string &identity, WorkMeter &meter,
                    size_t &split) {
  if (!meter.spend(5) ||
      identity.size() <= 7 || identity.compare(0, 5, "tcx2:"))
    return false;
  split = std::string::npos;
  for (size_t index = 5; index < identity.size(); ++index) {
    if (!meter.spend(1)) return false;
    const char ch = identity[index];
    if (ch == ':') {
      if (split != std::string::npos) return false;
      split = index;
    } else if (!((ch >= '0' && ch <= '9') ||
                 (ch >= 'a' && ch <= 'f'))) {
      return false;
    }
  }
  return split != std::string::npos && split != 5 &&
         split + 1 != identity.size() && (split - 5) % 2 == 0 &&
         (identity.size() - split - 1) % 2 == 0;
}

bool identity_equal(std::string_view left, std::string_view right,
                    WorkMeter &meter) {
  if (!meter.spend(1) || left.size() != right.size()) return false;
  for (size_t index = 0; index < left.size(); ++index) {
    if (!meter.spend(1) || left[index] != right[index])
      return false;
  }
  return true;
}

// Charges the copy plus a quadratic sort bound for one premise clause, then
// returns its literals in ascending order.
bool sorted_literals(const tseitin_cycle::LiveClause &clause, WorkMeter &meter,
                     std::vector<int32_t> &literals) {
  const size_t size = clause.literals.size();
  const uint64_t copied = static_cast<uint64_t>(size);
  if (static_cast<size_t>(copied) != size ||
      (copied && copied > UINT64_MAX / copied))
    return false;
  const uint64_t sorted = copied * copied;
  if (copied > UINT64_MAX - sorted || !meter.spend(copied + sorted))
    return false;
  literals = clause.literals;
  std::sort(literals.begin(), literals.end());
  return true;
}

// One-based position of a stable group identity in the plan; zero if absent.
uint64_t local_group(const tseitin_cycle_discovery::Plan &plan,
                     uint64_t identity) {
  const auto &groups = plan.groups();
  const auto found = std::find_if(
      groups.begin(), groups.end(),
      [&](const auto &group) { return group.identity == identity; });
  return found == groups.end() ? 0 : std::distance(groups.begin(), found) + 1;
}

const tseitin_cycle::LiveClause *clause_for(
    const tseitin_cycle_live::Snapshot &snapshot, uint64_t occurrence,
    uint64_t proof) {
  const auto found = std::find_if(
      snapshot.clauses.begin(), snapshot.clauses.end(),
      [&](const auto &clause) {
        return clause.occurrence == occurrence && clause.proof == proof;
      });
  return found == snapshot.clauses.end() ? nullptr : &*found;
}

bool encode_component(const tseitin_cycle_discovery::Plan &plan,
                      uint64_t maximum_bytes, std::string &component,
                      WorkMeter &meter) {
  Encoder bytes(maximum_bytes, meter);
  const auto &groups = plan.groups();
  if (!bytes.u64(3) || !bytes.u64(groups.size())) return false;
  bool charge = false;
  std::map<int32_t, std::vector<uint64_t>> incidence;
  for (size_t local = 0; local < groups.size(); ++local) {
    const auto &group = groups[local];
    if (!bytes.u64(local + 1) ||
        !bytes.u64(group.certified.equation.size) ||
        !bytes.u64(group.certified.equation.rhs))
      return false;
    charge ^= group.certified.equation.rhs;
    for (uint32_t i = 0; i < group.certified.equation.size; ++i) {
      const int32_t edge = group.certified.equation.variables[i];
      if (!bytes.i32(edge)) return false;
      incidence[edge].push_back(local + 1);
    }
    if (!bytes.u64(group.certified.premises.size())) return false;
    for (size_t i = 0; i < group.certified.premises.size(); ++i) {
      const uint64_t occurrence = group.certified.premises[i];
      const uint64_t proof = group.premise_proofs[i];
      if (!bytes.spend(plan.snapshot().clauses.size())) return false;
      const auto *clause = clause_for(plan.snapshot(), occurrence, proof);
      std::vector<int32_t> literals;
      if (!clause || !sorted_literals(*clause, meter, literals)) return false;
      if (!bytes.u64(occurrence) || !bytes.u64(proof) ||
          !bytes.u64(clause->literals.size()) || !bytes.i32s(literals))
        return false;
    }
  }
  if (!bytes.u64(charge) || !bytes.u64(incidence.size())) return false;
  for (const auto &[edge, vertices] : incidence) {
    if (vertices.size() != 2) return false;
    if (!bytes.i32(edge) || !bytes.u64(vertices[0]) ||
        !bytes.u64(vertices[1]))
      return false;
  }
  uint64_t assigned = 0;
  if (!bytes.spend(plan.snapshot().root_values.size())) return false;
  for (const int8_t value : plan.snapshot().root_values)
    assigned += value != 0;
  if (!bytes.u64(assigned)) return false;
  for (Var internal = 0; internal < plan.snapshot().root_values.size();
       ++internal) {
    const int8_t value = plan.snapshot().root_values[internal];
    if (!value) continue;
    if (!bytes.u64(plan.snapshot().internal_to_external[internal]) ||
        !bytes.i32(value))
      return false;
  }
  if (!bytes.spend(2 * bytes.bytes().size())) return false;
  component = hex("", bytes.bytes());
  return true;
}

bool encode_transaction(const tseitin_cycle_discovery::Plan &plan,
                        const tseitin_cycle::ProofTransaction &transaction,
                        uint64_t maximum_bytes, std::string &prepared,
                        WorkMeter &meter) {
  Encoder bytes(maximum_bytes, meter);
  if (!bytes.u64(transaction.version) ||
      !bytes.u64(transaction.requires_unassigned_pivot) ||
      !bytes.u64(transaction.requires_live_x7_validation) ||
      !bytes.u64(transaction.requires_post_apply_replan) ||
      !bytes.i32(transaction.identity.pivot) ||
      !bytes.u64(transaction.identity.cycle_edges.size()) ||
      !bytes.i32s(transaction.identity.cycle_edges) ||
      !bytes.u64(transaction.identity.sources.size()))
    return false;
  for (const auto &source : transaction.identity.sources) {
    if (!bytes.spend(plan.groups().size())) return false;
    const uint64_t local = local_group(plan, source.group_identity);
    if (!local || !bytes.u64(local) || !bytes.u64(source.vertex) ||
        !bytes.u64(source.premises.size()))
      return false;
    for (const auto &premise : source.premises)
      if (!bytes.u64(premise.occurrence) || !bytes.u64(premise.proof))
        return false;
  }
  if (!bytes.u64(transaction.x7.clause.size()) ||
      !bytes.i32s(transaction.x7.clause) ||
      !bytes.u64(transaction.x7.witness_true.size()) ||
      !bytes.i32s(transaction.x7.witness_true) ||
      !bytes.u64(transaction.x7.substitution.size()))
    return false;
  for (const auto &[source, target] : transaction.x7.substitution) {
    if (!bytes.i32(source) || !bytes.i32(target)) return false;
  }
  for (uint64_t value : accounting_fields(transaction, true))
    if (!bytes.u64(value)) return false;
  if (!bytes.u64(transaction.events.size())) return false;
  for (auto event : transaction.events)
    if (!bytes.u64(static_cast<uint8_t>(event))) return false;
  if (!bytes.spend(2 * bytes.bytes().size())) return false;
  prepared = hex("", bytes.bytes());
  return true;
}

bool encode_identity(const tseitin_cycle_discovery::Plan &plan,
                     const tseitin_cycle::ProofTransaction &transaction,
                     uint64_t maximum, std::string &component,
                     std::string &identity, WorkMeter &meter) {
  if (maximum <= 6) return false;
  const uint64_t raw_limit = (maximum - 6) / 2;
  std::string prepared;
  if (!encode_component(plan, raw_limit, component, meter) ||
      !encode_transaction(plan, transaction, raw_limit, prepared, meter) ||
      component.size() > maximum - 6 ||
      prepared.size() > maximum - 6 - component.size())
    return false;
  if (!meter.spend(6 + component.size() + prepared.size())) return false;
  identity = "tcx2:" + component + ':' + prepared;
  return identity.size() <= maximum;
}

bool encode_duplicate_key(const tseitin_cycle_discovery::Plan &plan,
                          const tseitin_cycle::ProofTransaction &transaction,
                          uint64_t maximum, std::string &key,
                          WorkMeter &meter) {
  if (maximum <= 5) return false;
  Encoder bytes((maximum - 5) / 2, meter);
  if (!bytes.u64(1) || !bytes.u64(plan.groups().size())) return false;
  for (const auto &group : plan.groups()) {
    if (!bytes.u64(group.certified.equation.size) ||
        !bytes.u64(group.certified.equation.rhs) ||
        !bytes.u64(group.certified.premises.size()))
      return false;
    for (uint32_t i = 0; i < group.certified.equation.size; ++i)
      if (!bytes.i32(group.certified.equation.variables[i])) return false;
    for (size_t i = 0; i < group.certified.premises.size(); ++i) {
      if (!bytes.spend(plan.snapshot().clauses.size())) return false;
      const auto *clause = clause_for(plan.snapshot(),
                                      group.certified.premises[i],
                                      group.premise_proofs[i]);
      std::vector<int32_t> literals;
      if (!clause || !bytes.u64(clause->literals.size()) ||
          !sorted_literals(*clause, meter, literals) || !bytes.i32s(literals))
        return false;
    }
  }
  if (!bytes.i32(-transaction.identity.pivot)) return false;
  if (!bytes.spend(2 * bytes.bytes().size())) return false;
  if (!bytes.spend(5)) return false;
  key = hex("tcd1:", bytes.bytes());
  return key.size() <= maximum;
}

tseitin_cycle_live::Request live_request(
    const TseitinCycleBreakingLimits &limits) {
  return {limits.snapshot, limits.admission, limits.planning,
          limits.transaction, limits.maximum_validation_ticks};
}

TseitinCycleBreakingStatus map(tseitin_cycle_live::Status status) {
  using S = tseitin_cycle_live::Status;
  if (const auto failure =
          strategy::map_standard_action_failure<TseitinCycleBreakingStatus>(
              status))
    return *failure;
  switch (status) {
  case S::Prepared:
  case S::Captured:
  case S::Applied: return TseitinCycleBreakingStatus::Applied;
  case S::Declined: return TseitinCycleBreakingStatus::NoStructure;
  default: std::abort();
  }
}

TseitinCycleBreakingStatus map(tseitin_cycle_discovery::Status status) {
  using S = tseitin_cycle_discovery::Status;
  switch (status) {
  case S::Discovered: return TseitinCycleBreakingStatus::Applied;
  case S::Inapplicable:
  case S::Declined: return TseitinCycleBreakingStatus::NoStructure;
  case S::ResourceExhausted:
    return TseitinCycleBreakingStatus::ResourceExhausted;
  case S::Interrupted: return TseitinCycleBreakingStatus::Interrupted;
  case S::InvalidInput: return TseitinCycleBreakingStatus::Invalid;
  }
  std::abort();
}

TseitinCycleBreakingStatus capture_components(
    Kernel &kernel, const TseitinCycleBreakingLimits &limits,
    uint64_t maximum_components,
    std::vector<tseitin_cycle_discovery::Plan> &plans,
    TseitinCycleBreakingEffort &effort) {
  plans.clear();
  tseitin_cycle_live::Snapshot snapshot;
  const auto captured =
      tseitin_cycle_live::capture(kernel, limits.snapshot, snapshot);
  effort.snapshot_clauses = captured.snapshot_clauses;
  effort.snapshot_literals = captured.snapshot_literals;
  effort.snapshot_work = captured.snapshot_work;
  if (captured.status != tseitin_cycle_live::Status::Captured)
    return map(captured.status);
  KernelPoll poll;
  auto bounded_discovery = with_kernel_poll(kernel, limits.discovery, poll);
  bounded_discovery.maximum_components = maximum_components;
  const auto discovered = tseitin_cycle_discovery::enumerate(
      snapshot, bounded_discovery, plans);
  effort.discovery_work = discovered.work;
  effort.retained_bytes = discovered.retained_bytes;
  effort.candidates = discovered.components;
  if (!discovered.discovered()) return map(discovered.status);
  return TseitinCycleBreakingStatus::Applied;
}

struct ComponentMatch {
  std::string_view target;
  uint64_t raw_limit = 0;
  WorkMeter meter;
};

bool matches_component(const tseitin_cycle_discovery::Plan &plan,
                       void *opaque) {
  auto &match = *static_cast<ComponentMatch *>(opaque);
  std::string encoded;
  // An unrelated component that exceeds the selected payload bound is a
  // definite non-match, not a failure of selected-action authentication.
  if (!encode_component(plan, match.raw_limit, encoded, match.meter))
    return false;
  return identity_equal(encoded, match.target, match.meter);
}

TseitinCycleBreakingStatus select_component(
    Kernel &kernel, const TseitinCycleBreakingLimits &limits,
    std::string_view target, tseitin_cycle_discovery::Plan &selected,
    TseitinCycleBreakingEffort &effort) {
  tseitin_cycle_live::Snapshot snapshot;
  const auto captured =
      tseitin_cycle_live::capture(kernel, limits.snapshot, snapshot);
  effort.snapshot_clauses = captured.snapshot_clauses;
  effort.snapshot_literals = captured.snapshot_literals;
  effort.snapshot_work = captured.snapshot_work;
  if (captured.status != tseitin_cycle_live::Status::Captured)
    return map(captured.status);
  KernelPoll poll;
  auto bounded_discovery = with_kernel_poll(kernel, limits.discovery, poll);
  bounded_discovery.maximum_components = bounded_discovery.maximum_groups;
  ComponentMatch match{target, (limits.maximum_identity_bytes - 6) / 2,
                       {limits.maximum_identity_work, effort.identity_work,
                        &poll}};
  const auto discovered = tseitin_cycle_discovery::select(
      snapshot, bounded_discovery, matches_component, &match, selected);
  effort.discovery_work = discovered.work;
  effort.retained_bytes = discovered.retained_bytes;
  effort.candidates = discovered.components;
  if (match.meter.interrupted) return TseitinCycleBreakingStatus::Interrupted;
  if (match.meter.exhausted) return TseitinCycleBreakingStatus::ResourceExhausted;
  if (discovered.discovered()) return TseitinCycleBreakingStatus::Applied;
  const auto status = map(discovered.status);
  return status == TseitinCycleBreakingStatus::NoStructure
             ? TseitinCycleBreakingStatus::Stale
             : status;
}

// Adds one live preparation's effort, or with `replay` one application's
// replayed stage work; false on counter overflow.
bool add_live_effort(const tseitin_cycle_live::Result &live, bool replay,
                     TseitinCycleBreakingEffort &effort) {
  return add(live.snapshot_work, UINT64_MAX, effort.recapture_work) &&
         add(replay ? live.replay_admission_work : live.admission_work,
             UINT64_MAX, effort.admission_work) &&
         add(replay ? live.replay_planning_work : live.planning_work,
             UINT64_MAX, effort.planning_work) &&
         add(replay ? live.replay_transaction_work : live.transaction_work,
             UINT64_MAX, effort.transaction_work) &&
         add(live.validation_ticks, UINT64_MAX, effort.validation_ticks);
}

TseitinCycleBreakingStatus enumerate_offers(
    Kernel &kernel, const TseitinCycleBreakingLimits &limits,
    KernelPoll &action_poll, std::vector<Offered> &offered,
    TseitinCycleBreakingEffort &effort) {
  offered.clear();
  std::vector<tseitin_cycle_discovery::Plan> plans;
  const auto captured = capture_components(kernel, limits,
                                           limits.maximum_components, plans,
                                           effort);
  if (captured != TseitinCycleBreakingStatus::Applied) return captured;
  uint64_t total_identity = 0;
  std::vector<Offered> local;
  local.reserve(plans.size());
  for (auto &plan : plans) {
    Offered item;
    item.plan = std::move(plan);
    tseitin_cycle_live::Prepared prepared;
    const auto response = tseitin_cycle_discovery::prepare(
        kernel, item.plan, live_request(limits), prepared);
    if (!add_live_effort(response, false, effort))
      return TseitinCycleBreakingStatus::ResourceExhausted;
    if (response.status == tseitin_cycle_live::Status::Stale &&
        response.reason == tseitin_cycle_live::Reason::AssignedPivot)
      continue;
    if (response.status == tseitin_cycle_live::Status::Declined &&
        response.reason == tseitin_cycle_live::Reason::PlanningDeclined &&
        response.planning_reason == tseitin_cycle::DeclineReason::Acyclic)
      continue;
    if (response.status != tseitin_cycle_live::Status::Prepared)
      return map(response.status);
    item.transaction = prepared.transaction();
    WorkMeter meter{limits.maximum_identity_work, effort.identity_work,
                        &action_poll};
    if (!encode_identity(item.plan, item.transaction,
                         limits.maximum_identity_bytes, item.component,
                         item.identity, meter) ||
        !encode_duplicate_key(item.plan, item.transaction,
                              limits.maximum_identity_bytes,
                              item.duplicate_key, meter) ||
        !add(item.identity.size(), limits.maximum_total_identity_bytes,
             total_identity))
      return meter.failure(TseitinCycleBreakingStatus::ResourceExhausted);
    local.push_back(std::move(item));
  }
  effort.identity_bytes = total_identity;
  offered = std::move(local);
  if (offered.empty()) return TseitinCycleBreakingStatus::NoStructure;
  return TseitinCycleBreakingStatus::Applied;
}

bool append(std::string &journal, uint64_t maximum,
            const std::string &line) {
  if (journal.size() > maximum || line.size() > maximum - journal.size())
    return false;
  journal += line;
  return true;
}

bool find_session_key(const std::vector<std::string> &keys,
                      const std::string &target, WorkMeter &meter,
                      size_t &position, bool &found) {
  position = 0;
  found = false;
  for (; position < keys.size(); ++position) {
    const std::string &key = keys[position];
    const size_t common = std::min(key.size(), target.size());
    size_t index = 0;
    for (; index < common; ++index) {
      if (!meter.spend(1)) return false;
      if (key[index] != target[index]) break;
    }
    if (index < common) {
      if (key[index] > target[index]) return true;
      continue;
    }
    if (!meter.charge(1)) return false;
    if (key.size() == target.size()) {
      found = true;
      return true;
    }
    if (key.size() > target.size()) return true;
  }
  return true;
}

// Locates `key` among the ordered session keys and, when it is new, admits it
// against the key-count, session-work and `identity_bytes` bounds. Applied
// leaves its insertion point in `position`; AlreadyApplied names a known key.
TseitinCycleBreakingStatus admit_session_key(
    const TseitinCycleBreakingSession &session,
    const TseitinCycleBreakingLimits &limits, const std::string &key,
    KernelPoll &poll, uint64_t &session_work, uint64_t &identity_bytes,
    size_t &position) {
  bool duplicate = false;
  WorkMeter meter{limits.maximum_session_work, session_work, &poll};
  if (!find_session_key(session.keys(), key, meter, position, duplicate))
    return meter.failure(TseitinCycleBreakingStatus::ResourceExhausted);
  if (duplicate) return TseitinCycleBreakingStatus::AlreadyApplied;
  if (session.keys().size() == limits.maximum_session_keys ||
      !add(key.size(), limits.maximum_session_work, session_work) ||
      !add(key.size(), limits.maximum_session_identity_bytes, identity_bytes))
    return TseitinCycleBreakingStatus::ResourceExhausted;
  return TseitinCycleBreakingStatus::Applied;
}

bool make_journal(const Kernel &kernel, uint64_t action_id,
                  const TseitinCycleBreakingLimits &limits,
                  const Offered &offered, uint64_t maximum,
                  std::string &journal, WorkMeter &meter) {
  const auto &transaction = offered.transaction;
  const auto put = [&](const std::string &text) {
    return meter.spend(text.size()) && append(journal, maximum, text);
  };
  const auto number = [&](uint64_t value) { return put(std::to_string(value)); };
  const auto integer = [&](int64_t value) { return put(std::to_string(value)); };
  // Comma-separated `item(value)` for every element of `values`.
  const auto list = [&](const auto &values, const auto &item) {
    for (size_t i = 0; i < values.size(); ++i)
      if ((i && !put(",")) || !item(values[i])) return false;
    return true;
  };
  // `[first,second]`, each element written by `write`.
  const auto pair_of = [&](const auto &write, auto first, auto second) {
    return put("[") && write(first) && put(",") && write(second) && put("]");
  };
  // `"name":value` members, comma-led unless the first of the object.
  const auto bounds =
      [&](bool first,
          std::initializer_list<std::pair<const char *, uint64_t>> members) {
        for (const auto &[name, value] : members) {
          if (!put(std::string(first ? "\"" : ",\"") + name + "\":") ||
              !number(value))
            return false;
          first = false;
        }
        return true;
      };
  if (!put("{\"action\":\"TseitinCycleBreaking\",\"version\":2,") ||
      !put("\"action_id\":") || !number(action_id) ||
      !put(",\"semantic_id\":") || !number(action_id) ||
      !put(",\"identity\":\"") || !put(offered.identity) ||
      !put("\",\"proof_format\":\"") ||
      !put(kernel.proof ? (kernel.proof->streaming_pbp()
                               ? "pbp"
                               : kernel.proof->binary() ? "dsr-binary"
                                                        : "dsr-ascii")
                        : "none") ||
      !put("\",\"parameters\":{") ||
      !bounds(true,
              {{"snapshot_clauses", limits.snapshot.maximum_clauses},
               {"snapshot_literals", limits.snapshot.maximum_literals},
               {"snapshot_work", limits.snapshot.maximum_work},
               {"snapshot_variables", limits.snapshot.maximum_variables},
               {"discovery_work", limits.discovery.work},
               {"discovery_clauses", limits.discovery.maximum_clauses},
               {"discovery_groups", limits.discovery.maximum_groups},
               {"discovery_width", limits.discovery.maximum_width},
               {"discovery_profile_components",
                limits.discovery.maximum_components},
               {"effective_component_cap", limits.discovery.maximum_groups}}) ||
      !put(",\"enumeration_mode\":\"selected\"") ||
      !bounds(false,
              {{"discovery_retained_bytes",
                limits.discovery.maximum_retained_bytes},
               {"admission_work", limits.admission.work},
               {"planning_work", limits.planning.work},
               {"planning_witness_length", limits.planning.witness_length},
               {"planning_additions", limits.planning.additions},
               {"transaction_work", limits.transaction.work},
               {"transaction_additions", limits.transaction.additions},
               {"transaction_deletions", limits.transaction.deletions},
               {"transaction_helpers", limits.transaction.helpers},
               {"transaction_witness_literals",
                limits.transaction.witness_literals},
               {"validation_ticks", limits.maximum_validation_ticks},
               {"maximum_components", limits.maximum_components},
               {"maximum_identity_bytes", limits.maximum_identity_bytes},
               {"maximum_total_identity_bytes",
                limits.maximum_total_identity_bytes},
               {"maximum_identity_work", limits.maximum_identity_work},
               {"maximum_journal_bytes", limits.maximum_journal_bytes},
               {"maximum_session_keys", limits.maximum_session_keys},
               {"maximum_session_work", limits.maximum_session_work},
               {"maximum_session_identity_bytes",
                limits.maximum_session_identity_bytes}}) ||
      !put("},\"semantic_key\":\"") || !put(offered.duplicate_key) ||
      !put("\",\"component\":{") ||
      !put("\"charge\":1,\"groups\":["))
    return false;
  std::map<int32_t, std::vector<uint64_t>> incidence;
  for (size_t local = 0; local < offered.plan.groups().size(); ++local) {
    const auto &group = offered.plan.groups()[local];
    if (local && !put(",")) return false;
    if (!put("{\"local\":") || !number(local + 1) ||
        !put(",\"rhs\":") || !number(group.certified.equation.rhs) ||
        !put(",\"edges\":["))
      return false;
    for (uint32_t i = 0; i < group.certified.equation.size; ++i) {
      if (i && !put(",")) return false;
      const int32_t edge = group.certified.equation.variables[i];
      if (!integer(edge)) return false;
      incidence[edge].push_back(local + 1);
    }
    if (!put("],\"sources\":[")) return false;
    for (size_t i = 0; i < group.certified.premises.size(); ++i) {
      if (i && !put(",")) return false;
      const uint64_t occurrence = group.certified.premises[i];
      const uint64_t proof = group.premise_proofs[i];
      const auto *clause = clause_for(offered.plan.snapshot(), occurrence,
                                      proof);
      std::vector<int32_t> literals;
      if (!clause || !sorted_literals(*clause, meter, literals) ||
          !put("{\"occurrence\":") || !number(occurrence) ||
          !put(",\"proof_id\":") || !number(proof) ||
          !put(",\"clause\":[") || !list(literals, integer) || !put("]}"))
        return false;
    }
    if (!put("]}")) return false;
  }
  if (!put("],\"incidence\":[")) return false;
  size_t incidence_index = 0;
  for (const auto &[edge, vertices] : incidence) {
    if (vertices.size() != 2) return false;
    if (incidence_index++ && !put(",")) return false;
    if (!put("{\"edge\":") || !integer(edge) ||
        !put(",\"groups\":[") || !number(vertices[0]) || !put(",") ||
        !number(vertices[1]) || !put("]}"))
      return false;
  }
  if (!put("]},\"transaction\":{\"version\":") ||
      !number(transaction.version) || !put(",\"flags\":[") ||
      !number(transaction.requires_unassigned_pivot) || !put(",") ||
      !number(transaction.requires_live_x7_validation) || !put(",") ||
      !number(transaction.requires_post_apply_replan) ||
      !put("],\"pivot\":") || !integer(transaction.identity.pivot) ||
      !put(",\"cycle\":[") ||
      !list(transaction.identity.cycle_edges, integer) ||
      !put("],\"sources\":["))
    return false;
  for (size_t i = 0; i < transaction.identity.sources.size(); ++i) {
    if (i && !put(",")) return false;
    const auto &source = transaction.identity.sources[i];
    const uint64_t local = local_group(offered.plan, source.group_identity);
    if (!local || !put("{\"local\":") || !number(local) ||
        !put(",\"vertex\":") || !number(source.vertex) ||
        !put(",\"premises\":[") ||
        !list(source.premises,
              [&](const auto &premise) {
                return pair_of(number, premise.occurrence, premise.proof);
              }) ||
        !put("]}"))
      return false;
  }
  if (!put("],\"clause\":[") || !list(transaction.x7.clause, integer) ||
      !put("],\"witness_true\":[") ||
      !list(transaction.x7.witness_true, integer) ||
      !put("],\"substitution\":[") ||
      !list(transaction.x7.substitution,
            [&](const auto &substituted) {
              return pair_of(integer, substituted.first, substituted.second);
            }))
    return false;
  const bool emits = kernel.proof != nullptr;
  const int64_t unit = -transaction.identity.pivot;
  const int64_t auxiliary =
      static_cast<int64_t>(offered.plan.snapshot().external_to_internal.size()) +
      1;
  if (!put("],\"planned_accounting\":[") ||
      !list(accounting_fields(transaction, true), number) || !put("]") ||
      !put(",\"emitted_accounting\":[") ||
      !list(accounting_fields(transaction, emits), number) ||
      !put("],\"fresh_auxiliary\":"))
    return false;
  if (emits) {
    if (!integer(auxiliary)) return false;
  } else if (!put("null"))
    return false;
  if (!put(",\"helper_additions\":[")) return false;
  if (emits &&
      (!put("[") || !integer(-auxiliary) || !put(",") || !integer(unit) ||
       !put("],[") || !integer(auxiliary) || !put(",") || !integer(unit) ||
       !put("]")))
    return false;
  if (!put("],\"candidate_additions\":[")) return false;
  if (emits && (!put("[") || !integer(unit) || !put("]"))) return false;
  if (!put("],\"helper_deletions\":[")) return false;
  if (emits &&
      (!put("[") || !integer(auxiliary) || !put(",") || !integer(unit) ||
       !put("],[") || !integer(-auxiliary) || !put(",") || !integer(unit) ||
       !put("]")))
    return false;
  if (!put("],\"proof_events\":[") ||
      (emits && !list(transaction.events, [&](auto event) {
         return number(static_cast<uint8_t>(event));
       })))
    return false;
  if (!put("],\"projection\":[],\"deletion_state\":\"") ||
      !put(emits ? "helpers-removed" : "none") ||
      !put("\"},\"mapping\":{") ||
      !put("\"internal_to_external\":[") ||
      !list(offered.plan.snapshot().internal_to_external, number) ||
      !put("],\"external_to_internal\":[") ||
      !list(offered.plan.snapshot().external_to_internal, number) ||
      !put("],\"root_values\":[") ||
      !list(offered.plan.snapshot().root_values, integer) ||
      !put("],\"external_root_units\":["))
    return false;
  size_t root_index = 0;
  for (size_t i = 0; i < offered.plan.snapshot().root_values.size(); ++i) {
    const int8_t value = offered.plan.snapshot().root_values[i];
    if (!value) continue;
    if (root_index++ && !put(",")) return false;
    const int64_t variable =
        static_cast<int64_t>(offered.plan.snapshot().internal_to_external[i]) +
        1;
    if (!integer(value > 0 ? variable : -variable)) return false;
  }
  return put("],\"proof_id\":") && number(kernel.next_clause_id + 1) &&
         put("},\"installed_unit\":") &&
         integer(-transaction.identity.pivot) &&
         put(",\"reconstruction\":\"none-original-model\"}\n");
}

} // namespace

bool TseitinCycleBreakingLimits::valid() const {
  return snapshot.maximum_clauses && snapshot.maximum_literals &&
         snapshot.maximum_work && snapshot.maximum_variables &&
         discovery.work && discovery.maximum_clauses &&
         discovery.maximum_groups && discovery.maximum_width == 4 &&
         admission.work && planning.work &&
         planning.witness_length && planning.additions && transaction.work &&
         transaction.additions && transaction.deletions &&
         transaction.helpers && transaction.witness_literals &&
         maximum_validation_ticks && maximum_components &&
         maximum_identity_bytes > 5 && maximum_total_identity_bytes &&
         maximum_identity_work &&
         maximum_journal_bytes && maximum_session_keys &&
         maximum_session_work && maximum_session_identity_bytes;
}

RuleEligibility tseitin_cycle_breaking_guard(const Kernel &kernel) {
  return StructuralActionGuard(kernel)
      .fact("consistent", !kernel.inconsistent)
      .fact("root", !kernel.level)
      .fact("root-assignments-only",
            std::all_of(kernel.trail.begin(), kernel.trail.end(),
                        [&](Lit lit) {
                          return kernel.assigned[var_of(lit)].level == 0;
                        }))
      .quiescent()
      .identity()
      .fact("sr-proof", !kernel.proof || kernel.proof->supports_sr())
      .finish();
}

// Proof support, the structural guard and limit validity, checked in this
// order by offer and apply. Applied means the request may proceed.
static TseitinCycleBreakingStatus admit_request(
    const Kernel &kernel, const TseitinCycleBreakingLimits &limits) {
  if (kernel.proof && !kernel.proof->supports_sr())
    return TseitinCycleBreakingStatus::UnsupportedProof;
  if (!tseitin_cycle_breaking_guard(kernel))
    return TseitinCycleBreakingStatus::Inapplicable;
  return limits.valid() ? TseitinCycleBreakingStatus::Applied
                        : TseitinCycleBreakingStatus::Invalid;
}

// Session ownership, key-count, and ordered per-key work/identity accounting
// shared by offer and apply. Applied means the retained session is admitted.
static TseitinCycleBreakingStatus account_session(
    Kernel &kernel, const TseitinCycleBreakingSession &session,
    const TseitinCycleBreakingLimits &limits, KernelPoll &action_poll,
    TseitinCycleBreakingEffort &effort) {
  if (session.retained() && !session.belongs_to(kernel))
    return TseitinCycleBreakingStatus::Stale;
  if (session.keys().size() > limits.maximum_session_keys)
    return TseitinCycleBreakingStatus::ResourceExhausted;
  for (const auto &key : session.keys()) {
    if (kernel_poll(&action_poll))
      return TseitinCycleBreakingStatus::Interrupted;
    if (!add(key.size(), limits.maximum_session_work, effort.session_work) ||
        !add(key.size(), limits.maximum_session_identity_bytes,
             effort.session_identity_bytes))
      return TseitinCycleBreakingStatus::ResourceExhausted;
  }
  return TseitinCycleBreakingStatus::Applied;
}

TseitinCycleBreakingOfferResult tseitin_cycle_breaking_offer(
    Kernel &kernel, const TseitinCycleBreakingSession &session,
    const TseitinCycleBreakingLimits &limits) {
  TseitinCycleBreakingOfferResult result;
  result.status = admit_request(kernel, limits);
  if (result.status != TseitinCycleBreakingStatus::Applied) return result;
  KernelPoll action_poll{&kernel, limits.discovery.interrupted,
                         limits.discovery.interrupt_state};
  result.status = account_session(kernel, session, limits, action_poll,
                                  result.effort);
  if (result.status != TseitinCycleBreakingStatus::Applied) return result;
  std::vector<Offered> offered;
  result.status =
      enumerate_offers(kernel, limits, action_poll, offered, result.effort);
  if (result.status != TseitinCycleBreakingStatus::Applied) return result;
  for (const auto &item : offered) {
    size_t position = 0;
    uint64_t prospective = result.effort.session_identity_bytes;
    const auto admitted = admit_session_key(
        session, limits, item.duplicate_key, action_poll,
        result.effort.session_work, prospective, position);
    if (admitted == TseitinCycleBreakingStatus::Applied) {
      result.offers.push_back(item.identity);
    } else if (admitted != TseitinCycleBreakingStatus::AlreadyApplied) {
      result.offers.clear();
      result.status = admitted;
      return result;
    }
  }
  result.effort.offers = result.offers.size();
  if (result.offers.empty())
    result.status = TseitinCycleBreakingStatus::AlreadyApplied;
  return result;
}

TseitinCycleBreakingResult tseitin_cycle_breaking_apply(
    Kernel &kernel, TseitinCycleBreakingSession &session,
    const TseitinCycleBreakingLimits &limits, const std::string &identity) {
  TseitinCycleBreakingResult result;
  // The result with the status of the step that ends this application.
  const auto stop = [&](TseitinCycleBreakingStatus status) {
    result.status = status;
    return result;
  };
  result.status = admit_request(kernel, limits);
  if (result.status != TseitinCycleBreakingStatus::Applied) return result;
  if (identity.size() > limits.maximum_identity_bytes)
    return stop(TseitinCycleBreakingStatus::ResourceExhausted);
  KernelPoll action_poll{&kernel, limits.discovery.interrupted,
                         limits.discovery.interrupt_state};
  size_t split = std::string::npos;
  WorkMeter syntax{limits.maximum_identity_work,
                       result.effort.identity_work, &action_poll};
  if (!parse_identity(identity, syntax, split))
    return stop(syntax.failure(TseitinCycleBreakingStatus::Invalid));
  result.status = account_session(kernel, session, limits, action_poll,
                                  result.effort);
  if (result.status != TseitinCycleBreakingStatus::Applied) return result;

  const std::string_view selected_component(identity.data() + 5, split - 5);
  Offered selected;
  result.status = select_component(kernel, limits, selected_component,
                                   selected.plan, result.effort);
  if (result.status != TseitinCycleBreakingStatus::Applied) return result;
  tseitin_cycle_live::Prepared prepared;
  const auto response = tseitin_cycle_discovery::prepare(
      kernel, selected.plan, live_request(limits), prepared);
  if (!add_live_effort(response, false, result.effort))
    return stop(TseitinCycleBreakingStatus::ResourceExhausted);
  if (response.status != tseitin_cycle_live::Status::Prepared)
    return stop(map(response.status));
  selected.transaction = prepared.transaction();
  WorkMeter meter{limits.maximum_identity_work,
                      result.effort.identity_work, &action_poll};
  const bool encoded = encode_identity(selected.plan, selected.transaction,
                                       limits.maximum_identity_bytes,
                                       selected.component, selected.identity,
                                       meter);
  if (!encoded || !identity_equal(selected.identity, identity, meter) ||
      !encode_duplicate_key(selected.plan, selected.transaction,
                            limits.maximum_identity_bytes,
                            selected.duplicate_key, meter))
    return stop(meter.failure(TseitinCycleBreakingStatus::Stale));
  result.effort.identity_bytes = selected.identity.size();
  size_t position = 0;
  result.status = admit_session_key(
      session, limits, selected.duplicate_key, action_poll,
      result.effort.session_work, result.effort.session_identity_bytes,
      position);
  if (result.status != TseitinCycleBreakingStatus::Applied) return result;
  std::vector<std::string> keys;
  keys.reserve(session.keys().size() + 1);
  keys.insert(keys.end(), session.keys().begin(),
              session.keys().begin() + position);
  keys.push_back(selected.duplicate_key);
  keys.insert(keys.end(), session.keys().begin() + position,
              session.keys().end());

  result.proposed_action_id = kernel.structural_action_sequence + 1;
  std::string journal;
  uint64_t journal_work = 0;
  WorkMeter journal_meter{UINT64_MAX, journal_work, &action_poll};
  const bool journal_ready = make_journal(
      kernel, result.proposed_action_id, limits, selected,
      limits.maximum_journal_bytes, journal, journal_meter);
  result.effort.journal_work = journal_work;
  result.effort.journal_bytes = journal.size();
  if (!journal_ready)
    return stop(
        journal_meter.failure(TseitinCycleBreakingStatus::ResourceExhausted));
  const auto apply_request = live_request(limits);
  result.live = tseitin_cycle_live::apply(kernel, prepared, &apply_request);
  if (!add_live_effort(result.live, true, result.effort)) std::abort();
  result.status = map(result.live.status);
  result.result = result.live.result;
  if (result.status != TseitinCycleBreakingStatus::Applied) return result;
  if (kernel.proof) {
    const auto &accounting = selected.transaction.accounting;
    result.effort.proof_additions = accounting.preflight_additions;
    result.effort.proof_deletions = accounting.preflight_deletions;
    result.effort.proof_helpers = accounting.temporary_helpers;
    result.effort.proof_only_variables = accounting.proof_only_variables;
    result.effort.proof_witness_literals = accounting.witness_literals;
    result.effort.proof_routes = 1;
    const uint64_t candidate_literals = selected.transaction.x7.clause.size();
    result.effort.proof_literal_occurrences =
        2 * (candidate_literals + 1) + candidate_literals;
  }
  if (std::fwrite(journal.data(), 1, journal.size(),
                  kernel.structural_provenance) != journal.size() ||
      std::fflush(kernel.structural_provenance) ||
      std::ferror(kernel.structural_provenance))
    std::abort();
  TseitinCycleBreakingSessionAccess::commit(session, kernel, std::move(keys));
  kernel.structural_action_sequence = result.proposed_action_id;
  result.committed_action_id = result.proposed_action_id;
  result.identity = std::move(selected.identity);
  return result;
}

void record_tseitin_cycle_breaking_feedback(
    Kernel &kernel, const TseitinCycleBreakingResult &result) {
  ++kernel.stats.tseitin_cycle_attempts;
  kernel.stats.tseitin_cycle_snapshot_clauses +=
      result.effort.snapshot_clauses;
  kernel.stats.tseitin_cycle_snapshot_literals +=
      result.effort.snapshot_literals;
  kernel.stats.tseitin_cycle_snapshot_work += result.effort.snapshot_work;
  kernel.stats.tseitin_cycle_recapture_work += result.effort.recapture_work;
  kernel.stats.tseitin_cycle_discovery_work += result.effort.discovery_work;
  kernel.stats.tseitin_cycle_retained_bytes += result.effort.retained_bytes;
  kernel.stats.tseitin_cycle_admission_work += result.effort.admission_work;
  kernel.stats.tseitin_cycle_planning_work += result.effort.planning_work;
  kernel.stats.tseitin_cycle_transaction_work +=
      result.effort.transaction_work;
  kernel.stats.tseitin_cycle_validation_ticks +=
      result.effort.validation_ticks;
  kernel.stats.tseitin_cycle_candidates += result.effort.candidates;
  kernel.stats.tseitin_cycle_offers += result.effort.offers;
  kernel.stats.tseitin_cycle_identity_bytes += result.effort.identity_bytes;
  kernel.stats.tseitin_cycle_identity_work += result.effort.identity_work;
  kernel.stats.tseitin_cycle_journal_work += result.effort.journal_work;
  kernel.stats.tseitin_cycle_journal_bytes += result.effort.journal_bytes;
  kernel.stats.tseitin_cycle_session_work += result.effort.session_work;
  kernel.stats.tseitin_cycle_session_identity_bytes +=
      result.effort.session_identity_bytes;
  kernel.stats.tseitin_cycle_proof_additions +=
      result.effort.proof_additions;
  kernel.stats.tseitin_cycle_proof_deletions +=
      result.effort.proof_deletions;
  kernel.stats.tseitin_cycle_proof_literal_occurrences +=
      result.effort.proof_literal_occurrences;
  kernel.stats.tseitin_cycle_proof_helpers += result.effort.proof_helpers;
  kernel.stats.tseitin_cycle_proof_only_variables +=
      result.effort.proof_only_variables;
  kernel.stats.tseitin_cycle_proof_witness_literals +=
      result.effort.proof_witness_literals;
  kernel.stats.tseitin_cycle_proof_routes += result.effort.proof_routes;
  const bool standard_status = strategy::record_standard_action_status(
      result.status,
      {kernel.stats.tseitin_cycle_applied, kernel.stats.tseitin_cycle_invalid,
       kernel.stats.tseitin_cycle_inapplicable,
       kernel.stats.tseitin_cycle_exhausted,
       kernel.stats.tseitin_cycle_interrupted,
       kernel.stats.tseitin_cycle_unsupported_proof,
       kernel.stats.tseitin_cycle_stale});
  if (result.status == TseitinCycleBreakingStatus::Applied)
    ++kernel.stats.tseitin_cycle_units;
  if (!standard_status) {
    switch (result.status) {
    case TseitinCycleBreakingStatus::NoStructure:
      ++kernel.stats.tseitin_cycle_no_structure;
      break;
    case TseitinCycleBreakingStatus::AlreadyApplied:
      ++kernel.stats.tseitin_cycle_already_applied;
      break;
    default: break;
    }
  }
}

} // namespace tactix
