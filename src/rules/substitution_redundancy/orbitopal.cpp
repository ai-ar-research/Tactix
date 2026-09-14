#include "rules/substitution_redundancy/substitution_redundancy.hpp"

#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace tactix {
namespace {

struct LogicalFormula {
  std::vector<std::vector<Lit>> clauses;
  std::unordered_map<std::string, uint32_t> multiplicity;
};

struct OrbitopalScreen {
  std::vector<uint32_t> occurrences;
  std::vector<std::vector<Lit>> candidate_clauses;
};

std::string key(std::vector<Lit> clause) {
  std::sort(clause.begin(), clause.end());
  return std::string(reinterpret_cast<const char *>(clause.data()),
                     clause.size() * sizeof(Lit));
}

bool charge(const Kernel &kernel, volatile sig_atomic_t *stop,
            const SubstitutionRedundancyParameters &parameters,
            OrbitopalEnumerationResult &result, uint64_t amount) {
  if ((stop && *stop) || (kernel.stop && *kernel.stop)) {
    result.status = OrbitopalEnumerationStatus::Interrupted;
    return false;
  }
  const uint64_t limit = parameters.maximum_detection_ticks;
  if (result.ticks >= limit || amount > limit - result.ticks) {
    result.ticks = limit;
    result.status = OrbitopalEnumerationStatus::LimitReached;
    return false;
  }
  result.ticks += amount;
  return true;
}

bool screen_formula(const Kernel &kernel,
                    const SubstitutionRedundancyParameters &parameters,
                    volatile sig_atomic_t *stop,
                    OrbitopalEnumerationResult &result,
                    OrbitopalScreen &screen) {
  screen.occurrences.assign(2 * kernel.nvars, 0);
  std::vector<Ref> possible_large;
  if (kernel.dense) {
    // The phase-level occurrence image already counts every irredundant large
    // occurrence. Binary watch counts are constant-time too. Redundant large
    // clauses may create false positives here, but never false proof steps:
    // the exact whole-formula automorphism test and direct SR validation still
    // include them before mutation.
    for (Lit literal = 0; literal < 2 * kernel.nvars; ++literal) {
      if (!charge(kernel, stop, parameters, result, 1))
        return false;
      screen.occurrences[literal] = kernel.dense->large_size(literal) +
                                    kernel.watches.binary_size(literal);
    }
  } else {
    std::unordered_set<uint64_t> binaries;
    for (Lit literal = 0; literal < 2 * kernel.nvars; ++literal)
      for (const WatchEntry watch : kernel.watches.entries(literal)) {
        if (!charge(kernel, stop, parameters, result, 1))
          return false;
        if (!watch.binary())
          continue;
        const Lit other = watch.other();
        if (!binaries.insert(kernel_binary_key(literal, other)).second)
          continue;
        ++screen.occurrences[literal];
        ++screen.occurrences[other];
      }
    for (Ref ref = 0; ref < kernel.arena.size();) {
      const Clause *clause = clause_at(kernel.arena, ref);
      const uint32_t words = clause_actual_words(clause);
      if (!clause->garbage()) {
        if (!charge(kernel, stop, parameters, result, clause->size))
          return false;
        possible_large.push_back(ref);
        for (const Lit literal : *clause)
          ++screen.occurrences[literal];
      }
      ref += words;
    }
  }
  for (Var var = 0; var < kernel.nvars; ++var) {
    if (!charge(kernel, stop, parameters, result, 1))
      return false;
    if (!kernel.unit_ids[var])
      continue;
    const Lit positive = make_lit(var, false);
    const Lit unit = kernel.value(positive) > 0 ? positive : negate(positive);
    ++screen.occurrences[unit];
  }

  std::unordered_set<uint64_t> candidate_binaries;
  std::unordered_set<Ref> candidate_large;
  for (Lit literal = 0; literal < 2 * kernel.nvars; ++literal) {
    if (screen.occurrences[literal] != 1)
      continue;
    if (kernel.watches.binary_size(literal) == 1 &&
        parameters.minimum_orbitopal_rows <= 2) {
      for (const WatchEntry watch : kernel.watches.entries(literal)) {
        if (!watch.binary())
          continue;
        const Lit other = watch.other();
        if (screen.occurrences[other] == 1 &&
            candidate_binaries.insert(
                kernel_binary_key(literal, other)).second) {
          std::vector<Lit> clause = {literal, other};
          std::sort(clause.begin(), clause.end());
          screen.candidate_clauses.push_back(std::move(clause));
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
  if (kernel.dense)
    possible_large.assign(candidate_large.begin(), candidate_large.end());
  for (const Ref ref : possible_large) {
    const Clause *clause = clause_at(kernel.arena, ref);
    if (clause->garbage() ||
        clause->size < parameters.minimum_orbitopal_rows)
      continue;
    if (!charge(kernel, stop, parameters, result, clause->size))
      return false;
    bool unique = true;
    for (const Lit literal : *clause)
      unique &= screen.occurrences[literal] == 1;
    if (unique) {
      std::vector<Lit> ordered(clause->begin(), clause->end());
      std::sort(ordered.begin(), ordered.end());
      screen.candidate_clauses.push_back(std::move(ordered));
    }
  }
  return true;
}

bool full_snapshot(const Kernel &kernel,
                   const SubstitutionRedundancyParameters &parameters,
                   volatile sig_atomic_t *stop,
                   OrbitopalEnumerationResult &enumeration,
                   LogicalFormula &result) {
  result.clauses.reserve(kernel.binary_ids.size() +
                         kernel.live_irredundant_large + kernel.nvars);
  std::unordered_set<uint64_t> seen_binaries;
  for (Lit literal = 0; literal < 2 * kernel.nvars; ++literal) {
    for (const WatchEntry watch : kernel.watches.entries(literal)) {
      if (!charge(kernel, stop, parameters, enumeration, 1))
        return false;
      if (!watch.binary())
        continue;
      const Lit other = watch.other();
      if (!seen_binaries.insert(kernel_binary_key(literal, other)).second)
        continue;
      result.clauses.push_back({literal, other});
      ++result.multiplicity[key(result.clauses.back())];
    }
  }
  for (Ref ref = 0; ref < kernel.arena.size();) {
    const Clause *clause = clause_at(kernel.arena, ref);
    const uint32_t words = clause_actual_words(clause);
    if (!clause->garbage()) {
      if (!charge(kernel, stop, parameters, enumeration, clause->size))
        return false;
      result.clauses.emplace_back(clause->begin(), clause->end());
      ++result.multiplicity[key(result.clauses.back())];
    }
    ref += words;
  }
  for (Var var = 0; var < kernel.nvars; ++var) {
    if (!charge(kernel, stop, parameters, enumeration, 1))
      return false;
    if (!kernel.unit_ids[var])
      continue;
    const Lit positive = make_lit(var, false);
    result.clauses.push_back(
        {kernel.value(positive) > 0 ? positive : negate(positive)});
    ++result.multiplicity[key(result.clauses.back())];
  }
  return true;
}

bool exact_automorphism(const Kernel &kernel, const LogicalFormula &formula,
                        const std::unordered_map<Lit, Lit> &mapping,
                        const SubstitutionRedundancyParameters &parameters,
                        volatile sig_atomic_t *stop,
                        OrbitopalEnumerationResult &result) {
  std::unordered_map<std::string, uint32_t> image;
  image.reserve(formula.multiplicity.size());
  for (const std::vector<Lit> &clause : formula.clauses) {
    if (!charge(kernel, stop, parameters, result, clause.size()))
      return false;
    std::vector<Lit> mapped;
    mapped.reserve(clause.size());
    for (const Lit literal : clause) {
      const auto found = mapping.find(literal);
      mapped.push_back(found == mapping.end() ? literal : found->second);
    }
    ++image[key(std::move(mapped))];
  }
  return image == formula.multiplicity;
}

bool align_columns(const Kernel &kernel,
                   const std::vector<Lit> &first,
                   const std::vector<Lit> &candidate,
                   std::vector<Lit> &aligned,
                   const SubstitutionRedundancyParameters &parameters,
                   volatile sig_atomic_t *stop,
                   OrbitopalEnumerationResult &result) {
  aligned.assign(first.size(), INVALID_LIT);
  std::unordered_set<Lit> used;
  for (size_t row = 0; row < first.size(); ++row) {
    Lit match = INVALID_LIT;
    for (const Lit other : candidate) {
      if (!charge(kernel, stop, parameters, result, 1))
        return false;
      if (!kernel_binary_live(kernel, negate(first[row]), negate(other)))
        continue;
      if (match != INVALID_LIT)
        return false;
      match = other;
    }
    if (match == INVALID_LIT || !used.insert(match).second)
      return false;
    aligned[row] = match;
  }
  return used.size() == candidate.size();
}

} // namespace

OrbitopalEnumerationResult substitution_redundancy_enumerate_orbitopal(
    const Kernel &kernel, const SubstitutionRedundancyParameters &parameters,
    const OrbitopalInstanceVisitor &visitor, volatile sig_atomic_t *stop) {
  OrbitopalEnumerationResult result;
  if (!parameters.orbitopal_producer ||
      !parameters.maximum_orbitopal_matrices ||
      !parameters.maximum_orbitopal_instances ||
      !parameters.maximum_detection_ticks)
    return result;
  OrbitopalScreen screen;
  if (!screen_formula(kernel, parameters, stop, result, screen))
    return result;

  std::sort(screen.candidate_clauses.begin(),
            screen.candidate_clauses.end());
  std::vector<uint8_t> claimed(kernel.nvars, 0);
  std::map<size_t, std::vector<std::vector<Lit>>> groups;
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

  std::optional<LogicalFormula> formula;

  for (auto &[row_count, columns] : groups) {
    std::sort(columns.begin(), columns.end());
    std::vector<uint8_t> used(columns.size(), 0);
    for (size_t seed = 0; seed < columns.size(); ++seed) {
      if (used[seed])
        continue;
      if ((stop && *stop) || (kernel.stop && *kernel.stop)) {
        result.status = OrbitopalEnumerationStatus::Interrupted;
        return result;
      }
      std::vector<std::vector<Lit>> aligned_columns = {columns[seed]};
      std::vector<size_t> members = {seed};
      for (size_t candidate = seed + 1; candidate < columns.size();
           ++candidate) {
        if (used[candidate])
          continue;
        std::vector<Lit> aligned;
        if (align_columns(kernel, columns[seed], columns[candidate], aligned,
                          parameters, stop, result)) {
          aligned_columns.push_back(std::move(aligned));
          members.push_back(candidate);
        }
        if (result.status != OrbitopalEnumerationStatus::Complete)
          return result;
      }
      if (aligned_columns.size() < parameters.minimum_orbitopal_columns)
        continue;

      if (!formula) {
        formula.emplace();
        if (!full_snapshot(kernel, parameters, stop, result, *formula))
          return result;
      }

      std::vector<std::vector<Lit>> matrix(row_count);
      for (size_t row = 0; row < row_count; ++row)
        for (const std::vector<Lit> &column : aligned_columns)
          matrix[row].push_back(column[row]);

      bool exact = true;
      for (size_t row = 0; row + 1 < row_count && exact; ++row) {
        std::unordered_map<Lit, Lit> mapping;
        for (size_t column = 0; column < aligned_columns.size(); ++column) {
          const Lit a = matrix[row][column];
          const Lit b = matrix[row + 1][column];
          mapping[a] = b;
          mapping[negate(a)] = negate(b);
          mapping[b] = a;
          mapping[negate(b)] = negate(a);
        }
        exact = exact_automorphism(kernel, *formula, mapping, parameters, stop,
                                   result);
        if (result.status != OrbitopalEnumerationStatus::Complete)
          return result;
      }
      if (!exact)
        continue;
      for (const size_t member : members)
        used[member] = 1;
      if (result.matrices == parameters.maximum_orbitopal_matrices) {
        result.status = OrbitopalEnumerationStatus::LimitReached;
        return result;
      }
      ++result.matrices;

      const size_t fixing_columns =
          std::min(aligned_columns.size(), row_count - 1);
      for (size_t column = 0; column < fixing_columns; ++column) {
        for (size_t row = 0; row + column + 1 < row_count; ++row) {
          if (result.instances == parameters.maximum_orbitopal_instances) {
            result.status = OrbitopalEnumerationStatus::LimitReached;
            return result;
          }
          SubstitutionRedundancyInstance instance;
          const Lit fixed = matrix[row][column];
          const Lit blocked = matrix[row + 1][column];
          instance.clause = {negate(fixed)};
          instance.witness = {negate(fixed), blocked};
          for (size_t other = 0; other < aligned_columns.size(); ++other) {
            const Lit a = matrix[row][other];
            const Lit b = matrix[row + 1][other];
            if (var_of(a) == var_of(fixed) ||
                var_of(a) == var_of(blocked) ||
                var_of(b) == var_of(fixed) || var_of(b) == var_of(blocked))
              continue;
            instance.substitution.emplace_back(a, b);
            instance.substitution.emplace_back(b, a);
          }
          ++result.instances;
          if (!visitor(instance))
            return result;
        }
      }
    }
  }
  return result;
}

} // namespace tactix
