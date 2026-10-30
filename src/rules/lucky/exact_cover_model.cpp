#include "rules/lucky/exact_cover_model.hpp"

#include "rules/lucky/model.hpp"
#include "rules/lucky/root_guard.hpp"
#include "strategy/rule_parameters.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <numeric>
#include <vector>

namespace tactix {
namespace {

// Rows are signed items (the literal made true by choosing the row); columns
// are the blocks followed by the covering clauses. The owner's rows cover
// exactly three columns: the item's block and its two covering clauses.
struct ExactCover {
  uint32_t blocks = 0;
  uint32_t columns = 0;
  std::vector<Lit> row_item;
  std::vector<int> row_external; // the owner orders ties by DIMACS literal
  std::vector<std::array<uint32_t, 3>> row_columns;
  std::vector<std::vector<uint32_t>> rows_by_column;
};

struct DisjointSets {
  std::vector<uint32_t> parent;
  explicit DisjointSets(size_t n) : parent(n) {
    std::iota(parent.begin(), parent.end(), 0u);
  }
  uint32_t find(uint32_t x) {
    while (parent[x] != x) {
      parent[x] = parent[parent[x]];
      x = parent[x];
    }
    return x;
  }
  void unite(uint32_t a, uint32_t b) { parent[find(a)] = find(b); }
};

// build_signed_sgen_exact_cover: a binary (a v b) is the at-most-one edge
// between the signed items ~a and ~b; every other clause is a covering
// clause of the block arity. The item literal orientation is the owner's.
bool recognize(Kernel &k, const strategy::ExactCoverModelParameters &p,
               ExactCover &cover) {
  const uint32_t arity = p.block_arity;
  const uint32_t literal_count = 2 * k.nvars;
  DisjointSets sets(literal_count);
  std::vector<uint8_t> touched(literal_count, 0);
  uint64_t binaries = 0;
  for (Lit literal = 0; literal < literal_count; ++literal)
    for (const WatchEntry watch : k.watches.entries(literal)) {
      if (!watch.binary() || literal > watch.other())
        continue;
      const Lit a = negate(literal), b = negate(watch.other());
      sets.unite(a, b);
      touched[a] = touched[b] = 1;
      ++binaries;
    }
  if (!binaries)
    return false;
  std::vector<std::vector<Lit>> covering;
  uint64_t visited = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    if (!clause->garbage()) {
      if (clause->size != arity)
        return false;
      std::vector<Lit> literals(clause->begin(), clause->end());
      std::sort(literals.begin(), literals.end());
      if (std::adjacent_find(literals.begin(), literals.end()) != literals.end())
        return false;
      covering.push_back(std::move(literals));
    }
    ref += clause_actual_words(clause);
    if (!(++visited & ((uint64_t{1} << 18) - 1)) &&
        kernel_rule_interrupt(k, k.stats.exact_cover_model_interrupted))
      return false;
  }
  if (covering.empty())
    return false;

  // Blocks: the connected components of the at-most-one edges, in order of
  // their first item, each of exactly the block arity.
  std::vector<uint32_t> block_of(literal_count, UINT32_MAX);
  std::vector<uint32_t> block_size;
  for (Lit literal = 0; literal < literal_count; ++literal) {
    if (!touched[literal])
      continue;
    const uint32_t root = sets.find(literal);
    if (block_of[root] == UINT32_MAX) {
      block_of[root] = static_cast<uint32_t>(block_size.size());
      block_size.push_back(0);
    }
    block_of[literal] = block_of[root];
    ++block_size[block_of[literal]];
  }
  const uint32_t blocks = static_cast<uint32_t>(block_size.size());
  if (!blocks || covering.size() != uint64_t{2} * blocks)
    return false;
  for (const uint32_t size : block_size)
    if (size != arity)
      return false;

  // Each item lies in one block and exactly two covering clauses; every
  // variable has exactly one item.
  std::vector<std::vector<uint32_t>> columns_of(literal_count);
  for (Lit literal = 0; literal < literal_count; ++literal)
    if (touched[literal])
      columns_of[literal].push_back(block_of[literal]);
  for (uint32_t index = 0; index < covering.size(); ++index)
    for (const Lit literal : covering[index])
      columns_of[literal].push_back(blocks + index);
  cover = ExactCover{};
  cover.blocks = blocks;
  cover.columns = blocks + static_cast<uint32_t>(covering.size());
  cover.rows_by_column.assign(cover.columns, {});
  std::vector<uint8_t> variable_seen(k.nvars, 0);
  for (Lit literal = 0; literal < literal_count; ++literal) {
    const auto &columns = columns_of[literal];
    if (columns.empty())
      continue;
    if (columns.size() != 3 || variable_seen[var_of(literal)] ||
        k.value(literal))
      return false;
    variable_seen[var_of(literal)] = 1;
    std::array<uint32_t, 3> row{columns[0], columns[1], columns[2]};
    std::sort(row.begin(), row.end());
    if (row[0] >= blocks || row[0] == row[1] || row[1] == row[2])
      return false;
    const uint32_t index = static_cast<uint32_t>(cover.row_item.size());
    cover.row_item.push_back(literal);
    cover.row_external.push_back(kernel_externalize(k, literal));
    cover.row_columns.push_back(row);
    for (const uint32_t column : row)
      cover.rows_by_column[column].push_back(index);
  }
  if (cover.row_item.size() != k.nvars)
    return false;
  for (const auto &rows : cover.rows_by_column)
    if (rows.empty())
      return false;
  return true;
}

// The owner's SplitMix64.
struct SplitMix64 {
  uint64_t state;
  uint64_t next() {
    uint64_t z = (state += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
  }
  uint64_t below(uint64_t n) { return next() % n; }
};

// ExactDfsSolver: minimum-remaining-rows column choice (random tie-break
// when randomized), rows ordered by the sum of their columns' sizes and then
// the DIMACS literal (shuffled when randomized).
struct Search {
  Kernel &k;
  const ExactCover &cover;
  const uint64_t node_limit;
  SplitMix64 rng;
  const bool randomized;
  std::vector<uint8_t> active;
  std::vector<uint32_t> chosen;
  uint64_t nodes = 0;
  bool interrupted = false;

  Search(Kernel &kernel, const ExactCover &c, uint64_t limit, uint64_t seed,
         bool random)
      : k(kernel), cover(c), node_limit(limit), rng{seed}, randomized(random),
        active(c.columns, 1) {}

  bool valid_row(uint32_t row) const {
    const auto &columns = cover.row_columns[row];
    return active[columns[0]] && active[columns[1]] && active[columns[2]];
  }

  bool dfs() {
    if (++nodes > node_limit)
      return false;
    if (!(nodes & 8191u) &&
        kernel_rule_interrupt(k, k.stats.exact_cover_model_interrupted)) {
      interrupted = true;
      return false;
    }
    uint32_t best_column = UINT32_MAX;
    uint32_t best_count = UINT32_MAX;
    uint64_t ties = 0;
    for (uint32_t column = 0; column < cover.columns; ++column) {
      if (!active[column])
        continue;
      uint32_t count = 0;
      for (const uint32_t row : cover.rows_by_column[column])
        count += valid_row(row);
      if (!count)
        return false;
      bool take = false;
      if (count < best_count) {
        take = true;
        ties = 1;
      } else if (randomized && count == best_count) {
        ++ties;
        take = rng.below(ties) == 0;
      }
      if (take) {
        best_count = count;
        best_column = column;
        if (!randomized && count == 1)
          break;
      }
    }
    if (best_column == UINT32_MAX)
      return true;
    std::vector<uint32_t> rows;
    for (const uint32_t row : cover.rows_by_column[best_column])
      if (valid_row(row))
        rows.push_back(row);
    if (randomized) {
      for (size_t i = rows.size(); i-- > 1;)
        std::swap(rows[i], rows[rng.below(i + 1)]);
    } else {
      const auto weight = [&](uint32_t row) {
        uint64_t sum = 0;
        for (const uint32_t column : cover.row_columns[row])
          sum += cover.rows_by_column[column].size();
        return sum;
      };
      std::sort(rows.begin(), rows.end(), [&](uint32_t a, uint32_t b) {
        const uint64_t wa = weight(a), wb = weight(b);
        if (wa != wb)
          return wa < wb;
        return cover.row_external[a] < cover.row_external[b];
      });
    }
    for (const uint32_t row : rows) {
      if (!valid_row(row))
        continue;
      const auto columns = cover.row_columns[row];
      active[columns[0]] = active[columns[1]] = active[columns[2]] = 0;
      chosen.push_back(row);
      if (dfs())
        return true;
      chosen.pop_back();
      active[columns[0]] = active[columns[1]] = active[columns[2]] = 1;
      if (interrupted || nodes > node_limit)
        return false;
    }
    return false;
  }
};

bool solve(Kernel &k, const strategy::ExactCoverModelParameters &p,
           const ExactCover &cover, std::vector<uint32_t> &solution) {
  {
    Search search(k, cover, p.maximum_nodes, p.seed, false);
    const bool found = search.dfs();
    k.stats.exact_cover_model_nodes += search.nodes;
    if (found) {
      solution = search.chosen;
      return true;
    }
    if (search.interrupted)
      return false;
  }
  // Randomized restarts: the owner reseeds each attempt from its stochastic
  // seed by a fixed stride.
  uint64_t seed = 0x9e3779b97f4a7c15ULL;
  for (uint64_t restart = 0; restart < p.restarts; ++restart) {
    seed += 0x100000001b3ULL;
    Search search(k, cover, p.restart_nodes, seed, true);
    const bool found = search.dfs();
    k.stats.exact_cover_model_nodes += search.nodes;
    ++k.stats.exact_cover_model_restarts;
    if (found) {
      solution = search.chosen;
      return true;
    }
    if (search.interrupted)
      return false;
  }
  ++k.stats.exact_cover_model_budget_exhausted;
  return false;
}

} // namespace

RuleEligibility exact_cover_model_guard(const Kernel &k) {
  return lucky_root_eligibility(k);
}

ApplyResult exact_cover_model_apply(Kernel &k) {
  assert(exact_cover_model_guard(k));
  ++k.stats.exact_cover_model_calls;
  const strategy::ExactCoverModelParameters p =
      strategy::ExactCoverModelParameters::from_kernel(k);
  if (!p.valid())
    return ApplyResult::NoOp;
  ExactCover cover;
  if (!recognize(k, p, cover))
    return ApplyResult::NoOp;
  ++k.stats.exact_cover_model_recognized;
  k.stats.exact_cover_model_blocks = cover.blocks;
  std::vector<uint32_t> solution;
  if (!solve(k, p, cover, solution))
    return ApplyResult::NoOp;
  // A chosen row makes its item true; every other item is false.
  std::vector<uint8_t> assignment(k.nvars, 0);
  std::vector<uint8_t> chosen(cover.row_item.size(), 0);
  for (const uint32_t row : solution)
    chosen[row] = 1;
  for (uint32_t row = 0; row < cover.row_item.size(); ++row) {
    const Lit item = cover.row_item[row];
    assignment[var_of(item)] = chosen[row] != negated(item);
  }
  if (!lucky_model_valid(k, assignment))
    return ApplyResult::NoOp;
  lucky_publish_model(k, assignment);
  ++k.stats.exact_cover_model_sat;
  return ApplyResult::Sat;
}

} // namespace tactix
