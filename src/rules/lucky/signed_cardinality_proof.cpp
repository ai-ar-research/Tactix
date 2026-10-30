#include "rules/lucky/signed_cardinality_proof.hpp"

#include "kernel/types.hpp"
#include "proof/cardinality_certificate.hpp"
#include "proof/proof.hpp"
#include "rules/lucky/root_guard.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace tactix {
namespace {

using External = int32_t;
using Clause = std::vector<External>;

struct Triple {
  std::array<External, 3> literals{};
  friend bool operator==(const Triple &, const Triple &) = default;
};

struct TripleHash {
  size_t operator()(const Triple &triple) const {
    uint64_t hash = 1469598103934665603ULL;
    for (const External literal : triple.literals) {
      hash ^= static_cast<uint32_t>(literal);
      hash *= 1099511628211ULL;
    }
    return static_cast<size_t>(hash);
  }
};

Triple triple_key(External a, External b, External c) {
  Triple result{{a, b, c}};
  std::sort(result.literals.begin(), result.literals.end());
  return result;
}

class Dsu {
  std::vector<uint32_t> parent_;
  std::vector<uint32_t> size_;

public:
  explicit Dsu(uint32_t size) : parent_(size), size_(size, 1) {
    std::iota(parent_.begin(), parent_.end(), 0);
  }

  uint32_t find(uint32_t value) {
    while (parent_[value] != value) {
      parent_[value] = parent_[parent_[value]];
      value = parent_[value];
    }
    return value;
  }

  void unite(uint32_t a, uint32_t b) {
    a = find(a);
    b = find(b);
    if (a == b)
      return;
    if (size_[a] < size_[b])
      std::swap(a, b);
    parent_[b] = a;
    size_[a] += size_[b];
  }
};

uint32_t signed_index(External literal, uint32_t variables) {
  const uint32_t variable = external_variable_of(literal);
  return literal > 0 ? variable - 1 : variables + variable - 1;
}

struct System {
  uint32_t variables = 0;
  std::vector<std::vector<External>> groups;
};

bool recognize(Kernel &k,
               const strategy::SignedCardinalityProofParameters &parameters,
               System &system) {
  if (!k.nvars || k.nvars > parameters.maximum_variables ||
      k.external_to_internal.size() != k.nvars ||
      k.input_unit_clause_count || k.input_binary_clause_count ||
      k.input_nonunit_clause_count > parameters.maximum_constraints)
    return false;

  const uint32_t variables = k.nvars;
  if (variables > static_cast<uint32_t>(INT32_MAX))
    return false;
  Dsu dsu(2 * variables);
  std::vector<uint8_t> touched(2 * variables, 0);
  std::unordered_set<Triple, TripleHash> triples;
  triples.reserve(static_cast<size_t>(k.input_nonunit_clause_count * 2 + 1));
  uint64_t clauses = 0;

  for (Ref ref = 0; ref < k.arena.size();) {
    const tactix::Clause *clause = clause_at(k.arena, ref);
    if (!clause->garbage() && !clause->redundant()) {
      if (clause->size != 3)
        return false;
      std::array<External, 3> external{};
      std::array<uint32_t, 3> absolute{};
      for (uint32_t i = 0; i < 3; ++i) {
        const Lit literal = clause->lits[i];
        const Var internal = var_of(literal);
        if (internal >= k.internal_to_external.size())
          return false;
        const uint64_t identity =
            uint64_t{k.internal_to_external[internal]} + 1;
        if (!identity || identity > variables ||
            identity > static_cast<uint64_t>(INT32_MAX))
          return false;
        external[i] = negated(literal) ? -static_cast<External>(identity)
                                       : static_cast<External>(identity);
        absolute[i] = static_cast<uint32_t>(identity);
      }
      if (absolute[0] == absolute[1] || absolute[0] == absolute[2] ||
          absolute[1] == absolute[2] ||
          !triples.insert(triple_key(external[0], external[1], external[2]))
               .second)
        return false;
      const uint32_t first = signed_index(external[0], variables);
      const uint32_t second = signed_index(external[1], variables);
      const uint32_t third = signed_index(external[2], variables);
      touched[first] = touched[second] = touched[third] = 1;
      dsu.unite(first, second);
      dsu.unite(first, third);
      ++clauses;
    }
    ref += clause_actual_words(clause);
    if (kernel_rule_interrupt(k,
                              k.stats.signed_cardinality_proof_interrupted))
      return false;
  }
  if (clauses != k.input_nonunit_clause_count)
    return false;

  std::unordered_map<uint32_t, uint32_t> component;
  component.reserve(2 * variables);
  system.variables = variables;
  system.groups.clear();
  for (uint32_t index = 0; index < 2 * variables; ++index) {
    if (!touched[index])
      return false;
    const uint32_t root = dsu.find(index);
    auto found = component.find(root);
    if (found == component.end()) {
      if (system.groups.size() == parameters.maximum_groups)
        return false;
      const uint32_t group = static_cast<uint32_t>(system.groups.size());
      component.emplace(root, group);
      system.groups.emplace_back();
      found = component.find(root);
    }
    const External variable = static_cast<External>(index % variables + 1);
    system.groups[found->second].push_back(index < variables ? variable
                                                            : -variable);
  }

  uint64_t expected = 0;
  uint64_t lower_bound = 0;
  for (std::vector<External> &group : system.groups) {
    std::sort(group.begin(), group.end());
    const uint32_t arity = static_cast<uint32_t>(group.size());
    if (arity < 3 || arity > parameters.maximum_group_arity)
      return false;
    const uint64_t combinations =
        uint64_t{arity} * (arity - 1) * (arity - 2) / 6;
    expected += combinations;
    lower_bound += arity - 2;
    if (expected > clauses)
      return false;
    for (uint32_t i = 0; i < arity; ++i)
      for (uint32_t j = i + 1; j < arity; ++j)
        for (uint32_t l = j + 1; l < arity; ++l)
          if (!triples.count(triple_key(group[i], group[j], group[l])))
            return false;
  }
  return expected == clauses && lower_bound > variables;
}

class Sink {
  Kernel &kernel_;
  bool writing_;
  const strategy::SignedCardinalityProofParameters &parameters_;

public:
  uint64_t additions = 0;
  uint64_t nodes = 0;
  uint32_t fresh;
  bool stopped = false;

  Sink(Kernel &kernel, bool writing,
       const strategy::SignedCardinalityProofParameters &parameters)
      : kernel_(kernel), writing_(writing), parameters_(parameters),
        fresh(static_cast<uint32_t>(kernel.external_to_internal.size())) {}

  bool poll() {
    if (!writing_ && !(additions & 4095) &&
        kernel_rule_interrupt(
            kernel_, kernel_.stats.signed_cardinality_proof_interrupted))
      stopped = true;
    return !stopped;
  }

  // The legacy signed-cardinality engine budgets nodes and additions only.
  // Preserve that contract while reusing the shared certificate machinery.
  bool charge_work(uint64_t) { return !stopped; }

  bool record_definition(External, External, int64_t, int64_t) {
    return !stopped;
  }

  bool new_variable(External &variable) {
    if (nodes == parameters_.maximum_bdd_nodes || fresh == INT32_MAX) {
      stopped = true;
      return false;
    }
    ++nodes;
    variable = static_cast<External>(++fresh);
    return true;
  }

  bool add(const Clause &clause, bool rat) {
    if (additions == parameters_.maximum_proof_additions) {
      stopped = true;
      return false;
    }
    ++additions;
    if (!poll() || !writing_ || !kernel_.proof)
      return !stopped;
    const uint64_t id = ++kernel_.next_clause_id;
    const bool ok = rat ? kernel_.proof->add_external_rat(
                              id, clause.data(), clause.size())
                        : kernel_.proof->add_external_rup(
                              id, clause.data(), clause.size());
    stopped |= !ok;
    return ok;
  }
};

using BddProof = cardinality_certificate::BddProof<Sink>;
struct Constraint {
  std::vector<std::pair<External, int64_t>> terms;
  int64_t bound = 0;
  uint32_t root = 0;
};

int64_t maximum_sum(const Constraint &constraint) {
  int64_t result = 0;
  for (const auto &[variable, coefficient] : constraint.terms) {
    (void)variable;
    result += std::max<int64_t>(0, coefficient);
  }
  return result;
}

int64_t minimum_sum(const Constraint &constraint) {
  int64_t result = 0;
  for (const auto &[variable, coefficient] : constraint.terms) {
    (void)variable;
    result += std::min<int64_t>(0, coefficient);
  }
  return result;
}

Constraint combine(const Constraint &positive, const Constraint &negative,
                   External pivot) {
  Constraint result;
  result.bound = positive.bound + negative.bound;
  size_t pi = 0, ni = 0;
  while (pi < positive.terms.size() || ni < negative.terms.size()) {
    const External pv = pi < positive.terms.size()
                            ? positive.terms[pi].first
                            : INT32_MAX;
    const External nv = ni < negative.terms.size()
                            ? negative.terms[ni].first
                            : INT32_MAX;
    const External variable = std::min(pv, nv);
    int64_t coefficient = 0;
    if (pv == variable)
      coefficient += positive.terms[pi++].second;
    if (nv == variable)
      coefficient += negative.terms[ni++].second;
    if (variable != pivot && coefficient)
      result.terms.push_back({variable, coefficient});
  }
  return result;
}

bool derive(const System &system, Sink &sink) {
  BddProof bdd(sink);
  std::vector<Constraint> constraints;
  constraints.reserve(system.groups.size() * 2);

  for (const std::vector<External> &group : system.groups) {
    std::vector<uint32_t> clause_roots;
    for (size_t i = 0; i < group.size(); ++i)
      for (size_t j = i + 1; j < group.size(); ++j)
        for (size_t l = j + 1; l < group.size(); ++l) {
          uint32_t root = 0;
          if (!bdd.validate_clause({group[i], group[j], group[l]}, root))
            return false;
          clause_roots.push_back(root);
        }
    const uint32_t conjunction = bdd.conjunction(std::move(clause_roots));
    if (sink.stopped)
      return false;

    Constraint constraint;
    constraint.bound = static_cast<int64_t>(group.size()) - 2;
    for (const External literal : group) {
      const External variable =
          static_cast<External>(external_variable_of(literal));
      if (literal > 0)
        constraint.terms.push_back({variable, 1});
      else {
        constraint.terms.push_back({variable, -1});
        --constraint.bound;
      }
    }
    std::sort(constraint.terms.begin(), constraint.terms.end());
    constraint.root = bdd.build_constraint(constraint.terms, constraint.bound);
    if (!bdd.imply_and_validate(conjunction, bdd.truth(), constraint.root))
      return false;
    constraints.push_back(std::move(constraint));
  }

  while (!constraints.empty() && !sink.stopped) {
    External pivot = 0;
    uint64_t best = UINT64_MAX;
    for (External variable = 1;
         variable <= static_cast<External>(system.variables); ++variable) {
      uint64_t positive = 0, negative = 0, positive_size = 0,
               negative_size = 0;
      for (const Constraint &constraint : constraints) {
        const auto found = std::lower_bound(
            constraint.terms.begin(), constraint.terms.end(), variable,
            [](const auto &term, External value) { return term.first < value; });
        if (found == constraint.terms.end() || found->first != variable)
          continue;
        if (found->second > 0) {
          ++positive;
          positive_size += constraint.terms.size() - 1;
        } else {
          ++negative;
          negative_size += constraint.terms.size() - 1;
        }
      }
      if (!positive && !negative)
        continue;
      const uint64_t score = negative * positive_size +
                             positive * negative_size;
      if (score < best) {
        best = score;
        pivot = variable;
      }
    }
    if (!pivot)
      return false;

    std::vector<size_t> positive, negative;
    std::vector<Constraint> retained;
    retained.reserve(constraints.size());
    for (size_t at = 0; at < constraints.size(); ++at) {
      const auto found = std::lower_bound(
          constraints[at].terms.begin(), constraints[at].terms.end(), pivot,
          [](const auto &term, External value) { return term.first < value; });
      if (found == constraints[at].terms.end() || found->first != pivot)
        retained.push_back(constraints[at]);
      else if (found->second > 0)
        positive.push_back(at);
      else
        negative.push_back(at);
      if (found != constraints[at].terms.end() && found->first == pivot &&
          std::abs(found->second) != 1)
        return false;
    }

    for (const size_t pi : positive)
      for (const size_t ni : negative) {
        Constraint derived = combine(constraints[pi], constraints[ni], pivot);
        derived.root = bdd.build_constraint(derived.terms, derived.bound);
        if (!bdd.imply_and_validate(constraints[pi].root,
                                    constraints[ni].root, derived.root))
          return false;
        if (maximum_sum(derived) < derived.bound)
          return derived.root == bdd.falsity() && !sink.stopped;
        if (minimum_sum(derived) < derived.bound)
          retained.push_back(std::move(derived));
      }
    constraints = std::move(retained);
  }
  return false;
}

} // namespace

RuleEligibility signed_cardinality_proof_guard(const Kernel &k) {
  RuleEligibility result = lucky_root_eligibility(k);
  return result.fact("proof-format", !k.proof || !k.proof->lrat());
}

ApplyResult signed_cardinality_proof_apply(Kernel &k) {
  assert(signed_cardinality_proof_guard(k));
  ++k.stats.signed_cardinality_proof_calls;
  const strategy::SignedCardinalityProofParameters parameters =
      k.signed_cardinality_proof_parameters;
  if (!parameters.valid())
    return ApplyResult::NoOp;

  System system;
  if (!recognize(k, parameters, system))
    return ApplyResult::NoOp;
  ++k.stats.signed_cardinality_proof_recognized;
  k.stats.signed_cardinality_proof_groups += system.groups.size();

  Sink preflight(k, false, parameters);
  if (!derive(system, preflight) || preflight.stopped) {
    if (preflight.stopped)
      ++k.stats.signed_cardinality_proof_budget_exhausted;
    return ApplyResult::NoOp;
  }
  if (k.proof) {
    Sink writer(k, true, parameters);
    if (!derive(system, writer) || writer.stopped ||
        writer.nodes != preflight.nodes ||
        writer.additions != preflight.additions)
      std::abort();
    k.stats.signed_cardinality_proof_bdd_nodes += writer.nodes;
    k.stats.signed_cardinality_proof_additions += writer.additions;
  } else {
    k.stats.signed_cardinality_proof_bdd_nodes += preflight.nodes;
    k.stats.signed_cardinality_proof_additions += preflight.additions;
  }
  k.inconsistent = true;
  ++k.stats.signed_cardinality_proof_unsat;
  return ApplyResult::Unsat;
}

} // namespace tactix
