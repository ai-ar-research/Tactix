#include "rules/lucky/opportunities.hpp"
#include "rules/lucky/parity_certificate.hpp"
#include "rules/lucky/refutation_proof_sink.hpp"
#include "rules/lucky/startup_pb_writer.hpp"

#include "kernel/types.hpp"
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

template <unsigned MaximumArity> struct Scope {
  std::array<Var, MaximumArity> vars{};
  uint8_t size = 0;
  friend bool operator==(const Scope &, const Scope &) = default;
};

template <unsigned MaximumArity> struct ScopeHash {
  size_t operator()(const Scope<MaximumArity> &scope) const {
    size_t hash = scope.size;
    for (uint32_t i = 0; i < scope.size; ++i)
      hash = hash * 0x9e3779b185ebca87ULL + scope.vars[i];
    return hash;
  }
};

template <unsigned MaximumArity> struct Group {
  Scope<MaximumArity> scope;
  static constexpr unsigned WORD_BITS = 16;
  std::array<uint16_t, (1u << MaximumArity) / WORD_BITS> masks{};

  void add(unsigned mask) {
    masks[mask / WORD_BITS] |= 1u << (mask % WORD_BITS);
  }
  bool contains(unsigned mask) const {
    return masks[mask / WORD_BITS] & (1u << (mask % WORD_BITS));
  }
  unsigned count() const {
    unsigned total = 0;
    for (const uint16_t word : masks)
      total += std::popcount(word);
    return total;
  }
};

using parity_certificate::Equation;
using parity_certificate::SmallClause;
using parity_certificate::Clauses;
using parity_certificate::Certificate;

template <unsigned MaximumArity> class Recognizer {
  Kernel &k_;
  const strategy::ParityModelParameters &parameters_;
  std::unordered_map<Scope<MaximumArity>, uint32_t, ScopeHash<MaximumArity>> index_;
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
  std::vector<Group<MaximumArity>> groups;

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
      std::array<std::pair<Var, bool>, MaximumArity> ordered{};
      for (uint32_t i = 0; i < size; ++i)
        ordered[i] = {var_of(lits[i]), negated(lits[i])};
      std::sort(ordered.begin(), ordered.begin() + size);
      bool distinct = true;
      for (uint32_t i = 1; i < size; ++i)
        distinct &= ordered[i - 1].first != ordered[i].first;
      if (distinct) {
        Scope<MaximumArity> scope;
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
          groups.push_back({scope});
          index_.emplace(scope, at);
          groups.back().add(mask);
        } else {
          if (constraints_ <= parameters_.refutation_probe_constraints)
            ++probe_repeats_;
          groups[found->second].add(mask);
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

template <unsigned MaximumArity>
bool collect_equations_bounded(Kernel &k,
                       const strategy::ParityModelParameters &parameters,
                       std::vector<Equation> &equations) {
  if (k.nvars > parameters.refutation_maximum_variables ||
      k.input_unit_clause_count || k.trail.size() ||
      k.external_to_internal.size() != k.nvars)
    return false;
  Recognizer<MaximumArity> recognizer(k, parameters);
  uint64_t polled = 0;
  for (const auto [ref, clause] : arena_records(k.arena)) {
    if (!clause->garbage() &&
        !recognizer.add(clause->lits, clause->size))
      return false;
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
  for (const auto &group : recognizer.groups) {
    const uint32_t expected = 1u << (group.scope.size - 1);
    if (group.count() != expected)
      continue;
    int forbidden_parity = -1;
    bool common = true;
    for (uint32_t mask = 0; mask < (1u << group.scope.size); ++mask) {
      if (!group.contains(mask))
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

bool collect_equations(Kernel &k,
                       const strategy::ParityModelParameters &parameters,
                       std::vector<Equation> &equations) {
  if (parameters.refutation_maximum_arity <= 4)
    return collect_equations_bounded<4>(k, parameters, equations);
  return collect_equations_bounded<8>(k, parameters, equations);
}

// Threshold carries represent floor(sum(x)/2), summed and rounded directly in
// PB; local splits have at most 2^arity leaves.
class PbParityCertificate : public StartupPbWriter {
  Kernel &k_;
  uint64_t limit_;
  uint32_t fresh_;

  void block_assignments(const Equation &row, unsigned depth, unsigned mask) {
    // Once n-1 variables are fixed the encoding and carries force the bound to
    // conflict without splitting the last one.
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

public:
  PbParityCertificate(Kernel &k, FILE *out, uint64_t limit)
      : StartupPbWriter(k, out), k_(k), limit_(limit),
        fresh_(static_cast<uint32_t>(k.external_to_internal.size())) {}

  bool run(const std::vector<Equation> &equations) {
    header();
    return append(equations, 0, fresh_);
  }

  bool append(const std::vector<Equation> &equations, uint64_t prefix_steps,
              uint32_t newest_variable) {
    id_ += prefix_steps;
    fresh_ = newest_variable;
    std::vector<uint64_t> lower, upper;
    lower.reserve(equations.size());
    upper.reserve(equations.size());
    for (const Equation &row : equations) {
      // Each carry has two inequalities; the first gets a repair proof when the
      // second sets the carry.
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
    // Sum each bound family and round by two.
    const uint64_t lo = sum(lower, 1, 2), hi = sum(upper, 1, 2);
    text("pol %llu %llu + ;\n", static_cast<unsigned long long>(lo),
         static_cast<unsigned long long>(hi));
    ++id_;
    footer();
    return additions() <= limit_ && (!out_ || !std::ferror(out_));
  }
};

} // namespace

bool write_startup_pb_parity_tail(Kernel &k, FILE *out,
                                 const std::vector<PbpParityEquation> &rows,
                                 uint64_t prefix_steps, uint32_t newest_variable,
                                 uint64_t limit, uint64_t &additions) {
  if (rows.empty() || prefix_steps > limit)
    return false;
  std::vector<Equation> equations;
  for (const auto &row : rows) {
    if (!valid_parity_source(row, newest_variable))
      return false;
    Equation &equation = equations.emplace_back();
    equation.size = static_cast<uint8_t>(row.variables.size());
    equation.rhs = row.rhs;
    std::copy(row.variables.begin(), row.variables.end(), equation.variables.begin());
  }
  PbParityCertificate certificate(k, out, limit);
  if (!certificate.append(equations, prefix_steps, newest_variable))
    return false;
  additions = certificate.additions();
  return true;
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
    std::vector<PbpParityEquation> rows;
    rows.reserve(equations.size());
    for (const Equation &equation : equations)
      rows.push_back({{equation.variables.begin(),
                       equation.variables.begin() + equation.size},
                      equation.rhs});
    const auto write_pb = [&](FILE *out) {
      PbParityCertificate proof(k, out,
                                parameters.refutation_maximum_proof_additions);
      return proof.run(equations) && proof.additions() == preflight.additions();
    };
    if (!k.proof->refute_parity(rows, write_pb))
      return ApplyResult::NoOp;
  }
  k.stats.parity_refutation_proof_additions += preflight.additions();
  ++k.stats.parity_refutation_unsat;
  k.close_startup_certificate();
  k.inconsistent = true;
  return ApplyResult::Unsat;
}

ApplyResult parity_refutation_apply(
    Kernel &k, const strategy::ParityModelParameters &parameters) {
  if (!parameters.valid())
    return ApplyResult::NoOp;
  std::vector<Equation> equations;
  if (!collect_equations(k, parameters, equations))
    return ApplyResult::NoOp;
  ++k.stats.parity_refutation_recognized;

  if (!refute_with_preflight<SmallClause>(
          k, parameters.refutation_maximum_proof_additions,
          k.stats.parity_model_interrupted,
          k.stats.parity_refutation_proof_additions,
          k.stats.parity_refutation_proof_deletions,
          [&](auto &sink, uint32_t variables) {
            Certificate certificate(sink, variables);
            return certificate.run(equations);
          }))
    return ApplyResult::NoOp;
  k.inconsistent = true;
  ++k.stats.parity_refutation_unsat;
  return ApplyResult::Unsat;
}

} // namespace tactix
