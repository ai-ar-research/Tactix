// Shared proof-only XOR accumulation. Certified source equations are combined
// through fresh RAT-defined balanced parity trees and bounded local RUP steps.
// Native installation and semantic provenance belong to the invoking rule.
#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstdint>
#include <vector>
#include <utility>
namespace tactix::parity_certificate {
using External = int32_t;
struct Equation {
  std::array<External, 4> variables{};
  uint8_t size = 0;
  bool rhs = false;
};

struct SmallClause {
  std::array<External, 16> lits{};
  uint8_t size = 0;
};

using Clauses = std::vector<SmallClause>;

inline Clauses xor_equation(const External *variables, uint32_t size, bool rhs) {
  std::array<External, 4> normalized{};
  uint32_t normalized_size = 0;
  for (uint32_t i = 0; i < size; ++i) {
    const External variable = variables[i];
    if (!variable)
      continue;
    uint32_t at = 0;
    while (at < normalized_size && normalized[at] != variable)
      ++at;
    if (at < normalized_size) {
      normalized[at] = normalized[--normalized_size];
    } else {
      assert(normalized_size < normalized.size());
      normalized[normalized_size++] = variable;
    }
  }
  // The arity is at most four.  Keep the bounded sort explicit so GCC does
  // not instantiate std::sort's 16-element insertion-sort fast path for this
  // four-element array (which also produces a false array-bounds warning).
  for (uint32_t i = 1; i < normalized_size; ++i)
    for (uint32_t j = i; j && normalized[j] < normalized[j - 1]; --j)
      std::swap(normalized[j], normalized[j - 1]);
  Clauses result;
  result.reserve(1u << (normalized_size ? normalized_size - 1 : 0));
  for (uint32_t mask = 0; mask < (1u << normalized_size); ++mask) {
    if ((std::popcount(mask) & 1) == rhs)
      continue;
    SmallClause clause;
    clause.size = static_cast<uint8_t>(normalized_size);
    for (uint32_t i = 0; i < normalized_size; ++i)
      clause.lits[i] =
          mask & (1u << i) ? -normalized[i] : normalized[i];
    result.push_back(clause);
  }
  return result;
}

template <class Sink> class Deriver {
  Sink &sink_;
  Clauses local_;
  Clauses emitted_;
  std::vector<External> variables_;
  std::vector<External> order_;

  int index(External variable) const {
    variable = variable < 0 ? -variable : variable;
    for (size_t i = 0; i < variables_.size(); ++i)
      if (variables_[i] == variable)
        return static_cast<int>(i);
    return -1;
  }

  bool propagate(const SmallClause &candidate,
                 std::vector<int8_t> &assignment) const {
    assignment.assign(variables_.size(), 0);
    for (uint32_t i = 0; i < candidate.size; ++i) {
      const External literal = candidate.lits[i];
      const int at = index(literal);
      assert(at >= 0);
      const int8_t value = literal > 0 ? -1 : 1;
      if (assignment[at] && assignment[at] != value)
        return true;
      assignment[at] = value;
    }
    bool changed = true;
    while (changed) {
      changed = false;
      for (const SmallClause &clause : local_) {
        bool satisfied = false;
        uint32_t unassigned = 0;
        External last = 0;
        for (uint32_t i = 0; i < clause.size; ++i) {
          const External literal = clause.lits[i];
          const int at = index(literal);
          assert(at >= 0);
          const int8_t value = assignment[at];
          if (value && ((literal > 0) == (value > 0))) {
            satisfied = true;
            break;
          }
          if (!value) {
            ++unassigned;
            last = literal;
          }
        }
        if (satisfied)
          continue;
        if (!unassigned)
          return true;
        if (unassigned == 1) {
          const int at = index(last);
          const int8_t value = last > 0 ? 1 : -1;
          if (assignment[at] && assignment[at] != value)
            return true;
          if (!assignment[at]) {
            assignment[at] = value;
            changed = true;
          }
        }
      }
    }
    return false;
  }

  bool visit(SmallClause &candidate) {
    std::vector<int8_t> assignment;
    if (!propagate(candidate, assignment)) {
      External branch = 0;
      for (const External variable : order_) {
        const int at = index(variable);
        if (at >= 0 && !assignment[at]) {
          branch = variable;
          break;
        }
      }
      if (!branch || static_cast<size_t>(candidate.size) + 1 >
                         candidate.lits.size())
        return false;
      candidate.lits[candidate.size++] = branch;
      if (!visit(candidate))
        return false;
      candidate.lits[candidate.size - 1] = -branch;
      if (!visit(candidate))
        return false;
      --candidate.size;
      if (!propagate(candidate, assignment))
        return false;
    }
    if (!sink_.add(candidate, false))
      return false;
    local_.push_back(candidate);
    emitted_.push_back(candidate);
    return true;
  }

public:
  explicit Deriver(Sink &sink) : sink_(sink) {}

  bool derive(const SmallClause &target, const Clauses &premises,
              const std::vector<External> &priority, SmallClause &kept) {
    local_ = premises;
    emitted_.clear();
    variables_.clear();
    for (const SmallClause &clause : local_)
      for (uint32_t i = 0; i < clause.size; ++i) {
        const External variable =
            clause.lits[i] < 0 ? -clause.lits[i] : clause.lits[i];
        if (std::find(variables_.begin(), variables_.end(), variable) ==
            variables_.end())
          variables_.push_back(variable);
      }
    for (uint32_t i = 0; i < target.size; ++i) {
      const External variable =
          target.lits[i] < 0 ? -target.lits[i] : target.lits[i];
      if (std::find(variables_.begin(), variables_.end(), variable) ==
          variables_.end())
        variables_.push_back(variable);
    }
    if (variables_.size() > 16)
      return false;
    std::sort(variables_.begin(), variables_.end());
    order_.clear();
    for (External variable : priority) {
      variable = variable < 0 ? -variable : variable;
      if (variable &&
          std::find(order_.begin(), order_.end(), variable) == order_.end())
        order_.push_back(variable);
    }
    for (const External variable : variables_)
      if (std::find(order_.begin(), order_.end(), variable) == order_.end())
        order_.push_back(variable);
    SmallClause candidate = target;
    if (!visit(candidate))
      return false;
    kept = target;
    if (target.size)
      for (size_t i = 0; i + 1 < emitted_.size(); ++i)
        if (!sink_.erase(emitted_[i]))
          return false;
    return true;
  }
};

template <class Sink> class Certificate {
  Sink &sink_;
  uint32_t width_ = 1;
  std::vector<External> nodes_;
  std::vector<Clauses> definitions_;
  Clauses current_unit_;
  bool parity_ = false;
  bool emitted_empty_ = false;
  bool started_ = false;
  bool completed_ = false;
  uint32_t variables_;

  bool erase(const Clauses &clauses) {
    for (const SmallClause &clause : clauses)
      if (!sink_.erase(clause))
        return false;
    return true;
  }

  bool gate(External a, External b, External &out, Clauses &definitions) {
    definitions.clear();
    if (!a) {
      out = b;
      return true;
    }
    if (!b) {
      out = a;
      return true;
    }
    if (a == b) {
      out = 0;
      return true;
    }
    if (!sink_.new_variable(out))
      return false;
    const External variables[3] = {a, b, out};
    definitions = xor_equation(variables, 3, false);
    for (SmallClause &clause : definitions) {
      uint32_t pivot = 0;
      while (pivot < clause.size &&
             (clause.lits[pivot] < 0 ? -clause.lits[pivot]
                                     : clause.lits[pivot]) != out)
        ++pivot;
      assert(pivot < clause.size);
      std::swap(clause.lits[0], clause.lits[pivot]);
      if (!sink_.add(clause, true))
        return false;
    }
    return true;
  }

public:
  Certificate(Sink &sink, uint32_t variables) : sink_(sink), variables_(variables) {
    while (width_ < variables)
      width_ <<= 1;
    nodes_.assign(2 * static_cast<size_t>(width_), 0);
    definitions_.resize(nodes_.size());
  }

  // The input equations must have independently certified clause premises.
  // This helper emits proof events only; it never installs kernel clauses.
  bool accumulate(const std::vector<Equation> &equations) {
    if (started_)
      return false;
    started_ = true;
    for (const Equation &equation : equations) {
      if (equation.size > equation.variables.size())
        return false;
      for (uint32_t i = 0; i < equation.size; ++i)
        if (equation.variables[i] <= 0 ||
            static_cast<uint32_t>(equation.variables[i]) > variables_)
          return false;
    }
    Deriver<Sink> deriver(sink_);
    for (const Equation &equation : equations) {
      const Clauses row_axioms =
          xor_equation(equation.variables.data(), equation.size, equation.rhs);
      Clauses transitions;
      for (uint32_t variable_at = 0; variable_at < equation.size;
           ++variable_at) {
        const External variable = equation.variables[variable_at];
        uint32_t at = width_ + static_cast<uint32_t>(variable) - 1;
        External old_child = nodes_[at];
        nodes_[at] = old_child ? 0 : variable;
        Clauses relation;
        Clauses old_definitions;
        while (at > 1) {
          const uint32_t parent = at >> 1;
          const uint32_t sibling = at ^ 1;
          const External old_parent = nodes_[parent];
          External new_parent = 0;
          Clauses new_definitions;
          if (!gate(nodes_[at], nodes_[sibling], new_parent, new_definitions))
            return false;
          Clauses premises = relation;
          premises.insert(premises.end(), definitions_[parent].begin(),
                          definitions_[parent].end());
          premises.insert(premises.end(), new_definitions.begin(),
                          new_definitions.end());
          const External relation_variables[3] = {old_parent, new_parent,
                                                  variable};
          const Clauses targets = xor_equation(relation_variables, 3, false);
          Clauses next_relation;
          next_relation.reserve(targets.size());
          for (const SmallClause &target : targets) {
            SmallClause kept;
            if (!deriver.derive(target, premises, {old_child}, kept))
              return false;
            next_relation.push_back(kept);
          }
          if (!erase(relation))
            return false;
          old_definitions.insert(old_definitions.end(),
                                 definitions_[parent].begin(),
                                 definitions_[parent].end());
          definitions_[parent] = std::move(new_definitions);
          nodes_[parent] = new_parent;
          relation = std::move(next_relation);
          old_child = old_parent;
          at = parent;
        }
        transitions.insert(transitions.end(), relation.begin(), relation.end());
        if (!erase(old_definitions))
          return false;
      }
      parity_ ^= equation.rhs;
      SmallClause target;
      bool has_target = true;
      if (nodes_[1]) {
        target.size = 1;
        target.lits[0] = parity_ ? nodes_[1] : -nodes_[1];
      } else if (parity_) {
        target.size = 0;
      } else {
        has_target = false;
      }
      Clauses next_unit;
      if (has_target) {
        Clauses premises = row_axioms;
        premises.insert(premises.end(), transitions.begin(), transitions.end());
        premises.insert(premises.end(), current_unit_.begin(),
                        current_unit_.end());
        SmallClause kept;
        std::vector<External> priority(equation.variables.begin(),
                                       equation.variables.begin() + equation.size);
        if (!deriver.derive(target, premises, priority, kept))
          return false;
        if (!target.size) {
          emitted_empty_ = true;
          completed_ = true;
          return true;
        }
        next_unit.push_back(kept);
      }
      if (!erase(transitions) || !erase(current_unit_))
        return false;
      current_unit_ = std::move(next_unit);
    }
    completed_ = true;
    return true;
  }

  // Preserve the existing terminal caller's contract.
  bool run(const std::vector<Equation> &equations) {
    return accumulate(equations) && emitted_empty_;
  }

  bool refuted() const { return completed_ && emitted_empty_; }

  // Export the sum when at most four original variables remain. Current
  // active tree definitions and the accumulated root unit certify every
  // clause. Fresh tree variables remain proof helpers, never search variables.
  // The caller owns the width choice, live installation, and helper retirement.
  bool export_small_consequence(uint32_t maximum_width, Clauses &output) {
    output.clear();
    if (!completed_ || emitted_empty_ || maximum_width > 4)
      return false;
    std::array<External, 4> residue{};
    uint32_t size = 0;
    for (uint32_t i = 0; i < variables_; ++i) {
      const External leaf = nodes_[width_ + i];
      if (!leaf)
        continue;
      if (size == maximum_width)
        return false;
      residue[size++] = leaf;
    }
    if (!size)
      return true; // 0 = 0 has no clauses.
    Clauses premises = current_unit_;
    for (const Clauses &definition : definitions_)
      premises.insert(premises.end(), definition.begin(), definition.end());
    const Clauses targets = xor_equation(residue.data(), size, parity_);
    Deriver<Sink> deriver(sink_);
    for (const SmallClause &target : targets) {
      SmallClause kept;
      if (!deriver.derive(target, premises, {}, kept))
        return false;
      output.push_back(kept);
    }
    return true;
  }
};

} // namespace tactix::parity_certificate
