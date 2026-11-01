#include "rules/lucky/parity_refutation.hpp"
#include "rules/lucky/parity_certificate.hpp"
#include "rules/lucky/refutation_proof_sink.hpp"

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

using parity_certificate::Equation;
using parity_certificate::SmallClause;
using parity_certificate::Clauses;
using parity_certificate::Certificate;

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

using Sink = RefutationProofSink<SmallClause>;

bool generate(Kernel &k, const std::vector<Equation> &equations,
              const strategy::ParityModelParameters &parameters, bool writing,
              uint64_t &additions, uint64_t &deletions, uint32_t &fresh) {
  Sink sink(k, writing, static_cast<uint32_t>(k.external_to_internal.size()),
            parameters.refutation_maximum_proof_additions,
            k.stats.parity_model_interrupted);
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
         external_variable_of(literal));
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
