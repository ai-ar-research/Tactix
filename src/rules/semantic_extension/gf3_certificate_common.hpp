#pragma once

#include "kernel/types.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace tactix::gf3_certificate_common {

inline bool add_checked(uint64_t first, uint64_t second, uint64_t &result) {
  if (second > std::numeric_limits<uint64_t>::max() - first) return false;
  result = first + second;
  return true;
}

inline std::vector<int32_t> canonical(std::vector<int32_t> clause) {
  std::sort(clause.begin(), clause.end());
  return clause;
}

template <class Theory, class Reference, class Proof>
bool retain_premise(const Theory &theory, const Reference &reference,
                    std::set<uint64_t> &occurrences, Proof &proof) {
  if (!reference.occurrence) return false;
  auto premise = theory.source.end();
  for (auto candidate = theory.source.begin(); candidate != theory.source.end();
       ++candidate) {
    if (!proof.meter.spend(1)) return false;
    if (candidate->occurrence == reference.occurrence) {
      premise = candidate;
      break;
    }
  }
  if (premise == theory.source.end() ||
      premise->native_proof_id != reference.native_proof_id)
    return false;
  if (!occurrences.insert(reference.occurrence).second) return true;
  if (proof.active.size() >= proof.limits.maximum_active_clauses) {
    proof.meter.limit = decltype(proof.meter.limit)::ActiveClauses;
    return false;
  }
  if (!proof.meter.spend(uint64_t{1} + premise->literals.size())) return false;
  proof.active.push_back(canonical(premise->literals));
  return true;
}

template <class Theory, class Bridge, class Sources, class Proof,
          class LimitKind>
bool collect_authenticated_premises(const Theory &theory,
                                    const Bridge &bridge,
                                    const Sources &sources, Proof &proof,
                                    LimitKind branch_domains_limit) {
  std::set<uint64_t> occurrences;
  std::set<uint32_t> domains;
  for (const auto &source : sources) {
    if (source.authentication_index >= bridge.authentication.size())
      return false;
    const auto &authentication =
        bridge.authentication[source.authentication_index];
    if (authentication.source_id != source.combination.source_id)
      return false;
    for (const auto &domain : authentication.domains) {
      if (!retain_premise(theory, domain.at_least_one, occurrences, proof))
        return false;
      for (const auto &amo : domain.at_most_one)
        if (!retain_premise(theory, amo, occurrences, proof)) return false;
      domains.insert(domain.domain);
    }
    for (const auto &tuple : authentication.forbidden_tuples)
      if (!retain_premise(theory, tuple.premise, occurrences, proof))
        return false;
  }
  if (domains.size() > proof.limits.maximum_branch_domains) {
    proof.meter.limit = branch_domains_limit;
    return false;
  }
  proof.branch_domains.assign(domains.begin(), domains.end());
  return true;
}

template <class Limits, class Charge, class Statistics, class LimitKind>
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

  bool retain_source() {
    if (stats.retained_source_references >=
        limits_.maximum_retained_source_references) {
      limit = LimitKind::RetainedSourceReferences;
      return false;
    }
    if (!spend(1)) return false;
    ++stats.retained_source_references;
    return true;
  }
};

template <class Derived, class Theory, class Limits, class Meter, class Fixed>
struct LocalProof {
  const Theory &theory;
  const Limits &limits;
  Meter &meter;
  std::vector<std::vector<int32_t>> active;
  std::vector<uint32_t> branch_domains;
  Fixed fixed;
  bool uncertifiable = false;

  LocalProof(const Theory &theory_, const Limits &limits_, Meter &meter_)
      : theory(theory_), limits(limits_), meter(meter_) {}

  bool available(const std::vector<int32_t> &candidate) {
    for (const std::vector<int32_t> &clause : active) {
      if (!meter.spend(uint64_t{1} + clause.size() + candidate.size()))
        return false;
      if (std::includes(candidate.begin(), candidate.end(), clause.begin(),
                        clause.end()))
        return true;
    }
    return false;
  }

  bool assign(std::map<uint32_t, int8_t> &values, int32_t literal) {
    const uint32_t atom = external_variable_of(literal);
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
          const auto found = values.find(external_variable_of(literal));
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
          const uint32_t atom = external_variable_of(last);
          const auto found = values.find(atom);
          const int8_t before = found == values.end() ? 0 : found->second;
          if (!assign(values, last)) return true;
          changed |= !before;
        }
      }
    }
    return false;
  }

  template <class EventRole>
  bool derive(std::vector<int32_t> clause, size_t branch,
              EventRole root_role, bool root) {
    clause = canonical(std::move(clause));
    if (clause.size() > limits.maximum_event_width) {
      meter.limit = decltype(meter.limit)::EventWidth;
      return false;
    }
    if (available(clause))
      return meter.limit == decltype(meter.limit)::None && !meter.interrupted;
    if (meter.limit != decltype(meter.limit)::None || meter.interrupted)
      return false;
    Derived &derived = static_cast<Derived &>(*this);
    if (rup(clause))
      return derived.add(std::move(clause),
                         root ? root_role : Derived::auxiliary_role());
    if (meter.limit != decltype(meter.limit)::None || meter.interrupted)
      return false;

    while (branch < branch_domains.size() && fixed.count(branch_domains[branch]))
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
      if (meter.limit == decltype(meter.limit)::None && !meter.interrupted)
        uncertifiable = true;
      return false;
    }
    return derived.add(std::move(clause),
                       Derived::branch_role(root_role, root));
  }
};

} // namespace tactix::gf3_certificate_common
