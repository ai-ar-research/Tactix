// Shared proof-only XOR accumulation. Certified source equations are combined
// through fresh RAT-defined balanced parity trees and bounded local RUP steps.
// Native installation and semantic provenance belong to the invoking rule.
#pragma once
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

template <class Sink> class Deriver {
  Sink &sink_;
  Clauses local_;
  Clauses emitted_;
  std::vector<External> variables_;
  std::vector<External> order_;
  uint32_t maximum_variables_ = 16;
  std::function<bool(uint64_t)> charge_;
  bool stopped_ = false;

  bool charge(uint64_t amount) {
    if (stopped_) return false;
    if (charge_ && !charge_(amount)) {
      stopped_ = true;
      return false;
    }
    return true;
  }

  int index(External variable) const {
    variable = variable < 0 ? -variable : variable;
    const auto found = std::lower_bound(variables_.begin(), variables_.end(),
                                        variable);
    return found == variables_.end() || *found != variable
               ? -1
               : static_cast<int>(found - variables_.begin());
  }

  bool propagate(const SmallClause &candidate,
                 std::vector<int8_t> &assignment) {
    if (!charge(1 + candidate.size)) return false;
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
        if (!charge(1 + clause.size)) return false;
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
      if (!propagate(candidate, assignment) || stopped_)
        return false;
    }
    if (!sink_.add(candidate, false))
      return false;
    local_.push_back(candidate);
    emitted_.push_back(candidate);
    return true;
  }

public:
  explicit Deriver(Sink &sink, uint32_t maximum_variables = 16,
                   std::function<bool(uint64_t)> charge = {})
      : sink_(sink), maximum_variables_(maximum_variables),
        charge_(std::move(charge)) {}

  bool derive(const SmallClause &target, const Clauses &premises,
              const std::vector<External> &priority, SmallClause &kept) {
    local_ = premises;
    emitted_.clear();
    variables_.clear();
    stopped_ = false;
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
    if (variables_.size() > maximum_variables_) return false;
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
  Sink &sink_;
  uint32_t width_ = 1;
  std::vector<External> nodes_;
  std::vector<Clauses> definitions_;
  // Operands of the currently live definition at each segment-tree node.
  // Entries are cleared whenever cancellation removes that node.  Keeping the
  // graph beside the clauses lets a continuing affine action retain exactly
  // the final conservative definitions and reconstruct their variables.
  std::vector<std::pair<External, External>> operands_;
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
    definitions_.resize(nodes_.size());
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
    Deriver<Sink> deriver(sink_, maximum_derivation_variables_, charge_);
    for (const Equation &equation : equations) {
      const Clauses row_axioms =
          xor_equation(equation.variables.data(), equation.size, equation.rhs);
      Clauses transitions;
      for (uint32_t variable_at = 0; variable_at < equation.size;
           ++variable_at) {
        if (!charge(1)) return false;
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
          operands_[parent] = new_parent && nodes_[at] && nodes_[sibling] &&
                                      nodes_[at] != nodes_[sibling]
                                  ? std::pair<External, External>{nodes_[at],
                                                                  nodes_[sibling]}
                                  : std::pair<External, External>{};
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
      if (!nodes_[node]) {
        if (!definitions_[node].empty() || operands_[node].first ||
            operands_[node].second)
          return false;
        continue;
      }
      if (definitions_[node].empty()) continue;
      const auto [first, second] = operands_[node];
      if (!first || !second || first == second)
        return false;
      tree.definitions.push_back(
          {static_cast<uint32_t>(node), nodes_[node], first, second,
           definitions_[node]});
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
    ActiveTree checked = tree;
    output = std::move(checked);
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
    std::vector<Clauses> definitions(definitions_.size());
    std::vector<std::pair<External, External>> operands(operands_.size());
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
          definition->output <= first || definition->output <= second)
        return false;
      const External variables[3] = {definition->first, definition->second,
                                     definition->output};
      Clauses expected = xor_equation(variables, 3, false);
      for (SmallClause &clause : expected) {
        uint32_t pivot = 0;
        while (pivot < clause.size &&
               std::abs(clause.lits[pivot]) != definition->output)
          ++pivot;
        if (pivot == clause.size) return false;
        std::swap(clause.lits[0], clause.lits[pivot]);
      }
      if (definition->clauses.size() != expected.size()) return false;
      for (size_t i = 0; i < expected.size(); ++i) {
        if (definition->clauses[i].size != expected[i].size) return false;
        for (uint32_t j = 0; j < expected[i].size; ++j)
          if (definition->clauses[i].lits[j] != expected[i].lits[j])
            return false;
      }
      nodes[node] = definition->output;
      definitions[node] = definition->clauses;
      operands[node] = {definition->first, definition->second};
      ++used;
    }
    if (used != tree.definitions.size() || !nodes[1] ||
        tree.root_unit.lits[0] != (tree.rhs ? nodes[1] : -nodes[1]))
      return false;
    nodes_.swap(nodes);
    definitions_.swap(definitions);
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
    for (const Clauses &definition : definitions_)
      premises.insert(premises.end(), definition.begin(), definition.end());
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
    for (const Clauses &definition : definitions_)
      if (!erase(definition))
        return false;
    current_unit_.clear();
    for (Clauses &definition : definitions_)
      definition.clear();
    std::fill(operands_.begin(), operands_.end(),
              std::pair<External, External>{});
    retired_ = true;
    return true;
  }
};

} // namespace tactix::parity_certificate
