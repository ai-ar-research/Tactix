#include "rules/lucky/count_preservation.hpp"

#include "kernel/types.hpp"

#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstdlib>
#include <numeric>
#include <tuple>
#include <vector>

namespace tactix {
namespace {
using Triple = std::array<int32_t, 3>;
using Pair = std::array<int32_t, 2>;
constexpr uint32_t absent = UINT32_MAX;
uint32_t variable(int32_t literal) {
  return external_variable_of(literal) - 1;
}
struct Edge {
  int32_t selector, a, b;
  uint32_t left = absent, right = absent;
};
struct Star {
  uint32_t leaf, row;
  std::vector<uint32_t> edges;
  int sign = 0;
};
struct Plan {
  std::vector<std::vector<int32_t>> rows;
  std::vector<Edge> edges;
  std::vector<Star> stars;
  std::vector<int8_t> flip, units;
  std::vector<uint32_t> chosen;
};
struct Work {
  Kernel &k;
  uint64_t used = 0;
  bool tick(uint64_t amount = 1) {
    used += amount;
    k.stats.counting_work += amount;
    return !kernel_work_budget_exceeded(k, used) &&
           !kernel_rule_interrupt(k, k.stats.counting_interrupted);
  }
};

// A signed union-find orients all selected leaf literals consistently. It is
// a recognition restriction, not a substitution of the original formula.
struct SignedSets {
  std::vector<uint32_t> parent, size;
  std::vector<uint8_t> parity;
  explicit SignedSets(uint32_t n) : parent(n), size(n, 1), parity(n, 0) {
    std::iota(parent.begin(), parent.end(), 0);
  }
  std::pair<uint32_t, uint8_t> find(uint32_t v) {
    if (parent[v] == v)
      return {v, 0};
    const auto [root, value] = find(parent[v]);
    parity[v] ^= value;
    parent[v] = root;
    return {root, parity[v]};
  }
  bool join(uint32_t a, uint32_t b, uint8_t value) {
    auto [ra, pa] = find(a);
    auto [rb, pb] = find(b);
    if (ra == rb)
      return (pa ^ pb) == value;
    if (size[ra] < size[rb])
      std::swap(ra, rb);
    parent[rb] = ra;
    parity[rb] = pa ^ pb ^ value;
    size[ra] += size[rb];
    return true;
  }
};

template<class Function>
bool clauses(const Kernel &k, Work &work, Function visit) {
  std::vector<int32_t> clause;
  uint64_t count = 0;
  for (const int32_t literal : k.startup_input) {
    if (literal) {
      clause.push_back(literal);
      continue;
    }
    std::sort(clause.begin(), clause.end());
    clause.erase(std::unique(clause.begin(), clause.end()), clause.end());
    bool tautology = false;
    for (const int32_t lit : clause)
      if (std::binary_search(clause.begin(), clause.end(), -lit)) {
        tautology = true;
        break;
      }
    if (!tautology && !visit(clause))
      return false;
    clause.clear();
    if (!(++count & 4095) && !work.tick(4096))
      return false;
  }
  return clause.empty() && work.tick(count & 4095);
}

bool discover(Kernel &k, Work &work, Plan &plan) {
  const size_t variables = k.external_to_internal.size();
  if (variables > INT32_MAX)
    return false;
  std::vector<Pair> binaries;
  std::vector<Triple> ternaries;
  plan.units.assign(variables, 0);
  if (!clauses(k, work, [&](const auto &clause) {
        if (clause.size() == 1) {
          const auto v = variable(clause[0]);
          const int8_t value = clause[0] > 0 ? 1 : -1;
          if (plan.units[v] && plan.units[v] != value)
            return false;
          plan.units[v] = value;
        } else if (clause.size() == 2) {
          binaries.push_back({clause[0], clause[1]});
        } else if (clause.size() == 3) {
          ternaries.push_back({clause[0], clause[1], clause[2]});
        }
        return true;
      }))
    return false;
  std::sort(binaries.begin(), binaries.end());
  binaries.erase(std::unique(binaries.begin(), binaries.end()), binaries.end());
  std::sort(ternaries.begin(), ternaries.end());
  ternaries.erase(std::unique(ternaries.begin(), ternaries.end()), ternaries.end());
  std::vector<uint8_t> selector_literal(2 * variables, 0);
  std::vector<Edge> candidates;
  for (const Triple &triple : ternaries) {
    for (unsigned guard = 0; guard < 3; ++guard) {
      Triple mate = triple;
      std::array<int32_t, 2> leaves{};
      unsigned n = 0;
      for (unsigned at = 0; at < 3; ++at)
        if (at != guard) {
          leaves[n++] = triple[at];
          mate[at] = -mate[at];
        }
      std::sort(mate.begin(), mate.end());
      if (mate <= triple || !std::binary_search(ternaries.begin(), ternaries.end(), mate))
        continue;
      const int32_t selector = -triple[guard];
      selector_literal[internalize(selector)] = 1;
      candidates.push_back({selector, -leaves[0], leaves[1]});
    }
    if (!work.tick())
      return false;
  }
  if (candidates.empty())
    return false;
  if (!clauses(k, work, [&](const auto &clause) {
        if (clause.size() < 2)
          return true;
        for (const int32_t lit : clause)
          if (!selector_literal[internalize(lit)])
            return true;
        for (size_t i = 0; i < clause.size(); ++i)
          for (size_t j = 0; j < i; ++j) {
            Pair pair{-clause[i], -clause[j]};
            std::sort(pair.begin(), pair.end());
            if (!std::binary_search(binaries.begin(), binaries.end(), pair))
              return true;
            if (!work.tick())
              return false;
          }
        plan.rows.push_back(clause);
        return true;
      }))
    return false;
  std::sort(plan.rows.begin(), plan.rows.end());
  plan.rows.erase(std::unique(plan.rows.begin(), plan.rows.end()), plan.rows.end());
  if (plan.rows.empty() || plan.rows.size() >= absent)
    return false;
  std::vector<std::vector<uint32_t>> memberships(2 * variables);
  std::vector<uint8_t> selector_variable(variables, 0);
  for (uint32_t row = 0; row < plan.rows.size(); ++row)
    for (const int32_t lit : plan.rows[row]) {
      memberships[internalize(lit)].push_back(row);
      selector_variable[variable(lit)] = 1;
    }
  for (const Edge &edge : candidates)
    if (!memberships[internalize(edge.selector)].empty() &&
        !selector_variable[variable(edge.a)] &&
        !selector_variable[variable(edge.b)])
      plan.edges.push_back(edge);
  if (plan.edges.empty() || plan.edges.size() >= absent ||
      plan.edges.size() + variables > INT32_MAX)
    return false;
  // Group each leaf's incident equality edges by a complete selector row.
  std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> incidence;
  for (uint32_t i = 0; i < plan.edges.size(); ++i) {
    const Edge &edge = plan.edges[i];
    for (const uint32_t row : memberships[internalize(edge.selector)]) {
      incidence.emplace_back(variable(edge.a), row, i);
      incidence.emplace_back(variable(edge.b), row, i);
    }
    if (!work.tick())
      return false;
  }
  std::sort(incidence.begin(), incidence.end());
  for (size_t begin = 0, end; begin < incidence.size(); begin = end) {
    const auto [leaf, row, unused] = incidence[begin];
    (void)unused;
    end = begin + 1;
    while (end < incidence.size() && std::get<0>(incidence[end]) == leaf &&
           std::get<1>(incidence[end]) == row)
      ++end;
    if (end - begin != plan.rows[row].size())
      continue;
    Star star{leaf, row, {}, 0};
    std::vector<int32_t> selectors;
    for (size_t at = begin; at < end; ++at) {
      const uint32_t edge = std::get<2>(incidence[at]);
      star.edges.push_back(edge);
      selectors.push_back(plan.edges[edge].selector);
    }
    std::sort(selectors.begin(), selectors.end());
    if (selectors != plan.rows[row])
      continue;
    if (plan.stars.size() >= absent)
      return false;
    const uint32_t id = static_cast<uint32_t>(plan.stars.size());
    for (const uint32_t at : star.edges) {
      Edge &edge = plan.edges[at];
      uint32_t &endpoint = variable(edge.a) == leaf ? edge.left : edge.right;
      if (endpoint != absent)
        return false; // ambiguous complete-row ownership is not guessed
      endpoint = id;
    }
    plan.stars.push_back(std::move(star));
    if (!work.tick())
      return false;
  }
  std::vector<std::vector<uint32_t>> leaf_stars(variables);
  SignedSets orientation(static_cast<uint32_t>(variables));
  for (const Edge &edge : plan.edges) {
    if (edge.left == absent && edge.right == absent)
      continue;
    if (edge.left == absent || edge.right == absent ||
        !orientation.join(variable(edge.a), variable(edge.b),
                          (edge.a < 0) != (edge.b < 0)))
      return false;
  }
  plan.flip.resize(variables);
  for (uint32_t v = 0; v < variables; ++v)
    plan.flip[v] = orientation.find(v).second;
  for (uint32_t i = 0; i < plan.stars.size(); ++i) {
    auto &list = leaf_stars[plan.stars[i].leaf];
    list.push_back(i);
    if (list.size() > 2)
      return false;
  }
  for (uint32_t seed = 0; seed < plan.stars.size(); ++seed) {
    if (plan.stars[seed].sign)
      continue;
    std::vector<uint32_t> component{seed};
    plan.stars[seed].sign = 1;
    int64_t boundary = 0;
    bool fixed_boundary = true;
    for (size_t head = 0; head < component.size(); ++head) {
      const uint32_t at = component[head];
      const Star &star = plan.stars[at];
      const auto connect = [&](uint32_t other) {
        if (!plan.stars[other].sign) {
          plan.stars[other].sign = -star.sign;
          component.push_back(other);
        }
        return plan.stars[other].sign == -star.sign;
      };
      for (const uint32_t index : star.edges) {
        const Edge &edge = plan.edges[index];
        if (!connect(edge.left == at ? edge.right : edge.left))
          return false;
      }
      const auto &same_leaf = leaf_stars[star.leaf];
      if (same_leaf.size() == 2) {
        if (!connect(same_leaf[0] == at ? same_leaf[1] : same_leaf[0]))
          return false;
      } else if (!plan.units[star.leaf]) {
        fixed_boundary = false;
      } else {
        const bool truth = (plan.units[star.leaf] > 0) != bool(plan.flip[star.leaf]);
        boundary += star.sign * int(truth);
      }
      if (!work.tick())
        return false;
    }
    if (fixed_boundary && boundary) {
      if (boundary < 0)
        for (const uint32_t at : component)
          plan.stars[at].sign = -plan.stars[at].sign;
      plan.chosen = std::move(component);
      return true;
    }
  }
  return false;
}

class Certificate {
  Kernel &k_;
  const Plan &p_;
  FILE *out_;
  uint64_t id_, initial_;
  void emit(const char *format, ...) {
    if (!out_)
      return;
    va_list args;
    va_start(args, format);
    std::vfprintf(out_, format, args);
    va_end(args);
  }
  void term(int32_t lit) {
    emit("1 %sx%u ", lit < 0 ? "~" : "", variable(lit) + 1);
  }
  uint64_t clause(const std::vector<int32_t> &lits) {
    emit("rup ");
    for (int32_t lit : lits)
      term(lit);
    emit(">= 1 ;\n");
    return ++id_;
  }
  uint64_t sum(const std::vector<uint64_t> &ids, uint64_t scale = 1,
               uint64_t divide = 1) {
    emit("pol %llu", (unsigned long long)ids[0]);
    if (scale != 1)
      emit(" %llu *", (unsigned long long)scale);
    for (size_t at = 1; at < ids.size(); ++at)
      emit(" %llu +", (unsigned long long)ids[at]);
    if (divide != 1)
      emit(" %llu d", (unsigned long long)divide);
    emit(" ;\n");
    return ++id_;
  }
  int32_t leaf(const Star &star) const {
    const int32_t value = static_cast<int32_t>(star.leaf + 1);
    return p_.flip[star.leaf] ? -value : value;
  }
  // Definition z <=> (s & x). The explicit repair subproofs concern only
  // the two earlier clauses containing fresh z, never the original variables.
  uint64_t conjunction(int32_t z, int32_t s, int32_t x) {
    emit("red "); term(-z); term(s);
    emit(">= 1 : x%d -> 0 ;\n", z);
    const uint64_t first = ++id_;
    emit("red "); term(-z); term(x);
    emit(">= 1 : x%d -> 0 ;\n", z);
    const uint64_t second = ++id_;
    emit("red "); term(z); term(-s); term(-x);
    emit(">= 1 : x%d -> 1 : subproof\n", z);
    const uint64_t negated = ++id_;
    emit("proofgoal #1\nqed ;\n");
    ++id_;
    for (uint64_t goal : {first, second}) {
      emit("proofgoal %llu\n", (unsigned long long)goal);
      const uint64_t negative_goal = ++id_;
      const uint64_t contradiction = sum({negated, negative_goal});
      emit("qed : %llu ;\n", (unsigned long long)contradiction);
    }
    emit("qed ;\n");
    ++id_;
    return first;
  }
public:
  Certificate(Kernel &k, const Plan &p, FILE *out)
      : k_(k), p_(p), out_(out),
        id_(k.input_unit_clause_count + k.input_nonunit_clause_count), initial_(id_) {}
  uint64_t additions() const { return id_ - initial_; }
  bool run(Work *work) {
    emit("pseudo-Boolean proof version 3.0\n");
    std::vector<uint64_t> row_amo(p_.rows.size(), 0), z_to_selector(p_.edges.size(), 0);
    std::vector<int32_t> carries(p_.edges.size(), 0);
    uint32_t fresh = static_cast<uint32_t>(k_.external_to_internal.size());
    for (uint32_t at : p_.chosen) {
      const Star &star = p_.stars[at];
      if (star.sign < 0)
        continue;
      for (uint32_t index : star.edges) {
        const Edge &edge = p_.edges[index];
        const int32_t z = static_cast<int32_t>(++fresh);
        carries[index] = z;
        z_to_selector[index] = conjunction(z, edge.selector, leaf(star));
        if (work && !work->tick(9))
          return false;
      }
    }
    std::vector<uint64_t> bounds;
    std::vector<int> coefficient(p_.units.size(), 0);
    for (uint32_t at : p_.chosen) {
      const Star &star = p_.stars[at];
      coefficient[star.leaf] += star.sign;
      if (star.sign > 0) {
        std::vector<int32_t> lits{-leaf(star)};
        for (uint32_t edge : star.edges)
          lits.push_back(carries[edge]);
        bounds.push_back(clause(lits));
      } else {
        const auto &row = p_.rows[star.row];
        uint64_t &amo = row_amo[star.row];
        if (!amo) {
          amo = clause({-row[0], -row[1]});
          // Clique AMO induction: (n-2)*AMO(n-1) plus the n-1 new
          // pairs, divided by n-1, yields AMO(n) by integer rounding.
          for (size_t n = 3; n <= row.size(); ++n) {
            std::vector<uint64_t> refs{amo};
            for (size_t j = 0; j + 1 < n; ++j)
              refs.push_back(clause({-row[j], -row[n-1]}));
            amo = sum(refs, n - 2, n - 1);
          }
        }
        std::vector<uint64_t> refs{amo};
        for (uint32_t edge : star.edges)
          refs.push_back(z_to_selector[edge]);
        const uint64_t z_amo = sum(refs);
        refs = {z_amo};
        for (uint32_t edge : star.edges)
          refs.push_back(clause({-carries[edge], leaf(star)}));
        bounds.push_back(sum(refs, row.size() - 1, row.size()));
      }
      if (work && !work->tick(star.edges.size() + 1))
        return false;
    }
    for (uint32_t v = 0; v < coefficient.size(); ++v)
      if (coefficient[v]) {
        if (std::abs(coefficient[v]) != 1 || !p_.units[v])
          return false;
        const int32_t literal = p_.units[v] > 0 ? int32_t(v + 1) : -int32_t(v + 1);
        // Only the unit opposite the uncancelled literal is needed. If the
        // bound already contains a true literal it is replaced by its axiom.
        const int32_t signed_leaf = p_.flip[v] ? -int32_t(v + 1) : int32_t(v + 1);
        const int32_t in_bound = coefficient[v] > 0 ? -signed_leaf : signed_leaf;
        if (literal == -in_bound)
          bounds.push_back(clause({literal}));
        else {
          // Adding the complementary Boolean axiom cancels a true literal
          // without increasing the RHS (pol literal denotes literal >= 0).
          emit("pol %sx%u ;\n", in_bound > 0 ? "~" : "", v + 1);
          bounds.push_back(++id_);
        }
      }
    sum(bounds);
    emit("output NONE ;\nconclusion UNSAT : -1 ;\nend pseudo-Boolean proof ;\n");
    return !out_ || !std::ferror(out_);
  }
};
} // namespace

RuleEligibility count_preservation_guard(const Kernel &k) {
  RuleEligibility result;
  return result.fact("consistent", !k.inconsistent)
      .fact("root", k.level == 0 && !k.conflict_valid)
      .fact("original-input-boundary", k.startup_certificate_open);
}
ApplyResult count_preservation_apply(Kernel &k) {
  if (!count_preservation_guard(k) || k.trail.empty() ||
      k.startup_input.empty() || (k.proof && !k.proof->startup_pb_available()))
    return ApplyResult::NoOp;
  ++k.stats.counting_calls;
  Work work{k};
  Plan plan;
  if (!discover(k, work, plan))
    return ApplyResult::NoOp;
  Certificate preflight(k, plan, nullptr);
  if (!preflight.run(&work))
    return ApplyResult::NoOp;
  if (k.proof) {
    FILE *out = k.proof->begin_pb();
    if (!out)
      return ApplyResult::NoOp;
    Certificate proof(k, plan, out);
    if (!proof.run(nullptr) || proof.additions() != preflight.additions() ||
        !k.proof->finish_pb())
      std::abort();
  }
  k.stats.counting_stars += plan.chosen.size();
  k.stats.counting_additions += preflight.additions();
  ++k.stats.counting_unsat;
  k.close_startup_certificate();
  k.inconsistent = true;
  return ApplyResult::Unsat;
}
} // namespace tactix
