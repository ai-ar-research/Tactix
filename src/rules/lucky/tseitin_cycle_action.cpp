#include "rules/lucky/tseitin_cycle_action.hpp"

#include "rules/structural_action_guard.hpp"

#include "proof/proof.hpp"
#include "strategy/action_status_mapping.hpp"
#include "strategy/action_status_statistics.hpp"

#include <algorithm>
#include <cstdio>
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

struct ActionPoll {
  Kernel *kernel = nullptr;
  tseitin_cycle::InterruptPoll nested = nullptr;
  void *nested_state = nullptr;
};

bool action_interrupted(void *opaque) {
  const auto &poll = *static_cast<ActionPoll *>(opaque);
  return kernel_terminating(*poll.kernel) ||
         (poll.nested && poll.nested(poll.nested_state));
}

tseitin_cycle_discovery::Limits discovery_limits(
    Kernel &kernel, const tseitin_cycle_discovery::Limits &source,
    ActionPoll &poll) {
  poll = {&kernel, source.interrupted, source.interrupt_state};
  auto result = source;
  result.interrupted = action_interrupted;
  result.interrupt_state = &poll;
  return result;
}

bool add(uint64_t amount, uint64_t maximum, uint64_t &value) {
  if (value > maximum || amount > maximum - value) return false;
  value += amount;
  return true;
}

class Encoder {
  std::string bytes_;
  uint64_t maximum_ = 0;
  uint64_t maximum_work_ = UINT64_MAX;
  uint64_t *work_ = nullptr;
  bool *exhausted_ = nullptr;
  tseitin_cycle::InterruptPoll interrupted_ = nullptr;
  void *interrupt_state_ = nullptr;
  bool *was_interrupted_ = nullptr;

public:
  explicit Encoder(uint64_t maximum, uint64_t maximum_work = UINT64_MAX,
                   uint64_t *work = nullptr, bool *exhausted = nullptr,
                   tseitin_cycle::InterruptPoll interrupted = nullptr,
                   void *interrupt_state = nullptr,
                   bool *was_interrupted = nullptr)
      : maximum_(maximum), maximum_work_(maximum_work), work_(work),
        exhausted_(exhausted), interrupted_(interrupted),
        interrupt_state_(interrupt_state),
        was_interrupted_(was_interrupted) {}
  bool spend(uint64_t amount) {
    if (interrupted_ && interrupted_(interrupt_state_)) {
      if (was_interrupted_) *was_interrupted_ = true;
      return false;
    }
    if (!work_) return true;
    if (add(amount, maximum_work_, *work_)) return true;
    if (exhausted_) *exhausted_ = true;
    return false;
  }
  bool u64(uint64_t value) {
    if (!spend(8)) return false;
    if (bytes_.size() > maximum_ || 8 > maximum_ - bytes_.size())
      return false;
    for (unsigned shift = 0; shift != 64; shift += 8)
      bytes_.push_back(static_cast<char>((value >> shift) & 0xff));
    return true;
  }
  bool i32(int32_t value) { return u64(static_cast<uint32_t>(value)); }
  const std::string &bytes() const { return bytes_; }
};

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

bool identity_step(uint64_t amount, uint64_t maximum, uint64_t &work,
                   tseitin_cycle::InterruptPoll interrupted,
                   void *interrupt_state, bool &exhausted,
                   bool &was_interrupted) {
  if (interrupted && interrupted(interrupt_state)) {
    was_interrupted = true;
    return false;
  }
  if (!add(amount, maximum, work)) {
    exhausted = true;
    return false;
  }
  return true;
}

bool parse_identity(const std::string &identity, uint64_t maximum_work,
                    uint64_t &work,
                    tseitin_cycle::InterruptPoll interrupted,
                    void *interrupt_state, bool &exhausted,
                    bool &was_interrupted, size_t &split) {
  if (!identity_step(5, maximum_work, work, interrupted, interrupt_state,
                     exhausted, was_interrupted) ||
      identity.size() <= 7 || identity.compare(0, 5, "tcx2:"))
    return false;
  split = std::string::npos;
  for (size_t index = 5; index < identity.size(); ++index) {
    if (!identity_step(1, maximum_work, work, interrupted, interrupt_state,
                       exhausted, was_interrupted))
      return false;
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
                    uint64_t maximum_work, uint64_t &work,
                    tseitin_cycle::InterruptPoll interrupted,
                    void *interrupt_state, bool &exhausted,
                    bool &was_interrupted) {
  if (!identity_step(1, maximum_work, work, interrupted, interrupt_state,
                     exhausted, was_interrupted) ||
      left.size() != right.size())
    return false;
  for (size_t index = 0; index < left.size(); ++index) {
    if (!identity_step(1, maximum_work, work, interrupted, interrupt_state,
                       exhausted, was_interrupted) ||
        left[index] != right[index])
      return false;
  }
  return true;
}

bool canonical_clause_work(size_t size, uint64_t &work) {
  const uint64_t copied = static_cast<uint64_t>(size);
  if (static_cast<size_t>(copied) != size ||
      (copied && copied > UINT64_MAX / copied))
    return false;
  const uint64_t sorted = copied * copied;
  if (copied > UINT64_MAX - sorted) return false;
  work = copied + sorted;
  return true;
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
                      uint64_t maximum_work = UINT64_MAX,
                      uint64_t *work = nullptr, bool *exhausted = nullptr,
                      tseitin_cycle::InterruptPoll interrupted = nullptr,
                      void *interrupt_state = nullptr,
                      bool *was_interrupted = nullptr) {
  Encoder bytes(maximum_bytes, maximum_work, work, exhausted, interrupted,
                interrupt_state, was_interrupted);
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
      if (!clause) return false;
      uint64_t canonical_work = 0;
      if (!canonical_clause_work(clause->literals.size(), canonical_work) ||
          !bytes.spend(canonical_work))
        return false;
      std::vector<int32_t> literals = clause->literals;
      std::sort(literals.begin(), literals.end());
      if (!bytes.u64(occurrence) || !bytes.u64(proof) ||
          !bytes.u64(clause->literals.size()))
        return false;
      for (int32_t literal : literals)
        if (!bytes.i32(literal)) return false;
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
                        uint64_t maximum_work = UINT64_MAX,
                        uint64_t *work = nullptr, bool *exhausted = nullptr,
                        tseitin_cycle::InterruptPoll interrupted = nullptr,
                        void *interrupt_state = nullptr,
                        bool *was_interrupted = nullptr) {
  Encoder bytes(maximum_bytes, maximum_work, work, exhausted, interrupted,
                interrupt_state, was_interrupted);
  if (!bytes.u64(transaction.version) ||
      !bytes.u64(transaction.requires_unassigned_pivot) ||
      !bytes.u64(transaction.requires_live_x7_validation) ||
      !bytes.u64(transaction.requires_post_apply_replan) ||
      !bytes.i32(transaction.identity.pivot) ||
      !bytes.u64(transaction.identity.cycle_edges.size()))
    return false;
  for (int32_t edge : transaction.identity.cycle_edges)
    if (!bytes.i32(edge)) return false;
  if (!bytes.u64(transaction.identity.sources.size())) return false;
  for (const auto &source : transaction.identity.sources) {
    if (!bytes.spend(plan.groups().size())) return false;
    const auto found = std::find_if(
        plan.groups().begin(), plan.groups().end(), [&](const auto &group) {
          return group.identity == source.group_identity;
        });
    if (found == plan.groups().end()) return false;
    const uint64_t local = std::distance(plan.groups().begin(), found) + 1;
    if (!bytes.u64(local) || !bytes.u64(source.vertex) ||
        !bytes.u64(source.premises.size()))
      return false;
    for (const auto &premise : source.premises)
      if (!bytes.u64(premise.occurrence) || !bytes.u64(premise.proof))
        return false;
  }
  if (!bytes.u64(transaction.x7.clause.size())) return false;
  for (int32_t literal : transaction.x7.clause)
    if (!bytes.i32(literal)) return false;
  if (!bytes.u64(transaction.x7.witness_true.size())) return false;
  for (int32_t literal : transaction.x7.witness_true)
    if (!bytes.i32(literal)) return false;
  if (!bytes.u64(transaction.x7.substitution.size())) return false;
  for (const auto &[source, target] : transaction.x7.substitution) {
    if (!bytes.i32(source) || !bytes.i32(target)) return false;
  }
  if (!bytes.u64(transaction.accounting.preflight_additions) ||
      !bytes.u64(transaction.accounting.preflight_deletions) ||
      !bytes.u64(transaction.accounting.temporary_helpers) ||
      !bytes.u64(transaction.accounting.proof_only_variables) ||
      !bytes.u64(transaction.accounting.committed_solver_clauses) ||
      !bytes.u64(transaction.accounting.witness_literals) ||
      !bytes.u64(transaction.events.size()))
    return false;
  for (auto event : transaction.events)
    if (!bytes.u64(static_cast<uint8_t>(event))) return false;
  if (!bytes.spend(2 * bytes.bytes().size())) return false;
  prepared = hex("", bytes.bytes());
  return true;
}

bool encode_identity(const tseitin_cycle_discovery::Plan &plan,
                     const tseitin_cycle::ProofTransaction &transaction,
                     uint64_t maximum, std::string &component,
                     std::string &identity, uint64_t maximum_work = UINT64_MAX,
                     uint64_t *work = nullptr, bool *exhausted = nullptr,
                     tseitin_cycle::InterruptPoll interrupted = nullptr,
                     void *interrupt_state = nullptr,
                     bool *was_interrupted = nullptr) {
  if (maximum <= 6) return false;
  const uint64_t raw_limit = (maximum - 6) / 2;
  std::string prepared;
  if (!encode_component(plan, raw_limit, component, maximum_work, work,
                        exhausted, interrupted, interrupt_state,
                        was_interrupted) ||
      !encode_transaction(plan, transaction, raw_limit, prepared,
                          maximum_work, work, exhausted, interrupted,
                          interrupt_state, was_interrupted) ||
      component.size() > maximum - 6 ||
      prepared.size() > maximum - 6 - component.size())
    return false;
  if (interrupted && interrupted(interrupt_state)) {
    if (was_interrupted) *was_interrupted = true;
    return false;
  }
  if (work && !add(6 + component.size() + prepared.size(), maximum_work,
                   *work)) {
    if (exhausted) *exhausted = true;
    return false;
  }
  identity = "tcx2:" + component + ':' + prepared;
  return identity.size() <= maximum;
}

bool encode_duplicate_key(const tseitin_cycle_discovery::Plan &plan,
                          const tseitin_cycle::ProofTransaction &transaction,
                          uint64_t maximum, std::string &key,
                          uint64_t maximum_work = UINT64_MAX,
                          uint64_t *work = nullptr,
                          bool *exhausted = nullptr,
                          tseitin_cycle::InterruptPoll interrupted = nullptr,
                          void *interrupt_state = nullptr,
                          bool *was_interrupted = nullptr) {
  if (maximum <= 5) return false;
  Encoder bytes((maximum - 5) / 2, maximum_work, work, exhausted, interrupted,
                interrupt_state, was_interrupted);
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
      if (!clause || !bytes.u64(clause->literals.size())) return false;
      uint64_t canonical_work = 0;
      if (!canonical_clause_work(clause->literals.size(), canonical_work) ||
          !bytes.spend(canonical_work))
        return false;
      std::vector<int32_t> literals = clause->literals;
      std::sort(literals.begin(), literals.end());
      for (int32_t literal : literals)
        if (!bytes.i32(literal)) return false;
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
  ActionPoll poll;
  auto bounded_discovery = discovery_limits(kernel, limits.discovery, poll);
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
  uint64_t maximum_work = 0;
  uint64_t *work = nullptr;
  bool exhausted = false;
  ActionPoll *poll = nullptr;
  bool interrupted = false;
};

bool matches_component(const tseitin_cycle_discovery::Plan &plan,
                       void *opaque) {
  auto &match = *static_cast<ComponentMatch *>(opaque);
  std::string encoded;
  // An unrelated component that exceeds the selected payload bound is a
  // definite non-match, not a failure of selected-action authentication.
  if (!encode_component(plan, match.raw_limit, encoded, match.maximum_work,
                        match.work, &match.exhausted, action_interrupted,
                        match.poll, &match.interrupted))
    return false;
  return identity_equal(encoded, match.target, match.maximum_work,
                        *match.work, action_interrupted, match.poll,
                        match.exhausted, match.interrupted);
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
  ActionPoll poll;
  auto bounded_discovery = discovery_limits(kernel, limits.discovery, poll);
  bounded_discovery.maximum_components = bounded_discovery.maximum_groups;
  ComponentMatch match{target, (limits.maximum_identity_bytes - 6) / 2,
                       limits.maximum_identity_work,
                       &effort.identity_work, false, &poll, false};
  const auto discovered = tseitin_cycle_discovery::select(
      snapshot, bounded_discovery, matches_component, &match, selected);
  effort.discovery_work = discovered.work;
  effort.retained_bytes = discovered.retained_bytes;
  effort.candidates = discovered.components;
  if (match.interrupted) return TseitinCycleBreakingStatus::Interrupted;
  if (match.exhausted) return TseitinCycleBreakingStatus::ResourceExhausted;
  if (discovered.discovered()) return TseitinCycleBreakingStatus::Applied;
  const auto status = map(discovered.status);
  return status == TseitinCycleBreakingStatus::NoStructure
             ? TseitinCycleBreakingStatus::Stale
             : status;
}

TseitinCycleBreakingStatus enumerate_offers(
    Kernel &kernel, const TseitinCycleBreakingLimits &limits,
    std::vector<Offered> &offered, TseitinCycleBreakingEffort &effort) {
  offered.clear();
  std::vector<tseitin_cycle_discovery::Plan> plans;
  const auto captured = capture_components(kernel, limits,
                                           limits.maximum_components, plans,
                                           effort);
  if (captured != TseitinCycleBreakingStatus::Applied) return captured;
  uint64_t total_identity = 0;
  ActionPoll action_poll{&kernel, limits.discovery.interrupted,
                         limits.discovery.interrupt_state};
  std::vector<Offered> local;
  local.reserve(plans.size());
  for (auto &plan : plans) {
    Offered item;
    item.plan = std::move(plan);
    tseitin_cycle_live::Prepared prepared;
    const auto response = tseitin_cycle_discovery::prepare(
        kernel, item.plan, live_request(limits), prepared);
    if (!add(response.snapshot_work, UINT64_MAX, effort.recapture_work) ||
        !add(response.admission_work, UINT64_MAX, effort.admission_work) ||
        !add(response.planning_work, UINT64_MAX, effort.planning_work) ||
        !add(response.transaction_work, UINT64_MAX, effort.transaction_work) ||
        !add(response.validation_ticks, UINT64_MAX, effort.validation_ticks))
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
    bool identity_exhausted = false;
    bool identity_interrupted = false;
    if (!encode_identity(item.plan, item.transaction,
                         limits.maximum_identity_bytes, item.component,
                         item.identity, limits.maximum_identity_work,
                         &effort.identity_work, &identity_exhausted,
                         action_interrupted, &action_poll,
                         &identity_interrupted) ||
        !encode_duplicate_key(item.plan, item.transaction,
                              limits.maximum_identity_bytes,
                              item.duplicate_key,
                              limits.maximum_identity_work,
                              &effort.identity_work, &identity_exhausted,
                              action_interrupted, &action_poll,
                              &identity_interrupted) ||
        !add(item.identity.size(), limits.maximum_total_identity_bytes,
             total_identity)) {
      if (identity_interrupted)
        return TseitinCycleBreakingStatus::Interrupted;
      return TseitinCycleBreakingStatus::ResourceExhausted;
    }
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
                      const std::string &target, uint64_t maximum_work,
                      uint64_t &work, size_t &position, bool &found,
                      tseitin_cycle::InterruptPoll interrupted = nullptr,
                      void *interrupt_state = nullptr,
                      bool *was_interrupted = nullptr) {
  position = 0;
  found = false;
  for (; position < keys.size(); ++position) {
    const std::string &key = keys[position];
    const size_t common = std::min(key.size(), target.size());
    size_t index = 0;
    for (; index < common; ++index) {
      if (interrupted && interrupted(interrupt_state)) {
        if (was_interrupted) *was_interrupted = true;
        return false;
      }
      if (!add(1, maximum_work, work)) return false;
      if (key[index] != target[index]) break;
    }
    if (index < common) {
      if (key[index] > target[index]) return true;
      continue;
    }
    if (!add(1, maximum_work, work)) return false;
    if (key.size() == target.size()) {
      found = true;
      return true;
    }
    if (key.size() > target.size()) return true;
  }
  return true;
}

bool make_journal(const Kernel &kernel, uint64_t action_id,
                  const TseitinCycleBreakingLimits &limits,
                  const Offered &offered, uint64_t maximum,
                  std::string &journal, ActionPoll &poll,
                  uint64_t &work, bool &was_interrupted) {
  const auto &transaction = offered.transaction;
  const auto put = [&](const std::string &text) {
    if (action_interrupted(&poll)) {
      was_interrupted = true;
      return false;
    }
    if (!add(text.size(), UINT64_MAX, work)) return false;
    return append(journal, maximum, text);
  };
  const auto number = [&](uint64_t value) { return put(std::to_string(value)); };
  const auto integer = [&](int64_t value) { return put(std::to_string(value)); };
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
      !put("\"snapshot_clauses\":") ||
      !number(limits.snapshot.maximum_clauses) ||
      !put(",\"snapshot_literals\":") ||
      !number(limits.snapshot.maximum_literals) ||
      !put(",\"snapshot_work\":") ||
      !number(limits.snapshot.maximum_work) ||
      !put(",\"snapshot_variables\":") ||
      !number(limits.snapshot.maximum_variables) ||
      !put(",\"discovery_work\":") || !number(limits.discovery.work) ||
      !put(",\"discovery_clauses\":") ||
      !number(limits.discovery.maximum_clauses) ||
      !put(",\"discovery_groups\":") ||
      !number(limits.discovery.maximum_groups) ||
      !put(",\"discovery_width\":") ||
      !number(limits.discovery.maximum_width) ||
      !put(",\"discovery_profile_components\":") ||
      !number(limits.discovery.maximum_components) ||
      !put(",\"effective_component_cap\":") ||
      !number(limits.discovery.maximum_groups) ||
      !put(",\"enumeration_mode\":\"selected\"") ||
      !put(",\"discovery_retained_bytes\":") ||
      !number(limits.discovery.maximum_retained_bytes) ||
      !put(",\"admission_work\":") || !number(limits.admission.work) ||
      !put(",\"planning_work\":") || !number(limits.planning.work) ||
      !put(",\"planning_witness_length\":") ||
      !number(limits.planning.witness_length) ||
      !put(",\"planning_additions\":") ||
      !number(limits.planning.additions) ||
      !put(",\"transaction_work\":") || !number(limits.transaction.work) ||
      !put(",\"transaction_additions\":") ||
      !number(limits.transaction.additions) ||
      !put(",\"transaction_deletions\":") ||
      !number(limits.transaction.deletions) ||
      !put(",\"transaction_helpers\":") ||
      !number(limits.transaction.helpers) ||
      !put(",\"transaction_witness_literals\":") ||
      !number(limits.transaction.witness_literals) ||
      !put(",\"validation_ticks\":") ||
      !number(limits.maximum_validation_ticks) ||
      !put(",\"maximum_components\":") ||
      !number(limits.maximum_components) ||
      !put(",\"maximum_identity_bytes\":") ||
      !number(limits.maximum_identity_bytes) ||
      !put(",\"maximum_total_identity_bytes\":") ||
      !number(limits.maximum_total_identity_bytes) ||
      !put(",\"maximum_identity_work\":") ||
      !number(limits.maximum_identity_work) ||
      !put(",\"maximum_journal_bytes\":") ||
      !number(limits.maximum_journal_bytes) ||
      !put(",\"maximum_session_keys\":") ||
      !number(limits.maximum_session_keys) ||
      !put(",\"maximum_session_work\":") ||
      !number(limits.maximum_session_work) ||
      !put(",\"maximum_session_identity_bytes\":") ||
      !number(limits.maximum_session_identity_bytes) ||
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
      if (!clause) return false;
      uint64_t canonical_work = 0;
      if (!canonical_clause_work(clause->literals.size(), canonical_work))
        return false;
      if (action_interrupted(&poll)) {
        was_interrupted = true;
        return false;
      }
      if (!add(canonical_work, UINT64_MAX, work)) return false;
      std::vector<int32_t> literals = clause->literals;
      std::sort(literals.begin(), literals.end());
      if (!put("{\"occurrence\":") || !number(occurrence) ||
          !put(",\"proof_id\":") || !number(proof) ||
          !put(",\"clause\":["))
        return false;
      for (size_t j = 0; j < clause->literals.size(); ++j) {
        if (j && !put(",")) return false;
        if (!integer(literals[j])) return false;
      }
      if (!put("]}")) return false;
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
      !put(",\"cycle\":["))
    return false;
  for (size_t i = 0; i < transaction.identity.cycle_edges.size(); ++i) {
    if (i && !put(",")) return false;
    if (!integer(transaction.identity.cycle_edges[i])) return false;
  }
  if (!put("],\"sources\":[")) return false;
  for (size_t i = 0; i < transaction.identity.sources.size(); ++i) {
    if (i && !put(",")) return false;
    const auto &source = transaction.identity.sources[i];
    const auto found = std::find_if(
        offered.plan.groups().begin(), offered.plan.groups().end(),
        [&](const auto &group) {
          return group.identity == source.group_identity;
        });
    if (found == offered.plan.groups().end()) return false;
    const uint64_t local =
        std::distance(offered.plan.groups().begin(), found) + 1;
    if (!put("{\"local\":") || !number(local) ||
        !put(",\"vertex\":") || !number(source.vertex) ||
        !put(",\"premises\":["))
      return false;
    for (size_t j = 0; j < source.premises.size(); ++j) {
      if (j && !put(",")) return false;
      if (!put("[") || !number(source.premises[j].occurrence) ||
          !put(",") || !number(source.premises[j].proof) || !put("]"))
        return false;
    }
    if (!put("]}")) return false;
  }
  if (!put("],\"clause\":[")) return false;
  for (size_t i = 0; i < transaction.x7.clause.size(); ++i) {
    if (i && !put(",")) return false;
    if (!integer(transaction.x7.clause[i])) return false;
  }
  if (!put("],\"witness_true\":[")) return false;
  for (size_t i = 0; i < transaction.x7.witness_true.size(); ++i) {
    if (i && !put(",")) return false;
    if (!integer(transaction.x7.witness_true[i])) return false;
  }
  if (!put("],\"substitution\":[")) return false;
  for (size_t i = 0; i < transaction.x7.substitution.size(); ++i) {
    if (i && !put(",")) return false;
    if (!put("[") || !integer(transaction.x7.substitution[i].first) ||
        !put(",") || !integer(transaction.x7.substitution[i].second) ||
        !put("]"))
      return false;
  }
  if (!put("],\"planned_accounting\":[") ||
      !number(transaction.accounting.preflight_additions) || !put(",") ||
      !number(transaction.accounting.preflight_deletions) || !put(",") ||
      !number(transaction.accounting.temporary_helpers) || !put(",") ||
      !number(transaction.accounting.proof_only_variables) || !put(",") ||
      !number(transaction.accounting.committed_solver_clauses) || !put(",") ||
      !number(transaction.accounting.witness_literals) || !put("]"))
    return false;
  const bool emits = kernel.proof != nullptr;
  const int64_t unit = -transaction.identity.pivot;
  const int64_t auxiliary =
      static_cast<int64_t>(offered.plan.snapshot().external_to_internal.size()) +
      1;
  if (!put(",\"emitted_accounting\":[") ||
      !number(emits ? transaction.accounting.preflight_additions : 0) ||
      !put(",") ||
      !number(emits ? transaction.accounting.preflight_deletions : 0) ||
      !put(",") ||
      !number(emits ? transaction.accounting.temporary_helpers : 0) ||
      !put(",") ||
      !number(emits ? transaction.accounting.proof_only_variables : 0) ||
      !put(",") ||
      !number(transaction.accounting.committed_solver_clauses) || !put(",") ||
      !number(emits ? transaction.accounting.witness_literals : 0) ||
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
  if (!put("],\"proof_events\":[")) return false;
  if (emits) {
    for (size_t i = 0; i < transaction.events.size(); ++i) {
      if (i && !put(",")) return false;
      if (!number(static_cast<uint8_t>(transaction.events[i]))) return false;
    }
  }
  if (!put("],\"projection\":[],\"deletion_state\":\"") ||
      !put(emits ? "helpers-removed" : "none") ||
      !put("\"},\"mapping\":{") ||
      !put("\"internal_to_external\":["))
    return false;
  for (size_t i = 0; i < offered.plan.snapshot().internal_to_external.size();
       ++i) {
    if (i && !put(",")) return false;
    if (!number(offered.plan.snapshot().internal_to_external[i])) return false;
  }
  if (!put("],\"external_to_internal\":[")) return false;
  for (size_t i = 0; i < offered.plan.snapshot().external_to_internal.size();
       ++i) {
    if (i && !put(",")) return false;
    if (!number(offered.plan.snapshot().external_to_internal[i])) return false;
  }
  if (!put("],\"root_values\":[")) return false;
  for (size_t i = 0; i < offered.plan.snapshot().root_values.size(); ++i) {
    if (i && !put(",")) return false;
    if (!integer(offered.plan.snapshot().root_values[i])) return false;
  }
  if (!put("],\"external_root_units\":[")) return false;
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

TseitinCycleBreakingOfferResult tseitin_cycle_breaking_offer(
    Kernel &kernel, const TseitinCycleBreakingSession &session,
    const TseitinCycleBreakingLimits &limits) {
  TseitinCycleBreakingOfferResult result;
  if (kernel.proof && !kernel.proof->supports_sr()) {
    result.status = TseitinCycleBreakingStatus::UnsupportedProof;
    return result;
  }
  if (!tseitin_cycle_breaking_guard(kernel)) {
    result.status = TseitinCycleBreakingStatus::Inapplicable;
    return result;
  }
  if (!limits.valid()) return result;
  ActionPoll action_poll{&kernel, limits.discovery.interrupted,
                         limits.discovery.interrupt_state};
  if (session.retained() && !session.belongs_to(kernel)) {
    result.status = TseitinCycleBreakingStatus::Stale;
    return result;
  }
  if (session.keys().size() > limits.maximum_session_keys) {
    result.status = TseitinCycleBreakingStatus::ResourceExhausted;
    return result;
  }
  for (const auto &key : session.keys()) {
    if (action_interrupted(&action_poll)) {
      result.status = TseitinCycleBreakingStatus::Interrupted;
      return result;
    }
    if (!add(key.size(), limits.maximum_session_work,
             result.effort.session_work) ||
        !add(key.size(), limits.maximum_session_identity_bytes,
             result.effort.session_identity_bytes)) {
      result.status = TseitinCycleBreakingStatus::ResourceExhausted;
      return result;
    }
  }
  std::vector<Offered> offered;
  result.status = enumerate_offers(kernel, limits, offered, result.effort);
  if (result.status != TseitinCycleBreakingStatus::Applied) return result;
  for (const auto &item : offered) {
    size_t position = 0;
    bool duplicate = false;
    bool session_interrupted = false;
    if (!find_session_key(session.keys(), item.duplicate_key,
                          limits.maximum_session_work,
                          result.effort.session_work, position, duplicate,
                          action_interrupted, &action_poll,
                          &session_interrupted)) {
      result.offers.clear();
      result.status = session_interrupted
                          ? TseitinCycleBreakingStatus::Interrupted
                          : TseitinCycleBreakingStatus::ResourceExhausted;
      return result;
    }
    if (!duplicate) {
      uint64_t prospective = result.effort.session_identity_bytes;
      if (session.keys().size() == limits.maximum_session_keys ||
          !add(item.duplicate_key.size(), limits.maximum_session_work,
               result.effort.session_work) ||
          !add(item.duplicate_key.size(),
               limits.maximum_session_identity_bytes, prospective)) {
        result.offers.clear();
        result.status = TseitinCycleBreakingStatus::ResourceExhausted;
        return result;
      }
      result.offers.push_back(item.identity);
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
  if (kernel.proof && !kernel.proof->supports_sr()) {
    result.status = TseitinCycleBreakingStatus::UnsupportedProof;
    return result;
  }
  if (!tseitin_cycle_breaking_guard(kernel)) {
    result.status = TseitinCycleBreakingStatus::Inapplicable;
    return result;
  }
  if (!limits.valid()) return result;
  if (identity.size() > limits.maximum_identity_bytes) {
    result.status = TseitinCycleBreakingStatus::ResourceExhausted;
    return result;
  }
  ActionPoll action_poll{&kernel, limits.discovery.interrupted,
                         limits.discovery.interrupt_state};
  size_t split = std::string::npos;
  bool syntax_exhausted = false, syntax_interrupted = false;
  if (!parse_identity(identity, limits.maximum_identity_work,
                      result.effort.identity_work, action_interrupted,
                      &action_poll, syntax_exhausted, syntax_interrupted,
                      split)) {
    result.status = syntax_interrupted
                        ? TseitinCycleBreakingStatus::Interrupted
                    : syntax_exhausted
                        ? TseitinCycleBreakingStatus::ResourceExhausted
                        : TseitinCycleBreakingStatus::Invalid;
    return result;
  }
  if (session.retained() && !session.belongs_to(kernel)) {
    result.status = TseitinCycleBreakingStatus::Stale;
    return result;
  }
  if (session.keys().size() > limits.maximum_session_keys) {
    result.status = TseitinCycleBreakingStatus::ResourceExhausted;
    return result;
  }
  for (const auto &key : session.keys()) {
    if (action_interrupted(&action_poll)) {
      result.status = TseitinCycleBreakingStatus::Interrupted;
      return result;
    }
    if (!add(key.size(), limits.maximum_session_work,
             result.effort.session_work) ||
        !add(key.size(), limits.maximum_session_identity_bytes,
             result.effort.session_identity_bytes)) {
      result.status = TseitinCycleBreakingStatus::ResourceExhausted;
      return result;
    }
  }

  const std::string_view selected_component(identity.data() + 5, split - 5);
  Offered selected;
  result.status = select_component(kernel, limits, selected_component,
                                   selected.plan, result.effort);
  if (result.status != TseitinCycleBreakingStatus::Applied) return result;
  tseitin_cycle_live::Prepared prepared;
  const auto response = tseitin_cycle_discovery::prepare(
      kernel, selected.plan, live_request(limits), prepared);
  if (!add(response.snapshot_work, UINT64_MAX, result.effort.recapture_work) ||
      !add(response.admission_work, UINT64_MAX,
           result.effort.admission_work) ||
      !add(response.planning_work, UINT64_MAX,
           result.effort.planning_work) ||
      !add(response.transaction_work, UINT64_MAX,
           result.effort.transaction_work) ||
      !add(response.validation_ticks, UINT64_MAX,
           result.effort.validation_ticks)) {
    result.status = TseitinCycleBreakingStatus::ResourceExhausted;
    return result;
  }
  if (response.status != tseitin_cycle_live::Status::Prepared) {
    result.status = map(response.status);
    return result;
  }
  selected.transaction = prepared.transaction();
  bool identity_exhausted = false;
  bool identity_interrupted = false;
  const bool encoded = encode_identity(selected.plan, selected.transaction,
                       limits.maximum_identity_bytes, selected.component,
                       selected.identity, limits.maximum_identity_work,
                       &result.effort.identity_work,
                       &identity_exhausted, action_interrupted, &action_poll,
                       &identity_interrupted);
  const bool exact = encoded && identity_equal(
      selected.identity, identity, limits.maximum_identity_work,
      result.effort.identity_work, action_interrupted, &action_poll,
      identity_exhausted, identity_interrupted);
  if (!exact ||
      !encode_duplicate_key(selected.plan, selected.transaction,
                            limits.maximum_identity_bytes,
                            selected.duplicate_key,
                            limits.maximum_identity_work,
                            &result.effort.identity_work,
                            &identity_exhausted, action_interrupted,
                            &action_poll,
                            &identity_interrupted)) {
    result.status = identity_interrupted
                        ? TseitinCycleBreakingStatus::Interrupted
                    : identity_exhausted
                        ? TseitinCycleBreakingStatus::ResourceExhausted
                        : TseitinCycleBreakingStatus::Stale;
    return result;
  }
  result.effort.identity_bytes = selected.identity.size();
  size_t position = 0;
  bool duplicate = false;
  bool session_interrupted = false;
  if (!find_session_key(session.keys(), selected.duplicate_key,
                        limits.maximum_session_work,
                        result.effort.session_work, position, duplicate,
                        action_interrupted, &action_poll,
                        &session_interrupted)) {
    result.status = session_interrupted
                        ? TseitinCycleBreakingStatus::Interrupted
                        : TseitinCycleBreakingStatus::ResourceExhausted;
    return result;
  }
  if (duplicate) {
    result.status = TseitinCycleBreakingStatus::AlreadyApplied;
    return result;
  }
  if (session.keys().size() == limits.maximum_session_keys ||
      !add(selected.duplicate_key.size(), limits.maximum_session_work,
           result.effort.session_work) ||
      !add(selected.duplicate_key.size(),
           limits.maximum_session_identity_bytes,
           result.effort.session_identity_bytes)) {
    result.status = TseitinCycleBreakingStatus::ResourceExhausted;
    return result;
  }
  std::vector<std::string> keys;
  keys.reserve(session.keys().size() + 1);
  keys.insert(keys.end(), session.keys().begin(),
              session.keys().begin() + position);
  keys.push_back(selected.duplicate_key);
  keys.insert(keys.end(), session.keys().begin() + position,
              session.keys().end());

  result.proposed_action_id = kernel.structural_action_sequence + 1;
  std::string journal;
  bool journal_interrupted = false;
  uint64_t journal_work = 0;
  const bool journal_ready = make_journal(
      kernel, result.proposed_action_id, limits, selected,
      limits.maximum_journal_bytes, journal, action_poll, journal_work,
      journal_interrupted);
  result.effort.journal_work = journal_work;
  result.effort.journal_bytes = journal.size();
  if (!journal_ready) {
    result.status = journal_interrupted
                        ? TseitinCycleBreakingStatus::Interrupted
                        : TseitinCycleBreakingStatus::ResourceExhausted;
    return result;
  }
  const auto apply_request = live_request(limits);
  result.live = tseitin_cycle_live::apply(kernel, prepared, &apply_request);
  if (!add(result.live.snapshot_work, UINT64_MAX,
           result.effort.recapture_work) ||
      !add(result.live.replay_admission_work, UINT64_MAX,
           result.effort.admission_work) ||
      !add(result.live.replay_planning_work, UINT64_MAX,
           result.effort.planning_work) ||
      !add(result.live.replay_transaction_work, UINT64_MAX,
           result.effort.transaction_work) ||
      !add(result.live.validation_ticks, UINT64_MAX,
           result.effort.validation_ticks))
    std::abort();
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
