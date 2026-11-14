#include "rules/substitution_redundancy/substitution_redundancy.hpp"
#include "rules/disjoint_sets.hpp"
#include "rules/substitution_redundancy/detection_support.hpp"
#include "rules/substitution_redundancy/orbitopal_group.hpp"
#include "rules/substitution_redundancy/row_feasibility.hpp"

#include "symmetry_backend/dejavu_backend.hpp"
#include "diagnostics/symmetry_trace.hpp"

#include <algorithm>
#include <map>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace tactix {
namespace {

using sr_detail::backend_stop;
using sr_detail::fail;
using sr_detail::LiteralGraph;
using sr_detail::saturating_add;
using sr_detail::stop_requested;
using sr_detail::StopContext;

// Diagnostics identify the exact local allowance that ended discovery. The
// returned status is unchanged; site coordinates belong to the pinned source.
OrbitopalEnumerationStatus traced_orbitopal_limit(const char *function,
                                                 unsigned line) {
  SymmetryTrace{}.event("orbitopal_limit", "function\t%s\tline\t%u", function, line);
  return OrbitopalEnumerationStatus::LimitReached;
}

// Fold one backend call's work into the pass. A stop maps to Interrupted and
// any other incomplete status to the traced limit at the caller's site.
bool backend_complete(OrbitopalEnumerationResult &result,
                      symmetry_backend::Status status, uint64_t ticks,
                      const char *function, unsigned line) {
  result.ticks = saturating_add(result.ticks, ticks);
  if (status == symmetry_backend::Status::Complete)
    return true;
  result.status = status == symmetry_backend::Status::Interrupted
                      ? OrbitopalEnumerationStatus::Interrupted
                      : traced_orbitopal_limit(function, line);
  return false;
}

// FNV-1a over the words of a vector key; a word wider than size_t also folds
// in its high half.
template <class Word> struct VectorKeyHash {
  size_t operator()(const std::vector<Word> &key) const noexcept {
    size_t hash = sizeof(size_t) == 8 ? 1469598103934665603ull : 2166136261u;
    for (const Word word : key) {
      hash ^= static_cast<size_t>(word);
      hash *= sizeof(size_t) == 8 ? 1099511628211ull : 16777619u;
      if constexpr (sizeof(size_t) < sizeof(Word)) {
        hash ^= static_cast<size_t>(word >> 32);
        hash *= 16777619u;
      }
    }
    return hash;
  }
};

using Matrix = std::vector<std::vector<Lit>>;

struct LogicalFormula {
  // First physical occurrence of each semantic clause, in kernel order; as in
  // Satsuma, duplicate clause vertices would obscure graph automorphisms.
  Matrix clauses;
  std::unordered_set<std::vector<Lit>, VectorKeyHash<Lit>> clause_set;
};

struct OrbitopalScreen {
  std::vector<uint32_t> occurrences;
  // Satsuma builds ULC AMOs before propagating input units, so that view
  // excludes root units.
  std::vector<uint32_t> ulc_occurrences;
  Matrix candidate_clauses;
  Matrix ulc_candidate_clauses;
};

// Satsuma's max(16 * variables, 16 Mi) structured split allowance, charged in
// deterministic work as a zero-progress allowance renewed by each admitted
// matrix.
class RefinementProgressFeedback {
public:
  RefinementProgressFeedback(const Kernel &kernel,
                             const SubstitutionRedundancyParameters &parameters,
                             const OrbitopalEnumerationResult &result)
      : enabled_(parameters.orbitopal_progress_feedback),
        allowance_(std::max<uint64_t>(
            16ull * 1024 * 1024,
            16ull * static_cast<uint64_t>(kernel.nvars))),
        checkpoint_(result.ticks), matrices_(result.matrices) {}

  bool stop(OrbitopalEnumerationResult &result, const char *producer) {
    if (!enabled_)
      return false;
    if (result.matrices != matrices_) {
      matrices_ = result.matrices;
      checkpoint_ = result.ticks;
      return false;
    }
    if (result.ticks < checkpoint_ ||
        result.ticks - checkpoint_ < allowance_)
      return false;
    ++result.unproductive_refinement_stops;
    SymmetryTrace{}.event(
        "orbitopal_progress_stop",
        "producer\t%s\twork\t%llu\tallowance\t%llu\tmatrices\t%llu",
        producer,
        static_cast<unsigned long long>(result.ticks - checkpoint_),
        static_cast<unsigned long long>(allowance_),
        static_cast<unsigned long long>(result.matrices));
    return true;
  }

private:
  bool enabled_ = false;
  uint64_t allowance_ = UINT64_MAX;
  uint64_t checkpoint_ = 0;
  uint64_t matrices_ = 0;
};

class MaximumCliqueSearch {
public:
  MaximumCliqueSearch(const Kernel &kernel,
                      const SubstitutionRedundancyParameters &parameters,
                      volatile sig_atomic_t *stop,
                      OrbitopalEnumerationResult &result,
                      const std::vector<std::vector<uint64_t>> &adjacency)
      : kernel_(kernel), parameters_(parameters), stop_(stop), result_(result),
        adjacency_(adjacency) {}

  bool run(std::vector<size_t> &maximum) {
    if (interrupted())
      return false;
    if (adjacency_.empty()) {
      maximum.clear();
      return true;
    }
    std::vector<size_t> candidates(adjacency_.size());
    for (size_t vertex = 0; vertex < candidates.size(); ++vertex)
      candidates[vertex] = vertex;
    if (!expand(candidates))
      return false;

    // Cliquer's identity order makes the chosen clique observable: take the
    // lexicographically first clique of the proven maximum size.
    const size_t maximum_size = best_.size();
    path_.clear();
    lexicographic_found_ = false;
    if (!find_lexicographic(candidates, maximum_size))
      return false;
    maximum = path_;
    return lexicographic_found_ && maximum.size() == maximum_size;
  }

  bool run_greedy(std::vector<size_t> &clique, bool retain_on_local_limit = false) {
    retain_on_local_limit_ = retain_on_local_limit;
    clique.clear();
    const auto retain = [&](std::vector<size_t> candidate) {
      std::sort(candidate.begin(), candidate.end());
      if (candidate.size() > clique.size() ||
          (candidate.size() == clique.size() && candidate < clique))
        clique = std::move(candidate);
    };
    const auto stopped = [&](const std::vector<size_t> &candidate) {
      if (!local_limit_reached_)
        return false;
      // Only selected vertices form a clique; partially filtered neighbor
      // lists do not establish pairwise adjacency among their members.
      retain(candidate);
      ++result_.clique_budget_stops;
      SymmetryTrace{}.event("orbitopal_clique_budget", "vertices\t%zu\tticks\t%llu",
                            clique.size(),
                            static_cast<unsigned long long>(clique_ticks_));
      return true;
    };
    for (size_t seed = 0; seed < adjacency_.size(); ++seed) {
      std::vector<size_t> candidate_clique = {seed};
      std::vector<size_t> candidates;
      for (size_t vertex = 0; vertex < adjacency_.size(); ++vertex) {
        if (!tick())
          return stopped(candidate_clique);
        if (vertex != seed && adjacent(seed, vertex))
          candidates.push_back(vertex);
      }
      while (!candidates.empty()) {
        const size_t selected = candidates.front();
        candidate_clique.push_back(selected);
        std::vector<size_t> next;
        for (size_t at = 1; at < candidates.size(); ++at) {
          if (!tick())
            return stopped(candidate_clique);
          if (adjacent(selected, candidates[at]))
            next.push_back(candidates[at]);
        }
        candidates = std::move(next);
      }
      retain(std::move(candidate_clique));
    }
    return true;
  }

private:
  bool interrupted() const {
    return stop_requested(kernel_, stop_);
  }

  bool tick() {
    if (retain_on_local_limit_) {
      if (result_.ticks >= parameters_.maximum_detection_ticks)
        return fail(result_, traced_orbitopal_limit(__func__, __LINE__));
      if (clique_ticks_ >= parameters_.maximum_orbitopal_clique_ticks) {
        if (interrupted())
          return fail(result_, OrbitopalEnumerationStatus::Interrupted);
        local_limit_reached_ = true;
        return false;
      }
    }
    if (clique_ticks_ >= parameters_.maximum_orbitopal_clique_ticks ||
        result_.ticks >= parameters_.maximum_detection_ticks)
      return fail(result_, traced_orbitopal_limit(__func__, __LINE__));
    // Poll once per 1024-tick block to bound interrupt latency.
    if (!(clique_ticks_ & 1023) && interrupted())
      return fail(result_, OrbitopalEnumerationStatus::Interrupted);
    ++clique_ticks_;
    ++result_.ticks;
    return true;
  }

  bool adjacent(size_t first, size_t second) const {
    return (adjacency_[first][second / 64] >> (second % 64)) & 1u;
  }

  bool color_sort(const std::vector<size_t> &candidates,
                  std::vector<size_t> &ordered,
                  std::vector<size_t> &bounds) {
    std::vector<size_t> remaining = candidates;
    ordered.clear();
    bounds.clear();
    size_t color = 0;
    while (!remaining.empty()) {
      ++color;
      std::vector<size_t> color_class;
      std::vector<size_t> next;
      for (const size_t vertex : remaining) {
        bool available = true;
        for (const size_t chosen : color_class) {
          if (!tick())
            return false;
          if (adjacent(vertex, chosen)) {
            available = false;
            break;
          }
        }
        if (available)
          color_class.push_back(vertex);
        else
          next.push_back(vertex);
      }
      for (const size_t vertex : color_class) {
        ordered.push_back(vertex);
        bounds.push_back(color);
      }
      remaining = std::move(next);
    }
    return true;
  }

  bool expand(std::vector<size_t> candidates) {
    if (!tick())
      return false;
    std::vector<size_t> ordered, bounds;
    if (!color_sort(candidates, ordered, bounds))
      return false;
    for (size_t at = ordered.size(); at; --at) {
      if (path_.size() + bounds[at - 1] <= best_.size())
        return true;
      const size_t vertex = ordered[at - 1];
      path_.push_back(vertex);
      std::vector<size_t> next;
      next.reserve(candidates.size());
      for (const size_t candidate : candidates) {
        if (candidate == vertex)
          continue;
        if (!tick())
          return false;
        if (adjacent(vertex, candidate))
          next.push_back(candidate);
      }
      if (next.empty()) {
        if (path_.size() > best_.size())
          best_ = path_;
      } else if (path_.size() + next.size() > best_.size()) {
        if (!expand(std::move(next)))
          return false;
      }
      path_.pop_back();
      const auto found = std::find(candidates.begin(), candidates.end(), vertex);
      if (found != candidates.end())
        candidates.erase(found);
    }
    return true;
  }

  bool find_lexicographic(const std::vector<size_t> &candidates,
                          size_t needed) {
    if (!tick())
      return false;
    if (!needed) {
      lexicographic_found_ = true;
      return true;
    }
    if (candidates.size() < needed)
      return true;
    for (size_t at = 0; at < candidates.size(); ++at) {
      const size_t vertex = candidates[at];
      std::vector<size_t> next;
      for (size_t later = at + 1; later < candidates.size(); ++later) {
        if (!tick())
          return false;
        if (adjacent(vertex, candidates[later]))
          next.push_back(candidates[later]);
      }
      if (needed > 1) {
        if (next.size() < needed - 1)
          continue;
        std::vector<size_t> ordered, bounds;
        if (!color_sort(next, ordered, bounds))
          return false;
        if (bounds.empty() || bounds.back() < needed - 1)
          continue;
      }
      path_.push_back(vertex);
      if (!find_lexicographic(next, needed - 1))
        return false;
      if (lexicographic_found_)
        return true;
      path_.pop_back();
    }
    return true;
  }

  const Kernel &kernel_;
  const SubstitutionRedundancyParameters &parameters_;
  volatile sig_atomic_t *stop_;
  OrbitopalEnumerationResult &result_;
  const std::vector<std::vector<uint64_t>> &adjacency_;
  uint64_t clique_ticks_ = 0;
  bool retain_on_local_limit_ = false;
  bool local_limit_reached_ = false;
  bool lexicographic_found_ = false;
  std::vector<size_t> path_, best_;
};

std::vector<Lit> clause_key(std::vector<Lit> clause) {
  std::sort(clause.begin(), clause.end());
  return clause;
}

bool index_clause(LogicalFormula &formula, const std::vector<Lit> &clause) {
  return formula.clause_set.emplace(clause_key(clause)).second;
}

bool contains_clause(const LogicalFormula &formula,
                     std::vector<Lit> clause) {
  return formula.clause_set.count(clause_key(std::move(clause))) != 0;
}

// Owns one backend result until scope exit; release is idempotent.
template <class Value> struct BackendOwner {
  Value value{};
  ~BackendOwner() { symmetry_backend::release(value); }
};

struct FormulaRefinement {
  bool initialized = false;
  uint32_t literal_count = 0;
  symmetry_backend::RefinementSession *session = nullptr;

  ~FormulaRefinement() { symmetry_backend::release(session); }
};

struct FormulaAutomorphisms {
  bool initialized = false;
  symmetry_backend::Status detection_status = symmetry_backend::Status::Failed;
  uint32_t literal_count = 0;
  std::vector<uint32_t> images;
};

struct ProductChildPartition {
  std::vector<uint32_t> cells;
  std::unordered_map<uint32_t, uint32_t> sizes;
};

// One discovery pass. Every step charges and records its limit in `result`;
// the formula snapshot, screen and backend caches belong to the pass.
struct OrbitopalPass {
  OrbitopalPass(const Kernel &kernel,
                const SubstitutionRedundancyParameters &parameters,
                const OrbitopalInstanceVisitor &visitor,
                volatile sig_atomic_t *stop, SymmetryDiscoveryHints *hints)
      : kernel(kernel), parameters(parameters), visitor(visitor), stop(stop),
        hints(hints) {}

  OrbitopalEnumerationResult enumerate();
  bool charge(uint64_t amount);
  template <class Visit> bool visit_first_binary_copies(Visit &&visit);
  template <class Visit> bool visit_root_units(Visit &&visit);
  bool screen_formula();
  bool full_snapshot();
  bool matrix_action_key(const Matrix &columns, std::vector<Lit> &key);
  bool exact_row_action_key(const Matrix &matrix, std::vector<Lit> &key);
  bool exact_full_row_group(const Matrix &matrix);
  bool exact_dimension(const Matrix &matrix, Matrix &permutations);
  bool order_columns_by_maximum_clique(Matrix &matrix);
  bool admit_direct_graph(LiteralGraph &storage, const char *function,
                          unsigned line);
  bool prepare_formula_refinement();
  bool complete_matrix_by_refinement(Matrix &matrix, bool unique_first = false);
  bool prepare_formula_automorphisms();
  bool constrained_dimension_automorphisms(const Matrix &matrix,
                                           Matrix &permutations);
  bool emit_dimension(const Matrix &matrix,
                      const std::vector<size_t> &fixable_columns,
                      const Matrix &permutations);
  std::vector<size_t> unique_literal_columns(const Matrix &matrix) const;
  bool admit_refinement_orbit(uint64_t candidate_size, const char *function,
                              unsigned line);
  template <typename Individualize>
  bool product_child_partition(const std::vector<Lit> &orbit,
                               ProductChildPartition &child,
                               Individualize individualize);
  bool product_child_partition(symmetry_backend::RefinementBranch &branch,
                               const std::vector<Lit> &orbit, Lit anchor,
                               ProductChildPartition &child);
  bool product_child_partition(const std::vector<Lit> &orbit, Lit anchor,
                               ProductChildPartition &child);
  bool product_matrix_from_refinement(const std::vector<Lit> &orbit,
                                      const std::vector<Lit> &first_side,
                                      const std::vector<Lit> &second_side,
                                      Matrix &matrix);
  bool collect_matrix_generators(const Matrix &matrix, Matrix &rows,
                                 Matrix &columns);
  bool emit_refined_product_orbits(bool &emitted);
  bool emit_refined_row_candidates(bool &emitted);

  const Kernel &kernel;
  const SubstitutionRedundancyParameters &parameters;
  const OrbitopalInstanceVisitor &visitor;
  volatile sig_atomic_t *stop;
  SymmetryDiscoveryHints *hints;
  OrbitopalEnumerationResult result;
  OrbitopalScreen screen;
  LogicalFormula formula;
  FormulaAutomorphisms automorphisms;
  FormulaRefinement refinement;
  std::vector<Lit> prefix_units;
};

bool OrbitopalPass::charge(uint64_t amount) {
  if (stop_requested(kernel, stop))
    return fail(result, OrbitopalEnumerationStatus::Interrupted);
  const uint64_t limit = parameters.maximum_detection_ticks;
  if (result.ticks >= limit || amount > limit - result.ticks) {
    result.ticks = limit;
    return fail(result, traced_orbitopal_limit(__func__, __LINE__));
  }
  result.ticks += amount;
  return true;
}

// Visit each binary clause once, at its smaller endpoint's first physical
// copy; a stamp on the other endpoint skips later copies. Every watch is
// charged, mirrors and duplicate copies included.
template <class Visit>
bool OrbitopalPass::visit_first_binary_copies(Visit &&visit) {
  std::vector<Lit> first(2 * kernel.nvars, INVALID_LIT);
  for (Lit literal = 0; literal < 2 * kernel.nvars; ++literal)
    for (const WatchEntry watch : kernel.watches.entries(literal)) {
      if (!charge(1))
        return false;
      if (!watch.binary())
        continue;
      const Lit other = watch.other();
      if (literal > other || first[other] == literal)
        continue;
      first[other] = literal;
      visit(literal, other);
    }
  return true;
}

// Visit each root unit literal in variable order, charging every variable.
template <class Visit> bool OrbitopalPass::visit_root_units(Visit &&visit) {
  for (Var var = 0; var < kernel.nvars; ++var) {
    if (!charge(1))
      return false;
    if (!kernel.unit_ids[var])
      continue;
    const Lit positive = make_lit(var, false);
    visit(kernel.value(positive) > 0 ? positive : negate(positive));
  }
  return true;
}

bool OrbitopalPass::screen_formula() {
  screen.occurrences.assign(2 * kernel.nvars, 0);
  std::vector<Ref> possible_large;
  if (kernel.dense) {
    // Dense counts omit redundant large clauses: a false positive here is
    // caught by the exact automorphism test and SR validation.
    for (Lit literal = 0; literal < 2 * kernel.nvars; ++literal) {
      if (!charge(1))
        return false;
      screen.occurrences[literal] = kernel.dense->large_size(literal) +
                                    kernel.watches.binary_size(literal);
    }
  } else {
    if (!visit_first_binary_copies([&](Lit literal, Lit other) {
          ++screen.occurrences[literal];
          ++screen.occurrences[other];
        }))
      return false;
    for (const auto [ref, clause] : arena_records(kernel.arena)) {
      if (!clause->garbage()) {
        if (!charge(clause->size))
          return false;
        possible_large.push_back(ref);
        for (const Lit literal : *clause)
          ++screen.occurrences[literal];
      }
    }
  }
  screen.ulc_occurrences = screen.occurrences;
  if (!visit_root_units([&](Lit unit) { ++screen.occurrences[unit]; }))
    return false;

  const auto collect_candidates =
      [&](const std::vector<uint32_t> &occurrences, Matrix &candidates,
          bool include_binaries) {
        std::unordered_set<uint64_t> candidate_binaries;
        std::unordered_set<Ref> candidate_large;
        for (Lit literal = 0; literal < 2 * kernel.nvars; ++literal) {
          if (occurrences[literal] != 1)
            continue;
          if (include_binaries && kernel.watches.binary_size(literal) == 1 &&
              parameters.minimum_orbitopal_rows <= 2) {
            for (const WatchEntry watch : kernel.watches.entries(literal)) {
              if (!watch.binary())
                continue;
              const Lit other = watch.other();
              if (occurrences[other] == 1 &&
                  candidate_binaries
                      .insert(kernel_binary_key(literal, other))
                      .second) {
                std::vector<Lit> clause = {literal, other};
                std::sort(clause.begin(), clause.end());
                candidates.push_back(std::move(clause));
              }
              break;
            }
          }
          if (kernel.dense && kernel.dense->large_size(literal) == 1)
            kernel.dense->with_large(literal, [&](auto refs) {
              if (!refs.empty())
                candidate_large.insert(*refs.begin());
            });
        }
        std::vector<Ref> candidate_refs = possible_large;
        if (kernel.dense) {
          candidate_refs.assign(candidate_large.begin(),
                                candidate_large.end());
          // ULC completion is clause-order sensitive: restore arena order.
          std::sort(candidate_refs.begin(), candidate_refs.end());
        }
        for (const Ref ref : candidate_refs) {
          const Clause *clause = clause_at(kernel.arena, ref);
          if (clause->garbage() ||
              clause->size < parameters.minimum_orbitopal_rows)
            continue;
          if (!charge(clause->size))
            return false;
          bool unique = true;
          for (const Lit literal : *clause)
            unique &= occurrences[literal] == 1;
          if (unique)
            candidates.emplace_back(clause->begin(), clause->end());
        }
        return true;
      };
  if (parameters.complete_unique_literal_amos &&
      !collect_candidates(screen.ulc_occurrences,
                          screen.ulc_candidate_clauses,
                          /*include_binaries=*/false))
    return false;
  if (!collect_candidates(screen.occurrences, screen.candidate_clauses,
                          /*include_binaries=*/true))
    return false;
  return true;
}

bool OrbitopalPass::full_snapshot() {
  formula.clauses.reserve(kernel.binary_ids.size() +
                          kernel.live_irredundant_large + kernel.nvars);
  formula.clause_set.reserve(kernel.binary_ids.size() +
                             kernel.live_irredundant_large + kernel.nvars);
  const auto add = [&](std::vector<Lit> clause) {
    if (index_clause(formula, clause))
      formula.clauses.push_back(std::move(clause));
  };
  if (!visit_first_binary_copies(
          [&](Lit literal, Lit other) { add({literal, other}); }))
    return false;
  for (const auto [ref, clause] : arena_records(kernel.arena)) {
    if (!clause->garbage()) {
      if (!charge(clause->size))
        return false;
      add(std::vector<Lit>(clause->begin(), clause->end()));
    }
  }
  return visit_root_units([&](Lit literal) { add({literal}); });
}

// Columns have disjoint supports, so their minimum literals give a common
// column order that keeps column correspondence across rows.
bool OrbitopalPass::matrix_action_key(const Matrix &columns,
                                      std::vector<Lit> &key) {
  const size_t rows = columns.front().size();
  std::vector<std::pair<Lit, size_t>> order;
  order.reserve(columns.size());
  for (size_t column = 0; column < columns.size(); ++column) {
    if (!charge(rows))
      return false;
    order.emplace_back(*std::min_element(columns[column].begin(),
                                        columns[column].end()), column);
  }
  std::sort(order.begin(), order.end());
  Matrix normalized(rows);
  for (auto &row : normalized)
    row.reserve(columns.size());
  for (const auto &[minimum, column] : order) {
    (void)minimum;
    for (size_t row = 0; row < rows; ++row)
      normalized[row].push_back(columns[column][row]);
  }
  std::sort(normalized.begin(), normalized.end());
  key.clear();
  key.push_back(static_cast<Lit>(rows));
  key.push_back(static_cast<Lit>(columns.size()));
  for (const auto &row : normalized)
    key.insert(key.end(), row.begin(), row.end());
  return charge(0);
}

// Flipping a column's polarity or permuting columns/rows gives the same row
// action; the key identifies failed exact actions.
bool OrbitopalPass::exact_row_action_key(const Matrix &matrix,
                                         std::vector<Lit> &key) {
  Matrix columns(matrix.front().size());
  for (size_t column = 0; column < columns.size(); ++column) {
    if (!charge(matrix.size()))
      return false;
    Lit smallest = matrix.front()[column];
    for (const auto &row : matrix)
      if (var_of(row[column]) < var_of(smallest))
        smallest = row[column];
    const Lit flip = smallest & 1u;
    columns[column].reserve(matrix.size());
    for (const auto &row : matrix)
      columns[column].push_back(row[column] ^ flip);
  }
  return matrix_action_key(columns, key);
}

uint64_t bounded_binomial(size_t n, size_t k, uint64_t limit) {
  if (k > n)
    return limit == UINT64_MAX ? UINT64_MAX : limit + 1;
  k = std::min(k, n - k);
  unsigned __int128 value = 1;
  for (size_t i = 1; i <= k; ++i) {
    value = value * (n - k + i) / i;
    if (value > limit)
      return limit == UINT64_MAX ? UINT64_MAX
                                 : limit + 1;
  }
  return static_cast<uint64_t>(value);
}

bool OrbitopalPass::exact_full_row_group(const Matrix &matrix) {
  struct Position {
    uint32_t row = UINT32_MAX;
    uint64_t token = 0;
  };
  struct OrbitSummary {
    uint64_t expected = 0;
    uint64_t distinct = 0;
  };

  const size_t rows = matrix.size();
  const size_t columns = matrix.front().size();
  std::vector<Position> position(2 * static_cast<size_t>(kernel.nvars));
  for (size_t row = 0; row < rows; ++row) {
    if (matrix[row].size() != columns)
      return false;
    for (size_t column = 0; column < columns; ++column) {
      const Lit literal = matrix[row][column];
      const Lit opposite = negate(literal);
      if (literal >= position.size() || opposite >= position.size() ||
          position[literal].row != UINT32_MAX ||
          position[opposite].row != UINT32_MAX)
        return false;
      position[literal] =
          {static_cast<uint32_t>(row), 2 * static_cast<uint64_t>(column)};
      position[opposite] =
          {static_cast<uint32_t>(row),
           2 * static_cast<uint64_t>(column) + 1};
    }
  }

  std::unordered_map<std::vector<uint64_t>, OrbitSummary,
                     VectorKeyHash<uint64_t>>
      orbits;
  orbits.reserve(formula.clause_set.size());
  std::vector<std::vector<uint64_t>> row_tokens(rows);
  std::vector<uint32_t> touched_rows;
  std::vector<Lit> outside;
  std::vector<uint32_t> role_rows;
  std::vector<uint64_t> orbit_key;
  const uint64_t maximum_distinct = formula.clause_set.size();

  for (const auto &clause : formula.clause_set) {
    if (!charge(clause.size()))
      return false;
    touched_rows.clear();
    outside.clear();
    for (const Lit literal : clause) {
      if (literal >= position.size() || position[literal].row == UINT32_MAX) {
        outside.push_back(literal);
        continue;
      }
      const uint32_t row = position[literal].row;
      if (row_tokens[row].empty())
        touched_rows.push_back(row);
      row_tokens[row].push_back(position[literal].token);
    }
    // A clause outside the matrix is a complete singleton orbit; it still
    // pays the charge above.
    if (touched_rows.empty())
      continue;
    std::sort(outside.begin(), outside.end());
    role_rows = touched_rows;
    for (const uint32_t row : role_rows)
      std::sort(row_tokens[row].begin(), row_tokens[row].end());
    std::sort(role_rows.begin(), role_rows.end(), [&](uint32_t first,
                                                      uint32_t second) {
      return row_tokens[first] < row_tokens[second];
    });

    orbit_key.clear();
    orbit_key.reserve(2 + outside.size() + clause.size() + role_rows.size());
    orbit_key.push_back(outside.size());
    orbit_key.insert(orbit_key.end(), outside.begin(), outside.end());
    orbit_key.push_back(role_rows.size());
    for (const uint32_t row : role_rows) {
      orbit_key.push_back(row_tokens[row].size());
      orbit_key.insert(orbit_key.end(), row_tokens[row].begin(),
                       row_tokens[row].end());
    }

    uint64_t expected = 1;
    size_t remaining = rows;
    for (size_t begin = 0; begin < role_rows.size();) {
      size_t end = begin + 1;
      while (end < role_rows.size() &&
             row_tokens[role_rows[begin]] == row_tokens[role_rows[end]])
        ++end;
      const uint64_t choices =
          bounded_binomial(remaining, end - begin, maximum_distinct);
      if (!choices || choices > maximum_distinct ||
          expected > maximum_distinct / choices)
        expected = maximum_distinct == UINT64_MAX ? UINT64_MAX
                                                   : maximum_distinct + 1;
      else
        expected *= choices;
      remaining -= end - begin;
      begin = end;
    }

    auto [found, inserted] =
        orbits.emplace(orbit_key, OrbitSummary{expected, 0});
    (void)inserted;
    OrbitSummary &summary = found->second;
    ++summary.distinct;
    if (summary.expected != expected)
      return false;
    for (const uint32_t row : touched_rows)
      row_tokens[row].clear();
  }

  for (const auto &[key, summary] : orbits) {
    (void)key;
    if (summary.distinct != summary.expected)
      return false;
  }
  return true;
}

// Pairwise transports are shared across BFS roots; each traversal composes
// its own frontier order.
class ColumnTransports {
  struct Position {
    size_t column;
    size_t row;
  };
  struct Incidence {
    size_t column;
    size_t source;
    size_t target;
    auto operator<=>(const Incidence &) const = default;
  };

public:
  struct Edge {
    size_t column;
    std::vector<Lit> image;
  };

  bool initialize(OrbitopalPass &pass, const Matrix &columns) {
    neighbors_.resize(columns.size());
    for (size_t column = 0; column < columns.size(); ++column)
      for (size_t row = 0; row < columns[column].size(); ++row) {
        if (!pass.charge(1))
          return false;
        positions_.emplace(columns[column][row], Position{column, row});
      }
    return true;
  }

  bool prepare(OrbitopalPass &pass, const Matrix &columns, size_t column) {
    const Kernel &kernel = pass.kernel;
    if (!pass.charge(1))
      return false;
    if (neighbors_[column]) {
      ++pass.result.reused_column_neighborhoods;
      return true;
    }
    const auto &source = columns[column];
    std::vector<Incidence> incidences;
    for (size_t row = 0; row < source.size(); ++row) {
      for (const WatchEntry watch : kernel.watches.entries(negate(source[row]))) {
        if (!pass.charge(1))
          return false;
        if (!watch.binary())
          continue;
        const auto found = positions_.find(negate(watch.other()));
        if (found == positions_.end() || found->second.column == column)
          continue;
        incidences.push_back({found->second.column, row, found->second.row});
      }
    }
    // Multiple physical copies of a binary clause represent one relation.
    std::sort(incidences.begin(), incidences.end());
    incidences.erase(std::unique(incidences.begin(), incidences.end()),
                     incidences.end());
    std::vector<Edge> neighbors;
    for (size_t begin = 0; begin < incidences.size();) {
      size_t end = begin;
      while (end < incidences.size() &&
             incidences[end].column == incidences[begin].column) {
        if (!pass.charge(1))
          return false;
        ++end;
      }
      if (end - begin == source.size()) {
        const size_t target_column = incidences[begin].column;
        std::vector<uint8_t> used(source.size(), 0);
        Edge edge{target_column, {}};
        edge.image.reserve(source.size());
        bool bijective = true;
        for (size_t row = 0; row < source.size(); ++row) {
          if (!pass.charge(1))
            return false;
          const auto &incidence = incidences[begin + row];
          if (incidence.source != row || used[incidence.target]) {
            bijective = false;
            break;
          }
          used[incidence.target] = 1;
          edge.image.push_back(columns[target_column][incidence.target]);
        }
        if (bijective) {
          neighbors.push_back(std::move(edge));
          ++pass.result.column_transports;
        }
      }
      begin = end;
    }
    if (!pass.charge(0))
      return false;
    neighbors_[column] = std::move(neighbors);
    ++pass.result.column_neighborhoods;
    return true;
  }

  const std::vector<Edge> &neighbors(size_t column) const {
    return *neighbors_[column];
  }

  Lit image(const Edge &edge, Lit source) const {
    return edge.image[positions_.at(source).row];
  }

private:
  std::unordered_map<Lit, Position> positions_;
  std::vector<std::optional<std::vector<Edge>>> neighbors_;
};

bool OrbitopalPass::exact_dimension(const Matrix &matrix,
                                    Matrix &permutations) {
  if (!exact_full_row_group(matrix))
    return false;
  if (result.status != OrbitopalEnumerationStatus::Complete)
    return false;
  permutations.clear();
  permutations.reserve(matrix.size() - 1);
  for (size_t row = 0; row + 1 < matrix.size(); ++row) {
    std::vector<Lit> permutation(2 * kernel.nvars);
    for (Lit literal = 0; literal < permutation.size(); ++literal)
      permutation[literal] = literal;
    for (size_t column = 0; column < matrix[row].size(); ++column) {
      const Lit first = matrix[row][column];
      const Lit second = matrix[row + 1][column];
      permutation[first] = second;
      permutation[negate(first)] = negate(second);
      permutation[second] = first;
      permutation[negate(second)] = negate(first);
    }
    permutations.push_back(std::move(permutation));
  }
  return true;
}

bool OrbitopalPass::order_columns_by_maximum_clique(Matrix &matrix) {
  if (parameters.orbitopal_column_order == OrbitopalColumnOrder::Canonical)
    return true;
  const size_t columns = matrix.front().size();
  if (columns < parameters.minimum_orbitopal_clique_columns)
    return true;

  if (columns > parameters.maximum_orbitopal_clique_vertices)
    return fail(result, traced_orbitopal_limit(__func__, __LINE__));
  const size_t words = (columns + 63) / 64;
  if (columns && words > SIZE_MAX / columns)
    return fail(result, traced_orbitopal_limit(__func__, __LINE__));
  std::vector<std::vector<uint64_t>> adjacency(
      columns, std::vector<uint64_t>(words));
  std::vector<size_t> variable_column(kernel.nvars, columns);
  for (size_t column = 0; column < columns; ++column) {
    const Var variable = var_of(matrix.front()[column]);
    if (variable >= variable_column.size() ||
        variable_column[variable] != columns)
      return fail(result, traced_orbitopal_limit(__func__, __LINE__));
    variable_column[variable] = column;
  }
  std::vector<size_t> seen(columns, 0);
  size_t clause_epoch = 0;
  std::vector<size_t> present;
  for (const auto &clause : formula.clauses) {
    present.clear();
    ++clause_epoch;
    for (const Lit literal : clause) {
      if (!charge(1))
        return false;
      const Var variable = var_of(literal);
      const size_t column = variable < variable_column.size()
                                ? variable_column[variable]
                                : columns;
      if (column != columns && seen[column] != clause_epoch) {
        seen[column] = clause_epoch;
        present.push_back(column);
      }
    }
    for (size_t first = 0; first < present.size(); ++first)
      for (size_t second = first + 1; second < present.size(); ++second) {
        if (!charge(1))
          return false;
        const size_t a = present[first], b = present[second];
        adjacency[a][b / 64] |= uint64_t{1} << (b % 64);
        adjacency[b][a / 64] |= uint64_t{1} << (a % 64);
      }
  }
  MaximumCliqueSearch search(kernel, parameters, stop, result, adjacency);
  std::vector<size_t> clique;
  const bool bounded = parameters.orbitopal_column_order ==
                       OrbitopalColumnOrder::CooccurrenceBoundedGreedyClique;
  const bool searched = bounded || parameters.orbitopal_column_order ==
                                      OrbitopalColumnOrder::CooccurrenceGreedyClique
                            ? search.run_greedy(clique, bounded)
                            : search.run(clique);
  if (!searched)
    return false;
  result.largest_ordering_clique =
      std::max<uint64_t>(result.largest_ordering_clique, clique.size());
  std::vector<uint8_t> selected(columns, 0);
  for (const size_t column : clique)
    selected[column] = 1;
  std::vector<size_t> order;
  order.reserve(columns);
  for (const size_t column : clique)
    order.push_back(column);
  for (size_t column = 0; column < columns; ++column)
    if (!selected[column])
      order.push_back(column);
  for (auto &row : matrix) {
    std::vector<Lit> reordered;
    reordered.reserve(columns);
    for (const size_t column : order)
      reordered.push_back(row[column]);
    row = std::move(reordered);
  }
  return true;
}

bool build_direct_binary_graph(const Kernel &kernel,
                               const LogicalFormula &formula,
                               const GraphSymmetryParameters &settings,
                               LiteralGraph &graph) {
  const uint64_t literal_count = 2ull * kernel.nvars;
  uint64_t nonbinary_clauses = 0;
  uint64_t edge_count = 2ull * kernel.nvars;
  for (const auto &clause : formula.clauses) {
    if (clause.size() == 2) {
      if (edge_count == UINT64_MAX)
        return false;
      ++edge_count;
    } else {
      if (nonbinary_clauses == UINT64_MAX ||
          clause.size() > UINT64_MAX - edge_count)
        return false;
      ++nonbinary_clauses;
      edge_count += clause.size();
    }
  }
  const uint64_t vertex_count =
      literal_count + kernel.nvars + nonbinary_clauses;
  if (literal_count >
          static_cast<uint64_t>(std::numeric_limits<int>::max() - 3) ||
      vertex_count >
          static_cast<uint64_t>(std::numeric_limits<int>::max()) ||
      edge_count > static_cast<uint64_t>(std::numeric_limits<int>::max()) ||
      edge_count > SIZE_MAX / (2 * sizeof(uint32_t)))
    return false;

  graph.reset(kernel, settings.pointwise_root_stabilizer,
              static_cast<uint32_t>(vertex_count), edge_count,
              static_cast<uint32_t>(literal_count));
  // Binary clauses are direct literal edges, as in Satsuma; a color-2 vertex
  // joins each variable's two literals.
  uint32_t next_vertex = static_cast<uint32_t>(literal_count);
  for (Var variable = 0; variable < kernel.nvars; ++variable)
    graph.add_negation_gadget(variable, next_vertex++);
  for (const auto &clause : formula.clauses) {
    if (clause.size() == 2) {
      if (clause[0] == clause[1])
        return false;
      graph.add_edge(clause[0], clause[1]);
      continue;
    }
    for (const Lit literal : clause)
      graph.add_edge(literal, next_vertex);
    ++next_vertex;
  }
  return next_vertex == graph.vertex_count &&
         graph.edges.size() == 2 * graph.edge_count;
}

// Build the direct binary graph within the graph limits and charge its size.
// A limit is traced at the caller's site.
bool OrbitopalPass::admit_direct_graph(LiteralGraph &storage,
                                       const char *function, unsigned line) {
  const GraphSymmetryParameters &limits = parameters.graph_symmetry;
  if (!build_direct_binary_graph(kernel, formula, limits, storage) ||
      storage.vertex_count > limits.maximum_graph_vertices ||
      storage.edge_count > limits.maximum_graph_edges)
    return fail(result, traced_orbitopal_limit(function, line));
  return charge(storage.vertex_count + storage.edge_count);
}

bool OrbitopalPass::prepare_formula_refinement() {
  const uint64_t literal_count = 2ull * kernel.nvars;
  if (literal_count > UINT32_MAX)
    return fail(result, traced_orbitopal_limit(__func__, __LINE__));
  if (!refinement.initialized) {
    refinement.initialized = true;
    LiteralGraph storage;
    if (!admit_direct_graph(storage, __func__, __LINE__))
      return false;
    const symmetry_backend::Graph graph = storage.backend();
    StopContext context{&kernel, stop};
    uint64_t preparation_ticks = 0;
    const uint64_t remaining =
        parameters.maximum_detection_ticks - result.ticks;
    const symmetry_backend::Status prepared =
        symmetry_backend::prepare_refinement(
            graph, remaining, backend_stop, &context, refinement.session,
            preparation_ticks);
    if (!backend_complete(result, prepared, preparation_ticks, __func__,
                          __LINE__))
      return false;
    refinement.literal_count = static_cast<uint32_t>(literal_count);
  }
  if (!refinement.session || refinement.literal_count != literal_count)
    return fail(result, traced_orbitopal_limit(__func__, __LINE__));
  return true;
}

// With `unique_first`, sources whose image column is a unique-literal clause
// of the formula are tried first.
bool OrbitopalPass::complete_matrix_by_refinement(Matrix &matrix,
                                                  bool unique_first) {
  if (parameters.orbitopal_completion == OrbitopalCompletion::MatrixOnly)
    return true;
  const uint64_t literal_count = 2ull * kernel.nvars;
  if (!prepare_formula_refinement())
    return false;
  std::vector<uint32_t> anchors;
  anchors.reserve(matrix.size());
  for (const auto &row : matrix)
    anchors.push_back(row.front());

  const uint64_t remaining = parameters.maximum_detection_ticks - result.ticks;
  StopContext context{&kernel, stop};
  BackendOwner<symmetry_backend::RefinementRows> owned_refined;
  auto &refined = owned_refined.value;
  RowColumnFeasibility feedback(matrix);
  const symmetry_backend::Status status = symmetry_backend::refine_rows(
      *refinement.session, anchors.data(), anchors.size(),
      refinement.literal_count,
      remaining, backend_stop, &context, refined,
      RowColumnFeasibility::observe, &feedback);
  if (!backend_complete(result, status, refined.ticks, __func__, __LINE__))
    return false;
  if (!refined.images)
    return true;

  std::vector<std::unordered_set<Var>> used(matrix.size());
  for (size_t row = 0; row < matrix.size(); ++row)
    for (const Lit literal : matrix[row])
      used[row].insert(var_of(literal));
  for (size_t column = 0; column < matrix.front().size(); ++column) {
    const Lit source = matrix.front()[column];
    for (size_t row = 0; row < matrix.size(); ++row)
      if (refined.images[row * literal_count + source] != matrix[row][column])
        return true;
  }

  std::vector<Lit> source_order;
  if (refined.source_count)
    source_order.assign(refined.sources,
                        refined.sources + refined.source_count);
  if (unique_first) {
    const auto unique_literal_column = [&](Lit source) {
      std::vector<Lit> clause;
      clause.reserve(matrix.size());
      for (size_t row = 0; row < matrix.size(); ++row) {
        const uint32_t target = refined.images[row * literal_count + source];
        if (target >= literal_count || screen.occurrences[target] != 1)
          return false;
        clause.push_back(target);
      }
      return contains_clause(formula, std::move(clause));
    };
    std::stable_partition(source_order.begin(), source_order.end(),
                          unique_literal_column);
  }
  for (const Lit source : source_order) {
    if (source >= literal_count)
      continue;
    if (used.front().count(var_of(source)))
      continue;
    std::vector<Lit> column;
    column.reserve(matrix.size());
    std::unordered_set<Var> column_variables;
    bool complete = true;
    for (size_t row = 0; row < matrix.size(); ++row) {
      const uint32_t target = refined.images[row * literal_count + source];
      if (target >= literal_count || used[row].count(var_of(target)) ||
          !column_variables.insert(var_of(target)).second) {
        complete = false;
        break;
      }
      column.push_back(target);
    }
    if (!complete)
      continue;
    for (size_t row = 0; row < matrix.size(); ++row) {
      matrix[row].push_back(column[row]);
      used[row].insert(var_of(column[row]));
    }
  }
  return true;
}

bool OrbitopalPass::prepare_formula_automorphisms() {
  if (automorphisms.initialized)
    return result.status == OrbitopalEnumerationStatus::Complete &&
           !automorphisms.images.empty();
  automorphisms.initialized = true;
  const uint64_t literal_count = 2ull * kernel.nvars;
  const GraphSymmetryParameters &settings = parameters.graph_symmetry;
  LiteralGraph storage;
  if (!admit_direct_graph(storage, __func__, __LINE__))
    return false;

  const symmetry_backend::DetectionSettings detection =
      sr_detail::detection_settings(settings,
                                    static_cast<uint32_t>(literal_count));
  const symmetry_backend::Graph graph = storage.backend();
  StopContext context{&kernel, stop};
  BackendOwner<symmetry_backend::Permutations> owned_proposed;
  auto &proposed = owned_proposed.value;
  automorphisms.detection_status = symmetry_backend::detect(
      graph, detection, backend_stop, &context, proposed);
  if (automorphisms.detection_status == symmetry_backend::Status::Interrupted)
    return fail(result, OrbitopalEnumerationStatus::Interrupted);
  if (automorphisms.detection_status == symmetry_backend::Status::Invalid ||
      automorphisms.detection_status ==
          symmetry_backend::Status::ResourceExhausted ||
      automorphisms.detection_status == symmetry_backend::Status::Failed)
    return fail(result, traced_orbitopal_limit(__func__, __LINE__));

  automorphisms.literal_count = static_cast<uint32_t>(literal_count);
  automorphisms.images.reserve(static_cast<size_t>(proposed.count) *
                               literal_count);
  std::vector<uint32_t> decoded_image(literal_count);
  for (uint64_t candidate = 0; candidate < proposed.count; ++candidate) {
    if (proposed.domain_size != literal_count)
      break;
    if (!symmetry_backend::materialize_permutation_image(
            proposed, candidate, decoded_image.data()))
      continue;
    const uint32_t *image = decoded_image.data();
    if (!charge(literal_count))
      break;
    // A graph automorphism preserves the clause multiset; checking a signed
    // bijection suffices without rescanning every clause.
    bool valid = true;
    std::vector<uint8_t> seen(literal_count, 0);
    for (Lit source = 0; source < literal_count; ++source) {
      const Lit target = image[source];
      if (target >= literal_count || seen[target] ||
          image[negate(source)] != negate(target) ||
          (kernel_var_retired(kernel, var_of(source)) && target != source)) {
        valid = false;
        break;
      }
      seen[target] = 1;
    }
    if (!valid)
      continue;
    automorphisms.images.insert(automorphisms.images.end(), image,
                                image + literal_count);
  }
  symmetry_backend::release(proposed);
  if (result.status != OrbitopalEnumerationStatus::Complete)
    return false;
  const SymmetryTrace trace;
  trace.permutation_support("orbitopal", automorphisms.images.data(),
                            literal_count ? automorphisms.images.size() / literal_count : 0,
                            literal_count, [&](size_t literal) {
    return kernel.value(static_cast<Lit>(literal)) != 0;
  });
  if (automorphisms.images.empty()) {
    if (automorphisms.detection_status == symmetry_backend::Status::Incomplete)
      result.status = traced_orbitopal_limit(__func__, __LINE__);
    return false;
  }
  return true;
}

bool OrbitopalPass::constrained_dimension_automorphisms(const Matrix &matrix,
                                                        Matrix &permutations) {
  permutations.clear();
  if (!prepare_formula_automorphisms())
    return false;
  const size_t literal_count = automorphisms.literal_count;
  const uint64_t generator_count =
      literal_count ? automorphisms.images.size() / literal_count : 0;

  struct RowGenerator {
    uint64_t full_index;
    std::vector<size_t> image;
    std::vector<size_t> inverse;
    std::vector<size_t> column_image;
    std::vector<size_t> column_inverse;
  };
  std::vector<RowGenerator> row_generators;
  size_t transposition = SIZE_MAX;
  size_t transposed_first = SIZE_MAX, transposed_second = SIZE_MAX;
  const size_t columns = matrix.front().size();
  std::vector<size_t> matrix_cell(literal_count, SIZE_MAX);
  for (size_t row = 0; row < matrix.size(); ++row)
    for (size_t column = 0; column < columns; ++column)
      matrix_cell[matrix[row][column]] = row * columns + column;
  for (uint64_t generator = 0; generator < generator_count; ++generator) {
    const uint32_t *image =
        automorphisms.images.data() + generator * literal_count;
    std::vector<size_t> row_image(matrix.size(), matrix.size());
    std::vector<size_t> column_image(columns, columns);
    bool row_action = true;
    for (size_t row = 0; row < matrix.size(); ++row) {
      const uint32_t target = image[matrix[row][0]];
      if (target >= matrix_cell.size() || matrix_cell[target] == SIZE_MAX) {
        row_action = false;
        break;
      }
      row_image[row] = matrix_cell[target] / columns;
      if (row && matrix_cell[target] % columns != column_image[0]) {
        row_action = false;
        break;
      }
      if (!row)
        column_image[0] = matrix_cell[target] % columns;
    }
    for (size_t column = 1; column < columns && row_action; ++column) {
      const uint32_t target = image[matrix[0][column]];
      if (target >= matrix_cell.size() || matrix_cell[target] == SIZE_MAX ||
          matrix_cell[target] / columns != row_image[0]) {
        row_action = false;
        break;
      }
      column_image[column] = matrix_cell[target] % columns;
    }
    for (size_t row = 0; row < matrix.size() && row_action; ++row)
      for (size_t column = 0; column < columns; ++column)
        if (image[matrix[row][column]] !=
            matrix[row_image[row]][column_image[column]]) {
          row_action = false;
          break;
        }
    std::vector<uint8_t> seen_columns(columns, 0);
    for (const size_t target : column_image) {
      if (target >= columns || seen_columns[target]) {
        row_action = false;
        break;
      }
      seen_columns[target] = 1;
    }
    if (!row_action)
      continue;
    std::vector<size_t> row_inverse(matrix.size(), matrix.size());
    for (size_t row = 0; row < matrix.size(); ++row) {
      if (row_image[row] >= matrix.size() ||
          row_inverse[row_image[row]] != matrix.size()) {
        row_action = false;
        break;
      }
      row_inverse[row_image[row]] = row;
    }
    if (!row_action)
      continue;
    std::vector<size_t> moved;
    for (size_t row = 0; row < matrix.size(); ++row)
      if (row_image[row] != row)
        moved.push_back(row);
    std::vector<size_t> column_inverse(columns, columns);
    for (size_t column = 0; column < columns; ++column)
      column_inverse[column_image[column]] = column;
    bool fixes_columns = true;
    for (size_t column = 0; column < columns; ++column)
      fixes_columns &= column_image[column] == column;
    row_generators.push_back({generator, std::move(row_image),
                              std::move(row_inverse), std::move(column_image),
                              std::move(column_inverse)});
    if (transposition == SIZE_MAX && fixes_columns && moved.size() == 2 &&
        row_generators.back().image[moved[0]] == moved[1] &&
        row_generators.back().image[moved[1]] == moved[0]) {
      transposition = row_generators.size() - 1;
      transposed_first = moved[0];
      transposed_second = moved[1];
    }
  }
  Matrix inverse_images(row_generators.size());
  for (size_t generator = 0; generator < row_generators.size(); ++generator) {
    if (!charge(literal_count))
      return false;
    inverse_images[generator].resize(literal_count);
    const uint32_t *image =
        automorphisms.images.data() +
        row_generators[generator].full_index * literal_count;
    for (Lit literal = 0; literal < literal_count; ++literal)
      inverse_images[generator][image[literal]] = literal;
  }

  std::vector<Lit> base_transposition;
  if (transposition != SIZE_MAX) {
    const uint32_t *image =
        automorphisms.images.data() +
        row_generators[transposition].full_index * literal_count;
    base_transposition.assign(image, image + literal_count);
  } else {
    // Every generator of a product basis may move both dimensions: search the
    // kernel of the induced column action and lift the word through the
    // checked generators.
    std::vector<orbitopal_group::Generator> induced;
    induced.reserve(row_generators.size());
    for (const RowGenerator &generator : row_generators)
      induced.push_back({generator.image, generator.column_image});
    const SymmetryTrace trace;
    if (trace.enabled())
      for (size_t ordinal = 0; ordinal < induced.size(); ++ordinal) {
        const auto &action = induced[ordinal];
        const std::vector<uint32_t> row_image(action.rows.begin(),
                                              action.rows.end());
        const std::vector<uint32_t> column_image(action.columns.begin(),
                                                 action.columns.end());
        trace.permutation("product_kernel_rows", ordinal, row_image.data(),
                          row_image.size());
        trace.permutation("product_kernel_columns", ordinal,
                          column_image.data(), column_image.size());
      }
    const uint64_t remaining =
        parameters.maximum_detection_ticks - result.ticks;
    StopContext context{&kernel, stop};
    const orbitopal_group::Result group =
        orbitopal_group::find_kernel_transposition(
            induced, parameters.maximum_orbitopal_product_states, remaining,
            backend_stop, &context);
    trace.event("product_kernel_search",
                "rows\t%zu\tcolumns\t%zu\tstates\t%llu\tticks\t%llu\tstatus\t%u\tword_nodes\t%zu",
                matrix.size(), columns,
                static_cast<unsigned long long>(group.states),
                static_cast<unsigned long long>(group.ticks),
                static_cast<unsigned>(group.status), group.word.size());
    result.ticks += group.ticks;
    if (group.status == orbitopal_group::Status::Interrupted)
      return fail(result, OrbitopalEnumerationStatus::Interrupted);
    if (group.status == orbitopal_group::Status::LimitReached ||
        group.status == orbitopal_group::Status::Invalid)
      return fail(result, traced_orbitopal_limit(__func__, __LINE__));
    if (group.status != orbitopal_group::Status::Found)
      return false;
    if (group.first >= matrix.size() || group.second >= matrix.size() ||
        group.first == group.second)
      return fail(result, traced_orbitopal_limit(__func__, __LINE__));

    std::vector<std::span<const uint32_t>> encoded_generators;
    encoded_generators.reserve(2 * row_generators.size());
    for (size_t generator = 0; generator < row_generators.size(); ++generator) {
      encoded_generators.emplace_back(
          automorphisms.images.data() +
              row_generators[generator].full_index * literal_count,
          literal_count);
      encoded_generators.emplace_back(inverse_images[generator]);
    }
    auto evaluated = orbitopal_group::evaluate_word(
        group.word, literal_count, encoded_generators,
        parameters.maximum_detection_ticks - result.ticks,
        backend_stop, &context);
    result.ticks += evaluated.ticks;
    if (evaluated.status != orbitopal_group::Status::Found) {
      result.status = evaluated.status == orbitopal_group::Status::Interrupted
                          ? OrbitopalEnumerationStatus::Interrupted
                          : traced_orbitopal_limit(__func__, __LINE__);
      return false;
    }
    base_transposition = std::move(evaluated.image);
    for (size_t row = 0; row < matrix.size(); ++row)
      for (size_t column = 0; column < columns; ++column) {
        if (!charge(1))
          return false;
        const size_t target = row == group.first ? group.second
                              : row == group.second ? group.first : row;
        if (base_transposition[matrix[row][column]] != matrix[target][column])
          return false;
      }
    transposed_first = group.first;
    transposed_second = group.second;
  }

  // If g maps (a,b) to (i,j) then g (a b) g^-1 = (i j): one transposition and
  // a transporter per row pair give every adjacent transposition.
  const size_t rows = matrix.size();
  if (rows > SIZE_MAX / rows)
    return fail(result, traced_orbitopal_limit(__func__, __LINE__));
  const size_t state_count = rows * rows;
  const size_t start = transposed_first * rows + transposed_second;
  std::vector<size_t> parent(state_count, SIZE_MAX);
  std::vector<uint32_t> parent_step(state_count, UINT32_MAX);
  std::vector<size_t> queue = {start};
  parent[start] = start;
  for (size_t at = 0; at < queue.size(); ++at) {
    const size_t state = queue[at];
    const size_t first = state / rows, second = state % rows;
    for (size_t generator = 0; generator < row_generators.size(); ++generator)
      for (uint32_t inverse = 0; inverse < 2; ++inverse) {
        if (!charge(1))
          return false;
        const std::vector<size_t> &step =
            inverse ? row_generators[generator].inverse
                    : row_generators[generator].image;
        const size_t target = step[first] * rows + step[second];
        if (parent[target] != SIZE_MAX)
          continue;
        parent[target] = state;
        parent_step[target] = static_cast<uint32_t>(2 * generator + inverse);
        queue.push_back(target);
      }
  }

  permutations.reserve(rows - 1);
  std::vector<Lit> transporter(literal_count), composed(literal_count),
      transporter_inverse(literal_count), image(literal_count);
  std::vector<uint32_t> path;
  for (size_t source_row = 0; source_row + 1 < rows; ++source_row) {
    const size_t target_state = source_row * rows + source_row + 1;
    if (parent[target_state] == SIZE_MAX)
      return false;
    path.clear();
    for (size_t state = target_state; state != start; state = parent[state])
      path.push_back(parent_step[state]);
    std::reverse(path.begin(), path.end());
    for (Lit literal = 0; literal < literal_count; ++literal)
      transporter[literal] = literal;
    for (const uint32_t encoded : path) {
      if (!charge(literal_count))
        return false;
      const size_t generator = encoded / 2;
      const uint32_t *step = encoded & 1
                                 ? inverse_images[generator].data()
                                 : automorphisms.images.data() +
                                       row_generators[generator].full_index *
                                           literal_count;
      for (Lit literal = 0; literal < literal_count; ++literal)
        composed[literal] = step[transporter[literal]];
      transporter.swap(composed);
    }
    if (!charge(2 * literal_count))
      return false;
    for (Lit literal = 0; literal < literal_count; ++literal)
      transporter_inverse[transporter[literal]] = literal;
    for (Lit literal = 0; literal < literal_count; ++literal)
      image[literal] =
          transporter[base_transposition[transporter_inverse[literal]]];
    bool exact_row = true;
    if (!charge(matrix.size() * matrix.front().size()))
      return false;
    for (size_t row = 0; row < rows && exact_row; ++row)
      for (size_t column = 0; column < matrix.front().size(); ++column) {
        const Lit expected = row == source_row
                                 ? matrix[source_row + 1][column]
                             : row == source_row + 1
                                 ? matrix[source_row][column]
                                 : matrix[row][column];
        if (image[matrix[row][column]] != expected) {
          exact_row = false;
          break;
        }
      }
    if (!exact_row)
      return false;
    // A composition of exact automorphisms needs no whole-formula rescan.
    permutations.push_back(image);
  }
  return true;
}

Matrix transpose(const Matrix &matrix) {
  Matrix transposed(matrix.front().size(), std::vector<Lit>(matrix.size()));
  for (size_t row = 0; row < matrix.size(); ++row)
    for (size_t column = 0; column < matrix.front().size(); ++column)
      transposed[column][row] = matrix[row][column];
  return transposed;
}

bool OrbitopalPass::emit_dimension(const Matrix &matrix,
                                   const std::vector<size_t> &fixable_columns,
                                   const Matrix &permutations) {
  SymmetryTrace trace;
  const size_t fixing_columns =
      std::min(fixable_columns.size(), matrix.size() - 1);
  for (size_t fixing = 0; fixing < fixing_columns; ++fixing) {
    const size_t column = fixable_columns[fixing];
    for (size_t row = 0; row + fixing + 1 < matrix.size(); ++row) {
      if (result.instances == parameters.maximum_orbitopal_instances)
        return fail(result, traced_orbitopal_limit(__func__, __LINE__));
      SubstitutionRedundancyInstance instance;
      const Lit fixed = matrix[row][column];
      const Lit blocked = matrix[row + 1][column];
      instance.clause = {negate(fixed)};
      instance.witness = {negate(fixed), blocked};
      const size_t sources = permutations.front().size() / 2;
      for (size_t other = 0; other < sources; ++other) {
        const Lit first = make_lit(static_cast<Var>(other), false);
        const Lit second = permutations[row][first];
        if (first == second)
          continue;
        if (var_of(first) == var_of(fixed) ||
            var_of(first) == var_of(blocked) ||
            var_of(second) == var_of(fixed) ||
            var_of(second) == var_of(blocked))
          continue;
        // Each source variable is visited once, so inverse moves appear too.
        instance.substitution.emplace_back(first, second);
      }
      if (row < permutations.size()) {
        const std::vector<Lit> &action = permutations[row];
        bool certified = fixed < action.size() && blocked < action.size() &&
                         action[fixed] == blocked &&
                         action[blocked] == fixed;
        std::vector<Lit> image;
        image.reserve(prefix_units.size());
        for (const Lit unit : prefix_units) {
          if (unit >= action.size()) {
            certified = false;
            break;
          }
          image.push_back(action[unit]);
        }
        if (certified) {
          std::sort(image.begin(), image.end());
          std::vector<Lit> expected = prefix_units;
          std::sort(expected.begin(), expected.end());
          certified = image == expected;
        }
        instance.orbitopal_prefix_certificate = certified;
      }
      ++result.instances;
      trace.event("orbitopal_fixing_literal",
                  "rows\t%zu\twidth\t%zu\tcolumn\t%zu\trow\t%zu\tunit\t%d\tblocked\t%d",
                  matrix.size(), matrix.front().size(), column, row,
                  externalize(negate(fixed)), externalize(blocked));
      if (!visitor(instance))
        return false;
      prefix_units.push_back(negate(fixed));
    }
  }
  return true;
}

std::vector<size_t>
OrbitopalPass::unique_literal_columns(const Matrix &matrix) const {
  std::vector<size_t> fixable;
  std::unordered_set<Lit> claimed;
  for (size_t column = 0; column < matrix.front().size(); ++column) {
    std::vector<Lit> clause;
    clause.reserve(matrix.size());
    bool unique = true;
    for (const auto &row : matrix) {
      const Lit literal = row[column];
      unique &= literal < screen.occurrences.size() &&
                screen.occurrences[literal] == 1;
      clause.push_back(literal);
    }
    const bool present = contains_clause(formula, clause);
    if (!unique || claimed.count(negate(clause.front())) || !present)
      continue;
    fixable.push_back(column);
    claimed.insert(clause.begin(), clause.end());
  }
  return fixable;
}

// Admit one more refinement orbit; the limit is traced at the caller's site.
bool OrbitopalPass::admit_refinement_orbit(uint64_t candidate_size,
                                           const char *function,
                                           unsigned line) {
  if (result.refinement_orbits ==
      parameters.maximum_orbitopal_refinement_orbits)
    return fail(result, traced_orbitopal_limit(function, line));
  ++result.refinement_orbits;
  result.largest_refinement_orbit =
      std::max(result.largest_refinement_orbit, candidate_size);
  return true;
}

template <typename Individualize>
bool OrbitopalPass::product_child_partition(const std::vector<Lit> &orbit,
                                            ProductChildPartition &child,
                                            Individualize individualize) {
  BackendOwner<symmetry_backend::IndividualizedPartition> owned_partition;
  auto &partition = owned_partition.value;
  const uint64_t remaining = parameters.maximum_detection_ticks - result.ticks;
  StopContext context{&kernel, stop};
  const symmetry_backend::Status status =
      individualize(remaining, &context, partition);
  if (!backend_complete(result, status, partition.ticks, __func__, __LINE__))
    return false;
  child.cells.resize(orbit.size());
  child.sizes.clear();
  for (size_t at = 0; at < orbit.size(); ++at) {
    const Lit literal = orbit[at];
    const uint32_t cell = partition.cells[literal];
    child.cells[at] = cell;
    ++child.sizes[cell];
  }
  return true;
}

bool OrbitopalPass::product_child_partition(
    symmetry_backend::RefinementBranch &branch, const std::vector<Lit> &orbit,
    Lit anchor, ProductChildPartition &child) {
  return product_child_partition(
      orbit, child,
      [&](uint64_t remaining, StopContext *context,
          symmetry_backend::IndividualizedPartition &partition) {
        return symmetry_backend::individualized_partition(
            branch, anchor, refinement.literal_count, remaining, backend_stop,
            context, partition);
      });
}

bool OrbitopalPass::product_child_partition(const std::vector<Lit> &orbit,
                                            Lit anchor,
                                            ProductChildPartition &child) {
  return product_child_partition(
      orbit, child,
      [&](uint64_t remaining, StopContext *context,
          symmetry_backend::IndividualizedPartition &partition) {
        return symmetry_backend::individualized_partition(
            *refinement.session, &anchor, 1, refinement.literal_count,
            remaining, backend_stop, context, partition);
      });
}

bool OrbitopalPass::product_matrix_from_refinement(
    const std::vector<Lit> &orbit, const std::vector<Lit> &first_side,
    const std::vector<Lit> &second_side, Matrix &matrix) {
  const Lit anchor = orbit.front();
  std::unordered_map<Lit, size_t> position;
  position.reserve(orbit.size());
  for (size_t at = 0; at < orbit.size(); ++at)
    position.emplace(orbit[at], at);

  std::vector<Lit> row = {anchor}, column = {anchor};
  row.insert(row.end(), first_side.begin(), first_side.end());
  column.insert(column.end(), second_side.begin(), second_side.end());
  if (row.size() > SIZE_MAX / column.size() ||
      row.size() * column.size() != orbit.size())
    return true;

  const Lit missing = std::numeric_limits<Lit>::max();
  std::vector<Lit> in_row(orbit.size(), missing),
      in_column(orbit.size(), missing);
  const auto assign = [&](Lit literal, Lit row_rep, Lit column_rep) {
    const auto found = position.find(literal);
    if (found == position.end())
      return false;
    const size_t at = found->second;
    if (row_rep != missing)
      in_row[at] = row_rep;
    if (column_rep != missing)
      in_column[at] = column_rep;
    return true;
  };
  if (!assign(anchor, anchor, anchor))
    return true;
  for (const Lit literal : first_side)
    if (!assign(literal, anchor, literal))
      return true;
  for (const Lit literal : second_side)
    if (!assign(literal, literal, anchor))
      return true;

  ProductChildPartition child;
  const auto assign_side = [&](const std::vector<Lit> &side,
                               std::vector<Lit> &assigned, size_t other_size) {
    for (size_t index = 1; index < side.size(); ++index) {
      if (!admit_refinement_orbit(orbit.size(), __func__, __LINE__) ||
          !product_child_partition(orbit, side[index], child))
        return false;
      uint32_t largest = 0;
      for (const auto &[cell, size] : child.sizes) {
        (void)cell;
        largest = std::max(largest, size);
      }
      for (size_t at = 0; at < orbit.size(); ++at) {
        const uint32_t size = child.sizes.at(child.cells[at]);
        if (assigned[at] == missing &&
            (size < largest || largest == other_size - 1))
          assigned[at] = side[index];
      }
    }
    return true;
  };
  if (!assign_side(row, in_column, column.size()) ||
      !assign_side(column, in_row, row.size()))
    return false;

  std::unordered_map<Lit, size_t> row_index, column_index;
  for (size_t at = 0; at < column.size(); ++at)
    row_index.emplace(column[at], at);
  for (size_t at = 0; at < row.size(); ++at)
    column_index.emplace(row[at], at);
  matrix.assign(column.size(), std::vector<Lit>(row.size(), missing));
  for (size_t at = 0; at < orbit.size(); ++at) {
    const auto row_at = row_index.find(in_row[at]);
    const auto column_at = column_index.find(in_column[at]);
    if (row_at == row_index.end() || column_at == column_index.end() ||
        matrix[row_at->second][column_at->second] != missing) {
      matrix.clear();
      return true;
    }
    matrix[row_at->second][column_at->second] = orbit[at];
  }
  for (const auto &matrix_row : matrix)
    if (std::find(matrix_row.begin(), matrix_row.end(), missing) !=
        matrix_row.end()) {
      matrix.clear();
      return true;
    }
  return true;
}

// Admit a complete matrix atomically: neither a partial basis nor its scope
// is visible after a budget or stop.
bool OrbitopalPass::collect_matrix_generators(const Matrix &matrix,
                                              Matrix &rows, Matrix &columns) {
  const uint64_t count = rows.size() + columns.size();
  if (result.matrices == parameters.maximum_orbitopal_matrices ||
      count > parameters.graph_symmetry.maximum_generators -
                  hints->generators.size())
    return fail(result, traced_orbitopal_limit(__func__, __LINE__));
  for (const auto *basis : {&rows, &columns})
    for (const auto &image : *basis)
      if (!charge(image.size()))
        return false;
  for (const auto &row : matrix)
    if (!charge(row.size()))
      return false;
  for (auto *basis : {&rows, &columns})
    for (auto &image : *basis)
      hints->generators.push_back(std::move(image));
  if (parameters.graph_symmetry.structured_scope == SymmetryStructuredScope::WholeMatrix) {
    for (const auto &row : matrix)
      for (const Lit literal : row)
        hints->fixed_variables.push_back(var_of(literal));
  } else {
    if (!rows.empty())
      for (const auto &row : matrix)
        hints->fixed_variables.push_back(var_of(row.front()));
    if (!columns.empty())
      for (const Lit literal : matrix.front())
        hints->fixed_variables.push_back(var_of(literal));
  }
  ++result.matrices;
  return true;
}

bool OrbitopalPass::emit_refined_product_orbits(bool &emitted) {
  const SymmetryTrace trace;
  emitted = false;
  if (parameters.orbitopal_orientation != OrbitopalOrientation::Both)
    return true;
  if (!prepare_formula_refinement())
    return false;
  BackendOwner<symmetry_backend::RefinementPartition> owned_root;
  auto &root = owned_root.value;
  const symmetry_backend::Status root_status =
      symmetry_backend::root_partition(*refinement.session,
                                       refinement.literal_count, root);
  if (!backend_complete(result, root_status, 0, __func__, __LINE__))
    return false;
  std::map<uint32_t, std::vector<Lit>> root_cells;
  for (Lit literal = 0; literal < root.domain_size; ++literal)
    root_cells[root.cells[literal]].push_back(literal);
  symmetry_backend::release(root);

  std::vector<uint8_t> tested(refinement.literal_count, 0);
  RefinementProgressFeedback progress(kernel, parameters, result);
  for (auto &[cell, orbit] : root_cells) {
    if (progress.stop(result, "product"))
      break;
    (void)cell;
    if (parameters.minimum_orbitopal_rows >
            SIZE_MAX / parameters.minimum_orbitopal_columns ||
        orbit.size() < static_cast<size_t>(parameters.minimum_orbitopal_rows) *
                           parameters.minimum_orbitopal_columns)
      continue;
    if (tested[negate(orbit.front())])
      continue;
    for (const Lit literal : orbit)
      tested[literal] = 1;
    BackendOwner<symmetry_backend::RefinementBranch *> owned_branch;
    auto &branch = owned_branch.value;
    const symmetry_backend::Status branch_status =
        symmetry_backend::prepare_refinement_branch(*refinement.session,
                                                    branch);
    if (branch_status != symmetry_backend::Status::Complete)
      return fail(result, traced_orbitopal_limit(__func__, __LINE__));
    std::vector<Lit> candidate = orbit;
    Matrix matrix;
    while (candidate.size() >=
           static_cast<size_t>(parameters.minimum_orbitopal_rows) *
               parameters.minimum_orbitopal_columns) {
      if (progress.stop(result, "product"))
        return true;
      if (!admit_refinement_orbit(candidate.size(), __func__, __LINE__))
        return false;
      const Lit anchor = candidate.front();
      ProductChildPartition anchor_partition;
      if (!product_child_partition(*branch, candidate, anchor,
                                   anchor_partition))
        return false;
      std::map<uint32_t, std::vector<Lit>> groups;
      for (size_t at = 0; at < candidate.size(); ++at)
        groups[anchor_partition.cells[at]].push_back(candidate[at]);

      uint32_t largest_cell = anchor_partition.cells.front();
      size_t largest_size = 0;
      std::unordered_set<uint32_t> seen_cells;
      for (size_t at = 0; at < candidate.size(); ++at) {
        const uint32_t cell_id = anchor_partition.cells[at];
        if (!seen_cells.insert(cell_id).second)
          continue;
        const size_t size = groups.at(cell_id).size();
        if (size > largest_size) {
          largest_size = size;
          largest_cell = cell_id;
        }
      }

      const uint32_t anchor_cell = anchor_partition.cells.front();
      uint32_t side_cell = anchor_cell;
      bool found_side = false;
      for (size_t at = 0; at < candidate.size(); ++at) {
        const uint32_t cell_id = anchor_partition.cells[at];
        const size_t size = groups.at(cell_id).size();
        if (size > 1 && size < largest_size) {
          side_cell = cell_id;
          found_side = true;
          break;
        }
      }
      std::vector<Lit> other_side;
      if (found_side)
        for (const auto &[cell_id, members] : groups)
          if (cell_id != anchor_cell && cell_id != side_cell &&
              cell_id != largest_cell)
            other_side.insert(other_side.end(), members.begin(),
                              members.end());

      std::unordered_set<Var> variables;
      bool signed_action = true;
      for (const Lit literal : candidate)
        signed_action &= variables.insert(var_of(literal)).second;
      const auto anchor_group = groups.find(anchor_cell);
      const bool plausible =
          signed_action && groups.size() == 4 &&
          anchor_group != groups.end() && anchor_group->second.size() == 1 &&
          found_side && !other_side.empty() &&
          groups.at(side_cell).size() <= SIZE_MAX / other_side.size() &&
          groups.at(largest_cell).size() ==
              groups.at(side_cell).size() * other_side.size() &&
          (groups.at(side_cell).size() + 1) <=
              SIZE_MAX / (other_side.size() + 1) &&
          (groups.at(side_cell).size() + 1) * (other_side.size() + 1) ==
              candidate.size();
      if (plausible) {
        ++result.product_shapes;
        result.largest_product_rows =
            std::max<uint64_t>(result.largest_product_rows,
                               groups.at(side_cell).size() + 1);
        result.largest_product_columns =
            std::max<uint64_t>(result.largest_product_columns,
                               other_side.size() + 1);
        if (!product_matrix_from_refinement(candidate, groups.at(side_cell),
                                            other_side, matrix) ||
            (matrix.empty() &&
             !product_matrix_from_refinement(candidate, other_side,
                                             groups.at(side_cell), matrix)))
          return false;
        break;
      }

      const auto largest = groups.find(largest_cell);
      if (largest == groups.end() || largest->second.size() <= 1 ||
          largest->second.size() >= candidate.size())
        break;
      candidate = largest->second;
      ++result.product_stabilizer_descents;
    }
    symmetry_backend::release(branch);
    if (matrix.empty() ||
        matrix.size() < parameters.minimum_orbitopal_rows ||
        matrix.front().size() < parameters.minimum_orbitopal_columns)
      continue;

    Matrix transposed = transpose(matrix);
    Matrix row_permutations, column_permutations;
    // A matrix action may also move coupled variables outside the matrix;
    // neither an incomplete axis nor an unchecked backend map is published.
    const auto check_axis = [&](const char *axis, bool enabled,
                                const Matrix &rows, Matrix &permutations,
                                bool &exact) {
      exact = enabled && exact_dimension(rows, permutations);
      if (result.status != OrbitopalEnumerationStatus::Complete)
        return false;
      trace.event("product_axis_check",
                  "axis\t%s\trows\t%zu\tcolumns\t%zu\tpure\t%u", axis,
                  matrix.size(), matrix.front().size(),
                  static_cast<unsigned>(exact));
      if (enabled && !exact && parameters.orbitopal_generator_fallback) {
        ++result.generator_fallbacks;
        exact = constrained_dimension_automorphisms(rows, permutations);
      }
      return result.status == OrbitopalEnumerationStatus::Complete;
    };
    bool rows_exact = false, columns_exact = false;
    if (!check_axis("rows", true, matrix, row_permutations, rows_exact) ||
        !check_axis("columns", rows_exact, transposed, column_permutations,
                    columns_exact))
      return false;
    trace.event("product_axes_completed", "rows\t%zu\tcolumns\t%zu\trow_valid\t%u\tcolumn_valid\t%u",
                matrix.size(), matrix.front().size(),
                static_cast<unsigned>(rows_exact), static_cast<unsigned>(columns_exact));
    if (!rows_exact || !columns_exact) {
      ++result.refinement_exact_failures;
      continue;
    }
    ++result.product_exact_actions;
    if (hints) {
      if (!collect_matrix_generators(matrix, row_permutations,
                                     column_permutations))
        return false;
      emitted = true;
      continue;
    }

    auto all_unique = [&](const Matrix &candidate) {
      return unique_literal_columns(candidate).size() ==
             candidate.front().size();
    };
    bool rows_ulc = all_unique(transposed);
    bool columns_ulc = all_unique(matrix);
    if (!rows_ulc && !columns_ulc) {
      for (auto &matrix_row : matrix)
        for (Lit &literal : matrix_row)
          literal = negate(literal);
      transposed = transpose(matrix);
      rows_ulc = all_unique(transposed);
      columns_ulc = all_unique(matrix);
    }
    if (!rows_ulc && !columns_ulc)
      continue;
    ++result.product_ulc_actions;
    if (result.matrices == parameters.maximum_orbitopal_matrices)
      return fail(result, traced_orbitopal_limit(__func__, __LINE__));
    ++result.matrices;
    const uint64_t before = result.instances;
    const Matrix &fixing = rows_ulc ? transposed : matrix;
    std::vector<size_t> fixable(fixing.front().size());
    for (size_t column = 0; column < fixable.size(); ++column)
      fixable[column] = column;
    if (!emit_dimension(fixing, fixable,
                        rows_ulc ? column_permutations : row_permutations))
      return false;
    emitted |= result.instances != before;
  }
  return true;
}

bool OrbitopalPass::emit_refined_row_candidates(bool &emitted) {
  const SymmetryTrace trace;
  emitted = false;
  if (parameters.orbitopal_orientation == OrbitopalOrientation::Columns)
    return true;
  if (!trace.measure("row_prepare_refinement",
                     [&] { return prepare_formula_refinement(); }))
    return false;
  const bool refinement_cells = parameters.orbitopal_row_orbit_source ==
      OrbitopalRowOrbitSource::RefinementCells;
  const uint32_t literal_count = refinement.literal_count;
  uint64_t generator_count = 0;
  std::map<uint32_t, std::vector<Lit>> orbit_map;
  if (refinement_cells) {
    // Satsuma tries equitable cells before generic automorphism search; a
    // cell only proposes candidates for the exact action checks.
    if (!charge(literal_count))
      return false;
    BackendOwner<symmetry_backend::RefinementPartition> owned_partition;
    auto &partition = owned_partition.value;
    const auto status = symmetry_backend::root_partition(
        *refinement.session, literal_count, partition);
    if (!backend_complete(result, status, 0, __func__, __LINE__))
      return false;
    for (Lit literal = 0; literal < literal_count; ++literal)
      orbit_map[partition.cells[literal]].push_back(literal);
    if (!charge(0))
      return false;
    trace.event("row_refinement_cells", "cells\t%zu", orbit_map.size());
  } else {
    if (!trace.measure("row_prepare_automorphisms",
                       [&] { return prepare_formula_automorphisms(); }))
      return result.status == OrbitopalEnumerationStatus::Complete;
    // Each root is its orbit's minimum literal, so the map keys are stable.
    DisjointSets orbits_of(literal_count);
    generator_count =
        literal_count ? automorphisms.images.size() / literal_count : 0;
    for (uint64_t generator = 0; generator < generator_count; ++generator) {
      const uint32_t *image =
          automorphisms.images.data() + generator * literal_count;
      for (uint32_t literal = 0; literal < literal_count; ++literal) {
        if (!charge(1))
          return false;
        orbits_of.unite(literal, image[literal]);
      }
    }
    for (Lit literal = 0; literal < literal_count; ++literal)
      orbit_map[orbits_of.find(literal)].push_back(literal);
  }
  Matrix orbits;
  for (auto &[cell, orbit] : orbit_map) {
    (void)cell;
    if (orbit.size() < parameters.minimum_orbitopal_rows)
      continue;
    std::unordered_set<Var> variables;
    bool signed_action = true;
    for (const Lit literal : orbit)
      signed_action &= variables.insert(var_of(literal)).second;
    if (refinement_cells || signed_action)
      orbits.push_back(std::move(orbit));
  }
  std::sort(orbits.begin(), orbits.end());
  trace.event("row_orbits", "generators\t%llu\torbits\t%zu\tclauses\t%zu",
              static_cast<unsigned long long>(generator_count), orbits.size(),
              formula.clauses.size());

  uint32_t probed_base_length = UINT32_MAX;
  if (parameters.orbitopal_base_probe_filter) {
    const uint64_t remaining =
        parameters.maximum_detection_ticks - result.ticks;
    StopContext context{&kernel, stop};
    uint64_t probe_ticks = 0;
    const symmetry_backend::Status probe_status =
        symmetry_backend::probe_base_length(
            *refinement.session, remaining, backend_stop, &context,
            probed_base_length, probe_ticks);
    if (!backend_complete(result, probe_status, probe_ticks, __func__,
                          __LINE__))
      return false;
  }
  std::vector<uint8_t> claimed(kernel.nvars, 0);
  std::vector<Lit> last_inexact_action;
  RefinementProgressFeedback progress(kernel, parameters, result);
  for (const std::vector<Lit> &orbit : orbits) {
    if (progress.stop(result, "rows"))
      break;
    if (parameters.orbitopal_base_probe_filter &&
        orbit.size() >
            static_cast<uint64_t>(parameters.orbitopal_base_probe_factor) *
                probed_base_length)
      continue;
    bool overlaps = false;
    for (const Lit literal : orbit)
      overlaps |= claimed[var_of(literal)] != 0;
    if (overlaps)
      continue;
    if (!admit_refinement_orbit(orbit.size(), __func__, __LINE__))
      return false;
    trace.event("row_candidate", "ordinal\t%llu\tanchor\t%u\tsize\t%zu",
                static_cast<unsigned long long>(result.refinement_orbits),
                orbit.front(), orbit.size());
    BackendOwner<symmetry_backend::RefinementOrbit> owned_reduced;
    auto &reduced = owned_reduced.value;
    const uint64_t remaining = parameters.maximum_detection_ticks - result.ticks;
    StopContext context{&kernel, stop};
    const symmetry_backend::Status reduction_status = trace.measure(
        "row_reduce_orbit", [&] {
          return symmetry_backend::reduce_orbit(
            *refinement.session, orbit.data(), orbit.size(),
            refinement.literal_count, remaining, backend_stop, &context,
            reduced);
        });
    trace.event("row_reduced", "count\t%u\tticks\t%llu\tstatus\t%u",
                reduced.count, static_cast<unsigned long long>(reduced.ticks),
                static_cast<unsigned>(reduction_status));
    if (!backend_complete(result, reduction_status, reduced.ticks, __func__,
                          __LINE__))
      return false;
    if (reduced.count < parameters.minimum_orbitopal_rows)
      continue;
    result.largest_reduced_orbit =
        std::max<uint64_t>(result.largest_reduced_orbit, reduced.count);
    result.reduced_refinement_orbits += reduced.count < orbit.size();
    Matrix matrix;
    matrix.reserve(reduced.count);
    for (uint32_t row = 0; row < reduced.count; ++row)
      matrix.push_back({reduced.vertices[row]});
    symmetry_backend::release(reduced);
    if (!trace.measure("row_complete_matrix",
                       [&] { return complete_matrix_by_refinement(matrix, true); }))
      return false;
    trace.event("row_matrix", "rows\t%zu\tcolumns\t%zu",
                matrix.size(), matrix.front().size());
    if (matrix.front().size() < parameters.minimum_orbitopal_columns)
      continue;
    result.largest_refinement_columns =
        std::max<uint64_t>(result.largest_refinement_columns,
                           matrix.front().size());
    std::vector<Lit> action_key;
    if (!parameters.orbitopal_generator_fallback) {
      if (!exact_row_action_key(matrix, action_key) ||
          !charge(action_key.size()))
        return false;
      if (action_key == last_inexact_action) {
        ++result.reused_inexact_matrices;
        trace.event("row_reuse_inexact", "rows\t%zu\tcolumns\t%zu",
                    matrix.size(), matrix.front().size());
        continue;
      }
    }
    // Column order matters only for emitted fixing clauses, not collection.
    if (!hints && !trace.measure("row_order_columns", [&] {
          return order_columns_by_maximum_clique(matrix);
        }))
      return false;

    Matrix permutations;
    bool exact = trace.measure("row_exact_dimension", [&] {
      return exact_dimension(matrix, permutations);
    });
    if (result.status != OrbitopalEnumerationStatus::Complete)
      return false;
    result.refinement_exact_failures += !exact;
    if (!exact && parameters.orbitopal_generator_fallback) {
      ++result.generator_fallbacks;
      exact = trace.measure("row_generator_fallback", [&] {
        return constrained_dimension_automorphisms(matrix, permutations);
      });
      if (result.status != OrbitopalEnumerationStatus::Complete)
        return false;
    }
    trace.event("row_exact_result", "exact\t%u\tticks\t%llu",
                static_cast<unsigned>(exact),
                static_cast<unsigned long long>(result.ticks));
    if (!exact) {
      if (!parameters.orbitopal_generator_fallback)
        last_inexact_action = std::move(action_key);
      continue;
    }
    if (hints) {
      if (trace.enabled())
        trace.event("row_handoff", "rows\t%zu\tcolumns\t%zu\tfixable_columns\t%zu",
                    matrix.size(), matrix.front().size(),
                    unique_literal_columns(matrix).size());
      Matrix no_columns;
      if (!collect_matrix_generators(matrix, permutations, no_columns))
        return false;
      for (const auto &row : matrix)
        for (const Lit literal : row)
          claimed[var_of(literal)] = 1;
      emitted = true;
      continue;
    }
    const std::vector<size_t> fixable = trace.measure(
        "row_unique_columns", [&] { return unique_literal_columns(matrix); });
    trace.event("row_fixable", "columns\t%zu", fixable.size());
    result.refinement_unique_literal_columns += fixable.size();
    if (fixable.empty())
      continue;
    if (result.matrices == parameters.maximum_orbitopal_matrices)
      return fail(result, traced_orbitopal_limit(__func__, __LINE__));
    ++result.matrices;
    for (const auto &row : matrix)
      for (const Lit literal : row)
        claimed[var_of(literal)] = 1;
    const uint64_t before = result.instances;
    if (!emit_dimension(matrix, fixable, permutations))
      return false;
    emitted |= result.instances != before;
  }
  return true;
}

OrbitopalEnumerationResult OrbitopalPass::enumerate() {
  const SymmetryTrace trace;
  trace.event("orbitopal_begin",
              "variables\t%u\tcompletion\t%u\torientation\t%u\tclique\t%u",
              kernel.nvars, static_cast<unsigned>(parameters.orbitopal_completion),
              static_cast<unsigned>(parameters.orbitopal_orientation),
              static_cast<unsigned>(parameters.orbitopal_column_order));
  if (!parameters.valid() ||
      parameters.producer != SubstitutionRedundancyProducer::Orbitopal ||
      (!hints && !parameters.maximum_orbitopal_instances) ||
      !parameters.maximum_detection_ticks)
    return result;
  if (!trace.measure("orbitopal_screen", [&] { return screen_formula(); }))
    return result;

  trace.event("orbitopal_screened", "candidates\t%zu",
              screen.candidate_clauses.size());

  // Satsuma first exposes the AMO of each unique-literal clause C: for a, b in
  // C, (-a | -b) is blocked on -a. Each emitted pair counts as occurrences, so
  // a later clause it touches is no longer unique. A productive phase ends the
  // pass; the next pass detects matrices over the emitted binaries.
  if (!hints && parameters.complete_unique_literal_amos) {
    trace.event("orbitopal_amo_completion", "boundary\tbegin");
    bool emitted = false;
    std::unordered_set<uint64_t> scheduled_binaries;
    for (const std::vector<Lit> &clause : screen.ulc_candidate_clauses) {
      bool currently_unique = true;
      for (const Lit literal : clause) {
        if (!charge(1))
          return result;
        currently_unique &= screen.ulc_occurrences[literal] == 1;
      }
      if (!currently_unique)
        continue;
      for (size_t first = 0; first < clause.size(); ++first) {
        for (size_t second = first + 1; second < clause.size(); ++second) {
          if (!charge(1))
            return result;
          const Lit a = negate(clause[first]);
          const Lit b = negate(clause[second]);
          const uint64_t binary = kernel_binary_key(a, b);
          if (kernel_binary_live(kernel, a, b) ||
              !scheduled_binaries.insert(binary).second)
            continue;
          if (result.instances == parameters.maximum_orbitopal_instances) {
            result.status = traced_orbitopal_limit("enumerate_orbitopal",
                                                   __LINE__);
            return result;
          }
          SubstitutionRedundancyInstance instance;
          instance.clause = {a, b};
          // Under the pointwise query stabilizer, orient the pair on a pivot
          // that preserves roots and A; withhold it if neither does.
          if (parameters.graph_symmetry.pointwise_root_stabilizer &&
              kernel.query_projection) {
            const auto preserves = [&](Lit pivot) {
              return kernel.value(pivot) >= 0 &&
                  kernel.query_projection->preserves_constant(kernel_external_lit(kernel, pivot));
            };
            if (!preserves(a)) {
              if (!preserves(b))
                continue;
              std::swap(instance.clause[0], instance.clause[1]);
            }
          }
          instance.unique_literal_amo_certificate = true;
          ++result.instances;
          emitted = true;
          if (!visitor(instance))
            return result;
          ++screen.ulc_occurrences[a];
          ++screen.ulc_occurrences[b];
        }
      }
    }
    trace.event("orbitopal_amo_completion", "boundary\tend\tinstances\t%llu",
                static_cast<unsigned long long>(result.instances));
    if (emitted)
      return result;
  }

  std::vector<uint8_t> claimed(kernel.nvars, 0);
  std::map<size_t, Matrix> groups;
  for (std::vector<Lit> &clause : screen.candidate_clauses) {
    bool overlaps = false;
    for (const Lit literal : clause)
      overlaps |= claimed[var_of(literal)] != 0;
    if (overlaps)
      continue;
    for (const Lit literal : clause)
      claimed[var_of(literal)] = 1;
    groups[clause.size()].push_back(std::move(clause));
  }

  if (!parameters.maximum_orbitopal_matrices)
    return result;

  // The whole-formula snapshot is taken once, on first use.
  bool snapshotted = false;
  const auto ensure_formula = [&] {
    if (snapshotted)
      return true;
    snapshotted = true;
    return trace.measure("orbitopal_snapshot", [&] { return full_snapshot(); });
  };
  const bool complete_action =
      parameters.orbitopal_completion != OrbitopalCompletion::MatrixOnly;
  const auto emit_refinement_phase = [&](bool &emitted) {
    emitted = false;
    if (!complete_action ||
        parameters.orbitopal_orientation == OrbitopalOrientation::Columns)
      return true;
    // Check the direct-binary refinement graph's lower bound (two literal
    // vertices and a gadget per variable) before copying the formula.
    if (3ull * kernel.nvars > parameters.graph_symmetry.maximum_graph_vertices ||
        3ull * kernel.nvars > static_cast<uint64_t>(std::numeric_limits<int>::max()) ||
        2ull * kernel.nvars > parameters.graph_symmetry.maximum_graph_edges)
      return fail(result,
                  traced_orbitopal_limit("enumerate_orbitopal", __LINE__));
    if (!ensure_formula())
      return false;
    bool product_emitted = false;
    if (!trace.measure("orbitopal_product", [&] {
          return emit_refined_product_orbits(product_emitted);
        }))
      return false;
    if (product_emitted) {
      emitted = true;
      return true;
    }
    return trace.measure("orbitopal_generator_orbits", [&] {
      return emit_refined_row_candidates(emitted);
    });
  };

  if (parameters.orbitopal_discovery_order ==
          OrbitopalDiscoveryOrder::RefinementThenUniqueLiteral ||
      parameters.orbitopal_discovery_order ==
          OrbitopalDiscoveryOrder::RefinementOnly) {
    bool emitted = false;
    if (!emit_refinement_phase(emitted))
      return result;
    if (emitted || parameters.orbitopal_discovery_order ==
                       OrbitopalDiscoveryOrder::RefinementOnly)
      return result;
  }

  std::vector<Lit> last_inexact_matrix;
  for (auto &[row_count, columns] : groups) {
    std::sort(columns.begin(), columns.end());
    ColumnTransports transports;
    if (!transports.initialize(*this, columns))
      return result;
    std::vector<uint8_t> used(columns.size(), 0);
    for (size_t seed = 0; seed < columns.size(); ++seed) {
      if (used[seed])
        continue;
      if (stop_requested(kernel, stop)) {
        result.status = OrbitopalEnumerationStatus::Interrupted;
        return result;
      }
      trace.event("orbitopal_align",
                  "boundary\tbegin\trows\t%zu\tcolumns\t%zu\tseed\t%zu",
                  row_count, columns.size(), seed);
      Matrix aligned_columns = {columns[seed]};
      std::vector<size_t> members = {seed};
      std::vector<uint8_t> in_component(columns.size(), 0);
      in_component[seed] = 1;
      // Column compatibility is a graph: compose each aligned column into the
      // common row order so A--B--C connectivity finds the whole action.
      for (size_t frontier = 0; frontier < aligned_columns.size(); ++frontier) {
        if (!transports.prepare(*this, columns, members[frontier]))
          return result;
        for (const auto &edge : transports.neighbors(members[frontier])) {
          if (!charge(1))
            return result;
          const size_t candidate = edge.column;
          if (used[candidate] || in_component[candidate])
            continue;
          std::vector<Lit> aligned;
          aligned.reserve(row_count);
          for (const Lit literal : aligned_columns[frontier]) {
            if (!charge(1))
              return result;
            aligned.push_back(transports.image(edge, literal));
          }
          in_component[candidate] = 1;
          aligned_columns.push_back(std::move(aligned));
          members.push_back(candidate);
        }
      }
      trace.event("orbitopal_align",
                  "boundary\tend\taligned\t%zu\tneighborhoods\t%llu"
                  "\treused\t%llu\ttransports\t%llu",
                  aligned_columns.size(),
                  static_cast<unsigned long long>(result.column_neighborhoods),
                  static_cast<unsigned long long>(result.reused_column_neighborhoods),
                  static_cast<unsigned long long>(result.column_transports));
      // A single ULC anchors completion; the width is checked on the
      // completed matrix.
      if (aligned_columns.size() < parameters.minimum_orbitopal_columns &&
          !complete_action)
        continue;

      std::vector<Lit> action_key;
      if (!complete_action) {
        if (!matrix_action_key(aligned_columns, action_key))
          return result;
        if (action_key == last_inexact_matrix) {
          ++result.reused_inexact_matrices;
          trace.event("orbitopal_reuse_inexact", "rows\t%zu\tcolumns\t%zu",
                      row_count, aligned_columns.size());
          continue;
        }
      }

      if (!ensure_formula())
        return result;

      Matrix matrix(row_count);
      for (size_t row = 0; row < row_count; ++row)
        for (const std::vector<Lit> &column : aligned_columns)
          matrix[row].push_back(column[row]);

      const bool try_rows =
          parameters.orbitopal_orientation != OrbitopalOrientation::Columns;
      const bool try_columns =
          parameters.orbitopal_orientation != OrbitopalOrientation::Rows;
      bool completion_attempted = false;
      if (matrix.front().size() < parameters.minimum_orbitopal_columns) {
        if (!trace.measure("orbitopal_complete_matrix",
                           [&] { return complete_matrix_by_refinement(matrix); }))
          return result;
        completion_attempted = true;
        if (matrix.front().size() < parameters.minimum_orbitopal_columns)
          continue;
      }
      Matrix transposed, row_permutations, column_permutations;
      bool rows_exact = false, columns_exact = false;
      // An axis is exact by its direct row-swap check or, when completing the
      // action, by constrained detected automorphisms.
      const auto exact_axis = [&](bool enabled, const Matrix &axis,
                                  Matrix &permutations) {
        permutations.clear();
        bool exact = enabled && trace.measure("orbitopal_exact_dimension", [&] {
          return exact_dimension(axis, permutations);
        });
        if (enabled && !exact && complete_action &&
            result.status == OrbitopalEnumerationStatus::Complete)
          exact = trace.measure("orbitopal_constrained_dimension", [&] {
            return constrained_dimension_automorphisms(axis, permutations);
          });
        return exact;
      };
      for (unsigned attempt = 0; attempt < 2; ++attempt) {
        if (attempt) {
          if (!complete_action)
            break;
          if (completion_attempted)
            break;
          const size_t prior_columns = matrix.front().size();
          if (!trace.measure("orbitopal_complete_matrix", [&] {
                return complete_matrix_by_refinement(matrix);
              }))
            return result;
          if (matrix.front().size() == prior_columns)
            break;
        }
        if (!trace.measure("orbitopal_clique_order",
                           [&] { return order_columns_by_maximum_clique(matrix); }))
          return result;
        transposed.clear();
        if (try_columns)
          transposed = transpose(matrix);
        rows_exact = exact_axis(try_rows, matrix, row_permutations);
        if (result.status != OrbitopalEnumerationStatus::Complete)
          return result;
        columns_exact = exact_axis(try_columns, transposed, column_permutations);
        if (result.status != OrbitopalEnumerationStatus::Complete)
          return result;
        if (rows_exact || columns_exact)
          break;
      }
      if (!rows_exact && !columns_exact) {
        if (!complete_action)
          last_inexact_matrix = std::move(action_key);
        continue;
      }
      if (hints) {
        // Only the exact successful dimensions have complete maps.
        if (!rows_exact) row_permutations.clear();
        if (!columns_exact) column_permutations.clear();
        if (!collect_matrix_generators(matrix, row_permutations,
                                       column_permutations))
          return result;
        for (const size_t member : members)
          used[member] = 1;
        continue;
      }
      std::vector<size_t> row_fixable;
      std::vector<size_t> column_fixable;
      if (rows_exact) {
        if (complete_action)
          row_fixable = unique_literal_columns(matrix);
        else {
          row_fixable.resize(matrix.front().size());
          for (size_t column = 0; column < row_fixable.size(); ++column)
            row_fixable[column] = column;
        }
      }
      // Column orientation is not restricted to the row detector's ULCs.
      if (columns_exact) {
        column_fixable.resize(transposed.front().size());
        for (size_t column = 0; column < column_fixable.size(); ++column)
          column_fixable[column] = column;
      }
      if (row_fixable.empty() && column_fixable.empty())
        continue;
      for (const size_t member : members)
        used[member] = 1;
      if (result.matrices == parameters.maximum_orbitopal_matrices) {
        result.status = traced_orbitopal_limit("enumerate_orbitopal",
                                               __LINE__);
        return result;
      }
      ++result.matrices;
      if (rows_exact && !trace.measure("orbitopal_publish", [&] {
            return emit_dimension(matrix, row_fixable, row_permutations);
          }))
        return result;
      if (columns_exact && !trace.measure("orbitopal_publish", [&] {
            return emit_dimension(transposed, column_fixable,
                                  column_permutations);
          }))
        return result;
    }
  }
  // A productive phase ends the pass; if the unique-literal phase found
  // nothing, run refinement in this same pass.
  if (!result.instances && (!hints || !result.matrices) &&
      parameters.orbitopal_discovery_order ==
          OrbitopalDiscoveryOrder::UniqueLiteralThenRefinement) {
    bool emitted = false;
    if (!emit_refinement_phase(emitted))
      return result;
  }
  return result;
}

} // namespace

OrbitopalEnumerationResult substitution_redundancy_enumerate_orbitopal(
    const Kernel &kernel, const SubstitutionRedundancyParameters &parameters,
    const OrbitopalInstanceVisitor &visitor, volatile sig_atomic_t *stop) {
  return OrbitopalPass(kernel, parameters, visitor, stop, nullptr).enumerate();
}

OrbitopalEnumerationResult substitution_redundancy_discover_orbitopal_generators(
    const Kernel &kernel, const SubstitutionRedundancyParameters &parameters,
    SymmetryDiscoveryHints &hints, volatile sig_atomic_t *stop) {
  hints = {};
  if (parameters.graph_symmetry.structured_scope < SymmetryStructuredScope::WholeMatrix ||
      parameters.graph_symmetry.structured_scope > SymmetryStructuredScope::AxisAnchors) {
    return {};
  }
  auto result =
      OrbitopalPass(kernel, parameters, {}, stop, &hints).enumerate();
  std::sort(hints.fixed_variables.begin(), hints.fixed_variables.end());
  hints.fixed_variables.erase(
      std::unique(hints.fixed_variables.begin(), hints.fixed_variables.end()),
      hints.fixed_variables.end());
  return result;
}

} // namespace tactix
