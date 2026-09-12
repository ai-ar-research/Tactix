#include "rules/factor/factor.hpp"

#include "kernel/kernel.hpp"

#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

namespace tactix {

namespace {

using Remainder = std::vector<Lit>;

struct PhysicalClause {
  bool binary = false;
  uint64_t identity = 0;

  friend bool operator<(const PhysicalClause &a, const PhysicalClause &b) {
    return std::tie(a.binary, a.identity) <
           std::tie(b.binary, b.identity);
  }
  friend bool operator==(const PhysicalClause &a, const PhysicalClause &b) {
    return a.binary == b.binary && a.identity == b.identity;
  }
};

struct Occurrence {
  Ref member = INVALID_REF;
  PhysicalClause physical;
};

using Columns = std::map<Remainder, std::vector<Occurrence>>;

struct ColumnChoice {
  Remainder remainder;
  std::vector<Ref> members;
  std::vector<PhysicalClause> physical;
};

struct Enumerator {
  const Kernel &kernel;
  const FactorGridVisitor &visitor;
  FactorEnumerationOptions options;
  FactorEnumerationResult result;
  std::map<Lit, Columns> universe;
  std::vector<Lit> candidates;
  std::vector<Lit> factors;
  std::vector<const Columns *> selected_columns;
  bool done = false;

  Enumerator(const Kernel &kernel_, const FactorGridVisitor &visitor_,
             FactorEnumerationOptions options_)
      : kernel(kernel_), visitor(visitor_), options(options_) {}

  bool stopped() {
    if (done)
      return true;
    if (options.stop && *options.stop) {
      result.status = FactorEnumerationStatus::Stopped;
      done = true;
    }
    return done;
  }

  bool factor_eligible(Lit factor) const {
    return factor < 2 * kernel.nvars && kernel.value(factor) == 0 &&
           !kernel_var_retired(kernel, var_of(factor));
  }

  void add_occurrence(Lit factor, Remainder remainder, Ref member,
                      PhysicalClause physical) {
    if (!factor_eligible(factor))
      return;
    std::sort(remainder.begin(), remainder.end());
    universe[factor][std::move(remainder)].push_back({member, physical});
  }

  void snapshot() {
    // A logical binary pair is one Factor cell even if the watch substrate
    // contains duplicate copies: exact application retires all copies of that
    // pair as one represented clause. Visit the pair once in literal order.
    for (Lit first = 0; first < static_cast<Lit>(2 * kernel.nvars); ++first) {
      if (stopped())
        return;
      for (const WatchEntry watch : kernel.watches.entries(first)) {
        if (!watch.binary())
          continue;
        const Lit second = watch.other();
        if (first >= second || !kernel_binary_live(kernel, first, second))
          continue;
        const PhysicalClause physical{true,
                                      kernel_binary_key(first, second)};
        add_occurrence(first, {second}, INVALID_REF, physical);
        add_occurrence(second, {first}, INVALID_REF, physical);
      }
    }

    for (Ref ref = 0; ref < kernel.arena.size();) {
      if (stopped())
        return;
      const Clause *clause = clause_at(kernel.arena, ref);
      const uint32_t words = clause_actual_words(clause);
      if (!clause->garbage() && !clause->redundant())
        for (const Lit factor : *clause) {
          Remainder remainder;
          remainder.reserve(clause->size - 1);
          for (const Lit literal : *clause)
            if (literal != factor)
              remainder.push_back(literal);
          if (remainder.size() + 1 == clause->size)
            add_occurrence(factor, std::move(remainder), ref,
                           PhysicalClause{false, ref});
        }
      ref += words;
    }

    for (auto &[factor, columns] : universe) {
      uint64_t occurrences = 0;
      for (auto &[remainder, choices] : columns) {
        (void)remainder;
        std::sort(choices.begin(), choices.end(),
                  [](const Occurrence &a, const Occurrence &b) {
                    return a.physical < b.physical;
                  });
        choices.erase(std::unique(choices.begin(), choices.end(),
                                  [](const Occurrence &a,
                                     const Occurrence &b) {
                                    return a.physical == b.physical;
                                  }),
                      choices.end());
        occurrences += choices.size();
      }
      if (occurrences >= 2)
        candidates.push_back(factor);
    }
  }

  uint64_t common_column_upper_bound() const {
    if (selected_columns.empty())
      return 0;
    uint64_t total = 0;
    for (const auto &[remainder, first_occurrences] : *selected_columns[0]) {
      size_t multiplicity = first_occurrences.size();
      for (size_t row = 1; row < selected_columns.size() && multiplicity;
           ++row) {
        const auto found = selected_columns[row]->find(remainder);
        if (found == selected_columns[row]->end()) {
          multiplicity = 0;
          break;
        }
        multiplicity = std::min(multiplicity, found->second.size());
      }
      if (multiplicity > std::numeric_limits<uint64_t>::max() - total)
        return std::numeric_limits<uint64_t>::max();
      total += multiplicity;
    }
    return total;
  }

  void build_column_products(
      const Remainder &remainder,
      const std::vector<const std::vector<Occurrence> *> &choices,
      size_t row, ColumnChoice &column, std::set<PhysicalClause> &used,
      std::vector<ColumnChoice> &out) {
    if (stopped())
      return;
    if (row == choices.size()) {
      out.push_back(column);
      return;
    }
    for (const Occurrence &occurrence : *choices[row]) {
      if (!used.insert(occurrence.physical).second)
        continue;
      column.members[row] = occurrence.member;
      column.physical[row] = occurrence.physical;
      build_column_products(remainder, choices, row + 1, column, used, out);
      used.erase(occurrence.physical);
      if (stopped())
        return;
    }
  }

  std::vector<ColumnChoice> common_columns() {
    std::vector<ColumnChoice> result_columns;
    for (const auto &[remainder, first_occurrences] : *selected_columns[0]) {
      std::vector<const std::vector<Occurrence> *> choices;
      choices.push_back(&first_occurrences);
      for (size_t row = 1; row < selected_columns.size(); ++row) {
        const auto found = selected_columns[row]->find(remainder);
        if (found == selected_columns[row]->end()) {
          choices.clear();
          break;
        }
        choices.push_back(&found->second);
      }
      if (choices.empty())
        continue;
      ColumnChoice column;
      column.remainder = remainder;
      column.members.resize(factors.size(), INVALID_REF);
      column.physical.resize(factors.size());
      std::set<PhysicalClause> used;
      build_column_products(remainder, choices, 0, column, used,
                            result_columns);
      if (stopped())
        break;
    }
    return result_columns;
  }

  bool emit(const std::vector<ColumnChoice> &columns,
            const std::vector<size_t> &selected) {
    if (result.grids == options.maximum_grids) {
      result.status = FactorEnumerationStatus::LimitReached;
      done = true;
      return false;
    }
    FactorGrid grid;
    grid.factors = factors;
    grid.remainders.reserve(selected.size());
    for (const size_t column : selected)
      grid.remainders.push_back(columns[column].remainder);
    grid.members.reserve(factors.size() * selected.size());
    for (size_t row = 0; row < factors.size(); ++row)
      for (const size_t column : selected)
        grid.members.push_back(columns[column].members[row]);
    ++result.grids;
    if (!visitor(grid)) {
      result.status = FactorEnumerationStatus::Stopped;
      done = true;
      return false;
    }
    return true;
  }

  void choose_columns(const std::vector<ColumnChoice> &columns, size_t start,
                      size_t need, std::vector<size_t> &selected,
                      std::set<PhysicalClause> &used) {
    if (stopped())
      return;
    if (!need) {
      emit(columns, selected);
      return;
    }
    if (columns.size() - start < need)
      return;
    for (size_t index = start; index + need <= columns.size(); ++index) {
      bool disjoint = true;
      for (const PhysicalClause physical : columns[index].physical)
        if (used.count(physical)) {
          disjoint = false;
          break;
        }
      if (!disjoint)
        continue;
      for (const PhysicalClause physical : columns[index].physical)
        used.insert(physical);
      selected.push_back(index);
      choose_columns(columns, index + 1, need - 1, selected, used);
      selected.pop_back();
      for (const PhysicalClause physical : columns[index].physical)
        used.erase(physical);
      if (stopped())
        return;
    }
  }

  void enumerate_for_current_factors() {
    const size_t rows = factors.size();
    const size_t minimum_columns = rows == 2 ? 3 : 2;
    std::vector<ColumnChoice> columns = common_columns();
    if (columns.size() < minimum_columns || stopped())
      return;
    for (size_t count = minimum_columns; count <= columns.size(); ++count) {
      std::vector<size_t> selected;
      std::set<PhysicalClause> used;
      choose_columns(columns, 0, count, selected, used);
      if (stopped())
        return;
    }
  }

  void choose_factors(size_t start) {
    if (stopped())
      return;
    if (factors.size() >= 2)
      enumerate_for_current_factors();

    // Adding a row can only remove common columns. Every profitable grid with
    // three or more rows needs at least two columns, so this branch cannot
    // recover once the multiplicity upper bound falls below two.
    if (!factors.empty() && common_column_upper_bound() < 2)
      return;
    for (size_t index = start; index < candidates.size(); ++index) {
      const Lit factor = candidates[index];
      factors.push_back(factor);
      selected_columns.push_back(&universe.find(factor)->second);
      choose_factors(index + 1);
      selected_columns.pop_back();
      factors.pop_back();
      if (stopped())
        return;
    }
  }

  FactorEnumerationResult run() {
    if (kernel.inconsistent || kernel.level != 0 || !visitor)
      return result;
    snapshot();
    choose_factors(0);
    return result;
  }
};

} // namespace

FactorEnumerationResult
factor_enumerate_grids(const Kernel &kernel, const FactorGridVisitor &visitor,
                       FactorEnumerationOptions options) {
  Enumerator enumerator(kernel, visitor, options);
  return enumerator.run();
}

} // namespace tactix
