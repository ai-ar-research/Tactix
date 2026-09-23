#include "rules/lucky/parity_refutation.hpp"

#include "proof/proof.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstdint>
#include <cstdarg>
#include <cstdlib>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tactix {
namespace {

using External = int32_t;

struct Scope {
  std::array<Var, 4> vars{};
  uint8_t size = 0;
  friend bool operator==(const Scope &, const Scope &) = default;
};

struct ScopeHash {
  size_t operator()(const Scope &scope) const {
    size_t hash = scope.size;
    for (uint32_t i = 0; i < scope.size; ++i)
      hash = hash * 0x9e3779b185ebca87ULL + scope.vars[i];
    return hash;
  }
};

struct Group {
  Scope scope;
  uint16_t masks = 0;
};

struct Equation {
  std::array<External, 4> variables{};
  uint8_t size = 0;
  bool rhs = false;
};

class Recognizer {
  Kernel &k_;
  const strategy::ParityModelParameters &parameters_;
  std::unordered_map<Scope, uint32_t, ScopeHash> index_;
  bool declined_ = false;
  bool probe_checked_ = false;
  uint64_t constraints_ = 0;
  uint32_t probe_repeats_ = 0;

  void check_probe() {
    if (probe_checked_ ||
        constraints_ < parameters_.refutation_probe_constraints)
      return;
    probe_checked_ = true;
    if (k_.input_nonunit_clause_count >
            parameters_.refutation_probe_constraints &&
        probe_repeats_ < parameters_.refutation_minimum_probe_repeats) {
      declined_ = true;
      ++k_.stats.parity_refutation_probe_declines;
    }
  }

public:
  std::vector<Group> groups;

  Recognizer(Kernel &k, const strategy::ParityModelParameters &parameters)
      : k_(k), parameters_(parameters) {
    const uint64_t reserve = std::min<uint64_t>(
        k.input_nonunit_clause_count,
        std::min<uint64_t>(parameters.refutation_maximum_groups,
                           parameters.refutation_probe_constraints));
    index_.reserve(static_cast<size_t>(reserve));
    groups.reserve(static_cast<size_t>(reserve));
  }

  bool add(const Lit *lits, uint32_t size) {
    if (declined_ || ++constraints_ >
                         parameters_.refutation_maximum_constraints)
      return false;
    if (size >= 2 && size <= parameters_.refutation_maximum_arity) {
      std::array<std::pair<Var, bool>, 4> ordered{};
      for (uint32_t i = 0; i < size; ++i)
        ordered[i] = {var_of(lits[i]), negated(lits[i])};
      std::sort(ordered.begin(), ordered.begin() + size);
      bool distinct = true;
      for (uint32_t i = 1; i < size; ++i)
        distinct &= ordered[i - 1].first != ordered[i].first;
      if (distinct) {
        Scope scope;
        scope.size = static_cast<uint8_t>(size);
        uint32_t mask = 0;
        for (uint32_t i = 0; i < size; ++i) {
          scope.vars[i] = ordered[i].first;
          mask |= uint32_t(ordered[i].second) << i;
        }
        auto found = index_.find(scope);
        if (found == index_.end()) {
          if (groups.size() == parameters_.refutation_maximum_groups)
            return false;
          const uint32_t at = static_cast<uint32_t>(groups.size());
          groups.push_back({scope, static_cast<uint16_t>(1u << mask)});
          index_.emplace(scope, at);
        } else {
          if (constraints_ <= parameters_.refutation_probe_constraints)
            ++probe_repeats_;
          groups[found->second].masks |= static_cast<uint16_t>(1u << mask);
        }
      }
    }
    check_probe();
    return !declined_;
  }

  bool finished() {
    check_probe();
    return !declined_;
  }
};

bool collect_equations(Kernel &k,
                       const strategy::ParityModelParameters &parameters,
                       std::vector<Equation> &equations) {
  if (k.nvars > parameters.refutation_maximum_variables ||
      k.input_unit_clause_count || k.trail.size() ||
      k.external_to_internal.size() != k.nvars)
    return false;
  Recognizer recognizer(k, parameters);
  uint64_t polled = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    const uint32_t words = clause_actual_words(clause);
    if (!clause->garbage() &&
        !recognizer.add(clause->lits, clause->size))
      return false;
    ref += words;
    if (!(++polled & 4095) &&
        kernel_rule_interrupt(k, k.stats.parity_model_interrupted))
      return false;
  }
  for (Lit first = 0; first < 2 * k.nvars; ++first) {
    for (const WatchEntry watch : k.watches.entries(first)) {
      if (!watch.binary() || first >= watch.other())
        continue;
      const Lit pair[2] = {first, watch.other()};
      if (!recognizer.add(pair, 2))
        return false;
    }
    if (!(++polled & 4095) &&
        kernel_rule_interrupt(k, k.stats.parity_model_interrupted))
      return false;
  }
  if (!recognizer.finished())
    return false;

  std::vector<uint8_t> odd(k.nvars, 0);
  bool total_rhs = false;
  equations.clear();
  equations.reserve(recognizer.groups.size());
  for (const Group &group : recognizer.groups) {
    const uint32_t expected = 1u << (group.scope.size - 1);
    if (static_cast<uint32_t>(std::popcount(group.masks)) != expected)
      continue;
    int forbidden_parity = -1;
    bool common = true;
    for (uint32_t mask = 0; mask < (1u << group.scope.size); ++mask) {
      if (!(group.masks & (1u << mask)))
        continue;
      const int parity = std::popcount(mask) & 1;
      if (forbidden_parity < 0)
        forbidden_parity = parity;
      else if (forbidden_parity != parity)
        common = false;
    }
    if (!common || forbidden_parity < 0)
      continue;
    Equation equation;
    equation.size = group.scope.size;
    equation.rhs = !forbidden_parity;
    total_rhs ^= equation.rhs;
    for (uint32_t i = 0; i < equation.size; ++i) {
      const Var internal = group.scope.vars[i];
      odd[internal] ^= 1;
      const uint64_t external = uint64_t(k.internal_to_external[internal]) + 1;
      if (external > static_cast<uint64_t>(INT32_MAX))
        return false;
      equation.variables[i] = static_cast<External>(external);
    }
    equations.push_back(equation);
  }
  if (equations.empty() || !total_rhs)
    return false;
  for (const uint8_t parity : odd)
    if (parity)
      return false;
  return true;
}

struct SmallClause {
  std::array<External, 16> lits{};
  uint8_t size = 0;
};

using Clauses = std::vector<SmallClause>;

Clauses xor_equation(const External *variables, uint32_t size, bool rhs) {
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

class Sink {
  Kernel &k_;
  bool writing_;
  uint64_t maximum_additions_;

public:
  uint64_t additions = 0;
  uint64_t deletions = 0;
  uint32_t fresh = 0;
  bool stopped = false;

  Sink(Kernel &k, bool writing, uint32_t original_variables,
       uint64_t maximum_additions)
      : k_(k), writing_(writing), maximum_additions_(maximum_additions),
        fresh(original_variables) {}

  bool add(const SmallClause &clause, bool rat) {
    if (additions == maximum_additions_) {
      stopped = true;
      return false;
    }
    if (!writing_ && !(additions & 4095) &&
        kernel_rule_interrupt(k_, k_.stats.parity_model_interrupted)) {
      stopped = true;
      return false;
    }
    ++additions;
    if (!writing_ || !k_.proof)
      return true;
    const uint64_t id = ++k_.next_clause_id;
    const bool ok = rat
                        ? k_.proof->add_external_rat(id, clause.lits.data(),
                                                    clause.size)
                        : k_.proof->add_external_rup(id, clause.lits.data(),
                                                    clause.size);
    stopped |= !ok;
    return ok;
  }

  bool erase(const SmallClause &clause) {
    ++deletions;
    if (!writing_ || !k_.proof)
      return true;
    const bool ok = k_.proof->del_external(k_.next_clause_id, 0,
                                           clause.lits.data(), clause.size);
    stopped |= !ok;
    return ok;
  }

  bool new_variable(External &variable) {
    if (fresh == static_cast<uint32_t>(INT32_MAX)) {
      stopped = true;
      return false;
    }
    variable = static_cast<External>(++fresh);
    return true;
  }
};

class Deriver {
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

class Certificate {
  Sink &sink_;
  uint32_t width_ = 1;
  std::vector<External> nodes_;
  std::vector<Clauses> definitions_;
  Clauses current_unit_;
  bool parity_ = false;
  bool emitted_empty_ = false;

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
  Certificate(Sink &sink, uint32_t variables) : sink_(sink) {
    while (width_ < variables)
      width_ <<= 1;
    nodes_.assign(2 * static_cast<size_t>(width_), 0);
    definitions_.resize(nodes_.size());
  }

  bool run(const std::vector<Equation> &equations) {
    Deriver deriver(sink_);
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
          return true;
        }
        next_unit.push_back(kept);
      }
      if (!erase(transitions) || !erase(current_unit_))
        return false;
      current_unit_ = std::move(next_unit);
    }
    return emitted_empty_;
  }
};

bool generate(Kernel &k, const std::vector<Equation> &equations,
              const strategy::ParityModelParameters &parameters, bool writing,
              uint64_t &additions, uint64_t &deletions, uint32_t &fresh) {
  Sink sink(k, writing, static_cast<uint32_t>(k.external_to_internal.size()),
            parameters.refutation_maximum_proof_additions);
  Certificate certificate(
      sink, static_cast<uint32_t>(k.external_to_internal.size()));
  const bool ok = certificate.run(equations) && !sink.stopped;
  additions = sink.additions;
  deletions = sink.deletions;
  fresh = sink.fresh;
  return ok;
}

// Threshold carry definitions represent floor(sum(x)/2). Unlike a clausal
// running parity tree, both bounds can be summed and rounded in PB directly.
// Local case splits have at most 2^arity leaves and cite no generator numbering.
class PbParityCertificate {
  Kernel &k_;
  FILE *out_;
  uint64_t initial_, id_, limit_;
  uint32_t fresh_;

  void text(const char *format, ...) {
    if (!out_)
      return;
    va_list args;
    va_start(args, format);
    std::vfprintf(out_, format, args);
    va_end(args);
  }
  void term(unsigned coefficient, int32_t literal) {
    text("%u %sx%u ", coefficient, literal < 0 ? "~" : "",
         static_cast<unsigned>(literal < 0 ? -literal : literal));
  }
  void block_assignments(const Equation &row, unsigned depth, unsigned mask) {
    // A complete XOR encoding propagates its last variable after n-1 are
    // fixed. The threshold definitions then force every carry, so the
    // negated bound conflicts without splitting that final variable.
    if (depth + 1 < row.size) {
      block_assignments(row, depth + 1, mask);
      block_assignments(row, depth + 1, mask | (1u << depth));
    }
    text("rup ");
    for (unsigned j = 0; j < depth; ++j)
      term(1, mask & (1u << j) ? -row.variables[j] : row.variables[j]);
    text(">= 1 ;\n");
    ++id_;
  }
  uint64_t bound(const Equation &row, const std::vector<int32_t> &carries,
                 bool lower) {
    text("pbc ");
    for (unsigned j = 0; j < row.size; ++j)
      term(1, lower ? row.variables[j] : -row.variables[j]);
    for (const int32_t carry : carries)
      term(2, lower ? -carry : carry);
    const unsigned rhs = lower ? unsigned(row.rhs) + 2 * carries.size()
                               : row.size - unsigned(row.rhs);
    text(">= %u : subproof\n", rhs);
    ++id_; // negated bound, local to the subproof
    block_assignments(row, 0, 0);
    text("qed pbc : -1 ;\n");
    return ++id_;
  }
  uint64_t sum_and_round(const std::vector<uint64_t> &ids) {
    text("pol %llu", static_cast<unsigned long long>(ids[0]));
    for (size_t i = 1; i < ids.size(); ++i)
      text(" %llu +", static_cast<unsigned long long>(ids[i]));
    text(" 2 d ;\n");
    return ++id_;
  }

public:
  PbParityCertificate(Kernel &k, FILE *out, uint64_t limit)
      : k_(k), out_(out),
        initial_(k.input_unit_clause_count + k.input_nonunit_clause_count),
        id_(initial_), limit_(limit),
        fresh_(static_cast<uint32_t>(k.external_to_internal.size())) {}

  uint64_t additions() const { return id_ - initial_; }

  bool run(const std::vector<Equation> &equations) {
    text("pseudo-Boolean proof version 3.0\n");
    std::vector<uint64_t> lower, upper;
    lower.reserve(equations.size());
    upper.reserve(equations.size());
    for (const Equation &row : equations) {
      // Input arity is validated by the shared recognizer. Each threshold
      // carry is defined by two inequalities, with an explicit repair proof
      // for the first inequality when the second sets the carry to one.
      std::vector<int32_t> carries;
      for (unsigned threshold = 2; threshold <= row.size; threshold += 2) {
        if (fresh_ == INT32_MAX)
          return false;
        const int32_t carry = static_cast<int32_t>(++fresh_);
        carries.push_back(carry);
        text("red ");
        for (unsigned j = 0; j < row.size; ++j)
          term(1, row.variables[j]);
        term(threshold, -carry);
        text(">= %u : x%d -> 0 ;\n", threshold, carry);
        const uint64_t first = ++id_;
        const unsigned complement = row.size - threshold + 1;
        text("red ");
        for (unsigned j = 0; j < row.size; ++j)
          term(1, -row.variables[j]);
        term(complement, carry);
        text(">= %u : x%d -> 1 : subproof\n", complement, carry);
        const uint64_t negated_second = ++id_;
        text("proofgoal #1\nqed ;\n");
        ++id_;
        text("proofgoal %llu\n", static_cast<unsigned long long>(first));
        const uint64_t negated_first = ++id_;
        text("pol %llu %llu + ;\nqed : -1 ;\nqed ;\n",
             static_cast<unsigned long long>(negated_second),
             static_cast<unsigned long long>(negated_first));
        id_ += 2; // contradiction and the completed second definition
      }
      lower.push_back(bound(row, carries, true));
      upper.push_back(bound(row, carries, false));
      if (additions() > limit_ ||
          (!out_ && kernel_rule_interrupt(k_, k_.stats.parity_model_interrupted)))
        return false;
    }
    const uint64_t lo = sum_and_round(lower), hi = sum_and_round(upper);
    text("pol %llu %llu + ;\n", static_cast<unsigned long long>(lo),
         static_cast<unsigned long long>(hi));
    ++id_;
    text("output NONE ;\nconclusion UNSAT : -1 ;\nend pseudo-Boolean proof ;\n");
    return additions() <= limit_ && (!out_ || !std::ferror(out_));
  }
};

} // namespace

RuleEligibility startup_parity_guard(const Kernel &k) {
  RuleEligibility result;
  return result.fact("consistent", !k.inconsistent)
      .fact("root", k.level == 0 && !k.conflict_valid)
      .fact("original-input-boundary", k.startup_certificate_open);
}

ApplyResult startup_parity_apply(Kernel &k) {
  if (!startup_parity_guard(k) ||
      (k.proof && !k.proof->startup_pb_available()))
    return ApplyResult::NoOp;
  const auto parameters = strategy::ParityModelParameters::from_kernel(k);
  if (!parameters.valid())
    return ApplyResult::NoOp;
  std::vector<Equation> equations;
  if (!collect_equations(k, parameters, equations))
    return ApplyResult::NoOp;
  ++k.stats.parity_refutation_recognized;
  PbParityCertificate preflight(k, nullptr,
                                parameters.refutation_maximum_proof_additions);
  if (!preflight.run(equations))
    return ApplyResult::NoOp;
  if (k.proof) {
    FILE *out = k.proof->begin_pb();
    if (!out)
      return ApplyResult::NoOp;
    PbParityCertificate proof(k, out,
                              parameters.refutation_maximum_proof_additions);
    if (!proof.run(equations) || proof.additions() != preflight.additions() ||
        !k.proof->finish_pb())
      std::abort(); // published but incomplete certificates never claim UNSAT
  }
  k.stats.parity_refutation_proof_additions += preflight.additions();
  ++k.stats.parity_refutation_unsat;
  k.close_startup_certificate();
  k.inconsistent = true;
  return ApplyResult::Unsat;
}

ApplyResult parity_refutation_apply(
    Kernel &k, const strategy::ParityModelParameters &parameters) {
  if (!parameters.valid() || (k.proof && k.proof->lrat()))
    return ApplyResult::NoOp;
  std::vector<Equation> equations;
  if (!collect_equations(k, parameters, equations))
    return ApplyResult::NoOp;
  ++k.stats.parity_refutation_recognized;

  if (k.proof) {
    uint64_t preflight_additions = 0, preflight_deletions = 0;
    uint32_t preflight_fresh = 0;
    if (!generate(k, equations, parameters, false, preflight_additions,
                  preflight_deletions, preflight_fresh))
      return ApplyResult::NoOp;
    uint64_t additions = 0, deletions = 0;
    uint32_t fresh = 0;
    // Emission is terminal and atomic with respect to solver scheduling.  Stop
    // requests are honored throughout recognition and preflight; once bytes are
    // written, finish the already-bounded proof rather than expose a prefix.
    if (!generate(k, equations, parameters, true, additions, deletions, fresh) ||
        additions != preflight_additions || deletions != preflight_deletions ||
        fresh != preflight_fresh)
      std::abort();
    k.stats.parity_refutation_proof_additions += additions;
    k.stats.parity_refutation_proof_deletions += deletions;
  }
  k.inconsistent = true;
  ++k.stats.parity_refutation_unsat;
  return ApplyResult::Unsat;
}

} // namespace tactix
