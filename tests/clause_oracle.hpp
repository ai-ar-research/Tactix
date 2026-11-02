#pragma once
// A clausal oracle over external DIMACS literals, independent of the solver:
// literal/clause values under a test assignment and reverse unit propagation.
// Its asserts are part of the check.
#ifdef NDEBUG
#error "clause_oracle.hpp asserts are checks; #undef NDEBUG before including it"
#endif

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <vector>

inline uint32_t variable(int32_t literal) {
  return static_cast<uint32_t>(literal < 0 ? -int64_t(literal) : literal);
}

// Bit v-1 of `assignment` is the value of variable v.
inline bool literal_value(int32_t literal, uint64_t assignment) {
  const bool value = assignment & (uint64_t{1} << (variable(literal) - 1));
  return literal > 0 ? value : !value;
}

inline bool clause_value(const std::vector<int32_t> &clause, uint64_t assignment) {
  return std::any_of(clause.begin(), clause.end(), [&](int32_t literal) {
    return literal_value(literal, assignment);
  });
}

// assignment[v] is +1 (true), -1 (false) or 0 (unassigned).
inline bool literal_value(int32_t literal, const std::vector<int8_t> &assignment) {
  const int8_t value = assignment[variable(literal)];
  return value && (literal > 0) == (value > 0);
}

inline bool clause_value(const std::vector<int32_t> &clause,
                         const std::vector<int8_t> &assignment) {
  return std::any_of(clause.begin(), clause.end(), [&](int32_t literal) {
    return literal_value(literal, assignment);
  });
}

// Make `literal` true; false if it is already false.
inline bool assign(std::vector<int8_t> &assignment, int32_t literal) {
  const uint32_t atom = variable(literal);
  const int8_t value = literal > 0 ? 1 : -1;
  if (assignment[atom] == -value) return false;
  assignment[atom] = value;
  return true;
}

struct ClauseLiterals {
  template <class Clause> const Clause &operator()(const Clause &clause) const {
    return clause;
  }
};

// Reverse unit propagation: assign the negation of every candidate literal (a
// complementary pair makes the candidate a tautology: true), then propagate
// the database to fixpoint; true iff some clause is falsified. Open literals
// are counted per occurrence, so a clause repeating its only open literal does
// not propagate. Storage covers the largest variable of database and
// candidate; a nonzero `variable_bound` additionally asserts that bound.
template <class Database, class Candidate, class Literals = ClauseLiterals>
bool rup_implied(const Database &database, const Candidate &candidate,
                 Literals literals = {}, uint32_t variable_bound = 0) {
  uint32_t maximum = 0;
  for (int32_t literal : literals(candidate))
    maximum = std::max(maximum, variable(literal));
  for (const auto &clause : database)
    for (int32_t literal : literals(clause))
      maximum = std::max(maximum, variable(literal));
  assert(!variable_bound || maximum <= variable_bound);
  std::vector<int8_t> values(uint64_t(maximum) + 1, 0);
  for (int32_t literal : literals(candidate))
    if (!assign(values, -literal)) return true;
  bool changed = true;
  while (changed) {
    changed = false;
    for (const auto &clause : database) {
      bool satisfied = false;
      uint32_t open = 0;
      int32_t unit = 0;
      for (int32_t literal : literals(clause)) {
        const int8_t value = values[variable(literal)];
        if (value && (literal > 0) == (value > 0)) {
          satisfied = true;
          break;
        }
        if (!value) {
          ++open;
          unit = literal;
        }
      }
      if (satisfied) continue;
      if (!open) return true;
      if (open == 1) {
        if (!assign(values, unit)) return true;
        changed = true;
      }
    }
  }
  return false;
}
