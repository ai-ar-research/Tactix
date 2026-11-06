// Shared proof-only XOR accumulation. Certified source equations are combined
// through fresh RAT-defined balanced parity trees and bounded local RUP steps.
// Native installation and semantic provenance belong to the invoking rule.
#pragma once
#include "rules/lucky/local_rup_deriver.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <vector>
#include <utility>
namespace tactix::parity_certificate {
using External = int32_t;
struct Equation {
  // Affine semantic-extension recognition admits complete encodings through
  // arity eight.  The original Lucky callers use at most four entries, but
  // the scalable row certificate must preserve the authenticated wider source
  // equations instead of splitting them into unauthenticated fragments.
  std::array<External, 8> variables{};
  uint8_t size = 0;
  bool rhs = false;
};

struct SmallClause {
  std::array<External, 16> lits{};
  uint8_t size = 0;
};

using Clauses = std::vector<SmallClause>;

inline Clauses xor_equation(const External *variables, uint32_t size, bool rhs) {
  std::array<External, 8> normalized{};
  assert(size <= normalized.size());
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

// The variables of x ^ y ^ z = 0 without zeros and cancelling pairs, sorted.
// Every relation and definition in a parity tree has zero or three of them.
struct Xor3 {
  std::array<External, 3> variables{};
  uint32_t size = 0;
};

inline Xor3 xor3(External x, External y, External z) {
  Xor3 result;
  for (const External variable : {x, y, z}) {
    if (!variable) continue;
    uint32_t at = 0;
    while (at < result.size && result.variables[at] != variable) ++at;
    if (at < result.size)
      result.variables[at] = result.variables[--result.size];
    else
      result.variables[result.size++] = variable;
  }
  for (uint32_t i = 1; i < result.size; ++i)
    for (uint32_t j = i; j && result.variables[j] < result.variables[j - 1];
         --j)
      std::swap(result.variables[j], result.variables[j - 1]);
  return result;
}

// Clause k of xor_equation(x.variables, 3, false): the odd masks 1, 2, 4, 7
// select the negated variables.
inline constexpr std::array<uint32_t, 4> xor3_masks{1, 2, 4, 7};

inline SmallClause xor3_clause(const Xor3 &x, uint32_t k) {
  assert(x.size == 3 && k < 4);
  SmallClause clause;
  clause.size = 3;
  for (uint32_t i = 0; i < 3; ++i)
    clause.lits[i] = xor3_masks[k] >> i & 1 ? -x.variables[i] : x.variables[i];
  return clause;
}

// Clause k of the RAT definition output = first ^ second, output first.
inline SmallClause definition_clause(External output, External first,
                                     External second, uint32_t k) {
  const Xor3 x = xor3(first, second, output);
  SmallClause clause = xor3_clause(x, k);
  const uint32_t pivot = x.variables[0] == output ? 0
                         : x.variables[1] == output ? 1
                                                    : 2;
  std::swap(clause.lits[0], clause.lits[pivot]);
  return clause;
}

inline Clauses definition_clauses(External output, External first,
                                  External second) {
  Clauses clauses;
  clauses.reserve(4);
  for (uint32_t k = 0; k < 4; ++k)
    clauses.push_back(definition_clause(output, first, second, k));
  return clauses;
}

template <class Sink>
using Deriver = LocalRupDeriver<SmallClause, Sink>;

// Deriver's search for clauses over at most 32 variables, none repeated in a
// clause, with each clause as two literal bit masks.  Scans, charges,
// branches and proof lines are Deriver's own.
template <class Sink> class MaskDeriver {
  struct Bits {
    uint32_t positive = 0;
    uint32_t negative = 0;
    uint32_t size = 0;
  };

  Sink &sink_;
  uint32_t maximum_variables_;
  const std::function<bool(uint64_t)> &charge_;
  std::vector<External> variables_;
  std::vector<uint32_t> order_;
  std::vector<Bits> local_;
  Clauses emitted_;
  bool stopped_ = false;

  bool charge(uint64_t amount) {
    if (stopped_) return false;
    if (charge_ && !charge_(amount)) {
      stopped_ = true;
      return false;
    }
    return true;
  }

  uint32_t bit(External literal) const {
    const External variable = literal < 0 ? -literal : literal;
    return 1u << (std::lower_bound(variables_.begin(), variables_.end(),
                                   variable) -
                  variables_.begin());
  }

  Bits bits(const SmallClause &clause) const {
    Bits result;
    result.size = clause.size;
    for (uint32_t i = 0; i < clause.size; ++i)
      (clause.lits[i] > 0 ? result.positive : result.negative) |=
          bit(clause.lits[i]);
    return result;
  }

  // Falsify `candidate` and propagate; true on a conflict.  `value` holds
  // the assigned variables that are true.
  bool propagate(const SmallClause &candidate, uint32_t &assigned,
                 uint32_t &value) {
    if (!charge(uint64_t{1} + candidate.size)) return false;
    assigned = value = 0;
    for (uint32_t i = 0; i < candidate.size; ++i) {
      const External literal = candidate.lits[i];
      const uint32_t at = bit(literal);
      if (assigned & at) {
        if (bool(value & at) != (literal < 0)) return true;
        continue;
      }
      assigned |= at;
      if (literal < 0) value |= at;
    }
    for (bool changed = true; changed;) {
      changed = false;
      for (const Bits &clause : local_) {
        if (!charge(uint64_t{1} + clause.size)) return false;
        if ((clause.positive & assigned & value) |
            (clause.negative & assigned & ~value))
          continue;
        const uint32_t open = (clause.positive | clause.negative) & ~assigned;
        if (!open) return true;
        if (!(open & (open - 1))) {
          assigned |= open;
          if (clause.positive & open) value |= open;
          changed = true;
        }
      }
    }
    return false;
  }

  bool visit(SmallClause &candidate) {
    if (!charge(1)) return false;
    uint32_t assigned = 0, value = 0;
    if (!propagate(candidate, assigned, value)) {
      if (stopped_) return false;
      const auto branch =
          std::find_if(order_.begin(), order_.end(),
                       [&](uint32_t at) { return !(assigned >> at & 1); });
      if (branch == order_.end() || candidate.size >= candidate.lits.size())
        return false;
      candidate.lits[candidate.size++] = variables_[*branch];
      if (!visit(candidate)) return false;
      candidate.lits[candidate.size - 1] = -candidate.lits[candidate.size - 1];
      if (!visit(candidate)) return false;
      --candidate.size;
      if (!propagate(candidate, assigned, value) || stopped_) return false;
    }
    if (!sink_.add(candidate, false)) return false;
    local_.push_back(bits(candidate));
    emitted_.push_back(candidate);
    return true;
  }

public:
  MaskDeriver(Sink &sink, uint32_t maximum_variables,
              const std::function<bool(uint64_t)> &charge)
      : sink_(sink), maximum_variables_(maximum_variables), charge_(charge) {}

  // Derive `target` from `premises`, branching first on `priority`.  Every
  // emitted intermediate is erased unless the target is the empty clause.
  bool derive(const SmallClause &target, const Clauses &premises,
              const External *priority, uint32_t priority_size) {
    variables_.clear();
    for (const SmallClause &clause : premises)
      for (uint32_t i = 0; i < clause.size; ++i)
        variables_.push_back(std::abs(clause.lits[i]));
    for (uint32_t i = 0; i < target.size; ++i)
      variables_.push_back(std::abs(target.lits[i]));
    std::sort(variables_.begin(), variables_.end());
    variables_.erase(std::unique(variables_.begin(), variables_.end()),
                     variables_.end());
    if (variables_.size() > maximum_variables_) return false;
    assert(variables_.size() <= 32);
    if (variables_.size() > 32) return false;
    order_.clear();
    const auto add_order = [&](External variable) {
      const auto found =
          std::lower_bound(variables_.begin(), variables_.end(), variable);
      if (found == variables_.end() || *found != variable) return;
      const uint32_t at = static_cast<uint32_t>(found - variables_.begin());
      if (std::find(order_.begin(), order_.end(), at) == order_.end())
        order_.push_back(at);
    };
    for (uint32_t i = 0; i < priority_size; ++i)
      add_order(std::abs(priority[i]));
    for (const External variable : variables_) add_order(variable);
    local_.clear();
    for (const SmallClause &clause : premises) local_.push_back(bits(clause));
    emitted_.clear();
    stopped_ = false;
    SmallClause candidate = target;
    if (!visit(candidate)) return false;
    if (target.size)
      for (size_t i = 0; i + 1 < emitted_.size(); ++i)
        if (!sink_.erase(emitted_[i])) return false;
    return true;
  }
};

struct ActiveDefinition {
  uint32_t position = 0;
  External output = 0;
  External first = 0;
  External second = 0;
  Clauses clauses;
};

struct ActiveTree {
  uint32_t variable_count = 0;
  std::vector<ActiveDefinition> definitions;
  std::vector<External> variables;
  SmallClause root_unit;
  bool rhs = false;
};

template <class Sink> class Certificate {
  using Operands = std::pair<External, External>;
  struct Replaced {
    External output = 0;
    Operands operands;
  };

  Sink &sink_;
  uint32_t width_ = 1;
  std::vector<External> nodes_;
  // Operands of the currently live definition at each segment-tree node;
  // its clauses are definition_clause(nodes_[node], first, second, 0..3).
  // Entries are cleared whenever cancellation removes that node.  Keeping the
  // graph lets a continuing affine action retain exactly the final
  // conservative definitions and reconstruct their variables.
  std::vector<Operands> operands_;
  // One equation's root relation per toggle, and one walk's replaced
  // definitions; both are erased once their consequence is derived.
  std::vector<Xor3> transitions_;
  std::vector<Replaced> replaced_;
  Clauses premises_;
  Clauses current_unit_;
  bool parity_ = false;
  bool emitted_empty_ = false;
  bool started_ = false;
  bool completed_ = false;
  bool export_started_ = false;
  bool exported_ = false;
  bool retirement_started_ = false;
  bool retired_ = false;
  uint32_t variables_;
  uint32_t maximum_derivation_variables_ = 16;
  std::function<bool(uint64_t)> charge_;

  bool charge(uint64_t amount) {
    return !charge_ || charge_(amount);
  }

  // `count` separate charges of `amount`, one per clause a propagation scans.
  bool charge(uint32_t count, uint64_t amount) {
    if (charge_)
      for (uint32_t i = 0; i < count; ++i)
        if (!charge_(amount))
          return false;
    return true;
  }

  bool erase(const Clauses &clauses) {
    for (const SmallClause &clause : clauses)
      if (!sink_.erase(clause))
        return false;
    return true;
  }

  bool erase(const Xor3 &relation) {
    for (uint32_t k = 0; relation.size && k < 4; ++k)
      if (!sink_.erase(xor3_clause(relation, k)))
        return false;
    return true;
  }

  bool erase_definition(External output, const Operands &operands) {
    for (uint32_t k = 0; k < 4; ++k)
      if (!sink_.erase(definition_clause(output, operands.first,
                                         operands.second, k)))
        return false;
    return true;
  }

  bool gate(External a, External b, External &out) {
    if (!a || !b) {
      out = a ? a : b;
      return true;
    }
    if (a == b) {
      out = 0;
      return true;
    }
    if (!sink_.new_variable(out))
      return false;
    for (uint32_t k = 0; k < 4; ++k)
      if (!sink_.add(definition_clause(out, a, b, k), true))
        return false;
    return true;
  }

  // Derive `next` (old_parent ^ new_parent ^ variable = 0) from `relation`
  // (old_child ^ child ^ variable = 0) and the parent's old and new
  // definitions, emitting exactly Deriver's lines and charges; the tree
  // fixes the shape of its search.  Without a sibling the parent passes its
  // child through and clause k repeats clause k of `relation`; with a sibling
  // but an empty relation it repeats clause k of the parent's one
  // definition.  Otherwise clause k splits on old_child: either branch
  // propagates `child` through the relation and `sibling` through the old
  // definition, then falsifies one clause of the new definition.
  bool derive(const Xor3 &relation, const Xor3 &next, External old_child,
              External child, External sibling, External old_parent,
              External new_parent, External variable) {
    if (!next.size)
      return true;
    assert(next.size == 3 && (relation.size == 0 || relation.size == 3));
    const bool split = sibling && relation.size;
    if ((split ? 6u : 3u) > maximum_derivation_variables_)
      return false;
    // The new-definition clause a branch falsifies: target clause k fixes
    // old_parent, new_parent and variable; the relation and old definition
    // then fix child and sibling.
    const auto conflict = [&](uint32_t k, bool old_child_value) {
      const auto value = [&](External x) {
        const uint32_t at = x == next.variables[0] ? 0
                            : x == next.variables[1] ? 1
                                                     : 2;
        return bool(xor3_masks[k] >> at & 1);
      };
      const bool child_value = old_child_value != value(variable);
      const bool sibling_value = old_child_value != value(old_parent);
      const Xor3 definition = xor3(child, sibling, new_parent);
      uint32_t mask = 0;
      for (uint32_t i = 0; i < 3; ++i) {
        const External x = definition.variables[i];
        mask |= uint32_t(x == child     ? child_value
                         : x == sibling ? sibling_value
                                        : value(new_parent))
                << i;
      }
      return mask == 7 ? 3u : static_cast<uint32_t>(std::countr_zero(mask));
    };
    for (uint32_t k = 0; k < 4; ++k) {
      const SmallClause target = xor3_clause(next, k);
      if (!split) {
        if (!charge(1) || !charge(4) || !charge(k + 1, 4) ||
            !sink_.add(target, false))
          return false;
        continue;
      }
      SmallClause positive = target;
      positive.lits[positive.size++] = old_child;
      SmallClause negative = positive;
      negative.lits[3] = -old_child;
      const uint32_t positive_scan = charge_ ? 9 + conflict(k, false) : 0;
      const uint32_t negative_scan = charge_ ? 9 + conflict(k, true) : 0;
      if (!charge(1) || !charge(4) || !charge(12, 4) ||
          !charge(1) || !charge(5) || !charge(positive_scan, 4) ||
          !sink_.add(positive, false) ||
          !charge(1) || !charge(5) || !charge(negative_scan, 4) ||
          !sink_.add(negative, false) ||
          !charge(4) || !charge(12, 4) || !charge(2, 5) ||
          !sink_.add(target, false) || !sink_.erase(positive) ||
          !sink_.erase(negative))
        return false;
    }
    return true;
  }

  // Flip one leaf and re-derive each relation up to the root.  The root's
  // relation becomes this toggle's transition.
  bool toggle(External variable) {
    uint32_t at = width_ + static_cast<uint32_t>(variable) - 1;
    External old_child = nodes_[at];
    nodes_[at] = old_child ? 0 : variable;
    Xor3 relation;
    replaced_.clear();
    while (at > 1) {
      const uint32_t parent = at >> 1;
      const External child = nodes_[at];
      const External sibling = nodes_[at ^ 1];
      const External old_parent = nodes_[parent];
      External new_parent = 0;
      if (!gate(child, sibling, new_parent))
        return false;
      const Xor3 next = xor3(old_parent, new_parent, variable);
      if (!derive(relation, next, old_child, child, sibling, old_parent,
                  new_parent, variable) ||
          !erase(relation))
        return false;
      if (operands_[parent].first)
        replaced_.push_back({old_parent, operands_[parent]});
      nodes_[parent] = new_parent;
      operands_[parent] = new_parent && child && sibling && child != sibling
                              ? Operands{child, sibling}
                              : Operands{};
      relation = next;
      old_child = old_parent;
      at = parent;
    }
    transitions_.push_back(relation);
    for (const Replaced &replaced : replaced_)
      if (!erase_definition(replaced.output, replaced.operands))
        return false;
    return true;
  }

public:
  using ActiveDefinition = parity_certificate::ActiveDefinition;
  using ActiveTree = parity_certificate::ActiveTree;

  Certificate(Sink &sink, uint32_t variables,
              uint32_t maximum_derivation_variables = 16,
              std::function<bool(uint64_t)> charge = {})
      : sink_(sink), variables_(variables),
        maximum_derivation_variables_(maximum_derivation_variables),
        charge_(std::move(charge)) {
    while (width_ < variables)
      width_ <<= 1;
    nodes_.assign(2 * static_cast<size_t>(width_), 0);
    operands_.resize(nodes_.size());
  }

  // The input equations must have independently certified clause premises.
  // This helper emits proof events only; it never installs kernel clauses.
  bool accumulate(const std::vector<Equation> &equations) {
    if (started_)
      return false;
    started_ = true;
    for (const Equation &equation : equations) {
      if (!charge(1 + equation.size)) return false;
      if (equation.size > equation.variables.size())
        return false;
      for (uint32_t i = 0; i < equation.size; ++i)
        if (equation.variables[i] <= 0 ||
            static_cast<uint32_t>(equation.variables[i]) > variables_)
          return false;
    }
    MaskDeriver<Sink> deriver(sink_, maximum_derivation_variables_, charge_);
    for (const Equation &equation : equations) {
      transitions_.clear();
      for (uint32_t variable_at = 0; variable_at < equation.size;
           ++variable_at) {
        if (!charge(1) || !toggle(equation.variables[variable_at]))
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
      // The root unit follows from the row, its transitions and the previous
      // unit.  Its splits depend on which prefixes of the row leave the tree
      // empty or a single leaf, so it keeps Deriver's search.
      if (has_target) {
        premises_ = xor_equation(equation.variables.data(), equation.size,
                                 equation.rhs);
        for (const Xor3 &transition : transitions_)
          for (uint32_t k = 0; transition.size && k < 4; ++k)
            premises_.push_back(xor3_clause(transition, k));
        premises_.insert(premises_.end(), current_unit_.begin(),
                         current_unit_.end());
        if (!deriver.derive(target, premises_, equation.variables.data(),
                            equation.size))
          return false;
        if (!target.size) {
          emitted_empty_ = true;
          completed_ = true;
          return true;
        }
      }
      for (const Xor3 &transition : transitions_)
        if (!erase(transition))
          return false;
      if (!erase(current_unit_))
        return false;
      current_unit_.clear();
      if (has_target)
        current_unit_.push_back(target);
    }
    completed_ = true;
    return true;
  }

  // Preserve the existing terminal caller's contract.
  bool run(const std::vector<Equation> &equations) {
    return accumulate(equations) && emitted_empty_;
  }

  bool refuted() const { return completed_ && emitted_empty_; }

  // Describe the proof clauses that remain live after accumulation.  This is
  // a pure snapshot: it emits no proof event and does not settle the later
  // export/retirement lifecycle.  Definitions are returned in fresh-variable
  // order, which is also topological because every gate is allocated after
  // its operands.  A continuing solver may install this exact closed set as a
  // conservative Boolean extension and retain the final implied root unit.
  bool active_tree(ActiveTree &output) const {
    if (!completed_ || emitted_empty_ || export_started_ ||
        retirement_started_ || current_unit_.size() != 1 ||
        current_unit_[0].size != 1 || !nodes_[1])
      return false;
    const External root_literal = current_unit_[0].lits[0];
    if ((root_literal < 0 ? -root_literal : root_literal) != nodes_[1] ||
        (root_literal > 0) != parity_)
      return false;
    ActiveTree tree;
    tree.variable_count = variables_;
    tree.root_unit = current_unit_[0];
    tree.rhs = parity_;
    for (uint32_t i = 0; i < variables_; ++i)
      if (nodes_[width_ + i]) tree.variables.push_back(i + 1);
    for (size_t node = 1; node < width_; ++node) {
      const auto [first, second] = operands_[node];
      if (!nodes_[node]) {
        if (first || second)
          return false;
        continue;
      }
      if (!first && !second) continue;
      if (!first || !second || first == second)
        return false;
      tree.definitions.push_back(
          {static_cast<uint32_t>(node), nodes_[node], first, second,
           definition_clauses(nodes_[node], first, second)});
    }
    std::sort(tree.definitions.begin(), tree.definitions.end(),
              [](const ActiveDefinition &a, const ActiveDefinition &b) {
                return a.output < b.output;
              });
    for (size_t i = 0; i < tree.definitions.size(); ++i) {
      const ActiveDefinition &definition = tree.definitions[i];
      if (i && tree.definitions[i - 1].output >= definition.output)
        return false;
      for (External operand : {definition.first, definition.second})
        if (operand > static_cast<External>(variables_) &&
            operand >= definition.output)
          return false;
    }
    output = std::move(tree);
    return true;
  }

  // Resume from clauses that are already active in the same proof stream.
  // The caller must establish their authenticated lifetime; this validates
  // the exact tree shape without emitting or assuming any new proof step.
  // A rejected snapshot leaves the certificate untouched.
  bool resume_active_tree(const ActiveTree &tree) {
    if (started_ || completed_ || export_started_ || retirement_started_ ||
        tree.variable_count != variables_ || tree.variables.empty() ||
        tree.root_unit.size != 1 ||
        tree.definitions.size() >= width_ ||
        !std::is_sorted(tree.variables.begin(), tree.variables.end()) ||
        std::adjacent_find(tree.variables.begin(), tree.variables.end()) !=
            tree.variables.end())
      return false;
    std::vector<External> nodes(nodes_.size(), 0);
    std::vector<Operands> operands(operands_.size());
    std::vector<const ActiveDefinition *> at(width_, nullptr);
    External last_output = 0;
    for (External variable : tree.variables) {
      if (variable <= 0 || static_cast<uint32_t>(variable) > variables_)
        return false;
      nodes[width_ + static_cast<uint32_t>(variable) - 1] = variable;
    }
    for (const ActiveDefinition &definition : tree.definitions) {
      if (!definition.position || definition.position >= width_ ||
          at[definition.position] ||
          definition.output <= static_cast<External>(variables_) ||
          definition.output <= last_output)
        return false;
      at[definition.position] = &definition;
      last_output = definition.output;
    }
    size_t used = 0;
    for (uint32_t node = width_ - 1; node; --node) {
      const External first = nodes[2 * node];
      const External second = nodes[2 * node + 1];
      const ActiveDefinition *definition = at[node];
      if (!first || !second) {
        if (definition) return false;
        nodes[node] = first ? first : second;
        continue;
      }
      if (first == second || !definition ||
          !((definition->first == first && definition->second == second) ||
            (definition->first == second && definition->second == first)) ||
          definition->output <= first || definition->output <= second ||
          definition->clauses.size() != 4)
        return false;
      for (uint32_t k = 0; k < 4; ++k) {
        const SmallClause expected = definition_clause(
            definition->output, definition->first, definition->second, k);
        const SmallClause &clause = definition->clauses[k];
        if (clause.size != expected.size ||
            !std::equal(expected.lits.begin(),
                        expected.lits.begin() + expected.size,
                        clause.lits.begin()))
          return false;
      }
      nodes[node] = definition->output;
      operands[node] = {definition->first, definition->second};
      ++used;
    }
    if (used != tree.definitions.size() || !nodes[1] ||
        tree.root_unit.lits[0] != (tree.rhs ? nodes[1] : -nodes[1]))
      return false;
    nodes_.swap(nodes);
    operands_.swap(operands);
    current_unit_ = {tree.root_unit};
    parity_ = tree.rhs;
    return true;
  }

  // Export the sum when at most four original variables remain. Current
  // active tree definitions and the accumulated root unit certify every
  // clause. Fresh tree variables remain proof helpers, never search variables.
  // The caller owns the width choice, live installation, and helper retirement.
  bool export_small_consequence(uint32_t maximum_width, Clauses &output) {
    output.clear();
    if (!completed_ || emitted_empty_ || export_started_ || maximum_width > 4)
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
    export_started_ = true;
    if (!size) {
      exported_ = true;
      return true; // 0 = 0 has no clauses.
    }
    Clauses premises = current_unit_;
    for (size_t node = 0; node < operands_.size(); ++node)
      if (operands_[node].first)
        for (uint32_t k = 0; k < 4; ++k)
          premises.push_back(definition_clause(
              nodes_[node], operands_[node].first, operands_[node].second, k));
    const Clauses targets = xor_equation(residue.data(), size, parity_);
    Deriver<Sink> deriver(sink_, maximum_derivation_variables_, charge_);
    for (const SmallClause &target : targets) {
      SmallClause kept;
      if (!deriver.derive(target, premises, {}, kept))
        return false;
      output.push_back(kept);
    }
    exported_ = true;
    return true;
  }

  // Export one arbitrary-width clausal reason for the accumulated parity.
  // Falsifying `target` must assign every active residue variable and violate
  // the root parity; extra authenticated variables only weaken the clause.
  // The live proof sink is responsible for checking the RUP addition. This
  // avoids materializing the exponentially large complete parity CNF when a
  // Gaussian implication or conflict needs only one reason.
  bool export_reason(const std::vector<External> &target) {
    if (!completed_ || emitted_empty_ || export_started_ || target.empty())
      return false;
    std::vector<int8_t> signs(static_cast<size_t>(variables_) + 1, 0);
    for (External literal : target) {
      if (!literal || literal == INT32_MIN) return false;
      const uint32_t variable = static_cast<uint32_t>(
          literal < 0 ? -int64_t{literal} : int64_t{literal});
      if (!variable || variable > variables_ || signs[variable]) return false;
      // The value that falsifies this literal.
      signs[variable] = literal < 0 ? 1 : -1;
    }
    bool falsified_parity = false;
    for (uint32_t i = 0; i < variables_; ++i) {
      if (!nodes_[width_ + i]) continue;
      if (!signs[i + 1]) return false;
      falsified_parity ^= signs[i + 1] > 0;
    }
    if (falsified_parity == parity_) return false;
    export_started_ = true;
    if (!sink_.add(target, false)) return false;
    exported_ = true;
    return true;
  }

  // Retire clauses only, never the variable namespace: some checkers retain
  // deleted helper-unit assignments. The caller must keep fresh IDs monotone.
  // Retire only after a complete export. Original-variable units are retained:
  // deleting one by content could erase the exported unit as well. No helper
  // appears in an exported clause. Once retirement starts, failed sinks cannot
  // be retried or reported safe; the caller must handle proof-output failure.
  bool retire_helpers() {
    if (retirement_started_)
      return retired_;
    if (!exported_)
      return false;
    retirement_started_ = true;
    for (const SmallClause &unit : current_unit_) {
      assert(unit.size == 1);
      const External lit = unit.lits[0];
      const uint32_t variable = static_cast<uint32_t>(lit < 0 ? -lit : lit);
      if (variable > variables_ && !sink_.erase(unit))
        return false;
    }
    for (size_t node = 0; node < operands_.size(); ++node)
      if (operands_[node].first &&
          !erase_definition(nodes_[node], operands_[node]))
        return false;
    current_unit_.clear();
    std::fill(operands_.begin(), operands_.end(), Operands{});
    retired_ = true;
    return true;
  }
};

} // namespace tactix::parity_certificate
