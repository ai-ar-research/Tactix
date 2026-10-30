#include "rules/lucky/exact_one_refutation.hpp"
#include "rules/lucky/refutation_proof_sink.hpp"

#include "proof/proof.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace tactix {
namespace {

using External = int32_t;
using ExternalClause = std::vector<External>;
using ExternalClauses = std::vector<ExternalClause>;

uint64_t pair_key(Var a, Var b) {
  if (b < a)
    std::swap(a, b);
  return (uint64_t{a} << 32) | b;
}

struct ExactOneGroup {
  std::vector<Var> variables;
};

struct ExactOneSystem {
  std::vector<std::pair<ExternalClause, uint8_t>> equations;
};

bool recognize(Kernel &k, const strategy::OnehotModelParameters &parameters,
               ExactOneSystem &system) {
  const bool captured = k.input_binary_order_captured;
  if (k.external_to_internal.size() != k.nvars ||
      k.nvars > parameters.refutation_maximum_variables ||
      (captured &&
       (k.input_nonunit_clause_count >
            parameters.refutation_maximum_constraints ||
        k.input_nonunit_clause_count < k.input_binary_clause_count)))
    return false;

  std::vector<ExactOneGroup> groups;
  if (captured)
    groups.reserve(static_cast<size_t>(std::min<uint64_t>(
        parameters.refutation_maximum_groups,
        k.input_nonunit_clause_count)));
  uint64_t large = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    if (!clause->garbage() && !clause->redundant()) {
      ++large;
      if ((!captured &&
           large > parameters.refutation_maximum_constraints) ||
          clause->size < 2 ||
          clause->size > parameters.refutation_maximum_arity ||
          !std::all_of(clause->begin(), clause->end(),
                       [](Lit lit) { return !negated(lit); }) ||
          groups.size() == parameters.refutation_maximum_groups)
        return false;
      ExactOneGroup group;
      group.variables.reserve(clause->size);
      for (const Lit lit : *clause)
        group.variables.push_back(var_of(lit));
      std::sort(group.variables.begin(), group.variables.end());
      if (std::adjacent_find(group.variables.begin(), group.variables.end()) !=
          group.variables.end())
        return false;
      groups.push_back(std::move(group));
    }
    ref += clause_actual_words(clause);
    if (kernel_rule_interrupt(k, k.stats.exact_one_refutation_interrupted))
      return false;
  }
  if (captured && large !=
                      k.input_nonunit_clause_count -
                          k.input_binary_clause_count)
    return false;

  std::vector<uint64_t> live_binary_order;
  if (!captured) {
    for (Lit first = 0; first < 2 * k.nvars; ++first) {
      for (const WatchEntry watch : k.watches.entries(first)) {
        if (!watch.binary() || first >= watch.other())
          continue;
        if (large + live_binary_order.size() ==
            parameters.refutation_maximum_constraints)
          return false;
        live_binary_order.push_back(kernel_binary_key(first, watch.other()));
      }
      if (kernel_rule_interrupt(k, k.stats.exact_one_refutation_interrupted))
        return false;
    }
  }
  const std::vector<uint64_t> &binary_order =
      captured ? k.input_binary_order : live_binary_order;
  std::unordered_set<uint64_t> negative_pairs;
  negative_pairs.reserve(binary_order.size());
  for (const uint64_t encoded : binary_order) {
    const Lit first = static_cast<Lit>(encoded >> 32);
    const Lit second = static_cast<Lit>(encoded);
    if (!negated(first) && !negated(second)) {
      if (groups.size() == parameters.refutation_maximum_groups)
        return false;
      ExactOneGroup group;
      group.variables = {var_of(first), var_of(second)};
      if (group.variables[0] == group.variables[1])
        return false;
      if (group.variables[1] < group.variables[0])
        std::swap(group.variables[0], group.variables[1]);
      groups.push_back(std::move(group));
    } else if (negated(first) && negated(second)) {
      const Var a = var_of(first), b = var_of(second);
      if (a == b || !negative_pairs.insert(pair_key(a, b)).second)
        return false;
    } else {
      return false;
    }
  }
  if (groups.size() < 3)
    return false;

  std::vector<std::vector<uint32_t>> incidence(k.nvars);
  for (uint32_t group = 0; group < groups.size(); ++group) {
    const std::vector<Var> &variables = groups[group].variables;
    for (const Var variable : variables)
      incidence[variable].push_back(group);
    for (size_t i = 0; i < variables.size(); ++i)
      for (size_t j = i + 1; j < variables.size(); ++j)
        if (!negative_pairs.count(pair_key(variables[i], variables[j])))
          return false;
  }
  for (const auto &occurrences : incidence)
    if (!occurrences.empty() && occurrences.size() != 2)
      return false;

  std::vector<int8_t> color(groups.size(), -1);
  std::vector<uint32_t> selected;
  for (uint32_t start = 0; start < groups.size() && selected.empty(); ++start) {
    if (color[start] >= 0)
      continue;
    std::queue<uint32_t> pending;
    std::vector<uint32_t> component;
    uint32_t counts[2] = {0, 0};
    color[start] = 0;
    pending.push(start);
    while (!pending.empty()) {
      const uint32_t group = pending.front();
      pending.pop();
      component.push_back(group);
      ++counts[color[group]];
      for (const Var variable : groups[group].variables) {
        for (const uint32_t other : incidence[variable]) {
          if (other == group)
            continue;
          if (color[other] < 0) {
            color[other] = color[group] ^ 1;
            pending.push(other);
          } else if (color[other] == color[group]) {
            return false;
          }
        }
      }
    }
    const int difference = static_cast<int>(counts[0]) -
                           static_cast<int>(counts[1]);
    if ((difference % 3 + 3) % 3)
      selected = std::move(component);
  }
  if (selected.empty())
    return false;

  std::sort(selected.begin(), selected.end(), [&](uint32_t a, uint32_t b) {
    return color[a] < color[b] || (color[a] == color[b] && a < b);
  });
  system.equations.clear();
  system.equations.reserve(selected.size());
  for (const uint32_t group : selected) {
    ExternalClause variables;
    variables.reserve(groups[group].variables.size());
    for (const Var internal : groups[group].variables) {
      const uint64_t external = uint64_t{k.internal_to_external[internal]} + 1;
      if (external > static_cast<uint64_t>(INT32_MAX))
        return false;
      variables.push_back(static_cast<External>(external));
    }
    std::sort(variables.begin(), variables.end());
    system.equations.push_back(
        {std::move(variables), static_cast<uint8_t>(color[group] ? 2 : 1)});
  }
  return true;
}

using Sink = RefutationProofSink<ExternalClause>;

class Deriver {
  Sink &sink_;
  ExternalClauses local_;
  ExternalClauses emitted_;
  std::vector<External> variables_;
  std::vector<External> order_;

  int index(External variable) const {
    variable = std::abs(variable);
    const auto found =
        std::lower_bound(variables_.begin(), variables_.end(), variable);
    return found == variables_.end() || *found != variable
               ? -1
               : static_cast<int>(found - variables_.begin());
  }

  bool propagate(const ExternalClause &candidate,
                 std::vector<int8_t> &assignment) const {
    assignment.assign(variables_.size(), 0);
    for (const External literal : candidate) {
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
      for (const ExternalClause &clause : local_) {
        bool satisfied = false;
        uint32_t unassigned = 0;
        External last = 0;
        for (const External literal : clause) {
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

  bool visit(ExternalClause &candidate) {
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
      if (!branch || candidate.size() == variables_.size())
        return false;
      candidate.push_back(branch);
      if (!visit(candidate))
        return false;
      candidate.back() = -branch;
      if (!visit(candidate))
        return false;
      candidate.pop_back();
      if (!propagate(candidate, assignment))
        return false;
    }
    if (!sink_.add(candidate))
      return false;
    local_.push_back(candidate);
    emitted_.push_back(candidate);
    return true;
  }

public:
  explicit Deriver(Sink &sink) : sink_(sink) {}

  bool derive(const ExternalClause &target, const ExternalClauses &premises,
              const std::vector<External> &priority, ExternalClause &kept) {
    local_ = premises;
    emitted_.clear();
    variables_.clear();
    for (const ExternalClause &clause : local_)
      for (const External literal : clause)
        variables_.push_back(std::abs(literal));
    for (const External literal : target)
      variables_.push_back(std::abs(literal));
    std::sort(variables_.begin(), variables_.end());
    variables_.erase(std::unique(variables_.begin(), variables_.end()),
                     variables_.end());
    if (variables_.size() > 20)
      return false;
    order_.clear();
    for (External variable : priority) {
      variable = std::abs(variable);
      if (variable &&
          std::find(order_.begin(), order_.end(), variable) == order_.end())
        order_.push_back(variable);
    }
    for (const External variable : variables_)
      if (std::find(order_.begin(), order_.end(), variable) == order_.end())
        order_.push_back(variable);
    ExternalClause candidate = target;
    if (!visit(candidate))
      return false;
    kept = target;
    for (size_t i = 0; i + 1 < emitted_.size(); ++i)
      if (!sink_.erase(emitted_[i]))
        return false;
    return true;
  }
};

struct State {
  External low = 0;
  External high = 0;
};

using Assignment = std::unordered_map<External, bool>;

int state_value(const State state, const Assignment &assignment) {
  const int low = state.low && assignment.at(state.low) ? 1 : 0;
  const int high = state.high && assignment.at(state.high) ? 2 : 0;
  return low + high;
}

template <class Predicate>
ExternalClauses forbidden_cnf(std::vector<External> variables,
                              Predicate forbidden) {
  variables.erase(std::remove(variables.begin(), variables.end(), 0),
                  variables.end());
  std::sort(variables.begin(), variables.end());
  variables.erase(std::unique(variables.begin(), variables.end()),
                  variables.end());
  ExternalClauses result;
  const uint64_t assignments = uint64_t{1} << variables.size();
  for (uint64_t mask = 0; mask < assignments; ++mask) {
    Assignment assignment;
    for (size_t i = 0; i < variables.size(); ++i)
      assignment.emplace(variables[i], (mask >> i) & 1);
    if (!forbidden(assignment))
      continue;
    ExternalClause clause;
    clause.reserve(variables.size());
    for (size_t i = 0; i < variables.size(); ++i)
      clause.push_back((mask >> i) & 1 ? -variables[i] : variables[i]);
    result.push_back(std::move(clause));
  }
  return result;
}

ExternalClauses transition(const State old_state, const State new_state,
                           External variable, uint8_t coefficient) {
  return forbidden_cnf(
      {old_state.low, old_state.high, new_state.low, new_state.high, variable},
      [=](const Assignment &assignment) {
        const int old_value = state_value(old_state, assignment) % 3;
        const int new_value = state_value(new_state, assignment) % 3;
        const int bit = assignment.at(variable) ? 1 : 0;
        return new_value != (old_value + coefficient * bit) % 3;
      });
}

class Certificate {
  Sink &sink_;
  Deriver deriver_;
  uint32_t width_ = 1;
  std::vector<State> nodes_;
  std::vector<ExternalClauses> definitions_;
  std::vector<uint8_t> coefficients_;
  ExternalClauses current_state_;
  uint8_t right_hand_side_ = 0;
  bool emitted_empty_ = false;

  bool erase(const ExternalClauses &clauses) {
    for (const ExternalClause &clause : clauses)
      if (!sink_.erase(clause))
        return false;
    return true;
  }

  State leaf(External variable) const {
    const uint8_t coefficient = coefficients_[variable];
    if (!coefficient)
      return {};
    return coefficient == 1 ? State{variable, 0} : State{0, variable};
  }

  ExternalClauses bit_definition(State left, State right, External output,
                                 bool high) {
    return forbidden_cnf(
        {left.low, left.high, right.low, right.high, output},
        [=](const Assignment &assignment) {
          const int sum =
              (state_value(left, assignment) + state_value(right, assignment)) %
              3;
          const bool expected = high ? sum == 2 : sum == 1;
          return assignment.at(output) != expected;
        });
  }

  bool gate(State left, State right, State &output,
            ExternalClauses &definitions) {
    definitions.clear();
    if (!left.low && !left.high) {
      output = right;
      return true;
    }
    if (!right.low && !right.high) {
      output = left;
      return true;
    }
    if (!sink_.new_variable(output.low) || !sink_.new_variable(output.high))
      return false;
    ExternalClauses low = bit_definition(left, right, output.low, false);
    ExternalClauses high = bit_definition(left, right, output.high, true);
    for (ExternalClause clause : low) {
      const auto pivot =
          std::find_if(clause.begin(), clause.end(), [&](External literal) {
            return std::abs(literal) == output.low;
          });
      assert(pivot != clause.end());
      std::iter_swap(clause.begin(), pivot);
      if (!sink_.add(clause, true))
        return false;
      definitions.push_back(std::move(clause));
    }
    for (ExternalClause clause : high) {
      const auto pivot =
          std::find_if(clause.begin(), clause.end(), [&](External literal) {
            return std::abs(literal) == output.high;
          });
      assert(pivot != clause.end());
      std::iter_swap(clause.begin(), pivot);
      if (!sink_.add(clause, true))
        return false;
      definitions.push_back(std::move(clause));
    }
    return true;
  }

  bool add_equation(const ExternalClause &variables, uint8_t coefficient) {
    ExternalClauses transitions;
    for (const External variable : variables) {
      uint32_t at = width_ + static_cast<uint32_t>(variable) - 1;
      State old_child = nodes_[at];
      coefficients_[variable] =
          static_cast<uint8_t>((coefficients_[variable] + coefficient) % 3);
      nodes_[at] = leaf(variable);
      ExternalClauses relation =
          transition(old_child, nodes_[at], variable, coefficient);
      ExternalClauses old_definitions;
      while (at > 1) {
        const uint32_t parent = at >> 1;
        const uint32_t sibling = at ^ 1;
        const State old_parent = nodes_[parent];
        State new_parent;
        ExternalClauses new_definitions;
        if (!gate(nodes_[at], nodes_[sibling], new_parent, new_definitions))
          return false;
        ExternalClauses premises = relation;
        premises.insert(premises.end(), definitions_[parent].begin(),
                        definitions_[parent].end());
        premises.insert(premises.end(), new_definitions.begin(),
                        new_definitions.end());
        const ExternalClauses targets =
            transition(old_parent, new_parent, variable, coefficient);
        ExternalClauses next_relation;
        next_relation.reserve(targets.size());
        for (const ExternalClause &target : targets) {
          ExternalClause kept;
          if (!deriver_.derive(
                  target, premises,
                  {variable, old_child.low, old_child.high}, kept))
            return false;
          next_relation.push_back(std::move(kept));
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

    right_hand_side_ =
        static_cast<uint8_t>((right_hand_side_ + coefficient) % 3);
    ExternalClauses premises;
    premises.push_back(variables);
    for (size_t i = 0; i < variables.size(); ++i)
      for (size_t j = i + 1; j < variables.size(); ++j)
        premises.push_back({-variables[i], -variables[j]});
    premises.insert(premises.end(), transitions.begin(), transitions.end());
    premises.insert(premises.end(), current_state_.begin(),
                    current_state_.end());
    const State root = nodes_[1];
    const ExternalClauses targets = forbidden_cnf(
        {root.low, root.high}, [&](const Assignment &assignment) {
          return state_value(root, assignment) % 3 != right_hand_side_;
        });
    ExternalClauses next_state;
    for (const ExternalClause &target : targets) {
      ExternalClause kept;
      if (!deriver_.derive(target, premises, variables, kept))
        return false;
      next_state.push_back(kept);
      premises.push_back(std::move(kept));
      if (target.empty()) {
        emitted_empty_ = true;
        return true;
      }
    }
    if (!erase(transitions) || !erase(current_state_))
      return false;
    current_state_ = std::move(next_state);
    return true;
  }

public:
  Certificate(Sink &sink, uint32_t variables)
      : sink_(sink), deriver_(sink), coefficients_(variables + 1, 0) {
    while (width_ < variables)
      width_ <<= 1;
    nodes_.resize(2 * static_cast<size_t>(width_));
    definitions_.resize(nodes_.size());
  }

  bool run(const ExactOneSystem &system) {
    for (const auto &[variables, coefficient] : system.equations)
      if (!add_equation(variables, coefficient))
        return false;
      else if (emitted_empty_)
        return true;
    return false;
  }
};

bool generate(Kernel &k, const ExactOneSystem &system,
              const strategy::OnehotModelParameters &parameters, bool writing,
              uint64_t &additions, uint64_t &deletions, uint32_t &fresh) {
  Sink sink(k, writing, static_cast<uint32_t>(k.external_to_internal.size()),
            parameters.refutation_maximum_proof_additions,
            k.stats.exact_one_refutation_interrupted);
  Certificate certificate(
      sink, static_cast<uint32_t>(k.external_to_internal.size()));
  const bool ok = certificate.run(system) && !sink.stopped;
  additions = sink.additions;
  deletions = sink.deletions;
  fresh = sink.fresh;
  return ok;
}

} // namespace

ApplyResult exact_one_refutation_apply(
    Kernel &k, const strategy::OnehotModelParameters &parameters) {
  ++k.stats.exact_one_refutation_calls;
  if (!parameters.valid() || (k.proof && k.proof->lrat()))
    return ApplyResult::NoOp;
  ExactOneSystem system;
  if (!recognize(k, parameters, system))
    return ApplyResult::NoOp;
  ++k.stats.exact_one_refutation_recognized;
  k.stats.exact_one_refutation_equations += system.equations.size();

  if (k.proof) {
    uint64_t preflight_additions = 0, preflight_deletions = 0;
    uint32_t preflight_fresh = 0;
    if (!generate(k, system, parameters, false, preflight_additions,
                  preflight_deletions, preflight_fresh))
      return ApplyResult::NoOp;
    uint64_t additions = 0, deletions = 0;
    uint32_t fresh = 0;
    if (!generate(k, system, parameters, true, additions, deletions, fresh) ||
        additions != preflight_additions || deletions != preflight_deletions ||
        fresh != preflight_fresh)
      std::abort();
    k.stats.exact_one_refutation_proof_additions += additions;
    k.stats.exact_one_refutation_proof_deletions += deletions;
  }
  k.inconsistent = true;
  ++k.stats.exact_one_refutation_unsat;
  return ApplyResult::Unsat;
}

} // namespace tactix
