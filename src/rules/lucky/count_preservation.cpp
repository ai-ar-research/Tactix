#include "rules/lucky/opportunities.hpp"
#include "rules/lucky/startup_pb_writer.hpp"

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
  uint64_t limit = UINT64_MAX;
  bool tick(uint64_t amount = 1) {
    if (amount > limit - used)
      return false;
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

// Visits every non-tautological startup clause sorted and duplicate-free.
// `keep` sees raw literals first and may skip clauses with no effect; every
// clause still advances the work-tick count.
template<class Keep, class Function>
bool clauses(const Kernel &k, Work &work, Keep keep, Function visit) {
  std::vector<int32_t> clause;
  uint64_t count = 0;
  for (const int32_t literal : k.startup_input) {
    if (literal) {
      clause.push_back(literal);
      continue;
    }
    if (keep(clause)) {
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
    }
    clause.clear();
    if (!(++count & 4095) && !work.tick(4096))
      return false;
  }
  return clause.empty() && work.tick(count & 4095);
}

// Normalization keeps every distinct literal, so a raw clause with four of
// them can never become one of the short clauses the first pass records.
bool at_most_three_distinct(const std::vector<int32_t> &clause) {
  if (clause.size() <= 3)
    return true;
  int32_t seen[3] = {};
  unsigned distinct = 0;
  for (const int32_t literal : clause) {
    if (std::find(seen, seen + distinct, literal) != seen + distinct)
      continue;
    if (distinct == 3)
      return false;
    seen[distinct++] = literal;
  }
  return true;
}

bool discover(Kernel &k, Work &work, Plan &plan) {
  const size_t variables = k.external_to_internal.size();
  if (variables > INT32_MAX)
    return false;
  std::vector<Pair> binaries;
  std::vector<Triple> ternaries;
  binaries.reserve(k.input_binary_clause_count);
  ternaries.reserve(k.input_ternary_clause_count);
  plan.units.assign(variables, 0);
  if (!clauses(k, work, at_most_three_distinct, [&](const auto &clause) {
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
  const auto is_selector = [&](int32_t lit) {
    return selector_literal[internalize(lit)] != 0;
  };
  // Only selector-row lookups (pairs of negated selectors) need the binaries,
  // so they are sorted after the candidate check.
  binaries.erase(std::remove_if(binaries.begin(), binaries.end(),
                                [&](const Pair &pair) {
                                  return !is_selector(-pair[0]) ||
                                         !is_selector(-pair[1]);
                                }),
                 binaries.end());
  std::sort(binaries.begin(), binaries.end());
  binaries.erase(std::unique(binaries.begin(), binaries.end()), binaries.end());
  // A raw clause shorter than two or with a non-selector literal returns
  // before any tick.
  const auto selector_row = [&](const std::vector<int32_t> &clause) {
    return clause.size() >= 2 &&
           std::all_of(clause.begin(), clause.end(), is_selector);
  };
  if (!clauses(k, work, selector_row, [&](const auto &clause) {
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

using PositiveGroups = std::vector<std::vector<int32_t>>;

bool select_disjoint_demands(PositiveGroups &rows, uint32_t variables) {
  std::vector<std::vector<size_t>> memberships(variables);
  bool overlaps = false;
  for (size_t row = 0; row < rows.size(); ++row) {
    for (int32_t lit : rows[row]) {
      auto &list = memberships[variable(lit)];
      list.push_back(row);
      overlaps |= list.size() > 1;
    }
  }
  if (!overlaps)
    return true;

  std::vector<std::vector<size_t>> neighbors(rows.size());
  for (const auto &list : memberships) {
    if (list.empty())
      continue;
    if (list.size() != 2)
      return false;
    neighbors[list[0]].push_back(list[1]);
    neighbors[list[1]].push_back(list[0]);
  }
  std::vector<int8_t> color(rows.size(), -1);
  std::vector<size_t> selected;
  for (size_t root = 0; root < rows.size(); ++root) {
    if (color[root] >= 0)
      continue;
    std::array<std::vector<size_t>, 2> sides;
    std::vector<size_t> pending{root};
    color[root] = 0;
    for (size_t at = 0; at < pending.size(); ++at) {
      const size_t row = pending[at];
      sides[color[row]].push_back(row);
      for (size_t other : neighbors[row]) {
        if (color[other] == color[row])
          return false;
        if (color[other] < 0) {
          color[other] = 1 - color[row];
          pending.push_back(other);
        }
      }
    }
    const auto &side = sides[sides[0].size() < sides[1].size()];
    selected.insert(selected.end(), side.begin(), side.end());
  }
  std::sort(selected.begin(), selected.end());
  PositiveGroups disjoint;
  for (size_t row : selected)
    disjoint.push_back(std::move(rows[row]));
  rows = std::move(disjoint);
  return true;
}

// Extract disjoint at-least-one demands and partition their literals into
// at-most-one cliques. Binary implication paths justify each conflict by RUP.
bool matching_obstruction(const Kernel &k, Work &work, PositiveGroups &demands,
                          PositiveGroups &pools) {
  constexpr size_t variable_limit = 8192;
  constexpr uint32_t choice_limit = 4096;
  constexpr size_t pool_width = 8;
  // Startup input retains original external ids, which may be sparse. The
  // temporary graph only needs a slot for variables that occur in the CNF.
  std::vector<int32_t> external;
  for (int32_t lit : k.startup_input)
    if (lit)
      external.push_back(static_cast<int32_t>(external_variable_of(lit)));
  if (!work.tick(external.size()))
    return false;
  std::sort(external.begin(), external.end());
  external.erase(std::unique(external.begin(), external.end()), external.end());
  if (external.size() > variable_limit)
    return false;
  const auto variables = static_cast<uint32_t>(external.size());
  const auto dense_literal = [&](int32_t lit) {
    const int32_t key = static_cast<int32_t>(external_variable_of(lit));
    const auto at = std::lower_bound(external.begin(), external.end(), key);
    assert(at != external.end() && *at == key);
    const auto local = static_cast<int32_t>(at - external.begin() + 1);
    return lit > 0 ? local : -local;
  };
  std::vector<bool> false_unit(variables, false);
  std::vector<std::vector<uint32_t>> implication(2 * variables);
  const auto index = [](int32_t lit) { return 2 * variable(lit) + (lit < 0); };
  if (!clauses(k, work, [](const auto &) { return true; }, [&](const auto &row) {
        if (row.empty())
          return false;
        const bool positive = std::all_of(row.begin(), row.end(), [](int32_t lit) {
          return lit > 0;
        });
        if (positive) {
          auto &demand = demands.emplace_back();
          demand.reserve(row.size());
          for (int32_t lit : row)
            demand.push_back(dense_literal(lit));
        } else if (row.size() == 1) {
          false_unit[variable(dense_literal(row[0]))] = true;
        } else if (row.size() == 2) {
          const int32_t a = dense_literal(row[0]), b = dense_literal(row[1]);
          implication[index(-a)].push_back(index(b));
          implication[index(-b)].push_back(index(a));
        }
        return work.tick(row.size());
      }))
    return false;
  size_t occurrences = 0;
  for (auto &row : demands) {
    std::erase_if(row, [&](int32_t lit) { return false_unit[variable(lit)]; });
    if (row.empty())
      return false;
    occurrences += row.size();
  }
  if (!occurrences || occurrences > 2 * choice_limit ||
      !select_disjoint_demands(demands, variables))
    return false;
  std::vector<size_t> owner(variables, SIZE_MAX);
  std::vector<int32_t> priority;
  for (size_t row = 0; row < demands.size(); ++row) {
    for (int32_t lit : demands[row]) {
      const uint32_t v = variable(lit);
      if (owner[v] != SIZE_MAX || priority.size() == choice_limit)
        return false;
      owner[v] = row;
      priority.push_back(lit);
    }
  }
  std::vector<uint32_t> seen(implication.size(), 0), queue;
  std::vector<std::vector<int32_t>> conflicts(variables);
  for (int32_t seed : priority) {
    const uint32_t v = variable(seed), epoch = static_cast<uint32_t>(seed);
    queue.assign(1, index(seed));
    seen[queue.front()] = epoch;
    for (size_t at = 0; at < queue.size(); ++at) {
      const uint32_t literal = queue[at];
      if (!work.tick(1 + implication[literal].size()))
        return false;
      if (literal == index(-seed))
        return false;
      const uint32_t other = literal / 2;
      if ((literal & 1) && owner[other] != SIZE_MAX && owner[other] != owner[v])
        conflicts[v].push_back(static_cast<int32_t>(other + 1));
      for (uint32_t next : implication[literal]) {
        if (seen[next] == epoch)
          continue;
        seen[next] = epoch;
        queue.push_back(next);
      }
    }
    std::sort(conflicts[v].begin(), conflicts[v].end());
  }
  std::sort(priority.begin(), priority.end(), [&](int32_t a, int32_t b) {
    const size_t da = conflicts[variable(a)].size(), db = conflicts[variable(b)].size();
    return da == db ? a < b : da > db;
  });
  std::vector<bool> used(variables, false);
  for (int32_t seed : priority) {
    if (used[variable(seed)])
      continue;
    used[variable(seed)] = true;
    auto &pool = pools.emplace_back(1, seed);
    auto usable = conflicts[variable(seed)];
    std::erase_if(usable, [&](int32_t lit) { return used[variable(lit)]; });
    while (!usable.empty() && pool.size() < pool_width) {
      if (!work.tick(uint64_t{usable.size()} * usable.size()))
        return false;
      int32_t best = usable.front();
      size_t count = 0;
      for (int32_t candidate : usable) {
        const auto &adjacent = conflicts[variable(candidate)];
        const size_t degree = std::count_if(usable.begin(), usable.end(), [&](int32_t lit) {
          return std::binary_search(adjacent.begin(), adjacent.end(), lit);
        });
        if (degree > count) {
          best = candidate;
          count = degree;
        }
      }
      pool.push_back(best);
      used[variable(best)] = true;
      const auto &adjacent = conflicts[variable(best)];
      std::erase_if(usable, [&](int32_t lit) {
        return !std::binary_search(adjacent.begin(), adjacent.end(), lit);
      });
    }
    std::sort(pool.begin(), pool.end());
  }
  if (demands.size() <= pools.size())
    return false;
  for (auto *groups : {&demands, &pools})
    for (auto &group : *groups)
      for (int32_t &lit : group)
        lit = external[variable(lit)];
  return true;
}

class MatchingCertificate : public StartupPbWriter {
public:
  using StartupPbWriter::StartupPbWriter;
  static bool write(Kernel &k, FILE *out, const PositiveGroups &demands,
                    const PositiveGroups &pools, uint64_t &additions,
                    const std::vector<int32_t> &failed = {}) {
    MatchingCertificate proof(k, out);
    proof.header();
    // Each failed literal's negation is RUP from the input and earlier units,
    // which in turn makes every shortened demand RUP.
    for (int32_t lit : failed)
      proof.clause({-lit});
    std::vector<uint64_t> bounds;
    bounds.reserve(demands.size() + pools.size());
    for (const auto &demand : demands)
      bounds.push_back(proof.clause(demand));
    for (const auto &pool : pools)
      bounds.push_back(proof.at_most_one(pool));
    proof.sum(bounds);
    proof.footer();
    additions = proof.additions();
    return !std::ferror(out);
  }
};

bool refute_matching_count(Kernel &k) {
  if (k.startup_input.size() > 131072)
    return false;
  Work work{k, 0, 2'000'000};
  PositiveGroups demands, pools;
  if (!matching_obstruction(k, work, demands, pools) || !work.tick())
    return false;
  uint64_t additions = 0;
  if (k.proof && !k.proof->refute_by_counting([&](FILE *out) {
        return MatchingCertificate::write(k, out, demands, pools, additions);
      }, k.startup_input))
    return false;
  k.stats.counting_additions += additions;
  ++k.stats.counting_matching_unsat;
  ++k.stats.counting_unsat;
  k.close_startup_certificate();
  k.inconsistent = true;
  return true;
}

// Unit propagation over the original unit and binary clauses, enough to
// license every RUP step of a caller-selected matching certificate.
class BinaryPropagation {
  std::vector<std::vector<uint32_t>> implication_;
  std::vector<uint8_t> root_true_;
  std::vector<uint32_t> seen_, queue_;
  uint32_t epoch_ = 0;
  static uint32_t index(int32_t lit) {
    return 2 * external_variable_of(lit) + (lit < 0);
  }

public:
  explicit BinaryPropagation(uint32_t variables)
      : implication_(2 * (size_t{variables} + 1)),
        root_true_(implication_.size(), 0), seen_(implication_.size(), 0) {}
  void binary(int32_t a, int32_t b) {
    implication_[index(-a)].push_back(index(b));
    implication_[index(-b)].push_back(index(a));
  }
  void assert_root(int32_t lit) { root_true_[index(lit)] = 1; }
  bool root_false(int32_t lit) const { return root_true_[index(lit) ^ 1]; }
  // True when assuming every literal of `assumed` propagates to a conflict.
  bool conflicts(std::initializer_list<int32_t> assumed, Work &work) {
    if (++epoch_ == 0) {
      std::fill(seen_.begin(), seen_.end(), 0);
      epoch_ = 1;
    }
    queue_.clear();
    for (int32_t lit : assumed)
      if (seen_[index(lit)] != epoch_) {
        seen_[index(lit)] = epoch_;
        queue_.push_back(index(lit));
      }
    for (size_t at = 0; at < queue_.size(); ++at) {
      const uint32_t lit = queue_[at];
      if (root_true_[lit ^ 1] || seen_[lit ^ 1] == epoch_)
        return true;
      if (!work.tick(1 + implication_[lit].size()))
        return false;
      for (uint32_t next : implication_[lit])
        if (seen_[next] != epoch_) {
          seen_[next] = epoch_;
          queue_.push_back(next);
        }
    }
    return false;
  }
};

std::vector<int32_t> normalized(std::vector<int32_t> lits) {
  std::sort(lits.begin(), lits.end());
  lits.erase(std::unique(lits.begin(), lits.end()), lits.end());
  return lits;
}

// Validate a caller's demands, failed literals and pools against the original
// input; on success `demands` holds the shortened demands, in caller order.
bool selected_matching_obstruction(const Kernel &k, Work &work,
                                   const MatchingSelection &selection,
                                   PositiveGroups &demands, PositiveGroups &pools) {
  constexpr uint32_t variable_limit = 1u << 24;
  uint32_t variables = 0;
  for (int32_t lit : k.startup_input)
    variables = std::max(variables, external_variable_of(lit));
  if (!work.tick(k.startup_input.size()) || variables > variable_limit)
    return false;
  const auto in_range = [&](int32_t lit) {
    return lit && lit != INT32_MIN && external_variable_of(lit) <= variables;
  };
  BinaryPropagation propagation(variables);
  PositiveGroups inputs;
  if (!clauses(k, work, [](const auto &) { return true; }, [&](const auto &row) {
        if (row.size() == 1)
          propagation.assert_root(row[0]);
        else if (row.size() == 2)
          propagation.binary(row[0], row[1]);
        inputs.push_back(row);
        return work.tick(row.size());
      }))
    return false;
  std::sort(inputs.begin(), inputs.end());
  const auto is_input = [&](const std::vector<int32_t> &row) {
    return std::binary_search(inputs.begin(), inputs.end(), row);
  };
  for (int32_t lit : selection.failed) {
    if (!in_range(lit) || !propagation.conflicts({lit}, work))
      return false;
    propagation.assert_root(-lit);
  }
  // owner[v]: 1 + the demand holding v, signed by its literal's polarity.
  std::vector<int64_t> owner(size_t{variables} + 1, 0);
  size_t uncovered = 0;
  for (const auto &given : selection.demands) {
    if (given.empty() || !std::all_of(given.begin(), given.end(), in_range))
      return false;
    const auto row = normalized(given);
    if (!work.tick(row.size()) || !is_input(row))
      return false;
    auto &demand = demands.emplace_back();
    for (int32_t lit : row) {
      if (propagation.root_false(lit))
        continue;
      auto &slot = owner[external_variable_of(lit)];
      if (slot)
        return false;
      slot = lit > 0 ? int64_t(demands.size()) : -int64_t(demands.size());
      demand.push_back(lit);
    }
    if (demand.empty())
      return false;
    uncovered += demand.size();
  }
  std::vector<bool> covered(size_t{variables} + 1, false);
  for (const auto &given : selection.pools) {
    if (given.empty() || !std::all_of(given.begin(), given.end(), in_range))
      return false;
    const auto pool = normalized(given);
    for (size_t i = 0; i < pool.size(); ++i) {
      const int32_t a = pool[i];
      const uint32_t v = external_variable_of(a);
      if (!owner[v] || (owner[v] > 0) != (a > 0) || covered[v])
        return false;
      covered[v] = true;
      --uncovered;
      for (size_t j = 0; j < i; ++j) {
        const int32_t b = pool[j];
        if (!work.tick(2) ||
            (!is_input(normalized({-a, -b})) && !propagation.conflicts({a, b}, work)))
          return false;
      }
    }
    pools.push_back(pool);
  }
  return !uncovered && demands.size() > pools.size();
}

bool refute_selected_matching(Kernel &k, const MatchingSelection &selection) {
  Work work{k};
  PositiveGroups demands, pools;
  if (!selected_matching_obstruction(k, work, selection, demands, pools) || !work.tick())
    return false;
  uint64_t additions = 0;
  if (k.proof && !k.proof->refute_by_counting([&](FILE *out) {
        return MatchingCertificate::write(k, out, demands, pools, additions,
                                          selection.failed);
      }, k.startup_input))
    return false;
  k.stats.counting_additions += additions;
  ++k.stats.counting_matching_unsat;
  ++k.stats.counting_unsat;
  k.close_startup_certificate();
  k.inconsistent = true;
  return true;
}

// Derives sum(~x) >= n - K from a sequential at-most-K counter, then
// contradicts it with weighted clique at-most-one bounds. Every premise clause
// is restated by RUP, so a missing premise makes the checker reject.
class CounterCoverCertificate : public StartupPbWriter {
  const CounterCover &c_;
  uint64_t pol_two(uint64_t a, uint64_t b) {
    text("pol %llu %llu + ;\n", (unsigned long long)a, (unsigned long long)b);
    return ++id_;
  }
  // The literal axiom `lit >= 0` as a pol operand.
  void axiom(int32_t lit) { text(" %sx%u +", lit < 0 ? "~" : "", external_variable_of(lit)); }

public:
  CounterCoverCertificate(const Kernel &k, FILE *out, const CounterCover &c)
      : StartupPbWriter(k, out), c_(c) {}
  bool run() {
    const size_t n = c_.vertices.size(), K = c_.rows.front().size();
    header();
    // P_i: sum_{l<=i} ~x_l + sum_j s_{i,j} >= i, starting from (~x_1 | s_{1,1}).
    uint64_t prefix = clause({-c_.vertices[0], c_.rows[0][0]});
    for (size_t j = 1; j < K; ++j) {
      text("pol %llu", (unsigned long long)prefix);
      axiom(c_.rows[0][j]);
      text(" ;\n");
      prefix = ++id_;
    }
    for (size_t i = 1; i + 1 < n; ++i) {
      const int32_t x = c_.vertices[i];
      const auto &a = c_.rows[i - 1], &b = c_.rows[i];
      // S_m: sum_{t<=m} b_t + sum_{t<m} ~a_t + ~x >= m.
      // T_m: sum_{t<=m} (b_t + ~a_t) >= m.
      // S_{m+1} = (S_m + (~x | ~a_m | b_{m+1}) + T_m + b_{m+1}) / 2.
      uint64_t step = clause({-x, b[0]});
      uint64_t total = clause({-a[0], b[0]});
      for (size_t m = 1; m < K; ++m) {
        const uint64_t carry = clause({-x, -a[m - 1], b[m]});
        text("pol %llu %llu + %llu +", (unsigned long long)step, (unsigned long long)carry,
             (unsigned long long)total);
        axiom(b[m]);
        text(" 2 d ;\n");
        step = ++id_;
        total = pol_two(total, clause({-a[m], b[m]}));
      }
      // Q_i = (S_K + (~x | ~a_K) + T_K) / 2: sum b + sum ~a + ~x >= K + 1.
      const uint64_t overflow = clause({-x, -a[K - 1]});
      text("pol %llu %llu + %llu + 2 d ;\n", (unsigned long long)step,
           (unsigned long long)overflow, (unsigned long long)total);
      prefix = pol_two(prefix, ++id_);
    }
    // sum(~x) >= n - K from the last overflow and s_{n-1,j} <= 1.
    const auto &last = c_.rows[n - 2];
    const uint64_t overflow = clause({-c_.vertices[n - 1], -last[K - 1]});
    text("pol %llu %llu +", (unsigned long long)prefix, (unsigned long long)overflow);
    for (size_t j = 0; j + 1 < K; ++j)
      axiom(-last[j]);
    text(" ;\n");
    const uint64_t cardinality = ++id_;
    // Each clique: at most one of its vertices is false.
    std::vector<uint64_t> bounds;
    for (const auto &clique : c_.cliques) {
      std::vector<int32_t> outside;
      for (int32_t v : clique)
        outside.push_back(-v);
      bounds.push_back(at_most_one(outside));
    }
    // W * cardinality + sum_C w_C * AMO_C + sum_v (cover_v - W) * (x_v false >= 0).
    text("pol %llu %llu *", (unsigned long long)cardinality,
         (unsigned long long)c_.coverage);
    for (size_t at = 0; at < bounds.size(); ++at)
      text(" %llu %llu * +", (unsigned long long)bounds[at],
           (unsigned long long)c_.weights[at]);
    for (size_t v = 0; v < n; ++v)
      if (c_.excess[v]) {
        const int32_t lit = c_.vertices[v];
        text(" %sx%u %llu * +", lit > 0 ? "~" : "", external_variable_of(lit),
             (unsigned long long)c_.excess[v]);
      }
    text(" ;\n");
    ++id_;
    footer();
    return !out_ || !std::ferror(out_);
  }
};

// Validate a caller's counter, cliques and weights against the original input.
bool counter_cover_valid(const Kernel &k, Work &work, CounterCover &c) {
  const size_t n = c.vertices.size();
  if (n < 3 || c.rows.size() != n - 1 || c.rows.front().empty() || !c.coverage ||
      c.cliques.size() != c.weights.size())
    return false;
  const size_t K = c.rows.front().size();
  if (K >= n)
    return false;
  for (const auto &row : c.rows)
    if (row.size() != K)
      return false;
  PositiveGroups inputs;
  if (!clauses(k, work, [](const auto &) { return true; }, [&](const auto &row) {
        inputs.push_back(row);
        return work.tick(row.size());
      }))
    return false;
  std::sort(inputs.begin(), inputs.end());
  const auto present = [&](std::vector<int32_t> row) {
    std::sort(row.begin(), row.end());
    return work.tick(row.size()) && std::binary_search(inputs.begin(), inputs.end(), row);
  };
  // Counter premises, exactly as the certificate restates them.
  if (!present({-c.vertices[0], c.rows[0][0]}))
    return false;
  for (size_t i = 1; i + 1 < n; ++i) {
    const int32_t x = c.vertices[i];
    const auto &a = c.rows[i - 1], &b = c.rows[i];
    if (!present({-x, b[0]}) || !present({-a[0], b[0]}) || !present({-x, -a[K - 1]}))
      return false;
    for (size_t m = 1; m < K; ++m)
      if (!present({-x, -a[m - 1], b[m]}) || !present({-a[m], b[m]}))
        return false;
  }
  if (!present({-c.vertices[n - 1], -c.rows[n - 2][K - 1]}))
    return false;
  // Distinct vertex variables; cliques of pairwise-covered vertices.
  std::vector<std::pair<int32_t, size_t>> position;
  for (size_t v = 0; v < n; ++v)
    position.emplace_back(c.vertices[v], v);
  std::sort(position.begin(), position.end());
  for (size_t v = 1; v < n; ++v)
    if (external_variable_of(position[v].first) == external_variable_of(position[v - 1].first))
      return false;
  std::vector<uint64_t> cover(n, 0);
  uint64_t weight = 0;
  for (size_t at = 0; at < c.cliques.size(); ++at) {
    const auto &clique = c.cliques[at];
    const uint64_t w = c.weights[at];
    if (clique.empty() || !w || w > (uint64_t{1} << 32))
      return false;
    for (size_t i = 0; i < clique.size(); ++i) {
      const auto found = std::lower_bound(position.begin(), position.end(),
                                          std::pair<int32_t, size_t>{clique[i], 0});
      if (found == position.end() || found->first != clique[i])
        return false;
      cover[found->second] += w;
      for (size_t j = 0; j < i; ++j)
        if (clique[j] == clique[i] || !present({clique[i], clique[j]}))
          return false;
    }
    weight += w;
  }
  c.excess.assign(n, 0);
  for (size_t v = 0; v < n; ++v) {
    if (cover[v] < c.coverage)
      return false;
    c.excess[v] = cover[v] - c.coverage;
  }
  // The cliques admit at most weight / coverage false vertices; the counter
  // demands n - K of them.
  return weight < c.coverage * (n - K);
}

bool refute_counter_cover(Kernel &k, CounterCover &cover) {
  Work work{k};
  if (!counter_cover_valid(k, work, cover) || !work.tick())
    return false;
  if (k.proof && !k.proof->refute_by_counting([&](FILE *out) {
        CounterCoverCertificate proof(k, out, cover);
        return proof.run();
      }, k.startup_input))
    return false;
  ++k.stats.counting_matching_unsat;
  ++k.stats.counting_unsat;
  k.close_startup_certificate();
  k.inconsistent = true;
  return true;
}

class Certificate : public StartupPbWriter {
  Kernel &k_;
  const Plan &p_;
  int32_t leaf(const Star &star) const {
    const int32_t value = static_cast<int32_t>(star.leaf + 1);
    return p_.flip[star.leaf] ? -value : value;
  }
  // Definition z <=> (s & x). The explicit repair subproofs concern only
  // the two earlier clauses containing fresh z, never the original variables.
  uint64_t conjunction(int32_t z, int32_t s, int32_t x) {
    text("red "); term(-z); term(s);
    text(">= 1 : x%d -> 0 ;\n", z);
    const uint64_t first = ++id_;
    text("red "); term(-z); term(x);
    text(">= 1 : x%d -> 0 ;\n", z);
    const uint64_t second = ++id_;
    text("red "); term(z); term(-s); term(-x);
    text(">= 1 : x%d -> 1 : subproof\n", z);
    const uint64_t negated = ++id_;
    text("proofgoal #1\nqed ;\n");
    ++id_;
    for (uint64_t goal : {first, second}) {
      text("proofgoal %llu\n", (unsigned long long)goal);
      const uint64_t negative_goal = ++id_;
      const uint64_t contradiction = sum({negated, negative_goal});
      text("qed : %llu ;\n", (unsigned long long)contradiction);
    }
    text("qed ;\n");
    ++id_;
    return first;
  }
public:
  Certificate(Kernel &k, const Plan &p, FILE *out)
      : StartupPbWriter(k, out), k_(k), p_(p) {}
  bool run(Work *work) {
    header();
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
        if (!amo)
          amo = at_most_one(row);
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
          text("pol %sx%u ;\n", in_bound > 0 ? "~" : "", v + 1);
          bounds.push_back(++id_);
        }
      }
    sum(bounds);
    footer();
    return !out_ || !std::ferror(out_);
  }
};
} // namespace

static bool count_input_available(const Kernel &k) {
  return count_preservation_guard(k) && !k.startup_input.empty() &&
         (!k.proof || k.proof->startup_pb_available());
}

ApplyResult matching_count_apply(Kernel &k) {
  return count_input_available(k) && refute_matching_count(k)
             ? ApplyResult::Unsat
             : ApplyResult::NoOp;
}

ApplyResult counter_cover_apply(Kernel &k, CounterCover &cover) {
  return count_input_available(k) && refute_counter_cover(k, cover)
             ? ApplyResult::Unsat
             : ApplyResult::NoOp;
}

ApplyResult matching_selection_apply(Kernel &k, const MatchingSelection &selection) {
  return count_input_available(k) && refute_selected_matching(k, selection)
             ? ApplyResult::Unsat
             : ApplyResult::NoOp;
}

ApplyResult count_preservation_apply(Kernel &k) {
  if (!count_input_available(k) || k.trail.empty())
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
    const auto write_pb = [&](FILE *out) {
      Certificate proof(k, plan, out);
      return proof.run(nullptr) && proof.additions() == preflight.additions();
    };
    if (!k.proof->refute_by_counting(write_pb, k.startup_input))
      return ApplyResult::NoOp;
  }
  k.stats.counting_stars += plan.chosen.size();
  k.stats.counting_additions += preflight.additions();
  ++k.stats.counting_unsat;
  k.close_startup_certificate();
  k.inconsistent = true;
  return ApplyResult::Unsat;
}
} // namespace tactix
