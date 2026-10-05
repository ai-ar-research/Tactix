// Proof-strength XOR recognition over an explicitly identified clause snapshot.
// The caller owns live-clause enumeration, external mapping and snapshot epoch.
#pragma once
#include "rules/lucky/parity_certificate.hpp"
#include <map>
#include <set>

namespace tactix::parity_premises {
using parity_certificate::Equation;
using parity_certificate::External;

struct Limits {
  uint64_t clauses;
  uint64_t groups;
};
struct CertifiedEquation {
  Equation equation;
  // Input occurrence IDs in ascending forbidden-assignment mask order.
  // Zero is never an occurrence ID; duplicates retain the first occurrence.
  std::vector<uint64_t> premises;
};
enum class Status { Complete, Exhausted, Invalid };

class Collector {
  struct Scope {
    std::array<External, 4> variables{};
    uint8_t size = 0;
    bool operator<(const Scope &other) const {
      if (variables != other.variables) return variables < other.variables;
      return size < other.size;
    }
  };
  struct Group { std::array<uint64_t, 16> occurrences{}; };
  Limits limits_;
  uint64_t seen_ = 0;
  std::map<Scope, Group> groups_;
  std::set<uint64_t> identities_;
  Status status_ = Status::Complete;

public:
  explicit Collector(Limits limits) : limits_(limits) {}
  Status status() const { return status_; }
  uint64_t clauses_seen() const { return seen_; }
  uint64_t groups_seen() const { return groups_.size(); }

  // Exact clauses only: no root shortening, guessed missing clauses or
  // recognition from a selected model. Unsupported widths lose opportunities.
  bool add(uint64_t occurrence, const External *literals, uint32_t size) {
    if (status_ != Status::Complete) return false;
    if (seen_ == limits_.clauses) {
      status_ = Status::Exhausted;
      return false;
    }
    ++seen_;
    if (!occurrence || !identities_.insert(occurrence).second ||
        (size && !literals)) {
      status_ = Status::Invalid;
      return false;
    }
    if (size < 2 || size > 4) return true;
    std::array<std::pair<External, bool>, 4> ordered{};
    for (uint32_t i = 0; i < size; ++i) {
      const External lit = literals[i];
      if (!lit || lit == INT32_MIN) {
        status_ = Status::Invalid;
        return false;
      }
      ordered[i] = {lit < 0 ? -lit : lit, lit < 0};
    }
    // Bounded insertion sort avoids library small-array overread warnings.
    for (uint32_t i = 1; i < size; ++i)
      for (uint32_t j = i; j && ordered[j] < ordered[j-1]; --j)
        std::swap(ordered[j], ordered[j-1]);
    Scope scope;
    scope.size = static_cast<uint8_t>(size);
    uint32_t mask = 0;
    for (uint32_t i = 0; i < size; ++i) {
      if (i && ordered[i].first == ordered[i-1].first)
        return true; // repeated-variable/tautological clause is not a premise
      scope.variables[i] = ordered[i].first;
      mask |= uint32_t(ordered[i].second) << i;
    }
    auto found = groups_.find(scope);
    if (found == groups_.end()) {
      if (groups_.size() == limits_.groups) {
        status_ = Status::Exhausted;
        return false;
      }
      found = groups_.emplace(scope, Group{}).first;
    }
    uint64_t &id = found->second.occurrences[mask];
    if (!id) id = occurrence;
    return true;
  }

  bool finish(std::vector<CertifiedEquation> &output) const {
    output.clear();
    if (status_ != Status::Complete) return false;
    for (const auto &[scope, group] : groups_) {
      for (unsigned forbidden = 0; forbidden < 2; ++forbidden) {
        CertifiedEquation candidate;
        candidate.equation.size = scope.size;
        candidate.equation.rhs = !forbidden;
        std::copy_n(scope.variables.begin(), scope.size,
                    candidate.equation.variables.begin());
        bool complete = true;
        for (unsigned mask = 0; mask < (1u << scope.size); ++mask) {
          if ((std::popcount(mask) & 1u) != forbidden) continue;
          if (!group.occurrences[mask]) { complete = false; break; }
          candidate.premises.push_back(group.occurrences[mask]);
        }
        if (complete) output.push_back(std::move(candidate));
      }
    }
    return true;
  }
};
} // namespace tactix::parity_premises
