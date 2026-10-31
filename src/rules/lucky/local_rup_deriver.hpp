#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace tactix {

// Bounded branching RUP derivation over either vector clauses or the fixed
// clause representation used by the parity certificate.  Clause order,
// branch order, work charges, and emitted/deleted proof order are observable.
template <class Clause, class Sink> class LocalRupDeriver {
  using External = int32_t;
  using Clauses = std::vector<Clause>;

  Sink &sink_;
  Clauses local_;
  Clauses emitted_;
  std::vector<External> variables_;
  std::vector<External> order_;
  uint32_t maximum_variables_;
  std::function<bool(uint64_t)> charge_;
  bool keep_intermediates_for_empty_target_;
  bool stopped_ = false;

  static uint32_t size(const Clause &clause) {
    if constexpr (requires { clause.size(); })
      return static_cast<uint32_t>(clause.size());
    else
      return clause.size;
  }

  static External literal(const Clause &clause, uint32_t index) {
    if constexpr (requires { clause[index]; })
      return clause[index];
    else
      return clause.lits[index];
  }

  static External magnitude(External value) {
    return value < 0 ? -value : value;
  }

  static bool can_append(const Clause &clause, size_t variable_count) {
    if constexpr (requires(Clause &value) { value.push_back(External{}); })
      return size(clause) < variable_count;
    else
      return size(clause) < clause.lits.size();
  }

  static void append(Clause &clause, External value) {
    if constexpr (requires { clause.push_back(value); })
      clause.push_back(value);
    else
      clause.lits[clause.size++] = value;
  }

  static void flip_last(Clause &clause) {
    if constexpr (requires { clause.back(); })
      clause.back() = -clause.back();
    else
      clause.lits[clause.size - 1] = -clause.lits[clause.size - 1];
  }

  static void pop(Clause &clause) {
    if constexpr (requires { clause.pop_back(); })
      clause.pop_back();
    else
      --clause.size;
  }

  bool charge(uint64_t amount) {
    if (stopped_) return false;
    if (charge_ && !charge_(amount)) {
      stopped_ = true;
      return false;
    }
    return true;
  }

  int index(External variable) const {
    variable = magnitude(variable);
    const auto found =
        std::lower_bound(variables_.begin(), variables_.end(), variable);
    return found == variables_.end() || *found != variable
               ? -1
               : static_cast<int>(found - variables_.begin());
  }

  bool propagate(const Clause &candidate, std::vector<int8_t> &assignment) {
    if (!charge(uint64_t{1} + size(candidate))) return false;
    assignment.assign(variables_.size(), 0);
    for (uint32_t i = 0; i < size(candidate); ++i) {
      const External value = literal(candidate, i);
      const int at = index(value);
      assert(at >= 0);
      const int8_t assignment_value = value > 0 ? -1 : 1;
      if (assignment[at] && assignment[at] != assignment_value)
        return true;
      assignment[at] = assignment_value;
    }
    bool changed = true;
    while (changed) {
      changed = false;
      for (const Clause &clause : local_) {
        if (!charge(uint64_t{1} + size(clause))) return false;
        bool satisfied = false;
        uint32_t unassigned = 0;
        External last = 0;
        for (uint32_t i = 0; i < size(clause); ++i) {
          const External value = literal(clause, i);
          const int at = index(value);
          assert(at >= 0);
          const int8_t assigned = assignment[at];
          if (assigned && ((value > 0) == (assigned > 0))) {
            satisfied = true;
            break;
          }
          if (!assigned) {
            ++unassigned;
            last = value;
          }
        }
        if (satisfied) continue;
        if (!unassigned) return true;
        if (unassigned == 1) {
          const int at = index(last);
          const int8_t value = last > 0 ? 1 : -1;
          if (assignment[at] && assignment[at] != value) return true;
          if (!assignment[at]) {
            assignment[at] = value;
            changed = true;
          }
        }
      }
    }
    return false;
  }

  bool visit(Clause &candidate) {
    if (!charge(1)) return false;
    std::vector<int8_t> assignment;
    if (!propagate(candidate, assignment)) {
      if (stopped_) return false;
      External branch = 0;
      for (const External variable : order_) {
        const int at = index(variable);
        if (at >= 0 && !assignment[at]) {
          branch = variable;
          break;
        }
      }
      if (!branch || !can_append(candidate, variables_.size())) return false;
      append(candidate, branch);
      if (!visit(candidate)) return false;
      flip_last(candidate);
      if (!visit(candidate)) return false;
      pop(candidate);
      if (!propagate(candidate, assignment) || stopped_) return false;
    }
    if (!sink_.add(candidate, false)) return false;
    local_.push_back(candidate);
    emitted_.push_back(candidate);
    return true;
  }

public:
  explicit LocalRupDeriver(
      Sink &sink, uint32_t maximum_variables = 16,
      std::function<bool(uint64_t)> charge = {},
      bool keep_intermediates_for_empty_target = true)
      : sink_(sink), maximum_variables_(maximum_variables),
        charge_(std::move(charge)), keep_intermediates_for_empty_target_(
                                         keep_intermediates_for_empty_target) {}

  bool derive(const Clause &target, const Clauses &premises,
              const std::vector<External> &priority, Clause &kept) {
    local_ = premises;
    emitted_.clear();
    variables_.clear();
    stopped_ = false;
    for (const Clause &clause : local_)
      for (uint32_t i = 0; i < size(clause); ++i)
        variables_.push_back(magnitude(literal(clause, i)));
    for (uint32_t i = 0; i < size(target); ++i)
      variables_.push_back(magnitude(literal(target, i)));
    std::sort(variables_.begin(), variables_.end());
    variables_.erase(std::unique(variables_.begin(), variables_.end()),
                     variables_.end());
    if (variables_.size() > maximum_variables_) return false;
    order_.clear();
    for (External variable : priority) {
      variable = magnitude(variable);
      if (variable &&
          std::find(order_.begin(), order_.end(), variable) == order_.end())
        order_.push_back(variable);
    }
    for (const External variable : variables_)
      if (std::find(order_.begin(), order_.end(), variable) == order_.end())
        order_.push_back(variable);
    Clause candidate = target;
    if (!visit(candidate)) return false;
    kept = target;
    if (size(target) || !keep_intermediates_for_empty_target_)
      for (size_t i = 0; i + 1 < emitted_.size(); ++i)
        if (!sink_.erase(emitted_[i])) return false;
    return true;
  }
};

} // namespace tactix
