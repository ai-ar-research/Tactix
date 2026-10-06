#include "rules/semantic_extension/affine_matrix.hpp"

#include <algorithm>
#include <climits>
#include <limits>
#include <map>
#include <set>
#include <utility>

namespace tactix::affine_semantic_extension {
namespace {

struct Work {
  uint64_t used = 0;
  uint64_t limit = 0;
  const std::function<bool(uint64_t)> &charge;
  bool interrupted = false;

  bool consume(uint64_t amount) {
    if (amount > limit - used) return false;
    if (!charge(amount)) {
      interrupted = true;
      return false;
    }
    used += amount;
    return true;
  }
};

template <class T>
void xor_sorted(const std::vector<T> &first, const std::vector<T> &second,
                std::vector<T> &output) {
  output.clear();
  output.reserve(first.size() + second.size());
  std::set_symmetric_difference(first.begin(), first.end(), second.begin(),
                                second.end(), std::back_inserter(output));
}

bool valid_equation(const Equation &equation) {
  if (!std::is_sorted(equation.variables.begin(), equation.variables.end()) ||
      std::adjacent_find(equation.variables.begin(),
                         equation.variables.end()) != equation.variables.end())
    return false;
  return std::all_of(equation.variables.begin(), equation.variables.end(),
                     [](External variable) {
                       return variable > 0 && variable != INT32_MIN;
                     });
}

bool bounded(const MatrixRow &row, const MatrixLimits &limits) {
  return row.variables.size() <= limits.maximum_row_width &&
         row.source_equations.size() <= limits.maximum_sources;
}

enum class CombineStatus { Complete, BoundedOut, Exhausted, Interrupted };

CombineStatus combine(MatrixRow &row, const MatrixRow &other,
                      const MatrixLimits &limits, Work &work) {
  if (!work.consume(1 + row.variables.size() + other.variables.size() +
                    row.source_equations.size() +
                    other.source_equations.size()))
    return work.interrupted ? CombineStatus::Interrupted
                            : CombineStatus::Exhausted;
  std::vector<External> variables;
  std::vector<size_t> sources;
  xor_sorted(row.variables, other.variables, variables);
  xor_sorted(row.source_equations, other.source_equations, sources);
  if (variables.size() > limits.maximum_row_width ||
      sources.size() > limits.maximum_sources)
    return CombineStatus::BoundedOut;
  row.variables = std::move(variables);
  row.source_equations = std::move(sources);
  row.rhs ^= other.rhs;
  return CombineStatus::Complete;
}

MatrixStatus reduce(const std::vector<MatrixRow> &input,
                    const std::vector<MatrixAssignment> &trail,
                    const MatrixLimits &limits, Work &work,
                    std::vector<MatrixRow> &output) {
  std::map<External, bool> assignments;
  for (const MatrixAssignment assignment : trail) {
    if (assignment.variable <= 0 || assignment.variable == INT32_MIN ||
        !assignments.emplace(assignment.variable, assignment.value).second)
      return MatrixStatus::Invalid;
  }

  std::map<External, MatrixRow> basis;
  std::vector<MatrixRow> contradictions;
  std::set<External> admitted_variables;
  bool partial = false;
  for (const MatrixRow &seed : input) {
    MatrixRow row;
    row.rhs = seed.rhs;
    row.source_equations = seed.source_equations;
    for (External variable : seed.variables) {
      const auto found = assignments.find(variable);
      if (found == assignments.end())
        row.variables.push_back(variable);
      else
        row.rhs ^= found->second;
    }
    if (!work.consume(1 + seed.variables.size())) {
      if (work.interrupted) return MatrixStatus::Interrupted;
      partial = true;
      break;
    }
    if (!bounded(row, limits)) {
      partial = true;
      continue;
    }

    while (!row.variables.empty()) {
      const auto found = basis.find(row.variables.front());
      if (found == basis.end()) break;
      const CombineStatus combined =
          combine(row, found->second, limits, work);
      if (combined == CombineStatus::Interrupted)
        return MatrixStatus::Interrupted;
      if (combined == CombineStatus::Exhausted) {
        partial = true;
        goto finished;
      }
      if (combined == CombineStatus::BoundedOut) {
        partial = true;
        row.variables.clear();
        row.source_equations.clear();
        break;
      }
    }
    if (row.source_equations.empty()) continue;
    if (row.variables.empty()) {
      if (row.rhs) {
        if (contradictions.size() + basis.size() == limits.maximum_rows) {
          partial = true;
          continue;
        }
        contradictions.push_back(std::move(row));
      }
      continue;
    }

    size_t additional_variables = 0;
    for (External variable : row.variables)
      additional_variables += !admitted_variables.contains(variable);
    if (additional_variables >
            limits.maximum_variables - admitted_variables.size() ||
        contradictions.size() + basis.size() == limits.maximum_rows) {
      partial = true;
      continue;
    }
    admitted_variables.insert(row.variables.begin(), row.variables.end());
    basis.emplace(row.variables.front(), std::move(row));
  }

finished:
  output.clear();
  output.reserve(contradictions.size() + basis.size());
  for (MatrixRow &row : contradictions) output.push_back(std::move(row));
  for (auto &[pivot, row] : basis) {
    (void)pivot;
    output.push_back(std::move(row));
  }
  return partial ? MatrixStatus::Partial : MatrixStatus::Complete;
}

std::vector<size_t> source_union(const std::vector<MatrixRow> &rows) {
  std::set<size_t> sources;
  for (const MatrixRow &row : rows)
    sources.insert(row.source_equations.begin(), row.source_equations.end());
  return {sources.begin(), sources.end()};
}

External false_literal(const MatrixAssignment assignment) {
  return assignment.value ? -assignment.variable : assignment.variable;
}

MatrixEvent find_event(const std::vector<MatrixRow> &rows) {
  MatrixEvent result;
  for (const MatrixRow &row : rows) {
    if (!row.variables.empty() || !row.rhs) continue;
    result.kind = MatrixEventKind::Conflict;
    result.row = row;
    return result;
  }
  for (const MatrixRow &row : rows) {
    if (row.variables.size() != 1) continue;
    result.kind = MatrixEventKind::Implication;
    result.row = row;
    result.implied_variable = row.variables.front();
    result.implied_value = row.rhs;
    return result;
  }
  return result;
}

void make_reason(const std::vector<Equation> &sources,
                 const std::vector<MatrixAssignment> &trail,
                 MatrixEvent &event) {
  if (event.kind == MatrixEventKind::None) return;
  std::set<External> support;
  for (size_t source : event.row.source_equations)
    for (External variable : sources[source].variables)
      support.insert(variable);
  for (const MatrixAssignment assignment : trail)
    if (support.contains(assignment.variable))
      event.reason.push_back(false_literal(assignment));
  if (event.kind == MatrixEventKind::Implication)
    event.reason.push_back(event.implied_value ? event.implied_variable
                                               : -event.implied_variable);
}

bool same_equation(const Equation &first, const Equation &second) {
  return first.variables == second.variables && first.rhs == second.rhs &&
         first.premises == second.premises &&
         first.premise_clauses == second.premise_clauses;
}

bool same_event(const MatrixEvent &first, const MatrixEvent &second) {
  return first.kind == second.kind && first.row == second.row &&
         first.implied_variable == second.implied_variable &&
         first.implied_value == second.implied_value &&
         first.reason == second.reason;
}

} // namespace

MatrixStatus build_affine_matrix(
    const std::vector<Equation> &input, uint64_t owner_identity,
    const MatrixLimits &limits, const std::function<bool(uint64_t)> &charge,
    AffineMatrix &output) {
  if (!owner_identity || !charge || !limits.maximum_work ||
      !limits.maximum_rows || !limits.maximum_variables ||
      !limits.maximum_row_width || !limits.maximum_sources ||
      !limits.maximum_assignments)
    return MatrixStatus::Invalid;
  if (input.empty()) return MatrixStatus::NoStructure;

  std::vector<MatrixRow> seeds;
  seeds.reserve(input.size());
  for (size_t source = 0; source < input.size(); ++source) {
    const Equation &equation = input[source];
    if (!valid_equation(equation)) return MatrixStatus::Invalid;
    MatrixRow seed;
    seed.variables = equation.variables;
    seed.rhs = equation.rhs;
    seed.source_equations.push_back(source);
    seeds.push_back(std::move(seed));
  }

  Work work{0, limits.maximum_work, charge};
  std::vector<MatrixRow> rows;
  const MatrixStatus status = reduce(seeds, {}, limits, work, rows);
  if (status == MatrixStatus::Interrupted || status == MatrixStatus::Invalid)
    return status;
  if (rows.empty())
    return status == MatrixStatus::Partial ? MatrixStatus::Exhausted
                                          : MatrixStatus::NoStructure;

  AffineMatrix candidate;
  candidate.owner_identity_ = owner_identity;
  candidate.work_ = work.used;
  candidate.complete_admission_ = status == MatrixStatus::Complete;
  candidate.limits_ = limits;
  candidate.sources_ = input;
  candidate.admitted_rows_ = rows;
  candidate.admitted_sources_ = source_union(rows);
  candidate.rows_ = std::move(rows);
  output = std::move(candidate);
  return status;
}

MatrixStatus AffineMatrix::synchronize(
    const std::vector<MatrixAssignment> &trail,
    const std::function<bool(uint64_t)> &charge, MatrixEvent &event) {
  if (!owner_identity_ || !charge) return MatrixStatus::Invalid;
  if (trail.size() > limits_.maximum_assignments)
    return MatrixStatus::Exhausted;

  Work work{0, limits_.maximum_work, charge};
  std::vector<MatrixRow> rows;
  const MatrixStatus status = reduce(admitted_rows_, trail, limits_, work, rows);
  if (status != MatrixStatus::Complete && status != MatrixStatus::Partial)
    return status;
  if (rows.empty() && status == MatrixStatus::Partial)
    return MatrixStatus::Exhausted;

  MatrixEvent candidate = find_event(rows);
  make_reason(sources_, trail, candidate);
  trail_ = trail;
  rows_ = std::move(rows);
  if (work.used > std::numeric_limits<uint64_t>::max() - work_)
    work_ = std::numeric_limits<uint64_t>::max();
  else
    work_ += work.used;
  event = std::move(candidate);
  return status;
}

bool AffineMatrix::authenticates(
    uint64_t owner_identity, const std::vector<Equation> &exact_sources,
    const MatrixEvent &event,
    const std::function<bool(uint64_t)> &charge) const {
  if (!charge || !charge(1) || !owner_identity_ ||
      owner_identity != owner_identity_ ||
      exact_sources.size() != sources_.size())
    return false;
  for (size_t i = 0; i < sources_.size(); ++i) {
    const Equation &source = sources_[i];
    if (!charge(3) || !charge(source.variables.size()) ||
        !charge(source.premises.size()) ||
        !charge(source.premise_clauses.size()))
      return false;
    for (const std::vector<External> &clause : source.premise_clauses)
      if (!charge(clause.size())) return false;
    if (!same_equation(exact_sources[i], sources_[i])) return false;
  }
  if (!charge(1) || !charge(trail_.size()) || !charge(rows_.size()))
    return false;
  for (const MatrixRow &row : rows_)
    if (!charge(row.variables.size() + row.source_equations.size()))
      return false;
  MatrixEvent expected = find_event(rows_);
  if (!charge(event.reason.size()) || !charge(event.row.variables.size()) ||
      !charge(event.row.source_equations.size()))
    return false;
  for (size_t source : expected.row.source_equations)
    if (source >= sources_.size() ||
        !std::binary_search(admitted_sources_.begin(),
                            admitted_sources_.end(), source) ||
        !charge(sources_[source].variables.size()))
      return false;
  make_reason(sources_, trail_, expected);
  return expected.kind != MatrixEventKind::None && same_event(expected, event);
}

} // namespace tactix::affine_semantic_extension
